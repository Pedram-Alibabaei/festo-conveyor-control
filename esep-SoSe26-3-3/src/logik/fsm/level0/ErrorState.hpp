#ifndef ERROR_STATE_HPP
#define ERROR_STATE_HPP

#include "logik/fsm/common/BaseState.hpp"

namespace logik {

/**
 * @brief Ebene 0: Error-Composite laut UML (anstehend/gegangen x quittiert).
 */
class ErrorState : public BaseState {
public:
    enum class SubState {
        PendingUnacknowledged,
        PendingAcknowledged,
        GoneUnacknowledged
    };

    ErrorState() = default;
    explicit ErrorState(SubState restoredSubState);

    void entry() override;
    void exit() override;
    void handle(Event e) override;

    SubState getSubState() const { return m_subState; }

private:
    SubState m_subState = SubState::PendingUnacknowledged;
    bool m_restoreEntry = false;
    /** true nach ERROR_CLEARED – Voraussetzung fuer START (H*). */
    bool m_errorCauseCleared = false;

    void enterPendingUnacknowledged();
    void enterPendingAcknowledged();
    void enterGoneUnacknowledged();
    void leaveErrorState();
    void onErrorCauseCleared();
    bool canLeaveErrorWithStart() const;
};

} // namespace logik

#endif // ERROR_STATE_HPP
