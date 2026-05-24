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
    std::string m_filepath;

    // Der RAM-Speicher (Key-Value Tabelle) für alle ADC- und Zeit-Schwellen
    std::map<std::string, int> m_data;

    /**
     * @brief Hilfsmethode zum Parsen einer einzelnen Zeile ("KEY=VALUE") aus der Textdatei.
     */
    bool parseLine(const std::string& line, std::string& outKey, int& outValue);

public:
    /**
     * @brief Konstruktor.
     * @param defaultFilepath Der Standardpfad zur Kalibrierungsdatei im QNX-System.
     */
    CalibrationManager(const std::string& defaultFilepath = "/root/festo_calibration.txt");

    ~CalibrationManager() override = default;

    // ========================================================================
    // DATEI-OPERATIONEN (Aus dem Interface)
    // ========================================================================

    /**
     * @brief Sucht und laedt die Textdatei von einem spezifischen Pfad.
     */
    void loadFromFile(const std::string& filepath) override;

    /**
     * @brief Speichert alle aktuellen RAM-Werte permanent in die angegebene Textdatei.
     */
    void saveToFile(const std::string& filepath) override;

    /**
     * @brief Prueft, ob an dem Pfad bereits eine Kalibrierung existiert.
     */
    bool fileExists(const std::string& filepath) override;

    /**
     * @brief Bequemlichkeits-Methode: Laedt die Daten vom im Konstruktor gesetzten Standardpfad.
     */
    void load();

    /**
     * @brief Bequemlichkeits-Methode: Speichert die Daten auf dem Standardpfad ab.
     */
    void save();

    // ========================================================================
    // DATEN-OPERATIONEN (Aus dem Interface)
    // ========================================================================

    /**
     * @brief Prueft, ob ein bestimmter Kalibrierwert (z.B. calib::TICKS_END_B_TO_END_C) existiert.
     */
    bool hasKey(const std::string& key) override;

    /**
     * @brief Holt einen Kalibrierwert aus dem RAM. Falls nicht kalibriert, greift der Default-Wert.
     */
    int getInt(const std::string& key, int defaultValue = 0) override;

    /**
     * @brief Setzt oder aktualisiert einen Wert im RAM (wird erst beim save() auf die SD-Karte geschrieben).
     */
    void setInt(const std::string& key, int value) override;

    /**
     * @brief Leert die gesamte Map im RAM.
     */
    void clear() override;
};

#endif // CALIBRATION_MANAGER_HPP