/**
 * @file eventManager.cpp
 * @brief Orchestrator: PlantContext + Event-Vorverarbeitung + Routing.
 *
 * Pipeline pro Event:
 *   1. ErrorHandler + WorkpieceManager (wenn Band-Subsysteme freigegeben)
 *   2. Kontext-Flags aktualisieren
 *   3. FSM-Events vorverarbeiten und gezielt an FSM senden
 *
 * SoSe2026 Team 3.3
 */

#include "eventManager.hpp"
#include "logik/logik.hpp"
#include "logik/workpieceManager/WorkpieceManager.hpp"
#include "logik/errorHandler/errorHandler.hpp"
#include "logik/errorHandler/errorTypes.hpp"
#include "logik/fsm/Fsm.hpp"
#include "logik/fsm/common/BaseState.hpp"
#include "dispatcher/i_Dispatcher.hpp"
#include "shared/detailLog.hpp"

#include <iostream>

namespace {

constexpr std::chrono::milliseconds START_LONG_THRESHOLD{1500};

bool isEntryLightBarrierClear(const logik::Logik* context) {
    return context != nullptr
        && context->getDispatcher() != nullptr
        && context->getDispatcher()->isEntryLightBarrierClear();
}

bool isEntryLightBarrierBlocked(const logik::Logik* context) {
    return !isEntryLightBarrierClear(context);
}

bool isPhysicalSensorEvent(logik::Event e) {
    switch (e) {
    case logik::Event::ENTRY_BLOCKED:
    case logik::Event::ENTRY_CLEARED:
    case logik::Event::HEIGHT_BLOCKED:
    case logik::Event::HEIGHT_CLEARED:
    case logik::Event::HEIGHT_ERROR:
    case logik::Event::METAL_DETECTED:
    case logik::Event::METAL_CLEARED:
    case logik::Event::EJECTOR_BLOCKED:
    case logik::Event::EJECTOR_CLEARED:
    case logik::Event::END_BLOCKED:
    case logik::Event::END_CLEARED:
    case logik::Event::CHUTE_BLOCKED:
    case logik::Event::CHUTE_CLEARED:
        return true;
    default:
        return false;
    }
}

bool isWorkpieceTrackingSensorEvent(logik::Event e) {
    switch (e) {
    case logik::Event::ENTRY_CLEARED:
    case logik::Event::HEIGHT_BLOCKED:
    case logik::Event::HEIGHT_CLEARED:
    case logik::Event::HEIGHT_ERROR:
    case logik::Event::METAL_DETECTED:
    case logik::Event::METAL_CLEARED:
    case logik::Event::EJECTOR_BLOCKED:
    case logik::Event::EJECTOR_CLEARED:
        return true;
    default:
        return false;
    }
}

bool isChuteSensorEvent(logik::Event e) {
    return e == logik::Event::CHUTE_BLOCKED || e == logik::Event::CHUTE_CLEARED;
}

bool isEntrySensorEvent(logik::Event e) {
    return e == logik::Event::ENTRY_BLOCKED || e == logik::Event::ENTRY_CLEARED;
}

bool isMetalClearedEvent(logik::Event e) {
    return e == logik::Event::METAL_CLEARED;
}

bool shouldEscalateUnexpectedEmptyBandSensor(
    logik::Event e,
    AnlagenPosition position,
    bool entryFrei,
    bool endBlocked,
    const logik::WorkpieceManager* wpManager)
{
    if (!isPhysicalSensorEvent(e) || isChuteSensorEvent(e) || isMetalClearedEvent(e)) {
        return false;
    }

    if (position == AnlagenPosition::VORN_ANLAGE_1) {
        if (e == logik::Event::ENTRY_BLOCKED && entryFrei && !endBlocked) {
            return false;
        }
        return true;
    }

    if (position == AnlagenPosition::HINTEN_ANLAGE_2) {
        if (e == logik::Event::ENTRY_BLOCKED && wpManager != nullptr
            && wpManager->hasPendingNetworkWorkpiece()) {
            return false;
        }
        return true;
    }

    return true;
}

bool isAllowedSensorWithoutWorkpieces(
    logik::Event e,
    AnlagenPosition position,
    bool entryFrei,
    bool endBlocked,
    const logik::WorkpieceManager* wpManager)
{
    return !shouldEscalateUnexpectedEmptyBandSensor(
        e, position, entryFrei, endBlocked, wpManager);
}

bool isSpuriousEmptyBandEjectorSignal(
    logik::Event e,
    const logik::Logik* context,
    const logik::WorkpieceManager* wpManager)
{
    if (e != logik::Event::EJECTOR_BLOCKED) {
        return false;
    }
    if (wpManager != nullptr && wpManager->isSorterEchoWindowActive()) {
        return true;
    }
    if (context != nullptr
        && context->getDispatcher() != nullptr
        && context->getDispatcher()->isEjectorLightBarrierClear()) {
        return true;
    }
    return false;
}

} // namespace

