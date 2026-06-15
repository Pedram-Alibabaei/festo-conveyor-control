/**
 * @file i_Actuators.hpp
 * @brief Abstraktions-Schnittstelle für die Aktorik der Festo-Sortieranlage.
 * * Dieses Interface definiert alle verfügbaren Operationen zur Steuerung der Hardware-Aktoren
 * (Motoren, Weichen, Lampen und LEDs). Die tatsächliche Implementierung erfolgt im 
 * HAL-Modul unter Berücksichtigung der spezifischen GPIO-Register des Beaglebone Black.
 */

#ifndef I_ACTUATORS_HPP
#define I_ACTUATORS_HPP

#include <cstdint>

class i_Actuators {
public:
    virtual ~i_Actuators() = default;

    // --- Motorsteuerung (GPIO 1) ---
    
    /** @brief Startet den Transportband-Motor im Rechtslauf. */
    virtual void driveRight() = 0;

    /** @brief Startet den Transportband-Motor im Linkslauf. */
    virtual void driveLeft() = 0;

    /** @brief Aktiviert den langsamen Modus für den Rechtslauf (z.B. für die Höhenmessung). */
    virtual void driveSlowRight() = 0;

    /** @brief Aktiviert den langsamen Modus für den Linkslauf. */
    virtual void driveSlowLeft() = 0;

    /** @brief Stoppt den Transportband-Motor sofort. */
    virtual void driveStop() = 0;


    // --- Sortiereinheit (GPIO 1) ---
    virtual bool isSwitch() = 0;
    virtual void openSwitch()= 0;
    virtual void closeSwitch()= 0;
    virtual void pushEjector()= 0;
    virtual void pushEjectorFor(std::uint32_t holdMs)= 0;
    virtual void pullEjector()= 0;


    // --- Ampelsteuerung / Signalleuchten (GPIO 1) ---

    // Grüne Signalleuchte
    /** @brief Schaltet die grüne Signalleuchte dauerhaft ein. */
    virtual void lightGreenOn() = 0;
    /** @brief Schaltet die grüne Signalleuchte aus. */
    virtual void lightGreenOff() = 0;
    /** @brief Lässt die grüne Signalleuchte schnell blinken (z.B. 1 Hz). */
    virtual void lightGreenBlinkFast() = 0;
    /** @brief Lässt die grüne Signalleuchte langsam blinken (z.B. 0,5 Hz). */
    virtual void lightGreenBlinkSlow() = 0;

    // Gelbe Signalleuchte
    /** @brief Schaltet die gelbe Signalleuchte dauerhaft ein. */
    virtual void lightYellowOn() = 0;
    /** @brief Schaltet die gelbe Signalleuchte aus. */
    virtual void lightYellowOff() = 0;
    /** @brief Lässt die gelbe Signalleuchte schnell blinken (z.B. 1 Hz für Warnungen). */
    virtual void lightYellowBlinkFast() = 0;
    /** @brief Lässt die gelbe Signalleuchte langsam blinken (z.B. 0,5 Hz). */
    virtual void lightYellowBlinkSlow() = 0;

    // Rote Signalleuchte
    /** @brief Schaltet die rote Signalleuchte dauerhaft ein. */
    virtual void lightRedOn() = 0;
    /** @brief Schaltet die rote Signalleuchte aus. */
    virtual void lightRedOff() = 0;
    /** @brief Lässt die rote Signalleuchte schnell blinken (z.B. 1 Hz für unquittierten Fehler). */
    virtual void lightRedBlinkFast() = 0;
    /** @brief Lässt die rote Signalleuchte langsam blinken (z.B. 0,5 Hz für gegangenen Fehler). */
    virtual void lightRedBlinkSlow() = 0;


   // --- Bedienpanel-LEDs (GPIO 2) ---

    // Start-Tasten-LED
    /** @brief Aktiviert die Hintergrundbeleuchtung der Start-Taste dauerhaft. */
    virtual void turnLEDStartOn() = 0;
    /** @brief Deaktiviert die Hintergrundbeleuchtung der Start-Taste. */
    virtual void turnLEDStartOff() = 0;
    /** @brief Lässt die Start-Taste schnell blinken (z.B. System wartet dringend auf Startfreigabe). */
    virtual void turnLEDStartBlinkFast() = 0;
    /** @brief Lässt die Start-Taste langsam blinken (z.B. Anlage ist betriebsbereit). */
    virtual void turnLEDStartBlinkSlow() = 0;

    // Reset-Tasten-LED
    /** @brief Aktiviert die Hintergrundbeleuchtung der Reset-Taste dauerhaft. */
    virtual void turnLEDResetOn() = 0;
    /** @brief Deaktiviert die Hintergrundbeleuchtung der Reset-Taste. */
    virtual void turnLEDResetOff() = 0;
    /** @brief Lässt die Reset-Taste schnell blinken (Signalisiert zwingend notwendige Quittierung nach Fehler). */
    virtual void turnLEDResetBlinkFast() = 0;
    /** @brief Lässt die Reset-Taste langsam blinken. */
    virtual void turnLEDResetBlinkSlow() = 0;

    // Frei belegbare Signalleuchte Q1
    /** @brief Schaltet die zusätzliche Signalleuchte Q1 dauerhaft ein. */
    virtual void turnLEDQ1On() = 0;
    /** @brief Schaltet die zusätzliche Signalleuchte Q1 aus. */
    virtual void turnLEDQ1Off() = 0;
    /** @brief Lässt die Signalleuchte Q1 schnell blinken. */
    virtual void turnLEDQ1BlinkFast() = 0;
    /** @brief Lässt die Signalleuchte Q1 langsam blinken. */
    virtual void turnLEDQ1BlinkSlow() = 0;

    // Frei belegbare Signalleuchte Q2
    /** @brief Schaltet die zusätzliche Signalleuchte Q2 dauerhaft ein. */
    virtual void turnLEDQ2On() = 0;
    /** @brief Schaltet die zusätzliche Signalleuchte Q2 aus. */
    virtual void turnLEDQ2Off() = 0;
    /** @brief Lässt die Signalleuchte Q2 schnell blinken. */
    virtual void turnLEDQ2BlinkFast() = 0;
    /** @brief Lässt die Signalleuchte Q2 langsam blinken. */
    virtual void turnLEDQ2BlinkSlow() = 0;


    // --- Globaler Reset ---

    /** * @brief Setzt alle Aktoren auf den sicheren Ausgangszustand zurück.
     * Schaltet Motoren, Weichen, Ampeln und alle LEDs sofort AUS und stoppt laufende Blink-Timer.
     */
    virtual void turnAllOff() = 0;
};

#endif // I_ACTUATORS_HPP