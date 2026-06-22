#include "ServiceCalibSubState.hpp"
#include "ServiceCalibRegistry.hpp"
#include "../IdleState.hpp"
#include "../ServiceState.hpp"
#include "logik/logik.hpp"
#include "logik/eventManager/eventManager.hpp"
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

    dispatcher->pushEjectorFor(1000);
    std::cout << "[Service] Auswerfer push 1s bei EJECTOR_BLOCKED (1. Lauf)." << std::endl;
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

/** ADC-Hoehensensor fuer Service-Tick-Messung scharf schalten (2. Lauf). */
void armHeightSensorForServiceTiming(Logik* context) {
    if (context == nullptr) {
        return;
    }

    if (auto* dispatcher = context->getDispatcher()) {
        dispatcher->setServiceTimingMeasureState();
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
    std::cout << "[Service] Init_Empty entry"
              << (ServiceCalibRegistry::isResuming() ? " (Resume)" : "") << std::endl;

    if (context == nullptr) {
        return;
    }

    if (ServiceCalibRegistry::isResuming()) {
        if (auto* dispatcher = context->getDispatcher()) {
            dispatcher->lightGreenBlinkSlow();
            dispatcher->driveStop();
        }
        return;
    }

    context->resetServiceTickCounter();
    m_emptyBeltReady = false;
    m_entryBlockedWhileWaiting = false;

    if (auto* dispatcher = context->getDispatcher()) {
        dispatcher->setAdcCalibrationsState();
        dispatcher->lightGreenBlinkSlow();
        dispatcher->driveStop();

        std::cout << "[Service] Sortier-Hardware: "
                  << (dispatcher->isSwitch() ? "Weiche" : "Auswerfer") << std::endl;
    }
}

void ServiceInitEmptySubState::handle(Event e, Logik* context, ServiceState& parent) {
    if (e == Event::SERVICE_EMPTY_BELT_READY) {
        m_emptyBeltReady = true;
        std::cout << "[Service] EMPTY_BELT bereit. "
                  << (m_entryBlockedWhileWaiting
                          ? "Werkstueck wartet bereits am Einlauf -> starte Slow-Lauf."
                          : "Warte auf Referenz-Werkstueck am Einlauf.")
                  << std::endl;
        if (!m_entryBlockedWhileWaiting) {
            return;
        }
        logTransition("Init_Empty", "Motor_Right_Slow", e);
        parent.replaceSubState(std::make_unique<ServiceMotorRightSlowSubState>());
        return;
    }

    if (e == Event::ENTRY_BLOCKED) {
        if (!m_emptyBeltReady) {
            m_entryBlockedWhileWaiting = true;
            std::cout << "[Service] ENTRY_BLOCKED vor EMPTY_BELT fertig -> "
                      << "warte mit Motorstart bis Leerband-Referenz bereit ist." << std::endl;
            return;
        }
        logTransition("Init_Empty", "Motor_Right_Slow", e);
        parent.replaceSubState(std::make_unique<ServiceMotorRightSlowSubState>());
        return;
    }

    if (e == Event::ENTRY_CLEARED && !m_emptyBeltReady) {
        m_entryBlockedWhileWaiting = false;
    }
}

void ServiceMotorRightSlowSubState::entry(Logik* context, ServiceState& parent) {
    (void)parent;
    std::cout << "[Service] Motor_Right_Slow entry"
              << (ServiceCalibRegistry::isResuming() ? " (Resume)" : "") << std::endl;
    if (!ServiceCalibRegistry::isResuming()) {
        startSlowMotor(context);
    }
}

void ServiceMotorRightSlowSubState::handle(Event e, Logik* context, ServiceState& parent) {
    if (e == Event::ENTRY_CLEARED) {
        logTransition("Motor_Right_Slow", "Slow_Reset_Tick", e);
        parent.replaceSubState(std::make_unique<ServiceSlowResetTickSubState>());
    }
}

void ServiceSlowResetTickSubState::entry(Logik* context, ServiceState& parent) {
    (void)parent;
    std::cout << "[Service] Reset_Tick (Slow) entry"
              << (ServiceCalibRegistry::isResuming() ? " (Resume)" : "") << std::endl;

    if (context != nullptr && !ServiceCalibRegistry::isResuming()) {
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
    std::cout << "[Service] Ticks_Height_b_To_Height_c_Slow (warte EJECTOR_BLOCKED)"
              << (ServiceCalibRegistry::isResuming() ? " (Resume)" : "") << std::endl;

    if (context != nullptr && !ServiceCalibRegistry::isResuming()) {
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
    std::cout << "[Service] Ticks_Height_c_To_Ejector_b_Slow (warte EJECTOR_CLEARED)"
              << (ServiceCalibRegistry::isResuming() ? " (Resume)" : "") << std::endl;
    if (!ServiceCalibRegistry::isResuming()) {
        startSlowMotor(context);
    }
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
    std::cout << "[Service] Ticks_Ejector_b_To_Ejector_c_Slow (warte CHUTE_BLOCKED)"
              << (ServiceCalibRegistry::isResuming() ? " (Resume)" : "") << std::endl;
    if (!ServiceCalibRegistry::isResuming()) {
        startSlowMotor(context);
    }
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
              << (ServiceCalibRegistry::isResuming() ? " (Resume)" : "") << std::endl;

    if (context != nullptr && !ServiceCalibRegistry::isResuming()) {
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
    std::cout << "[Service] Normal_Start: driveRight(), warte ENTRY_CLEARED."
              << (ServiceCalibRegistry::isResuming() ? " (Resume)" : "") << std::endl;

    if (context != nullptr && !ServiceCalibRegistry::isResuming()) {
        armHeightSensorForServiceTiming(context);
        context->resetServiceTickCounter();
        startNormalMotor(context);
    }
}

void ServiceNormalStartSubState::handle(Event e, Logik* context, ServiceState& parent) {
    if (e != Event::ENTRY_CLEARED || context == nullptr) {
        return;
    }

    storeMeasuredTick(context,
                      context->getCalibrationData().ticks_EntryB_to_EntryC_Normal,
                      "ticks_EntryB_to_EntryC_Normal");
    logTransition("Normal_Start", "Normal_Reset_Tick", e);
    parent.replaceSubState(std::make_unique<ServiceNormalResetTickSubState>());
}

void ServiceNormalResetTickSubState::entry(Logik* context, ServiceState& parent) {
    (void)parent;
    std::cout << "[Service] Reset_Tick (Normal) entry"
              << (ServiceCalibRegistry::isResuming() ? " (Resume)" : "") << std::endl;

    if (context != nullptr && !ServiceCalibRegistry::isResuming()) {
        context->resetServiceTickCounter();
        startNormalMotor(context);
    }
}

void ServiceNormalResetTickSubState::handle(Event e, Logik* context, ServiceState& parent) {
    if (e != Event::HEIGHT_BLOCKED || context == nullptr) {
        return;
    }

    storeMeasuredTick(context,
                      context->getCalibrationData().ticks_EntryC_to_HeightB_Normal,
                      "ticks_EntryC_to_HeightB_Normal");
    logTransition("Normal_Reset_Tick", "Normal_Wait_Height_Cleared", e);
    parent.replaceSubState(std::make_unique<ServiceNormalWaitHeightClearedSubState>());
}

void ServiceNormalWaitHeightClearedSubState::entry(Logik* context, ServiceState& parent) {
    (void)parent;
    std::cout << "[Service] Ticks_Entry_c_To_Height_b_Normal (warte HEIGHT_CLEARED)"
              << (ServiceCalibRegistry::isResuming() ? " (Resume)" : "") << std::endl;
    if (!ServiceCalibRegistry::isResuming()) {
        startNormalMotor(context);
    }
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
    std::cout << "[Service] Ticks_Height_b_To_Height_c_Normal (warte EJECTOR_BLOCKED)"
              << (ServiceCalibRegistry::isResuming() ? " (Resume)" : "") << std::endl;
    if (!ServiceCalibRegistry::isResuming()) {
        startNormalMotor(context);
    }
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
    std::cout << "[Service] Ticks_Height_c_To_Ejector_b_Normal (warte EJECTOR_CLEARED)"
              << (ServiceCalibRegistry::isResuming() ? " (Resume)" : "") << std::endl;
    if (!ServiceCalibRegistry::isResuming()) {
        startNormalMotor(context);
    }
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
    std::cout << "[Service] Ticks_Ejector_b_To_Ejector_c_Normal (warte END_BLOCKED)"
              << (ServiceCalibRegistry::isResuming() ? " (Resume)" : "") << std::endl;
    if (!ServiceCalibRegistry::isResuming()) {
        startNormalMotor(context);
    }
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

    logTransition("Normal_Wait_End_Blocked", "Wait_Wp_Removed", e);
    parent.replaceSubState(std::make_unique<ServiceWaitWpRemovedSubState>());
}

namespace {

void enterCalibAwaitReset(Logik* context, ServiceState& parent) {
    (void)context;
    parent.replaceSubState(std::make_unique<ServiceCalibFinishedSubState>());
}

} // namespace

void ServiceWaitWpRemovedSubState::entry(Logik* context, ServiceState& parent) {
    (void)parent;
    std::cout << "[Service] Bandende erreicht. Bitte Werkstueck entfernen."
              << (ServiceCalibRegistry::isResuming() ? " (Resume)" : "") << std::endl;

    if (context == nullptr) {
        return;
    }

    if (auto* eventManager = context->getEventManager()) {
        if (!eventManager->isEndBlocked()) {
            enterCalibAwaitReset(context, parent);
        }
    }
}

void ServiceWaitWpRemovedSubState::handle(Event e, Logik* context, ServiceState& parent) {
    if (e != Event::END_CLEARED || context == nullptr) {
        return;
    }

    logTransition("Wait_Wp_Removed", "Calib_Finished", e);
    enterCalibAwaitReset(context, parent);
}

void ServiceCalibFinishedSubState::entry(Logik* context, ServiceState& parent) {
    (void)parent;
    std::cout << "[Service] Werkstueck entfernt. RESET-LED blinkt – RESET druecken zum Speichern."
              << std::endl;

    if (context != nullptr) {
        if (auto* dispatcher = context->getDispatcher()) {
            dispatcher->turnLEDResetBlinkSlow();
        }
    }
}

void ServiceCalibFinishedSubState::handle(Event e, Logik* context, ServiceState& parent) {
    if (e != Event::RESET_RELEASED || context == nullptr) {
        return;
    }

    logTransition("Calib_Finished", "Done", e);
    if (auto* dispatcher = context->getDispatcher()) {
        dispatcher->turnLEDResetOff();
    }

    if (context->saveCurrentCalibration()) {
        if (auto* eventManager = context->getEventManager()) {
            eventManager->setCalibrationDataReady(true);
        }
        std::cout << "[Service] Kalibrierung gespeichert -> IdleState" << std::endl;
        context->transitionTo(new IdleState());
    } else {
        std::cout << "[Service] Speichern fehlgeschlagen. Kalibrierung wiederholen."
                  << std::endl;
        if (auto* dispatcher = context->getDispatcher()) {
            dispatcher->lightYellowBlinkFast();
            dispatcher->turnLEDResetBlinkSlow();
        }
    }
    (void)parent;
}

} // namespace logik
