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

} // namespace operatorLog

#endif // OPERATOR_LOG_HPP
