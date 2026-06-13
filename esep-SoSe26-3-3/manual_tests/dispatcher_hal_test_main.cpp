/**
 * Hardware-Test: HAL -> Dispatcher -> Logik (nur Logging, keine FSM).
 *
 * Bauen:  make dispatcher_hal_test
 *
 * Auf der Anlage (root / I/O-Rechte):
 *   ./build/armv7le-debug/dispatcher_hal_test
 *   ./build/armv7le-debug/dispatcher_hal_test -c    (Anlage 2)
 *
 * Erwartete Kette pro Signal:
 *   [GPIO] ...  ->  [Dispatcher] ...  ->  [HAL-Test -> Logik] ...
 *
 * Checkliste (jede Zeile einmal ausloesen):
 *   Taster:  START, STOP, RESET, ESTOP (druecken + loslassen)
 *   LS:      ENTRY, SWITCH/Weiche, END, CHUTE
 *   Sensor:  METAL
 *   Hoehe:   Werkstueck unter ADC (HEIGHT BLOCKED/CLEARED/Typ)
 */

#include <csignal>
#include <cstdlib>
#include <iostream>
#include <string>
#include <thread>

#include "dispatcher/dispatcher.hpp"
#include "hal/hal_sensors.hpp"
#include "logik/i_logik.hpp"
#include "shared/ipc_definitions.hpp"
#include "shared/logikEvent.hpp"
#include "shared/types.hpp"

extern "C" {
#include <sys/neutrino.h>
}

namespace {

const char* eventName(logik::Event e) {
    switch (e) {
    case logik::Event::ENTRY_BLOCKED: return "ENTRY_BLOCKED";
    case logik::Event::ENTRY_CLEARED: return "ENTRY_CLEARED";
    case logik::Event::HEIGHT_BLOCKED: return "HEIGHT_BLOCKED";
    case logik::Event::HEIGHT_CLEARED: return "HEIGHT_CLEARED";
    case logik::Event::HEIGHT_ERROR: return "HEIGHT_ERROR";
    case logik::Event::EJECTOR_BLOCKED: return "EJECTOR_BLOCKED";
    case logik::Event::EJECTOR_CLEARED: return "EJECTOR_CLEARED";
    case logik::Event::METAL_DETECTED: return "METAL_DETECTED";
    case logik::Event::METAL_CLEARED: return "METAL_CLEARED";
    case logik::Event::END_BLOCKED: return "END_BLOCKED";
    case logik::Event::END_CLEARED: return "END_CLEARED";
    case logik::Event::CHUTE_BLOCKED: return "CHUTE_BLOCKED";
    case logik::Event::CHUTE_CLEARED: return "CHUTE_CLEARED";
    case logik::Event::START_PRESSED: return "START_PRESSED";
    case logik::Event::START_RELEASED: return "START_RELEASED";
    case logik::Event::STOP_PRESSED: return "STOP_PRESSED";
    case logik::Event::STOP_RELEASED: return "STOP_RELEASED";
    case logik::Event::RESET_PRESSED: return "RESET_PRESSED";
    case logik::Event::RESET_RELEASED: return "RESET_RELEASED";
    case logik::Event::ESTOP_PRESSED: return "ESTOP_PRESSED";
    case logik::Event::ESTOP_RELEASED: return "ESTOP_RELEASED";
    default: return "OTHER";
    }
}

const char* wpTypeName(WpType type) {
    switch (type) {
    case WpType::FLAT: return "FLAT";
    case WpType::TALL: return "TALL";
    case WpType::TALL_DRILLED: return "TALL_DRILLED";
    case WpType::TALL_DRILLED_METAL: return "TALL_DRILLED_METAL";
    case WpType::TALL_METAL: return "TALL_METAL";
    default: return "UNKNOWN";
    }
}

class LoggingLogik : public i_Logik {
public:
    void init(AnlagenPosition) override {}

    void setCalibrationManager(I_CalibrationManager*) override {}

    void processEvent(logik::Event e) override {
        std::cout << "[HAL-Test -> Logik] Event: " << eventName(e)
                  << " (" << static_cast<int>(e) << ")" << std::endl;
    }

    void workpieceTypeFromHS(WpType type) override {
        std::cout << "[HAL-Test -> Logik] Hoehensensor-Typ: "
                  << wpTypeName(type) << " (" << static_cast<int>(type) << ")" << std::endl;
    }

