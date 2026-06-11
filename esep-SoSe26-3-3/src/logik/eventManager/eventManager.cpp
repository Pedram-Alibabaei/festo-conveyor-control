/**
 * @file eventManager.cpp
 * @brief Implementierung des EventManagers mit Koppelung der Logik-Komponenten.
 *
 * SoSe2026 Team 3.3
 */

#include "eventManager.hpp"
#include "logik/logik.hpp"
#include "logik/workpieceManager/WorkpieceManager.hpp"
#include "logik/errorHandler/errorHandler.hpp" // Header inkludieren für m_errorHandler->processEvent

#include <iostream>

namespace logik {

    EventManager::EventManager(Logik* context)
        : m_context(context),
          m_wpManager(nullptr),      // Initial per init() gekoppelt
          m_errorHandler(nullptr),   // Initial per init() gekoppelt
         // m_fsm(nullptr),
          m_isMotorOn(false),
          m_isMotorSlow(false),
          m_anlagenPosition(AnlagenPosition::UNKNOWN_POSITION),
          m_isEStopActive(false),
          m_isErrorActive(false),
          m_isEntryFrei(true),
          m_isChuteFull(false),
          m_isPartnerChuteFull(false),
          START_LONG_THRESHOLD(2000),
          m_isEndBlocked(false)
    {
    }

void EventManager::init(AnlagenPosition pos) {
    m_anlagenPosition = pos;

    // ========================================================================
    // ANPASSUNG: GESCHWISTER-ZEIGER REINREDUZIEREN (MEDIATOR PATTERN)
    // ========================================================================
    if (m_context != nullptr) {
        m_wpManager    = m_context->getWorkpieceManager();
        m_errorHandler = m_context->getErrorHandler();
    }

    std::cout << "[Event-Manager] Auf Position " << static_cast<int>(pos)
              << " konfiguriert und mit Geschwister-Modulen verknuepft." << std::endl;
}

void EventManager::handleEvent(logik::Event e) {

    // ========================================================================
    // ANPASSUNG: ERST ROUTEN, DANN INTERNE FLAGS UPDATEN
    // Wichtig, damit der ErrorHandler m_isEntryFrei pruefen kann, BEVOR es kippt!
    // ========================================================================
    if (!m_isEStopActive && !m_isErrorActive) {
        routeToErrorHandler(e);
        routeToWorkpieceManager(e);
    }

    switch (e) {
        case logik::Event::ESTOP_PRESSED:
        case logik::Event::PARTNER_ESTOP_PRESSED:
            m_isEStopActive = true;
            break;

        case logik::Event::ESTOP_RELEASED:
        case logik::Event::PARTNER_ESTOP_RELEASED:
            m_isEStopActive = false;
            break;

        case logik::Event::END_BLOCKED:
            m_isEndBlocked = true;
            routeToFsm(logik::Event::MOTOR_STOP);
            break;

        case logik::Event::END_CLEARED:
            m_isEndBlocked = false;
            break;

        case logik::Event::ENTRY_BLOCKED:
            if (m_isEntryFrei && !m_isEndBlocked) {
                routeToFsm(logik::Event::MOTOR_RUN);
                routeToFsm(logik::Event::ENTRY_NOT_FREI);
                m_isEntryFrei = false;
            }
            break;

        case logik::Event::HEIGHT_BLOCKED:
            routeToFsm(logik::Event::ENTRY_FREI);
            m_isEntryFrei = true;
            break;

        case logik::Event::START_PRESSED:
            m_startPressedStartTime = std::chrono::steady_clock::now();
            break;

        case logik::Event::START_RELEASED: {
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - m_startPressedStartTime);

            if (duration >= START_LONG_THRESHOLD) {
                routeToFsm(logik::Event::START_LONG_PRESSED);
            } else {
                routeToFsm(logik::Event::START_SHORT_PRESSED);
            }
            break;
        }

        default:
            break;
    }

    // Event an die eigentliche Schicht-1-FSM weiterleiten
    routeToFsm(e);
}

// ============================================================================
// COMPONENT WATCHDOG TICK (DEINE RECHTLICHE STEUERUNG)
// ============================================================================
void EventManager::handleTakt() {
    if (m_wpManager == nullptr) return;

    if (m_isEStopActive || m_isErrorActive || m_isEndBlocked) { return; }

    if (m_isMotorOn) {
        m_wpManager->takt(m_isMotorSlow);
    }
}

void EventManager::routeToWorkpieceManager(logik::Event e) {
    if (m_wpManager == nullptr) return;

    if (m_isEStopActive || m_isErrorActive || m_isEndBlocked) { return; }

    switch (e) {
        case logik::Event::ENTRY_BLOCKED:
            // Nur auf Anlage 1 (VORN) wird hier lokal ein neues Werkstück erzeugt
            if (m_isEntryFrei && m_anlagenPosition == AnlagenPosition::VORN_ANLAGE_1) {
                m_wpManager->addWorkpiece();
            }
            break;

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
        case logik::Event::CHUTE_BLOCKED: // Hinzugefügt für Schieberegister-Rutscherkennung
        case logik::Event::CHUTE_CLEARED: // Hinzugefügt für Schieberegister-Rutscherkennung
            m_wpManager->handleSensorSignal(e);
            break;
        default:
            break;
    }
}

void EventManager::routeToErrorHandler(logik::Event e) {
    if (m_errorHandler == nullptr) return;

    // ========================================================================
    // ANPASSUNG: AKTIVIERUNG DER SENSOREN-ZEITÜBERWACHUNG
    // ========================================================================
    switch (e) {
        case logik::Event::ENTRY_BLOCKED:
        case logik::Event::ENTRY_CLEARED:
        case logik::Event::CHUTE_BLOCKED:
        case logik::Event::CHUTE_CLEARED:
            m_errorHandler->processEvent(e);
            break;
        default:
            break;
    }
}

void EventManager::routeToFsm(logik::Event e) {
    // Falls m_fsm genutzt wird, hier einkoppeln. Ansonsten leitet Logik.cpp direkt weiter.
    /* if (m_fsm != nullptr) {
         m_fsm->processEvent(e);
    }
    */
}

void EventManager::setPartnerChuteStatus(bool isFull) {
    m_isPartnerChuteFull = isFull;
    if (m_isPartnerChuteFull && m_isChuteFull) {
        setErrorActive(true);
        handleEvent(logik::Event::ERROR);
    }
}

} // namespace logik
