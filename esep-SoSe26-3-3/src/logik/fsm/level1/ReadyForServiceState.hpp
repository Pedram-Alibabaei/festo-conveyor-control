#ifndef READY_FOR_SERVICE_STATE_HPP
#define READY_FOR_SERVICE_STATE_HPP

#include "logik/fsm/common/BaseState.hpp"

namespace logik {

/**
 * @brief Ebene 1: Keine Kalibrierungsdatei – START-LED an, langer START -> Service.
 */
class ReadyForServiceState : public BaseState {
public:
    void entry() override;
    void exit() override;
    void handle(Event e) override;
};

} // namespace logik

#endif // READY_FOR_SERVICE_STATE_HPP
