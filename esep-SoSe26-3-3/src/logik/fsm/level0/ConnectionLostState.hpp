#ifndef CONNECTION_LOST_STATE_HPP
#define CONNECTION_LOST_STATE_HPP

#include "logik/fsm/common/BaseState.hpp"

namespace logik {

class ConnectionLostState : public BaseState {
public:
    void entry() override;
    void handle(Event e) override;
};

} // namespace logik

#endif
