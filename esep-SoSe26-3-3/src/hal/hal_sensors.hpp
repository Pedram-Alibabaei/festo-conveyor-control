/**
 * @file hal_sensors.hpp
 * @brief Konkrete Implementierung der i_Sensors Schnittstelle (Facade Pattern).
 */

#ifndef HAL_SENSORS_HPP
#define HAL_SENSORS_HPP

#include "i_Sensors.hpp"

// Hier inkludieren wir die internen Low-Level Hardware-Treiber
#include "hal/input_driver/gpio_0_interrupt.hpp"
#include "hal/input_driver/adc_interrupt.hpp"
#include "hal/input_driver/profileAnalyzer.hpp"

namespace hal {

    class HalSensors : public i_Sensors {
    private:
        // Die konkreten Hardware-Treiber sind hier versteckt (Kapselung)
        input_driver::Gpio0Interrupt m_gpio_interrupt;
        input_driver::AdcInterrupt   m_adc_interrupt;
        ProfileAnalyzer              m_analyzer;

    public:
        HalSensors() = default;
        ~HalSensors() override;

        // ========================================================================
        // 1. Digitale Interrupt-Steuerung (GPIO)
        // ========================================================================
        void init(int coid) override;
        void stopInterrupts() override;

        // ========================================================================
        // 2. ADC- & PROFILANALYSATOR-ZUSTÄNDE (Aus dem Interface)
        // ========================================================================
        void setAdcIdelState() override;
        void setAdcWaitigForWpState() override;
        void setAdcCalibrationsState() override;
        void setAdcTransportPaused(bool paused) override;

        /**
         * @brief Aktualisiert die internen Schwellenwerte des ProfileAnalyzers live.
         */
        void updateCalibrationThresholds(uint16_t calib_mode, uint16_t calib_value) override;

        // ========================================================================
        // 3. Initiale Statusabfragen (Polling)
        // ========================================================================
        bool isEstopPressed() override;
    };

} // namespace hal

#endif // HAL_SENSORS_HPP
