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
            m_context->addWorkpiece(); // REQ_01 und REQ_02: Registrierung und ID Vergabe
            m_context->getDispatcher()->driveRight();
            m_context->getDispatcher()->lightGreenBlinkSlow();
            break;

        default:
            // Check if it's a sensor event that needs to be consumed by a WP
            if (e == Event::ENTRY_CLEARED || e == Event::HEIGHT_BLOCKED || e == Event::HEIGHT_CLEARED || 
                e == Event::EJECTOR_BLOCKED || e == Event::EJECTOR_CLEARED || e == Event::END_BLOCKED || 
                e == Event::END_CLEARED || e == Event::CHUTE_BLOCKED) {
                
                if (!m_context->dispatchSensorEventToWorkpiece(e)) {
                    std::cout << "[RunningState] FEHLER: Unplausibles Sensor-Event! Keines der Werkstücke hat es erwartet." << std::endl;
                    // TODO: m_context->transitionTo(new ErrorState());
                } else {
                    // Falls das Event erwartet wurde, spezifische Reaktionen des Systems:
                    if (e == Event::END_BLOCKED || e == Event::CHUTE_BLOCKED) {
                        std::cout << "[RunningState] WP am Ende oder in Rutsche -> Motor STOPP!" << std::endl;
                        m_context->getDispatcher()->driveStop();
                        m_context->getDispatcher()->lightGreenOff();
                    }
                }
            }
            break;
    }
}

} // namespace logik
