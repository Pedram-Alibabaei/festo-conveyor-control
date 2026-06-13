#ifndef LEVEL0_ACTIONS_HPP
#define LEVEL0_ACTIONS_HPP

class I_Dispatcher;

namespace logik {

class Logik;

namespace level0 {

void applyConnectionLostEntry(I_Dispatcher* dispatcher);
void clearTrafficLightsForRestore(I_Dispatcher* dispatcher);
void applyEstopSafetyClear(Logik* logik);
bool isAnyEstopActive(Logik* logik);

} // namespace level0
} // namespace logik

#endif
