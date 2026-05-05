/**
* @file ejector_controller.hpp
 * @brief Steuerung der Sortiereinheit (Weiche oder Auswerfer).
 *
 * Kapselt die Hardware-Unterschiede zwischen den beiden Anlagentypen
 * und erkennt beim Start automatisch, welche Hardware vorliegt.
 */

#ifndef HAL_OUTPUT_DRIVER_EJECTOR_CONTROLLER_HPP_
#define HAL_OUTPUT_DRIVER_EJECTOR_CONTROLLER_HPP_

namespace hal {
    namespace output_driver {

        // Beschreibt die physikalische Beschaffenheit der Anlage
        enum class SorterHardwareType {
            WEICHE,     // Anlage 1: Weiche (z.B. 1 = Offen/Aussortieren, 0 = Geschlossen/Durchlassen)
            AUSWERFER   // Anlage 2: Auswerfer (Zylinder) (Logik physikalisch invertiert)
        };

        class EjectorController {
        public:
            /**
             * @brief Konstruktor. Erkennt automatisch anhand des Zylinder-Sensors,
             * ob eine Weiche oder ein Auswerfer angeschlossen ist.
             */
            EjectorController();
            ~EjectorController();

            // Verhindert Kopieren
            EjectorController(const EjectorController&) = delete;
            EjectorController& operator=(const EjectorController&) = delete;

            /** @brief Sortiert das Werkstück aus (Weiche auf / Zylinder ausfahren) */
            void sortItemOut();

            /** @brief Lässt das Werkstück passieren (Weiche zu / Zylinder einfahren) */
            void letItemPass();

        private:
            SorterHardwareType m_type;
            bool m_isSortingOut; // Speichert den Zustand, um unnötige GPIO-Aufrufe zu sparen
        };

    } // namespace output_driver
} // namespace hal

#endif // HAL_OUTPUT_DRIVER_EJECTOR_CONTROLLER_HPP_