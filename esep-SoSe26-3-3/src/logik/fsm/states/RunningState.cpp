#include "RunningState.hpp"
#include "IdleState.hpp"
#include "logik/logik.hpp"
#include "logik/fsm/level1/Level1Actions.hpp"
#include <iostream>

namespace logik {

void RunningState::entry() {
    std::cout << "[RunningState] Entered (Ebene 1)." << std::endl;

    if (m_context == nullptr) {
        return;
    }

    level1::applyRunningEntry(m_context->getDispatcher(), m_context->getEventManager());
}

void RunningState::exit() {
    if (m_context == nullptr) {
        return;
    }

    level1::applyRunningExit(m_context->getDispatcher(), m_context->getEventManager());
}

void RunningState::handle(Event e) {
    if (!m_context || !m_context->getDispatcher()) {
        return;
    }

    auto* dispatcher = m_context->getDispatcher();
    auto* eventManager = m_context->getEventManager();

    switch (e) {
    case Event::STOP_RELEASED:
        std::cout << "[RunningState] STOP -> IdleState" << std::endl;
        m_context->transitionTo(new IdleState());
        break;

    case Event::MOTOR_RUN:
        std::cout << "[RunningState] MOTOR_RUN -> Band rechts" << std::endl;
        level1::applyMotorRun(dispatcher, eventManager, true);
        break;

    case Event::MOTOR_STOP:
    case Event::END_BLOCKED:
    case Event::CHUTE_BLOCKED:
        std::cout << "[RunningState] Band stoppen" << std::endl;
        level1::applyMotorStop(dispatcher, eventManager, true);
        break;

    case Event::SET_SLOW:
        std::cout << "[RunningState] SET_SLOW -> Langsamfahrt" << std::endl;
        level1::applyMotorSlow(dispatcher, eventManager);
        break;

    case Event::HEIGHT_CLEARED:
        std::cout << "[RunningState] HEIGHT_CLEARED -> Normalfahrt" << std::endl;
        level1::applyHeightCleared(dispatcher, eventManager);
        break;

    default:
        break;
    }
}

} // namespace logik
