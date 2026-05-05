/*
 * Embedded Systems Engineering WiSe2025 Team 1.1
 */

#ifndef HAL_INPUT_DRIVER_INTERRUPT_HPP_
#define HAL_INPUT_DRIVER_INTERRUPT_HPP_

#include <shared/types.hpp>

#include <thread>

extern "C"
{
#include <sys/neutrino.h>
}

namespace hal
{
namespace input_driver
{

class Interrupt
{
public:
    /**************************************************************************
     * @brief Konstruktor started einen Thread für Interrupt-Handling
     *
     * Die Interrupts werden von der erbenden Klasse ausgewählt.
     *
     * @param intr Kennnummer
     */
    Interrupt();

    /**************************************************************************
     * @brief Beendet Interrupt-Handling und räumt auf
     */
    virtual
    ~Interrupt() noexcept;

protected:
    /**************************************************************************
     * @brief Konfiguriert Interrupt Handler
     *
     * @param pulse Puls ID
     * @param intr Kennnummer
     * @return Interrupt ID
     */
    int
    setup_interrupt(SInt8 pulse, int intr) const;

    /**************************************************************************
     * @brief Legt fest, auf welche Interrupts reagiert wird
     */
    virtual void
    configure_interrupt() const noexcept = 0;

    /**************************************************************************
     * @brief Interrupt Service Routine
     */
    virtual void
    handle_interrupt() const noexcept = 0;

private:
    /**************************************************************************
     * @brief Fordert Berechtigungen für Interrupts beim Betriebssystem ein
     */
    void
    setup_io_permissions() const noexcept;

    /**************************************************************************
     * @brief Konfiguriert einen Betriebssystem-Kanal zur Kommunikation
     */
    void
    setup_channel();

    /**************************************************************************
     * @brief Reagiert auf Interrupts
     */
    void
    receive_loop();

    /**************************************************************************
     * @brief Resourcen beim Betriebssystem wieder freigeben
     */
    void
    cleanup() const noexcept;

protected:
    enum Pulse
        : SInt8
        {
            pulse_stop_thread = _PULSE_CODE_MINAVAIL + 1,
        pulse_intr,
    };

    bool m_running;
    int m_interrupt_id;
    std::thread m_receiving_thread;

private:
    int m_channel_id;
    int m_connection_id;
};

} // namespace input_driver
} // namespace hal

#endif // HAL_INPUT_DRIVER_INTERRUPT_HPP_
