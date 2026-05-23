#include "RunningState.hpp"
#include "IdleState.hpp"
#include "EStopState.hpp"
#include <iostream>

namespace logik {

void RunningState::handle(Event e) {
    if (!m_context || !m_context->getDispatcher()) return;

    switch (e) {
        case Event::START_RELEASED:
            std::cout << "[RunningState] START_RELEASED -> Gruen aus!" << std::endl;
            m_context->getDispatcher()->lightGreenOff();
            break;
        case Event::STOP_PRESSED:
            std::cout << "[RunningState] STOP_PRESSED -> Wechsel zu IdleState" << std::endl;
            m_context->getDispatcher()->driveStop();
            m_context->getDispatcher()->lightRedBlinkSlow();
            m_context->transitionTo(new IdleState());
            break;
        case Event::ESTOP_PRESSED:
        case Event::NET_ESTOP_PRESSED:
            std::cout << "[RunningState] ESTOP -> Wechsel zu EStopState" << std::endl;
            m_context->transitionTo(new EStopState());
            if (e == Event::ESTOP_PRESSED) {
                m_context->getDispatcher()->turnLEDQ2BlinkFast();
            }
            break;
        case Event::ENTRY_BLOCKED:
            std::cout << "[RunningState] ENTRY_BLOCKED -> Motor faehrt RECHTS!" << std::endl;
            m_context->getDispatcher()->driveRight();
            m_context->getDispatcher()->lightGreenBlinkSlow();
            break;
        case Event::END_BLOCKED:
            std::cout << "[RunningState] END_BLOCKED -> Motor STOPP!" << std::endl;
            m_context->getDispatcher()->driveStop();
            m_context->getDispatcher()->lightGreenOff();
            break;
        case Event::CHUTE_BLOCKED:
            std::cout << "[RunningState] CHUTE_BLOCKED -> Motor STOPP!" << std::endl;
            m_context->getDispatcher()->driveStop();
            m_context->getDispatcher()->lightGreenOff();
            break;
        default:
            break;
    }
}

} // namespace logik
