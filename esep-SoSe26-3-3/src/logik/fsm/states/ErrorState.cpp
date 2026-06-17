#include "ErrorState.hpp"
#include "ActiveRegionState.hpp"
#include "ConnectionLostState.hpp"
#include "EStopState.hpp"
#include "logik/logik.hpp"
#include "logik/eventManager/eventManager.hpp"
#include "logik/workpieceManager/WorkpieceManager.hpp"
#include "dispatcher/i_Dispatcher.hpp"
#include <iostream>

namespace logik {

void ErrorState::entry() {
    std::cout << "[ErrorState] Entered (Ebene 0)." << std::endl;
    enterPendingUnacknowledged();
}

void ErrorState::exit() {
    if (m_context != nullptr && m_context->getDispatcher() != nullptr) {
        auto* dispatcher = m_context->getDispatcher();
        dispatcher->lightRedOff();
        dispatcher->lightGreenOff();
        dispatcher->turnLEDResetOff();
        dispatcher->turnLEDStartOff();
    }
}

void ErrorState::enterPendingUnacknowledged() {
    m_subState = SubState::PendingUnacknowledged;

    if (m_context != nullptr && m_context->getEventManager() != nullptr) {
        auto* eventManager = m_context->getEventManager();
        m_resumeMotorOn = eventManager->isMotorOn();
        m_resumeMotorSlow = eventManager->isMotorSlow();
    }

    if (m_context != nullptr && m_context->getDispatcher() != nullptr) {
        auto* dispatcher = m_context->getDispatcher();
        dispatcher->driveStop();
        dispatcher->lightGreenOff();
        dispatcher->lightRedBlinkFast();
        dispatcher->turnLEDResetBlinkFast();
        dispatcher->turnLEDStartOff();
    }

    if (m_context != nullptr && m_context->getEventManager() != nullptr) {
        m_context->getEventManager()->setErrorActive(true);
        m_context->getEventManager()->setMotorOn(false);
        m_context->getEventManager()->setMotorSlow(false);
    }

    std::cout << "[ErrorState] Anstehend unquittiert" << std::endl;
}

void ErrorState::enterPendingAcknowledged() {
    m_subState = SubState::PendingAcknowledged;

    if (m_context != nullptr && m_context->getDispatcher() != nullptr) {
        auto* dispatcher = m_context->getDispatcher();
        dispatcher->lightRedOn();
        dispatcher->turnLEDResetOn();
        dispatcher->turnLEDStartBlinkSlow();
    }

    std::cout << "[ErrorState] Anstehend quittiert - START druecken fuer Rueckkehr (H*)"
              << std::endl;
}

void ErrorState::enterGoneUnacknowledged() {
    m_subState = SubState::GoneUnacknowledged;

    if (m_context != nullptr && m_context->getDispatcher() != nullptr) {
        auto* dispatcher = m_context->getDispatcher();
        dispatcher->lightRedBlinkSlow();
        dispatcher->turnLEDResetBlinkFast();
        dispatcher->turnLEDStartOff();
    }

    std::cout << "[ErrorState] Gegangen unquittiert" << std::endl;
}

void ErrorState::leaveErrorState() {
    if (m_context == nullptr) {
        return;
    }

    bool chuteWarningActive = false;
    bool resumeMotor = false;
    bool resumeSlow = false;
    if (auto* eventManager = m_context->getEventManager()) {
        eventManager->setErrorActive(false);
        chuteWarningActive = eventManager->isChuteFull();
        resumeMotor = m_resumeMotorOn;
        resumeSlow = m_resumeMotorSlow;
        if (auto* wpManager = m_context->getWorkpieceManager()) {
            resumeMotor = resumeMotor && wpManager->hasActiveWorkpieces();
        } else {
            resumeMotor = false;
        }
    }

    std::cout << "[ErrorState] Fehler abgeschlossen -> ActiveRegion (H*)" << std::endl;
    m_context->transitionTo(
        new ActiveRegionState(ActiveRegionState::EntryMode::RestoreHistory));

    if (auto* eventManager = m_context->getEventManager()) {
        if (chuteWarningActive) {
            std::cout << "[ErrorState] Rutsche nach Fehler weiterhin voll -> WARNING" << std::endl;
            eventManager->handleEvent(Event::WARNING);
        } else {
            std::cout << "[ErrorState] Rutsche nach Fehler frei -> WARNING_CLEARED" << std::endl;
            eventManager->handleEvent(Event::WARNING_CLEARED);
        }

        if (resumeMotor) {
            std::cout << "[ErrorState] Aktive WPs vorhanden -> Motor "
                      << (resumeSlow ? "Slow" : "Fast") << " fortsetzen" << std::endl;
            eventManager->handleEvent(Event::MOTOR_RUN);
            if (resumeSlow) {
                eventManager->handleEvent(Event::SET_SLOW);
            }
        }
    }
}

void ErrorState::handle(Event e) {
    if (m_context != nullptr) {
        switch (e) {
        case Event::CONNECTION_LOST:
            std::cout << "[ErrorState] CONNECTION_LOST -> ConnectionLostState" << std::endl;
            m_context->transitionTo(new ConnectionLostState());
            return;

        case Event::ESTOP_PRESSED:
        case Event::PARTNER_ESTOP_PRESSED:
        case Event::PARTNER_ESTOP_SAFETY_SYNC:
            std::cout << "[ErrorState] ESTOP -> EStopState" << std::endl;
            m_context->transitionTo(new EStopState());
            return;

        default:
            break;
        }
    }

    switch (m_subState) {
    case SubState::PendingUnacknowledged:
        if (e == Event::RESET_RELEASED || e == Event::PARTNER_RESET_RELEASED) {
            enterPendingAcknowledged();
        } else if (e == Event::ERROR_CLEARED) {
            enterGoneUnacknowledged();
        } else if (e == Event::ERROR) {
            enterPendingUnacknowledged();
        }
        break;

    case SubState::PendingAcknowledged:
        if (e == Event::START_SHORT_PRESSED || e == Event::START_LONG_PRESSED) {
            leaveErrorState();
        } else if (e == Event::ERROR) {
            enterPendingUnacknowledged();
        }
        break;

    case SubState::GoneUnacknowledged:
        if (e == Event::RESET_RELEASED || e == Event::PARTNER_RESET_RELEASED) {
            enterPendingAcknowledged();
        } else if (e == Event::ERROR) {
            enterPendingUnacknowledged();
        }
        break;
    }
}

} // namespace logik
