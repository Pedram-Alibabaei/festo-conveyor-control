/**
 * Unit-Tests fuer Dispatcher-Pulse-Routing (HAL -> Logik, Netzwerk -> Logik).
 *
 * Bauen:  make dispatcher_tests
 * Target: ./build/armv7le-debug/dispatcher_tests
 *
 * Pulse-Matrix (Hardware morgen pruefen):
 *   HAL_INPUT   -> Taster, Lichtschranken, Metal, Hoehensensor-Typ
 *   HAL_ESTOP   -> Not-Aus (invertierte Active-Low Logik)
 *   PARTNER_MSG -> Partner-START/RESET, REQ/READY, Rutsche
 *   PARTNER_ESTOP -> Partner Not-Aus
 *   BELT_TICK   -> Logik::takt()
 *   WP_INFO     -> workpieceInfo(id, typ)
 *   UPDATE_LIST -> updateWpListe(typ)
 *   HAL_ADC_CALIBRATION -> setLiveAdcCalibration()
 */

#define private public
#include "dispatcher/dispatcher.hpp"
#undef private

#include <gtest/gtest.h>

#include "FakeConnection.hpp"
#include "FakeLogik.hpp"
#include "shared/ipc_definitions.hpp"

namespace {

uint32_t halPayload(ipc::SensorID sensor, ipc::State state) {
    return ipc::encodeHalPayload(static_cast<uint16_t>(sensor), static_cast<uint16_t>(state));
}

uint32_t netPayload(uint16_t messageId, uint16_t state) {
    return ipc::encodeHalPayload(messageId, state);
}

} // namespace

class DispatcherPulseTest : public ::testing::Test {
protected:
    Dispatcher* dispatcher = nullptr;
    FakeLogik* fakeLogik = nullptr;
    FakeConnection* fakeConnection = nullptr;

    void SetUp() override {
        dispatcher = new Dispatcher();
        fakeLogik = new FakeLogik();
        fakeConnection = new FakeConnection();
        dispatcher->setLogik(fakeLogik);
        dispatcher->setConnectionManager(fakeConnection);
        dispatcher->m_partnerCoid = -1;
    }

    void TearDown() override {
        delete dispatcher;
        delete fakeLogik;
        delete fakeConnection;
    }

    void sendHal(ipc::SensorID sensor, ipc::State state) {
        dispatcher->handleHalPulse(halPayload(sensor, state));
    }

    void sendEStop(ipc::State state) {
        dispatcher->handleHalEStopPulse(halPayload(ipc::BTN_ESTOP, state));
    }

    void sendNetwork(uint16_t messageId, uint16_t state) {
        dispatcher->handleNetworkPulse(netPayload(messageId, state));
    }

    void sendPulse(int code, uint32_t value = 0) {
        struct _pulse pulse {};
        pulse.code = static_cast<int8_t>(code);
        pulse.value.sival_int = static_cast<int>(value);
        dispatcher->handlePulse(pulse);
    }
};

// ============================================================================
// HAL – Taster
// ============================================================================

TEST_F(DispatcherPulseTest, StartPressed_ForwardsToLogik) {
    sendHal(ipc::BTN_START, ipc::State::PRESSED);
    EXPECT_TRUE(fakeLogik->hadEvent(logik::Event::START_PRESSED));
}

TEST_F(DispatcherPulseTest, StartReleased_ForwardsToLogik) {
    sendHal(ipc::BTN_START, ipc::State::RELEASED);
    EXPECT_TRUE(fakeLogik->hadEvent(logik::Event::START_RELEASED));
}

TEST_F(DispatcherPulseTest, StopPressed_ForwardsToLogik) {
    sendHal(ipc::BTN_STOP, ipc::State::PRESSED);
    EXPECT_TRUE(fakeLogik->hadEvent(logik::Event::STOP_PRESSED));
}

TEST_F(DispatcherPulseTest, StopReleased_ForwardsToLogik) {
    sendHal(ipc::BTN_STOP, ipc::State::RELEASED);
    EXPECT_TRUE(fakeLogik->hadEvent(logik::Event::STOP_RELEASED));
}

