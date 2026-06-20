#ifndef ESTOP_STATE_HPP
#define ESTOP_STATE_HPP

#include "logik/fsm/common/BaseState.hpp"
#include "estop/EStopSubState.hpp"
#include <memory>

namespace logik {

class Logik;

/**
 * @brief Ebene 0: Estop — Composite mit Unterzuständen laut Estop-UML.
 */
class EStopState : public BaseState {
public:
    void entry() override;
    void exit() override;
    void handle(Event e) override;

    void replaceSubState(std::unique_ptr<EStopSubStateBase> next);
    void completeResetAndLeave();

    void clearResetLatches();
    void latchLocalReset(Logik* context);
    void latchPartnerReset(Logik* context);
    void tryFinishReset();
    void applyResetWaitingVisuals(Logik* context);

    bool isLocalResetLatched() const { return m_localResetLatched; }
    bool isPartnerResetLatched() const { return m_partnerResetLatched; }

private:
    std::unique_ptr<EStopSubStateBase> m_sub;
    bool m_localResetLatched = false;
    bool m_partnerResetLatched = false;
};

} // namespace logik

#endif // ESTOP_STATE_HPP
