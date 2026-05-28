/**
 * @file hal_sensors.cpp
 * @brief Implementierung der Sensor-Fassade (Facade Pattern).
 */

#include "hal_sensors.hpp"

// Für die synchrone isEstopPressed() Abfrage brauchen wir Zugriff auf den Pin
#include "pal.hpp"

namespace hal {

    HalSensors::~HalSensors() {
        // Sicherheitshalber beim Zerstören alles abschalten
        stopInterrupts();
    }

    // ========================================================================
    // 1. Digitale Interrupt-Steuerung (GPIO)
    // ========================================================================

    void HalSensors::init(int coid) {
        // Weiterleiten an den GPIO und ADC Treiber (Übergabe des Analyzers per Zeiger)
        m_gpio_interrupt.initInterrupts(coid);
        m_analyzer.setConnectionId(coid);
        m_adc_interrupt.initAdcInterrupt(&m_analyzer);

    }

    void HalSensors::stopInterrupts() {
        m_gpio_interrupt.stopInterrupts();
        m_adc_interrupt.stopAdcInterrupt();
    }

    // ========================================================================
    // 2. LOGISCHE ADC- & PROFILANALYSATOR-ZUSTÄNDE (Direkt auf m_analyzer)
    // ========================================================================

    void HalSensors::setAdcIdelState() {
        // Schaltet den Profilanalysator direkt in den Idle-Zustand (Band leer)
        m_analyzer.setAdcIdelState();
    }

    void HalSensors::setAdcWaitigForWpState() {
        // Versetzt den Profilanalysator direkt in den Zustand "Warte auf Werkstück"
        m_analyzer.setAdcWaitigForWpState();
    }

    void HalSensors::setAdcCalibrationsState() {
        // Versetzt den Profilanalysator direkt in den Kalibrierungsmodus
        m_analyzer.setAdcCalibrationsState();
    }

    void HalSensors::updateCalibrationThresholds(uint16_t calib_mode, uint16_t calib_value) {
        // Übergibt die geladenen Schwellenwerte direkt an die lokale m_analyzer-Instanz
        m_analyzer.updateCalibrationThresholds(calib_mode, calib_value);
    }

    // ========================================================================
    // 3. Initiale Statusabfragen (Polling)
    // ========================================================================

    bool HalSensors::isEstopPressed() {
        // Polling-Abfrage für den Systemstart direkt über die PAL
        pal::ii::Estop::config_as_input();
        return !pal::ii::Estop::is_data_set();
    }

} // namespace hal