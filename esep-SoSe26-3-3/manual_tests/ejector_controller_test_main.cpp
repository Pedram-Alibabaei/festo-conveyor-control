/**
 * Standalone-Test fuer EjectorController (Weiche vs. Auswerfer + Auto-Close).
 *
 * Bauen:  make ejector_test
 * Schnell: make ejector_test EJECTOR_TEST_FAST=1   (nur Entwicklung, 5 s)
 *
 * Standard-Timeout: 60 s (Sicherheits-Fallback in EjectorController)
 *
 * Auf der Anlage ausfuehren (root / I/O-Rechte noetig):
 *   ./build/armv7le-debug/ejector_test
 *   ./build/armv7le-debug/ejector_test -f
 */

#include <chrono>
#include <csignal>
#include <cstdlib>
#include <iostream>
#include <thread>

#include <hal/output_driver/ejector_controller.hpp>
#include <hal/pal.hpp>

extern "C" {
#include <sys/neutrino.h>
}

namespace
{
hal::output_driver::EjectorController* g_controller = nullptr;

void emergencyClose(int)
{
    if (g_controller != nullptr) {
        if (g_controller->isSwitch()) {
            g_controller->closeSwitch();
        } else {
            g_controller->pullEjector();
        }
    }
    std::_Exit(EXIT_FAILURE);
}

void printUsage(const char* prog)
{
    std::cout << "Usage: " << prog << " [-f]\n"
              << "  -f   Schnelltest mit 5 s Auto-Close (Makefile: EJECTOR_TEST_FAST=1)\n"
              << "\nErkennung:\n"
              << "  Status-Eingang is::Ejector HIGH -> AUSWERFER\n"
              << "  Status-Eingang is::Ejector LOW  -> WEICHE\n"
              << "\nAuto-Close: Watchdog schliesst nach Timeout automatisch.\n";
}
} // namespace

int main(int argc, char* argv[])
{
    bool fastMode = false;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-f") {
            fastMode = true;
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

    std::signal(SIGINT, emergencyClose);
    std::signal(SIGTERM, emergencyClose);

    hal::pal::is::Ejector::config_as_input();
    const bool statusPinHigh = hal::pal::is::Ejector::is_data_set();
    std::cout << "========================================\n"
              << " EjectorController Hardware-Test\n"
              << "========================================\n"
              << "Status-Pin is::Ejector: "
              << (statusPinHigh ? "HIGH (1)" : "LOW (0)") << std::endl;

    hal::output_driver::EjectorController controller;
    g_controller = &controller;

    std::cout << "Auto-Close Timeout (Controller): "
              << controller.getAutoCloseTimeout().count() << " ms\n";

    if (fastMode) {
        std::cout << "Hinweis: -f setzt nur Makefile-Flags; massgeblich ist der Controller-Wert oben.\n";
    }

    if (controller.isSwitch()) {
        std::cout << "Erkannt: WEICHE (Junction)\n";
        std::cout << "Aktion: openSwitch() ...\n";
        controller.openSwitch();
    } else {
        std::cout << "Erkannt: AUSWERFER (Pusher)\n";
        std::cout << "Aktion: pushEjector() ...\n";
        controller.pushEjector();
    }

    if (!controller.isActive()) {
        std::cerr << "FEHLER: Aktivierung fehlgeschlagen (bereits aktiv?)." << std::endl;
        return EXIT_FAILURE;
    }

    const auto start = std::chrono::steady_clock::now();

    std::cout << "Beobachte Auto-Close (alle 2 s Status) ...\n";

    while (controller.isActive()) {
        const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start);

        std::cout << "  [+" << elapsed.count() << " ms] Sortiereinheit AKTIV\n";
        std::this_thread::sleep_for(std::chrono::seconds(2));

        if (elapsed.count() > 90000) {
            std::cerr << "ABBRUCH: Kein Auto-Close nach 90 s." << std::endl;
            if (controller.isSwitch()) {
                controller.closeSwitch();
            } else {
                controller.pullEjector();
            }
            return EXIT_FAILURE;
        }
    }

    const auto total = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start);

    std::cout << "OK: Auto-Close erkannt nach " << total.count() << " ms.\n";
    std::cout << "Test erfolgreich beendet.\n";
    g_controller = nullptr;
    return EXIT_SUCCESS;
}
