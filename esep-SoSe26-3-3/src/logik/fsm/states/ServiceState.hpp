#ifndef SERVICE_STATE_HPP
#define SERVICE_STATE_HPP

#include "BaseState.hpp"

namespace logik {

class ServiceState : public BaseState {
public:
    void onEnter() override;
    void onExit() override;
    void handle(Event e) override;
};

} // namespace logik

#endif // SERVICE_STATE_HPP
