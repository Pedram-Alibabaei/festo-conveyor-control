#ifndef ESTOP_SUB_STATE_HPP
#define ESTOP_SUB_STATE_HPP

#include "shared/logikEvent.hpp"
#include <memory>

namespace logik {

class Logik;
class EStopState;

/**
 * @brief Ebene-0-Unterzustände der Estop-Region (laut Estop-UML).
 */
class EStopSubStateBase {
public:
    virtual ~EStopSubStateBase() = default;

    virtual void entry(Logik* context, EStopState& parent) = 0;
    virtual void exit(Logik* context) {}
    virtual void handle(Event e, Logik* context, EStopState& parent) = 0;
};

std::unique_ptr<EStopSubStateBase> createInitialEStopSubState(Logik* logik);

class EstopLocalActiveSubState : public EStopSubStateBase {
public:
    void entry(Logik* context, EStopState& parent) override;
    void handle(Event e, Logik* context, EStopState& parent) override;
};

class EstopPartnerActiveSubState : public EStopSubStateBase {
public:
    void entry(Logik* context, EStopState& parent) override;
    void handle(Event e, Logik* context, EStopState& parent) override;
};

class EstopBothActiveSubState : public EStopSubStateBase {
public:
    void entry(Logik* context, EStopState& parent) override;
    void handle(Event e, Logik* context, EStopState& parent) override;
};

class EstopBothNotActiveSubState : public EStopSubStateBase {
public:
    void entry(Logik* context, EStopState& parent) override;
    void handle(Event e, Logik* context, EStopState& parent) override;
};

} // namespace logik

#endif // ESTOP_SUB_STATE_HPP
