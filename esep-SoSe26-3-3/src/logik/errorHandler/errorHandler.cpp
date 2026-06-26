/**
 * @file errorHandler.cpp
 * @brief Implementierung der Watchdog-Überwachung und der zentralen Fehler-Eskalation.
 *
 * SoSe2026 Team 3.3
 */

#include "logik/errorHandler/errorHandler.hpp"
#include "logik/logik.hpp"
#include "logik/eventManager/eventManager.hpp"
#include "logik/workpieceManager/WorkpieceManager.hpp"
#include "dispatcher/i_Dispatcher.hpp"
#include "shared/detailLog.hpp"
#include <iostream>

namespace {

const char* wpLocationExpectedArrival(WpLocation location) {
    switch (location) {
    case WpLocation::AT_ENTRY:
        return "ENTRY_BLOCKED (Teil muss am Einlauf ankommen)";
    case WpLocation::TRANSIT_TO_HEIGHT:
        return "HEIGHT_BLOCKED (Hoehensensor / Lichttunnel)";
    case WpLocation::TRANSIT_TO_EJECTOR:
        return "EJECTOR_BLOCKED (Sortierweiche)";
    case WpLocation::TRANSIT_TO_CHUTE:
        return "CHUTE_BLOCKED (Rutsche)";
    case WpLocation::TRANSIT_TO_END:
        return "END_BLOCKED (Bandende / Uebergabe)";
    default:
        return "Sensor-Ereignis in naechster Zone";
    }
}

const char* wpLocationName(WpLocation location) {
    switch (location) {
    case WpLocation::AT_ENTRY:
        return "AT_ENTRY (Einlauf)";
    case WpLocation::TRANSIT_TO_HEIGHT:
        return "TRANSIT_TO_HEIGHT (Fahrt zum Hoehensensor)";
    case WpLocation::AT_HEIGHT_SENSOR:
        return "AT_HEIGHT_SENSOR (Hoehenmessung)";
    case WpLocation::TRANSIT_TO_EJECTOR:
        return "TRANSIT_TO_EJECTOR (Fahrt zur Sortierweiche)";
    case WpLocation::AT_EJECTOR:
        return "AT_EJECTOR (Sortierweiche)";
    case WpLocation::TRANSIT_TO_CHUTE:
        return "TRANSIT_TO_CHUTE (Fahrt zur Rutsche)";
    case WpLocation::AT_CHUTE:
        return "AT_CHUTE (Rutsche)";
    case WpLocation::TRANSIT_TO_END:
        return "TRANSIT_TO_END (Fahrt zum Bandende)";
    case WpLocation::AT_END:
        return "AT_END (Bandende / Uebergabe)";
    case WpLocation::UNKNOWN_LOCATION:
    default:
        return "UNKNOWN_LOCATION";
    }
}

const char* eventSensorName(logik::Event triggerEvent) {
    switch (triggerEvent) {
    case logik::Event::ENTRY_BLOCKED:
        return "ENTRY_BLOCKED (Einlauf-Lichtschranke blockiert)";
    case logik::Event::ENTRY_CLEARED:
        return "ENTRY_CLEARED (Einlauf-Lichtschranke frei)";
    case logik::Event::HEIGHT_BLOCKED:
        return "HEIGHT_BLOCKED (Hoehenmessung / Lichttunnel blockiert)";
    case logik::Event::HEIGHT_CLEARED:
        return "HEIGHT_CLEARED (Hoehenmessung / Lichttunnel frei)";
    case logik::Event::HEIGHT_ERROR:
        return "HEIGHT_ERROR (Hoehenmessung / Geometrie-Fehler)";
    case logik::Event::EJECTOR_BLOCKED:
        return "EJECTOR_BLOCKED (Sortierweiche blockiert)";
    case logik::Event::EJECTOR_CLEARED:
        return "EJECTOR_CLEARED (Sortierweiche frei)";
    case logik::Event::METAL_DETECTED:
        return "METAL_DETECTED (Induktivsensor)";
    case logik::Event::METAL_CLEARED:
        return "METAL_CLEARED (Induktivsensor frei)";
    case logik::Event::END_BLOCKED:
        return "END_BLOCKED (Bandende-Lichtschranke blockiert)";
    case logik::Event::END_CLEARED:
        return "END_CLEARED (Bandende-Lichtschranke frei)";
    case logik::Event::CHUTE_BLOCKED:
        return "CHUTE_BLOCKED (Rutsche blockiert)";
    case logik::Event::CHUTE_CLEARED:
        return "CHUTE_CLEARED (Rutsche frei)";
    case logik::Event::REQ_TO_GIVE_WP:
        return "REQ_TO_GIVE_WP (Uebergabe-Anfrage von Anlage 1)";
    case logik::Event::READY_TO_RECEIVE_WP:
        return "READY_TO_RECEIVE_WP (Anlage 2 bereit zur Uebernahme)";
    case logik::Event::PARTNER_HANDOVER_COMPLETE:
        return "PARTNER_HANDOVER_COMPLETE (Anlage 2: WP am Einlauf)";
    case logik::Event::PARTNER_HANDOVER_WP_LOST:
        return "PARTNER_HANDOVER_WP_LOST (Anlage 2: WP bei Uebergabe verloren)";
    default:
        return "UNBEKANNTES_SENSOR-EVENT";
    }
}

} // namespace

