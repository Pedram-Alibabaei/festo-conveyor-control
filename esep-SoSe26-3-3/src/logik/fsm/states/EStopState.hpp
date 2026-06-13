#ifndef ESTOP_STATE_HPP
#define ESTOP_STATE_HPP

#include "BaseState.hpp"

namespace logik {

class EStopState : public BaseState {
private:
    bool m_isHardwareUnlocked = false;

public:
    void entry() override;
    void handle(Event e) override;
};

} // namespace logik

#endif // ESTOP_STATE_HPP
