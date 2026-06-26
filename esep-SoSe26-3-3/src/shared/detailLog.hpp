/**
 * @file detailLog.hpp
 * @brief Entwickler-Detailausgaben (Abnahme: standardmaessig aus).
 *
 * Produktivbetrieb laut Aufgabenstellung: keine Debug-Ausgaben auf der Konsole.
 * Bediener-Hinweise und [ERROR]-Bloecke bleiben direkt als std::cout/std::cerr.
 */

#ifndef DETAIL_LOG_HPP
#define DETAIL_LOG_HPP

#include <iostream>

namespace detailLog {
static constexpr bool enabled = false;
} // namespace detailLog

#define DETAIL_LOG(expr) \
    do { \
        if (detailLog::enabled) { \
            std::cout << expr << std::endl; \
        } \
    } while (0)

#endif // DETAIL_LOG_HPP
