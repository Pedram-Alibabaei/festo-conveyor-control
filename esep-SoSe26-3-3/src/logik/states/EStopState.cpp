#include "EStopState.hpp"
#include "IdleState.hpp"
#include "logik/logik.hpp"
#include "logik/i_Logik.hpp"
#include "dispatcher/dispatcher.hpp"
#include <iostream>

namespace logik {

// Zwei statische Variablen, um den Zustand beider Taster absolut sauber zu trennen.
// Diese werden bei jedem neuen Not-Halt in onEnter() wieder zurückgesetzt!
static bool m_localResetPressed = false;
static bool m_partnerResetPressed = false;

// ============================================================================
// 1. EStopLocalActiveState (Nur LOKAL gedrueckt)
// ============================================================================
void EStopLocalActiveState::onEnter() {
    m_localResetPressed = false;   // Reset-Flags für den neuen Not-Halt löschen
    m_partnerResetPressed = false;
    if (auto d = m_context->getDispatcher()) {
        d->turnAllOff();
        d->lightRedBlinkFast();
        d->turnLEDQ2BlinkFast();
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
        default: break;
    }
}

// ============================================================================
// 2. EStopPartnerActiveState (Nur PARTNER gedrueckt)
// ============================================================================
void EStopPartnerActiveState::onEnter() {
    m_localResetPressed = false;   // Reset-Flags für den neuen Not-Halt löschen
    m_partnerResetPressed = false;
    if (auto d = m_context->getDispatcher()) {
        d->turnAllOff();
        d->lightRedBlinkFast();
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
        default: break;
    }
}

// ============================================================================
// 3. EStopBothActiveState (LOKAL & PARTNER gedrueckt)
// ============================================================================
void EStopBothActiveState::onEnter() {
    if (auto d = m_context->getDispatcher()) {
        d->lightRedBlinkFast();
        d->turnLEDQ2BlinkFast();
    }
}

void EStopBothActiveState::handle(Event e) {
    if (!m_context) return;
    switch (e) {
        case Event::ESTOP_RELEASED:
            m_context->transitionTo(new EStopPartnerActiveState());
            break;
        case Event::PARTNER_ESTOP_RELEASED:
            m_context->transitionTo(new EStopLocalActiveState());
            break;
        default: break;
    }
}

// ============================================================================
// 4. EStopUnlockedState (Entriegelt, warte auf synchrone Quittierung)
// ============================================================================
void EStopUnlockedState::onEnter() {
    if (auto d = m_context->getDispatcher()) {
        d->turnLEDQ2Off();
        d->lightRedOn();   // Dauer-Rot: Beide Pilztaster sind gezogen
        d->turnLEDResetBlinkFast(); // Reset-LED blinkt: Aufforderung zum Drücken
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
            // 1. Dem Partner aktiv sagen: "Ich habe hier gerade Reset gedrückt!"
            if (auto d = m_context->getDispatcher()) {
                d->sendPartnerResetRequest();
            }

            // 2. Für uns selbst merken: Lokaler Knopf ist gedrückt
            m_localResetPressed = true;

            // Feedback an den Bediener: Reset-LED leuchtet jetzt dauerhaft statt zu blinken.
            // Das signalisiert: An dieser Anlage ist die Quittierung registriert.
            if (auto d = m_context->getDispatcher()) {
                d->turnLEDResetOn();
            }

            // 3. Wenn der Partner VORHER oder GENAU JETZT auch schon bereit war: Auslösen!
            if (m_localResetPressed && m_partnerResetPressed) {
                executeReset();
            }
            break;

        case Event::PARTNER_RESET_REQUESTED:
            // Der Partner teilt uns mit, dass sein Reset-Knopf betätigt wurde!
            m_partnerResetPressed = true;

            // Erst wenn wir SELBST auch schon gedrückt haben, schalten wir ab.
            if (m_localResetPressed && m_partnerResetPressed) {
                executeReset();
            }
            break;

        default:
            break;
    }
}

// Hilfsmethode: Schaltet Lichter aus und geht in den IdleState
void EStopUnlockedState::executeReset() {
    if (auto d = m_context->getDispatcher()) {
        d->lightRedOff();
        d->turnLEDResetOff();
    }
    // Wichtig: Die statischen Flags für den nächsten E-Stop säubern
    m_localResetPressed = false;
    m_partnerResetPressed = false;

    m_context->transitionTo(new IdleState()); //
}
    // ============================================================================
    // 5. ConnectionLostState (Netzwerkverbindung abgerissen)
    // ============================================================================
    void ConnectionLostState::onEnter() {
    if (auto d = m_context->getDispatcher()) {
        d->turnAllOff();
        d->lightRedBlinkFast(); // Identisch zum physischen Not-Halt
    }
   // std::cout << "[Safety] KRITISCH: NETZWERKABBRUCH! Anlage sicherheitsgerichtet gestoppt." << std::endl;
}

    void ConnectionLostState::handle(Event e) {
    if (!m_context) return;

    switch (e) {
        case Event::CONNECTION_RESTORED:
            // Verbindung ist wieder da -> Wir wechseln in den Quittierungs-Modus!
          //  std::cout << "[Safety] Verbindung wiederhergestellt. Warte auf Quittierung (Reset)..." << std::endl;
            m_context->transitionTo(new EStopUnlockedState());
            break;

        case Event::ESTOP_PRESSED:
            // Falls jemand während des Stromausfalls aus Panik noch den echten Pilztaster drückt
            m_context->transitionTo(new EStopLocalActiveState());
            break;

        default:
            // ALLES andere (inklusive Reset) verpufft wirkungslos, solange das Netz weg ist!
            break;
    }
}
} // namespace logik