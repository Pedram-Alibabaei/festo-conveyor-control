#include "ConnectionLostState.hpp"
#include "ActiveRegionState.hpp"
#include "ConnectionLostAndEStopState.hpp"
#include "EStopState.hpp"
#include "logik/fsm/level0/Level0Actions.hpp"
#include "logik/logik.hpp"
#include "dispatcher/i_Dispatcher.hpp"
#include "shared/detailLog.hpp"
#include "shared/operatorLog.hpp"
#include <iostream>

namespace logik {

void ConnectionLostState::entry() {
    DETAIL_LOG( "[ConnectionLost] Entered (Ebene 0).");

    if (m_context != nullptr) {
        if (auto* em = m_context->getEventManager()) {
            operatorLog::logConnectionLost(em->context().position);
        }
        level0::applyConnectionLostEntry(m_context->getDispatcher(),
                                         m_context->getEventManager());
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
            DETAIL_LOG( "[ConnectionLost] RESTORED mit ESTOP -> EStopState");
            if (auto* em = m_context->getEventManager()) {
                operatorLog::logConnectionRestored(em->context().position);
            }
            m_context->transitionTo(new EStopState());
        } else {
            level0::transitionAfterConnectionRestore(m_context);
        }
        break;

    case Event::ESTOP_PRESSED:
    case Event::PARTNER_ESTOP_PRESSED:
    case Event::PARTNER_ESTOP_SAFETY_SYNC:
        DETAIL_LOG( "[ConnectionLost] ESTOP -> ConnectionLostAndEStop");
        m_context->transitionTo(new ConnectionLostAndEStopState());
        break;

    default:
        break;
    }
}

} // namespace logik
