#ifndef READY_FOR_ACTIVE_STATE_HPP
#define READY_FOR_ACTIVE_STATE_HPP

#include "logik/fsm/common/BaseState.hpp"

namespace logik {

/**
 * @brief Ebene 1: Betrieb nach Verbindungsverlust pausiert – beide START -> Running fortsetzen.
 */
class ReadyForActiveState : public BaseState {
public:
    void entry() override;
    void exit() override;
    void handle(Event e) override;
};

} // namespace logik

#endif // READY_FOR_ACTIVE_STATE_HPP
