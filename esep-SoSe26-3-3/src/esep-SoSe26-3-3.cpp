/**
 * @file main.cpp
 * @brief Haupteinstiegspunkt der Festo Anlage mit Thread-sicherer Kalibrierung.
 *
 * SoSe2026 Team 3.3
 */

#include <iostream>
#include <cstdlib>
#include <sys/neutrino.h>
#include <thread>

#include "dispatcher/dispatcher.hpp"
#include "shared/ipc_definitions.hpp"
#include "hal/hal_sensors.hpp"
#include "hal/hal_actuators.hpp"
#include "shared/types.hpp"
#include "connectionManager/ConnectionManager.hpp"
#include "calibrationManager/CalibrationManager.hpp"
#include "calibrationManager/I_CalibrationManager.hpp"
#include "logik/logik.hpp"
#include "timer/timer.hpp"

int main(int argc, char *argv[]) {

    AnlagenPosition pos = AnlagenPosition::VORN_ANLAGE_1;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-s") {
            pos = AnlagenPosition::VORN_ANLAGE_1;
        } else if (arg == "-c") {
            pos = AnlagenPosition::HINTEN_ANLAGE_2;
        }
    }

    if (ThreadCtl(_NTO_TCTL_IO, 0) == -1) {
        std::cerr << "[Main] CRITICAL: ThreadCtl fehlgeschlagen. Keine I/O Rechte!" << std::endl;
        return EXIT_FAILURE;
    }

    try {
        std::cout << "[System] Starte System-Initialisierung..." << std::endl;

        Dispatcher dispatcher;
        dispatcher.init(pos);
        int dis_coid = dispatcher.getMyLocalCoid();
        if (dis_coid == -1) {
            std::cerr << "[Main] CRITICAL: Dispatcher-COID ungueltig – Abbruch." << std::endl;
            return EXIT_FAILURE;
        }

        ConnectionManager c_manager;
        c_manager.init(dis_coid, pos);

        hal::HalActuators actuators;

        CalibrationManager calib_manager(
            pos == AnlagenPosition::VORN_ANLAGE_1
                ? "festo_calib_anlage1.txt"
                : "festo_calib_anlage2.txt");

        logik::Logik logik(&dispatcher);

        logik::EventManager eventManager(&logik);
        logik.setEventManager(&eventManager);
        logik.setCalibrationManager(&calib_manager);
        logik.init(pos);
        eventManager.init(pos);

        dispatcher.setActuators(&actuators);
        dispatcher.setConnectionManager(&c_manager);
        dispatcher.setLogik(&logik);
        dispatcher.setCalibrationManager(&calib_manager);

        hal::HalSensors sensors;
        sensors.init(dis_coid);
        dispatcher.setSensor(&sensors);

        if (sensors.isEstopPressed()) {
            std::cerr << "ACHTUNG: E-Stop ist gedrueckt! Bitte entriegeln und System neu starten." << std::endl;
            return EXIT_FAILURE;
        }

        Timer timer;
        timer.init(dis_coid);
        dispatcher.setTimer(&timer);

        std::cout << "[System] Starte Dispatcher-Reaktor-Thread..." << std::endl;
        std::thread dispatcherThread(&Dispatcher::run, &dispatcher);

        std::this_thread::sleep_for(std::chrono::milliseconds(50));

        logik.checkCalibrationFile();

        timer.startHeartbeatTimer(50);
        timer.startBeltTimer(10);

        std::cout << "[System] System erfolgreich hochgefahren. Bereit fuer Laborbetrieb." << std::endl;

        if (dispatcherThread.joinable()) {
            dispatcherThread.join();
        }

    } catch (const std::exception& e) {
        std::cerr << "KRITISCHER SYSTEMFEHLER in main(): " << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    std::cout << "[System] Anwendung sauber beendet." << std::endl;
    return EXIT_SUCCESS;
}
