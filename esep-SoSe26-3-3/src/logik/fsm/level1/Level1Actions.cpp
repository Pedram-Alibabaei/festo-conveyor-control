#include "Level1Actions.hpp"
#include "logik/eventManager/eventManager.hpp"
#include "logik/logik.hpp"
#include "logik/fsm/states/ReadyForServiceState.hpp"
#include "logik/fsm/states/ReadyForRunState.hpp"
#include "dispatcher/i_Dispatcher.hpp"
#include <iostream>

namespace logik {
namespace level1 {

void applyIdleEntry(I_Dispatcher* dispatcher, EventManager* eventManager) {
    if (dispatcher == nullptr) {
        return;
    }

    dispatcher->setAdcIdelState();
    dispatcher->driveStop();
    dispatcher->lightGreenOff();
    dispatcher->lightYellowOff();
    dispatcher->turnLEDResetOff();
    dispatcher->turnLEDStartOff();

    if (eventManager != nullptr) {
        eventManager->setServiceMode(false);
        eventManager->setMotorOn(false);
        eventManager->setMotorSlow(false);
    }
}

void applyReadyForServiceEntry(I_Dispatcher* dispatcher, EventManager* eventManager) {
    if (dispatcher == nullptr) {
        return;
    }

    dispatcher->setAdcIdelState();
    dispatcher->driveStop();
    dispatcher->lightGreenOff();
    dispatcher->lightYellowOff();
    dispatcher->turnLEDResetOff();
    dispatcher->turnLEDStartBlinkSlow();

    if (eventManager != nullptr) {
        eventManager->setServiceMode(false);
        eventManager->setMotorOn(false);
        eventManager->setMotorSlow(false);
    }
}

void applyReadyForServiceExit(I_Dispatcher* dispatcher, EventManager* eventManager) {
    if (dispatcher != nullptr) {
        dispatcher->turnLEDStartOff();
    }

    (void)eventManager;
}

void applyReadyForRunEntry(I_Dispatcher* dispatcher, EventManager* eventManager, Logik* logik) {
    if (dispatcher == nullptr) {
        return;
    }

    dispatcher->setAdcWaitigForWpState();
    dispatcher->driveStop();
    dispatcher->lightGreenOff();
    dispatcher->lightYellowOff();
    dispatcher->turnLEDResetOff();
    dispatcher->turnLEDStartOn();

    if (logik != nullptr) {
        logik->loadCalibrationData();
    }

    if (eventManager != nullptr) {
        eventManager->setServiceMode(false);
        eventManager->setMotorOn(false);
        eventManager->setMotorSlow(false);
    }
}

void applyReadyForRunExit(I_Dispatcher* dispatcher, EventManager* eventManager) {
    if (dispatcher != nullptr) {
        dispatcher->driveStop();
        dispatcher->turnLEDStartOff();
    }

    if (eventManager != nullptr) {
        eventManager->setMotorOn(false);
        eventManager->setMotorSlow(false);
    }
}

void applyRunningEntry(I_Dispatcher* dispatcher, EventManager* eventManager) {
    if (dispatcher == nullptr) {
        return;
    }

    dispatcher->setAdcWaitigForWpState();
    dispatcher->lightGreenOn();
    dispatcher->turnLEDStartOff();

    if (eventManager != nullptr) {
        eventManager->setServiceMode(false);
    }
}

void applyRunningExit(I_Dispatcher* dispatcher, EventManager* eventManager) {
    if (dispatcher != nullptr) {
        dispatcher->driveStop();
    }

    if (eventManager != nullptr) {
        eventManager->setMotorOn(false);
        eventManager->setMotorSlow(false);
    }
}

void applyServiceResumeEntry(I_Dispatcher* dispatcher, EventManager* eventManager) {
    if (dispatcher == nullptr) {
        return;
    }

    if (eventManager != nullptr) {
        eventManager->setServiceMode(true);
        eventManager->setMotorOn(false);
        eventManager->setMotorSlow(false);
    }

    dispatcher->driveStop();
    dispatcher->lightGreenBlinkSlow();
}

void applyServiceEntry(I_Dispatcher* dispatcher, EventManager* eventManager) {
    if (dispatcher == nullptr) {
        return;
    }

    if (eventManager != nullptr) {
        eventManager->setServiceMode(true);
        eventManager->setMotorOn(false);
        eventManager->setMotorSlow(false);
    }

    dispatcher->setAdcCalibrationsState();
    dispatcher->lightGreenBlinkFast();
    dispatcher->driveStop();
    dispatcher->turnLEDStartOff();
}

void applyServiceExit(I_Dispatcher* dispatcher, EventManager* eventManager) {
    if (dispatcher != nullptr) {
        dispatcher->driveStop();
        dispatcher->lightGreenOff();
        dispatcher->turnLEDResetOff();
    }

    if (eventManager != nullptr) {
        eventManager->setServiceMode(false);
        eventManager->setMotorOn(false);
        eventManager->setMotorSlow(false);
    }
}

void applyMotorRun(I_Dispatcher* dispatcher, EventManager* eventManager, bool blinkGreenSlow) {
    if (dispatcher == nullptr) {
        return;
    }

    dispatcher->driveRight();
    if (blinkGreenSlow) {
        dispatcher->lightGreenBlinkSlow();
    }

    if (eventManager != nullptr) {
        eventManager->setMotorOn(true);
    }
}

void applyMotorStop(I_Dispatcher* dispatcher, EventManager* eventManager, bool clearGreenLight) {
    if (dispatcher != nullptr) {
        dispatcher->driveStop();
        if (clearGreenLight) {
            dispatcher->lightGreenOff();
        }
    }

    if (eventManager != nullptr) {
        eventManager->setMotorOn(false);
    }
}

void applyMotorSlow(I_Dispatcher* dispatcher, EventManager* eventManager) {
    if (dispatcher == nullptr) {
        return;
    }

    dispatcher->driveSlowRight();

    if (eventManager != nullptr) {
        eventManager->setMotorSlow(true);
    }
}

void applyHeightCleared(I_Dispatcher* dispatcher, EventManager* eventManager) {
    if (dispatcher == nullptr) {
        return;
    }

    dispatcher->driveRight();

    if (eventManager != nullptr) {
        eventManager->setMotorSlow(false);
    }
}

bool canEnterRunning(EventManager* eventManager) {
    if (eventManager == nullptr) {
        return false;
    }

    if (!eventManager->isCalibrationDataReady()) {
        return false;
    }
    if (!eventManager->isPartnerCalibrationReady()) {
        return false;
    }
    if (eventManager->isConnectionLost()) {
        return false;
    }

    return true;
}

bool tryEnterReadyForRun(Logik* logik) {
    if (logik == nullptr) {
        return false;
    }

    auto* eventManager = logik->getEventManager();
    auto* dispatcher = logik->getDispatcher();

    if (dispatcher != nullptr) {
        const bool fileOk = dispatcher->requestFileExists();
        if (eventManager != nullptr) {
            eventManager->setCalibrationDataReady(fileOk);
        }
        if (!fileOk) {
            return false;
        }
        logik->loadCalibrationData();
    }

    if (!canEnterRunning(eventManager)) {
        return false;
    }

    std::cout << "[Level1] Lokal + Partner kalibriert -> ReadyForRun" << std::endl;
    logik->transitionTo(new ReadyForRunState());
    return true;
}

bool tryEnterReadyForService(Logik* logik) {
    if (logik == nullptr) {
        return false;
    }

    auto* dispatcher = logik->getDispatcher();
    auto* eventManager = logik->getEventManager();

    if (dispatcher != nullptr && dispatcher->requestFileExists()) {
        return false;
    }

    if (eventManager != nullptr) {
        eventManager->setCalibrationDataReady(false);
    }

    std::cout << "[Level1] Keine Kalibrierungsdatei -> ReadyForService" << std::endl;
    logik->transitionTo(new ReadyForServiceState());
    return true;
}

void routeFromIdle(Logik* logik) {
    if (logik == nullptr) {
        return;
    }

    if (tryEnterReadyForRun(logik)) {
        return;
    }

    tryEnterReadyForService(logik);
}

} // namespace level1
} // namespace logik
