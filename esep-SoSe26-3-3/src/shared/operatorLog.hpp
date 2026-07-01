/**
 * @file operatorLog.hpp
 * @brief Konsole-Ausgaben gemaess Aufgabenstellung (Bediener / Abnahme).
 */

#ifndef OPERATOR_LOG_HPP
#define OPERATOR_LOG_HPP

#include "shared/types.hpp"

#include <iostream>

namespace operatorLog {

inline const char* anlageLabel(AnlagenPosition pos) {
    switch (pos) {
    case AnlagenPosition::VORN_ANLAGE_1:
        return "Anlage 1";
    case AnlagenPosition::HINTEN_ANLAGE_2:
        return "Anlage 2";
    default:
        return "Anlage";
    }
}

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

/** Anlage 2: WP kam nach WP-Info nicht rechtzeitig am Einlauf an. */
inline void logHandoverFailedA2(int wpId,
                                WpType type,
                                std::uint32_t elapsedTicks,
                                std::uint32_t limitTicks) {
    std::cout << "[Uebergabe] Anlage 2: FEHLER — WP id=" << wpId
              << ", Typ=" << wpTypeLabel(type)
              << " am Einlauf nicht innerhalb von " << (limitTicks * 10U)
              << " ms angekommen (Ist: " << (elapsedTicks * 10U)
              << " ms). Anlage 1 wird zum Motor-Stopp aufgefordert." << std::endl;
}

/** Anlage 1: Partner meldet fehlgeschlagene Uebergabe -> Motor aus. */
inline void logHandoverMotorStopA1() {
    std::cout << "[Uebergabe] Anlage 1: Anlage 2 meldet fehlgeschlagene Uebergabe "
              << "-> Motor aus." << std::endl;
}

// --- Not-Aus (E-Stop) ---

inline void logEstopLocalActive(AnlagenPosition pos) {
    std::cout << "[Not-Aus] " << anlageLabel(pos)
              << ": E-Stop-Taster gedrueckt — Anlage gestoppt (Motor aus, "
              << "Werkstueckliste geloescht)." << std::endl;
}

inline void logEstopPartnerActive(AnlagenPosition pos) {
    std::cout << "[Not-Aus] " << anlageLabel(pos)
              << ": Partner-Anlage meldet E-Stop — beide Anlagen gestoppt." << std::endl;
}

inline void logEstopBothActive() {
    std::cout << "[Not-Aus] E-Stop an beiden Anlagen aktiv — Anlage gestoppt."
              << std::endl;
}

inline void logEstopWaitReset(AnlagenPosition pos) {
    std::cout << "[Not-Aus] " << anlageLabel(pos)
              << ": E-Stop-Taster freigegeben — Reset-Taster an beiden "
              << "Anlagen betaetigen." << std::endl;
}

inline void logEstopLocalResetAck(AnlagenPosition pos) {
    std::cout << "[Not-Aus] " << anlageLabel(pos)
              << ": Lokaler Reset quittiert." << std::endl;
}

inline void logEstopPartnerResetAck(AnlagenPosition pos) {
    std::cout << "[Not-Aus] " << anlageLabel(pos)
              << ": Partner-Reset quittiert." << std::endl;
}

inline void logEstopResetComplete(AnlagenPosition pos) {
    std::cout << "[Not-Aus] " << anlageLabel(pos)
              << ": Reset abgeschlossen — Anlage wieder bereit (Idle)."
              << std::endl;
}

// --- Verbindung ---

inline void logConnectionLost(AnlagenPosition pos) {
    std::cout << "[Verbindung] " << anlageLabel(pos)
              << ": Verbindung zur Partner-Anlage verloren — Anlage gestoppt."
              << std::endl;
}

inline void logConnectionRestored(AnlagenPosition pos) {
    std::cout << "[Verbindung] " << anlageLabel(pos)
              << ": Verbindung zur Partner-Anlage wiederhergestellt."
              << std::endl;
}

inline void logConnectionLostAndEstop(AnlagenPosition pos) {
    std::cout << "[Verbindung] " << anlageLabel(pos)
              << ": Verbindung verloren und Not-Aus aktiv — Anlage gestoppt."
              << std::endl;
}

inline void logConnectionLostWaitReset(AnlagenPosition pos) {
    std::cout << "[Verbindung] " << anlageLabel(pos)
              << ": E-Stop freigegeben, Verbindung noch getrennt — "
              << "Reset nach Wiederherstellung erforderlich." << std::endl;
}

} // namespace operatorLog

#endif // OPERATOR_LOG_HPP
