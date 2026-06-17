/**
 * @file errorHandler.cpp
 * @brief Implementierung der Watchdog-Überwachung und der zentralen Fehler-Eskalation.
 *
 * SoSe2026 Team 3.3
 */

#include "errorHandler.hpp"
#include "logik/logik.hpp"
#include "logik/eventManager/eventManager.hpp"
#include "logik/workpieceManager/WorkpieceManager.hpp"
#include <iostream>

namespace {

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
void ErrorHandler::handleEscalation(ErrorType type, logik::Event triggerEvent, WpLocation location, int wpId) {
    if (m_context == nullptr) return;

    // 1. Schieberegister einfrieren
    if (m_context->getEventManager() != nullptr) {
        m_context->getEventManager()->setErrorActive(true);
    }

    WorkpieceManager* wpManager = m_context->getWorkpieceManager();

    if (wpManager != nullptr) {
        if (wpManager->isHandoverReceiveActive()
            && (type == ErrorType::WP_LOST || type == ErrorType::STUCK
                || type == ErrorType::UNEXPECTED_EVENT)) {
            wpManager->notifyPartnerHandoverFailed();
        }
        wpManager->resetHandoverState();
    }

    // 2. Diagnostische Ausgaben mit präzisen Typen
    switch (type) {
        case ErrorType::BOTH_CHUTES_FULL:
            std::cout << "\n====================================================" << std::endl;
            std::cout << "[ERROR] CRITICAL: BEIDE RUTSCHEN SIND VOLL!" << std::endl;
            std::cout << "-> BEDIENER-ANWEISUNG: Bitte entleeren Sie BEIDE Rutschen manuell." << std::endl;
            std::cout << "====================================================\n" << std::endl;
            break;

        case ErrorType::WP_LOST:
            std::cout << "\n====================================================" << std::endl;
            std::cout << "[ERROR] TRANSIT-FEHLER: WERKSTUECK VERLOREN (WP_LOST)!" << std::endl;
            // NEU: Zone statt Event ausgeben!
            std::cout << "-> Verloren in Zone: " << wpLocationName(location) << std::endl;
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
            std::cout << "-> BEDIENER-ANWEISUNG: Bitte entfernen Sie das fremde Objekt vom Sensorbereich." << std::endl;
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
            // NEU: Zone statt Event ausgeben!
            std::cout << "-> Blockiert in Zone: " << wpLocationName(location) << std::endl;
            if (wpId != -1) {
                std::cout << "-> Massnahme: Loesche blockiertes WP-ID " << wpId << " aus der Liste." << std::endl;
                if (wpManager != nullptr) wpManager->removeWorkpiece(wpId);
            }
            std::cout << "-> BEDIENER-ANWEISUNG: Der blockierte Bereich MUSS manuell frei gemacht werden!" << std::endl;
            std::cout << "====================================================\n" << std::endl;
            break;
    }

    // 3. Band stoppen
    if (m_context->getEventManager() != nullptr) {
        m_context->getEventManager()->handleEvent(logik::Event::ERROR);
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
        case logik::Event::CHUTE_BLOCKED:
            m_isChuteTimerRunning = true;
            m_chuteStartTime = std::chrono::steady_clock::now();
            break;

        case logik::Event::CHUTE_CLEARED:
            m_isChuteTimerRunning = false;

            // Wenn vor dem Freiräumen eine Warnung aktiv war, loeschen wir sie jetzt sauber
            if (m_isChuteWarningActive) {
                if (m_context->getEventManager() != nullptr) {
                    m_context->getEventManager()->setChuteFull(false);
                    m_context->getEventManager()->handleEvent(logik::Event::WARNING_CLEARED);
                }
                m_isChuteWarningActive = false;
            }
            break;

        default:
            break;
    }
}

// ============================================================================
// ZYKLISCHE ZEIT-ÜBERWACHUNG (AUFRUF ALLE 10MS)
// ============================================================================
void ErrorHandler::takt() {
    if (m_context->getEventManager() != nullptr && m_context->getEventManager()->isServiceMode()) {
        return;
    }

    // --- WATCHDOG: EINLAUF-STAU (STUCK) ---
    if (m_isEntryTimerRunning) {
        EventManager* eventManager = m_context->getEventManager();
        const bool entryWatchdogAllowed = eventManager != nullptr
            && eventManager->isMotorOn()
            && !eventManager->isEndBlocked()
            && !eventManager->isErrorActive();

        if (!entryWatchdogAllowed) {
            // Band steht (Bandende belegt o.ae.) – Stau-Timer pausieren, kein STUCK
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

    // --- WATCHDOG: RUTSCHE VOLL ODER BEIDE RUTSCHEN VOLL ---
    if (m_isChuteTimerRunning && !m_isChuteWarningActive) {
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - m_chuteStartTime);

        if (duration >= TIMEOUT_THRESHOLD) {
            m_isChuteWarningActive = true;

            if (m_context->getEventManager() != nullptr) {
                m_context->getEventManager()->setChuteFull(true);

                // Wenn unsere Rutsche voll ist UND das Netzwerk meldet, dass die Partner-Rutsche auch voll ist:
                if (m_context->getEventManager()->isPartnerChuteFull()) {
                    handleEscalation(ErrorType::BOTH_CHUTES_FULL, logik::Event::CHUTE_BLOCKED);
                } else {
                    // Ansonsten nur unkritische Warnung (Anlage weicht aus, Ampel wird gelb)
                    m_context->getEventManager()->handleEvent(logik::Event::WARNING);
                }
            }
        }
    }
}

} // namespace logik