TEST_F(DispatcherPulseTest, ResetPressed_ForwardsToLogik) {
    sendHal(ipc::BTN_RESET, ipc::State::PRESSED);
    EXPECT_TRUE(fakeLogik->hadEvent(logik::Event::RESET_PRESSED));
}

TEST_F(DispatcherPulseTest, ResetReleased_ForwardsToLogik) {
    sendHal(ipc::BTN_RESET, ipc::State::RELEASED);
    EXPECT_TRUE(fakeLogik->hadEvent(logik::Event::RESET_RELEASED));
}

// ============================================================================
// HAL – Not-Aus (eigener Pulse-Code, GPIO Active-Low)
// ============================================================================

TEST_F(DispatcherPulseTest, EstopReleased_ForwardsAsEstopPressed) {
    sendEStop(ipc::State::RELEASED);
    EXPECT_TRUE(fakeLogik->hadEvent(logik::Event::ESTOP_PRESSED));
}

TEST_F(DispatcherPulseTest, EstopPressed_ForwardsAsEstopReleased) {
    sendEStop(ipc::State::PRESSED);
    EXPECT_TRUE(fakeLogik->hadEvent(logik::Event::ESTOP_RELEASED));
}

// ============================================================================
// HAL – Lichtschranken & Sensoren
// ============================================================================

TEST_F(DispatcherPulseTest, EntryBlocked_ForwardsToLogik) {
    sendHal(ipc::LS_ENTRY, ipc::State::BLOCKED);
    EXPECT_TRUE(fakeLogik->hadEvent(logik::Event::ENTRY_BLOCKED));
}

TEST_F(DispatcherPulseTest, EntryCleared_ForwardsToLogik) {
    sendHal(ipc::LS_ENTRY, ipc::State::CLEARED);
    EXPECT_TRUE(fakeLogik->hadEvent(logik::Event::ENTRY_CLEARED));
}

TEST_F(DispatcherPulseTest, HeightBlocked_ForwardsToLogik) {
    sendHal(ipc::HEIGHT, ipc::State::BLOCKED);
    EXPECT_TRUE(fakeLogik->hadEvent(logik::Event::HEIGHT_BLOCKED));
}

TEST_F(DispatcherPulseTest, HeightCleared_ForwardsToLogik) {
    sendHal(ipc::HEIGHT, ipc::State::CLEARED);
    EXPECT_TRUE(fakeLogik->hadEvent(logik::Event::HEIGHT_CLEARED));
}

TEST_F(DispatcherPulseTest, HeightError_ForwardsToLogik) {
    sendHal(ipc::HEIGHT, ipc::State::HIGH_ERROR);
    EXPECT_TRUE(fakeLogik->hadEvent(logik::Event::HEIGHT_ERROR));
}

TEST_F(DispatcherPulseTest, HeightWpType_ForwardsToWorkpieceTypeFromHS) {
    sendHal(ipc::HEIGHT, static_cast<ipc::State>(WpType::FLAT));
    ASSERT_EQ(fakeLogik->hsTypes.size(), 1u);
    EXPECT_EQ(fakeLogik->hsTypes[0], WpType::FLAT);
}

TEST_F(DispatcherPulseTest, MetalBlocked_ForwardsToLogik) {
    sendHal(ipc::SENSOR_METAL, ipc::State::BLOCKED);
    EXPECT_TRUE(fakeLogik->hadEvent(logik::Event::METAL_DETECTED));
}

TEST_F(DispatcherPulseTest, MetalCleared_ForwardsToLogik) {
    sendHal(ipc::SENSOR_METAL, ipc::State::CLEARED);
    EXPECT_TRUE(fakeLogik->hadEvent(logik::Event::METAL_CLEARED));
}

TEST_F(DispatcherPulseTest, EjectorBlocked_ForwardsToLogik) {
    sendHal(ipc::LS_SWITCH, ipc::State::BLOCKED);
    EXPECT_TRUE(fakeLogik->hadEvent(logik::Event::EJECTOR_BLOCKED));
}

