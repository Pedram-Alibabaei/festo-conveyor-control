/**
* @file motor_controller.hpp
 * @brief Sichere Steuerung des Transportband-Motors.
 *
 * Verhindert illegale Zustände (z.B. Rechtslauf und Linkslauf gleichzeitig)
 * und filtert redundante Aufrufe heraus, um den GPIO nicht unnötig zu belasten.
 */

#ifndef HAL_OUTPUT_DRIVER_MOTOR_CONTROLLER_HPP_
#define HAL_OUTPUT_DRIVER_MOTOR_CONTROLLER_HPP_

namespace hal {
    namespace output_driver {

        class MotorController {
        public:
            MotorController();
            ~MotorController();

            // Verhindert das versehentliche Kopieren des Objekts (Regel für Hardware-Ressourcen)
            MotorController(const MotorController&) = delete;
            MotorController& operator=(const MotorController&) = delete;

            void driveRight();
            void driveLeft();
            void driveSlowRight();
            void driveSlowLeft();
            void stop();

        private:
            // Speichert den aktuellen Status, um Doppel-Aufrufe zu ignorieren
            enum class MotorState {
                STOPPED,
                RIGHT,
                LEFT,
                SLOW_RIGHT,
                SLOW_LEFT
            };

            MotorState m_currentState;

            /**
             * @brief Setzt alle Motor-Pins (Links, Rechts, Langsam) sofort auf 0.
             * Dient als Sicherheits-Interlock vor jedem Richtungswechsel.
             */
            void resetAllPins();
        };

    } // namespace output_driver
} // namespace hal

#endif // HAL_OUTPUT_DRIVER_MOTOR_CONTROLLER_HPP_