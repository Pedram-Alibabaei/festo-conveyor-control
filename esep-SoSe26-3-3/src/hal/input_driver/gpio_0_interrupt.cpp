/*
 * Embedded Systems Engineering WiSe2025 Team 1.1
 */

#include "gpio_0_interrupt.hpp"

#include <hal/pal.hpp>
#include <hal/pal/gpio.hpp>
#include <shared/types.hpp>
#include <shared/ipc_definitions.hpp>
#include "shared/detailLog.hpp"

#include <thread>
#include <iostream>
#include <cerrno>

extern "C"
{
#include <sys/neutrino.h>
}

namespace
{
constexpr int intr_gpio_port_0 = 97;
constexpr bool kVerboseGpioPulseLog = false;
} // namespace

using namespace hal::input_driver;

// ============================================================================
// 1. Konstruktor
// ============================================================================
Gpio0Interrupt::Gpio0Interrupt() :
                Interrupt(),
                m_coid(-1)

{
    // Absichtlich leer! Der Thread und die Hardware-Registrierung
    // passieren jetzt erst kontrolliert in initInterrupts().
}

// ============================================================================
// 2. Initialisierung (Von außen durch main.cpp aufgerufen)
// ============================================================================
void Gpio0Interrupt::initInterrupts(int coid)
{
    m_coid = coid;
    DETAIL_LOG( "[GPIO] Interrupt-Thread startet mit Dispatcher-COID=" << m_coid);

    if (m_coid == -1) {
        std::cerr << "[GPIO] FEHLER: Ungueltige COID – Taster-Events kommen nicht an!" << std::endl;
        return;
    }
    m_interrupt_id = setup_interrupt(pulse_intr, intr_gpio_port_0);

    // 2. Hardware-Pins konfigurieren (Deine Methode)
    configure_interrupt();

    // 3. Thread starten
    m_running = true;
    m_receiving_thread = std::thread([this]()
    {
        receive_loop();
    });
}

// ============================================================================
// 3. Deaktivierung
// ============================================================================
void Gpio0Interrupt::stopInterrupts()
{
    m_running = false;
    // Interrupt auf OS-Ebene maskieren und abhängen
    InterruptMask(intr_gpio_port_0, m_interrupt_id);
    InterruptDetach(m_interrupt_id);

    // Thread lösen (In einer finalen Version könnte man hier noch einen
    // Dummy-Pulse an den Thread senden, um MsgReceive zu beenden und zu joinen).
    if (m_receiving_thread.joinable()) {
        m_receiving_thread.detach();
    }
}

// ============================================================================
// 4. Pin-Konfiguration (Bleibt unverändert)
// ============================================================================
void Gpio0Interrupt::configure_interrupt() const noexcept
{
    {
        using namespace pal::ii;
        Estop::config_as_input();
        Estop::enable_interrupt();
        Estop::config_interrupt_rising();
        Estop::config_interrupt_falling();
        Estop::enable_debounce();

        Reset::config_as_input();
        Reset::enable_interrupt();
        Reset::config_interrupt_rising();
        Reset::config_interrupt_falling();
        Reset::enable_debounce();

        Start::config_as_input();
        Start::enable_interrupt();
        Start::config_interrupt_rising();
        Start::config_interrupt_falling();
        Start::enable_debounce();

        Stop::config_as_input();
        Stop::enable_interrupt();
        Stop::config_interrupt_rising();
        Stop::config_interrupt_falling();
        Stop::enable_debounce();

        Stop::debounce_time_ms(50); // gilt für alle GPIO 0 Pins
    }

    {
        using namespace pal::ip;
        Chute::config_as_input();
        Chute::enable_interrupt();
        Chute::config_interrupt_rising();
        Chute::config_interrupt_falling();

        Ejector::config_as_input();
        Ejector::enable_interrupt();
        Ejector::config_interrupt_rising();
        Ejector::config_interrupt_falling();

        End::config_as_input();
        End::enable_interrupt();
        End::config_interrupt_rising();
        End::config_interrupt_falling();

        Entry::config_as_input();
        Entry::enable_interrupt();
        Entry::config_interrupt_rising();
        Entry::config_interrupt_falling();

        Metal::config_as_input();
        Metal::enable_interrupt();
        Metal::config_interrupt_rising();
        Metal::config_interrupt_falling();
    }

    {
        using namespace pal::is;
        Ejector::config_as_input();
        Ejector::enable_interrupt();
        Ejector::config_interrupt_rising();
        Ejector::config_interrupt_falling();

       // Height::config_as_input();
       // Height::enable_interrupt();
       // Height::config_interrupt_rising();
       // Height::config_interrupt_falling();
       // Height::enable_debounce();
    }
}

