#ifndef L1_STATE_REGISTRY_HPP
#define L1_STATE_REGISTRY_HPP

#include "logik/fsm/common/BaseState.hpp"
#include <memory>

namespace logik {

enum class L1StateId {
    IDLE,
    READY_FOR_SERVICE,
    READY_FOR_RUN,
    READY_FOR_ACTIVE,
    RUNNING,
    SERVICE
};

class L1StateRegistry {
public:
    static L1StateId history();
    static void setHistory(L1StateId id);
    static void rememberFrom(const BaseState* state);

    static std::unique_ptr<BaseState> createFromHistory();
    static std::unique_ptr<BaseState> create(L1StateId id);
    static L1StateId idFrom(const BaseState* state);
};

} // namespace logik

#endif