namespace logik {

EventManager::EventManager(Logik* context)
    : m_context(context),
      m_wpManager(nullptr),
      m_errorHandler(nullptr)
{
}

EventManager::~EventManager() = default;

void EventManager::init(AnlagenPosition pos) {
    m_ctx.position = pos;

    if (m_context != nullptr) {
        m_wpManager = m_context->getWorkpieceManager();
        m_errorHandler = m_context->getErrorHandler();
    }

    m_fsm = std::make_unique<Fsm>(m_context, this);
    resetStartPressTimer();

    DETAIL_LOG( "[EventManager] Initialisiert (Position "
              << static_cast<int>(pos) << "). Rolle: Orchestrator + PlantContext."
             );
}

bool EventManager::canRouteToBandSubsystems() const {
    return m_ctx.runningMode
        && !m_ctx.eStopActive
        && !m_ctx.errorActive
        && !m_ctx.endBlocked
        && !m_ctx.connectionLost;
}

bool EventManager::canRouteWorkpieceTracking() const {
    return m_ctx.runningMode
        && !m_ctx.eStopActive
        && !m_ctx.errorActive
        && !m_ctx.connectionLost;
}

bool EventManager::isFsmPassthroughEvent(Event e) const {
    switch (e) {
    case Event::STOP_PRESSED:
    case Event::STOP_RELEASED:
    case Event::RESET_PRESSED:
    case Event::RESET_RELEASED:
    case Event::PARTNER_STOP_PRESSED:
    case Event::PARTNER_STOP_RELEASED:
    case Event::PARTNER_RESET_PRESSED:
    case Event::PARTNER_RESET_RELEASED:
    case Event::ESTOP_PRESSED:
    case Event::ESTOP_RELEASED:
    case Event::PARTNER_ESTOP_PRESSED:
    case Event::PARTNER_ESTOP_RELEASED:
    case Event::PARTNER_ESTOP_SAFETY_SYNC:
    case Event::CONNECTION_LOST:
    case Event::CONNECTION_RESTORED:
    case Event::DATA_EXIST:
    case Event::DATA_NOT_EXIST:
    case Event::SERVICE_EMPTY_BELT_READY:
    case Event::ERROR:
    case Event::ERROR_CLEARED:
    case Event::WARNING:
    case Event::WARNING_CLEARED:
    case Event::END_BLOCKED:
    case Event::END_CLEARED:
    case Event::CHUTE_BLOCKED:
    case Event::HEIGHT_CLEARED:
    case Event::SET_SLOW:
    case Event::SET_FAST:
    case Event::BAND_EMPTY:
    case Event::MOTOR_RUN:
    case Event::MOTOR_STOP:
    case Event::ENTRY_FREI:
    case Event::ENTRY_NOT_FREI:
    case Event::PARTNER_CALIB_READY:
    case Event::PARTNER_CALIB_NOT_READY:
    case Event::PARTNER_START_RUNNING:
    case Event::PARTNER_START_SERVICE:
        return true;
    default:
        return false;
    }
}

void EventManager::handleEvent(Event e) {
    if (isPhysicalSensorEvent(e)
        && !m_ctx.runningMode
        && !m_ctx.serviceMode
        && !m_ctx.errorActive) {
        return;
    }

    if (m_ctx.runningMode
        && !m_ctx.serviceMode
        && !m_ctx.errorActive
        && m_wpManager != nullptr
        && !m_wpManager->hasActiveWorkpieces()
        && shouldEscalateUnexpectedEmptyBandSensor(
            e, m_ctx.position, m_ctx.entryFrei, m_ctx.endBlocked, m_wpManager)) {
        if (isSpuriousEmptyBandEjectorSignal(e, m_context, m_wpManager)) {
            DETAIL_LOG( "[EventManager] EJECTOR_BLOCKED ohne WPs – Weiche/Auswerfer-Echo "
                      << "oder LS bereits frei, ignoriert.");
            return;
        }
        if (m_ctx.position == AnlagenPosition::HINTEN_ANLAGE_2 && isEntrySensorEvent(e)) {
            DETAIL_LOG( "[EventManager] Anlage 2: Einlauf muss frei sein – "
                      << "unerwartetes Entry-Signal ohne WP-Uebergabe.");
        } else {
            DETAIL_LOG( "[EventManager] Unerwartetes Sensor-Signal ohne aktive WPs "
                      << "(nur Einlauf/Rutsche erlaubt).");
        }
        if (m_errorHandler != nullptr) {
            m_errorHandler->handleEscalation(ErrorType::UNEXPECTED_EVENT, e);
        }
        return;
    }

    if (e == Event::ENTRY_BLOCKED
        && m_ctx.runningMode
        && !m_ctx.serviceMode
        && !m_ctx.errorActive
        && m_wpManager != nullptr) {
        const bool earlyEntryA1 =
            m_ctx.position == AnlagenPosition::VORN_ANLAGE_1
            && !m_ctx.entryFrei
            && m_wpManager->isEarlyEntryViolationOnBlocked();
        const bool earlyEntryA2 =
            m_ctx.position == AnlagenPosition::HINTEN_ANLAGE_2
            && m_wpManager->isEarlyEntryViolationOnBlocked()
            && !m_wpManager->isExpectedNetworkEntryAtAnlage2();
        if (earlyEntryA1 || earlyEntryA2) {
            if (m_ctx.position == AnlagenPosition::HINTEN_ANLAGE_2) {
                DETAIL_LOG( "[EventManager] Anlage 2: Einlauf-Fehler – WP am Einlauf, "
                          << "obwohl vorderes WP den Hoehensensor noch nicht erreicht hat."
                         );
            } else {
                DETAIL_LOG( "[EventManager] Einlauf-Fehler: WP am Anfang gelegt, "
                          << "obwohl Q1 noch nicht frei ist (vorderes WP hat HS nicht erreicht)."
                         );
            }
            if (m_errorHandler != nullptr) {
                m_errorHandler->handleEscalation(
                    ErrorType::EARLY_ENTRY, Event::ENTRY_BLOCKED, WpLocation::AT_ENTRY);
            }
            return;
        }
    }

    if (m_wpManager != nullptr) {
        if (e == Event::REQ_TO_GIVE_WP && m_ctx.position == AnlagenPosition::HINTEN_ANLAGE_2) {
            m_wpManager->onPartnerHandoverRequest();
        } else if (e == Event::READY_TO_RECEIVE_WP
                   && m_ctx.position == AnlagenPosition::VORN_ANLAGE_1) {
            m_wpManager->onPartnerReadyToReceive();
        } else if (e == Event::PARTNER_HANDOVER_COMPLETE
                   && m_ctx.position == AnlagenPosition::VORN_ANLAGE_1) {
            m_wpManager->onPartnerHandoverComplete();
        } else if (e == Event::PARTNER_HANDOVER_WP_LOST
                   && m_ctx.position == AnlagenPosition::VORN_ANLAGE_1) {
            m_wpManager->onPartnerHandoverWpLost();
        } else if (e == Event::PARTNER_HANDOVER_PAUSE
                   && m_ctx.position == AnlagenPosition::VORN_ANLAGE_1) {
            m_wpManager->onPartnerHandoverPause();
        }
    }

    const bool entryWhileEndBlocked =
        e == Event::ENTRY_BLOCKED
        && m_ctx.runningMode
        && !m_ctx.serviceMode
        && !m_ctx.errorActive
        && m_ctx.endBlocked
        && m_ctx.position == AnlagenPosition::VORN_ANLAGE_1;
    const bool bandSubsystemsAllowed =
        canRouteToBandSubsystems() && !m_ctx.serviceMode;
    const bool endAndChuteSensorsAllowed =
        (m_ctx.runningMode || m_ctx.errorActive) && !m_ctx.serviceMode;
    const bool errorRecoverySensorEvent =
        m_ctx.errorActive
        && !m_ctx.serviceMode
        && isPhysicalSensorEvent(e);
    const bool endBandEvent =
        (e == Event::END_BLOCKED || e == Event::END_CLEARED)
        && endAndChuteSensorsAllowed;
    const bool chuteFillEvent =
        (e == Event::CHUTE_BLOCKED || e == Event::CHUTE_CLEARED)
        && endAndChuteSensorsAllowed;
    const bool workpieceTrackingEvent =
        isWorkpieceTrackingSensorEvent(e)
        && canRouteWorkpieceTracking()
        && !m_ctx.serviceMode;

    // END_CLEARED-Flag vor WP-Manager setzen, damit canAcceptNetworkHandover() nach
    // Bandende-Freigabe sofort READY an Anlage 1 senden kann.
    const bool endClearedContextEarly =
        e == Event::END_CLEARED && !m_ctx.serviceMode;
    if (endClearedContextEarly) {
        updateContextFlags(e);
    }

    // Phase 1: Subsysteme — END/CHUTE auch bei errorActive (Entleeren im ErrorState)
    if (errorRecoverySensorEvent && m_errorHandler != nullptr) {
        routeToErrorHandler(e);
    }
    if (bandSubsystemsAllowed || endBandEvent || chuteFillEvent || workpieceTrackingEvent
        || entryWhileEndBlocked) {
        routeToErrorHandler(e);
        if (bandSubsystemsAllowed || endBandEvent || chuteFillEvent || workpieceTrackingEvent
            || entryWhileEndBlocked) {
            routeToWorkpieceManager(e);
        }
    } else if (m_ctx.serviceMode
               && !m_ctx.eStopActive
               && !m_ctx.errorActive
               && !m_ctx.connectionLost) {
        routeToErrorHandler(e);
    }

    // Phase 2: Kontext-Flags
    if (!endClearedContextEarly) {
        updateContextFlags(e);
    }

    if (errorRecoverySensorEvent && m_errorHandler != nullptr) {
        m_errorHandler->checkActiveErrorClearedByEvent(e);
    }

    // Phase 3: FSM — nur vorverarbeitete + explizit erlaubte Events
    std::vector<Event> fsmEvents = preprocessForFsm(e);
    if (e == Event::END_CLEARED && !m_ctx.serviceMode) {
        resumeEntryAfterEndCleared(fsmEvents);
    }
    for (Event fsmEvent : fsmEvents) {
        routeToFsm(fsmEvent);
    }

    if (m_ctx.errorActive) {
        enforceMotorStoppedDuringError();
    }
}

void EventManager::enforceMotorStoppedDuringError() {
    if (!m_ctx.errorActive || m_context == nullptr) {
        return;
    }

    const bool wasMotorOn = m_ctx.motorOn || m_ctx.motorSlow;
    m_ctx.motorOn = false;
    m_ctx.motorSlow = false;

    if (I_Dispatcher* dispatcher = m_context->getDispatcher()) {
        dispatcher->driveStop();
    }

    if (wasMotorOn) {
        DETAIL_LOG( "[EventManager] Fehler aktiv: Motor aus "
                  << "(unabhaengig von END_BLOCKED / weiteren WPs auf dem Band).");
    }
}

void EventManager::updateContextFlags(Event e) {
    switch (e) {
    case Event::ESTOP_PRESSED:
    case Event::PARTNER_ESTOP_PRESSED:
    case Event::PARTNER_ESTOP_SAFETY_SYNC:
        m_ctx.eStopActive = true;
        break;

    case Event::ESTOP_RELEASED:
    case Event::PARTNER_ESTOP_RELEASED:
        // Bei Connection_lost Estop-Sicherheitszustand halten bis koordinierter Reset.
        if (!(m_ctx.connectionLost && m_ctx.eStopActive)) {
            m_ctx.eStopActive = false;
        }
        break;

    case Event::END_BLOCKED:
        m_ctx.endBlocked = true;
        break;

    case Event::END_CLEARED:
        m_ctx.endBlocked = false;
        break;

    case Event::HEIGHT_BLOCKED:
    case Event::HEIGHT_ERROR:
        m_ctx.heightBlocked = true;
        break;

    case Event::HEIGHT_CLEARED:
        m_ctx.heightBlocked = false;
        break;

    case Event::ENTRY_CLEARED:
        m_entryDeferredWhileEndBlocked = false;
        break;

    case Event::ENTRY_FREI:
        m_ctx.entryFrei = true;
        break;

    case Event::ENTRY_NOT_FREI:
        m_ctx.entryFrei = false;
        break;

    case Event::CONNECTION_LOST:
        m_ctx.connectionLost = true;
        m_ctx.partnerCalibrationReady = false;
        break;

    case Event::CONNECTION_RESTORED:
        m_ctx.connectionLost = false;
        break;

    case Event::DATA_EXIST:
        m_ctx.calibrationDataReady = true;
        break;

    case Event::DATA_NOT_EXIST:
        m_ctx.calibrationDataReady = false;
        break;

    case Event::PARTNER_CALIB_READY:
        m_ctx.partnerCalibrationReady = true;
        break;

    case Event::PARTNER_CALIB_NOT_READY:
        m_ctx.partnerCalibrationReady = false;
        break;

    case Event::ERROR:
        m_ctx.errorActive = true;
        break;

    default:
        break;
    }
}

std::vector<Event> EventManager::preprocessForFsm(Event e) {
    std::vector<Event> fsmEvents;

    if (m_ctx.errorActive) {
        if (e == Event::MOTOR_RUN || e == Event::SET_SLOW || e == Event::SET_FAST) {
            enforceMotorStoppedDuringError();
            return fsmEvents;
        }
        if (e == Event::END_BLOCKED) {
            enforceMotorStoppedDuringError();
            return fsmEvents;
        }
    }

    switch (e) {
    // --- START: Roh-Event → kurz/lang (FSM sieht nie START_PRESSED/RELEASED) ---
    case Event::START_PRESSED:
        m_startPressedStartTime = std::chrono::steady_clock::now();
        m_startButtonHeld = true;
        DETAIL_LOG( "[EventManager] START gedrueckt (lokal)");
        fsmEvents.push_back(Event::START_PRESSED);
        break;

    case Event::START_RELEASED: {
        m_startButtonHeld = false;
        const auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - m_startPressedStartTime);

        if (duration >= START_LONG_THRESHOLD) {
            DETAIL_LOG( "[EventManager] START lang (" << duration.count() << "ms)");
            fsmEvents.push_back(Event::START_LONG_PRESSED);
        } else {
            DETAIL_LOG( "[EventManager] START kurz (" << duration.count() << "ms)");
            fsmEvents.push_back(Event::START_SHORT_PRESSED);
        }
        break;
    }

    case Event::PARTNER_START_PRESSED:
        DETAIL_LOG( "[EventManager] Partner-START gedrueckt (nur lokal wirksam).");
        break;

    case Event::PARTNER_START_RELEASED:
        DETAIL_LOG( "[EventManager] Partner-START losgelassen (kein Moduswechsel).");
        break;

    // --- Einlauf-Zone: Sensor-Events → Motor-/Freigabe-Signale fuer FSM ---
    case Event::ENTRY_BLOCKED:
        if (m_ctx.serviceMode) {
            fsmEvents.push_back(e);
            break;
        }
        if (m_ctx.endBlocked && m_ctx.entryFrei
            && m_ctx.position == AnlagenPosition::VORN_ANLAGE_1) {
            m_entryDeferredWhileEndBlocked = true;
            DETAIL_LOG( "[EventManager] Entry blockiert waehrend Bandende belegt "
                     << "-> WP wird angelegt, Motor wartet auf END_CLEARED."
                     );
        } else if (!m_ctx.entryFrei && !m_ctx.endBlocked
                   && m_ctx.position == AnlagenPosition::VORN_ANLAGE_1
                   && m_ctx.runningMode
                   && m_wpManager != nullptr
                   && m_wpManager->hasLeadingWorkpieceBeforeHeightSensor()) {
            // EARLY_ENTRY wird bereits in handleEvent() vor WP-Routing behandelt.
            break;
        }
        fsmEvents.push_back(Event::ENTRY_NOT_FREI);
        if (m_ctx.entryFrei && !m_ctx.endBlocked
            && m_ctx.position == AnlagenPosition::VORN_ANLAGE_1) {
            fsmEvents.push_back(Event::MOTOR_RUN);
        }
        m_ctx.entryFrei = false;
        break;

    case Event::ENTRY_CLEARED:
    case Event::EJECTOR_BLOCKED:
    case Event::EJECTOR_CLEARED:
    case Event::CHUTE_BLOCKED:
        if (m_ctx.serviceMode) {
            fsmEvents.push_back(e);
        }
        break;

    case Event::HEIGHT_BLOCKED:
        if (m_ctx.serviceMode) {
            fsmEvents.push_back(e);
        } else if (m_ctx.position == AnlagenPosition::VORN_ANLAGE_1
                   || m_ctx.position == AnlagenPosition::HINTEN_ANLAGE_2) {
            fsmEvents.push_back(Event::ENTRY_FREI);
            m_ctx.entryFrei = true;
        }
        break;

    case Event::HEIGHT_CLEARED:
        if (m_ctx.serviceMode) {
            fsmEvents.push_back(e);
        } else if (!m_ctx.errorActive) {
            DETAIL_LOG( "[EventManager] SEND SET_FAST: trigger=HEIGHT_CLEARED");
            fsmEvents.push_back(Event::SET_FAST);
        }
        break;

    case Event::END_BLOCKED:
        if (m_ctx.serviceMode) {
            fsmEvents.push_back(e);
        } else {
            const bool trailingWpsNeedMotor =
                m_wpManager != nullptr && m_wpManager->hasWorkpiecesNeedingMotor();
            if (!trailingWpsNeedMotor) {
                const bool wpWaitingAtEndOnA1 =
                    m_ctx.position == AnlagenPosition::VORN_ANLAGE_1
                    && m_wpManager != nullptr
                    && m_wpManager->getFirstWorkpieceAt(WpLocation::AT_END) != nullptr
                    && !m_wpManager->isHandoverMotorActive();
                const bool forceStopAtEnd =
                    m_ctx.position == AnlagenPosition::HINTEN_ANLAGE_2
                    || wpWaitingAtEndOnA1
                    || (m_ctx.position == AnlagenPosition::VORN_ANLAGE_1
                        && m_wpManager != nullptr
                        && m_wpManager->isAwaitingHandoverReady()
                        && !m_wpManager->isHandoverMotorActive());
                if (forceStopAtEnd
                    || (!(m_wpManager != nullptr && m_wpManager->isHandoverMotorActive())
                        && (m_wpManager == nullptr || m_wpManager->shouldAllowMotorStop()))) {
                    fsmEvents.push_back(Event::MOTOR_STOP);
                }
            } else {
                DETAIL_LOG( "[EventManager] MOTOR_STOP bei END_BLOCKED unterdrueckt "
                          << "(weitere WPs auf dem Band).");
            }
            fsmEvents.push_back(Event::END_BLOCKED);
        }
        break;

    case Event::END_CLEARED:
        if (m_ctx.serviceMode) {
            fsmEvents.push_back(e);
        }
        break;

    case Event::SERVICE_EMPTY_BELT_READY:
        fsmEvents.push_back(e);
        break;

    case Event::BAND_EMPTY:
        if (m_ctx.serviceMode) {
            fsmEvents.push_back(e);
        } else {
            fsmEvents.push_back(Event::BAND_EMPTY);
            if (m_ctx.position == AnlagenPosition::VORN_ANLAGE_1) {
                fsmEvents.push_back(Event::ENTRY_FREI);
            }
        }
        break;

    // --- Netzwerk ---
    case Event::CONNECTION_LOST:
        DETAIL_LOG( "[EventManager] Verbindung zum Partner verloren.");
        fsmEvents.push_back(Event::CONNECTION_LOST);
        break;

    case Event::CONNECTION_RESTORED: {
        DETAIL_LOG( "[EventManager] Verbindung zum Partner wiederhergestellt.");
        fsmEvents.push_back(Event::CONNECTION_RESTORED);
        break;
    }

    default:
        if (e == Event::MOTOR_RUN && m_wpManager != nullptr
            && m_ctx.position == AnlagenPosition::VORN_ANLAGE_1
            && m_wpManager->isAnlage1MotorPausedForPartnerHandover()) {
            DETAIL_LOG( "[EventManager] MOTOR_RUN auf Anlage 1 unterdrueckt "
                      << "(Partner pausiert, WP am Bandende).");
            break;
        }
        if (e == Event::MOTOR_RUN && m_wpManager != nullptr
            && m_ctx.position == AnlagenPosition::HINTEN_ANLAGE_2) {
            if (m_ctx.endBlocked
                || m_wpManager->getFirstWorkpieceAt(WpLocation::AT_END) != nullptr
                || !m_wpManager->isAnlage2Ready()) {
                DETAIL_LOG( "[EventManager] MOTOR_RUN auf Anlage 2 unterdrueckt "
                          << "(Bandende belegt oder nicht bereit).");
                break;
            }
        }
        if (e == Event::MOTOR_STOP && m_wpManager != nullptr) {
            if (m_wpManager->isHandoverMotorActive()) {
                DETAIL_LOG( "[EventManager] MOTOR_STOP unterdrueckt (WP-Uebergabe laeuft)."
                         );
                break;
            }
            if (m_wpManager->hasWorkpiecesNeedingMotor()) {
                DETAIL_LOG( "[EventManager] MOTOR_STOP unterdrueckt (weitere WPs auf Band)."
                         );
                break;
            }
        }
        if (isFsmPassthroughEvent(e)) {
            fsmEvents.push_back(e);
        }
        break;
    }

