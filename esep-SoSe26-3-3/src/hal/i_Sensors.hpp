/**
 * @file i_Sensors.hpp
 * @brief Abstraktions-Schnittstelle für die Sensorik der Festo-Sortieranlage.
 * * @details WICHTIGER ARCHITEKTUR-HINWEIS: 
 * Diese Klasse arbeitet ereignisgesteuert (Event-Driven) mittels QNX Pulses. 
 * Anstatt Sensoren aktiv abzufragen (Polling), muss die Logik/der Dispatcher 
 * die Interrupts einmalig über initInterrupts() aktivieren. 
 * Danach sendet diese HAL-Klasse bei jeder Flankenänderung (Sensor blockiert/frei, 
 * Taste gedrückt/losgelassen) automatisch einen 40-Bit Pulse an den übergebenen Kanal.
 * * Das Bit-Packing der Pulse-Payload (ID + Zustand) ist in der IPC_definitions.hpp definiert.
 */

#ifndef I_SENSORS_HPP
#define I_SENSORS_HPP

#include <cstdint>

class i_Sensors {
public:
    virtual ~i_Sensors() = default;

    // --- Interrupt-Steuerung (Digitale Sensoren & Taster) ---

    /**
     * @brief Aktiviert die Hardware-Interrupts für alle digitalen Eingänge (GPIO 0).
     * * Sobald diese Methode aufgerufen wird, beginnt die HAL, bei Änderungen an den 
     * Lichtschranken, dem Metallsensor oder dem Bedienpanel (Start, Stop, Reset, E-Stop) 
     * Nachrichten (Pulses) zu generieren.
     * * @param coid Die Connection-ID (Kanal) des Dispatchers, an den die Pulses gesendet werden.
     * @param pulseCode Der 8-Bit Identifier (z.B. PulseCode::HAL_INPUT), der den Pulsen angehängt wird.
     */
    virtual void initInterrupts(int coid, uint8_t pulseCode) = 0;

    /**
     * @brief Deaktiviert alle Hardware-Interrupts und stoppt das Senden von Pulsen.
     * * @note Sollte beim sauberen Herunterfahren der Anlage oder im Falle eines 
     * kritischen Systemfehlers aufgerufen werden.
     */
    virtual void stopInterrupts() = 0;


    // --- Analoge Sensorik & Initiale Statusabfragen ---

    /**
     * @brief Liest den aktuellen analogen Wert des Höhensensors aus.
     * * Im Gegensatz zu den digitalen Lichtschranken wird dieser Wert synchron über 
     * den ADC (Analog-to-Digital Converter) des Beaglebone Black ausgelesen.
     * * @return uint16_t Der gemessene Rohwert oder umgerechnete Millimeter-Wert.
     */
    virtual uint16_t getHeight() = 0;

    /**
     * @brief Synchrone Abfrage des E-Stop-Zustands (Sicherheitsfunktion).
     * * Obwohl der E-Stop auch über Interrupts gemeldet wird, ist es für die 
     * Hochfahr-Routine (Startup) der Anlage wichtig, den Zustand *vor* der 
     * Aktivierung der Interrupts aktiv prüfen zu können.
     * * @return true Wenn der Not-Halt-Taster physisch gedrückt ist.
     * @return false Wenn der Not-Halt-Taster gelöst (betriebsbereit) ist.
     */
    virtual bool isEstopPressed() = 0;
};

#endif // I_SENSORS_HPP