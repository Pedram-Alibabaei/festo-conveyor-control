#include "RunningState.hpp"
#include "EStopState.hpp"
#include "IdleState.hpp"
#include "logik/logik.hpp"
#include "logik/eventManager/eventManager.hpp"
#include "dispatcher/i_Dispatcher.hpp"
#include <iostream>

namespace logik {

void RunningState::onEnter() {
    std::cout << "[RunningState] Entered." << std::endl;

    if (!m_context || !m_context->getDispatcher()) {
        return;
    }

    m_context->getDispatcher()->setAdcWaitigForWpState();
    m_context->getDispatcher()->lightGreenOn();

    if (auto* em = m_context->getEventManager()) {
        em->setServiceMode(false);
    }
}

void RunningState::onExit() {
    if (!m_context) {
        return;
    }

    if (auto* dispatcher = m_context->getDispatcher()) {
        dispatcher->driveStop();
    }

    if (auto* em = m_context->getEventManager()) {
        em->setMotorOn(false);
        em->setMotorSlow(false);
    }
}

void RunningState::handle(Event e) {
    if (!m_context || !m_context->getDispatcher()) {
        return;
    }

    auto* dispatcher = m_context->getDispatcher();
    auto* em = m_context->getEventManager();

    switch (e) {
    case Event::STOP_PRESSED:
        std::cout << "[RunningState] STOP -> IdleState" << std::endl;
        dispatcher->lightRedBlinkSlow();
        m_context->transitionTo(new IdleState());
        break;

    case Event::ESTOP_PRESSED:
    case Event::PARTNER_ESTOP_PRESSED:
        std::cout << "[RunningState] ESTOP -> EStopState" << std::endl;
        if (e == Event::ESTOP_PRESSED) {
            dispatcher->turnLEDQ2BlinkFast();
        }
        m_context->transitionTo(new EStopState());
        break;

    case Event::MOTOR_RUN:
        std::cout << "[RunningState] MOTOR_RUN -> Band rechts" << std::endl;
        dispatcher->driveRight();
        dispatcher->lightGreenBlinkSlow();
        if (em) {
            em->setMotorOn(true);
        }
        break;

    case Event::MOTOR_STOP:
    case Event::END_BLOCKED:
    case Event::CHUTE_BLOCKED:
        std::cout << "[RunningState] Band stoppen" << std::endl;
        dispatcher->driveStop();
        dispatcher->lightGreenOff();
        if (em) {
            em->setMotorOn(false);
        }
        break;

    case Event::SET_SLOW:
        std::cout << "[RunningState] SET_SLOW -> Langsamfahrt" << std::endl;
        dispatcher->driveSlowRight();
        if (em) {
            em->setMotorSlow(true);
        }
        break;

    case Event::HEIGHT_CLEARED:
        std::cout << "[RunningState] HEIGHT_CLEARED -> Normalfahrt" << std::endl;
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
