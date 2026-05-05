/**
 * @file panel_led_controller.cpp
 * @brief Implementierung der Bedienpanel-LED-Steuerung.
 */

#include "panel_led_controller.hpp"

namespace hal {
namespace output_driver {

PanelLedController::PanelLedController(BlinkTimer* timer)
    : m_blinkTimer(timer)
{
    // Beim Hochfahren sollen alle Panel-LEDs standardmäßig aus sein
    turnLEDStartOff();
    turnLEDResetOff();
    turnLEDQ1Off();
    turnLEDQ2Off();
}

// ============================================================================
// Start-Tasten-LED
// ============================================================================
void PanelLedController::turnLEDStartOn() {
    if (m_blinkTimer) m_blinkTimer->setLightState(LightID::PANEL_START, LightState::SOLID_ON);
}

void PanelLedController::turnLEDStartOff() {
    if (m_blinkTimer) m_blinkTimer->setLightState(LightID::PANEL_START, LightState::OFF);
}

void PanelLedController::turnLEDStartBlinkFast() {
    if (m_blinkTimer) m_blinkTimer->setLightState(LightID::PANEL_START, LightState::BLINK_FAST);
}

void PanelLedController::turnLEDStartBlinkSlow() {
    if (m_blinkTimer) m_blinkTimer->setLightState(LightID::PANEL_START, LightState::BLINK_SLOW);
}

// ============================================================================
// Reset-Tasten-LED
// ============================================================================
void PanelLedController::turnLEDResetOn() {
    if (m_blinkTimer) m_blinkTimer->setLightState(LightID::PANEL_RESET, LightState::SOLID_ON);
}

void PanelLedController::turnLEDResetOff() {
    if (m_blinkTimer) m_blinkTimer->setLightState(LightID::PANEL_RESET, LightState::OFF);
}

void PanelLedController::turnLEDResetBlinkFast() {
    if (m_blinkTimer) m_blinkTimer->setLightState(LightID::PANEL_RESET, LightState::BLINK_FAST);
}

void PanelLedController::turnLEDResetBlinkSlow() {
    if (m_blinkTimer) m_blinkTimer->setLightState(LightID::PANEL_RESET, LightState::BLINK_SLOW);
}

// ============================================================================
// Signalleuchte Q1
// ============================================================================
void PanelLedController::turnLEDQ1On() {
    if (m_blinkTimer) m_blinkTimer->setLightState(LightID::PANEL_Q1, LightState::SOLID_ON);
}

void PanelLedController::turnLEDQ1Off() {
    if (m_blinkTimer) m_blinkTimer->setLightState(LightID::PANEL_Q1, LightState::OFF);
}

void PanelLedController::turnLEDQ1BlinkFast() {
    if (m_blinkTimer) m_blinkTimer->setLightState(LightID::PANEL_Q1, LightState::BLINK_FAST);
}

void PanelLedController::turnLEDQ1BlinkSlow() {
    if (m_blinkTimer) m_blinkTimer->setLightState(LightID::PANEL_Q1, LightState::BLINK_SLOW);
}

// ============================================================================
// Signalleuchte Q2
// ============================================================================
void PanelLedController::turnLEDQ2On() {
    if (m_blinkTimer) m_blinkTimer->setLightState(LightID::PANEL_Q2, LightState::SOLID_ON);
}

void PanelLedController::turnLEDQ2Off() {
    if (m_blinkTimer) m_blinkTimer->setLightState(LightID::PANEL_Q2, LightState::OFF);
}

void PanelLedController::turnLEDQ2BlinkFast() {
    if (m_blinkTimer) m_blinkTimer->setLightState(LightID::PANEL_Q2, LightState::BLINK_FAST);
}

void PanelLedController::turnLEDQ2BlinkSlow() {
    if (m_blinkTimer) m_blinkTimer->setLightState(LightID::PANEL_Q2, LightState::BLINK_SLOW);
}

} // namespace output_driver
} // namespace hal