#include "ConnectionLostState.hpp"
#include "ActiveRegionState.hpp"
#include "ConnectionLostAndEStopState.hpp"
#include "EStopState.hpp"
#include "logik/fsm/level0/Level0Actions.hpp"
#include "logik/logik.hpp"
#include "dispatcher/i_Dispatcher.hpp"
#include <iostream>

namespace logik {

void ConnectionLostState::entry() {
    std::cout << "[ConnectionLost] Entered (Ebene 0)." << std::endl;

    if (m_context != nullptr) {
        level0::applyConnectionLostEntry(m_context->getDispatcher());
    }
}

void ConnectionLostState::handle(Event e) {
    if (m_context == nullptr) {
        return;
    }

    auto* dispatcher = m_context->getDispatcher();

    switch (e) {
    case Event::CONNECTION_RESTORED:
        if (level0::isAnyEstopActive(m_context)) {
            std::cout << "[ConnectionLost] RESTORED mit ESTOP -> EStopState" << std::endl;
            m_context->transitionTo(new EStopState());
        } else {
            std::cout << "[ConnectionLost] RESTORED -> ActiveRegion (H*)" << std::endl;
            level0::clearTrafficLightsForRestore(dispatcher);
            m_context->transitionTo(
                new ActiveRegionState(ActiveRegionState::EntryMode::RestoreHistory));
        }
        break;

    case Event::ESTOP_PRESSED:
    case Event::PARTNER_ESTOP_PRESSED:
        std::cout << "[ConnectionLost] ESTOP -> ConnectionLostAndEStop" << std::endl;
        if (e == Event::ESTOP_PRESSED && dispatcher != nullptr) {
            dispatcher->turnLEDQ2BlinkFast();
        }
        m_context->transitionTo(new ConnectionLostAndEStopState());
        break;

    default:
        break;
    }
}

} // namespace logik
