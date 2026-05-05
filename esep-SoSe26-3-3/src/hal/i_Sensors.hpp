/**
 * @file i_Sensors.hpp
 * @brief Abstraktions-Schnittstelle für die Sensorik der Festo-Sortieranlage.
 *
 * @details WICHTIGER ARCHITEKTUR-HINWEIS:
 * Diese Klasse arbeitet vollständig ereignisgesteuert (Event-Driven) mittels QNX Pulses.
 * Anstatt Sensoren aktiv abzufragen (Polling), muss die Logik/der Dispatcher
 * die Interrupts einmalig über initInterrupts() und initAdcInterrupt() aktivieren.
 * Danach sendet diese HAL-Klasse bei jeder Flankenänderung und nach jeder
 * Werkstück-Profilanalyse automatisch einen Pulse an den übergebenen Kanal.
 *
 * Das Bit-Packing der Pulse-Payload (ID + Zustand) ist in der IPC_definitions.hpp definiert.
 */

#ifndef I_SENSORS_HPP
#define I_SENSORS_HPP

#include <cstdint>

class i_Sensors {
public:
    virtual ~i_Sensors() = default;

    // ========================================================================
    // 1. Digitale Interrupt-Steuerung (GPIO)
    // ========================================================================

    /**
     * @brief Aktiviert die Hardware-Interrupts für alle digitalen Eingänge (GPIO 0).
     *
     * Sobald diese Methode aufgerufen wird, beginnt die HAL, bei Änderungen an den
     * Lichtschranken, dem Metallsensor oder dem Bedienpanel (Start, Stop, Reset, E-Stop)
     * Nachrichten (Pulses) zu generieren.
     *
     * @param coid Die Connection-ID (Kanal) des Dispatchers, an den die Pulses gesendet werden.
     * @param pulseCode Der 8-Bit Identifier (z.B. PulseCode::HAL_INPUT), der den Pulsen angehängt wird.
     */
    virtual void initInterrupts(int coid, uint8_t pulseCode) = 0;

    /**
     * @brief Deaktiviert alle digitalen Hardware-Interrupts.
     */
    virtual void stopInterrupts() = 0;


    // ========================================================================
    // 2. Analoge Sensorik (Asynchrone Höhen-Profilanalyse)
    // ========================================================================

    /**
     * @brief Überträgt die Kalibrierungsdaten an die ADC-Auswertelogik (ProfileAnalyzer).
     *
     * Muss aufgerufen werden, bevor Messungen stattfinden (z.B. nach dem Auslesen
     * einer Konfigurationsdatei beim Systemstart).
     */
    virtual void setAdcCalibration(uint32_t trigger,
                                   uint32_t flatMin, uint32_t flatMax,
                                   uint32_t tallMin, uint32_t tallMax,
                                   uint32_t holeMin, uint32_t holeMax,
                                   uint32_t emptyCountLimit) = 0;



    // ========================================================================
    // 3. Initiale Statusabfragen (Polling nur für Startup-Sicherheit)
    // ========================================================================

    /**
     * @brief Synchrone Abfrage des E-Stop-Zustands (Sicherheitsfunktion).
     *
     * Obwohl der E-Stop auch über Interrupts gemeldet wird, ist es für die
     * Hochfahr-Routine (Startup) der Anlage wichtig, den Zustand *vor* der
     * Aktivierung der Interrupts aktiv prüfen zu können.
     *
     * @return true Wenn der Not-Halt-Taster physisch gedrückt ist.
     * @return false Wenn der Not-Halt-Taster gelöst (betriebsbereit) ist.
     */
    virtual bool isEstopPressed() = 0;
};

#endif // I_SENSORS_HPP