namespace {

void notifyPartnerChuteFull(logik::Logik* context) {
    if (context != nullptr && context->getDispatcher() != nullptr) {
        context->getDispatcher()->sendeChuteStats(true);
    }
}

} // namespace

namespace logik {

ErrorHandler::ErrorHandler(Logik* context)
    : m_context(context),
      m_isEntryTimerRunning(false),
      m_isChuteTimerRunning(false),
      m_isChuteWarningActive(false),
      TIMEOUT_THRESHOLD(1000) // 1000 Millisekunden = 1 Sekunde Watchdog-Zeit
{
}

// ============================================================================
// ZENTRALE FEHLER-ESKALATION & DIAGNOSE-MASSNAHMEN
// ============================================================================
void ErrorHandler::handleEscalation(ErrorType type,
                                    logik::Event triggerEvent,
                                    WpLocation location,
                                    int wpId,
                                    std::uint32_t elapsedTicks,
                                    std::uint32_t expectedLimitTicks,
                                    std::uint32_t toleranceTicks) {
    if (m_context == nullptr) return;

    EventManager* em = m_context->getEventManager();
    if (type == ErrorType::HEIGHT_ERROR
        && em != nullptr
        && em->isErrorActive()
        && m_activeErrorType == ErrorType::HEIGHT_ERROR) {
        return;
    }
    if (type == ErrorType::UNEXPECTED_EVENT
        && em != nullptr
        && em->isErrorActive()
        && m_activeErrorType == ErrorType::UNEXPECTED_EVENT
        && triggerEvent == m_activeErrorTrigger) {
        return;
    }

    // 1. Schieberegister einfrieren
    if (m_context->getEventManager() != nullptr) {
        m_context->getEventManager()->setErrorActive(true);
    }

    WorkpieceManager* wpManager = m_context->getWorkpieceManager();

    if (wpManager != nullptr) {
        if (wpManager->isHandoverReceiveActive()
            && (type == ErrorType::STUCK
                || (type == ErrorType::WP_LOST
                    && (location == WpLocation::AT_ENTRY
                        || location == WpLocation::TRANSIT_TO_HEIGHT)))) {
            wpManager->notifyPartnerHandoverFailed();
        }
        wpManager->resetHandoverState();
    }

    // 2. Diagnostische Ausgaben mit präzisen Typen
    switch (type) {
        case ErrorType::BOTH_CHUTES_FULL:
            std::cout << "\n====================================================" << std::endl;
            std::cout << "[ERROR] CRITICAL: BEIDE RUTSCHEN SIND VOLL!" << std::endl;
            if (wpManager != nullptr) {
                int removeId = wpId;
                if (removeId == -1) {
                    Workpiece* wp = wpManager->getFirstWorkpieceAt(WpLocation::AT_EJECTOR);
                    if (wp != nullptr) {
                        removeId = wp->getId();
                    }
                }
                if (removeId != -1) {
                    std::cout << "-> Massnahme: Loesche WP-ID " << removeId
                              << " an der Weiche aus dem Schieberegister." << std::endl;
                    wpManager->removeWorkpiece(removeId);
                }
                Workpiece* transitWp =
                    wpManager->getFirstWorkpieceAt(WpLocation::TRANSIT_TO_CHUTE);
                if (transitWp != nullptr) {
                    std::cout << "-> Massnahme: Loesche WP-ID " << transitWp->getId()
                              << " auf dem Weg zur Rutsche aus dem Schieberegister." << std::endl;
                    wpManager->removeWorkpiece(transitWp->getId());
                }
                wpManager->resetMetalWaitState();
            }
            std::cout << "-> BEDIENER-ANWEISUNG: Bitte entleeren Sie BEIDE Rutschen manuell." << std::endl;
            std::cout << "====================================================\n" << std::endl;
            break;

        case ErrorType::WP_LOST:
            std::cout << "\n====================================================" << std::endl;
            std::cout << "[ERROR] TRANSIT-FEHLER: WERKSTUECK VERLOREN (WP_LOST)!" << std::endl;
            if (wpId != -1) {
                std::cout << "-> WP-ID: " << wpId << std::endl;
            }
            std::cout << "-> Verloren in Zone: " << wpLocationName(location) << std::endl;
            std::cout << "-> Erwartetes Ereignis: "
                      << wpLocationExpectedArrival(location) << std::endl;
            if (elapsedTicks > 0 && expectedLimitTicks > 0) {
                const std::uint32_t deadline = expectedLimitTicks + toleranceTicks;
                const std::int32_t overdue =
                    static_cast<std::int32_t>(elapsedTicks)
                    - static_cast<std::int32_t>(deadline);
                std::cout << "-> Ist-Dauer: "
                          << elapsedTicks << " Takte (" << (elapsedTicks * 10) << " ms)"
                          << std::endl;
                std::cout << "-> Berechnetes Limit: "
                          << expectedLimitTicks << " Takte (" << (expectedLimitTicks * 10) << " ms)";
                if (toleranceTicks > 0) {
                    std::cout << " + " << toleranceTicks << " Toleranz";
                }
                std::cout << " = spaetestens " << deadline << " Takte ("
                          << (deadline * 10) << " ms)"
                          << std::endl;
                std::cout << "-> Ueberfaellig um "
                          << overdue << " Takte (" << (overdue * 10) << " ms)"
                          << std::endl;
                if (location == WpLocation::TRANSIT_TO_HEIGHT) {
                    std::cout << "-> Limit-Hinweis: Kalibrierung Entry->Height (Normal/Slow-Mix)"
                              << " + Worst-Case-Einlauf + Sicherheit (+50 Takte)"
                              << std::endl;
                }
            }
            if (wpId != -1) {
                std::cout << "-> Massnahme: Loesche WP-ID " << wpId << " automatisch aus dem Schieberegister." << std::endl;
                if (wpManager != nullptr) wpManager->removeWorkpiece(wpId);
            }
            std::cout << "-> BEDIENER-ANWEISUNG: Pruefen Sie, ob das Teil vom Band gefallen ist." << std::endl;
            std::cout << "====================================================\n" << std::endl;
            break;

        case ErrorType::UNEXPECTED_EVENT:
            std::cout << "\n====================================================" << std::endl;
            std::cout << "[ERROR] MANIPULATION: UNERWARTETES HARDWARE-SIGNAL (UNEXPECTED_EVENT)!" << std::endl;
            std::cout << "-> Sensor: " << eventSensorName(triggerEvent)
                      << " schlug unzulaessig an." << std::endl;
            if (wpManager != nullptr
                && m_context->getEventManager() != nullptr
                && !wpManager->hasActiveWorkpieces()) {
                const auto pos = m_context->getEventManager()->context().position;
                if (pos == AnlagenPosition::HINTEN_ANLAGE_2
                    && (triggerEvent == logik::Event::ENTRY_BLOCKED
                        || triggerEvent == logik::Event::ENTRY_CLEARED)) {
                    std::cout << "-> Regel (Anlage 2): Einlauf muss frei sein, solange kein "
                              << "WP von Anlage 1 erwartet wird." << std::endl;
                } else {
                    std::cout << "-> Regel: Ohne aktive WPs sind nur Einlauf (Anlage 1, Q1 frei) "
                              << "und Rutsche erlaubt." << std::endl;
                }
            }
            std::cout << "-> BEDIENER-ANWEISUNG: Bitte entfernen Sie das fremde Objekt vom Sensorbereich." << std::endl;
            std::cout << "====================================================\n" << std::endl;
            break;

        case ErrorType::EARLY_ENTRY:
            std::cout << "\n====================================================" << std::endl;
            std::cout << "[ERROR] EINLAUF-FEHLER: WERKSTUECK ZU FRUEH AM ANFANG GELEGT!" << std::endl;
            if (m_context->getEventManager() != nullptr
                && m_context->getEventManager()->context().position
                    == AnlagenPosition::HINTEN_ANLAGE_2) {
                std::cout << "-> Regel (Anlage 2): Kein weiteres Werkstueck am Einlauf, "
                          << "bevor das vordere den Hoehensensor (HS) erreicht hat." << std::endl;
            } else {
                std::cout << "-> Regel: Kein Werkstueck am Einlauf ablegen, bevor Q1 leuchtet "
                          << "(Signalleuchte Q1 = Einlauf frei)." << std::endl;
            }
            std::cout << "-> Ursache: Das vordere Werkstueck hat den Hoehensensor (HS) "
                      << "noch nicht erreicht." << std::endl;
            std::cout << "-> BEDIENER-ANWEISUNG: Entfernen Sie das zusaetzliche Werkstueck vom Einlauf. "
                      << "Warten Sie, bis der Einlauf freigegeben ist (Q1 bzw. HS-Freigabe)."
                      << std::endl;
            std::cout << "====================================================\n" << std::endl;
            break;

        case ErrorType::HEIGHT_ERROR:
            std::cout << "\n====================================================" << std::endl;
            std::cout << "[ERROR] GEOMETRIE-FEHLER: HOEHENMESSUNG REISSGRENZE (HEIGHT_ERROR)!" << std::endl;
            std::cout << "-> Zone: " << wpLocationName(location) << std::endl;
            if (wpId != -1) {
                std::cout << "-> Massnahme: Loesche fehlerhaftes WP-ID " << wpId << " aus dem System." << std::endl;
                if (wpManager != nullptr) wpManager->removeWorkpiece(wpId);
            }
            std::cout << "-> BEDIENER-ANWEISUNG: Bitte entfernen Sie das Werkstueck MANUELL aus dem Lichttunnel!" << std::endl;
            std::cout << "====================================================\n" << std::endl;
            break;

        case ErrorType::STUCK:
            std::cout << "\n====================================================" << std::endl;
            std::cout << "[ERROR] WATCHDOG: WERKSTUECK STECKEN GEBLIEBEN (STUCK)!" << std::endl;
            std::cout << "-> Blockiert in Zone: " << wpLocationName(location) << std::endl;
            if (location == WpLocation::AT_HEIGHT_SENSOR) {
                std::cout << "-> Ursache: Werkstueck blockiert die Hoehenmessung zu lange "
                          << "(kalibriertes HeightB->HeightC-Limit ueberschritten)." << std::endl;
            }
            if (elapsedTicks > 0 && expectedLimitTicks > 0) {
                const std::uint32_t deadline = expectedLimitTicks + toleranceTicks;
                std::cout << "-> Timing: Blockiert seit "
                          << elapsedTicks << " Takte (" << (elapsedTicks * 10) << " ms)"
                          << ", erlaubt max "
                          << expectedLimitTicks << " Takte (" << (expectedLimitTicks * 10) << " ms)"
                          << " + " << toleranceTicks << " Toleranz" << std::endl;
                std::cout << "-> Timing: Grenze ueberschritten um "
                          << (static_cast<std::int32_t>(elapsedTicks)
                              - static_cast<std::int32_t>(deadline))
                          << " Takte" << std::endl;
            }
            if (wpId != -1) {
                std::cout << "-> Massnahme: Loesche blockiertes WP-ID " << wpId << " aus der Liste." << std::endl;
                if (wpManager != nullptr) wpManager->removeWorkpiece(wpId);
            }
            std::cout << "-> BEDIENER-ANWEISUNG: Der blockierte Bereich MUSS manuell frei gemacht werden!" << std::endl;
            std::cout << "====================================================\n" << std::endl;
            break;
    }

    if (wpManager != nullptr
        && m_context->getEventManager() != nullptr
        && m_context->getEventManager()->context().position == AnlagenPosition::HINTEN_ANLAGE_2
        && (type == ErrorType::WP_LOST || type == ErrorType::STUCK
            || type == ErrorType::HEIGHT_ERROR)) {
        wpManager->notifyPartnerSequenceSync();
    }

    // 3. Band stoppen
    m_hasActiveError = true;
    m_activeErrorType = type;
    m_activeErrorTrigger = triggerEvent;
    m_activeErrorLocation = location;
    m_isEntryTimerRunning = false;

    if (m_context->getEventManager() != nullptr) {
        m_context->getEventManager()->handleEvent(logik::Event::ERROR);
    }

    // WP_LOST / behobene Sensor-Fehler: sofort pruefen ob GoneUnacknowledged moeglich
    if (type == ErrorType::WP_LOST) {
        tryEmitErrorCleared("WP aus Schieberegister entfernt");
    }
}

// ============================================================================
// EVENT-BASIERTE WATCHDOG-STEUERUNG (TRIGGERUNG DER STARTZEITEN)
// ============================================================================
void ErrorHandler::processEvent(logik::Event e) {
    switch (e) {
        // --- EINLAUF (ENTRY) ---
        case logik::Event::ENTRY_BLOCKED:
            if (m_context->getEventManager() != nullptr && m_context->getEventManager()->isServiceMode()) {
                break;
            }
            {
                auto* eventManager = m_context->getEventManager();
                WorkpieceManager* wpManager = m_context->getWorkpieceManager();
                bool expectEntryWp = false;

                if (eventManager != nullptr) {
                    if (eventManager->isEntryFrei()
                        && !eventManager->isEndBlocked()
                        && eventManager->context().position == AnlagenPosition::VORN_ANLAGE_1) {
                        // Anlage 1: neues WP wird direkt danach in addWorkpiece() angelegt.
                        expectEntryWp = true;
                    } else if (eventManager->context().position == AnlagenPosition::HINTEN_ANLAGE_2
                               && wpManager != nullptr) {
                        expectEntryWp =
                            (wpManager->getFirstWorkpieceAt(WpLocation::AT_ENTRY) != nullptr)
                            || wpManager->hasPendingNetworkWorkpiece();
                    }
                }

                if (expectEntryWp) {
                    m_isEntryTimerRunning = true;
                    m_entryStartTime = std::chrono::steady_clock::now();
                }
            }
            break;

        case logik::Event::ENTRY_CLEARED:
            m_isEntryTimerRunning = false;
            break;

        case logik::Event::END_BLOCKED:
        case logik::Event::ERROR:
            m_isEntryTimerRunning = false;
            break;

        case logik::Event::END_CLEARED: {
            m_isEntryTimerRunning = false;
            WorkpieceManager* wpManager = m_context->getWorkpieceManager();
            if (wpManager != nullptr
                && wpManager->getFirstWorkpieceAt(WpLocation::AT_ENTRY) != nullptr) {
                m_isEntryTimerRunning = true;
                m_entryStartTime = std::chrono::steady_clock::now();
            }
            break;
        }

        // --- RUTSCHE (CHUTE) ---
        case logik::Event::CHUTE_BLOCKED: {
            m_isChuteTimerRunning = true;
            m_chuteStartTime = std::chrono::steady_clock::now();
            if (m_context->getEventManager() != nullptr) {
                const int pos = static_cast<int>(m_context->getEventManager()->context().position);
                DETAIL_LOG( "[ErrorHandler] CHUTE_BLOCKED (Anlage " << pos
                          << ") -> Watchdog gestartet.");
            }
            break;
        }

        case logik::Event::CHUTE_CLEARED:
            m_isChuteTimerRunning = false;
            if (!isChutePhysicallyBlocked()) {
                clearChuteWarningState();
            }
            break;

        default:
            break;
    }
}

void ErrorHandler::checkActiveErrorClearedByEvent(logik::Event e) {
    if (m_hasActiveError && doesEventClearActiveError(e)) {
        tryEmitErrorCleared("Sensor-/Zustandsaenderung", e);
    }
}

// ============================================================================
// ZYKLISCHE ZEIT-ÜBERWACHUNG (AUFRUF ALLE 10MS)
// ============================================================================
void ErrorHandler::takt() {
    if (m_context->getEventManager() != nullptr && m_context->getEventManager()->isServiceMode()) {
        return;
    }

    EventManager* em = m_context->getEventManager();
    if (em != nullptr && em->isErrorActive()) {
        if (m_hasActiveError && isActiveErrorConditionCleared()) {
            tryEmitErrorCleared("periodische Pruefung");
        } else if (!m_hasActiveError
                   && isPhysicalErrorConditionStillPresent()
                   && !(em->isErrorActive()
                        && m_activeErrorType == ErrorType::HEIGHT_ERROR)) {
            m_hasActiveError = true;
            std::cout << "[ErrorHandler] Fehlerursache noch aktiv "
                      << "(Sensor blockiert) -> ERROR. Rot blinkt SCHNELL."
                      << std::endl;
            em->handleEvent(logik::Event::ERROR);
        }
    }

    // --- WATCHDOG: EINLAUF-STAU (STUCK) ---
    if (m_isEntryTimerRunning) {
        EventManager* eventManager = m_context->getEventManager();
        WorkpieceManager* wpManager = m_context->getWorkpieceManager();
        const bool queuedBehindLeadingWp =
            wpManager != nullptr
            && wpManager->isEntryQueuedBehindLeadingWorkpiece();
        const bool entryWatchdogAllowed = eventManager != nullptr
            && eventManager->isMotorOn()
            && !eventManager->isEndBlocked()
            && !eventManager->isErrorActive()
            && !queuedBehindLeadingWp;

        if (!entryWatchdogAllowed) {
            // Band steht oder Einlauf wartet regulaer hinter fuehrendem WP – Timer pausieren.
            m_entryStartTime = std::chrono::steady_clock::now();
        } else {
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - m_entryStartTime);

            if (duration >= TIMEOUT_THRESHOLD) {
                m_isEntryTimerRunning = false;

                Workpiece* wp = nullptr;
                if (m_context->getWorkpieceManager() != nullptr) {
                    wp = m_context->getWorkpieceManager()->getFirstWorkpieceAt(WpLocation::AT_ENTRY);
                }

                if (wp != nullptr) {
                    handleEscalation(
                        ErrorType::STUCK, logik::Event::ENTRY_BLOCKED, WpLocation::AT_ENTRY, wp->getId());
                }
            }
        }
    }

