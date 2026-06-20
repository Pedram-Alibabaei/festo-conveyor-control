/**
 * @file Workpiece.hpp
 * @brief Klassenbeschreibung: Workpiece als digitaler Zwilling.
 *
 * Die Klasse Workpiece ist ein reines Datenobjekt (Data Transfer Object), das
 * ein physisches Werkstück auf der Förderanlage repräsentiert. Sie hält
 * ausschließlich zustandsrelevante Informationen während des Durchlaufs.
 *
 * Embedded Systems Engineering WiSe2025 Team 1.1
 */

#ifndef WORKPIECE_HPP
#define WORKPIECE_HPP

#include "shared/types.hpp"// Enthält WpType und WpLocation
#include <cstdint>

class Workpiece {
private:
    // Statische Variable zur automatischen und eindeutigen ID-Vergabe auf Anlage 1
	static int s_next_id;

    // --- Private Member-Variablen ---
    int m_id;
    WpType m_type;
    WpLocation m_location;

    // --- Variablen für das logische 10ms-Takt-Schieberegister ---
    std::uint32_t m_ticksInCurrentState;  ///< Verbrachte 10ms-Timer-Takte in der aktuellen Zone
    std::uint32_t m_targetLimitForState;  ///< Das aktuell gültige Takt-Limit für diese Zone
    bool m_isCorrectInSequence;           ///< Flag, ob das Teil an der aktuellen Weiche korrekt im Rezept ist
    bool m_isExpectedFromNetwork;         ///< Flag für Anlage 2: Passte der übertragene Netzwerk-Typ zum lokalen Rezept?
    bool m_entryBlockedSeen;              ///< Physische ENTRY_BLOCKED-Flanke fuer dieses WP gesehen?
    bool m_deferRejectToPartner;          ///< Anlage 1: falsches WP bei voller Rutsche zur Partner-Anlage leiten

public:
    /**
     * @brief Konstruktor: Vergibt automatisch die nächste freie ID,
     * setzt das Werkstück an den Einlauf und initialisiert die Taktzähler sowie Flags.
     */
    Workpiece() :
        m_id(s_next_id++),
        m_type(WpType::UNKNOWN),
        m_location(WpLocation::AT_ENTRY),
        m_ticksInCurrentState(0),
        m_targetLimitForState(0),
        m_isCorrectInSequence(true),
        m_isExpectedFromNetwork(true),
        m_entryBlockedSeen(false),
        m_deferRejectToPartner(false)
    {}

    // --- Getter & Setter für die ID ---
    /**
     * @brief Liefert die eindeutige ID des Werkstücks.
     */
    int getId() const {
        return m_id;
    }

    /**
     * @brief Überschreibt die ID des Werkstücks.
     * @note Exklusiv wichtig für Anlage 2, um die ID von Anlage 1 via QNX-Netzwerk zu übernehmen (Tracing).
     * @param id Die originale ID der ersten Teilanlage.
     */
    void setId(int id) {
        m_id = id;
    }

    // --- Getter & Setter für den Werkstücktyp ---
    void setType(WpType type) {
        m_type = type;
    }

    WpType getType() const {
        return m_type;
    }

    // --- Getter & Setter für den Aufenthaltsort (Zonen-Tracking) ---
    /**
     * @brief Aktualisiert den Aufenthaltsort des Werkstücks.
     * @note Setzt den internen Taktzähler automatisch zurück,
     * da jeder neue Bandabschnitt mathematisch bei Takt 0 beginnt.
     * Das targetLimit wird vom WorkpieceManager direkt danach neu gesetzt.
     */
    void updateLocation(WpLocation newLocation) {
        m_location = newLocation;
        m_ticksInCurrentState = 0;
        m_targetLimitForState = 0;
    }

    WpLocation getLocation() const {
        return m_location;
    }

    // ====================================================================
    // INTERFACE FÜR DAS TAKT-TRACKING (Steuerung durch den WorkpieceManager)
    // ====================================================================

    /**
     * @brief Inkrementiert den Taktzähler um genau +1 (wird alle 10ms vom Timer aufgerufen).
     */
    void incrementTicks() {
        m_ticksInCurrentState++;
    }

    void resetTicks() {
        m_ticksInCurrentState = 0;
    }

    /**
     * @brief Liefert die im aktuellen Abschnitt bereits verbrachten 10ms-Takte.
     */
    std::uint32_t getTicks() const {
        return m_ticksInCurrentState;
    }

    /**
     * @brief Setzt das berechnete Takt-Limit für die aktuelle Zone fest.
     * Wichtig bei Geschwindigkeitswechseln (z.B. Umschalten nach 200 Ticks auf Slow).
     */
    void setTargetLimit(std::uint32_t limit) {
        m_targetLimitForState = limit;
    }

    /**
     * @brief Liefert das aktuell erwartete Takt-Limit für diese Zone.
     */
    std::uint32_t getTargetLimit() const {
        return m_targetLimitForState;
    }

    void markEntryBlockedSeen() {
        m_entryBlockedSeen = true;
        m_ticksInCurrentState = 0;
    }

    bool hasEntryBlockedSeen() const {
        return m_entryBlockedSeen;
    }

    // --- Getter & Setter für die Rezeptur-Flags ---
    /**
     * @brief Setzt fest, ob das Teil laut TargetSequence an der lokalen Weiche an der Reihe ist.
     */
    void setIsCorrectInSequence(bool value) {
        m_isCorrectInSequence = value;
    }

    /**
     * @brief Gibt an, ob das Teil an der lokalen Weiche der korrekte Rezept-Typ ist.
     */
    bool isCorrectInSequence() const {
        return m_isCorrectInSequence;
    }

    /**
     * @brief Setzt fest, ob der via Netzwerk uebertragene Typ beim Eintreffen auf Anlage 2 der Sequenz entsprach.
     */
    void setIsExpectedFromNetwork(bool value) {
        m_isExpectedFromNetwork = value;
    }

    /**
     * @brief Gibt an, ob der Netzwerk-Typ des Teils beim Eintritt auf Anlage 2 zum Rezept passte.
     */
    bool isExpectedFromNetwork() const {
        return m_isExpectedFromNetwork;
    }

    void setDeferRejectToPartner(bool value) {
        m_deferRejectToPartner = value;
    }

    bool isDeferRejectToPartner() const {
        return m_deferRejectToPartner;
    }
};

#endif // WORKPIECE_HPP
