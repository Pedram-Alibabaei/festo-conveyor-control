#ifndef SERVICE_CALIB_SUB_STATE_HPP
#define SERVICE_CALIB_SUB_STATE_HPP

#include "shared/logikEvent.hpp"
#include <memory>

namespace logik {

class Logik;
class ServiceState;

/**
 * @brief Ebene-1-Unterzustaende der Service-Kalibrierung (Slow + Normal laut UML).
 */
class ServiceCalibSubStateBase {
public:
    virtual ~ServiceCalibSubStateBase() = default;

    virtual void entry(Logik* context, ServiceState& parent) = 0;
    virtual void exit(Logik* context) {}
    virtual void handle(Event e, Logik* context, ServiceState& parent) = 0;
};

std::unique_ptr<ServiceCalibSubStateBase> createInitialServiceCalibSubState();

// --- Slow-Pfad ---
class ServiceInitEmptySubState : public ServiceCalibSubStateBase {
public:
    void entry(Logik* context, ServiceState& parent) override;
    void handle(Event e, Logik* context, ServiceState& parent) override;
};

class ServiceMotorRightSlowSubState : public ServiceCalibSubStateBase {
public:
    void entry(Logik* context, ServiceState& parent) override;
    void handle(Event e, Logik* context, ServiceState& parent) override;
};

class ServiceSlowResetTickSubState : public ServiceCalibSubStateBase {
public:
    void entry(Logik* context, ServiceState& parent) override;
    void handle(Event e, Logik* context, ServiceState& parent) override;
};

class ServiceWaitHeightClearedSubState : public ServiceCalibSubStateBase {
public:
    void entry(Logik* context, ServiceState& parent) override;
    void handle(Event e, Logik* context, ServiceState& parent) override;
};

class ServiceWaitEjectorBlockedSubState : public ServiceCalibSubStateBase {
public:
    void entry(Logik* context, ServiceState& parent) override;
    void handle(Event e, Logik* context, ServiceState& parent) override;
};

class ServiceWaitEjectorClearedSubState : public ServiceCalibSubStateBase {
public:
    void entry(Logik* context, ServiceState& parent) override;
    void handle(Event e, Logik* context, ServiceState& parent) override;
};

class ServiceWaitChuteBlockedSubState : public ServiceCalibSubStateBase {
public:
    void entry(Logik* context, ServiceState& parent) override;
    void handle(Event e, Logik* context, ServiceState& parent) override;
};

class ServiceSlowCalibDoneSubState : public ServiceCalibSubStateBase {
public:
    void entry(Logik* context, ServiceState& parent) override;
    void handle(Event e, Logik* context, ServiceState& parent) override;
};

// --- Normal-Pfad (2. Lauf) ---
class ServiceNormalStartSubState : public ServiceCalibSubStateBase {
public:
    void entry(Logik* context, ServiceState& parent) override;
    void handle(Event e, Logik* context, ServiceState& parent) override;
};

class ServiceNormalWaitHeightClearedSubState : public ServiceCalibSubStateBase {
public:
    void entry(Logik* context, ServiceState& parent) override;
    void handle(Event e, Logik* context, ServiceState& parent) override;
};

class ServiceNormalWaitEjectorBlockedSubState : public ServiceCalibSubStateBase {
public:
    void entry(Logik* context, ServiceState& parent) override;
    void handle(Event e, Logik* context, ServiceState& parent) override;
};

class ServiceNormalWaitEjectorClearedSubState : public ServiceCalibSubStateBase {
public:
    void entry(Logik* context, ServiceState& parent) override;
    void handle(Event e, Logik* context, ServiceState& parent) override;
};

class ServiceNormalWaitEndBlockedSubState : public ServiceCalibSubStateBase {
public:
    void entry(Logik* context, ServiceState& parent) override;
    void handle(Event e, Logik* context, ServiceState& parent) override;
};

class ServiceCalibFinishedSubState : public ServiceCalibSubStateBase {
public:
    void entry(Logik* context, ServiceState& parent) override;
    void handle(Event e, Logik* context, ServiceState& parent) override;
};

} // namespace logik

#endif // SERVICE_CALIB_SUB_STATE_HPP
