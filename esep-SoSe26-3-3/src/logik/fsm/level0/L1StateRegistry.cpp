#include "L1StateRegistry.hpp"
#include "logik/fsm/states/IdleState.hpp"
#include "logik/fsm/states/ReadyForServiceState.hpp"
#include "logik/fsm/states/ReadyForRunState.hpp"
#include "logik/fsm/states/RunningState.hpp"
#include "logik/fsm/states/ServiceState.hpp"

namespace logik {

namespace {
L1StateId s_history = L1StateId::IDLE;
} // namespace

L1StateId L1StateRegistry::history() {
    return s_history;
}

void L1StateRegistry::setHistory(L1StateId id) {
    s_history = id;
}

void L1StateRegistry::rememberFrom(const BaseState* state) {
    s_history = idFrom(state);
}

std::unique_ptr<BaseState> L1StateRegistry::create(L1StateId id) {
    switch (id) {
    case L1StateId::READY_FOR_SERVICE:
        return std::unique_ptr<BaseState>(new ReadyForServiceState());
    case L1StateId::READY_FOR_RUN:
        return std::unique_ptr<BaseState>(new ReadyForRunState());
    case L1StateId::RUNNING:
        return std::unique_ptr<BaseState>(new RunningState());
    case L1StateId::SERVICE:
        return std::unique_ptr<BaseState>(new ServiceState());
    case L1StateId::IDLE:
    default:
        return std::unique_ptr<BaseState>(new IdleState());
    }
}

std::unique_ptr<BaseState> L1StateRegistry::createFromHistory() {
    return create(s_history);
}

L1StateId L1StateRegistry::idFrom(const BaseState* state) {
    if (dynamic_cast<const RunningState*>(state) != nullptr) {
        return L1StateId::RUNNING;
    }
    if (dynamic_cast<const ReadyForRunState*>(state) != nullptr) {
        return L1StateId::READY_FOR_RUN;
    }
    if (dynamic_cast<const ReadyForServiceState*>(state) != nullptr) {
        return L1StateId::READY_FOR_SERVICE;
    }
    if (dynamic_cast<const ServiceState*>(state) != nullptr) {
        return L1StateId::SERVICE;
    }
    return L1StateId::IDLE;
}

} // namespace logik
