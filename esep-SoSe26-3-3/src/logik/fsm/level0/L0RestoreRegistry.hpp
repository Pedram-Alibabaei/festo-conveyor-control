#ifndef L0_RESTORE_REGISTRY_HPP
#define L0_RESTORE_REGISTRY_HPP

namespace logik {

class BaseState;

namespace level0 {

enum class L0RestoreTarget {
    ACTIVE_REGION,
    ERROR_PENDING_UNACK,
    ERROR_PENDING_ACK,
    ERROR_GONE_UNACK
};

class L0RestoreRegistry {
public:
    static void captureFrom(const BaseState* stateBeforeConnectionLost);
    static L0RestoreTarget target();
    static void clear();

private:
    static L0RestoreTarget s_target;
};

} // namespace level0
} // namespace logik

#endif // L0_RESTORE_REGISTRY_HPP
