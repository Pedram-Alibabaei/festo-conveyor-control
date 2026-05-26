/**
 * @file main.cpp
 * @brief Haupteinstiegspunkt der Festo Anlage mit Thread-sicherer Kalibrierung.
 */

#include <iostream>
#include <cstdlib>
#include <sys/neutrino.h>
#include <thread> // NEU: Für den Dispatcher-Thread

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
    std::cout << "===========================================" << std::endl;
    std::cout << "    Festo Anlage - Systemstart             " << std::endl;
    std::cout << "===========================================" << std::endl;

    AnlagenPosition pos = AnlagenPosition::VORN_ANLAGE_1;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-s") {
            pos = VORN_ANLAGE_1;
        } else if (arg == "-c") {
            pos = HINTEN_ANLAGE_2;
        }
    }

    if (ThreadCtl(_NTO_TCTL_IO, 0) == -1) {
        std::cerr << "FEHLER: Konnte keine Hardware-Rechte anfordern (root?)." << std::endl;
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
        logik::Logik logik_fsm;

        Timer timer;
        timer.init(dis_coid);

        // 3. Dependency Injection
        dispatcher.setActuators(&actuators);
        dispatcher.setConnectionManager(&c_manager);
        dispatcher.setSensor(&sensors);
        dispatcher.setTimer(&timer);
        dispatcher.setLogik(&logik_fsm);
        dispatcher.setCalibrationManager(&calib_manager);

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
            std::cout << "[System] HINWEIS: Kalibrierungsdatei existiert nicht (Erststart)." << std::endl;
            std::cout << "[System] WICHTIG: Anlage muss vor dem Automatiklauf kalibriert werden!" << std::endl;
        }
        else {
            calib_manager.load();

            if (calib_manager.hasKey(calib::EMPTY_BELT) && calib_manager.hasKey(calib::FLAT_CENTER)) {
                std::cout << "[System] Gueltige Kalibrierung gefunden! Streamen der Werte an den aktiven Reaktor..." << std::endl;

                // Da der Thread läuft, werden diese Pulses JETZT live in exakt derselben Millisekunde verarbeitet!
                calib_manager.distributeValues(dis_coid);
            }
            else {
                std::cerr << "[System] WARNUNG: Datei beschädigt! System startet unkalibriert." << std::endl;
            }
        }

        // 6. Erst NACH der Kalibrierung die Timer starten, damit Ticks nicht die Kalibrierung überholen
        timer.startHeartbeatTimer(2000);
        timer.startBeltTimer(1000);

        std::cout << "Anlage ist nun VOLLSTÄNDIG BEREIT und synchronisiert!" << std::endl;
        std::cout << "======================================================\n" << std::endl;

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
