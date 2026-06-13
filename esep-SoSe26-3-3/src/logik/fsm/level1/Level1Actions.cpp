#include "Level1Actions.hpp"
#include "logik/eventManager/eventManager.hpp"
#include "dispatcher/i_Dispatcher.hpp"

namespace logik {
namespace level1 {

void applyIdleEntry(I_Dispatcher* dispatcher, EventManager* eventManager) {
    if (dispatcher == nullptr) {
        return;
    }

    dispatcher->setAdcIdelState();
    dispatcher->driveStop();
    dispatcher->lightGreenOff();

    if (eventManager != nullptr) {
        eventManager->setServiceMode(false);
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
}

void applyServiceExit(I_Dispatcher* dispatcher, EventManager* eventManager) {
    if (dispatcher != nullptr) {
        dispatcher->driveStop();
        dispatcher->lightGreenOff();
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

} // namespace level1
} // namespace logik
