/**
 * @file main.cpp
 * @brief Haupteinstiegspunkt der Festo Anlage.
 */

#include <iostream>
#include <cstdlib>
#include <sys/neutrino.h>

#include "dispatcher/dispatcher.hpp"
#include "dispatcher/i_Dispatcher.hpp"
#include "shared/ipc_definitions.hpp"
#include "hal/hal_sensors.hpp"
#include "hal/hal_actuators.hpp"
#include "shared/types.hpp"
#include "connectionManager/ConnectionManager.hpp"


int main(int argc, char *argv[]) {
    std::cout << "===========================================" << std::endl;
    std::cout << "    Festo Anlage - Systemstart           " << std::endl;
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

        Dispatcher dispatcher;
        dispatcher.init(pos);
        int dis_coid = dispatcher.getMyLocalCoid();

        ConnectionManager c_manager;
        c_manager.init(dis_coid, pos);

        hal::HalSensors sensors;
        hal::HalActuators actuators;
        sensors.init(dis_coid);

        if (sensors.isEstopPressed()) {
            std::cerr << "ACHTUNG: E-Stop ist gedrueckt! Bitte entriegeln." << std::endl;
            return EXIT_FAILURE;
        }

        std::cout << "Anlage ist nun BEREIT!" << std::endl;
        std::cout << "=====================\n" << std::endl;

        dispatcher.setActuators(&actuators);
        dispatcher.setConnectionManager(&c_manager);
        dispatcher.setSensor(&sensors);
        dispatcher.run();

    } catch (const std::exception& e) {
        std::cerr << "KRITISCHER SYSTEMFEHLER: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
