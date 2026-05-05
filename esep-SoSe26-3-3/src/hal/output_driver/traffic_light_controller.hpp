/**
* @file traffic_light_controller.hpp
 * @brief Steuerung der Ampelanlage (Grün, Gelb, Rot).
 *
 * Nutzt den zentralen BlinkTimer, um asynchrones Blinken zu ermöglichen,
 * ohne den aufrufenden Thread zu blockieren.
 */

#ifndef HAL_OUTPUT_DRIVER_TRAFFIC_LIGHT_CONTROLLER_HPP_
#define HAL_OUTPUT_DRIVER_TRAFFIC_LIGHT_CONTROLLER_HPP_

#include "blink_timer.hpp"

namespace hal {
    namespace output_driver {

        class TrafficLightController {
        public:
            /**
             * @brief Konstruktor.
             * @param timer Ein Zeiger auf den zentralen Blink-Thread des Systems.
             */
            explicit TrafficLightController(BlinkTimer* timer);
            ~TrafficLightController() = default;

            // Verhindert das Kopieren der Hardware-Ressource
            TrafficLightController(const TrafficLightController&) = delete;
            TrafficLightController& operator=(const TrafficLightController&) = delete;

            // --- Grüne Ampel ---
            void greenOn();
            void greenOff();
            void greenBlinkFast();
            void greenBlinkSlow();

            // --- Gelbe Ampel ---
            void yellowOn();
            void yellowOff();
            void yellowBlinkFast();
            void yellowBlinkSlow();

            // --- Rote Ampel ---
            void redOn();
            void redOff();
            void redBlinkFast();
            void redBlinkSlow();

        private:
            BlinkTimer* m_blinkTimer; // Zeiger auf den asynchronen Helfer
        };

    } // namespace output_driver
} // namespace hal

#endif // HAL_OUTPUT_DRIVER_TRAFFIC_LIGHT_CONTROLLER_HPP_