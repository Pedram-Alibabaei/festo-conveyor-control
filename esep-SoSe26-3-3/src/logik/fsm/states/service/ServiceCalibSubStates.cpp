#include "ServiceCalibSubState.hpp"
#include "logik/fsm/states/ServiceState.hpp"
#include "logik/logik.hpp"
#include "dispatcher/i_Dispatcher.hpp"
#include <iostream>

namespace logik {

namespace {

void logTransition(const char* from, const char* to, Event e) {
    std::cout << "[Service/" << from << "] "
              << static_cast<int>(e) << " -> " << to << std::endl;
}

void setServiceMotorOn(Logik* context, bool on) {
    if (context != nullptr && context->getEventManager() != nullptr) {
        context->getEventManager()->setMotorOn(on);
    }
}

void startSlowMotor(Logik* context) {
    if (context == nullptr) {
        return;
    }
    if (auto* dispatcher = context->getDispatcher()) {
        dispatcher->driveSlowRight();
        setServiceMotorOn(context, true);
    }
}

void startNormalMotor(Logik* context) {
    if (context == nullptr) {
        return;
    }
    if (auto* dispatcher = context->getDispatcher()) {
        dispatcher->driveRight();
        setServiceMotorOn(context, true);
    }
}

void stopServiceMotor(Logik* context) {
    if (context == nullptr) {
        return;
    }
    if (auto* dispatcher = context->getDispatcher()) {
        dispatcher->driveStop();
    }
    setServiceMotorOn(context, false);
}

void storeMeasuredTick(Logik* context, std::uint32_t& targetField, const char* label) {
    if (context == nullptr) {
        return;
    }

    targetField = context->readAndResetServiceTickCounter();
    context->resetServiceTickCounter();
    context->syncCalibrationToWorkpieceManager();

    std::cout << "[Service] Gemessen " << label << " = " << targetField << " Takte" << std::endl;
}

/** 1. Lauf (Slow): Weiche geschlossen halten, Auswerfer noch nicht ausfahren. */
void prepareSlowPathSorter(Logik* context) {
    if (context == nullptr) {
        return;
    }

    auto* dispatcher = context->getDispatcher();
    if (dispatcher == nullptr) {
        return;
    }

    if (dispatcher->isSwitch()) {
        dispatcher->closeSwitch();
        std::cout << "[Service] Weiche geschlossen (1. Lauf -> Rutsche)." << std::endl;
    } else {
        std::cout << "[Service] Auswerfer bereit (Push bei EJECTOR_BLOCKED)." << std::endl;
    }
}

/** 1. Lauf (Slow): Auswerfer einmal ausfahren, wenn Werkstueck die LS erreicht. */
void actuateSlowPathOnEjectorBlocked(Logik* context) {
    if (context == nullptr) {
        return;
    }

    auto* dispatcher = context->getDispatcher();
    if (dispatcher == nullptr || dispatcher->isSwitch()) {
        return;
    }

    dispatcher->pushEjector();
    std::cout << "[Service] Auswerfer push bei EJECTOR_BLOCKED (1. Lauf)." << std::endl;
}

/** 2. Lauf (Normal): Weiche oeffnen, damit Werkstueck zum Bandende faehrt. */
void prepareNormalPathSorter(Logik* context) {
    if (context == nullptr) {
        return;
    }

    auto* dispatcher = context->getDispatcher();
    if (dispatcher == nullptr) {
        return;
    }

    if (dispatcher->isSwitch()) {
        dispatcher->openSwitch();
        std::cout << "[Service] Weiche offen (2. Lauf -> Bandende)." << std::endl;
    } else {
        std::cout << "[Service] Auswerfer eingefahren (2. Lauf -> Bandende)." << std::endl;
    }
}

/** 2. Lauf (Normal): Weiche am Bandende wieder schliessen. */
void finishNormalPathSorter(Logik* context) {
    if (context == nullptr) {
        return;
    }

    auto* dispatcher = context->getDispatcher();
    if (dispatcher == nullptr) {
        return;
    }

    if (dispatcher->isSwitch()) {
        dispatcher->closeSwitch();
        std::cout << "[Service] Weiche geschlossen (2. Lauf Bandende erreicht)." << std::endl;
    }
}

} // namespace

std::unique_ptr<ServiceCalibSubStateBase> createInitialServiceCalibSubState() {
    return std::make_unique<ServiceInitEmptySubState>();
}

// ============================================================================
// Slow-Pfad
// ============================================================================

void ServiceInitEmptySubState::entry(Logik* context, ServiceState& parent) {
    (void)parent;
    std::cout << "[Service] Init_Empty entry" << std::endl;

    if (context == nullptr) {
        return;
    }

    context->resetServiceTickCounter();

    if (auto* dispatcher = context->getDispatcher()) {
        dispatcher->setAdcCalibrationsState();
        dispatcher->lightGreenBlinkSlow();
        dispatcher->driveStop();

        std::cout << "[Service] Sortier-Hardware: "
                  << (dispatcher->isSwitch() ? "Weiche" : "Auswerfer") << std::endl;
    }
}

void ServiceInitEmptySubState::handle(Event e, Logik* context, ServiceState& parent) {
    if (e == Event::ENTRY_BLOCKED) {
        logTransition("Init_Empty", "Motor_Right_Slow", e);
        parent.replaceSubState(std::make_unique<ServiceMotorRightSlowSubState>());
    }
}

void ServiceMotorRightSlowSubState::entry(Logik* context, ServiceState& parent) {
    (void)parent;
    std::cout << "[Service] Motor_Right_Slow entry" << std::endl;
    startSlowMotor(context);
}

void ServiceMotorRightSlowSubState::handle(Event e, Logik* context, ServiceState& parent) {
    if (e == Event::ENTRY_CLEARED) {
        logTransition("Motor_Right_Slow", "Slow_Reset_Tick", e);
        parent.replaceSubState(std::make_unique<ServiceSlowResetTickSubState>());
    }
}

void ServiceSlowResetTickSubState::entry(Logik* context, ServiceState& parent) {
    (void)parent;
    std::cout << "[Service] Reset_Tick (Slow) entry" << std::endl;

    if (context != nullptr) {
        context->resetServiceTickCounter();
    }
}

void ServiceSlowResetTickSubState::handle(Event e, Logik* context, ServiceState& parent) {
    if (e != Event::HEIGHT_BLOCKED || context == nullptr) {
        return;
    }

    storeMeasuredTick(context,
                      context->getCalibrationData().ticks_EntryC_to_HeightB_Slow,
                      "ticks_EntryC_to_HeightB_Slow");
    logTransition("Slow_Reset_Tick", "Wait_Height_Cleared", e);
    parent.replaceSubState(std::make_unique<ServiceWaitHeightClearedSubState>());
}

void ServiceWaitHeightClearedSubState::entry(Logik* context, ServiceState& parent) {
    (void)parent;
    (void)context;
    std::cout << "[Service] Ticks_Entry_c_To_Height_b_Slow (warte HEIGHT_CLEARED)" << std::endl;
}

void ServiceWaitHeightClearedSubState::handle(Event e, Logik* context, ServiceState& parent) {
    if (e != Event::HEIGHT_CLEARED || context == nullptr) {
        return;
    }

    storeMeasuredTick(context,
                      context->getCalibrationData().ticks_HeightB_to_HeightC_Slow,
                      "ticks_HeightB_to_HeightC_Slow");
    logTransition("Wait_Height_Cleared", "Wait_Ejector_Blocked", e);
    parent.replaceSubState(std::make_unique<ServiceWaitEjectorBlockedSubState>());
}

void ServiceWaitEjectorBlockedSubState::entry(Logik* context, ServiceState& parent) {
    (void)parent;
    std::cout << "[Service] Ticks_Height_b_To_Height_c_Slow (warte EJECTOR_BLOCKED)" << std::endl;

    if (context != nullptr) {
        prepareSlowPathSorter(context);
        startSlowMotor(context);
    }
}

void ServiceWaitEjectorBlockedSubState::handle(Event e, Logik* context, ServiceState& parent) {
    if (e != Event::EJECTOR_BLOCKED || context == nullptr) {
        return;
    }

    actuateSlowPathOnEjectorBlocked(context);

    storeMeasuredTick(context,
                      context->getCalibrationData().ticks_HeightC_to_EjectorB_Slow,
                      "ticks_HeightC_to_EjectorB_Slow");
    logTransition("Wait_Ejector_Blocked", "Wait_Ejector_Cleared", e);
    parent.replaceSubState(std::make_unique<ServiceWaitEjectorClearedSubState>());
}

void ServiceWaitEjectorClearedSubState::entry(Logik* context, ServiceState& parent) {
    (void)parent;
    std::cout << "[Service] Ticks_Height_c_To_Ejector_b_Slow (warte EJECTOR_CLEARED)" << std::endl;
    startSlowMotor(context);
}

void ServiceWaitEjectorClearedSubState::handle(Event e, Logik* context, ServiceState& parent) {
    if (e != Event::EJECTOR_CLEARED || context == nullptr) {
        return;
    }

    storeMeasuredTick(context,
                      context->getCalibrationData().ticks_EjectorB_to_EjectorC_Slow,
                      "ticks_EjectorB_to_EjectorC_Slow");
    logTransition("Wait_Ejector_Cleared", "Wait_Chute_Blocked", e);
    parent.replaceSubState(std::make_unique<ServiceWaitChuteBlockedSubState>());
}

void ServiceWaitChuteBlockedSubState::entry(Logik* context, ServiceState& parent) {
    (void)parent;
    std::cout << "[Service] Ticks_Ejector_b_To_Ejector_c_Slow (warte CHUTE_BLOCKED)" << std::endl;
    startSlowMotor(context);
}

void ServiceWaitChuteBlockedSubState::handle(Event e, Logik* context, ServiceState& parent) {
    if (e != Event::CHUTE_BLOCKED || context == nullptr) {
        return;
    }

    storeMeasuredTick(context,
                      context->getCalibrationData().ticks_EjectorC_to_ChuteB_Slow,
                      "ticks_EjectorC_to_ChuteB_Slow");
    logTransition("Wait_Chute_Blocked", "Slow_Calib_Done", e);
    parent.replaceSubState(std::make_unique<ServiceSlowCalibDoneSubState>());
}

void ServiceSlowCalibDoneSubState::entry(Logik* context, ServiceState& parent) {
    (void)parent;
    std::cout << "[Service] Slow-Kalibrierung abgeschlossen. Bitte 2. Werkstueck an Einlauf legen."
              << std::endl;

    if (context != nullptr) {
        context->recalculateSlowLimits();
        stopServiceMotor(context);
        if (auto* dispatcher = context->getDispatcher()) {
            dispatcher->lightGreenBlinkSlow();
        }
    }
}

void ServiceSlowCalibDoneSubState::handle(Event e, Logik* context, ServiceState& parent) {
    if (e == Event::ENTRY_BLOCKED) {
        logTransition("Slow_Calib_Done", "Normal_Start", e);
        parent.replaceSubState(std::make_unique<ServiceNormalStartSubState>());
    }
}

// ============================================================================
// Normal-Pfad (2. Lauf: driveRight bis END_BLOCKED)
// ============================================================================

void ServiceNormalStartSubState::entry(Logik* context, ServiceState& parent) {
    (void)parent;
    std::cout << "[Service] Normal_Start: driveRight(), Normal-Ticks messen." << std::endl;

    if (context != nullptr) {
        context->resetServiceTickCounter();
        startNormalMotor(context);
    }
}

void ServiceNormalStartSubState::handle(Event e, Logik* context, ServiceState& parent) {
    if (e != Event::HEIGHT_BLOCKED || context == nullptr) {
        return;
    }

    storeMeasuredTick(context,
                      context->getCalibrationData().ticks_EntryC_to_HeightB_Normal,
                      "ticks_EntryC_to_HeightB_Normal");
    logTransition("Normal_Start", "Normal_Wait_Height_Cleared", e);
    parent.replaceSubState(std::make_unique<ServiceNormalWaitHeightClearedSubState>());
}

void ServiceNormalWaitHeightClearedSubState::entry(Logik* context, ServiceState& parent) {
    (void)parent;
    std::cout << "[Service] Ticks_Entry_c_To_Height_b_Normal (warte HEIGHT_CLEARED)" << std::endl;
    startNormalMotor(context);
}

void ServiceNormalWaitHeightClearedSubState::handle(Event e, Logik* context, ServiceState& parent) {
    if (e != Event::HEIGHT_CLEARED || context == nullptr) {
        return;
    }

    storeMeasuredTick(context,
                      context->getCalibrationData().ticks_HeightB_to_HeightC_Normal,
                      "ticks_HeightB_to_HeightC_Normal");

    prepareNormalPathSorter(context);

    logTransition("Normal_Wait_Height_Cleared", "Normal_Wait_Ejector_Blocked", e);
    parent.replaceSubState(std::make_unique<ServiceNormalWaitEjectorBlockedSubState>());
}

void ServiceNormalWaitEjectorBlockedSubState::entry(Logik* context, ServiceState& parent) {
    (void)parent;
    std::cout << "[Service] Ticks_Height_b_To_Height_c_Normal (warte EJECTOR_BLOCKED)" << std::endl;
    startNormalMotor(context);
}

void ServiceNormalWaitEjectorBlockedSubState::handle(Event e, Logik* context, ServiceState& parent) {
    if (e != Event::EJECTOR_BLOCKED || context == nullptr) {
        return;
    }

    storeMeasuredTick(context,
                      context->getCalibrationData().ticks_HeightC_to_EjectorB_Normal,
                      "ticks_HeightC_to_EjectorB_Normal");
    logTransition("Normal_Wait_Ejector_Blocked", "Normal_Wait_Ejector_Cleared", e);
    parent.replaceSubState(std::make_unique<ServiceNormalWaitEjectorClearedSubState>());
}

void ServiceNormalWaitEjectorClearedSubState::entry(Logik* context, ServiceState& parent) {
    (void)parent;
    std::cout << "[Service] Ticks_Height_c_To_Ejector_b_Normal (warte EJECTOR_CLEARED)" << std::endl;
    startNormalMotor(context);
}

void ServiceNormalWaitEjectorClearedSubState::handle(Event e, Logik* context, ServiceState& parent) {
    if (e != Event::EJECTOR_CLEARED || context == nullptr) {
        return;
    }

    storeMeasuredTick(context,
                      context->getCalibrationData().ticks_EjectorB_to_EjectorC_Normal,
                      "ticks_EjectorB_to_EjectorC_Normal");
    logTransition("Normal_Wait_Ejector_Cleared", "Normal_Wait_End_Blocked", e);
    parent.replaceSubState(std::make_unique<ServiceNormalWaitEndBlockedSubState>());
}

void ServiceNormalWaitEndBlockedSubState::entry(Logik* context, ServiceState& parent) {
    (void)parent;
    std::cout << "[Service] Ticks_Ejector_b_To_Ejector_c_Normal (warte END_BLOCKED)" << std::endl;
    startNormalMotor(context);
}

void ServiceNormalWaitEndBlockedSubState::handle(Event e, Logik* context, ServiceState& parent) {
    if (e != Event::END_BLOCKED || context == nullptr) {
        return;
    }

    storeMeasuredTick(context,
                      context->getCalibrationData().ticks_EjectorC_to_EndB_Normal,
                      "ticks_EjectorC_to_EndB_Normal");

    stopServiceMotor(context);
    finishNormalPathSorter(context);

    if (auto* dispatcher = context->getDispatcher()) {
        dispatcher->lightGreenBlinkSlow();
    }

    logTransition("Normal_Wait_End_Blocked", "Calib_Finished", e);
    parent.replaceSubState(std::make_unique<ServiceCalibFinishedSubState>());
}

void ServiceCalibFinishedSubState::entry(Logik* context, ServiceState& parent) {
    (void)parent;
    std::cout << "[Service] Normal-Kalibrierung abgeschlossen. RESET druecken zum Speichern."
              << std::endl;
}

void ServiceCalibFinishedSubState::handle(Event e, Logik* context, ServiceState& parent) {
    if (e == Event::RESET_RELEASED || e == Event::PARTNER_RESET_RELEASED) {
        logTransition("Calib_Finished", "Done", e);
        if (context != nullptr) {
            context->saveCurrentCalibration();
            context->recalculateSlowLimits();

            if (context->getEventManager() != nullptr) {
                context->getEventManager()->setCalibrationDataReady(true);
            }
            if (auto* dispatcher = context->getDispatcher()) {
                dispatcher->lightGreenOff();
            }

            context->processEvent(Event::DATA_EXIST);
            context->notifyPartnerCalibStatus();
        }
        (void)parent;
    }
}

} // namespace logik
