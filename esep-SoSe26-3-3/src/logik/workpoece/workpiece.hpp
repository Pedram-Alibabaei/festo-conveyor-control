/**
 * @file Workpiece.hpp
 *
 * @brief Klassenbeschreibung: Workpiece
 *
 * Die Klasse Workpiece ist ein reines Datenobjekt (Data Transfer Object), das
 * ein physisches Werkstück auf der Förderanlage als digitalen Zwilling repräsentiert.
 * Sie hält ausschließlich zustandsrelevante Informationen eines Werkstücks während
 * seines Durchlaufs durch die Anlage.
 *
 * @details ARCHITEKTUR-HINWEIS:
 * Diese Klasse enthält bewusst KEINE eigene Zeit- oder Timeout-Logik mehr.
 * In unserer ereignisgesteuerten (Event-Driven) QNX-Architektur übernimmt das
 * Betriebssystem (mittels Hardware-Timern und Pulses) die Überwachung von
 * verlorenen Werkstücken.
 *
 * Hauptaufgaben (Zuständigkeiten):
 * - Identifikation: Automatische Generierung einer eindeutigen ID.
 * - Eigenschaftsspeicherung: Hält den ermittelten Werkstücktyp (WPType) und die
 *   Sortierentscheidung (canPass).
 * - Zonen-Tracking: Speichert den exakten aktuellen Aufenthaltsort (WpLocation).
 */

#ifndef WORKPIECE_HPP
#define WORKPIECE_HPP
#include <shared/types.hpp>
// Alle physischen Zonen/Abschnitte auf dem Förderband
enum class WpLocation {
    AT_ENTRY,             // Gerade am Einlauf (Lichtschranke blockiert)
    TRANSIT_TO_HEIGHT,    // Auf dem Weg zum Höhensensor
    AT_HEIGHT_SENSOR,     // Genau unter dem Höhensensor (wird gerade vermessen)
    TRANSIT_TO_EJECTOR,   // Auf dem Weg zur Weiche
    AT_EJECTOR,           // An der Weiche (Entscheidungspunkt)
    TRANSIT_TO_END,       // Auf dem Weg zum Ende (darf passieren)
    AT_END                // Am Bandende (Übergabe-bereit an Anlage 2)
};

class Workpiece {
private:
    // Statische Variable, um klassenübergreifend die nächste ID zu berechnen
    static int s_next_id;

    // --- Private Member-Variablen (sicher gekapselt) ---
    int m_id;
    WpType m_type;
    bool m_canPass;
    WpLocation m_location;

public:
    /**
     * @brief Konstruktor: Vergibt automatisch die nächste freie ID und
     * setzt das Werkstück an den Einlauf.
     */
    Workpiece() :
        m_id(s_next_id++),
        m_type(WPType::UNKNOWN),
        m_canPass(false),
        m_location(WpLocation::AT_ENTRY)
    {}

    // --- Getter für die ID ---
    // (Kein Setter, da die ID nach der Erstellung unveränderlich ist!)
    int getId() const {
        return m_id;
    }

    // --- Getter & Setter für den Werkstücktyp ---
    void setType(WPType type) {
        m_type = type;
    }

    WPType getType() const {
        return m_type;
    }

    // --- Getter & Setter für die Passier-Erlaubnis ---
    void setCanPass(bool canPass) {
        m_canPass = canPass;
    }

    bool getCanPass() const {
        return m_canPass;
    }

    // --- Getter & Setter für den Aufenthaltsort ---
    void updateLocation(WpLocation newLocation) {
        m_location = newLocation;
    }

    WpLocation getLocation() const {
        return m_location;
    }
};

// Initialisierung der statischen ID-Variable.
// Das 'inline' (ab C++17) verhindert Multiple-Definition Fehler beim Linken,
// da der Compiler dafür sorgt, dass diese Variable nur einmal im Speicher existiert.
inline int Workpiece::s_next_id = 1;

#endif // WORKPIECE_HPP