#include "ServiceState.hpp"
#include "EStopState.hpp"
#include "IdleState.hpp"
#include "logik/logik.hpp"
#include "logik/eventManager/eventManager.hpp"
#include "dispatcher/i_Dispatcher.hpp"
#include <iostream>

namespace logik {

void ServiceState::onEnter() {
    std::cout << "[ServiceState] Entered. Kalibrierungsmodus aktiv." << std::endl;

    if (!m_context || !m_context->getDispatcher()) {
        return;
    }

    if (auto* em = m_context->getEventManager()) {
        em->setServiceMode(true);
        em->setMotorOn(false);
        em->setMotorSlow(false);
    }

    m_context->getDispatcher()->setAdcCalibrationsState();
    m_context->getDispatcher()->lightGreenBlinkFast();
    m_context->getDispatcher()->driveStop();
}

void ServiceState::onExit() {
    if (!m_context || !m_context->getDispatcher()) {
        return;
    }

    if (auto* em = m_context->getEventManager()) {
        em->setServiceMode(false);
        em->setMotorOn(false);
        em->setMotorSlow(false);
    }

    m_context->getDispatcher()->driveStop();
    m_context->getDispatcher()->lightGreenOff();
}

void ServiceState::handle(Event e) {
    if (!m_context || !m_context->getDispatcher()) {
        return;
    }

    auto* dispatcher = m_context->getDispatcher();
    auto* em = m_context->getEventManager();

    switch (e) {
    case Event::ESTOP_PRESSED:
    case Event::PARTNER_ESTOP_PRESSED:
        std::cout << "[ServiceState] ESTOP -> EStopState" << std::endl;
        if (e == Event::ESTOP_PRESSED) {
            dispatcher->turnLEDQ2BlinkFast();
        }
        m_context->transitionTo(new EStopState());
        break;

    case Event::STOP_PRESSED:
        std::cout << "[ServiceState] STOP -> IdleState" << std::endl;
        dispatcher->lightRedBlinkSlow();
        m_context->transitionTo(new IdleState());
        break;

    case Event::MOTOR_RUN:
        std::cout << "[ServiceState] MOTOR_RUN -> Band fuer Kalibrierung" << std::endl;
        dispatcher->driveRight();
        if (em) {
            em->setMotorOn(true);
        }
        break;

    case Event::MOTOR_STOP:
    case Event::END_BLOCKED:
    case Event::CHUTE_BLOCKED:
        std::cout << "[ServiceState] Band stoppen" << std::endl;
        dispatcher->driveStop();
        if (em) {
            em->setMotorOn(false);
        }
        break;

    case Event::SET_SLOW:
        std::cout << "[ServiceState] SET_SLOW -> Langsamfahrt" << std::endl;
        dispatcher->driveSlowRight();
        if (em) {
            em->setMotorSlow(true);
        }
        break;

    case Event::HEIGHT_CLEARED:
        std::cout << "[ServiceState] HEIGHT_CLEARED -> Normalfahrt" << std::endl;
        dispatcher->driveRight();
        if (em) {
            em->setMotorSlow(false);
        }
        break;

    default:
        break;
    }
}

} // namespace logik
