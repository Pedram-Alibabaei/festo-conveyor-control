/*
 * Embedded Systems Engineering WiSe2025 Team 1.1
 */

#ifndef HAL_INPUT_DRIVER_ADC_INTERRUPT_HPP_
#define HAL_INPUT_DRIVER_ADC_INTERRUPT_HPP_

#include <hal/input_driver/interrupt.hpp>
#include <cstdint>

// Vorwärtsdeklaration, um zyklische Includes zu vermeiden.
// Der echte Header wird erst in der .cpp inkludiert.
class ProfileAnalyzer;

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
     * @brief Beendet Interrupt-Handling und räumt auf.
     */
    ~AdcInterrupt() noexcept = default;

    /**************************************************************************
     * @brief Startet den asynchronen ADC-Thread/Interrupt für die Höhenmessung.
     * @param coid Die Connection-ID (Kanal) des Dispatchers.
     * @param analyzer Zeiger auf den zentralen ProfileAnalyzer, der in HalSensors lebt.
     */
    void initAdcInterrupt(ProfileAnalyzer* analyzer);

    /**************************************************************************
     * @brief Stoppt den asynchronen ADC-Mess-Thread und maskiert die Hardware.
     */
    void stopAdcInterrupt();

private:
    /**************************************************************************
     * @brief Legt fest, auf welche Interrupts reagiert wird.
     */
    void configure_interrupt() const noexcept final override;

    /**************************************************************************
     * @brief Interrupt Service Routine (ISR) - Liest den ADC-Wert und leitet ihn weiter.
     */
    void handle_interrupt() const noexcept final override;

    // ========================================================================
    // Member-Variablen für QNX Kommunikation und Hardware-Kopplung
    // ========================================================================
    ProfileAnalyzer* m_analyzer;        // Zeiger auf das Analyse-Objekt der Fassade
};

} // namespace input_driver
} // namespace hal

#endif // HAL_INPUT_DRIVER_ADC_INTERRUPT_HPP_