    // --- WATCHDOG: RUTSCHE VOLL (Level + Timer, unabhaengig von GPIO-Flanken) ---
    updateChuteLevelWatchdog();
}

bool ErrorHandler::isChutePhysicallyBlocked() const {
    if (m_context == nullptr || m_context->getDispatcher() == nullptr) {
        return false;
    }
    return !m_context->getDispatcher()->isChuteLightBarrierClear();
}

void ErrorHandler::raiseChuteFullWarning() {
    if (m_isChuteWarningActive) {
        return;
    }

    m_isChuteWarningActive = true;
    m_isChuteTimerRunning = false;

    EventManager* em = m_context != nullptr ? m_context->getEventManager() : nullptr;
    if (em == nullptr) {
        return;
    }

    em->setChuteFull(true);
    notifyPartnerChuteFull(m_context);
    em->handleEvent(logik::Event::WARNING);

    const int pos = static_cast<int>(em->context().position);
    std::cout << "[ErrorHandler] Rutsche voll (Watchdog) -> WARNING (Anlage "
              << pos << ")." << std::endl;
}

void ErrorHandler::updateChuteLevelWatchdog() {
    EventManager* em = m_context != nullptr ? m_context->getEventManager() : nullptr;
    if (em == nullptr || em->isServiceMode() || em->isErrorActive()) {
        return;
    }

    if (isChutePhysicallyBlocked()) {
        if (!m_isChuteWarningActive && !m_isChuteTimerRunning) {
            m_isChuteTimerRunning = true;
            m_chuteStartTime = std::chrono::steady_clock::now();
        }

        if (m_isChuteTimerRunning && !m_isChuteWarningActive) {
            const auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - m_chuteStartTime);
            if (duration >= TIMEOUT_THRESHOLD) {
                raiseChuteFullWarning();
            }
        }
    } else {
        m_isChuteTimerRunning = false;
        if (m_isChuteWarningActive || em->isChuteFull()) {
            clearChuteWarningState();
        }
    }
}

