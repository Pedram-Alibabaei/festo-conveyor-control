#ifndef RUNNING_STATE_HPP
#define RUNNING_STATE_HPP

#include "logik/fsm/common/BaseState.hpp"

namespace logik {

class RunningState : public BaseState {
public:
    void entry() override;
    void exit() override;
    void handle(Event e) override;

private:
    enum class WarningSubState {
        Normal,
        Warning
    };

    enum class MotorSubState {
        Idle,
        Fast,
        Slow
    };

    enum class EntrySubState {
        Free,
        NotFree
    };

    WarningSubState m_warningState = WarningSubState::Normal;
    MotorSubState m_motorState = MotorSubState::Idle;
    EntrySubState m_entryState = EntrySubState::Free;

    void enterRunningNormal();
    void enterRunningWarning();
    void enterEntryFree();
    void enterEntryNotFree();
    void enterMotorIdle();
    void enterMotorFast();
    void enterMotorSlow();
};

} // namespace logik

#endif // RUNNING_STATE_HPP
