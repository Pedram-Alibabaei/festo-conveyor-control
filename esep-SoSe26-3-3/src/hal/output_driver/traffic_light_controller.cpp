/**
 * @file traffic_light_controller.cpp
 * @brief Implementierung der Ampelsteuerung.
 */

#include "traffic_light_controller.hpp"

namespace hal {
namespace output_driver {

TrafficLightController::TrafficLightController(BlinkTimer* timer)
    : m_blinkTimer(timer)
{
    // Beim Start der Anlage sollen alle Ampeln standardmäßig aus sein
    greenOff();
    yellowOff();
    redOff();
}

// ============================================================================
// Grüne Ampel
// ============================================================================
void TrafficLightController::greenOn() {
    if (m_blinkTimer) m_blinkTimer->setLightState(LightID::TRAFFIC_GREEN, LightState::SOLID_ON);
}

void TrafficLightController::greenOff() {
    if (m_blinkTimer) m_blinkTimer->setLightState(LightID::TRAFFIC_GREEN, LightState::OFF);
}

void TrafficLightController::greenBlinkFast() {
    if (m_blinkTimer) m_blinkTimer->setLightState(LightID::TRAFFIC_GREEN, LightState::BLINK_FAST);
}

void TrafficLightController::greenBlinkSlow() {
    if (m_blinkTimer) m_blinkTimer->setLightState(LightID::TRAFFIC_GREEN, LightState::BLINK_SLOW);
}

// ============================================================================
// Gelbe Ampel
// ============================================================================
void TrafficLightController::yellowOn() {
    if (m_blinkTimer) m_blinkTimer->setLightState(LightID::TRAFFIC_YELLOW, LightState::SOLID_ON);
}

void TrafficLightController::yellowOff() {
    if (m_blinkTimer) m_blinkTimer->setLightState(LightID::TRAFFIC_YELLOW, LightState::OFF);
}

void TrafficLightController::yellowBlinkFast() {
    if (m_blinkTimer) m_blinkTimer->setLightState(LightID::TRAFFIC_YELLOW, LightState::BLINK_FAST);
}

void TrafficLightController::yellowBlinkSlow() {
    if (m_blinkTimer) m_blinkTimer->setLightState(LightID::TRAFFIC_YELLOW, LightState::BLINK_SLOW);
}

// ============================================================================
// Rote Ampel
// ============================================================================
void TrafficLightController::redOn() {
    if (m_blinkTimer) m_blinkTimer->setLightState(LightID::TRAFFIC_RED, LightState::SOLID_ON);
}

void TrafficLightController::redOff() {
    if (m_blinkTimer) m_blinkTimer->setLightState(LightID::TRAFFIC_RED, LightState::OFF);
}

void TrafficLightController::redBlinkFast() {
    if (m_blinkTimer) m_blinkTimer->setLightState(LightID::TRAFFIC_RED, LightState::BLINK_FAST);
}

void TrafficLightController::redBlinkSlow() {
    if (m_blinkTimer) m_blinkTimer->setLightState(LightID::TRAFFIC_RED, LightState::BLINK_SLOW);
}

} // namespace output_driver
} // namespace hal