void ErrorHandler::ensureChuteWarningRaised() {
    if (m_isChuteWarningActive) {
        return;
    }

    m_isChuteWarningActive = true;
    m_isChuteTimerRunning = false;

    if (m_context != nullptr && m_context->getEventManager() != nullptr) {
        m_context->getEventManager()->setChuteFull(true);
        notifyPartnerChuteFull(m_context);
        m_context->getEventManager()->handleEvent(logik::Event::WARNING);
        const int pos = static_cast<int>(m_context->getEventManager()->context().position);
        std::cout << "[ErrorHandler] Rutsche voll -> WARNING ausgeloest (Anlage "
                  << pos << ")." << std::endl;
    }
}

void ErrorHandler::clearChuteWarningState() {
    EventManager* em = m_context != nullptr ? m_context->getEventManager() : nullptr;
    if (em == nullptr) {
        m_isChuteWarningActive = false;
        m_isChuteTimerRunning = false;
        return;
    }

    if (!m_isChuteWarningActive && !em->isChuteFull()) {
        return;
    }

    m_isChuteWarningActive = false;
    m_isChuteTimerRunning = false;
    em->setChuteFull(false);
    em->handleEvent(logik::Event::WARNING_CLEARED);

    if (m_context->getDispatcher() != nullptr) {
        m_context->getDispatcher()->sendeChuteStats(false);
    }

    std::cout << "[ErrorHandler] Rutsche frei -> WARNING_CLEARED." << std::endl;
}

