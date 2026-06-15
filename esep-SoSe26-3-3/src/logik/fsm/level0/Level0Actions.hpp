#ifndef LEVEL0_ACTIONS_HPP
#define LEVEL0_ACTIONS_HPP

class I_Dispatcher;

namespace logik {

class Logik;
class EventManager;

namespace level0 {

void applyConnectionLostEntry(I_Dispatcher* dispatcher, EventManager* eventManager = nullptr);
void applyConnectionLostEstopWaitingResetEntry(I_Dispatcher* dispatcher);
void clearTrafficLightsForRestore(I_Dispatcher* dispatcher);
void applyEstopPressedTraffic(I_Dispatcher* dispatcher);
void applyEstopLocalActiveEntry(Logik* logik);
void applyEstopPartnerActiveEntry(Logik* logik);
void applyEstopBothNotActiveEntry(I_Dispatcher* dispatcher);
void syncLocalEstopPanelLed(I_Dispatcher* dispatcher);
void applyEstopSafetyClear(Logik* logik);
void finishEstopReset(Logik* logik);
bool isAnyEstopActive(Logik* logik);
bool isLocalEstopActive(I_Dispatcher* dispatcher);
bool isPartnerEstopActive(Logik* logik); ///< Nur Partner-Taster gedrückt (nicht Sicherheitsmodus)

} // namespace level0
} // namespace logik

#endif
