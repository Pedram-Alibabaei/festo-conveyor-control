/*
 * Embedded Systems Engineering WiSe2025 Team 1.1
 */

#include "adc_interrupt.hpp"

#include <hal/pal.hpp>
#include <hal/pal/adc.hpp>
#include <shared/types.hpp>

// NEU: Wichtig für ipc::encodeHalPayload und ipc::HEIGHT_ADC
#include <shared/ipc_definitions.hpp>

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
                m_coid(-1),
                m_pulseCode(0)
{
    // Absichtlich leer! Der Thread und die Hardware-Registrierung
    // passieren jetzt erst kontrolliert in initAdcInterrupt().
}

// ============================================================================
// 2. Initialisierung (Von außen durch main.cpp aufgerufen)
// ============================================================================
void AdcInterrupt::initAdcInterrupt(int coid, uint8_t pulseCode)
{
    m_coid = coid;
    m_pulseCode = pulseCode;

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
// 3. Kalibrierung (Muss vor initAdcInterrupt gerufen werden)
// ============================================================================
void AdcInterrupt::setAdcCalibration(uint32_t trigger,
                                     uint32_t flatMin, uint32_t flatMax,
                                     uint32_t tallMin, uint32_t tallMax,
                                     uint32_t holeMin, uint32_t holeMax,
                                     uint32_t emptyCountLimit)
{
    // Reicht die Werte einfach an das interne "Gehirn" (ProfileAnalyzer) weiter
    m_analyzer.setCalibration(trigger, flatMin, flatMax, tallMin, tallMax, holeMin, holeMax, emptyCountLimit);
}

// ============================================================================
// 4. Deaktivierung
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
// 5. Hardware Konfiguration
// ============================================================================
void AdcInterrupt::configure_interrupt() const noexcept
{
    pal::ip::Height::enable_interrupt();
}

// ============================================================================
// 6. Interrupt-Behandlung (Der Puls-Sender)
// ============================================================================
void AdcInterrupt::handle_interrupt() const noexcept
{
    using pal::ip::Height;

    // 1. Rohwert aus dem Hardware-Register lesen
    UInt32 current_adc_value = Height::analog_value();

    // 2. Den Wert in den Analyzer füttern
    m_analyzer.processSample(current_adc_value);

    // 3. Den Analyzer fragen: "Ist das Werkstück komplett vorbeigefahren?"
    if (m_analyzer.isMeasurementComplete()) {

        // 4. JA! Jetzt holen wir uns das finale Ergebnis ab:
        // (Achte darauf, dass WPType hier so geschrieben wird, wie in deiner types.hpp)
        WpType detected_type = m_analyzer.determineFinalType();

        if (detected_type != WpType::UNKNOWN) {
            std::cout << "[ADC HAL] Profilanalyse beendet! Erkannt: "
                      << static_cast<int>(detected_type) << std::endl;

            // ================================================================
            // NEU: QNX Pulse an den Dispatcher senden!
            // ================================================================
            if (m_coid != -1) {
                // Wir packen die ID (HEIGHT_ADC) und den Typ (z.B. TALL) in ein 32-Bit Paket
                uint32_t payload = ipc::encodeHalPayload(ipc::HEIGHT_ADC, static_cast<uint16_t>(detected_type));

                // Pulse abfeuern (Priorität 10, Code z.B. HAL_INPUT, Daten = payload)
                MsgSendPulse(m_coid, 10, m_pulseCode, payload);
            }
        }

        // 5. WICHTIG: Analyzer für das nächste Werkstück zurücksetzen!
        m_analyzer.reset();
    }

    // 6. Hardware-Interrupt löschen und für die nächste Messung scharfschalten
    Height::clear_interrupt_event();
    InterruptUnmask(intr_adc, m_interrupt_id);
}
