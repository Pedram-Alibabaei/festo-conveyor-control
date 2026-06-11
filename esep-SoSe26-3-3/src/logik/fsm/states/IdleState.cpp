#include "IdleState.hpp"
#include "EStopState.hpp"
#include "RunningState.hpp"
#include "ServiceState.hpp"
#include "logik/logik.hpp"
#include "dispatcher/i_Dispatcher.hpp"
#include <iostream>

namespace logik {

void IdleState::onEnter() {
    std::cout << "[IdleState] Entered." << std::endl;

    if (!m_context || !m_context->getDispatcher()) {
        return;
    }

    auto* dispatcher = m_context->getDispatcher();
    dispatcher->setAdcIdelState();
    dispatcher->driveStop();

    if (auto* em = m_context->getEventManager()) {
        em->setServiceMode(false);
        em->setMotorOn(false);
        em->setMotorSlow(false);
    }
}

void IdleState::handle(Event e) {
    if (!m_context || !m_context->getDispatcher()) {
        return;
    }

    auto* dispatcher = m_context->getDispatcher();

    switch (e) {
    case Event::START_SHORT_PRESSED:
        std::cout << "[IdleState] Kurzer Start -> RunningState" << std::endl;
        dispatcher->lightGreenOn();
        m_context->transitionTo(new RunningState());
        break;

    case Event::START_LONG_PRESSED:
        std::cout << "[IdleState] Langer Start -> ServiceState" << std::endl;
        dispatcher->lightGreenBlinkFast();
        m_context->transitionTo(new ServiceState());
        break;

    case Event::ESTOP_PRESSED:
    case Event::PARTNER_ESTOP_PRESSED:
        std::cout << "[IdleState] ESTOP -> EStopState" << std::endl;
        if (e == Event::ESTOP_PRESSED) {
            dispatcher->turnLEDQ2BlinkFast();
        }
        m_context->transitionTo(new EStopState());
        break;

    case Event::STOP_PRESSED:
        dispatcher->driveStop();
        dispatcher->lightRedBlinkSlow();
        break;

    case Event::STOP_RELEASED:
        dispatcher->lightRedOff();
        break;

    case Event::RESET_PRESSED:
    case Event::PARTNER_RESET_PRESSED:
        dispatcher->lightYellowBlinkFast();
        break;

    case Event::RESET_RELEASED:
    case Event::PARTNER_RESET_RELEASED:
        dispatcher->lightYellowOff();
        break;

    default:
        break;
    }
}

} // namespace logik