    return fsmEvents;
}

void EventManager::resumeEntryAfterEndCleared(std::vector<Event>& fsmEvents) {
    if (m_wpManager == nullptr) {
        m_entryDeferredWhileEndBlocked = false;
        return;
    }

    if (m_ctx.errorActive) {
        return;
    }

    if (m_ctx.position == AnlagenPosition::VORN_ANLAGE_1
        && m_wpManager->getFirstWorkpieceAt(WpLocation::AT_END) != nullptr) {
        return;
    }

    m_wpManager->discardGhostEntryWorkpieceIfSensorClear();

    Workpiece* waitingWp = m_wpManager->getFirstWorkpieceAt(WpLocation::AT_ENTRY);

    if (m_entryDeferredWhileEndBlocked
        && m_ctx.position == AnlagenPosition::VORN_ANLAGE_1) {
        m_entryDeferredWhileEndBlocked = false;
    }

    if (!m_ctx.entryFrei && isEntryLightBarrierClear(m_context)) {
        DETAIL_LOG( "[EventManager] Einlauf-Flag blockiert, LS aber frei -> ENTRY_FREI synchronisiert.");
        m_ctx.entryFrei = true;
    }

    if (waitingWp == nullptr
        && m_ctx.position == AnlagenPosition::VORN_ANLAGE_1
        && !m_ctx.entryFrei
        && isEntryLightBarrierBlocked(m_context)) {
        m_wpManager->addWorkpiece();
        waitingWp = m_wpManager->getFirstWorkpieceAt(WpLocation::AT_ENTRY);
        DETAIL_LOG( "[EventManager] Bandende frei: Einlauf noch blockiert "
                  << "-> WP nachtraeglich angelegt (id="
                  << (waitingWp != nullptr ? waitingWp->getId() : -1) << ").");
    }

    const bool hasActiveWorkpieces = m_wpManager->hasActiveWorkpieces();
    if (!hasActiveWorkpieces) {
        return;
    }

    if (m_ctx.position == AnlagenPosition::HINTEN_ANLAGE_2
        && (m_ctx.endBlocked
            || m_wpManager->getFirstWorkpieceAt(WpLocation::AT_END) != nullptr
            || !m_wpManager->isAnlage2Ready())) {
        DETAIL_LOG( "[EventManager] Anlage 2: END_CLEARED – Motor bleibt aus "
                  << "(Bandende noch nicht frei fuer weiteren Betrieb).");
        return;
    }

    fsmEvents.push_back(Event::MOTOR_RUN);
    const bool useSlow = m_wpManager->shouldUseSlowMotor();
    if (useSlow) {
        fsmEvents.push_back(Event::SET_SLOW);
    }

    if (waitingWp != nullptr) {
        DETAIL_LOG( "[EventManager] Bandende frei -> Entry-WP id="
                  << waitingWp->getId() << " + aktive WPs, MOTOR_RUN"
                  << (useSlow ? " Slow" : " Fast"));
    } else {
        DETAIL_LOG( "[EventManager] Bandende frei -> "
                  << "weitere aktive WPs auf Band, MOTOR_RUN"
                  << (useSlow ? " Slow" : " Fast"));
    }
}

