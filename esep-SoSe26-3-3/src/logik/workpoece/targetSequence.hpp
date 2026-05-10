/**
 * @file TargetSequence.hpp
 *
 * @brief Klassenbeschreibung: TargetSequence
 *
 * Die Klasse TargetSequence verwaltet die zyklische Soll-Reihenfolge der Werkstücke
 * für den Sortierprozess zwischen Anlage 1 und Anlage 2. Sie fungiert als
 * zentrales Regelwerk (Strategy Pattern) für die Weichenschaltung.
 *
 * Hauptaufgaben (Zuständigkeiten):
 * - Sequenz-Definition: Hält die fest definierte Reihenfolge (Ringpuffer), in der
 *   Anlage 2 die Werkstücke zwingend erwartet (Nicht-metallisch mit Bohrung ->
 *   Metallisch ohne Bohrung -> Flach -> Metallisch mit Bohrung).
 * - Validierung: Bietet der Logik eine schnelle Prüf-Schnittstelle (isCorrectType()),
 *   um zu entscheiden, ob ein Werkstück die Weiche passieren darf (canPass = true)
 *   oder aussortiert werden muss.
 * - Fortschrittssteuerung: Schaltet den internen Zeiger (Ampel) zum nächsten
 *   erwarteten Typ weiter, sobald eine erfolgreiche Materialübergabe an Anlage 2
 *   bestätigt wurde.
 */

#ifndef TARGET_SEQUENCE_HPP
#define TARGET_SEQUENCE_HPP

#include <vector>
#include "Workpiece.hpp" // Enthält euer WPType Enum

class targetSequence { // <-- Korrigiert: Großes 'T' für C++ Klassen!
private:
    std::vector<WPType> m_sequence;
    size_t m_currentIndex;

public:
    // Konstruktor: Baut die geforderte Reihenfolge auf
    targetSequence() {
        m_sequence = {
            WPType::TALL_DRILLED,       // 1. Nicht-metallisch mit Bohrung
            WPType::TALL_METAL,         // 2. Metallisch ohne Bohrung
            WPType::FLAT,               // 3. Flaches Werkstück
            WPType::TALL_DRILLED_METAL  // 4. Metallisch mit Bohrung
        };
        m_currentIndex = 0;
    }

    // Welches Werkstück wird JETZT gerade erwartet?
    WPType getExpectedType() const {
        return m_sequence[m_currentIndex];
    }

    // Prüft, ob das aktuell auf dem Band liegende Werkstück das richtige ist
    bool isCorrectType(WPType currentType) const {
        return currentType == m_sequence[m_currentIndex];
    }

    // Wird aufgerufen, wenn das Werkstück erfolgreich das Ende von Anlage 2 erreicht hat!
    // Schaltet die Ampel sozusagen auf das nächste Werkstück um.
    void advanceSequence() {
        m_currentIndex++;
        // Wenn wir am Ende der Liste sind, wieder bei 0 anfangen (Ringpuffer-Verhalten)
        if (m_currentIndex >= m_sequence.size()) {
            m_currentIndex = 0; 
        }
    }

    // Wird aufgerufen, wenn jemand die RESET-Taste drückt
    void resetSequence() {
        m_currentIndex = 0;
    }
};

#endif // TARGET_SEQUENCE_HPP