/**
 * @file Fsm.hpp
 * @brief Hierarchische FSM: Ebene 0 (Active/Estop/Connection) + Ebene 1 (Idle/Running/Service).
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

    bool isLevel1State(const BaseState* state) const;
    void transitionLevel0(std::unique_ptr<BaseState> nextState);

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
