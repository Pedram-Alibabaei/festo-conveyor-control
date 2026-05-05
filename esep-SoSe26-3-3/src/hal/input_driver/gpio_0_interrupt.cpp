/*
 * Embedded Systems Engineering WiSe2025 Team 1.1
 */

#include "gpio_0_interrupt.hpp"

#include <hal/pal.hpp>
#include <hal/pal/gpio.hpp>
#include <shared/types.hpp>

// NEU: Wichtig für die Definitionen von ipc::encodeHalPayload etc.
#include <shared/ipc_definitions.hpp>

#include <thread>
#include <iostream>

extern "C"
{
#include <sys/neutrino.h>
}

namespace
{
constexpr int intr_gpio_port_0 = 97;
} // namespace

using namespace hal::input_driver;

// ============================================================================
// 1. Konstruktor
// ============================================================================
Gpio0Interrupt::Gpio0Interrupt() :
                Interrupt(),
                m_coid(-1),
                m_pulseCode(0)
{
    // Absichtlich leer! Der Thread und die Hardware-Registrierung
    // passieren jetzt erst kontrolliert in initInterrupts().
}

// ============================================================================
// 2. Initialisierung (Von außen durch main.cpp aufgerufen)
// ============================================================================
void Gpio0Interrupt::initInterrupts(int coid, uint8_t pulseCode)
{
    m_coid = coid;
    m_pulseCode = pulseCode;

    // 1. Hardware-Interrupt beim OS registrieren
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

        Height::config_as_input();
        Height::enable_interrupt();
        Height::config_interrupt_rising();
        Height::config_interrupt_falling();
        Height::enable_debounce();
    }
}

// ============================================================================
// 5. Interrupt-Behandlung (Der Puls-Sender)
// ============================================================================
void Gpio0Interrupt::handle_interrupt() const noexcept
{
    using namespace pal;

    // HINWEIS: Prüft nochmal, ob 'ii::Start::read_interrupt_status()' wirklich
    // den Status des gesamten GPIO_0 Ports (IRQSTATUS_0) zurückgibt. Wenn hier nur
    // das Bit für den Start-Knopf liegt, funktioniert die Maskierung unten nicht für alle!
    UInt32 status = ii::Start::read_interrupt_status();

    // clear all interrupts
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
    is::Height::clear_interrupt_status();

    // Wir definieren das Struct um eine 'sensor_id' aus unseren IPC-Definitionen
    struct PinInfo
    {
        uint16_t sensor_id; // NEU: Damit der Dispatcher weiß, wer gefeuert hat
        const char* name;
        bool active;
        UInt32 mask;
    };

    // ACHTUNG: Bitte passe die ipc:: Werte an die Namen an,
    // die du in deiner ipc_definitions.hpp vergeben hast!
    PinInfo pins[] = {
        { ipc::BTN_ESTOP,    "ii-estop",   ii::Estop::is_data_set(),   ii::pin_mask_estop },
        { ipc::BTN_RESET,    "ii-reset",   ii::Reset::is_data_set(),   ii::pin_mask_reset },
        { ipc::BTN_START,    "ii-start",   ii::Start::is_data_set(),   ii::pin_mask_start },
        { ipc::BTN_STOP,     "ii-stop",    ii::Stop::is_data_set(),    ii::pin_mask_stop },
        { ipc::LS_CHUTE,     "ip-chute",   ip::Chute::is_data_set(),   ip::pin_mask_chute },
        { ipc::LS_EJECTOR,   "ip-ejector", ip::Ejector::is_data_set(), ip::pin_mask_ejector },
        { ipc::LS_ENTRY,     "ip-entry",   ip::Entry::is_data_set(),   ip::pin_mask_entry },
        { ipc::LS_END,       "ip-end",     ip::End::is_data_set(),     ip::pin_mask_end },
        { ipc::SENSOR_METAL, "ip-metal",   ip::Metal::is_data_set(),   ip::pin_mask_metal },
        // is-ejector und is-height sind vermutlich Aktoren-Rückmeldungen oder doppelt,
        // passe hier die IDs ebenfalls an deine Definitionen an.
        { ipc::IS_EJECTOR,   "is-ejector", is::Ejector::is_data_set(), is::pin_mask_ejector },
        { ipc::IS_HEIGHT,    "is-height",  is::Height::is_data_set(),  is::pin_mask_height },
    };

    for (const auto &pin : pins)
    {
        // Hat sich dieser Pin verändert?
        if (status & pin.mask)
        {
            // Optional: Fürs Debugging in der Konsole lassen
            std::cout << "[GPIO HAL] Flanke an " << pin.name << " -> Neu: " << pin.active << "\n";

            // 1. QNX Payload bauen (Sensor ID + Zustand 1 oder 0)
            uint32_t payload = ipc::encodeHalPayload(pin.sensor_id, pin.active ? 1 : 0);

            // 2. Pulse an den Dispatcher feuern!
            if (m_coid != -1) {
                MsgSendPulse(m_coid, 10, m_pulseCode, payload);
            }
        }
    }

    // OS erlauben, den nächsten Interrupt für diesen Port zu werfen
    InterruptUnmask(intr_gpio_port_0, m_interrupt_id);
}