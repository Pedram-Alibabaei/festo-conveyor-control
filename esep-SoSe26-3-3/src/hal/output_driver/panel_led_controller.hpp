/**
* @file panel_led_controller.hpp
 * @brief Steuerung der LEDs auf dem Bedienpanel (Start, Reset, Q1, Q2).
 *
 * Nutzt den zentralen BlinkTimer, um asynchrones Blinken zu ermöglichen.
 */

#ifndef HAL_OUTPUT_DRIVER_PANEL_LED_CONTROLLER_HPP_
#define HAL_OUTPUT_DRIVER_PANEL_LED_CONTROLLER_HPP_

#include "blink_timer.hpp"

namespace hal {
    namespace output_driver {

        class PanelLedController {
        public:
            /**
             * @brief Konstruktor.
             * @param timer Ein Zeiger auf den zentralen Blink-Thread des Systems.
             */
            explicit PanelLedController(BlinkTimer* timer);
            ~PanelLedController() = default;

            // Verhindert das versehentliche Kopieren
            PanelLedController(const PanelLedController&) = delete;
            PanelLedController& operator=(const PanelLedController&) = delete;

            // --- Start-Tasten-LED ---
            void turnLEDStartOn();
            void turnLEDStartOff();
            void turnLEDStartBlinkFast();
            void turnLEDStartBlinkSlow();

            // --- Reset-Tasten-LED ---
            void turnLEDResetOn();
            void turnLEDResetOff();
            void turnLEDResetBlinkFast();
            void turnLEDResetBlinkSlow();

            // --- Signalleuchte Q1 ---
            void turnLEDQ1On();
            void turnLEDQ1Off();
            void turnLEDQ1BlinkFast();
            void turnLEDQ1BlinkSlow();

            // --- Signalleuchte Q2 ---
            void turnLEDQ2On();
            void turnLEDQ2Off();
            void turnLEDQ2BlinkFast();
            void turnLEDQ2BlinkSlow();

        private:
            BlinkTimer* m_blinkTimer; // Zeiger auf den asynchronen Helfer
        };

    } // namespace output_driver
} // namespace hal

#endif // HAL_OUTPUT_DRIVER_PANEL_LED_CONTROLLER_HPP_