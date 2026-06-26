#include "ActiveRegionState.hpp"
#include "ConnectionLostState.hpp"
#include "EStopState.hpp"
#include "ErrorState.hpp"
#include "logik/fsm/level1/IdleState.hpp"
#include "logik/fsm/level1/L1StateRegistry.hpp"
#include "logik/logik.hpp"
#include "dispatcher/i_Dispatcher.hpp"
#include "shared/detailLog.hpp"
#include <iostream>

namespace logik {

ActiveRegionState::ActiveRegionState()
    : m_entryMode(EntryMode::WithIdle)
{
    m_level1 = std::unique_ptr<BaseState>(new IdleState());
    L1StateRegistry::setHistory(L1StateId::IDLE);
}

ActiveRegionState::ActiveRegionState(EntryMode mode)
    : m_entryMode(mode)
{
}

ActiveRegionState::ActiveRegionState(std::unique_ptr<BaseState> level1State)
    : m_level1(std::move(level1State)),
      m_entryMode(EntryMode::WithIdle)
{
    if (m_level1 != nullptr) {
        L1StateRegistry::rememberFrom(m_level1.get());
    }
}

void ActiveRegionState::entry() {
    DETAIL_LOG( "[ActiveRegion] Entered (Ebene 0).");

    if (m_entryMode == EntryMode::RestoreHistory || m_level1 == nullptr) {
        m_level1 = L1StateRegistry::createFromHistory();
        DETAIL_LOG( "[ActiveRegion] History H* wiederhergestellt (L1="
                  << static_cast<int>(L1StateRegistry::history()) << ").");
    }

    if (m_level1 != nullptr) {
        m_level1->setContext(m_context);
        m_level1->entry();
    }
}

void ActiveRegionState::exit() {
    if (m_level1 != nullptr) {
        L1StateRegistry::rememberFrom(m_level1.get());
        m_level1->exit();
        m_level1.reset();
    }
}

void ActiveRegionState::replaceLevel1(std::unique_ptr<BaseState> nextLevel1) {
    if (m_level1 != nullptr) {
        m_level1->exit();
    }

    m_level1 = std::move(nextLevel1);
    if (m_level1 != nullptr) {
        L1StateRegistry::rememberFrom(m_level1.get());
        m_level1->setContext(m_context);
        m_level1->entry();
    }
}

void ActiveRegionState::handle(Event e) {
    switch (e) {
    case Event::CONNECTION_LOST:
        DETAIL_LOG( "[ActiveRegion] CONNECTION_LOST -> ConnectionLostState");
        m_context->transitionTo(new ConnectionLostState());
        return;

    case Event::ESTOP_PRESSED:
    case Event::PARTNER_ESTOP_PRESSED:
    case Event::PARTNER_ESTOP_SAFETY_SYNC:
        DETAIL_LOG( "[ActiveRegion] ESTOP -> EStopState");
        m_context->transitionTo(new EStopState());
        return;

    case Event::ERROR:
        if (m_context != nullptr) {
            DETAIL_LOG( "[ActiveRegion] ERROR -> ErrorState");
            m_context->transitionTo(new ErrorState());
        }
        return;

    case Event::ERROR_CLEARED:
        if (m_context != nullptr && m_context->getDispatcher() != nullptr) {
            m_context->getDispatcher()->lightRedOff();
        }
        if (m_level1 != nullptr) {
            m_level1->handle(e);
        }
        return;

    case Event::WARNING:
        if (m_level1 != nullptr) {
            m_level1->handle(e);
        }
        return;

    case Event::WARNING_CLEARED:
        if (m_level1 != nullptr) {
            m_level1->handle(e);
        }
        return;

    default:
        break;
    }

    if (m_level1 != nullptr) {
        m_level1->handle(e);
    }
}

} // namespace logik
