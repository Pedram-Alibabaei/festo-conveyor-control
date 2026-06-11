/**
 * Standalone-Hardware-Test fuer ProfileAnalyzer (Kalibrierung + Typerkennung).
 *
 * Bauen:  make profile_analyzer_test
 *
 * Ablauf Transport:
 *   ENTRY_BLOCKED -> Band NORMAL -> nach 2 s LANGSAM -> HEIGHT_CLEARED/Typ -> NORMAL
 *   -> Stopp bei Rutsche oder Bandende
 *
 * Auf der Anlage (root / I/O-Rechte):
 *   ./build/armv7le-debug/profile_analyzer_test
 *   ./build/armv7le-debug/profile_analyzer_test --end   (Weiche offen -> Bandende)
 */

#include <chrono>
#include <condition_variable>
#include <csignal>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>

#include <hal/hal_sensors.hpp>
#include <hal/output_driver/motor_controller.hpp>
#include <hal/output_driver/ejector_controller.hpp>
#include <shared/ipc_definitions.hpp>
#include <shared/types.hpp>

extern "C" {
#include <sys/dispatch.h>
#include <sys/neutrino.h>
}

namespace
{
constexpr auto kMotorStartDelay = std::chrono::seconds(2);
constexpr auto kRunTimeout = std::chrono::seconds(120);

struct TestState {
    std::mutex mtx;
    std::condition_variable cv;

    bool running = true;
    bool calibEmptyBelt = false;
    bool calibFlatWp = false;
    bool calibMinSamples = false;

    bool heightBlocked = false;
    bool heightError = false;
    bool heightServiceTiming = false;
    bool wpTypeReceived = false;
    WpType lastType = WpType::UNKNOWN;

    bool chuteBlocked = false;
    bool endBlocked = false;
    bool entryBlocked = false;

