/**
 * @file CalibrationManager.hpp
 * @brief Konkrete Implementierung des CalibrationManager für das QNX-Dateisystem.
 *
 * @details Diese Klasse liest die Kalibrierungsdaten (ADC-Werte und Band-Ticks)
 * zeilenweise aus einer Textdatei aus und konvertiert diese in das typsichere
 * CalibrationData-Struct. Nach dem Service-Modus schreibt sie die geänderten
 * Werte formatiert zurück.
 *
 * Embedded Systems Engineering WiSe2025 Team 1.1
 */

#ifndef CALIBRATION_MANAGER_HPP
#define CALIBRATION_MANAGER_HPP

#include "I_CalibrationManager.hpp"
#include <string>

class CalibrationManager : public I_CalibrationManager {
private:
    i_Logik* m_logik;                 ///< Zeiger auf den Logik-Kontext (Dependency Injection)
    std::string m_filePath;           ///< Der absolute Speicherpfad zur Kalibrierungsdatei
    bool m_validDataCached = false;     ///< true, wenn m_cachedData aus der Datei gelesen wurde
    CalibrationData m_cachedData;       ///< Zuletzt validierte Kalibrierdaten (Boot-Cache)

    bool readValidCalibration(CalibrationData& out);

public:
    /** Persistenter Standardpfad auf dem QNX-Target (nicht /tmp). */
    static const char CALIBRATION_FILE_NAME[];

    /**
     * @brief Konstruktor.
     * @details Initialisiert den Manager und setzt den Standard-Dateipfad für die Anlage fest.
     * @param filePath Optionaler benutzerdefinierter Pfad, standardmaessig CALIBRATION_FILE_NAME.
     */
    CalibrationManager(const std::string& filePath = CALIBRATION_FILE_NAME);

    /**
     * @brief Destruktor.
     */
    ~CalibrationManager() override = default;

    // ========================================================================
    // IMPLEMENTIERUNG DER I_CALIBRATION_MANAGER SCHNITTSTELLE
    // ========================================================================

    /**
     * @brief Setzt den Zeiger auf die Logik-Komponente.
     * @param logik Zeiger auf das i_Logik-Interface.
     */
    void setLogikContext(i_Logik* logik) override;

    /**
     * @brief Prueft, ob die Kalibrierungsdatei existiert und gueltige Inhalte hat.
     * @details Oeffnet die Datei, parst die Schluessel-Wert-Paare und validiert
     * ADC-Schwellen sowie Band-Ticks auf Vollstaendigkeit und Plausibilitaet.
     * @return true, wenn die Datei existiert und valide Kalibrierdaten enthaelt.
     */
    bool fileExists() override;

    /**
     * @brief Liest die Kalibrierungsdaten aus der Datei und liefert das befüllte Struct.
     * @details Öffnet die Datei, parst die Schlüssel-Wert-Paare und befüllt das CalibrationData-Struct.
     * Falls das Lesen fehlschlägt oder die Datei korrupt ist, werden sichere Labor-Fallback-Werte geladen.
     * @return Das vollständig befüllte CalibrationData-Struct.
     */
    CalibrationData loadCalibrationData() override;

    /**
     * @brief Schreibt die übergebenen Kalibrierungsdaten strukturiert zurück in die Datei.
     * @details Erstellt oder überschreibt die Datei und sichert alle Felder im Format "Schluessel=Wert".
     * @param data Das aktuelle Datenpaket mit allen Live-ADC-Werten und gelernten Ticks.
     * @return true, wenn der Schreibvorgang erfolgreich war, sonst false.
     */
    bool saveCalibrationData(const CalibrationData& data) override;
};

#endif // CALIBRATION_MANAGER_HPP