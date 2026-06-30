/**
 * @file operatorLog.hpp
 * @brief Konsole-Ausgaben gemaess Aufgabenstellung (Bediener / Abnahme).
 */

#ifndef OPERATOR_LOG_HPP
#define OPERATOR_LOG_HPP

#include "shared/types.hpp"

#include <iostream>

namespace operatorLog {

inline const char* wpTypeLabel(WpType type) {
    switch (type) {
    case WpType::FLAT:
        return "FLAT (Flach)";
    case WpType::TALL:
        return "TALL (Hoch)";
    case WpType::TALL_DRILLED:
        return "TALL_DRILLED (Gebohrt)";
    case WpType::TALL_DRILLED_METAL:
        return "TALL_DRILLED_METAL (Gebohrt, Metall)";
    case WpType::TALL_METAL:
        return "TALL_METAL (Metall)";
    default:
        return "UNKNOWN (Unbekannt/Fehlerhaft)";
    }
}

inline void logNextExpectedWorkpiece(WpType type) {
    std::cout << "[Anlage 2] Naechstes erwartetes Werkstueck in Zielreihenfolge: "
              << wpTypeLabel(type) << std::endl;
}

inline void logWorkpieceBandExit(int wpId, WpType type, const char* reason) {
    std::cout << "[Band] Werkstueck verlaesst Anlage: id=" << wpId
              << ", Typ=" << wpTypeLabel(type)
              << ", Grund=" << reason << std::endl;
}

/** Anlage 1: WP am Bandende, wartet auf Uebergabe. */
inline void logHandoverReadyAtA1End(int wpId, WpType type) {
    std::cout << "[Uebergabe] Anlage 1: WP id=" << wpId
              << ", Typ=" << wpTypeLabel(type)
              << " am Bandende -> bereit fuer Abgabe an Anlage 2." << std::endl;
}

/** Anlage 1: Abgabe gestartet — WP verlaesst Bandende Richtung Anlage 2. */
inline void logHandoverDepartedA1(int wpId, WpType type, std::uint32_t estimateMs) {
    std::cout << "[Uebergabe] Anlage 1: WP id=" << wpId
              << ", Typ=" << wpTypeLabel(type)
              << " verlaesst Bandende -> erwartete Ankunft Anlage 2 Einlauf "
              << "in ca. " << estimateMs << " ms." << std::endl;
}

/** Anlage 1: Partner meldet ENTRY_BLOCKED — Uebergabe erfolgreich. */
inline void logHandoverSuccessA1(int wpId, WpType type) {
    std::cout << "[Uebergabe] Anlage 1: Abgabe erfolgreich — WP id=" << wpId
              << ", Typ=" << wpTypeLabel(type)
              << " von Anlage 2 am Einlauf bestaetigt." << std::endl;
}

/** Anlage 2: WP physisch am Einlauf angekommen. */
inline void logHandoverReceivedA2(int wpId, WpType type) {
    std::cout << "[Uebergabe] Anlage 2: WP id=" << wpId
              << ", Typ=" << wpTypeLabel(type)
              << " am Einlauf angekommen (Uebernahme von Anlage 1)." << std::endl;
}

} // namespace operatorLog

#endif // OPERATOR_LOG_HPP
