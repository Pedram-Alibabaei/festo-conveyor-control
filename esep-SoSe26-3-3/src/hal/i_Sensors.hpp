/**
 * @file i_Sensors.hpp
 * @brief Abstraktions-Schnittstelle für die Sensorik der Festo-Sortieranlage.
 *
 * @details WICHTIGER ARCHITEKTUR-HINWEIS:
 * Diese Klasse arbeitet vollständig ereignisgesteuert (Event-Driven) mittels QNX Pulses.
 * Anstatt Sensoren aktiv abzufragen (Polling), muss der Dispatcher/die Logik
 * die Hardware-Meldungen einmalig über init() aktivieren.
 * Danach sendet diese HAL-Klasse bei jeder Flankenänderung und nach jeder
 * Werkstück-Profilanalyse automatisch einen Pulse an den übergebenen Kanal.
 *
 * Das Bit-Packing der Pulse-Payload (ID + Zustand) ist in der ipc_definitions.hpp definiert.
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
     */
    virtual void init(int coid) = 0;

    /**
     * @brief Deaktiviert alle digitalen Hardware-Interrupts.
     */
    virtual void stopInterrupts() = 0;


    // ========================================================================
    // 2. ADC- & PROFILANALYSATOR-ZUSTÄNDE (Zustandsmaschine der Messung)
    // ========================================================================

    /**
     * @brief Setzt den ADC-Zustand auf IDLE (Keine Messung aktiv, Band leer).
     */
    virtual void setAdcIdelState() = 0;

    /**
     * @brief Bereitet den ADC auf ein ankommendes Werkstueck vor.
     */
    virtual void setAdcWaitigForWpState() = 0;

    /**
     * @brief Versetzt den Profilanalysator in den Kalibrierungs-Modus.
     */
    virtual void setAdcCalibrationsState() = 0;

    /**
     * @brief Service-Kalibrierung: Hoehensensor nur fuer Tick-Messung (ohne Wp-Typ).
     */
    virtual void setServiceTimingMeasureState() = 0;

    /**
     * @brief Pausiert oder reaktiviert die Profilauswertung bei stehendem Band.
     *
     * Die bisherigen Messdaten bleiben erhalten; nur neue ADC-Werte werden
     * während der Pause nicht in die Profilentscheidung eingerechnet.
     */
    virtual void setAdcTransportPaused(bool paused) = 0;

    /**
     * @brief Aktualisiert die internen Schwellenwerte des ProfileAnalyzers live.
     * * Wird beim Systemstart vom Dispatcher aufgerufen, sobald der CalibrationManager
     * die Werte erfolgreich aus der Textdatei geladen hat.
     * * @param calib_mode Der Sub-Typ der Kalibrierung (z.B. ipc::CalibMode::EMPTY_BELT).
     * @param calib_value Der geladene ADC-Rohwert oder Ticks-Wert.
     */
    virtual void updateCalibrationThresholds(uint16_t calib_mode, uint16_t calib_value) = 0;


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