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

    // ====================================================================
    // SCHRITT 1: ANLAGENPOSITION AUS DEN STARTARGUMENTEN ERMITTELN
    // ====================================================================
    AnlagenPosition pos = AnlagenPosition::VORN_ANLAGE_1; // Standardwert
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-s") {
            pos = AnlagenPosition::VORN_ANLAGE_1;
        } else if (arg == "-c") {
            pos = AnlagenPosition::HINTEN_ANLAGE_2;
        }
    }

    // QNX I/O Berechtigungen für den Thread anfordern (Zwingend für HAL-Zugriff)
    if (ThreadCtl(_NTO_TCTL_IO, 0) == -1) {
        std::cerr << "[Main] CRITICAL: ThreadCtl fehlgeschlagen. Keine I/O Rechte!" << std::endl;
        return EXIT_FAILURE;
    }

    try {
        std::cout << "[System] Starte System-Initialisierung..." << std::endl;

        // ====================================================================
        // SCHRITT 2: DISPATCHER ERZEUGEN UND INITIALISIEREN
        // ====================================================================
        Dispatcher dispatcher;
        dispatcher.init(pos);
        int dis_coid = dispatcher.getMyLocalCoid();

        // ====================================================================
        // SCHRITT 3: HARDWARE- UND SUB-KOMPONENTEN ERZEUGEN
        // ====================================================================
        ConnectionManager c_manager;
        c_manager.init(dis_coid, pos);

        hal::HalSensors sensors;
        hal::HalActuators actuators;
        sensors.init(dis_coid);

        // Sicherheitscheck beim Booten: Steht die Anlage im Not-Aus?
        if (sensors.isEstopPressed()) {
            std::cerr << "ACHTUNG: E-Stop ist gedrueckt! Bitte entriegeln und System neu starten." << std::endl;
            return EXIT_FAILURE;
        }

        CalibrationManager calib_manager;

        // ====================================================================
        // SCHRITT 4: LOGIK & TIMER INSTANZIIEREN (DEPENDENCY INJECTION)
        // ====================================================================
        logik::Logik logik(&dispatcher); // Übergibt den Zeiger auf das Kommunikations-Interface

        // KORREKTUR: Pointer-Adresse von calib_manager übergeben & Dopplung entfernt
        logik.setCalibrationManager(&calib_manager);
        logik.init(pos); // Konfiguriert Rolle (Master/Slave) und Schieberegister

        Timer timer;
        timer.init(dis_coid); // Registriert die OS-Timer an der Dispatcher-Coid

        // ====================================================================
        // SCHRITT 5: DISPATCHER-REAKTOR KOPPELN (ZWEITE DIRECTION INJECTION)
        // ====================================================================
        dispatcher.setActuators(&actuators);
        dispatcher.setConnectionManager(&c_manager);
        dispatcher.setSensor(&sensors);
        dispatcher.setTimer(&timer);
        dispatcher.setLogik(&logik);
        dispatcher.setCalibrationManager(&calib_manager);

        // ====================================================================
        // SCHRITT 6: DISPATCHER IN EINEM SEPARATEN THREAD STARTEN
        // ====================================================================
        std::cout << "[System] Starte Dispatcher-Reaktor-Thread..." << std::endl;
        std::thread dispatcherThread(&Dispatcher::run, &dispatcher);

        // Dem Thread kurz Zeit geben, um die native QNX MsgReceive-Schleife zu betreten
        std::this_thread::sleep_for(std::chrono::milliseconds(50));

        // ====================================================================
        // SCHRITT 7: CHECK DER KALIBRIERUNG & SYSTEM-START
        // ====================================================================
        // Prüft, ob Datei da ist, lädt Daten und triggert DATA_EXIST / DATA_NOT_EXIST
        logik.checkCalibrationFile();

        // Erst NACH dem Kalibrierungs-Setup starten wir die zyklischen Herzschläge,
        // damit eintreffende Ticks keine uninitialisierten Zeitwerte überholen!
        timer.startHeartbeatTimer(50); // 50ms Netzwerk-Watchdog
        timer.startBeltTimer(10);      // 10ms Schieberegister-Takt

        std::cout << "[System] System erfolgreich hochgefahren. Bereit fuer Laborbetrieb." << std::endl;

        // ====================================================================
        // SCHRITT 8: AUF DAS ENDE DES DISPATCHERS WARTEN (BLOCKIEREN DER MAIN)
        // ====================================================================
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
