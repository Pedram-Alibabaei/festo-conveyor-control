/**
 * @file Fsm.cpp
 * @brief Implementierung der Schicht-1-Zustandsmaschine.
 *
 * SoSe2026 Team 3.3
 */

#include "logik/fsm/Fsm.hpp"
#include "logik/logik.hpp"
#include "logik/eventManager/eventManager.hpp"
#include "logik/fsm/states/IdleState.hpp"
#include <iostream>

namespace logik {

Fsm::Fsm(Logik* logik, EventManager* eventManager)
    : m_logik(logik),
      m_eventManager(eventManager)
{
    auto initial = std::make_unique<IdleState>();
    initial->setContext(m_logik);
    initial->onEnter();
    m_currentState = std::move(initial);
    std::cout << "[FSM] Initialisiert im IdleState." << std::endl;
}

void Fsm::processEvent(Event e) {
    if (m_currentState) {
        m_currentState->handle(e);
    }
}

void Fsm::transitionTo(std::unique_ptr<BaseState> nextState) {
    if (!nextState) {
        return;
    }

    if (m_currentState) {
        m_currentState->onExit();
    }

    nextState->setContext(m_logik);
    nextState->onEnter();
    m_currentState = std::move(nextState);
}

} // namespace logik