void ErrorHandler::reconcileChuteWarningAfterErrorRecovery() {
    reconcileChuteWarningOnOperationalEntry();
}

void ErrorHandler::reconcileChuteWarningOnOperationalEntry() {
    EventManager* em = m_context != nullptr ? m_context->getEventManager() : nullptr;
    if (em == nullptr) {
        return;
    }

    const int pos = static_cast<int>(em->context().position);

    if (isChutePhysicallyBlocked()) {
        em->setChuteFull(true);
        m_isChuteWarningActive = true;
        m_isChuteTimerRunning = false;
        notifyPartnerChuteFull(m_context);
        DETAIL_LOG( "[ErrorHandler] Betriebsstart (Anlage " << pos
                  << "): Rutsche-LS blockiert -> WARNING (Level-Check).");
        return;
    }

    m_isChuteTimerRunning = false;
    if (m_isChuteWarningActive || em->isChuteFull()) {
        clearChuteWarningState();
    }
    DETAIL_LOG( "[ErrorHandler] Betriebsstart (Anlage " << pos
              << "): Rutsche-LS frei.");
}

bool ErrorHandler::isTriggerSensorClear(logik::Event trigger) const {
    if (m_context == nullptr || m_context->getDispatcher() == nullptr) {
        return false;
    }

    I_Dispatcher* dispatcher = m_context->getDispatcher();
    EventManager* em = m_context->getEventManager();

    switch (trigger) {
    case logik::Event::ENTRY_BLOCKED:
    case logik::Event::ENTRY_CLEARED:
        return dispatcher->isEntryLightBarrierClear();

    case logik::Event::HEIGHT_BLOCKED:
    case logik::Event::HEIGHT_CLEARED:
    case logik::Event::HEIGHT_ERROR:
        return em != nullptr && !em->isHeightBlocked();

    case logik::Event::END_BLOCKED:
    case logik::Event::END_CLEARED:
        return dispatcher->isEndLightBarrierClear();

    case logik::Event::EJECTOR_BLOCKED:
    case logik::Event::EJECTOR_CLEARED:
        return dispatcher->isEjectorLightBarrierClear();

    case logik::Event::METAL_DETECTED:
    case logik::Event::METAL_CLEARED:
        return dispatcher->isMetalSensorClear();

    default:
        return false;
    }
}

