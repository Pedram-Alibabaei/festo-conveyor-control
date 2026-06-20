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
#include "logik/fsm/Fsm.hpp"
#include "logik/fsm/states/BaseState.hpp"
#include "dispatcher/i_Dispatcher.hpp"

#include <iostream>

namespace {

constexpr std::chrono::milliseconds START_LONG_THRESHOLD{1500};

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

    std::cout << "[EventManager] Initialisiert (Position "
              << static_cast<int>(pos) << "). Rolle: Orchestrator + PlantContext."
              << std::endl;
}

bool EventManager::canRouteToBandSubsystems() const {
    return !m_ctx.eStopActive
        && !m_ctx.errorActive
        && !m_ctx.endBlocked
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
        }
    }

    const bool bandSubsystemsAllowed =
        canRouteToBandSubsystems() && !m_ctx.serviceMode;
    const bool endBandEvent =
        (e == Event::END_BLOCKED || e == Event::END_CLEARED) && !m_ctx.serviceMode;
    const bool chuteFillEvent =
        (e == Event::CHUTE_BLOCKED || e == Event::CHUTE_CLEARED) && !m_ctx.serviceMode;

    // Phase 1: Subsysteme — END/CHUTE auch bei errorActive (Entleeren im ErrorState)
    if (bandSubsystemsAllowed || endBandEvent || chuteFillEvent) {
        routeToErrorHandler(e);
        if (bandSubsystemsAllowed || endBandEvent || chuteFillEvent) {
            routeToWorkpieceManager(e);
        }
    } else if (canRouteToBandSubsystems() && m_ctx.serviceMode) {
        routeToErrorHandler(e);
    }

    // Phase 2: Kontext-Flags
    updateContextFlags(e);

    // Phase 3: FSM — nur vorverarbeitete + explizit erlaubte Events
    std::vector<Event> fsmEvents = preprocessForFsm(e);
    if (e == Event::END_CLEARED && !m_ctx.serviceMode) {
        resumeEntryAfterEndCleared(fsmEvents);
    }
    for (Event fsmEvent : fsmEvents) {
        routeToFsm(fsmEvent);
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
        m_entryDeferredWhileOccupied = false;
        break;

    default:
        break;
    }
}

