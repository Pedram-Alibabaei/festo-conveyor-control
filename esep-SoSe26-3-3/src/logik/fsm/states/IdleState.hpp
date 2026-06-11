#ifndef IDLE_STATE_HPP
#define IDLE_STATE_HPP

#include "BaseState.hpp"

namespace logik {

class IdleState : public BaseState {
public:
    void onEnter() override;
    void handle(Event e) override;
};

} // namespace logik

#endif // IDLE_STATE_HPP
