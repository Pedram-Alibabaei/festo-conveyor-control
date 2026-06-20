#ifndef READY_FOR_RUN_STATE_HPP
#define READY_FOR_RUN_STATE_HPP

#include "logik/fsm/common/BaseState.hpp"

namespace logik {

/**
 * @brief Ebene 1: Beide Anlagen kalibriert + verbunden – kurzer START -> Running.
 */
class ReadyForRunState : public BaseState {
public:
    void entry() override;
    void exit() override;
    void handle(Event e) override;
};

} // namespace logik

#endif // READY_FOR_RUN_STATE_HPP