void EventManager::handleTakt() {
    if (m_ctx.errorActive) {
        enforceMotorStoppedDuringError();
    }

    if (m_context != nullptr && m_ctx.serviceMode && m_ctx.motorOn) {
        m_context->incrementServiceTickCounter();
    }

    if (m_wpManager == nullptr) {
        return;
    }

    // Weiche/Auswerfer-Ruecksetzung auch bei stehendem Band
    m_wpManager->taktActuators();

    const bool trackingAllowed =
        canRouteWorkpieceTracking() && !m_ctx.serviceMode;
    const bool taktAllowed =
        m_ctx.motorOn
        && (canRouteToBandSubsystems()
            || (trackingAllowed && m_wpManager->hasActiveWorkpieces()));

    if (!taktAllowed) {
        return;
    }

    m_wpManager->takt(m_ctx.motorSlow);
}

void EventManager::routeToWorkpieceManager(Event e) {
    if (m_wpManager == nullptr) {
        return;
    }

    switch (e) {
    case Event::ENTRY_BLOCKED:
        if (m_ctx.entryFrei
            && (m_ctx.position == AnlagenPosition::VORN_ANLAGE_1 || m_ctx.serviceMode)) {
            if (m_wpManager->getFirstWorkpieceAt(WpLocation::AT_ENTRY) == nullptr
                && (m_ctx.serviceMode || isEntryLightBarrierBlocked(m_context))) {
                m_wpManager->addWorkpiece();
            } else if (!m_ctx.serviceMode && isEntryLightBarrierClear(m_context)) {
                DETAIL_LOG( "[EventManager] ENTRY_BLOCKED ignoriert: Einlauf-LS ist frei "
                          << "(kein physisches Teil).");
            }
        } else if (m_ctx.position == AnlagenPosition::HINTEN_ANLAGE_2) {
            m_wpManager->handleSensorSignal(e);
        }
        break;

    case Event::CHUTE_BLOCKED:
    case Event::CHUTE_CLEARED: {
        const bool noActiveWps =
            !m_wpManager->hasActiveWorkpieces() && !m_ctx.serviceMode;
        if (noActiveWps
            && !isAllowedSensorWithoutWorkpieces(
                e, m_ctx.position, m_ctx.entryFrei, m_ctx.endBlocked, m_wpManager)) {
            break;
        }
        if (!m_ctx.motorOn && noActiveWps && !isChuteSensorEvent(e)) {
            break;
        }
        m_wpManager->handleSensorSignal(e);
        break;
    }

    case Event::ENTRY_CLEARED:
    case Event::HEIGHT_BLOCKED:
    case Event::HEIGHT_CLEARED:
    case Event::HEIGHT_ERROR:
    case Event::METAL_DETECTED:
    case Event::METAL_CLEARED:
    case Event::EJECTOR_BLOCKED:
    case Event::EJECTOR_CLEARED:
    case Event::END_BLOCKED:
    case Event::END_CLEARED:
        if (!m_ctx.motorOn && !m_wpManager->hasActiveWorkpieces() && !m_ctx.serviceMode) {
            break;
        }
        m_wpManager->handleSensorSignal(e);
        break;

    default:
        break;
    }
}

void EventManager::routeToErrorHandler(Event e) {
    if (m_errorHandler == nullptr) {
        return;
    }

    switch (e) {
    case Event::ENTRY_BLOCKED:
    case Event::ENTRY_CLEARED:
    case Event::END_BLOCKED:
    case Event::END_CLEARED:
    case Event::CHUTE_BLOCKED:
    case Event::CHUTE_CLEARED:
    case Event::HEIGHT_BLOCKED:
    case Event::HEIGHT_CLEARED:
    case Event::HEIGHT_ERROR:
        m_errorHandler->processEvent(e);
        break;

    default:
        break;
    }
}

void EventManager::routeToFsm(Event e) {
    if (m_fsm != nullptr) {
        m_fsm->processEvent(e);
    }
}

void EventManager::transitionFsmState(std::unique_ptr<BaseState> nextState) {
    if (m_fsm != nullptr) {
        m_fsm->transitionTo(std::move(nextState));
    }
}

void EventManager::resetStartPressTimer() {
    m_startPressedStartTime = std::chrono::steady_clock::now();
}

void EventManager::setPartnerChuteStatus(bool isFull) {
    m_ctx.partnerChuteFull = isFull;
}

} // namespace logik
