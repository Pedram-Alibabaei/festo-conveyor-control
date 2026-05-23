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

// Test: When START_RELEASED event is processed, lightGreenOff should be called
TEST_F(LogikTest, StartReleasedTurnsGreenOff) {
    EXPECT_CALL(*mockDispatcher, lightGreenOff()).Times(Exactly(1));

    fsm->processEvent(logik::Event::START_RELEASED);
}

// Test: When ENTRY_BLOCKED detects an item, motor drives right and green blinks slow
TEST_F(LogikTest, EntryBlockedStartsMotorRightAndBlinksSlow) {
    EXPECT_CALL(*mockDispatcher, driveRight()).Times(Exactly(1));
    EXPECT_CALL(*mockDispatcher, lightGreenBlinkSlow()).Times(Exactly(1));

    fsm->processEvent(logik::Event::ENTRY_BLOCKED);
}

// Test: When END_BLOCKED is processed, motor stops
TEST_F(LogikTest, EndBlockedStopsMotor) {
    EXPECT_CALL(*mockDispatcher, driveStop()).Times(Exactly(1));
    EXPECT_CALL(*mockDispatcher, lightGreenOff()).Times(Exactly(1));

    fsm->processEvent(logik::Event::END_BLOCKED);
}
