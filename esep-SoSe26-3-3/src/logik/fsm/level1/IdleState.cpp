#include "IdleState.hpp"
#include "ServiceState.hpp"

#include "logik/logik.hpp"

#include "logik/fsm/level1/Level1Actions.hpp"

#include "dispatcher/i_Dispatcher.hpp"

#include <iostream>



namespace logik {



namespace {



void syncLocalCalibrationFile(Logik* context) {

    if (context == nullptr) {

        return;

    }



    auto* dispatcher = context->getDispatcher();

    auto* eventManager = context->getEventManager();

    if (dispatcher == nullptr || eventManager == nullptr) {

        return;

    }



    if (dispatcher->requestFileExists()) {

        eventManager->setCalibrationDataReady(true);

        context->loadCalibrationData();

    } else {

        eventManager->setCalibrationDataReady(false);

    }

}



} // namespace



void IdleState::entry() {

    std::cout << "[IdleState] Entered (Ebene 1)." << std::endl;



    if (m_context == nullptr) {

        return;

    }



    level1::applyIdleEntry(m_context->getDispatcher(), m_context->getEventManager());

    syncLocalCalibrationFile(m_context);

    level1::routeFromIdle(m_context);

}



void IdleState::handle(Event e) {

    if (!m_context || !m_context->getDispatcher()) {

        return;

    }



    auto* dispatcher = m_context->getDispatcher();

    auto* eventManager = m_context->getEventManager();



    switch (e) {

    case Event::DATA_EXIST:

        if (eventManager != nullptr) {

            eventManager->setCalibrationDataReady(true);

        }

        m_context->loadCalibrationData();

        m_context->notifyPartnerCalibStatus();

        std::cout << "[IdleState] Kalibrierungsdaten vorhanden." << std::endl;

        level1::routeFromIdle(m_context);

        break;



    case Event::DATA_NOT_EXIST:

        std::cout << "[IdleState] Keine lokale Kalibrierung -> ReadyForService pruefen." << std::endl;

        if (eventManager != nullptr) {

            eventManager->setCalibrationDataReady(false);

        }

        level1::tryEnterReadyForService(m_context);

        break;



    case Event::PARTNER_CALIB_NOT_READY:
        std::cout << "[IdleState] Partner-Kalibrierung nicht bereit." << std::endl;
        break;

    case Event::PARTNER_CALIB_READY:
        std::cout << "[IdleState] Partner kalibriert." << std::endl;
        level1::routeFromIdle(m_context);
        break;

    case Event::CONNECTION_RESTORED:

        level1::routeFromIdle(m_context);

        break;



    case Event::START_SHORT_PRESSED:
        std::cout << "[IdleState] START kurz ignoriert – zuerst ReadyForRun/ReadyForService." << std::endl;
        break;

    case Event::START_LONG_PRESSED:
        std::cout << "[IdleState] START lang -> ServiceState" << std::endl;
        dispatcher->sendPartnerStartService();
        dispatcher->sendPartnerCalibStatus(false);
        m_context->transitionTo(new ServiceState());
        break;


    case Event::STOP_PRESSED:

        dispatcher->driveStop();

        break;



    case Event::STOP_RELEASED:

        break;



    case Event::RESET_PRESSED:

    case Event::PARTNER_RESET_PRESSED:

        break;



    case Event::RESET_RELEASED:

    case Event::PARTNER_RESET_RELEASED:

        dispatcher->lightYellowOff();

        dispatcher->turnLEDResetOff();

        break;



    default:

        break;

    }

}



} // namespace logik

