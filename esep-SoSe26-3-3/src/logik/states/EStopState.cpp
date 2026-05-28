#include "EStopState.hpp"
#include "IdleState.hpp"
#include "logik/logik.hpp"
#include "logik/i_Logik.hpp"
#include "dispatcher/dispatcher.hpp"
#include <iostream>

namespace logik {

// ============================================================================
// 1. EStopLocalActiveState (Nur LOKAL gedrueckt)
// ============================================================================
void EStopLocalActiveState::onEnter() {
    std::cout << "[EStopLocal] Entered: LOKALER E-Stop aktiv!" << std::endl;
    if (auto d = m_context->getDispatcher()) {
        d->turnAllOff();       // Motor aus, alle Lampen aus
        d->lightRedBlinkFast();
        d->turnLEDQ2BlinkFast(); // Q2 an, da lokal gedrueckt
    }
}

void EStopLocalActiveState::handle(Event e) {
    if (!m_context) return;
    switch (e) {
        case Event::PARTNER_ESTOP_PRESSED:
            m_context->transitionTo(new EStopBothActiveState());
            break;
        case Event::ESTOP_RELEASED:
            m_context->transitionTo(new EStopUnlockedState());
            break;
        default: break; // Alles andere ignorieren
    }
}

// ============================================================================
// 2. EStopPartnerActiveState (Nur PARTNER gedrueckt)
// ============================================================================
void EStopPartnerActiveState::onEnter() {
    std::cout << "[EStopPartner] Entered: PARTNER E-Stop aktiv!" << std::endl;
    if (auto d = m_context->getDispatcher()) {
        d->turnAllOff();       // Motor aus, alle Lampen aus
        d->lightRedBlinkFast();
        // Q2 bleibt AUS, da nicht lokal gedrueckt
    }
}

void EStopPartnerActiveState::handle(Event e) {
    if (!m_context) return;
    switch (e) {
        case Event::ESTOP_PRESSED:
            m_context->transitionTo(new EStopBothActiveState());
            break;
        case Event::PARTNER_ESTOP_RELEASED:
            m_context->transitionTo(new EStopUnlockedState());
            break;
        default: break; // Alles andere ignorieren
    }
}

// ============================================================================
// 3. EStopBothActiveState (LOKAL & PARTNER gedrueckt)
// ============================================================================
void EStopBothActiveState::onEnter() {
    std::cout << "[EStopBoth] Entered: BEIDE E-Stops aktiv!" << std::endl;
    if (auto d = m_context->getDispatcher()) {
        // Motor ist durch vorherige States schon aus
        d->lightRedBlinkFast();
        d->turnLEDQ2BlinkFast(); // Lokal ist gedrueckt, also blinkt Q2
    }
}

void EStopBothActiveState::handle(Event e) {
    if (!m_context) return;
    switch (e) {
        case Event::ESTOP_RELEASED:
            // Lokal entriegelt, aber Partner ist noch gedrueckt
            m_context->transitionTo(new EStopPartnerActiveState());
            break;
        case Event::PARTNER_ESTOP_RELEASED:
            // Partner entriegelt, aber Lokal ist noch gedrueckt
            m_context->transitionTo(new EStopLocalActiveState());
            break;
        default: break; // Alles andere ignorieren
    }
}

// ============================================================================
// 4. EStopUnlockedState (Entriegelt, warte auf LOKALEN Reset)
// ============================================================================
void EStopUnlockedState::onEnter() {
    std::cout << "[EStopUnlocked] Alle E-Stops entriegelt. Warte auf RESET..." << std::endl;
    if (auto d = m_context->getDispatcher()) {
        d->turnLEDQ2Off(); // E-Stop ist gezogen, Q2 geht aus
        d->lightRedOn();   // Dauer-Rot signalisiert: Fehler behoben, warte auf Quittierung
        d->turnLEDResetBlinkFast(); // Fordert den Bediener auf, Reset zu druecken
    }
}

void EStopUnlockedState::handle(Event e) {
    if (!m_context) return;
    switch (e) {
        case Event::ESTOP_PRESSED:
            m_context->transitionTo(new EStopLocalActiveState());
            break;
        case Event::PARTNER_ESTOP_PRESSED:
            m_context->transitionTo(new EStopPartnerActiveState());
            break;
        case Event::RESET_PRESSED:
            std::cout << "[EStopUnlocked] RESET erfolgreich! Anlage ist wieder bereit." << std::endl;
            if (auto d = m_context->getDispatcher()) {
                d->lightRedOff();
                d->turnLEDResetOff();
            }
            m_context->transitionTo(new IdleState());
            break;
        default: break; // Alles andere ignorieren
    }
}

} // namespace logik
