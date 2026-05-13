/**
* @file I_Dispatcher.hpp
 * @brief Die einzige Schnittstelle, die die Logik zur Aussenwelt braucht.
 */

#ifndef I_DISPATCHER_HPP
#define I_DISPATCHER_HPP

#include <cstdint>

class I_Dispatcher {
public:
    virtual ~I_Dispatcher() = default;

    // ========================================================================
    // (Weiterleitung an Timer )
    // ========================================================================
    virtual void startBeltClock(int ms)=0;


    // ========================================================================
    // (Weiterleitung an andere Anlage )
    // ========================================================================

    // ========================================================================
    // (Weiterleitung an HAL)
    // ========================================================================

    //Motorsteuerung
    virtual void driveRight() = 0;
    virtual void driveLeft() = 0;
    virtual void driveSlowRight() = 0;
    virtual void driveSlowLeft() = 0;
    virtual void driveStop() = 0;

    //Sortiereinheit
    virtual void openJunction() = 0;
    virtual void closeJunction() = 0;

    // Ampelsteuerung
    // Grüne
    virtual void lightGreenOn() = 0;
    virtual void lightGreenOff() = 0;
    virtual void lightGreenBlinkFast() = 0;
    virtual void lightGreenBlinkSlow() = 0;

    // Gelbe
    virtual void lightYellowOn() = 0;
    virtual void lightYellowOff() = 0;
    virtual void lightYellowBlinkFast() = 0;
    virtual void lightYellowBlinkSlow() = 0;

    // Rote
    virtual void lightRedOn() = 0;
    virtual void lightRedOff() = 0;
    virtual void lightRedBlinkFast() = 0;
    virtual void lightRedBlinkSlow() = 0;

   // Bedienpanel-LEDs
    // Start-Tasten-LED
    virtual void turnLEDStartOn() = 0;
    virtual void turnLEDStartOff() = 0;
    virtual void turnLEDStartBlinkFast() = 0;
    virtual void turnLEDStartBlinkSlow() = 0;

    // Reset-Tasten-LED
    virtual void turnLEDResetOn() = 0;
    virtual void turnLEDResetOff() = 0;
    virtual void turnLEDResetBlinkFast() = 0;
    virtual void turnLEDResetBlinkSlow() = 0;

    //  Q1
    virtual void turnLEDQ1On() = 0;
    virtual void turnLEDQ1Off() = 0;
    virtual void turnLEDQ1BlinkFast() = 0;
    virtual void turnLEDQ1BlinkSlow() = 0;

    //  Q2
    virtual void turnLEDQ2On() = 0;
    virtual void turnLEDQ2Off() = 0;
    virtual void turnLEDQ2BlinkFast() = 0;
    virtual void turnLEDQ2BlinkSlow() = 0;


    virtual void turnAllOff() = 0;
};

#endif // I_DISPATCHER_HPP