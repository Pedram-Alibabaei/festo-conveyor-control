/*
 * Embedded Systems Engineering WiSe2025 Team 1.1
 */

#ifndef HAL_INPUT_DRIVER_GPIO_0_INTERRUPT_HPP_
#define HAL_INPUT_DRIVER_GPIO_0_INTERRUPT_HPP_

#include <hal/input_driver/interrupt.hpp>
#include <cstdint> // NEU: Für uint8_t

namespace hal
{
namespace input_driver
{

class Gpio0Interrupt : public Interrupt
{
public:
    /**************************************************************************
     * @brief Konstruktor. Setzt nur Standardwerte, startet noch keine Interrupts!
     */
    Gpio0Interrupt();

    /**************************************************************************
     * @brief Beendet Interrupt-Handling und räumt auf
     */
    ~Gpio0Interrupt() noexcept = default;



     /**************************************************************************
     * @brief Aktiviert die Hardware-Interrupts und startet den Empfangs-Thread.
     * @param coid Die Connection-ID (Kanal) des Dispatchers.
     * @param pulseCode Der 8-Bit Identifier (z.B. ipc::HAL_INPUT).
     */
    void initInterrupts(int coid);

    /**
     * @brief Deaktiviert die Hardware-Interrupts.
     */
    void stopInterrupts();

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
    // NEU: Member-Variablen für die QNX Kommunikation
    // ========================================================================
    int m_coid;
};

} // namespace input_driver
} // namespace hal

#endif // HAL_INPUT_DRIVER_GPIO_0_INTERRUPT_HPP_