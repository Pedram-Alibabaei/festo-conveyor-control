#include "ServiceState.hpp"
#include "IdleState.hpp"
#include "EStopState.hpp"
#include <iostream>

namespace logik {

void ServiceState::onEnter() {
    std::cout << "[ServiceState] Entered. Kalibrierungen koennen starten!" << std::endl;
}

void ServiceState::handle(Event e) {
    if (!m_context || !m_context->getDispatcher()) return;

    switch (e) {
        case Event::ESTOP_PRESSED:
        case Event::NET_ESTOP_PRESSED:
            std::cout << "[ServiceState] ESTOP -> Wechsel zu EStopState" << std::endl;
            m_context->transitionTo(new EStopState());
            if (e == Event::ESTOP_PRESSED) {
                m_context->getDispatcher()->turnLEDQ2BlinkFast();
            }
            break;
        case Event::STOP_PRESSED:
            std::cout << "[ServiceState] STOP_PRESSED -> Zurueck in Ruhezustand" << std::endl;
            m_context->getDispatcher()->lightGreenOff();
            m_context->getDispatcher()->lightRedBlinkSlow();
            m_context->transitionTo(new IdleState());
            break;
        default:
            break;
    }
}

} // namespace logik
