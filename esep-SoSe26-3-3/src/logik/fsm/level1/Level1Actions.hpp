#ifndef LEVEL1_ACTIONS_HPP
#define LEVEL1_ACTIONS_HPP

class I_Dispatcher;

namespace logik {

class EventManager;

class Logik;

namespace level1 {

void applyIdleEntry(I_Dispatcher* dispatcher, EventManager* eventManager);
void applyReadyForServiceEntry(I_Dispatcher* dispatcher, EventManager* eventManager);
void applyReadyForServiceExit(I_Dispatcher* dispatcher, EventManager* eventManager);
void applyReadyForRunEntry(I_Dispatcher* dispatcher, EventManager* eventManager, Logik* logik);
void applyReadyForRunExit(I_Dispatcher* dispatcher, EventManager* eventManager);
void applyRunningEntry(I_Dispatcher* dispatcher, EventManager* eventManager);
void applyRunningExit(I_Dispatcher* dispatcher, EventManager* eventManager);
void applyServiceEntry(I_Dispatcher* dispatcher, EventManager* eventManager);
/** Nach ConnectionLost-Restore: Service-Modus ohne ADC-Neustart. */
void applyServiceResumeEntry(I_Dispatcher* dispatcher, EventManager* eventManager);
void applyServiceExit(I_Dispatcher* dispatcher, EventManager* eventManager);

void applyMotorRun(I_Dispatcher* dispatcher, EventManager* eventManager, bool blinkGreenSlow);
void applyMotorStop(I_Dispatcher* dispatcher, EventManager* eventManager, bool clearGreenLight);
void applyMotorSlow(I_Dispatcher* dispatcher, EventManager* eventManager);
void applyHeightCleared(I_Dispatcher* dispatcher, EventManager* eventManager);

/** Running nur wenn lokal + Partner kalibriert und verbunden. */
bool canEnterRunning(EventManager* eventManager);

/** Idle -> ReadyForRun wenn Datei + Partner bereit; sonst false. */
bool tryEnterReadyForRun(Logik* logik);

/** Idle -> ReadyForService wenn keine Kalibrierungsdatei; sonst false. */
bool tryEnterReadyForService(Logik* logik);

/** Nach Idle-Entry: ReadyForRun oder ReadyForService oder Idle belassen. */
void routeFromIdle(Logik* logik);

} // namespace level1
} // namespace logik

#endif
