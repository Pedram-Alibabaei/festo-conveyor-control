/**
 * @file hal_actuators.hpp
 * @brief Konkrete Implementierung der i_Actuators Schnittstelle (Facade Pattern).
 *
 * Bündelt alle Output-Controller (Motor, Weiche, Ampel, LEDs) und delegiert
 * die Befehle der Hauptlogik sicher an die Hardware-Treiber.
 */

#ifndef HAL_ACTUATORS_HPP
#define HAL_ACTUATORS_HPP

#include "i_Actuators.hpp"

// Unsere 5 genialen Controller einbinden
#include "output_driver/blink_timer.hpp"
#include "output_driver/motor_controller.hpp"
#include "output_driver/ejector_controller.hpp"
#include "output_driver/traffic_light_controller.hpp"
#include "output_driver/panel_led_controller.hpp"

namespace hal {

class HalActuators : public i_Actuators {
private:
    // WICHTIG: Die Reihenfolge hier bestimmt die Initialisierungsreihenfolge in C++!
    // Der Timer muss vor den Licht-Controllern gebaut werden, damit er existiert.
    output_driver::BlinkTimer m_blinkTimer;

    output_driver::MotorController m_motorController;
    output_driver::EjectorController m_ejectorController;
    output_driver::TrafficLightController m_trafficLight;
    output_driver::PanelLedController m_panelLeds;

public:
    /**
     * @brief Konstruktor der Aktorik-Fassade.
     * @param type Bestimmt, ob die Anlage eine Weiche oder einen Auswerfer hat.
     */
    explicit HalActuators();
    ~HalActuators() override;

    // Verhindert Kopieren
    HalActuators(const HalActuators&) = delete;
    HalActuators& operator=(const HalActuators&) = delete;

    // ========================================================================
    // Implementierung des i_Actuators Vertrages (override)
    // ========================================================================

    // --- Motorsteuerung ---
    void driveRight() override;
    void driveLeft() override;
    void driveSlowRight() override;
    void driveSlowLeft() override;
    void driveStop() override;

    // --- Sortiereinheit ---
    void openJunction() override;
    void closeJunction() override;

    // --- Ampelsteuerung ---
    void lightGreenOn() override;
    void lightGreenOff() override;
    void lightGreenBlinkFast() override;
    void lightGreenBlinkSlow() override;

    void lightYellowOn() override;
    void lightYellowOff() override;
    void lightYellowBlinkFast() override;
    void lightYellowBlinkSlow() override;

    void lightRedOn() override;
    void lightRedOff() override;
    void lightRedBlinkFast() override;
    void lightRedBlinkSlow() override;

    // --- Bedienpanel-LEDs ---
    void turnLEDStartOn() override;
    void turnLEDStartOff() override;
    void turnLEDStartBlinkFast() override;
    void turnLEDStartBlinkSlow() override;

    void turnLEDResetOn() override;
    void turnLEDResetOff() override;
    void turnLEDResetBlinkFast() override;
    void turnLEDResetBlinkSlow() override;

    void turnLEDQ1On() override;
    void turnLEDQ1Off() override;
    void turnLEDQ1BlinkFast() override;
    void turnLEDQ1BlinkSlow() override;

    void turnLEDQ2On() override;
    void turnLEDQ2Off() override;
    void turnLEDQ2BlinkFast() override;
    void turnLEDQ2BlinkSlow() override;

    // --- Globaler Reset ---
    void turnAllOff() override;
};

} // namespace hal

#endif // HAL_ACTUATORS_HPP