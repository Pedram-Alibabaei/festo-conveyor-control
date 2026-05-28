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
    // ========================================================================
    // [ PHASE 1 ] SYSTEMSTART & PARAMETER
    // ========================================================================
    std::cout << "===========================================" << std::endl;
    std::cout << "    Festo Anlage - Systemstart             " << std::endl;
    std::cout << "===========================================" << std::endl;

    // Standardmäßig gehen wir davon aus, dass wir Anlage 1 sind.
    AnlagenPosition pos = AnlagenPosition::VORN_ANLAGE_1;

    // Konsolen-Argumente auslesen (-s für Server/Anlage 1, -c für Client/Anlage 2)
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-s") {
            pos = VORN_ANLAGE_1;
        } else if (arg == "-c") {
            pos = HINTEN_ANLAGE_2;
        }
    }

    // ========================================================================
    // [ PHASE 2 ] QNX HARDWARE BERECHTIGUNGEN
    // ========================================================================
    // Das Programm braucht zwingend Root-Rechte, um auf die GPIO-Pins
    // des BeagleBones zugreifen zu können.
    if (ThreadCtl(_NTO_TCTL_IO, 0) == -1) {
        std::cerr << "FEHLER: Konnte keine Hardware-Rechte anfordern (Programm mit 'sudo' gestartet?)." << std::endl;
        return EXIT_FAILURE;
    }

    try {
        // ====================================================================
        // [ PHASE 3 ] OBJEKT-ERZEUGUNG & INITIALISIERUNG
        // ====================================================================

        // 1. Dispatcher (Das zentrale Kommunikations-Herzstück)
        Dispatcher dispatcher;
        dispatcher.init(pos);
        int dis_coid = dispatcher.getMyLocalCoid(); // Briefkasten-ID für alle anderen

        // 2. Netzwerk-Manager
        ConnectionManager c_manager;
        c_manager.init(dis_coid, pos);

        // 3. Hardware-Fassaden (Sensoren & Aktoren)
        hal::HalSensors sensors;
        hal::HalActuators actuators;
        sensors.init(dis_coid); // Sensoren wissen jetzt, wohin sie Interrupts schicken müssen

        // SICHERHEITS-CHECK: Verhindert den Start, wenn der Not-Halt physisch eingedrückt ist
        if (sensors.isEstopPressed()) {
            std::cerr << "ACHTUNG: E-Stop ist gedrueckt! Bitte entriegeln und neu starten." << std::endl;
            return EXIT_FAILURE;
        }

        // 4. Speicher, Logik und Timer
        CalibrationManager calib_manager;
        logik::Logik logik_fsm;

        Timer timer;
        timer.init(dis_coid);

        // ====================================================================
        // [ PHASE 4 ] DEPENDENCY INJECTION (Die Komponenten verdrahten)
        // ====================================================================
        // Wir geben dem Dispatcher die Zeiger auf alle anderen Klassen,
        // damit er Nachrichten an sie weiterleiten kann.
        dispatcher.setActuators(&actuators);
        dispatcher.setConnectionManager(&c_manager);
        dispatcher.setSensor(&sensors);
        dispatcher.setTimer(&timer);
        dispatcher.setLogik(&logik_fsm);
        dispatcher.setCalibrationManager(&calib_manager);

        // Die Zustandsmaschine braucht den Dispatcher, um Lampen/Motoren zu schalten
        logik_fsm.setDispatcher(&dispatcher);

        // Zustandsmaschine starten (Wechselt initial in den IdleState!)
        logik_fsm.init(pos);

        // ====================================================================
        // [ PHASE 5 ] THREADING: DISPATCHER STARTEN
        // ====================================================================
        // Ab hier spaltet sich das Programm! Der Dispatcher läuft ab jetzt
        // in einem eigenen, parallelen Hintergrund-Thread und wartet auf Nachrichten.
        std::cout << "[System] Starte Dispatcher-Reaktor-Thread..." << std::endl;
        std::thread dispatcherThread(&Dispatcher::run, &dispatcher);

        // Kurze Pause, damit der Thread sicher hochfahren und blockieren (MsgReceive) kann
        std::this_thread::sleep_for(std::chrono::milliseconds(50));

        // ====================================================================
        // [ PHASE 6 ] KALIBRIERUNG LADEN & VERTEILEN
        // ====================================================================
        std::cout << "[System] Pruefe persistenten Speicher auf Kalibrierungsdatei..." << std::endl;

        if (!calib_manager.fileExists()) {
            std::cout << "[System] HINWEIS: Kalibrierungsdatei existiert nicht (Erststart)." << std::endl;
            std::cout << "[System] WICHTIG: Anlage muss vor dem Automatiklauf kalibriert werden!" << std::endl;
        }
        else {
            calib_manager.load();

            // Prüfen, ob die wichtigsten Keys (Leeres Band & Flaches Werkstück) vorhanden sind
            if (calib_manager.hasKey(calib::EMPTY_BELT) && calib_manager.hasKey(calib::FLAT_CENTER)) {
                std::cout << "[System] Gueltige Kalibrierung gefunden! Streamen der Werte an den aktiven Reaktor..." << std::endl;

                // Da der Dispatcher-Thread jetzt läuft, können wir ihm die geladenen
                // Werte direkt als Pulses zuschießen!
                calib_manager.distributeValues(dis_coid);
            }
            else {
                std::cerr << "[System] WARNUNG: Datei beschädigt oder unvollstaendig! System startet unkalibriert." << std::endl;
            }
        }

        // ====================================================================
        // [ PHASE 7 ] TIMING SYSTEM STARTEN
        // ====================================================================
        // Erst NACH der Kalibrierung die Timer starten, damit Ticks nicht die
        // Kalibrierungsnachrichten überholen (Race Conditions vermeiden).
        timer.startHeartbeatTimer(2000); // Partner-Überwachung (Netzwerk)
        timer.startBeltTimer(1000);      // Logik-Ticks (Förderband)

        std::cout << "Anlage ist nun VOLLSTÄNDIG BEREIT und synchronisiert!" << std::endl;
        std::cout << "======================================================\n" << std::endl;

        // ====================================================================
        // [ PHASE 8 ] MAIN-THREAD BLOCKIEREN (Lebenserhaltung)
        // ====================================================================
        // Wenn wir hier nicht warten würden, würde "main" sofort beendet werden
        // und das gesamte Programm (inkl. Dispatcher-Thread) würde abstürzen.
        // join() friert den Main-Thread ein, bis der Dispatcher-Thread beendet wird.
        if (dispatcherThread.joinable()) {
            dispatcherThread.join();
        }

    } catch (const std::exception& e) {
        std::cerr << "KRITISCHER SYSTEMFEHLER in main(): " << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
