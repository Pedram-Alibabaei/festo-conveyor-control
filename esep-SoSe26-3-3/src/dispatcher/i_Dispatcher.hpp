/**
 * @file I_Dispatcher.hpp
 * @brief Die einzige Schnittstelle, die die Logik zur Aussenwelt braucht.
 */

#ifndef I_DISPATCHER_HPP
#define I_DISPATCHER_HPP

#include <cstdint>
#include <map>
#include <string>
#include "shared/ipc_definitions.hpp"
#include "shared/types.hpp"

class i_Actuators;
class i_Sensors;
class i_Connection;
class i_Timer;
class i_Logik;

class I_Dispatcher {
public:
    virtual ~I_Dispatcher() = default;

    /**
     * @brief Initialisiert die QNX-Ressourcen (Channel, CoID).
     * @param pos Die physische Position der Anlage.
     */
    virtual void init(AnlagenPosition pos) = 0;

    /**
     * @brief Startet die zentrale Event-Schleife (blockierend).
     */
    virtual void run() = 0;

    /**
     * @brief Beendet die Event-Schleife sicher.
     */
    virtual void stop() = 0;

    // ========================================================================
    // KALIBRIERUNGS-SCHNITTSTELLE
    // ========================================================================

    /**
     * @brief Leitet die geladenen ADC-Schwellenwerte und Mindestsamples an die HAL (ProfileAnalyzer) weiter.
     * @note Wird beim Systemstart aufgerufen, sobald die Konfigurationsdatei geladen wurde.
     * @param emptyBelt Der ADC-Wert für das leere Band.
     * @param flatCenter Der ADC-Wert für das flache Werkstück.
     * @param minSamples Die Mindestanzahl an Samples für die Werkstücklänge.
     */
    virtual void forwardCalibrationToHal(std::uint16_t emptyBelt, std::uint16_t flatCenter, std::uint32_t minSamples) = 0;
    // ========================================================================
    // Dependency Injection (Setter)
    // ========================================================================
    virtual void setActuators(i_Actuators* actuators) = 0;
    virtual void setSensor(i_Sensors* sensor) = 0;
    virtual void setConnectionManager(i_Connection* connection) = 0;
    virtual void setTimer(i_Timer* timer) = 0;
    virtual void setLogik(i_Logik* logik) = 0;

    // ========================================================================
    // (Weiterleitung an andere Anlage / Netzwerk )
    // ========================================================================
    virtual void sendWorkpieceInfo(int id, WpType type) = 0;
    virtual void RequestToReceiveWp() = 0;
    virtual void readyToReceiveWp() = 0;
    virtual void updateWpListe(WpType type) = 0;
    virtual void sendeChuteStats(bool isVoll)=0;
    virtual void sendPartnerResetPressed() = 0;

    // ========================================================================
    // (Weiterleitung an HAL - ADC Zustand)
    // ========================================================================
    /** @brief Setzt den ADC-Zustand auf IDLE (Keine Messung aktiv, Band leer). */
    virtual void setAdcIdelState() = 0;

    /** @brief Bereitet den ADC auf ein ankommendes Werkstueck vor. */
    virtual void setAdcWaitigForWpState() = 0;

    /** @brief Versetzt den Profilanalysator in den Kalibrierungs-Modus. */
    virtual void setAdcCalibrationsState() = 0;

    virtual bool requestFileExists()= 0;

    /**
     * @brief Synchrone Abfrage des lokalen Not-Aus (delegiert an HAL/Sensors).
     */
    virtual bool isEstopPressed() = 0;

    /**
     * @brief Gibt an, ob beim Partner zuletzt ein Not-Aus gemeldet wurde.
     * @note Wird nach CONNECTION_RESTORED genutzt, um in den EStopState zu wechseln.
     */
    virtual bool isPartnerEstopPressed() const = 0;



 // ========================================================================
 // (Weiterleitung an HAL - Aktorik)
 // ========================================================================

    // Motorsteuerung
    virtual void driveRight() = 0;
    virtual void driveLeft() = 0;
    virtual void driveSlowRight() = 0;
    virtual void driveSlowLeft() = 0;
    virtual void driveStop() = 0;

    // Sortiereinheit
    virtual bool isSwitch() = 0;
    virtual void openSwitch()= 0;
    virtual void closeSwitch()= 0;
    virtual void pushEjector()= 0;
    virtual void pullEjector()= 0;

    // Ampelsteuerung (Gruen, Gelb, Rot)
    virtual void lightGreenOn() = 0;
    virtual void lightGreenOff() = 0;
    virtual void lightGreenBlinkFast() = 0;
    virtual void lightGreenBlinkSlow() = 0;

    virtual void lightYellowOn() = 0;
    virtual void lightYellowOff() = 0;
    virtual void lightYellowBlinkFast() = 0;
    virtual void lightYellowBlinkSlow() = 0;

    virtual void lightRedOn() = 0;
    virtual void lightRedOff() = 0;
    virtual void lightRedBlinkFast() = 0;
    virtual void lightRedBlinkSlow() = 0;

    // Bedienpanel-LEDs
    virtual void turnLEDStartOn() = 0;
    virtual void turnLEDStartOff() = 0;
    virtual void turnLEDStartBlinkFast() = 0;
    virtual void turnLEDStartBlinkSlow() = 0;

    virtual void turnLEDResetOn() = 0;
    virtual void turnLEDResetOff() = 0;
    virtual void turnLEDResetBlinkFast() = 0;
    virtual void turnLEDResetBlinkSlow() = 0;

    virtual void turnLEDQ1On() = 0;
    virtual void turnLEDQ1Off() = 0;
    virtual void turnLEDQ1BlinkFast() = 0;
    virtual void turnLEDQ1BlinkSlow() = 0;

    virtual void turnLEDQ2On() = 0;
    virtual void turnLEDQ2Off() = 0;
    virtual void turnLEDQ2BlinkFast() = 0;
    virtual void turnLEDQ2BlinkSlow() = 0;

    virtual void turnAllOff() = 0;
};

#endif // I_DISPATCHER_HPP