bool ErrorHandler::isPhysicalErrorConditionStillPresent() const {
    if (m_context == nullptr) {
        return false;
    }

    switch (m_activeErrorType) {
    case ErrorType::WP_LOST:
        return false;

    case ErrorType::BOTH_CHUTES_FULL:
        return isChutePhysicallyBlocked();

    case ErrorType::UNEXPECTED_EVENT:
    case ErrorType::EARLY_ENTRY:
        return !isTriggerSensorClear(m_activeErrorTrigger);

    case ErrorType::HEIGHT_ERROR:
        return m_context->getEventManager() != nullptr
            && m_context->getEventManager()->isHeightBlocked();

    case ErrorType::STUCK:
        if (m_activeErrorLocation == WpLocation::AT_ENTRY) {
            return m_context->getDispatcher() != nullptr
                && !m_context->getDispatcher()->isEntryLightBarrierClear();
        }
        if (m_activeErrorLocation == WpLocation::AT_HEIGHT_SENSOR) {
            return m_context->getEventManager() != nullptr
                && m_context->getEventManager()->isHeightBlocked();
        }
        return true;

    default:
        return false;
    }
}

bool ErrorHandler::isActiveErrorConditionCleared() const {
    if (!m_hasActiveError || m_context == nullptr) {
        return false;
    }
    return !isPhysicalErrorConditionStillPresent();
}

