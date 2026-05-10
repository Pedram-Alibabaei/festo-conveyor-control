/**
 * @file main.cpp
 * @brief Haupteinstiegspunkt der Festo Anlage.
 */

#include <iostream>
#include <cstdlib>
#include <sys/neutrino.h>

#include "dispatcher/dispatcher.hpp"
#include "shared/ipc_definitions.hpp"
#include "hal/hal_sensors.hpp"
#include "hal/hal_actuators.hpp"


int main(int argc, char *argv[]) {
    std::cout << "===========================================" << std::endl;
    std::cout << "    Festo Anlage - Systemstart           " << std::endl;
    std::cout << "===========================================" << std::endl;


    bool isdevice_1 = false; // Anlage 1

    // Wir starten bei i=1, da argv[0] immer der Pfad zum Programm selbst ist
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-s") {
            isdevice_1 = true;
        } else if (arg == "-c") {
            isdevice_1 = false;
        }
    }

    // Hardware-Rechte anfordern
    if (ThreadCtl(_NTO_TCTL_IO, 0) == -1) {
        std::cerr << "FEHLER: Konnte keine Hardware-Rechte anfordern (root?)." << std::endl;
        return EXIT_FAILURE;
    }

    try {
        // DISPATCHER STARTEN
        Dispatcher dispatcher(isdevice_1);
        int chid = dispatcher.getMyChannelId();
        int coid = ConnectAttach(0, 0, chid, _NTO_SIDE_CHANNEL, 0);

        if (coid == -1) {
            std::cerr << "FEHLER: Konnte QNX-Verbindung nicht herstellen!" << std::endl;
            return EXIT_FAILURE;
        }


        // Position im Dispatcher setzen
        if (isdevice_1) {
            std::cout << "[Konfiguration] Ich bin vorne (Anlage 1)" << std::endl;
        } else {
            std::cout << "[Konfiguration] Ich bin hinten (Anlage 2)" << std::endl;
        }

        // 3. HARDWARE BAUEN
        // A) Sensoren bauen und E-Stop prüfen
        hal::HalSensors sensors;
        if (sensors.isEstopPressed()) {
            std::cerr << "ACHTUNG: E-Stop ist gedrueckt! Bitte entriegeln." << std::endl;
            return EXIT_FAILURE;
        }

        // B) Aktoren bauen (mit Auto-Detect im Hintergrund!)
        hal::HalActuators actuators;

        // 4. VERKABELN (Die Magie)
        // Dem Dispatcher die Aktorik übergeben, damit er steuern kann
        dispatcher.setActuators(&actuators);


        // 5. SENSOREN SCHARFSCHALTEN (Startet die Interrupts)
        sensors.setAdcCalibration(1000, 2000, 2200, 3000, 3200, 2400, 2600, 20);
        sensors.initInterrupts(coid, ipc::HAL_INPUT);

        std::cout << "[Main] Anlage ist nun BEREIT! Druecke die START-Taste..." << std::endl;
        std::cout << "===========================================\n" << std::endl;

        // 6. ENDLOSSCHLEIFE
        dispatcher.run();

    } catch (const std::exception& e) {
        std::cerr << "KRITISCHER SYSTEMFEHLER: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
