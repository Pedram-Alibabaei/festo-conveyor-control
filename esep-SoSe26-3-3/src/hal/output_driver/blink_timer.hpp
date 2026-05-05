/**
* @file blink_timer.hpp
 * @brief Asynchroner Hintergrund-Thread für alle Signalleuchten.
 *
 * Verhindert blockierende sleep()-Aufrufe in der Hauptlogik und schützt
 * die GPIO-Register vor Race-Conditions zwischen verschiedenen Threads.
 */

#ifndef HAL_OUTPUT_DRIVER_BLINK_TIMER_HPP_
#define HAL_OUTPUT_DRIVER_BLINK_TIMER_HPP_

#include <thread>
#include <mutex>
#include <atomic>
#include <array>

namespace hal {
    namespace output_driver {

        // Alle Lampen/LEDs, die das System kennt
        enum class LightID {
            TRAFFIC_GREEN = 0,
            TRAFFIC_YELLOW,
            TRAFFIC_RED,
            PANEL_START,
            PANEL_RESET,
            PANEL_Q1,
            PANEL_Q2,
            NUM_LIGHTS // Automatischer Zähler für die Array-Größe
        };

        // Die möglichen Zustände einer Lampe
        enum class LightState {
            OFF,
            SOLID_ON,
            BLINK_FAST, // 1 Hz (500ms an, 500ms aus)
            BLINK_SLOW  // 0.5 Hz (1000ms an, 1000ms aus)
        };

        class BlinkTimer {
        public:
            BlinkTimer();
            ~BlinkTimer();

            // Verhindert Kopieren
            BlinkTimer(const BlinkTimer&) = delete;
            BlinkTimer& operator=(const BlinkTimer&) = delete;

            /**
             * @brief Setzt den Zustand einer bestimmten Lampe.
             * Kann gefahrlos von jedem Thread (z.B. Dispatcher/Logik) aufgerufen werden.
             */
            void setLightState(LightID light, LightState state);

        private:
            std::array<LightState, static_cast<size_t>(LightID::NUM_LIGHTS)> m_lightStates;

            std::mutex m_mutex;            // Schützt das Array vor gleichzeitigem Zugriff
            std::atomic<bool> m_running;   // Steuert die Endlosschleife des Threads
            std::thread m_timerThread;

            // Die Hintergrund-Schleife
            void timerLoop();

            // Führt den tatsächlichen Hardware-Zugriff (PAL) aus
            void updateHardwarePin(LightID light, bool turnOn);
        };

    } // namespace output_driver
} // namespace hal

#endif // HAL_OUTPUT_DRIVER_BLINK_TIMER_HPP_