bool ErrorHandler::doesEventClearActiveError(logik::Event e) const {
    if (!m_hasActiveError) {
        return false;
    }

    switch (m_activeErrorType) {
    case ErrorType::UNEXPECTED_EVENT:
        switch (m_activeErrorTrigger) {
        case logik::Event::ENTRY_BLOCKED:
        case logik::Event::ENTRY_CLEARED:
            return e == logik::Event::ENTRY_CLEARED;
        case logik::Event::HEIGHT_BLOCKED:
        case logik::Event::HEIGHT_ERROR:
            return e == logik::Event::HEIGHT_CLEARED;
        case logik::Event::HEIGHT_CLEARED:
            return e == logik::Event::HEIGHT_CLEARED;
        case logik::Event::END_BLOCKED:
            return e == logik::Event::END_CLEARED;
        case logik::Event::END_CLEARED:
            return e == logik::Event::END_BLOCKED;
        case logik::Event::EJECTOR_BLOCKED:
            return e == logik::Event::EJECTOR_CLEARED;
        case logik::Event::EJECTOR_CLEARED:
            return e == logik::Event::EJECTOR_BLOCKED;
        case logik::Event::METAL_DETECTED:
            return e == logik::Event::METAL_CLEARED;
        default:
            return false;
        }

    case ErrorType::EARLY_ENTRY:
        return e == logik::Event::ENTRY_CLEARED;

    case ErrorType::HEIGHT_ERROR:
        return e == logik::Event::HEIGHT_CLEARED;

    case ErrorType::BOTH_CHUTES_FULL:
        return e == logik::Event::CHUTE_CLEARED;

    case ErrorType::STUCK:
        if (m_activeErrorLocation == WpLocation::AT_ENTRY) {
            return e == logik::Event::ENTRY_CLEARED;
        }
        if (m_activeErrorLocation == WpLocation::AT_HEIGHT_SENSOR) {
            return e == logik::Event::HEIGHT_CLEARED;
        }
        return false;

    default:
        return false;
    }
}

