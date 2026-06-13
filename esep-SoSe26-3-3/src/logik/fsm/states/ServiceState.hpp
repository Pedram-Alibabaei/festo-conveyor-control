#ifndef SERVICE_STATE_HPP
#define SERVICE_STATE_HPP

#include "BaseState.hpp"
#include "service/ServiceCalibSubState.hpp"
#include <memory>

namespace logik {

/**
 * @brief Ebene 1: Service — Composite mit Kalibrierungs-Unterzustaenden.
 */
class ServiceState : public BaseState {
public:
    void entry() override;
    void exit() override;
    void handle(Event e) override;

    void replaceSubState(std::unique_ptr<ServiceCalibSubStateBase> next);

private:
    std::unique_ptr<ServiceCalibSubStateBase> m_sub;
};

} // namespace logik

#endif // SERVICE_STATE_HPP
