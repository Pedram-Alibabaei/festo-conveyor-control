/**
 * @file blink_timer.cpp
 * @brief Implementierung des asynchronen Blink-Timers.
 */

#include "blink_timer.hpp"
#include <hal/pal.hpp>
#include <chrono>

namespace hal {
namespace output_driver {

BlinkTimer::BlinkTimer() : m_running(true) {
    // 1. Alle benötigten Pins als Ausgänge konfigurieren
    pal::os::Green::config_as_output();
    pal::os::Yellow::config_as_output();
    pal::os::Red::config_as_output();

    pal::oi::Start::config_as_output();
    pal::oi::Reset::config_as_output();
    pal::oi::Q1::config_as_output();
    pal::oi::Q2::config_as_output();

    // 2. Initial alle Lampen auf OFF setzen
    for (size_t i = 0; i < static_cast<size_t>(LightID::NUM_LIGHTS); ++i) {
        m_lightStates[i] = LightState::OFF;
        updateHardwarePin(static_cast<LightID>(i), false);
    }

    // 3. Den Hintergrund-Thread starten
    m_timerThread = std::thread(&BlinkTimer::timerLoop, this);
}

BlinkTimer::~BlinkTimer() {
    // Thread sauber beenden
    m_running = false;
    if (m_timerThread.joinable()) {
        m_timerThread.join();
    }

    // Zur Sicherheit beim Herunterfahren alle Lampen ausschalten
    for (size_t i = 0; i < static_cast<size_t>(LightID::NUM_LIGHTS); ++i) {
        updateHardwarePin(static_cast<LightID>(i), false);
    }
}

void BlinkTimer::setLightState(LightID light, LightState state) {
    // Den Mutex sperren, damit der Timer-Thread nicht genau in diesem
    // Moment das Array liest.
    std::lock_guard<std::mutex> lock(m_mutex);

    m_lightStates[static_cast<size_t>(light)] = state;

    // Wenn auf OFF oder SOLID_ON geschaltet wird, führen wir es SOFORT aus,
    // damit das System extrem reaktionsschnell wirkt (keine 500ms warten).
    if (state == LightState::OFF) {
        updateHardwarePin(light, false);
    } else if (state == LightState::SOLID_ON) {
        updateHardwarePin(light, true);
    }
}

void BlinkTimer::timerLoop() {
    bool fastPhase = false; // Wechselt alle 500ms
    bool slowPhase = false; // Wechselt alle 1000ms
    int tickCount = 0;

    while (m_running) {
        // Exakt 500ms schlafen (Das blockiert nur diesen Thread, nicht das Hauptprogramm!)
        std::this_thread::sleep_for(std::chrono::milliseconds(250));

        if (!m_running) break;

        // Phasen umschalten
        fastPhase = !fastPhase;
        tickCount++;
        if (tickCount >= 2) {
            slowPhase = !slowPhase;
            tickCount = 0;
        }

        // Jetzt das Array lesen und Hardware updaten
        std::lock_guard<std::mutex> lock(m_mutex);

        for (size_t i = 0; i < static_cast<size_t>(LightID::NUM_LIGHTS); ++i) {
            LightState currentState = m_lightStates[i];
            LightID currentLight = static_cast<LightID>(i);

            if (currentState == LightState::BLINK_FAST) {
                updateHardwarePin(currentLight, fastPhase);
            }
            else if (currentState == LightState::BLINK_SLOW) {
                updateHardwarePin(currentLight, slowPhase);
            }
            // OFF und SOLID_ON werden ignoriert, da sie schon in setLightState()
            // gesetzt wurden und ihren Zustand behalten sollen.
        }
    }
}

void BlinkTimer::updateHardwarePin(LightID light, bool turnOn) {
    // Mappt die logische LightID auf die konkreten PAL Hardware-Aufrufe
    switch (light) {
        case LightID::TRAFFIC_GREEN:
            if (turnOn) pal::os::Green::data_set(); else pal::os::Green::data_clear();
            break;
        case LightID::TRAFFIC_YELLOW:
            if (turnOn) pal::os::Yellow::data_set(); else pal::os::Yellow::data_clear();
            break;
        case LightID::TRAFFIC_RED:
            if (turnOn) pal::os::Red::data_set(); else pal::os::Red::data_clear();
            break;
        case LightID::PANEL_START:
            if (turnOn) pal::oi::Start::data_set(); else pal::oi::Start::data_clear();
            break;
        case LightID::PANEL_RESET:
            if (turnOn) pal::oi::Reset::data_set(); else pal::oi::Reset::data_clear();
            break;
        case LightID::PANEL_Q1:
            if (turnOn) pal::oi::Q1::data_set(); else pal::oi::Q1::data_clear();
            break;
        case LightID::PANEL_Q2:
            if (turnOn) pal::oi::Q2::data_set(); else pal::oi::Q2::data_clear();
            break;
        default:
            break;
    }
}

} // namespace output_driver
} // namespace hal