void ErrorHandler::tryEmitErrorCleared(const char* reason, logik::Event clearingEvent) {
    if (m_context == nullptr || !m_hasActiveError) {
        return;
    }

    auto* em = m_context->getEventManager();
    if (em == nullptr || !em->isErrorActive()) {
        return;
    }

    const bool eventMatches = (clearingEvent != logik::Event::UNKNOWN)
        ? doesEventClearActiveError(clearingEvent)
        : true;
    if (!eventMatches || isPhysicalErrorConditionStillPresent()) {
        return;
    }

    std::cout << "[ErrorHandler] Fehlerursache behoben (" << reason
              << ") -> ERROR_CLEARED. "
              << "Rot blinkt LANGSAM = Fehler weg, RESET quittieren."
              << std::endl;

    m_hasActiveError = false;
    em->handleEvent(logik::Event::ERROR_CLEARED);
}

void ErrorHandler::resetActiveErrorTracking() {
    m_hasActiveError = false;
    m_activeErrorType = ErrorType::WP_LOST;
    m_activeErrorTrigger = logik::Event::UNKNOWN;
    m_activeErrorLocation = WpLocation::UNKNOWN_LOCATION;
}

bool ErrorHandler::isErrorCauseCleared() const {
    return !isPhysicalErrorConditionStillPresent();
}

} // namespace logik
