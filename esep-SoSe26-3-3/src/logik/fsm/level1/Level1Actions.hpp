#ifndef LEVEL1_ACTIONS_HPP
#define LEVEL1_ACTIONS_HPP

class I_Dispatcher;

namespace logik {

class EventManager;

namespace level1 {

void applyIdleEntry(I_Dispatcher* dispatcher, EventManager* eventManager);
void applyRunningEntry(I_Dispatcher* dispatcher, EventManager* eventManager);
void applyRunningExit(I_Dispatcher* dispatcher, EventManager* eventManager);
void applyServiceEntry(I_Dispatcher* dispatcher, EventManager* eventManager);
void applyServiceExit(I_Dispatcher* dispatcher, EventManager* eventManager);

void applyMotorRun(I_Dispatcher* dispatcher, EventManager* eventManager, bool blinkGreenSlow);
void applyMotorStop(I_Dispatcher* dispatcher, EventManager* eventManager, bool clearGreenLight);
void applyMotorSlow(I_Dispatcher* dispatcher, EventManager* eventManager);
void applyHeightCleared(I_Dispatcher* dispatcher, EventManager* eventManager);

/** Running nur wenn lokal + Partner kalibriert und verbunden. */
bool canEnterRunning(EventManager* eventManager);

} // namespace level1
} // namespace logik

#endif
