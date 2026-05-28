#include "IdleState.hpp"
#include "EStopState.hpp"
#include "RunningState.hpp"
#include "ServiceState.hpp"
#include "dispatcher/dispatcher.hpp"
#include <iostream>

namespace logik {

void IdleState::onEnter() {
  //  std::cout << "[IdleState] Entered. Anlage wartet auf Befehle." << std::endl;
}

void IdleState::handle(Event e) {
    if (!m_context || !m_context->getDispatcher()) return;

    // 1. Lese die aktuelle Position aus (Bin ich Anlage 1 oder Anlage 2?)
    AnlagenPosition myPos = m_context->getPosition();

    // 2. Dispatcher-Pointer fuer besseren Lesefluss speichern
    auto dispatcher = m_context->getDispatcher();

    switch (e) {
        // ==========================================================
        // 1. START-TASTER (Lokal & Netzwerk)
        // ==========================================================
        case Event::START_RELEASED:
            if (myPos == AnlagenPosition::VORN_ANLAGE_1) {
                //std::cout << "[IdleState] Anlage 1: LOKALER START -> Wechsel zu RunningState" << std::endl;
                dispatcher->lightGreenOn();
                m_context->transitionTo(new RunningState());
            }
            else {
                // Anlage 2 kann auch ueber den eigenen Taster gestartet werden
              //  std::cout << "[IdleState] Anlage 2: LOKALER START -> Wechsel zu RunningState" << std::endl;
                dispatcher->lightGreenOn();
                m_context->transitionTo(new RunningState());
            }
            break;

        case Event::PARTNER_START_RELEASED:
            if (myPos == AnlagenPosition::HINTEN_ANLAGE_2) {
                // Master-Master Logik: Anlage 2 faehrt automatisch mit an, wenn Anlage 1 gestartet wird
               // std::cout << "[IdleState] Anlage 2: Partner hat START gedrueckt -> Laufe mit an!" << std::endl;
                dispatcher->lightGreenOn();
                m_context->transitionTo(new RunningState());
            }
            else {
                // Anlage 1 ignoriert normalerweise, wenn Anlage 2 manuell gestartet wird
               // std::cout << "[IdleState] Anlage 1: Partner-Start ignoriert." << std::endl;
            }
            break;

        // ==========================================================
        // 2. STOP-TASTER (Lokal & Netzwerk)
        // ==========================================================
        case Event::STOP_PRESSED:
          //  std::cout << "[IdleState] LOKALER STOP gedrueckt -> Ruhezustand (Motor Stop)!" << std::endl;
            dispatcher->driveStop();
            dispatcher->lightRedBlinkSlow();
            break;

        case Event::STOP_RELEASED:
           // std::cout << "[IdleState] LOKALER STOP losgelassen -> Rot aus!" << std::endl;
            dispatcher->lightRedOff();
            break;

        case Event::PARTNER_STOP_PRESSED:
            // Da beide im IdleState sind, koennen wir das als Info loggen, muessen aber nichts tun
            if (myPos == AnlagenPosition::HINTEN_ANLAGE_2) {
              //  std::cout << "[IdleState] Info fuer Anlage 2: Anlage 1 wurde gestoppt." << std::endl;
            } else {
             //   std::cout << "[IdleState] Info fuer Anlage 1: Anlage 2 wurde gestoppt." << std::endl;
            }
            break;

        // ==========================================================
        // 3. RESET-TASTER (Lokal & Netzwerk)
        // ==========================================================
        case Event::RESET_PRESSED:
            //std::cout << "[IdleState] LOKALER RESET gedrueckt -> Gelb blinkt!" << std::endl;
            dispatcher->lightYellowBlinkFast();
            break;

        case Event::RESET_RELEASED:
           // std::cout << "[IdleState] LOKALER RESET losgelassen -> Gelb aus!" << std::endl;
            dispatcher->lightYellowOff();
            break;

        case Event::PARTNER_RESET_PRESSED:
        case Event::PARTNER_RESET_RELEASED:
            // Da unser Reset-Konzept jetzt zu 100% autark ist (jeder kuemmert sich um seinen eigenen Reset),
            // ignorieren wir den Partner-Reset im normalen Leerlauf komplett.
            break;

        // ==========================================================
        // 4. NOT-HALT (E-STOP) - Löst SOFORT den Zustandswechsel aus!
        // ==========================================================
        case Event::ESTOP_PRESSED:
                  //  std::cout << "[State] LOKALER ESTOP -> Wechsel zu EStopLocalActiveState" << std::endl;
                    m_context->transitionTo(new EStopLocalActiveState());
                    break;

                case Event::PARTNER_ESTOP_PRESSED:
                  //  std::cout << "[State] PARTNER ESTOP -> Wechsel zu EStopPartnerActiveState" << std::endl;
                    m_context->transitionTo(new EStopPartnerActiveState());
                    break;

        default:
            // Ignoriere Lichtschranken, Hoehensensoren etc., da das Band ohnehin steht
            break;
    }
}

} // namespace logik
