// Hack, um private Methoden (z.B. handleHalPulse) im Test aufrufen zu koennen
#define private public
#include "dispatcher/dispatcher.hpp"
#undef private

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "MockLogik.hpp"
#include "MockActuators.hpp"
#include "MockConnection.hpp"

using ::testing::_;
using ::testing::Exactly;

class DispatcherTest : public ::testing::Test {
protected:
    Dispatcher* dispatcher;
    MockLogik* mockLogik;
    MockActuators* mockActuators;
    MockConnection* mockConnection;

    virtual void SetUp() override {
        dispatcher = new Dispatcher();
        
        mockLogik = new MockLogik();
        mockActuators = new MockActuators();
        mockConnection = new MockConnection();
        
        dispatcher->setLogik(mockLogik);
        dispatcher->setActuators(mockActuators);
        dispatcher->setConnectionManager(mockConnection);
        
        // Initialisierung ueberspringen wir, da sie QNX Channels anlegt (ChannelCreate)
        // Aber wir koennen die State-Variablen manuell setzen falls noetig
    }

    virtual void TearDown() override {
        delete dispatcher;
        delete mockLogik;
        delete mockActuators;
        delete mockConnection;
    }
};

// Test: TASTE START gedrueckt (1) -> Logik erhaelt Event::START_PRESSED
TEST_F(DispatcherTest, StartBtnPressedSendsEventToLogik) {
    // payload: Sensor-ID 0 (BTN_START), State 1
    uint32_t payload = ipc::encodeSensorData(ipc::BTN_START, 1);
    
    EXPECT_CALL(*mockLogik, processEvent(logik::Event::START_PRESSED)).Times(Exactly(1));
    
    dispatcher->handleHalPulse(payload);
}

// Test: TASTE START losgelassen (0) -> Logik erhaelt Event::START_RELEASED
TEST_F(DispatcherTest, StartBtnReleasedSendsEventToLogik) {
    uint32_t payload = ipc::encodeSensorData(ipc::BTN_START, 0);
    
    EXPECT_CALL(*mockLogik, processEvent(logik::Event::START_RELEASED)).Times(Exactly(1));
    
    dispatcher->handleHalPulse(payload);
}

// Test: LS_ENTRY unterbrochen (0 -> Active Low) -> Logik erhaelt Event::ENTRY_BLOCKED
TEST_F(DispatcherTest, LSEntryBlockedSendsEventToLogik) {
    uint32_t payload = ipc::encodeSensorData(ipc::LS_ENTRY, 0); // 0 = Unterbrochen (Active low)
    
    EXPECT_CALL(*mockLogik, processEvent(logik::Event::ENTRY_BLOCKED)).Times(Exactly(1));
    
    dispatcher->handleHalPulse(payload);
}

// Test: LS_END frei (1 -> Active Low) -> Logik erhaelt Event::END_CLEARED
TEST_F(DispatcherTest, LSEndClearedSendsEventToLogik) {
    uint32_t payload = ipc::encodeSensorData(ipc::LS_END, 1); // 1 = Frei (Active low)
    
    EXPECT_CALL(*mockLogik, processEvent(logik::Event::END_CLEARED)).Times(Exactly(1));
    
    dispatcher->handleHalPulse(payload);
}
