#include "IdleState.hpp"
#include "RunningState.hpp"
#include "ServiceState.hpp"
#include "EStopState.hpp"
#include <iostream>

namespace logik {

void IdleState::onEnter() {
    std::cout << "[IdleState] Entered." << std::endl;
}

void IdleState::handle(Event e) {
    if (!m_context || !m_context->getDispatcher()) return;

    switch (e) {
        case Event::START_PRESSED:
            std::cout << "[IdleState] START_PRESSED -> Starte Timer..." << std::endl;
            m_startPressedTime = std::chrono::steady_clock::now();
            break;
        case Event::START_RELEASED: {
            auto end = std::chrono::steady_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::seconds>(end - m_startPressedTime).count();
            
            if (duration < 2) {
                std::cout << "[IdleState] Kurzer Druck (" << duration << "s) -> Wechsel zu RunningState" << std::endl;
                m_context->getDispatcher()->lightGreenOn();
                m_context->transitionTo(new RunningState());
            } else {
                std::cout << "[IdleState] Langer Druck (" << duration << "s) -> Wechsel zu ServiceState" << std::endl;
                m_context->getDispatcher()->lightGreenBlinkFast();
                m_context->transitionTo(new ServiceState());
            }
            break;
        }
        case Event::ESTOP_PRESSED:
        case Event::NET_ESTOP_PRESSED:
            std::cout << "[IdleState] ESTOP -> Wechsel zu EStopState" << std::endl;
            m_context->transitionTo(new EStopState());
            // Das Blink-Verhalten wird im onEnter() des EStopState oder hier geregelt:
            if (e == Event::ESTOP_PRESSED) {
                m_context->getDispatcher()->turnLEDQ2BlinkFast();
            }
            break;
        case Event::STOP_PRESSED:
            std::cout << "[IdleState] STOP_PRESSED -> Ruhezustand (Motor Stop)!" << std::endl;
            m_context->getDispatcher()->driveStop();
            m_context->getDispatcher()->lightRedBlinkSlow();
            break;
        case Event::STOP_RELEASED:
            std::cout << "[IdleState] STOP_RELEASED -> Rot aus!" << std::endl;
            m_context->getDispatcher()->lightRedOff();
            break;
        case Event::RESET_PRESSED:
            std::cout << "[IdleState] RESET_PRESSED -> Gelb blinkt!" << std::endl;
            m_context->getDispatcher()->lightYellowBlinkFast();
            break;
        case Event::RESET_RELEASED:
            std::cout << "[IdleState] RESET_RELEASED -> Gelb aus!" << std::endl;
            m_context->getDispatcher()->lightYellowOff();
            break;
        default:
            // Ignoriere alle anderen Events im Ruhezustand
            break;
    }
}

} // namespace logik
