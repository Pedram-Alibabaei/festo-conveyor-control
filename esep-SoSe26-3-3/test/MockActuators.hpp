#ifndef MOCK_ACTUATORS_HPP
#define MOCK_ACTUATORS_HPP

#include <gmock/gmock.h>
#include "hal/i_Actuators.hpp"

class MockActuators : public i_Actuators {
public:
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

    MOCK_METHOD(void, turnAllOff, (), (override));
};

#endif // MOCK_ACTUATORS_HPP
