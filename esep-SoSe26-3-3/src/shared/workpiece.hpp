/**
 * @file Workpiece.hpp
 *
 * @brief Klassenbeschreibung: Workpiece
 *
 * Die Klasse Workpiece ist ein reines Datenobjekt (Data Transfer Object), das
 * ein physisches Werkstück auf der Förderanlage als digitalen Zwilling repräsentiert.
 * Sie hält alle zustandsrelevanten Informationen eines Werkstücks während seines
 * Durchlaufs durch die Anlage.
 *
 * Hauptaufgaben (Zuständigkeiten):
 * - Identifikation: Automatische Generierung und Zuweisung einer eindeutigen ID
 *   bei Erstellung.
 * - Eigenschaftsspeicherung: Hält den ermittelten Werkstücktyp (WPType) und die
 *   Entscheidung der Logik, ob das Werkstück die Anlage passieren darf (canPass).
 * - Zonen-Tracking: Speichert den exakten aktuellen Aufenthaltsort (WpLocation)
 *   auf dem Förderband.
 * - Timeout-Überwachung: Führt interne Zeitstempel bei jedem Zonenwechsel, um
 *   verlorene oder manuell entfernte Werkstücke über die isLost()-Methode zu erkennen.
 */

#ifndef WORKPIECE_HPP
#define WORKPIECE_HPP

#include <chrono>

// Alle bekannten Werkstücktypen der Anlage
enum class WPType {
    UNKNOWN,             // wp_unknown (Startzustand)
    FLAT,                // wp_flat
    TALL,                // wp_tall
    TALL_DRILLED,        // wp_drilled_new/old
    TALL_DRILLED_METAL,  // wp_drilled_metal
    TALL_METAL           // wp_metal_new
};

enum class WpLocation {
    AT_ENTRY,             // Gerade am Einlauf (Lichtschranke blockiert)
    TRANSIT_TO_HEIGHT,    // Auf dem Weg zum Höhensensor
    AT_HEIGHT_SENSOR,     // Genau unter dem Höhensensor
    TRANSIT_TO_EJECTOR,   // Auf dem Weg zur Weiche
    AT_EJECTOR,           // An der Weiche
    TRANSIT_TO_END,       // Auf dem Weg zum Ende
    AT_END                // Am Bandende (Übergabe-bereit)
};

class workpiece {
private:
    // Statische Variable, um klassenübergreifend die nächste ID zu berechnen
    static int s_next_id;

    // Private Member-Variablen (sicher gekapselt)
    int m_id;
    WPType m_type;
    bool m_canPass;

    // Der aktuelle Ort des Werkstücks
    WpLocation m_location;
    // Der Zeitstempel, wann es diese Location erreicht hat
    std::chrono::steady_clock::time_point m_lastUpdateTime;

public:
    // Konstruktor: Vergibt automatisch die nächste freie ID und startet den Timer
    workpiece() :
        m_id(s_next_id++),
        m_type(WPType::UNKNOWN),
        m_canPass(false),
        m_location(WpLocation::AT_ENTRY),
        m_lastUpdateTime(std::chrono::steady_clock::now())
    {}

    // Getter für die ID (braucht keinen Setter, da ID unveränderlich sein sollte)
    int getId() const {
        return m_id;
    }

    // --- Getter & Setter für den Typ ---
    void setType(WPType type) {
        m_type = type;
    }

    WPType getType() const {
        return m_type;
    }

    // --- Getter & Setter für canPass ---
    void setCanPass(bool canPass) {
        m_canPass = canPass;
    }

    bool getCanPass() const {
        return m_canPass;
    }

    // --- Location Updaten ---
    void updateLocation(WpLocation newLocation) {
        m_location = newLocation;
        // Timer für diese neue Zone neu starten!
        m_lastUpdateTime = std::chrono::steady_clock::now();
    }

    WpLocation getLocation() const {
        return m_location;
    }

    // --- Verlorenes Werkstück Check ---
    bool isLost(long limitInMs) const {
        auto currentTime = std::chrono::steady_clock::now();
        auto elapsedTime = std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - m_lastUpdateTime).count();
        return elapsedTime > limitInMs;
    }
};

// Initialisierung der statischen ID-Variable (wichtig in C++!)
// Das 'inline' verhindert Multiple-Definition Fehler beim Linken (ab C++17).
inline int workpiece::s_next_id = 1;

#endif //WORKPIECE_HPP