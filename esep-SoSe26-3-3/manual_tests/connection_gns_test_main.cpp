/**
 * Hardware-Test: GNS-Registrierung, Partnersuche, Heartbeat, Reconnect, ESTOP-Abfrage.
 *
 * Bauen:  make connection_gns_test
 *
 * Auf der Anlage (root / I/O-Rechte):
 *   Anlage 1 (.108):  ./build/armv7le-debug/connection_gns_test -s
 *   Anlage 2 (.111):  ./build/armv7le-debug/connection_gns_test -c
 *
 * Testablauf:
 *   1. Anlage 1 starten -> [Dispatcher] Registriert als 'Anlage_1'
 *   2. Anlage 2 starten -> [ConnMgr] PARTNER GEFUNDEN + CONNECTION_RESTORED
 *   3. Partner-Prozess stoppen (Ctrl+C auf Partner) -> CONNECTION_LOST + Reconnect-Suche
 *   4. Waehrend Disconnect: START-Taster druecken -> HAL-Pulse muessen weiter ankommen
 *   5. Partner neu starten -> CONNECTION_RESTORED + ESTOP-Abfrage
 *   6. Optional: Partner-ESTOP gedrueckt halten beim Reconnect -> PARTNER_ESTOP_PRESSED
 */

#include <csignal>
#include <cstdlib>
#include <iostream>
#include <string>
#include <thread>

#include "connectionManager/ConnectionManager.hpp"
#include "dispatcher/dispatcher.hpp"
#include "hal/hal_sensors.hpp"
#include "logik/i_logik.hpp"
#include "shared/ipc_definitions.hpp"
#include "shared/logikEvent.hpp"
#include "shared/types.hpp"
#include "timer/timer.hpp"

extern "C" {
#include <sys/neutrino.h>
}

namespace {

const char* eventName(logik::Event e) {
    switch (e) {
    case logik::Event::START_PRESSED: return "START_PRESSED";
    case logik::Event::START_RELEASED: return "START_RELEASED";
    case logik::Event::ESTOP_PRESSED: return "ESTOP_PRESSED";
    case logik::Event::ESTOP_RELEASED: return "ESTOP_RELEASED";
    case logik::Event::PARTNER_ESTOP_PRESSED: return "PARTNER_ESTOP_PRESSED";
    case logik::Event::PARTNER_ESTOP_RELEASED: return "PARTNER_ESTOP_RELEASED";
    case logik::Event::CONNECTION_LOST: return "CONNECTION_LOST";
    case logik::Event::CONNECTION_RESTORED: return "CONNECTION_RESTORED";
    default: return "OTHER";
    }
}

class LoggingLogik : public i_Logik {
public:
    void init(AnlagenPosition) override {}

    void setCalibrationManager(I_CalibrationManager*) override {}

    void processEvent(logik::Event e) override {
        std::cout << "[GNS-Test -> Logik] Event: " << eventName(e)
                  << " (" << static_cast<int>(e) << ")" << std::endl;
    }

    void workpieceTypeFromHS(WpType) override {}
    void workpieceInfo(int, WpType) override {}
    void updateWpListe(WpType) override {}
    void takt() override {}
    void setPartnerChuteStatus(bool) override {}
    void setLiveAdcCalibration(uint16_t, uint16_t, uint32_t) override {}
};

volatile bool g_running = true;

void onSignal(int) {
    g_running = false;
}

void printBanner(AnlagenPosition pos) {
    const char* role = (pos == AnlagenPosition::VORN_ANLAGE_1)
        ? "Anlage 1 (passiv, wartet auf Partner)"
        : "Anlage 2 (aktiv, sucht Anlage_1)";

    std::cout << "\n========================================\n"
              << "  GNS / Connection Hardware-Test\n"
              << "  Rolle: " << role << "\n"
              << "========================================\n"
              << "Erwartete Logs:\n"
              << "  [Dispatcher] Registriert als 'Anlage_X'\n"
              << "  [ConnMgr] PARTNER GEFUNDEN!\n"
              << "  [Dispatcher] Partner-ESTOP-Status angefragt\n"
              << "  [GNS-Test -> Logik] CONNECTION_RESTORED\n"
              << "  Bei Verlust: CONNECTION_LOST + erneute Suche\n"
              << "  Bei Reconnect: ESTOP-Abfrage + CONNECTION_RESTORED\n"
              << "\nPruefung Non-Blocking:\n"
              << "  Partner stoppen, dann START druecken -> HAL muss weiter laufen\n"
              << "Beenden: Ctrl+C\n"
              << "========================================\n\n";
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
            std::cout << "Usage: connection_gns_test [-s|-c]\n"
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

    printBanner(pos);

    Dispatcher dispatcher;
    dispatcher.init(pos);

    const int disCoid = dispatcher.getMyLocalCoid();
    if (disCoid == -1) {
        std::cerr << "FEHLER: Dispatcher-COID ungueltig." << std::endl;
        return EXIT_FAILURE;
    }

    ConnectionManager connMgr;
    connMgr.init(disCoid, pos);

    LoggingLogik logik;
    dispatcher.setLogik(&logik);
    dispatcher.setConnectionManager(&connMgr);

    Timer timer;
    timer.init(disCoid);
    dispatcher.setTimer(&timer);

    hal::HalSensors sensors;
    sensors.init(disCoid);
    dispatcher.setSensor(&sensors);

    if (sensors.isEstopPressed()) {
        std::cerr << "ACHTUNG: E-Stop ist gedrueckt! Bitte entriegeln und neu starten." << std::endl;
        return EXIT_FAILURE;
    }

    sensors.setAdcWaitigForWpState();

    std::thread dispatcherThread([&dispatcher]() { dispatcher.run(); });

    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    timer.startHeartbeatTimer(50);

    std::cout << "[GNS-Test] Heartbeat laeuft (50ms). Warte auf Partner...\n" << std::endl;

    while (g_running) {
        std::this_thread::sleep_for(std::chrono::seconds(2));
        std::cout << "[GNS-Test] Status: verbunden="
                  << (connMgr.isConnected() ? "JA" : "NEIN")
                  << " | lokaler ESTOP="
                  << (sensors.isEstopPressed() ? "PRESSED" : "OK")
                  << " | Partner-ESTOP-Cache="
                  << (dispatcher.isPartnerEstopPressed() ? "PRESSED" : "OK")
                  << std::endl;
    }

    timer.stop();
    dispatcher.stop();
    sensors.stopInterrupts();

    if (dispatcherThread.joinable()) {
        dispatcherThread.join();
    }

    std::cout << "\n[GNS-Test] Beendet." << std::endl;
    return EXIT_SUCCESS;
}