    uint16_t emptyBeltAdc = 0;
    uint16_t flatCenterAdc = 0;
    uint32_t minSamples = 0;
};

hal::output_driver::MotorController* g_motor = nullptr;

const char* wpTypeName(WpType type)
{
    switch (type) {
    case WpType::FLAT: return "FLAT (Flach)";
    case WpType::TALL: return "TALL (Hoch)";
    case WpType::TALL_DRILLED: return "TALL_DRILLED (Gebohrt)";
    case WpType::TALL_DRILLED_METAL: return "TALL_DRILLED_METAL";
    case WpType::TALL_METAL: return "TALL_METAL";
    default: return "UNKNOWN";
    }
}

void notify(TestState& state)
{
    state.cv.notify_all();
}

void handleHalPulse(uint32_t payload, TestState& state)
{
    const uint16_t sensorId = ipc::decodeSensorID(payload);
    const uint16_t sensorState = ipc::decodeSensorState(payload);

    std::lock_guard<std::mutex> lock(state.mtx);

    if (sensorId == ipc::HEIGHT) {
        if (sensorState == ipc::State::BLOCKED) {
            state.heightBlocked = true;
            std::cout << "[Test] HAL: HEIGHT_BLOCKED (Messung startet)\n";
        } else if (sensorState == ipc::State::CLEARED) {
            state.heightServiceTiming = true;
            std::cout << "[Test] HAL: HEIGHT_CLEARED -> Band NORMAL\n";
        } else if (sensorState == ipc::State::HIGH_ERROR) {
            state.heightError = true;
            std::cout << "[Test] HAL: HEIGHT_ERROR (WP zu hoch / verkehrt)\n";
        } else {
            state.lastType = static_cast<WpType>(sensorState);
            state.wpTypeReceived = true;
            std::cout << "[Test] HAL: WP-Typ erkannt -> "
                      << wpTypeName(state.lastType)
                      << " (ID=" << static_cast<int>(sensorState)
                      << ") -> Band NORMAL\n";
        }
    } else if (sensorId == ipc::LS_CHUTE && sensorState == ipc::State::BLOCKED) {
        state.chuteBlocked = true;
        std::cout << "[Test] HAL: CHUTE_BLOCKED (WP in Rutsche)\n";
    } else if (sensorId == ipc::LS_END && sensorState == ipc::State::BLOCKED) {
        state.endBlocked = true;
        std::cout << "[Test] HAL: END_BLOCKED (WP am Bandende)\n";
    } else if (sensorId == ipc::LS_ENTRY && sensorState == ipc::State::BLOCKED) {
        state.entryBlocked = true;
        std::cout << "[Test] HAL: ENTRY_BLOCKED (Einlauf)\n";
    }

    notify(state);
}

void handleCalibPulse(uint32_t payload, TestState& state)
{
    uint16_t value = 0;
    uint16_t mode = 0;
    ipc::decodeCalibPayload(payload, value, mode);

    std::lock_guard<std::mutex> lock(state.mtx);

    switch (static_cast<ipc::CalibMode>(mode)) {
    case ipc::CalibMode::EMPTY_BELT:
        state.emptyBeltAdc = value;
        state.calibEmptyBelt = true;
        std::cout << "[Test] Kalibrierung: EMPTY_BELT = " << value << "\n";
        break;
    case ipc::CalibMode::FLAT_WP:
        state.flatCenterAdc = value;
        state.calibFlatWp = true;
        std::cout << "[Test] Kalibrierung: FLAT_WP = " << value << "\n";
        break;
    case ipc::CalibMode::MIN_SAMPLES:
        state.minSamples = value;
        state.calibMinSamples = true;
        std::cout << "[Test] Kalibrierung: MIN_SAMPLES = " << value << "\n";
        break;
    default:
        break;
    }

    notify(state);
}

void pulseReceiverLoop(int chid, TestState& state)
{
    struct _pulse pulse {};

    while (state.running) {
        const int rcvid = MsgReceivePulse(chid, &pulse, sizeof(pulse), nullptr);
        if (rcvid == -1) {
            if (errno == EINTR) {
                continue;
            }
            break;
        }

        if (rcvid != 0) {
            continue;
        }

        switch (pulse.code) {
        case ipc::HAL_INPUT:
            handleHalPulse(static_cast<uint32_t>(pulse.value.sival_int), state);
            break;
        case ipc::HAL_ADC_CALIBRATION:
            handleCalibPulse(static_cast<uint32_t>(pulse.value.sival_int), state);
            break;
        default:
            break;
        }
    }
}

void emergencyStop(int)
{
    if (g_motor != nullptr) {
        g_motor->stop();
    }
    std::_Exit(EXIT_FAILURE);
}

bool waitFlag(TestState& state, const char* label,
              const std::function<bool(const TestState&)>& pred,
              std::chrono::milliseconds timeout)
{
    std::unique_lock<std::mutex> lock(state.mtx);
    const bool ok = state.cv.wait_for(lock, timeout, [&] {
        return !state.running || pred(state);
    });

    if (!ok) {
        std::cerr << "[Test] TIMEOUT: " << label << std::endl;
        return false;
    }
    return true;
}

void resetRunFlags(TestState& state)
{
    std::lock_guard<std::mutex> lock(state.mtx);
    state.heightBlocked = false;
    state.heightError = false;
    state.heightServiceTiming = false;
    state.wpTypeReceived = false;
    state.lastType = WpType::UNKNOWN;
    state.chuteBlocked = false;
    state.endBlocked = false;
    state.entryBlocked = false;
}

enum class BeltPhase {
    WaitingEntry,
    NormalAfterEntry,
    SlowForHeight,
    NormalAfterHeight
};

bool runTransportCycle(hal::HalSensors& sensors,
                       hal::output_driver::MotorController& motor,
                       hal::output_driver::EjectorController* ejector,
                       TestState& state,
                       bool expectType,
                       bool routeToEnd)
{
    resetRunFlags(state);
    sensors.setAdcWaitigForWpState();

    if (routeToEnd && ejector != nullptr) {
        if (ejector->isSwitch()) {
            ejector->openSwitch();
            std::cout << "[Test] Weiche geoeffnet (-> Bandende)\n";
        } else {
            std::cout << "[Test] Auswerfer: kein Push (-> Bandende)\n";
        }
    }

    std::cout << "\n--- Werkstueck am Einlauf ablegen (warte auf ENTRY_BLOCKED) ---\n";

    if (!waitFlag(state, "ENTRY_BLOCKED",
                  [](const TestState& s) { return s.entryBlocked; },
                  kRunTimeout)) {
        return false;
    }

    motor.driveRight();
    const auto entryTime = std::chrono::steady_clock::now();
    auto phase = BeltPhase::NormalAfterEntry;
    bool switchedToSlow = false;
    bool switchedToNormalAfterHeight = false;

    std::cout << "[Test] Band NORMAL, in " << kMotorStartDelay.count()
              << " s -> LANGSAM (Hoehenmessung)\n";

    const auto deadline = std::chrono::steady_clock::now() + kRunTimeout;
    bool typeOk = !expectType;
    bool destinationOk = false;

    while (std::chrono::steady_clock::now() < deadline) {
        const auto now = std::chrono::steady_clock::now();

        if (!switchedToSlow &&
            phase == BeltPhase::NormalAfterEntry &&
            now - entryTime >= kMotorStartDelay) {
            motor.driveSlowRight();
            switchedToSlow = true;
            phase = BeltPhase::SlowForHeight;
            std::cout << "[Test] Band LANGSAM (Hoehenmessung)\n";
        }

        std::unique_lock<std::mutex> lock(state.mtx);
        state.cv.wait_for(lock, std::chrono::milliseconds(50), [&] {
            return state.heightError || state.wpTypeReceived ||
                   state.heightServiceTiming ||
                   state.chuteBlocked || state.endBlocked;
        });

        if (state.heightError) {
            lock.unlock();
            break;
        }

        if (!switchedToNormalAfterHeight &&
            (state.heightServiceTiming || state.wpTypeReceived)) {
            motor.driveRight();
            switchedToNormalAfterHeight = true;
            phase = BeltPhase::NormalAfterHeight;
        }

        if (expectType && state.wpTypeReceived) {
            typeOk = true;
        }
        if (!expectType && state.heightServiceTiming) {
            typeOk = true;
        }
        if (state.chuteBlocked || state.endBlocked) {
            destinationOk = true;
        }

        lock.unlock();

        if (typeOk && destinationOk) {
            break;
        }
    }

    motor.stop();

    if (routeToEnd && ejector != nullptr && ejector->isSwitch()) {
        ejector->closeSwitch();
    }

    std::cout << "\n========== LAUF-ERGEBNIS ==========\n";
    if (state.heightError) {
        std::cout << "  Ergebnis: HEIGHT_ERROR\n";
        return false;
    }
    if (expectType) {
        std::cout << "  Erkannter Typ: " << wpTypeName(state.lastType) << "\n";
    } else {
        std::cout << "  Service-Timing: HEIGHT_CLEARED (kein Typ erwartet)\n";
    }
    if (state.chuteBlocked) {
        std::cout << "  Ziel: RUTSCHE\n";
    } else if (state.endBlocked) {
        std::cout << "  Ziel: BANDENDE\n";
    } else {
        std::cout << "  Ziel: UNBEKANNT (Timeout / kein Sensor)\n";
        return false;
    }
    std::cout << "===================================\n\n";

    return typeOk && destinationOk;
}

bool runCalibration(hal::HalSensors& sensors,
                    hal::output_driver::MotorController& motor,
                    TestState& state)
{
    std::cout << "\n========================================\n"
              << " Phase 1: ADC-Kalibrierung\n"
              << "========================================\n"
              << " Schritt 1: Band LEER lassen\n";

    state.calibEmptyBelt = false;
    state.calibFlatWp = false;
    state.calibMinSamples = false;

    sensors.setAdcCalibrationsState();

    if (!waitFlag(state, "EMPTY_BELT",
                  [](const TestState& s) { return s.calibEmptyBelt; },
                  std::chrono::seconds(30))) {
        return false;
    }

    std::cout << "\n Schritt 2: FLACHES Referenz-WP am Einlauf ablegen\n";

    resetRunFlags(state);
    if (!waitFlag(state, "ENTRY_BLOCKED (Kalibrierung)",
                  [](const TestState& s) { return s.entryBlocked; },
                  kRunTimeout)) {
        return false;
    }

    motor.driveRight();
    const auto entryTime = std::chrono::steady_clock::now();
    bool switchedToSlow = false;

    const auto deadline = std::chrono::steady_clock::now() + kRunTimeout;
    bool calibOk = false;

    while (std::chrono::steady_clock::now() < deadline) {
        if (!switchedToSlow &&
            std::chrono::steady_clock::now() - entryTime >= kMotorStartDelay) {
            motor.driveSlowRight();
            switchedToSlow = true;
            std::cout << "[Test] Kalibrierung: Band LANGSAM\n";
        }

        std::unique_lock<std::mutex> lock(state.mtx);
        if (state.cv.wait_for(lock, std::chrono::milliseconds(50), [&] {
                return state.calibMinSamples;
            })) {
            calibOk = state.calibMinSamples;
        }
        lock.unlock();

        if (calibOk) {
            break;
        }
    }

    motor.stop();

    if (!calibOk) {
        std::cerr << "[Test] TIMEOUT: Kalibrierung MIN_SAMPLES\n";
        return false;
    }

    std::cout << "\n>>> Kalibrierung OK <<<\n"
              << "  EMPTY_BELT  : " << state.emptyBeltAdc << "\n"
              << "  FLAT_WP     : " << state.flatCenterAdc << "\n"
              << "  MIN_SAMPLES : " << state.minSamples << "\n\n";

    return true;
}

void printUsage(const char* prog)
{
    std::cout << "Usage: " << prog << " [--end]\n"
              << "  --end   Weiche oeffnen -> WP faehrt zum Bandende (sonst Rutsche)\n";
}

} // namespace