// ============================================================================
// 5. Interrupt-Behandlung (Der Puls-Sender)
// ============================================================================
void Gpio0Interrupt::handle_interrupt() const noexcept
{
    using namespace pal;

    // 1. Interrupt-Status des gesamten GPIO_0 Ports einlesen
    UInt32 status = ii::Start::read_interrupt_status();

    // 2. Alle Interrupts auf diesem Port sofort quittieren (löschen)
    ii::Estop::clear_interrupt_status();
    ii::Start::clear_interrupt_status();
    ii::Reset::clear_interrupt_status();
    ii::Stop::clear_interrupt_status();
    ip::Chute::clear_interrupt_status();
    ip::Ejector::clear_interrupt_status();
    ip::End::clear_interrupt_status();
    ip::Entry::clear_interrupt_status();
    ip::Metal::clear_interrupt_status();
    is::Ejector::clear_interrupt_status();
    //is::Height::clear_interrupt_status();

    // Struktur für die Pin-Auswertung
    struct PinInfo
    {
        uint16_t sensor_id;  // ID des Sensors aus ipc_definitions.hpp
        bool is_button;      // true = Taster (Pressed/Released), false = Lichtschranke (Blocked/Cleared)
        bool active;         // Aktueller physikalischer Zustand des Pins
        UInt32 mask;         // Bitmaske für diesen Pin im GPIO-Register
    };

    // Mapping aller Pins auf dem GPIO_0 Port
    PinInfo pins[] = {
        { ipc::BTN_ESTOP,     true,  ii::Estop::is_data_set(),   ii::pin_mask_estop },
        { ipc::BTN_RESET,     true,  ii::Reset::is_data_set(),   ii::pin_mask_reset },
        { ipc::BTN_START,     true,  ii::Start::is_data_set(),   ii::pin_mask_start },
        { ipc::BTN_STOP,      true,  ii::Stop::is_data_set(),    ii::pin_mask_stop },
        { ipc::LS_CHUTE,      false, ip::Chute::is_data_set(),   ip::pin_mask_chute },
        { ipc::LS_SWITCH,     false, ip::Ejector::is_data_set(), ip::pin_mask_ejector },
        { ipc::LS_ENTRY,      false, ip::Entry::is_data_set(),   ip::pin_mask_entry },
        { ipc::LS_END,        false, ip::End::is_data_set(),     ip::pin_mask_end },
        { ipc::SENSOR_METAL,  false, ip::Metal::is_data_set(),   ip::pin_mask_metal },
        { ipc::EJECTOR,    false, is::Ejector::is_data_set(), is::pin_mask_ejector }
        //{ ipc::LS_HEIGHT,     false, is::Height::is_data_set(),  is::pin_mask_height }
    };

    // 3. Alle Pins durchgehen und prüfen, welcher Pin den Interrupt ausgelöst hat
    for (const auto &pin : pins)
    {
        if (status & pin.mask)
        {
            // Bestimmung des genauen Zustands basierend auf deinem State-Enum
            uint16_t state_value;

            if (pin.is_button) {
                // Taster-Logik
                state_value = pin.active ? ipc::State::PRESSED : ipc::State::RELEASED;
            } else if (pin.sensor_id == ipc::SENSOR_METAL) {
                // Induktivsensor: Pin HIGH = Metall erkannt -> BLOCKED (wie LS unterbrochen)
                state_value = pin.active ? ipc::State::BLOCKED : ipc::State::CLEARED;
            } else {
                // Lichtschranken: Pin HIGH = frei -> CLEARED
                state_value = pin.active ? ipc::State::CLEARED : ipc::State::BLOCKED;
            }

            // Spezialfall für E-Stop: Wenn der E-Stop-Pin auslöst (egal ob gedrückt oder gelöst),
            // bekommt er den eigenen, hochpriorisierten Pulse-Code HAL_ESTOP.
            int8_t pulse_code = (pin.sensor_id == ipc::BTN_ESTOP) ? ipc::HAL_ESTOP : ipc::HAL_INPUT;

            // 4. QNX Payload packen (Sensor ID + neuer Zustand)
            uint32_t payload = ipc::encodeHalPayload(pin.sensor_id, state_value);

            // 5. Puls absolut blockierungsfrei an den Dispatcher senden
            if (m_coid != -1) {
                int rc = MsgSendPulse(m_coid, 10, pulse_code, payload);
                if (rc != 0) {
                    std::cerr << "[GPIO] MsgSendPulse fehlgeschlagen fuer Sensor "
                              << pin.sensor_id << " errno=" << errno << std::endl;
                } else if (kVerboseGpioPulseLog && !pin.is_button) {
                    const char* stateLabel =
                        (state_value == ipc::State::BLOCKED) ? "BLOCKED" : "CLEARED";
                    DETAIL_LOG( "[GPIO] Sensor id=" << pin.sensor_id
                              << " -> " << stateLabel);
                } else if (kVerboseGpioPulseLog && pin.is_button) {
                    DETAIL_LOG( "[GPIO] Taster id=" << pin.sensor_id
                              << " -> " << (pin.active ? "PRESSED" : "RELEASED"));
                }
            }
        }
    }

    // 6. Dem Betriebssystem erlauben, neue Interrupts für diesen Port durchzulassen
    InterruptUnmask(intr_gpio_port_0, m_interrupt_id);
}
