#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "logik/logik.hpp"
#include "MockDispatcher.hpp"

using ::testing::_;
using ::testing::Exactly;

class LogikTest : public ::testing::Test {
protected:
    logik::Logik* fsm;
    MockDispatcher* mockDispatcher;

    virtual void SetUp() override {
        fsm = new logik::Logik();
        mockDispatcher = new MockDispatcher();
        
        fsm->setDispatcher(mockDispatcher);
        fsm->init(ipc::VORN_ANLAGE_1);
    }

    virtual void TearDown() override {
        delete fsm;
        delete mockDispatcher;
    }
};

// Test: When START_PRESSED event is processed, lightGreenBlinkFast should be called
TEST_F(LogikTest, StartPressedBlinksGreenFast) {
    // Expectation: Dispatcher soll genau 1x blinken anfordern
    EXPECT_CALL(*mockDispatcher, lightGreenBlinkFast()).Times(Exactly(1));

    // Ausfuehrung: Event in die Logik pushen
    fsm->processEvent(logik::Event::START_PRESSED);
}

// Test: When START_RELEASED event is processed in RunningState, lightGreenOff should be called
TEST_F(LogikTest, StartReleasedTurnsGreenOff) {
    // Wechsel in RunningState (Kurzer Druck: PRESSED -> RELEASED sofort)
    fsm->processEvent(logik::Event::START_PRESSED);
    EXPECT_CALL(*mockDispatcher, lightGreenOn()).Times(Exactly(1));
    fsm->processEvent(logik::Event::START_RELEASED); // Wechselt zu RunningState

    EXPECT_CALL(*mockDispatcher, lightGreenOff()).Times(Exactly(1));
    fsm->processEvent(logik::Event::START_RELEASED); // Event im RunningState
}


// Test: When ENTRY_BLOCKED detects an item in RunningState, motor drives right and green blinks slow
TEST_F(LogikTest, EntryBlockedStartsMotorRightAndBlinksSlow) {
    // Wechsel in RunningState
    fsm->processEvent(logik::Event::START_PRESSED);
    EXPECT_CALL(*mockDispatcher, lightGreenOn()).Times(Exactly(1));
    fsm->processEvent(logik::Event::START_RELEASED);

    EXPECT_CALL(*mockDispatcher, driveRight()).Times(Exactly(1));
    EXPECT_CALL(*mockDispatcher, lightGreenBlinkSlow()).Times(Exactly(1));

    fsm->processEvent(logik::Event::ENTRY_BLOCKED);
}

// Test: When END_BLOCKED is processed in RunningState, motor stops
TEST_F(LogikTest, EndBlockedStopsMotor) {
    // Wechsel in RunningState
    fsm->processEvent(logik::Event::START_PRESSED);
    EXPECT_CALL(*mockDispatcher, lightGreenOn()).Times(Exactly(1));
    fsm->processEvent(logik::Event::START_RELEASED);

    EXPECT_CALL(*mockDispatcher, driveStop()).Times(Exactly(1));
    EXPECT_CALL(*mockDispatcher, lightGreenOff()).Times(Exactly(1));

    fsm->processEvent(logik::Event::END_BLOCKED);
}


// Test: When STOP_PRESSED is processed, motor stops and red light blinks slow
TEST_F(LogikTest, StopPressedStopsMotorAndBlinksRed) {
    EXPECT_CALL(*mockDispatcher, driveStop()).Times(Exactly(1));
    EXPECT_CALL(*mockDispatcher, lightRedBlinkSlow()).Times(Exactly(1));
    fsm->processEvent(logik::Event::STOP_PRESSED);
}

// Test: When ESTOP_PRESSED is processed, safe state is reached and LED Q2 blinks fast
TEST_F(LogikTest, EstopPressedStopsMotorTurnsOffAllAndBlinksQ2) {
    EXPECT_CALL(*mockDispatcher, driveStop()).Times(Exactly(1));
    EXPECT_CALL(*mockDispatcher, turnAllOff()).Times(Exactly(1));
    EXPECT_CALL(*mockDispatcher, lightRedBlinkFast()).Times(Exactly(1));
    EXPECT_CALL(*mockDispatcher, turnLEDQ2BlinkFast()).Times(Exactly(1));
    fsm->processEvent(logik::Event::ESTOP_PRESSED);
}

// Test: When NET_ESTOP_PRESSED is processed, safe state is reached but no LED Q2 blink
TEST_F(LogikTest, NetEstopPressedStopsMotorTurnsOffAll) {
    EXPECT_CALL(*mockDispatcher, driveStop()).Times(Exactly(1));
    EXPECT_CALL(*mockDispatcher, turnAllOff()).Times(Exactly(1));
    EXPECT_CALL(*mockDispatcher, lightRedBlinkFast()).Times(Exactly(1));
    EXPECT_CALL(*mockDispatcher, turnLEDQ2BlinkFast()).Times(0);
    fsm->processEvent(logik::Event::NET_ESTOP_PRESSED);
}

// Test: E-Stop Reset Sequence (UC 004)
TEST_F(LogikTest, EstopResetSequence) {
    // 1. E-Stop
    EXPECT_CALL(*mockDispatcher, driveStop()).Times(Exactly(1));
    EXPECT_CALL(*mockDispatcher, turnAllOff()).Times(Exactly(1));
    EXPECT_CALL(*mockDispatcher, lightRedBlinkFast()).Times(Exactly(1));
    EXPECT_CALL(*mockDispatcher, turnLEDQ2BlinkFast()).Times(Exactly(1));
    fsm->processEvent(logik::Event::ESTOP_PRESSED);

    // 2. Release E-Stop
    EXPECT_CALL(*mockDispatcher, turnLEDQ2Off()).Times(Exactly(1));
    EXPECT_CALL(*mockDispatcher, lightRedOn()).Times(Exactly(1));
    fsm->processEvent(logik::Event::ESTOP_RELEASED);

    // 3. Reset 
    EXPECT_CALL(*mockDispatcher, lightRedOff()).Times(Exactly(1));
    fsm->processEvent(logik::Event::RESET_PRESSED);
}

// Test: When RESET_PRESSED is processed, yellow light blinks
TEST_F(LogikTest, ResetPressedBlinksYellow) {
    EXPECT_CALL(*mockDispatcher, lightYellowBlinkFast()).Times(Exactly(1));
    fsm->processEvent(logik::Event::RESET_PRESSED);
}

