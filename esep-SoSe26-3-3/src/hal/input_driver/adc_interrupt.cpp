/*
 * Embedded Systems Engineering WiSe2025 Team 1.1
 */

#include "adc_interrupt.hpp"

#include <hal/pal.hpp>
#include <hal/pal/adc.hpp>
#include <shared/types.hpp>

// Wichtig für ipc::encodeHalPayload und deine neuen Puls-Codes
#include <shared/ipc_definitions.hpp>
#include "profileAnalyzer.hpp" // Hier den echten Header für den Zeiger inkludieren

#include <iostream>
#include <thread>

extern "C"
{
#include <sys/neutrino.h>
}

namespace
{
constexpr int intr_adc = 16;
} // namespace

using namespace hal::input_driver;

// ============================================================================
// 1. Konstruktor
// ============================================================================
AdcInterrupt::AdcInterrupt() :
                Interrupt(),
                m_analyzer(nullptr) // Initialisiert als Nullpointer
{
    // Absichtlich leer! Der Thread und die Hardware-Registrierung
    // passieren erst kontrolliert in initAdcInterrupt().
}

// ============================================================================
// 2. Initialisierung (Von außen durch HalSensors aufgerufen)
// ============================================================================
void AdcInterrupt::initAdcInterrupt( ProfileAnalyzer* analyzer)
{
    m_analyzer = analyzer; // Sichert den Zeiger auf das Objekt in der Fassade

    // 1. Hardware-Interrupt beim OS registrieren
    m_interrupt_id = setup_interrupt(pulse_intr, intr_adc);

    // 2. Hardware konfigurieren
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
void AdcInterrupt::stopAdcInterrupt()
{
    m_running = false;

    // Interrupt auf OS-Ebene maskieren und abhängen
    InterruptMask(intr_adc, m_interrupt_id);
    InterruptDetach(m_interrupt_id);

    // Thread lösen
    if (m_receiving_thread.joinable()) {
        m_receiving_thread.detach();
    }
}

// ============================================================================
// 4. Hardware Konfiguration
// ============================================================================
void AdcInterrupt::configure_interrupt() const noexcept
{
    pal::ip::Height::enable_interrupt();
}

// ============================================================================
// 5. Interrupt-Behandlung (Der Puls-Sender)
// ============================================================================
void AdcInterrupt::handle_interrupt() const noexcept
{
    using pal::ip::Height;

    // 1. Rohwert aus dem Hardware-Register lesen
    Height::clear_interrupt_event();
    InterruptUnmask(intr_adc, m_interrupt_id);
    uint32_t current_adc_value = Height::analog_value();

    // 2. Den Wert direkt an den Analyzer der HalSensors-Klasse hochreichen
    if (m_analyzer != nullptr) {
        m_analyzer->processSample(current_adc_value);
    }
}