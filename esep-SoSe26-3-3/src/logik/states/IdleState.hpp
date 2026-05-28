#ifndef IDLE_STATE_HPP
#define IDLE_STATE_HPP

#include "BaseState.hpp"
#include "logik/logik.hpp"
#include <chrono>

namespace logik {

class IdleState : public BaseState {
private:
    std::chrono::steady_clock::time_point m_startPressedTime;
public:
    void onEnter() override;
    void handle(Event e) override;
};

} // namespace logik


#endif // IDLE_STATE_HPP
