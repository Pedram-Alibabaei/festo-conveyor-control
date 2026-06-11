/**
 * @file hal_actuators.cpp
 * @brief Implementierung der Aktorik-Fassade.
 */

#include "hal_actuators.hpp"

namespace hal {

// Konstruktor: Wir übergeben den Typ an den Ejector und die Adresse des Timers an die Licht-Controller
HalActuators::HalActuators()
    : m_blinkTimer(),                      // 1. Hintergrund-Thread startet
      m_motorController(),                 // 2. Motor wird initialisiert (und gestoppt)
      m_ejectorController(),           // 3. Weiche/Auswerfer wird konfiguriert
      m_trafficLight(&m_blinkTimer),       // 4. Ampel bekommt den Timer
      m_panelLeds(&m_blinkTimer)           // 5. Panel bekommt den Timer
{
    // Beim Start zur Sicherheit alles in den Grundzustand versetzen
    turnAllOff();
}

HalActuators::~HalActuators() {
    turnAllOff();
}

// ============================================================================
// Motorsteuerung
// ============================================================================
void HalActuators::driveRight()       { m_motorController.driveRight(); }
void HalActuators::driveLeft()        { m_motorController.driveLeft(); }
void HalActuators::driveSlowRight()   { m_motorController.driveSlowRight(); }
void HalActuators::driveSlowLeft()    { m_motorController.driveSlowLeft(); }
void HalActuators::driveStop()        { m_motorController.stop(); }

// ============================================================================
// Sortiereinheit
// ============================================================================

bool HalActuators::isSwitch()         { return m_ejectorController.isSwitch(); }
void HalActuators::openSwitch()       { m_ejectorController.openSwitch(); }
void HalActuators::closeSwitch()      { m_ejectorController.closeSwitch(); }
void HalActuators::pushEjector()      { m_ejectorController.pushEjector(); }
void HalActuators::pullEjector()      { m_ejectorController.pullEjector(); }

// ============================================================================
// Ampelsteuerung
// ============================================================================
void HalActuators::lightGreenOn()         { m_trafficLight.greenOn(); }
void HalActuators::lightGreenOff()        { m_trafficLight.greenOff(); }
void HalActuators::lightGreenBlinkFast()  { m_trafficLight.greenBlinkFast(); }
void HalActuators::lightGreenBlinkSlow()  { m_trafficLight.greenBlinkSlow(); }

void HalActuators::lightYellowOn()        { m_trafficLight.yellowOn(); }
void HalActuators::lightYellowOff()       { m_trafficLight.yellowOff(); }
void HalActuators::lightYellowBlinkFast() { m_trafficLight.yellowBlinkFast(); }
void HalActuators::lightYellowBlinkSlow() { m_trafficLight.yellowBlinkSlow(); }

void HalActuators::lightRedOn()           { m_trafficLight.redOn(); }
void HalActuators::lightRedOff()          { m_trafficLight.redOff(); }
void HalActuators::lightRedBlinkFast()    { m_trafficLight.redBlinkFast(); }
void HalActuators::lightRedBlinkSlow()    { m_trafficLight.redBlinkSlow(); }

// ============================================================================
// Bedienpanel-LEDs
// ============================================================================
void HalActuators::turnLEDStartOn()         { m_panelLeds.turnLEDStartOn(); }
void HalActuators::turnLEDStartOff()        { m_panelLeds.turnLEDStartOff(); }
void HalActuators::turnLEDStartBlinkFast()  { m_panelLeds.turnLEDStartBlinkFast(); }
void HalActuators::turnLEDStartBlinkSlow()  { m_panelLeds.turnLEDStartBlinkSlow(); }

void HalActuators::turnLEDResetOn()         { m_panelLeds.turnLEDResetOn(); }
void HalActuators::turnLEDResetOff()        { m_panelLeds.turnLEDResetOff(); }
void HalActuators::turnLEDResetBlinkFast()  { m_panelLeds.turnLEDResetBlinkFast(); }
void HalActuators::turnLEDResetBlinkSlow()  { m_panelLeds.turnLEDResetBlinkSlow(); }

void HalActuators::turnLEDQ1On()            { m_panelLeds.turnLEDQ1On(); }
void HalActuators::turnLEDQ1Off()           { m_panelLeds.turnLEDQ1Off(); }
void HalActuators::turnLEDQ1BlinkFast()     { m_panelLeds.turnLEDQ1BlinkFast(); }
void HalActuators::turnLEDQ1BlinkSlow()     { m_panelLeds.turnLEDQ1BlinkSlow(); }

void HalActuators::turnLEDQ2On()            { m_panelLeds.turnLEDQ2On(); }
void HalActuators::turnLEDQ2Off()           { m_panelLeds.turnLEDQ2Off(); }
void HalActuators::turnLEDQ2BlinkFast()     { m_panelLeds.turnLEDQ2BlinkFast(); }
void HalActuators::turnLEDQ2BlinkSlow()     { m_panelLeds.turnLEDQ2BlinkSlow(); }

// ============================================================================
// Globaler Reset
// ============================================================================
void HalActuators::turnAllOff() {
    driveStop();
    if (isSwitch()){closeSwitch();}else{pullEjector();}


    lightGreenOff();
    lightYellowOff();
    lightRedOff();

    turnLEDStartOff();
    turnLEDResetOff();
    turnLEDQ1Off();
    turnLEDQ2Off();
}

} // namespace hal
