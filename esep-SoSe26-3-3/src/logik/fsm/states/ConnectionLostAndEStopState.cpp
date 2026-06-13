#include "ConnectionLostAndEStopState.hpp"
#include "ActiveRegionState.hpp"
#include "ConnectionLostState.hpp"
#include "EStopState.hpp"
#include "logik/fsm/level0/Level0Actions.hpp"
#include "logik/eventManager/eventManager.hpp"
#include "logik/logik.hpp"
#include "dispatcher/i_Dispatcher.hpp"
#include <iostream>

namespace logik {

void ConnectionLostAndEStopState::entry() {
    std::cout << "[ConnectionLost+EStop] Entered (Ebene 0)." << std::endl;

    if (m_context == nullptr) {
        return;
    }

    level0::applyConnectionLostEntry(m_context->getDispatcher());
    level0::applyEstopSafetyClear(m_context);

    if (auto* dispatcher = m_context->getDispatcher()) {
        dispatcher->turnLEDQ2BlinkFast();
    }

    if (auto* em = m_context->getEventManager()) {
        em->setEStopActive(true);
        em->setMotorOn(false);
        em->setMotorSlow(false);
    }
}

void ConnectionLostAndEStopState::handle(Event e) {
    if (m_context == nullptr) {
        return;
    }

    auto* dispatcher = m_context->getDispatcher();
    auto* em = m_context->getEventManager();

    switch (e) {
    case Event::ESTOP_RELEASED:
    case Event::PARTNER_ESTOP_RELEASED:
        if (!level0::isAnyEstopActive(m_context) && em != nullptr && em->isConnectionLost()) {
            std::cout << "[ConnectionLost+EStop] ESTOP frei, Netz weg -> ConnectionLost" << std::endl;
            m_context->transitionTo(new ConnectionLostState());
        }
        break;

    case Event::CONNECTION_RESTORED:
        if (level0::isAnyEstopActive(m_context)) {
            std::cout << "[ConnectionLost+EStop] RESTORED, ESTOP aktiv -> EStopState" << std::endl;
            m_context->transitionTo(new EStopState());
        } else {
            std::cout << "[ConnectionLost+EStop] RESTORED, alles ok -> ActiveRegion (H*)" << std::endl;
            level0::clearTrafficLightsForRestore(dispatcher);
            if (em != nullptr) {
                em->setEStopActive(false);
            }
            m_context->transitionTo(
                new ActiveRegionState(ActiveRegionState::EntryMode::RestoreHistory));
        }
        break;

    case Event::ESTOP_PRESSED:
    case Event::PARTNER_ESTOP_PRESSED:
        if (dispatcher != nullptr) {
            dispatcher->lightRedBlinkFast();
        }
        break;

    default:
        break;
    }
}

} // namespace logik
