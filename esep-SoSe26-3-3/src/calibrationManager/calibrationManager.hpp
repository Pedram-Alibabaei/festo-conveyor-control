/**
 * @file CalibrationManager.hpp
 * @brief Konkrete Implementierung des Key-Value Kalibrierungs-Speichers.
 */

#ifndef CALIBRATION_MANAGER_HPP
#define CALIBRATION_MANAGER_HPP

#include "I_CalibrationManager.hpp"
#include <map>
#include <string>

class CalibrationManager : public i_CalibrationManager {
private:
    // Der intern genutzte Pfad zur Kalibrierungsdatei (z.B. auf der SD-Karte)
    std::string m_filepath;

    // Der RAM-Speicher (Key-Value Tabelle) für alle ADC- und Zeit-Schwellen
    std::map<std::string, int> m_data;

    /**
     * @brief Hilfsmethode zum Parsen einer einzelnen Zeile ("KEY=VALUE") aus der Textdatei.
     */
    bool parseLine(const std::string& line, std::string& outKey, int& outValue);

public:
    /**
     * @brief Konstruktor. Setzt den Standardpfad, der für alle Datei-Operationen genutzt wird.
     * @param defaultFilepath Der Pfad zur Datei im QNX-System.
     */
    CalibrationManager(const std::string& defaultFilepath = "/root/festo_calibration.txt");

    ~CalibrationManager() override = default;

    // ========================================================================
    // DATEI-OPERATIONEN (Aus dem Interface - Pfadunabhängig)
    // ========================================================================

    /**
     * @brief Lädt die Kalibrierungsdatei vom intern gesetzten Standardpfad.
     */
    void load() override;

    /**
     * @brief Speichert alle aktuellen RAM-Werte permanent auf den Standardpfad.
     */
    void save() override;

    /**
     * @brief Prüft, ob die Kalibrierungsdatei auf dem Standardpfad existiert.
     */
    bool fileExists() override;

    /**
     * @brief Verpackt alle geladenen Werte als spezifische Pulse und schickt sie
     * zur Verteilung an den Dispatcher.
     * @param dispatcherCoid Der Verbindungs-Kanal (Connection ID) zum Dispatcher.
     */
    void distributeValues(int dispatcherCoid) override;

    // ========================================================================
    // DATEN-OPERATIONEN (Aus dem Interface)
    // ========================================================================

    /**
     * @brief Prüft, ob ein bestimmter Kalibrierwert im RAM existiert.
     */
    bool hasKey(const std::string& key) override;

    /**
     * @brief Holt einen Kalibrierwert aus dem RAM. Falls nicht vorhanden, greift der Default-Wert.
     */
    int getInt(const std::string& key, int defaultValue = 0) override;

    /**
     * @brief Setzt oder aktualisiert einen Wert im RAM.
     */
    void setInt(const std::string& key, int value) override;

    /**
     * @brief Leert die gesamte Map im RAM.
     */
    void clear() override;
};

#endif // CALIBRATION_MANAGER_HPP