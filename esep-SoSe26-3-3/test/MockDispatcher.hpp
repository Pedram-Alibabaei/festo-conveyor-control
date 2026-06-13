#ifndef MOCK_DISPATCHER_HPP
#define MOCK_DISPATCHER_HPP

#include <gmock/gmock.h>
#include "dispatcher/i_Dispatcher.hpp"

class MockDispatcher : public I_Dispatcher {
public:
    MOCK_METHOD(void, init, (ipc::AnlagenPosition pos), (override));
    MOCK_METHOD(void, run, (), (override));
    MOCK_METHOD(void, stop, (), (override));

    MOCK_METHOD(void, setActuators, (i_Actuators*), (override));
    MOCK_METHOD(void, setSensor, (i_Sensor*), (override));
    MOCK_METHOD(void, setConnectionManager, (i_Connection*), (override));
    MOCK_METHOD(void, setTimer, (i_Timer*), (override));
    MOCK_METHOD(void, setLogik, (logik::i_Logik*), (override));

    MOCK_METHOD(void, startBeltClock, (int ms), (override));

    MOCK_METHOD(void, workpieceInfo, (int id, WpType type), (override));
    MOCK_METHOD(void, RequestToReceiveWp, (), (override));
    MOCK_METHOD(void, readyToReceiveWp, (), (override));
    MOCK_METHOD(void, updateWpListe, (WpType type), (override));

    MOCK_METHOD(void, driveRight, (), (override));
    MOCK_METHOD(void, driveLeft, (), (override));
    MOCK_METHOD(void, driveSlowRight, (), (override));
    MOCK_METHOD(void, driveSlowLeft, (), (override));
    MOCK_METHOD(void, driveStop, (), (override));

    MOCK_METHOD(void, openJunction, (), (override));
    MOCK_METHOD(void, closeJunction, (), (override));

    MOCK_METHOD(void, lightGreenOn, (), (override));
    MOCK_METHOD(void, lightGreenOff, (), (override));
    MOCK_METHOD(void, lightGreenBlinkFast, (), (override));
    MOCK_METHOD(void, lightGreenBlinkSlow, (), (override));

    MOCK_METHOD(void, lightYellowOn, (), (override));
    MOCK_METHOD(void, lightYellowOff, (), (override));
    MOCK_METHOD(void, lightYellowBlinkFast, (), (override));
    MOCK_METHOD(void, lightYellowBlinkSlow, (), (override));

    MOCK_METHOD(void, lightRedOn, (), (override));
    MOCK_METHOD(void, lightRedOff, (), (override));
    MOCK_METHOD(void, lightRedBlinkFast, (), (override));
    MOCK_METHOD(void, lightRedBlinkSlow, (), (override));

    MOCK_METHOD(void, turnLEDStartOn, (), (override));
    MOCK_METHOD(void, turnLEDStartOff, (), (override));
    MOCK_METHOD(void, turnLEDStartBlinkFast, (), (override));
    MOCK_METHOD(void, turnLEDStartBlinkSlow, (), (override));

    MOCK_METHOD(void, turnLEDResetOn, (), (override));
    MOCK_METHOD(void, turnLEDResetOff, (), (override));
    MOCK_METHOD(void, turnLEDResetBlinkFast, (), (override));
    MOCK_METHOD(void, turnLEDResetBlinkSlow, (), (override));

    MOCK_METHOD(void, turnLEDQ1On, (), (override));
    MOCK_METHOD(void, turnLEDQ1Off, (), (override));
    MOCK_METHOD(void, turnLEDQ1BlinkFast, (), (override));
    MOCK_METHOD(void, turnLEDQ1BlinkSlow, (), (override));

    MOCK_METHOD(void, turnLEDQ2On, (), (override));
    MOCK_METHOD(void, turnLEDQ2Off, (), (override));
    MOCK_METHOD(void, turnLEDQ2BlinkFast, (), (override));
    MOCK_METHOD(void, turnLEDQ2BlinkSlow, (), (override));

    MOCK_METHOD(void, sendPartnerResetPressed, (), (override));
    MOCK_METHOD(void, sendLocalCalibStatusToPartner, (), (override));
    MOCK_METHOD(void, forwardCalibrationToHal, (std::uint16_t, std::uint16_t, std::uint32_t), (override));
    MOCK_METHOD(bool, requestFileExists, (), (override));
    MOCK_METHOD(bool, isEstopPressed, (), (override));
    MOCK_METHOD(bool, isPartnerEstopPressed, (), (const, override));
    MOCK_METHOD(void, sendeChuteStats, (bool), (override));

    MOCK_METHOD(void, turnAllOff, (), (override));
};

#endif // MOCK_DISPATCHER_HPP
