/**
 * @file Fsm.hpp
 * @brief GoF-Zustandsmaschine fuer Schicht-1 (Idle, Running, Service, EStop).
 *
 * SoSe2026 Team 3.3
 */

#ifndef FSM_HPP
#define FSM_HPP

#include <memory>
#include "logik/fsm/states/BaseState.hpp"

namespace logik {

class Logik;
class EventManager;

class Fsm {
private:
    Logik* m_logik;
    EventManager* m_eventManager;
    std::unique_ptr<BaseState> m_currentState;

public:
    Fsm(Logik* logik, EventManager* eventManager);
    ~Fsm() = default;

    Fsm(const Fsm&) = delete;
    Fsm& operator=(const Fsm&) = delete;

    void processEvent(Event e);
    void transitionTo(std::unique_ptr<BaseState> nextState);
};

} // namespace logik

#endif // FSM_HPP
