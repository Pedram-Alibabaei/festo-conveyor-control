#ifndef ERROR_STATE_HPP
#define ERROR_STATE_HPP

#include "BaseState.hpp"

namespace logik {

/**
 * @brief Ebene 0: Error-Composite laut UML (anstehend/gegangen x quittiert).
 */
class ErrorState : public BaseState {
public:
    void entry() override;
    void exit() override;
    void handle(Event e) override;

private:
    enum class SubState {
        PendingUnacknowledged,
        PendingAcknowledged,
        GoneUnacknowledged
    };

    SubState m_subState = SubState::PendingUnacknowledged;
    bool m_resumeMotorOn = false;
    bool m_resumeMotorSlow = false;

    void enterPendingUnacknowledged();
    void enterPendingAcknowledged();
    void enterGoneUnacknowledged();
    void leaveErrorState();
};

} // namespace logik

#endif // ERROR_STATE_HPP