TEST_F(DispatcherPulseTest, EjectorCleared_ForwardsToLogik) {
    sendHal(ipc::LS_SWITCH, ipc::State::CLEARED);
    EXPECT_TRUE(fakeLogik->hadEvent(logik::Event::EJECTOR_CLEARED));
}

TEST_F(DispatcherPulseTest, EndBlocked_ForwardsToLogik) {
    sendHal(ipc::LS_END, ipc::State::BLOCKED);
    EXPECT_TRUE(fakeLogik->hadEvent(logik::Event::END_BLOCKED));
}

TEST_F(DispatcherPulseTest, EndCleared_ForwardsToLogik) {
    sendHal(ipc::LS_END, ipc::State::CLEARED);
    EXPECT_TRUE(fakeLogik->hadEvent(logik::Event::END_CLEARED));
}

TEST_F(DispatcherPulseTest, ChuteBlocked_ForwardsToLogik) {
    sendHal(ipc::LS_CHUTE, ipc::State::BLOCKED);
    EXPECT_TRUE(fakeLogik->hadEvent(logik::Event::CHUTE_BLOCKED));
}

TEST_F(DispatcherPulseTest, ChuteCleared_ForwardsToLogik) {
    sendHal(ipc::LS_CHUTE, ipc::State::CLEARED);
    EXPECT_TRUE(fakeLogik->hadEvent(logik::Event::CHUTE_CLEARED));
}

// ============================================================================
// Netzwerk – Partner-Taster & Uebergabe
// ============================================================================

TEST_F(DispatcherPulseTest, PartnerStartPressed_ForwardsToLogik) {
    sendNetwork(ipc::BTN_START, ipc::State::PRESSED);
    EXPECT_TRUE(fakeLogik->hadEvent(logik::Event::PARTNER_START_PRESSED));
}

TEST_F(DispatcherPulseTest, PartnerStartReleased_ForwardsToLogik) {
    sendNetwork(ipc::BTN_START, ipc::State::RELEASED);
    EXPECT_TRUE(fakeLogik->hadEvent(logik::Event::PARTNER_START_RELEASED));
}

TEST_F(DispatcherPulseTest, PartnerResetPressed_ForwardsToLogik) {
    sendNetwork(ipc::BTN_RESET, ipc::State::PRESSED);
    EXPECT_TRUE(fakeLogik->hadEvent(logik::Event::PARTNER_RESET_PRESSED));
}

TEST_F(DispatcherPulseTest, PartnerResetReleased_ForwardsToLogik) {
    sendNetwork(ipc::BTN_RESET, ipc::State::RELEASED);
    EXPECT_TRUE(fakeLogik->hadEvent(logik::Event::PARTNER_RESET_RELEASED));
}

TEST_F(DispatcherPulseTest, ReqReceive_ForwardsToLogik) {
    sendNetwork(ipc::NetworkMessage::REQ_RECEIVE, 0);
    EXPECT_TRUE(fakeLogik->hadEvent(logik::Event::REQ_TO_GIVE_WP));
}

TEST_F(DispatcherPulseTest, ReadyReceive_ForwardsToLogik) {
    sendNetwork(ipc::NetworkMessage::READY_RECEIVE, 0);
    EXPECT_TRUE(fakeLogik->hadEvent(logik::Event::READY_TO_RECEIVE_WP));
}

TEST_F(DispatcherPulseTest, PartnerChuteFull_UpdatesLogik) {
    sendNetwork(ipc::NetworkMessage::CHUTE, ipc::State::VOLL);
    EXPECT_TRUE(fakeLogik->partnerChuteSet);
    EXPECT_TRUE(fakeLogik->partnerChuteFull);
}

TEST_F(DispatcherPulseTest, PartnerChuteEmpty_UpdatesLogik) {
    sendNetwork(ipc::NetworkMessage::CHUTE, ipc::State::EMPTY);
    EXPECT_TRUE(fakeLogik->partnerChuteSet);
    EXPECT_FALSE(fakeLogik->partnerChuteFull);
}

