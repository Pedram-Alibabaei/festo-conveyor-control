#include "logik/fsm/Fsm.hpp"
#include "logik/logik.hpp"
#include "logik/eventManager/eventManager.hpp"
#include "logik/fsm/level0/ActiveRegionState.hpp"
#include "logik/fsm/level1/IdleState.hpp"
#include "logik/fsm/level1/ReadyForServiceState.hpp"
#include "logik/fsm/level1/ReadyForRunState.hpp"
#include "logik/fsm/level1/RunningState.hpp"
#include "logik/fsm/level1/ServiceState.hpp"
#include "logik/fsm/level1/ReadyForActiveState.hpp"
#include "logik/fsm/level0/ConnectionLostState.hpp"
#include "logik/fsm/level0/ConnectionLostAndEStopState.hpp"
#include "logik/fsm/level0/L0RestoreRegistry.hpp"
#include <iostream>

namespace logik {

Fsm::Fsm(Logik* logik, EventManager* eventManager)
    : m_logik(logik),
      m_eventManager(eventManager)
{
    auto initial = std::make_unique<ActiveRegionState>();
    initial->setContext(m_logik);
    initial->entry();
    m_currentState = std::move(initial);
    std::cout << "[FSM] Initialisiert: Ebene 0 ActiveRegion + Ebene 1 Idle." << std::endl;
}

void Fsm::processEvent(Event e) {
    if (m_currentState) {
        m_currentState->handle(e);
    }
}

namespace {

bool isConnectionLostTransition(const BaseState* nextState) {
    return dynamic_cast<const ConnectionLostState*>(nextState) != nullptr
        || dynamic_cast<const ConnectionLostAndEStopState*>(nextState) != nullptr;
}

} // namespace

bool Fsm::isLevel1State(const BaseState* state) const {
    return dynamic_cast<const IdleState*>(state) != nullptr
        || dynamic_cast<const ReadyForServiceState*>(state) != nullptr
        || dynamic_cast<const ReadyForRunState*>(state) != nullptr
        || dynamic_cast<const ReadyForActiveState*>(state) != nullptr
        || dynamic_cast<const RunningState*>(state) != nullptr
        || dynamic_cast<const ServiceState*>(state) != nullptr;
}

void Fsm::transitionLevel0(std::unique_ptr<BaseState> nextState) {
    if (!nextState) {
        return;
    }

    if (isConnectionLostTransition(nextState.get()) && m_currentState) {
        level0::L0RestoreRegistry::captureFrom(m_currentState.get());
    }

    if (m_currentState) {
        m_currentState->exit();
    }

    nextState->setContext(m_logik);
    nextState->entry();
    m_currentState = std::move(nextState);
}

void Fsm::transitionTo(std::unique_ptr<BaseState> nextState) {
    if (!nextState) {
        return;
    }

    if (isLevel1State(nextState.get())) {
        if (auto* active = dynamic_cast<ActiveRegionState*>(m_currentState.get())) {
            active->replaceLevel1(std::move(nextState));
            return;
        }

        auto wrapper = std::make_unique<ActiveRegionState>(std::move(nextState));
        transitionLevel0(std::move(wrapper));
        return;
    }

    transitionLevel0(std::move(nextState));
}

} // namespace logik
