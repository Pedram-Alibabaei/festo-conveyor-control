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
            std::cout << "-> Verloren in Zone: " << static_cast<int>(location) << std::endl;
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
            // Hier bleibt das Event wichtig!
            std::cout << "-> Sensor-ID: " << static_cast<int>(triggerEvent) << " schlug unzulaessig an." << std::endl;
            std::cout << "-> BEDIENER-ANWEISUNG: Bitte entfernen Sie das fremde Objekt vom Sensorbereich." << std::endl;
            std::cout << "====================================================\n" << std::endl;
            break;

        case ErrorType::HEIGHT_ERROR:
            std::cout << "\n====================================================" << std::endl;
            std::cout << "[ERROR] GEOMETRIE-FEHLER: HOEHENMESSUNG REISSGRENZE (HEIGHT_ERROR)!" << std::endl;
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
            std::cout << "-> Blockiert in Zone: " << static_cast<int>(location) << std::endl;
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
            m_isEntryTimerRunning = true;
            m_entryStartTime = std::chrono::steady_clock::now();

            break;

        case logik::Event::ENTRY_CLEARED:
            m_isEntryTimerRunning = false;
            break;

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
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - m_entryStartTime);

        if (duration >= TIMEOUT_THRESHOLD) {
            m_isEntryTimerRunning = false; // Watchdog zurücksetzen

            // Versuchen, die ID des blockierenden Werkstücks am Einlauf zu ermitteln
            int id = -1;
            if (m_context->getWorkpieceManager() != nullptr) {
                auto* wp = m_context->getWorkpieceManager()->getFirstWorkpieceAt(WpLocation::AT_ENTRY);
                if (wp != nullptr) id = wp->getId();
            }

            handleEscalation(ErrorType::STUCK, logik::Event::ENTRY_BLOCKED, WpLocation::AT_ENTRY, id);
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
