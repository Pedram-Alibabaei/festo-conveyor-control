/**
 * @file ejector_controller.cpp
 * @brief Implementierung der Ejector-Steuerung mit Auto-Detect.
 */

#include "ejector_controller.hpp"
#include <hal/pal.hpp>
#include <iostream>

namespace hal {
namespace output_driver {

    EjectorController::EjectorController() : m_isSortingOut(false) {

        // ========================================================================
        // 1. AUTOMATISCHE HARDWARE-ERKENNUNG
        // ========================================================================

        // NEU: Zuerst den Pin als Eingang konfigurieren!
        // Das zwingt die PAL, den Speicherbereich (Device Mapping) zu initialisieren.
        pal::is::Ejector::config_as_input();

        // JETZT erst abfragen (Die Assertion wird nun erfolgreich passieren!)
        if (pal::is::Ejector::is_data_set()) {
            m_type = SorterHardwareType::AUSWERFER;
            std::cout << "[EjectorController] Auto-Detect: Sensor an IS_EJECTOR (High) -> AUSWERFER (Anlage 2)" << std::endl;
        } else {
            m_type = SorterHardwareType::WEICHE;
            std::cout << "[EjectorController] Auto-Detect: Kein Sensor (Low) -> WEICHE (Anlage 1)" << std::endl;
        }

        // ========================================================================
        // 2. HARDWARE KONFIGURIEREN
        // ========================================================================
        // Aktor-Pin zwingend als Ausgang konfigurieren
        pal::op::Ejector::config_as_output();

        // In den sicheren Ausgangszustand fahren
        letItemPass();
    }

EjectorController::~EjectorController() {
    letItemPass(); // Beim Beenden Weiche zu / Zylinder einfahren
}

void EjectorController::sortItemOut() {
    if (m_isSortingOut) return;

    if (m_type == SorterHardwareType::WEICHE) {
        pal::op::Ejector::data_set();
    } else {
        pal::op::Ejector::data_clear();
    }

    m_isSortingOut = true;
}

void EjectorController::letItemPass() {
    if (!m_isSortingOut) return;

    if (m_type == SorterHardwareType::WEICHE) {
        pal::op::Ejector::data_clear();
    } else {
        pal::op::Ejector::data_set();
    }

    m_isSortingOut = false;
}

} // namespace output_driver
} // namespace hal