/**
 * @file eventManager.cpp
 * @brief Implementierung des EventManagers mit Koppelung der Logik-Komponenten.
 *
 * SoSe2026 Team 3.3
 */

#include "eventManager.hpp"
#include "logik/logik.hpp"
#include "logik/workpieceManager/WorkpieceManager.hpp"
#include "logik/errorHandler/errorHandler.hpp"
#include "logik/fsm/Fsm.hpp"
#include "logik/fsm/states/BaseState.hpp"

#include <iostream>

namespace logik {

    EventManager::EventManager(Logik* context)
        : m_context(context),
          m_wpManager(nullptr),      // Initial per init() gekoppelt
          m_errorHandler(nullptr),   // Initial per init() gekoppelt
          m_fsm(nullptr),
          m_isMotorOn(false),
          m_isMotorSlow(false),
          m_anlagenPosition(AnlagenPosition::UNKNOWN_POSITION),
          m_isEStopActive(false),
          m_isErrorActive(false),
          m_isEntryFrei(true),
          m_isChuteFull(false),
          m_isPartnerChuteFull(false),
          START_LONG_THRESHOLD(2000),
          m_isEndBlocked(false),
          m_isServiceMode(false)
    {
    }

EventManager::~EventManager() = default;

void EventManager::init(AnlagenPosition pos) {
    m_anlagenPosition = pos;

    // ========================================================================
    // ANPASSUNG: GESCHWISTER-ZEIGER REINREDUZIEREN (MEDIATOR PATTERN)
    // ========================================================================
    if (m_context != nullptr) {
        m_wpManager    = m_context->getWorkpieceManager();
        m_errorHandler = m_context->getErrorHandler();
    }

    m_fsm = std::make_unique<Fsm>(m_context, this);

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
            std::cout << "[Event-Manager] START gedrueckt (lokal)" << std::endl;
            break;

        case logik::Event::START_RELEASED: {
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - m_startPressedStartTime);

            if (duration >= START_LONG_THRESHOLD) {
                std::cout << "[Event-Manager] START lang (" << duration.count() << "ms)" << std::endl;
                routeToFsm(logik::Event::START_LONG_PRESSED);
            } else {
                std::cout << "[Event-Manager] START kurz (" << duration.count() << "ms)" << std::endl;
                routeToFsm(logik::Event::START_SHORT_PRESSED);
            }
            break;
        }

        case logik::Event::PARTNER_START_PRESSED:
            m_partnerStartPressedTime = std::chrono::steady_clock::now();
            std::cout << "[Event-Manager] START gedrueckt (Partner)" << std::endl;
            break;

        case logik::Event::PARTNER_START_RELEASED: {
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - m_partnerStartPressedTime);

            if (duration >= START_LONG_THRESHOLD) {
                std::cout << "[Event-Manager] Partner-START lang (" << duration.count() << "ms)" << std::endl;
                routeToFsm(logik::Event::START_LONG_PRESSED);
            } else {
                std::cout << "[Event-Manager] Partner-START kurz (" << duration.count() << "ms)" << std::endl;
                routeToFsm(logik::Event::START_SHORT_PRESSED);
            }
            break;
        }

        case logik::Event::RESET_RELEASED:
        case logik::Event::PARTNER_RESET_RELEASED:
            if (m_isErrorActive) {
                std::cout << "[Event-Manager] Fehler quittiert (RESET)." << std::endl;
                m_isErrorActive = false;
            }
            break;

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
            if (m_isEntryFrei && (m_anlagenPosition == AnlagenPosition::VORN_ANLAGE_1 || m_isServiceMode)) {
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
        case logik::Event::CHUTE_BLOCKED:
        case logik::Event::CHUTE_CLEARED:
            if (!m_isMotorOn && !m_wpManager->hasActiveWorkpieces() && !m_isServiceMode) {
                break;
            }
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
    if (m_fsm != nullptr) {
        m_fsm->processEvent(e);
    }
}

void EventManager::transitionFsmState(std::unique_ptr<BaseState> nextState) {
    if (m_fsm != nullptr) {
        m_fsm->transitionTo(std::move(nextState));
    }
}

void EventManager::setPartnerChuteStatus(bool isFull) {
    m_isPartnerChuteFull = isFull;
    if (m_isPartnerChuteFull && m_isChuteFull) {
        setErrorActive(true);
        handleEvent(logik::Event::ERROR);
    }
}

} // namespace logik
