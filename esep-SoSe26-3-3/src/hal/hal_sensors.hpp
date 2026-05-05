/**
* @file hal_sensors.hpp
 * @brief Konkrete Implementierung der i_Sensors Schnittstelle (Facade Pattern).
 */

#ifndef HAL_SENSORS_HPP
#define HAL_SENSORS_HPP

#include "i_Sensors.hpp"

// Hier inkludieren wir die "geheimen" internen Klassen
#include "hal/input_driver/gpio_0_interrupt.hpp"
#include "hal/input_driver/adc_interrupt.hpp"

namespace hal {

    class HalSensors : public i_Sensors {
    private:
        // Die konkreten Hardware-Treiber sind hier versteckt (Kapselung)
        input_driver::Gpio0Interrupt m_gpio_interrupt;
        input_driver::AdcInterrupt   m_adc_interrupt;

    public:
        HalSensors() = default;
        ~HalSensors() override;

        // ========================================================================
        // Implementierung des i_Sensors Vertrages (override)
        // ========================================================================
        void initInterrupts(int coid, uint8_t pulseCode) override;
        void stopInterrupts() override;

        void setAdcCalibration(uint32_t trigger,
                               uint32_t flatMin, uint32_t flatMax,
                               uint32_t tallMin, uint32_t tallMax,
                               uint32_t holeMin, uint32_t holeMax,
                               uint32_t emptyCountLimit) override;

        bool isEstopPressed() override;
    };

} // namespace hal

#endif // HAL_SENSORS_HPP
