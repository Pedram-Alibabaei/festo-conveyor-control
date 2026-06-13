#ifndef ACTIVE_REGION_STATE_HPP
#define ACTIVE_REGION_STATE_HPP

#include "BaseState.hpp"
#include <memory>

namespace logik {

/**
 * @brief Ebene 0: Active — Composite mit Ebene-1 (Idle / Running / Service) + Deep History.
 */
class ActiveRegionState : public BaseState {
public:
    enum class EntryMode {
        WithIdle,
        RestoreHistory
    };

    ActiveRegionState();
    explicit ActiveRegionState(EntryMode mode);
    explicit ActiveRegionState(std::unique_ptr<BaseState> level1State);

    void entry() override;
    void exit() override;
    void handle(Event e) override;

    void replaceLevel1(std::unique_ptr<BaseState> nextLevel1);

private:
    std::unique_ptr<BaseState> m_level1;
    EntryMode m_entryMode;
};

} // namespace logik

#endif