int main(int argc, char* argv[])
{
    bool routeToEnd = false;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--end" || arg == "-e") {
            routeToEnd = true;
        } else if (arg == "-h" || arg == "--help") {
            printUsage(argv[0]);
            return EXIT_SUCCESS;
        } else {
            std::cerr << "Unbekanntes Argument: " << arg << std::endl;
            printUsage(argv[0]);
            return EXIT_FAILURE;
        }
    }

    if (ThreadCtl(_NTO_TCTL_IO, 0) == -1) {
        std::cerr << "FEHLER: Keine I/O-Rechte (als root starten?)." << std::endl;
        return EXIT_FAILURE;
    }

    std::signal(SIGINT, emergencyStop);
    std::signal(SIGTERM, emergencyStop);

    const int chid = ChannelCreate(0);
    if (chid == -1) {
        std::cerr << "FEHLER: ChannelCreate" << std::endl;
        return EXIT_FAILURE;
    }

    const int coid = ConnectAttach(0, 0, chid, _NTO_SIDE_CHANNEL, 0);
    if (coid == -1) {
        std::cerr << "FEHLER: ConnectAttach" << std::endl;
        ChannelDestroy(chid);
        return EXIT_FAILURE;
    }

    TestState state;
    hal::HalSensors sensors;
    hal::output_driver::MotorController motor;
    hal::output_driver::EjectorController ejector;

    g_motor = &motor;

    sensors.init(coid);

    std::thread receiver(pulseReceiverLoop, chid, std::ref(state));

    std::cout << "========================================\n"
              << " ProfileAnalyzer Hardware-Test\n"
              << "========================================\n"
              << " Ziel Bandende: " << (routeToEnd ? "ja (--end)" : "nein (Rutsche)") << "\n\n";

    if (!runCalibration(sensors, motor, state)) {
        std::cerr << "Kalibrierung fehlgeschlagen.\n";
        state.running = false;
        if (receiver.joinable()) {
            receiver.join();
        }
        ConnectDetach(coid);
        ChannelDestroy(chid);
        return EXIT_FAILURE;
    }

    std::cout << "========================================\n"
              << " Phase 2: Service-Timing (1. Lauf nach Kalibrierung)\n"
              << "========================================\n"
              << " Legen Sie das flache Referenz-WP erneut auf (oder irgendein WP).\n"
              << " Es wird KEIN Typ erwartet – nur Timing.\n";

    if (!runTransportCycle(sensors, motor, &ejector, state, false, routeToEnd)) {
        std::cerr << "Service-Timing-Lauf unvollstaendig.\n";
    }

    int testNumber = 1;
    while (true) {
        std::cout << "========================================\n"
                  << " Phase 3: Typerkennung #" << testNumber << "\n"
                  << "========================================\n"
                  << " WP am Einlauf ablegen (automatischer Start).\n"
                  << " Vorher 'q' + Enter zum Beenden.\n";

        std::cout << "Enter = weiter, q = Beenden: ";
        std::string line;
        if (!std::getline(std::cin, line) || line == "q" || line == "Q") {
            break;
        }

        if (!runTransportCycle(sensors, motor, &ejector, state, true, routeToEnd)) {
            std::cerr << "Test #" << testNumber << " fehlgeschlagen oder unklar.\n";
        } else {
            std::cout << "Test #" << testNumber << " OK.\n";
        }

        ++testNumber;
    }

    motor.stop();
    sensors.stopInterrupts();
    state.running = false;

    if (receiver.joinable()) {
        receiver.join();
    }

    ConnectDetach(coid);
    ChannelDestroy(chid);

    std::cout << "ProfileAnalyzer-Test beendet.\n";
    return EXIT_SUCCESS;
}