    void workpieceInfo(int, WpType) override {}
    void updateWpListe(WpType) override {}
    void takt() override {}
    void setPartnerChuteStatus(bool) override {}
    void setLiveAdcCalibration(uint16_t emptyBelt, uint16_t flatCenter, uint32_t minSamples) override {
        std::cout << "[HAL-Test -> Logik] ADC-Kalibrier-Pulse: emptyBelt=" << emptyBelt
                  << " flatCenter=" << flatCenter << " minSamples=" << minSamples << std::endl;
    }
};

volatile bool g_running = true;

void onSignal(int) {
    g_running = false;
}

void printBanner() {
    std::cout << "\n========================================\n"
              << "  Dispatcher HAL Hardware-Test\n"
              << "========================================\n"
              << "Taster:     START, STOP, RESET, ESTOP\n"
              << "Lichtschr.: ENTRY, SWITCH, END, CHUTE\n"
              << "Sensor:     METAL (Induktiv)\n"
              << "Hoehe:      Werkstueck unter ADC schieben\n"
              << "Beenden:    Ctrl+C\n"
              << "========================================\n\n";
}

void printChecklist() {
    std::cout << "--- Pruefliste (jeweils [GPIO] + [Dispatcher] + [HAL-Test -> Logik]) ---\n"
              << " [ ] START gedrueckt / losgelassen\n"
              << " [ ] STOP gedrueckt / losgelassen\n"
              << " [ ] RESET gedrueckt / losgelassen\n"
              << " [ ] ESTOP gedrueckt / losgelassen\n"
              << " [ ] LS_ENTRY blockiert / frei\n"
              << " [ ] LS_SWITCH (Weiche) blockiert / frei\n"
              << " [ ] LS_END blockiert / frei\n"
              << " [ ] LS_CHUTE blockiert / frei\n"
              << " [ ] METAL erkannt / frei\n"
              << " [ ] HEIGHT blockiert / frei / Typ erkannt\n"
              << "-------------------------------------------------------------------\n\n";
}

} // namespace

int main(int argc, char* argv[]) {
    AnlagenPosition pos = AnlagenPosition::VORN_ANLAGE_1;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "-s") {
            pos = AnlagenPosition::VORN_ANLAGE_1;
        } else if (arg == "-c") {
            pos = AnlagenPosition::HINTEN_ANLAGE_2;
        } else if (arg == "-h" || arg == "--help") {
            std::cout << "Usage: dispatcher_hal_test [-s|-c]\n"
                      << "  -s   Anlage 1 (Standard)\n"
                      << "  -c   Anlage 2\n";
            return EXIT_SUCCESS;
        }
    }

    if (ThreadCtl(_NTO_TCTL_IO, 0) == -1) {
        std::cerr << "FEHLER: Keine I/O-Rechte (als root starten?)." << std::endl;
        return EXIT_FAILURE;
    }

    std::signal(SIGINT, onSignal);
    std::signal(SIGTERM, onSignal);

    printBanner();
    printChecklist();

    Dispatcher dispatcher;
    dispatcher.init(pos);

    const int disCoid = dispatcher.getMyLocalCoid();
    if (disCoid == -1) {
        std::cerr << "FEHLER: Dispatcher-COID ungueltig." << std::endl;
        return EXIT_FAILURE;
    }

    LoggingLogik logik;
    dispatcher.setLogik(&logik);

    hal::HalSensors sensors;
    sensors.init(disCoid);
    dispatcher.setSensor(&sensors);

    if (sensors.isEstopPressed()) {
        std::cerr << "ACHTUNG: E-Stop ist gedrueckt! Bitte entriegeln und neu starten." << std::endl;
        return EXIT_FAILURE;
    }

    sensors.setAdcWaitigForWpState();
    std::cout << "[HAL-Test] ADC auf Warte-auf-Werkstueck. Hoehensensor bereit.\n" << std::endl;

    std::thread dispatcherThread([&dispatcher]() { dispatcher.run(); });

    std::cout << "[HAL-Test] Lausche auf HAL-Pulse ... (Ctrl+C zum Beenden)\n" << std::endl;

    while (g_running) {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    dispatcher.stop();
    sensors.stopInterrupts();

    if (dispatcherThread.joinable()) {
        dispatcherThread.join();
    }

    std::cout << "\n[HAL-Test] Beendet." << std::endl;
    return EXIT_SUCCESS;
}