std::vector<Event> EventManager::preprocessForFsm(Event e) {
    std::vector<Event> fsmEvents;

    switch (e) {
    // --- START: Roh-Event → kurz/lang (FSM sieht nie START_PRESSED/RELEASED) ---
    case Event::START_PRESSED:
        m_startPressedStartTime = std::chrono::steady_clock::now();
        m_startButtonHeld = true;
        std::cout << "[EventManager] START gedrueckt (lokal)" << std::endl;
        fsmEvents.push_back(Event::START_PRESSED);
        break;

    case Event::START_RELEASED: {
        m_startButtonHeld = false;
        const auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - m_startPressedStartTime);

        if (duration >= START_LONG_THRESHOLD) {
            std::cout << "[EventManager] START lang (" << duration.count() << "ms)" << std::endl;
            fsmEvents.push_back(Event::START_LONG_PRESSED);
        } else {
            std::cout << "[EventManager] START kurz (" << duration.count() << "ms)" << std::endl;
            fsmEvents.push_back(Event::START_SHORT_PRESSED);
        }
        break;
    }

    case Event::PARTNER_START_PRESSED:
        std::cout << "[EventManager] Partner-START gedrueckt (nur lokal wirksam)." << std::endl;
        break;

    case Event::PARTNER_START_RELEASED:
        std::cout << "[EventManager] Partner-START losgelassen (kein Moduswechsel)." << std::endl;
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
            std::cout << "[EventManager] Entry blockiert waehrend Bandende belegt -> WP wartet."
                      << std::endl;
        } else if (!m_ctx.entryFrei && !m_ctx.endBlocked
                   && m_ctx.position == AnlagenPosition::VORN_ANLAGE_1) {
            m_entryDeferredWhileOccupied = true;
            std::cout << "[EventManager] Entry-Signal waehrend Einlauf gesperrt -> "
                      << "WP wird nach Freigabe angelegt." << std::endl;
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
        } else if (m_ctx.position == AnlagenPosition::VORN_ANLAGE_1) {
            fsmEvents.push_back(Event::ENTRY_FREI);
            m_ctx.entryFrei = true;
            resumeDeferredEntryWhileOccupied(fsmEvents);
        }
        break;

    case Event::HEIGHT_CLEARED:
        if (m_ctx.serviceMode) {
            fsmEvents.push_back(e);
        } else {
            std::cout << "[EventManager] SEND SET_FAST: trigger=HEIGHT_CLEARED" << std::endl;
            fsmEvents.push_back(Event::SET_FAST);
        }
        break;

    case Event::END_BLOCKED:
        if (m_ctx.serviceMode) {
            fsmEvents.push_back(e);
        } else {
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
            fsmEvents.push_back(Event::END_BLOCKED);
        }
        break;

    case Event::END_CLEARED:
        if (m_ctx.serviceMode) {
            fsmEvents.push_back(e);
        }
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
        std::cout << "[EventManager] Verbindung zum Partner verloren." << std::endl;
        fsmEvents.push_back(Event::CONNECTION_LOST);
        break;

    case Event::CONNECTION_RESTORED: {
        std::cout << "[EventManager] Verbindung zum Partner wiederhergestellt." << std::endl;
        fsmEvents.push_back(Event::CONNECTION_RESTORED);
        break;
    }

    default:
        if (e == Event::MOTOR_RUN && m_wpManager != nullptr
            && m_ctx.position == AnlagenPosition::HINTEN_ANLAGE_2) {
            if (m_ctx.endBlocked
                || m_wpManager->getFirstWorkpieceAt(WpLocation::AT_END) != nullptr
                || !m_wpManager->isAnlage2Ready()) {
                std::cout << "[EventManager] MOTOR_RUN auf Anlage 2 unterdrueckt "
                          << "(Bandende belegt oder nicht bereit)." << std::endl;
                break;
            }
        }
        if (e == Event::MOTOR_STOP && m_wpManager != nullptr) {
            if (m_wpManager->isHandoverMotorActive()) {
                std::cout << "[EventManager] MOTOR_STOP unterdrueckt (WP-Uebergabe laeuft)."
                          << std::endl;
                break;
            }
            const bool stopForEndHandover =
                m_ctx.position == AnlagenPosition::VORN_ANLAGE_1
                && m_wpManager->getFirstWorkpieceAt(WpLocation::AT_END) != nullptr;
            if (!stopForEndHandover && m_wpManager->hasWorkpiecesNeedingMotor()) {
                std::cout << "[EventManager] MOTOR_STOP unterdrueckt (weitere WPs auf Band)."
                          << std::endl;
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

void EventManager::resumeDeferredEntryWhileOccupied(std::vector<Event>& fsmEvents) {
    if (m_wpManager == nullptr
        || m_ctx.position != AnlagenPosition::VORN_ANLAGE_1
        || !m_entryDeferredWhileOccupied) {
        return;
    }
    if (m_ctx.endBlocked) {
        return;
    }
    if (m_wpManager->getFirstWorkpieceAt(WpLocation::AT_ENTRY) != nullptr) {
        m_entryDeferredWhileOccupied = false;
        return;
    }

    m_entryDeferredWhileOccupied = false;
    m_wpManager->addWorkpiece();
    fsmEvents.push_back(Event::ENTRY_NOT_FREI);
    fsmEvents.push_back(Event::MOTOR_RUN);
    m_ctx.entryFrei = false;
    std::cout << "[EventManager] Einlauf frei -> aufgeschobenes Entry-WP angelegt + MOTOR_RUN."
              << std::endl;
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

    Workpiece* waitingWp = m_wpManager->getFirstWorkpieceAt(WpLocation::AT_ENTRY);

    if (m_entryDeferredWhileEndBlocked
        && m_ctx.position == AnlagenPosition::VORN_ANLAGE_1) {
        if (waitingWp == nullptr) {
            m_wpManager->addWorkpiece();
            waitingWp = m_wpManager->getFirstWorkpieceAt(WpLocation::AT_ENTRY);
        }
        m_entryDeferredWhileEndBlocked = false;
    }

    const bool hasActiveWorkpieces = m_wpManager->hasActiveWorkpieces();
    if (!hasActiveWorkpieces) {
        return;
    }

    if (m_ctx.position == AnlagenPosition::HINTEN_ANLAGE_2
        && (m_ctx.endBlocked
            || m_wpManager->getFirstWorkpieceAt(WpLocation::AT_END) != nullptr
            || !m_wpManager->isAnlage2Ready())) {
        std::cout << "[EventManager] Anlage 2: END_CLEARED – Motor bleibt aus "
                  << "(Bandende noch nicht frei fuer weiteren Betrieb)." << std::endl;
        return;
    }

    fsmEvents.push_back(Event::MOTOR_RUN);
    const bool useSlow = m_wpManager->shouldUseSlowMotor();
    if (useSlow) {
        fsmEvents.push_back(Event::SET_SLOW);
    }

    if (waitingWp != nullptr) {
        std::cout << "[EventManager] Bandende frei -> Entry-WP id="
                  << waitingWp->getId() << " + aktive WPs, MOTOR_RUN"
                  << (useSlow ? " Slow" : " Fast") << std::endl;
    } else {
        std::cout << "[EventManager] Bandende frei -> "
                  << "weitere aktive WPs auf Band, MOTOR_RUN"
                  << (useSlow ? " Slow" : " Fast") << std::endl;
    }
}

void EventManager::handleTakt() {
    if (m_context != nullptr && m_ctx.serviceMode && m_ctx.motorOn) {
        m_context->incrementServiceTickCounter();
    }

    if (m_wpManager == nullptr) {
        return;
    }

    // Weiche/Auswerfer-Ruecksetzung auch bei stehendem Band
    m_wpManager->taktActuators();

    if (!canRouteToBandSubsystems() || !m_ctx.motorOn) {
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
        if (m_ctx.entryFrei && !m_ctx.endBlocked
            && (m_ctx.position == AnlagenPosition::VORN_ANLAGE_1 || m_ctx.serviceMode)) {
            m_wpManager->addWorkpiece();
        } else if (m_ctx.position == AnlagenPosition::HINTEN_ANLAGE_2) {
            m_wpManager->handleSensorSignal(e);
        }
        break;

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
    case Event::CHUTE_BLOCKED:
    case Event::CHUTE_CLEARED:
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
