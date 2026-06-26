#include "EStopSubState.hpp"
#include "logik/fsm/level0/EStopState.hpp"
#include "logik/fsm/level0/Level0Actions.hpp"
#include "logik/logik.hpp"
#include "dispatcher/i_Dispatcher.hpp"
#include "shared/detailLog.hpp"
#include <iostream>

namespace logik {

namespace {

void logTransition(const char* from, const char* to, Event e) {
    DETAIL_LOG( "[EStop/" << from << "] "
              << static_cast<int>(e) << " -> " << to);
}

} // namespace

std::unique_ptr<EStopSubStateBase> createInitialEStopSubState(Logik* logik) {
    if (logik == nullptr) {
        return std::make_unique<EstopBothNotActiveSubState>();
    }

    auto* dispatcher = logik->getDispatcher();
    const bool localHw = dispatcher != nullptr && dispatcher->isEstopPressed();
    const bool partnerHw = level0::isPartnerEstopActive(logik);

    if (localHw && partnerHw) {
        return std::make_unique<EstopBothActiveSubState>();
    }
    if (localHw) {
        return std::make_unique<EstopLocalActiveSubState>();
    }
    if (partnerHw) {
        return std::make_unique<EstopPartnerActiveSubState>();
    }
    return std::make_unique<EstopBothNotActiveSubState>();
}

void EstopLocalActiveSubState::entry(Logik* context, EStopState& parent) {
    (void)parent;
    DETAIL_LOG( "[EStop] Estop_Local_Active entry");
    level0::applyEstopLocalActiveEntry(context);
}

void EstopLocalActiveSubState::handle(Event e, Logik* context, EStopState& parent) {
    switch (e) {
    case Event::PARTNER_ESTOP_PRESSED:
        logTransition("LocalActive", "BothActive", e);
        parent.clearResetLatches();
        parent.replaceSubState(std::make_unique<EstopBothActiveSubState>());
        break;
    case Event::ESTOP_RELEASED:
        if (level0::isPartnerEstopActive(context)) {
            logTransition("LocalActive", "PartnerActive", e);
            parent.clearResetLatches();
            parent.replaceSubState(std::make_unique<EstopPartnerActiveSubState>());
        } else {
            logTransition("LocalActive", "BothNotActive", e);
            parent.clearResetLatches();
            parent.replaceSubState(std::make_unique<EstopBothNotActiveSubState>());
        }
        break;
    default:
        break;
    }
}

void EstopPartnerActiveSubState::entry(Logik* context, EStopState& parent) {
    (void)parent;
    DETAIL_LOG( "[EStop] Estop_Partner_Active entry");
    level0::applyEstopPartnerActiveEntry(context);
}

void EstopPartnerActiveSubState::handle(Event e, Logik* context, EStopState& parent) {
    switch (e) {
    case Event::ESTOP_PRESSED:
        logTransition("PartnerActive", "BothActive", e);
        parent.clearResetLatches();
        parent.replaceSubState(std::make_unique<EstopBothActiveSubState>());
        break;
    case Event::PARTNER_ESTOP_RELEASED:
    case Event::PARTNER_ESTOP_SAFETY_SYNC:
        if (context != nullptr && context->getDispatcher() != nullptr
            && context->getDispatcher()->isEstopPressed()) {
            logTransition("PartnerActive", "LocalActive", e);
            parent.clearResetLatches();
            parent.replaceSubState(std::make_unique<EstopLocalActiveSubState>());
        } else {
            logTransition("PartnerActive", "BothNotActive", e);
            parent.clearResetLatches();
            parent.replaceSubState(std::make_unique<EstopBothNotActiveSubState>());
        }
        break;
    default:
        break;
    }
}

void EstopBothActiveSubState::entry(Logik* context, EStopState& parent) {
    (void)parent;
    DETAIL_LOG( "[EStop] Estop_Both_Active entry");
    if (auto* dispatcher = context != nullptr ? context->getDispatcher() : nullptr) {
        level0::applyEstopPressedTraffic(dispatcher);
        level0::syncLocalEstopPanelLed(dispatcher);
    }
}

void EstopBothActiveSubState::handle(Event e, Logik* context, EStopState& parent) {
    switch (e) {
    case Event::PARTNER_ESTOP_RELEASED:
    case Event::PARTNER_ESTOP_SAFETY_SYNC:
        logTransition("BothActive", "LocalActive", e);
        parent.clearResetLatches();
        parent.replaceSubState(std::make_unique<EstopLocalActiveSubState>());
        break;
    case Event::ESTOP_RELEASED:
        logTransition("BothActive", "PartnerActive", e);
        parent.clearResetLatches();
        parent.replaceSubState(std::make_unique<EstopPartnerActiveSubState>());
        break;
    default:
        break;
    }
}

void EstopBothNotActiveSubState::entry(Logik* context, EStopState& parent) {
    DETAIL_LOG( "[EStop] Estop_Both_Not_Active entry");
    parent.clearResetLatches();
    if (auto* dispatcher = context != nullptr ? context->getDispatcher() : nullptr) {
        level0::applyEstopBothNotActiveEntry(dispatcher);
    }
}

void EstopBothNotActiveSubState::handle(Event e, Logik* context, EStopState& parent) {
    switch (e) {
    case Event::ESTOP_PRESSED:
        logTransition("BothNotActive", "LocalActive", e);
        parent.clearResetLatches();
        parent.replaceSubState(std::make_unique<EstopLocalActiveSubState>());
        break;
    case Event::PARTNER_ESTOP_PRESSED:
        logTransition("BothNotActive", "PartnerActive", e);
        parent.clearResetLatches();
        parent.replaceSubState(std::make_unique<EstopPartnerActiveSubState>());
        break;
    case Event::RESET_PRESSED:
        parent.latchLocalReset(context);
        parent.applyResetWaitingVisuals(context);
        parent.tryFinishReset();
        break;
    case Event::PARTNER_RESET_PRESSED:
        parent.latchPartnerReset(context);
        parent.applyResetWaitingVisuals(context);
        parent.tryFinishReset();
        break;
    default:
        break;
    }
}

} // namespace logik
