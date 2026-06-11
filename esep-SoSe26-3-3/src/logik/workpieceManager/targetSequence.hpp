/**
 * @file TargetSequence.hpp
 * @brief Klassenbeschreibung: TargetSequence für die Rezeptur-Validierung.
 *
 * Die Klasse TargetSequence verwaltet die zyklische Soll-Reihenfolge der Werkstücke
 * für den Sortierprozess zwischen Anlage 1 und Anlage 2. Sie fungiert als
 * zentrales Regelwerk für die Weichenschaltung.
 *
 * Embedded Systems Engineering WiSe2025 Team 1.1
 */

#ifndef TARGET_SEQUENCE_HPP
#define TARGET_SEQUENCE_HPP

#include <vector>
#include <iostream>
#include "shared/types.hpp"// Enthält das global gültige WpType Enum

class TargetSequence {
private:
    std::vector<WpType> m_sequence; ///< Die vorgegebene Rezeptur-Reihenfolge (Ringpuffer)
    std::size_t m_currentIndex;     ///< Zeiger auf den aktuell erwarteten Typen-Index

public:
    /**
     * @brief Konstruktor: Baut die geforderte Labor-Reihenfolge auf.
     */
    TargetSequence() : m_currentIndex(0) {
        m_sequence = {
            WpType::TALL_DRILLED,       // 1. Hoch, mit Bohrung, nicht-metallisch
            WpType::TALL_METAL,         // 2. Hoch, ohne Bohrung, metallisch
            WpType::FLAT,               // 3. Flaches Werkstück
            WpType::TALL_DRILLED_METAL  // 4. Hoch, mit Bohrung, metallisch
        };
    }

    ~TargetSequence() = default;

    /**
     * @brief Liefert den Werkstücktyp, der aktuell an der Reihe ist.
     * @return WpType Der erwartete Typ.
     */
    WpType getExpectedType() const {
        return m_sequence.at(m_currentIndex);
    }

    /**
     * @brief Prüft, ob das übergebene Werkstück dem aktuell erwarteten Rezept-Typ entspricht.
     * @param currentType Der physisch gemessene Typ des zu prüfenden Werkstücks.
     * @return true, wenn der Typ übereinstimmt (Teil darf passieren), andernfalls false.
     */
    bool isCorrectType(WpType currentType) const {
        return currentType == m_sequence.at(m_currentIndex);
    }

    /**
     * @brief Schaltet den internen Zeiger zum nächsten erwarteten Typ weiter.
     * @note Wird im WorkpieceManager aufgerufen, sobald eine erfolgreiche Weichenpassage (EJECTOR_CLEARED) bestätigt wurde.
     */
    void advanceSequence() {
        if (!m_sequence.empty()) {
            // Kompakter Ringpuffer-Sprung mittels Modulo-Operator
            m_currentIndex = (m_currentIndex + 1) % m_sequence.size();
            std::cout << "[TargetSequence] Rezeptur vorgerueckt. Naechster Index: " << m_currentIndex
                      << " (Erwarteter Typ-ID: " << static_cast<int>(getExpectedType()) << ")" << std::endl;
        }
    }

    /**
     * @brief Zwingt den Rezeptzeiger hart auf einen bestimmten Typen.
     * @note Wichtig für die netzwerkbasierte absolute Synchronisation durch Anlage 2.
     * @param type Der Typ, auf den das Rezept gesetzt werden soll.
     */
    void forceType(WpType type) {
        for (std::size_t i = 0; i < m_sequence.size(); ++i) {
            if (m_sequence.at(i) == type) {
                m_currentIndex = i;
                std::cout << "[TargetSequence] Rezept per Netzwerk-Befehl synchronisiert auf Index: "
                          << m_currentIndex << " (Typ-ID: " << static_cast<int>(type) << ")" << std::endl;
                return;
            }
        }
        std::cerr << "[TargetSequence] ERROR: Typ-ID " << static_cast<int>(type)
                  << " existiert nicht im Rezept!" << std::endl;
    }

    /**
     * @brief Setzt die Sequenz wieder an den Anfang des Rezepts (z.B. bei STOP, RESET oder E-STOP).
     */
    void resetSequence() {
        m_currentIndex = 0;
        std::cout << "[TargetSequence] Rezeptur auf Startposition zurueckgesetzt." << std::endl;
    }
};

#endif // TARGET_SEQUENCE_HPP
