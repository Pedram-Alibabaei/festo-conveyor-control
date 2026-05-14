/*
 * Embedded Systems Engineering WiSe2025 Team 1.1
 */

#ifndef HAL_INPUT_DRIVER_ADC_INTERRUPT_HPP_
#define HAL_INPUT_DRIVER_ADC_INTERRUPT_HPP_

#include <hal/input_driver/interrupt.hpp>
#include "profileAnalyzer.hpp"
#include <shared/ipc_definitions.hpp>
#include <cstdint> // NEU: Wichtig für uint8_t und uint32_t

namespace hal
{
namespace input_driver
{

class AdcInterrupt : public Interrupt
{
public:
    /**************************************************************************
     * @brief Konstruktor. Setzt nur Standardwerte, startet noch keine Interrupts!
     */
    AdcInterrupt();

    /**************************************************************************
     * @brief Beendet Interrupt-Handling und räumt auf
     */
    ~AdcInterrupt() noexcept = default;

     /**************************************************************************
     * @brief Startet den asynchronen ADC-Thread/Interrupt für die Höhenmessung.
     * @param coid Die Connection-ID (Kanal) des Dispatchers.
     * @param pulseCode Der 8-Bit Identifier (z.B. ipc::HAL_INPUT).
     */
    void initAdcInterrupt(int coid, uint8_t pulseCode);

     /**************************************************************************
     * @brief Überträgt die Kalibrierungsdaten an den internen ProfileAnalyzer.
     * Muss zwingend vor initAdcInterrupt() aufgerufen werden!
     */
    void setAdcCalibration(uint32_t trigger,
                           uint32_t flatMin, uint32_t flatMax,
                           uint32_t tallMin, uint32_t tallMax,
                           uint32_t holeMin, uint32_t holeMax,
                           uint32_t emptyCountLimit);

     /**************************************************************************
     * @brief Stoppt den asynchronen ADC-Mess-Thread.
     */
    void stopAdcInterrupt();


private:
    /**************************************************************************
     * @brief Legt fest, auf welche Interrupts reagiert wird
     */
    void
    configure_interrupt() const noexcept final override;

    /**************************************************************************
     * @brief Interrupt Service Routine
     */
    void
    handle_interrupt() const noexcept final override;

    // ========================================================================
    // Member-Variablen für QNX Kommunikation und Auswertung
    // ========================================================================
    int m_coid;                         // Die Ziel-Adresse für unsere Pulses
    uint8_t m_pulseCode;                // Der 8-Bit QNX Pulse-Code (z.B. HAL_INPUT)
    mutable ProfileAnalyzer m_analyzer; // 'mutable', damit es in der const Methode nutzbar bleibt
};

} // namespace input_driver
} // namespace hal

#endif // HAL_INPUT_DRIVER_ADC_INTERRUPT_HPP_