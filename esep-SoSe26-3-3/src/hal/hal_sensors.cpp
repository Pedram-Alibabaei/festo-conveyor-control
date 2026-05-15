/**
* @file hal_sensors.cpp
 * @brief Implementierung der Sensor-Fassade.
 */

#include "hal_sensors.hpp"

// Für die synchrone isEstopPressed() Abfrage brauchen wir Zugriff auf den Pin
#include "pal.hpp"

namespace hal {

    HalSensors::~HalSensors() {
        // Sicherheitshalber beim Zerstören alles abschalten
        stopInterrupts();

    }

    void HalSensors::init(int coid) {
        // Weiterleiten an den GPIO Treiber
        m_gpio_interrupt.initInterrupts(coid);
        m_adc_interrupt.initAdcInterrupt(coid);
    }

    void HalSensors::stopInterrupts() {
        m_gpio_interrupt.stopInterrupts();
        m_adc_interrupt.stopAdcInterrupt();
    }

     void HalSensors::setAdcCalibrationStats(){
 //TODO

     }

    void HalSensors::setAdcCalibration(uint32_t trigger,
                                       uint32_t flatMin, uint32_t flatMax,
                                       uint32_t tallMin, uint32_t tallMax,
                                       uint32_t holeMin, uint32_t holeMax,
                                       uint32_t emptyCountLimit) {
        // Weiterleiten an den ADC Treiber
        m_adc_interrupt.setAdcCalibration(trigger, flatMin, flatMax,
                                          tallMin, tallMax, holeMin, holeMax, emptyCountLimit);
    }

    bool HalSensors::isEstopPressed() {
        // Polling-Abfrage für den Systemstart direkt über die PAL
        pal::ii::Estop::config_as_input();
        return !pal::ii::Estop::is_data_set();
    }

} // namespace hal
