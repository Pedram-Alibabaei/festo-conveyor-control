/**
 * @file main.cpp
 * @brief Haupteinstiegspunkt der Festo Anlage mit Thread-sicherer Kalibrierung.
 */

#include <iostream>
#include <cstdlib>
#include <sys/neutrino.h>
#include <thread> // NEU: Für den Dispatcher-Thread
#include <chrono>

#include "dispatcher/dispatcher.hpp"
#include "shared/ipc_definitions.hpp"
#include "hal/hal_sensors.hpp"
#include "hal/hal_actuators.hpp"
#include "shared/types.hpp"
#include "connectionManager/ConnectionManager.hpp"
#include "calibrationManager/calibrationManager.hpp"
#include "calibrationManager/i_CalibrationManager.hpp"
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
        return EXIT_FAILURE;
    }

    try {
        // 1. Dispatcher erzeugen und initialisieren
        Dispatcher dispatcher;
        dispatcher.init(pos);
        int dis_coid = dispatcher.getMyLocalCoid();

        // 2. Alle anderen Komponenten erzeugen
        ConnectionManager c_manager;
        c_manager.init(dis_coid, pos);

        hal::HalSensors sensors;
        hal::HalActuators actuators;
        sensors.init(dis_coid);

        if (sensors.isEstopPressed()) {
            std::cerr << "ACHTUNG: E-Stop ist gedrueckt! Bitte entriegeln." << std::endl;
            return EXIT_FAILURE;
        }

        CalibrationManager calib_manager;
        logik::Logik logik_fsm(&dispatcher);

        Timer timer;
        timer.init(dis_coid);

        // 3. Dependency Injection
        dispatcher.setActuators(&actuators);
        dispatcher.setConnectionManager(&c_manager);
        dispatcher.setSensor(&sensors);
        dispatcher.setTimer(&timer);
        dispatcher.setLogik(&logik_fsm);
        dispatcher.setCalibrationManager(&calib_manager);
        logik_fsm.setCalibrationManager(&calib_manager);
        calib_manager.setLogikContext(&logik_fsm);

        // 2. Logik initialisieren
        logik_fsm.init(pos);

        // ====================================================================
        // SCHRITT 4: DISPATCHER IN EINEM THREAD STARTEN (Der Empfänger steht bereit!)
        // ====================================================================
        std::cout << "[System] Starte Dispatcher-Reaktor-Thread..." << std::endl;
        std::thread dispatcherThread(&Dispatcher::run, &dispatcher);

        // Kurze Pause, damit der Thread sicher bei MsgReceive() angekommen ist
        std::this_thread::sleep_for(std::chrono::milliseconds(50));

        // ====================================================================
        // SCHRITT 5: JETZT DIE KALIBRIERUNG SENDEN (Absolut sicher)
        // ====================================================================
        std::cout << "[System] Pruefe persistenten Speicher auf Kalibrierungsdatei..." << std::endl;

        if (!calib_manager.fileExists()) {
            logik_fsm.processEvent(logik::Event::DATA_NOT_EXIST);
        }
        else {
            std::cout << "[System] Kalibrierung gefunden. Lade und verteile Werte..." << std::endl;
            logik_fsm.loadCalibrationData();
        }

        // 6. Erst NACH der Kalibrierung die Timer starten, damit Ticks nicht die Kalibrierung überholen
        timer.startHeartbeatTimer(50);
        timer.startBeltTimer(10);

        // ====================================================================
        // SCHRITT 7: AUF DAS ENDE DES DISPATCHERS WARTEN
        // ====================================================================
        // Da die main() sonst sofort zu Ende wäre, "fetten" wir uns an den Thread
        if (dispatcherThread.joinable()) {
            dispatcherThread.join();
        }

    } catch (const std::exception& e) {
        std::cerr << "KRITISCHER SYSTEMFEHLER in main(): " << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