TEST_F(DispatcherPulseTest, PartnerEstopReleased_ForwardsAsPartnerEstopPressed) {
    dispatcher->handlePartnerEStopPulse(halPayload(ipc::BTN_ESTOP, ipc::State::RELEASED));
    EXPECT_TRUE(fakeLogik->hadEvent(logik::Event::PARTNER_ESTOP_PRESSED));
}

TEST_F(DispatcherPulseTest, PartnerEstopPressed_ForwardsAsPartnerEstopReleased) {
    dispatcher->handlePartnerEStopPulse(halPayload(ipc::BTN_ESTOP, ipc::State::PRESSED));
    EXPECT_TRUE(fakeLogik->hadEvent(logik::Event::PARTNER_ESTOP_RELEASED));
}

// ============================================================================
// System-Pulse (handlePulse)
// ============================================================================

TEST_F(DispatcherPulseTest, BeltTick_CallsLogikTakt) {
    sendPulse(ipc::BELT_TICK);
    EXPECT_EQ(fakeLogik->taktCount, 1);
}

TEST_F(DispatcherPulseTest, Heartbeat_CallsPartnerAlive) {
    sendPulse(ipc::HEARTBEAT);
    EXPECT_EQ(fakeConnection->partnerAliveTicks, 1);
}

TEST_F(DispatcherPulseTest, HeartbeatTakt_CallsConnectionHeartbeat) {
    sendPulse(ipc::HEARTBEATTACKT);
    EXPECT_EQ(fakeConnection->heartbeatTicks, 1);
}

TEST_F(DispatcherPulseTest, WpInfo_ForwardsIdAndType) {
    const uint32_t payload = (static_cast<uint32_t>(7) << 16) | static_cast<uint32_t>(WpType::TALL);
    sendPulse(ipc::WP_INFO, payload);
    ASSERT_EQ(fakeLogik->wpInfos.size(), 1u);
    EXPECT_EQ(fakeLogik->wpInfos[0].first, 7);
    EXPECT_EQ(fakeLogik->wpInfos[0].second, WpType::TALL);
}

TEST_F(DispatcherPulseTest, UpdateList_ForwardsWpType) {
    sendPulse(ipc::UPDATE_LIST, static_cast<uint32_t>(WpType::TALL_DRILLED));
    ASSERT_EQ(fakeLogik->wpListUpdates.size(), 1u);
    EXPECT_EQ(fakeLogik->wpListUpdates[0], WpType::TALL_DRILLED);
}

TEST_F(DispatcherPulseTest, PartnerEstopPressed_SetsPartnerEstopFlag) {
    dispatcher->handlePartnerEStopPulse(halPayload(ipc::BTN_ESTOP, ipc::State::RELEASED));
    EXPECT_TRUE(dispatcher->isPartnerEstopPressed());
}

TEST_F(DispatcherPulseTest, PartnerEstopReleased_ClearsPartnerEstopFlag) {
    dispatcher->handlePartnerEStopPulse(halPayload(ipc::BTN_ESTOP, ipc::State::RELEASED));
    dispatcher->handlePartnerEStopPulse(halPayload(ipc::BTN_ESTOP, ipc::State::PRESSED));
    EXPECT_FALSE(dispatcher->isPartnerEstopPressed());
}

TEST_F(DispatcherPulseTest, IsEstopPressed_WithoutSensors_ReturnsFalse) {
    EXPECT_FALSE(dispatcher->isEstopPressed());
}

TEST_F(DispatcherPulseTest, AdcCalibrationMinSamples_ForwardsLiveAdc) {
    Dispatcher::s_liveEmptyBelt = 700;
    Dispatcher::s_liveFlatCenter = 1100;

    const uint32_t payload = ipc::encodeCalibPayload(
        25, static_cast<uint16_t>(ipc::CalibMode::MIN_SAMPLES));
    sendPulse(ipc::HAL_ADC_CALIBRATION, payload);

    EXPECT_TRUE(fakeLogik->liveAdcSet);
    EXPECT_EQ(fakeLogik->liveEmptyBelt, 700);
    EXPECT_EQ(fakeLogik->liveFlatCenter, 1100);
    EXPECT_EQ(fakeLogik->liveMinSamples, 25u);
}
