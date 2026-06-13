#ifndef RUNNING_STATE_HPP
#define RUNNING_STATE_HPP

#include "BaseState.hpp"

namespace logik {

class RunningState : public BaseState {
public:
    void entry() override;
    void exit() override;
    void handle(Event e) override;
};

} // namespace logik

#endif // RUNNING_STATE_HPP
