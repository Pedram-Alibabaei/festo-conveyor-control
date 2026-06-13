/**
 * @file I_CalibrationManager.hpp
 * @brief Interface für die Datei-Persistierung aller Kalibrierungs- und Anlagendaten.
 *
 * @details Diese Komponente bildet die Abstraktionsschicht zum Dateisystem der Festo-Anlage.
 * Sie ist exklusiv dafür zuständig, die Kalibrierungswerte (ADC-Schwellen und Band-Ticks)
 * beim Systemstart aus einer Datei zu laden und nach dem Service-Modus wieder abzuspeichern.
 *
 * Embedded Systems Engineering WiSe2025 Team 1.1
 */

#ifndef I_CALIBRATION_MANAGER_HPP
#define I_CALIBRATION_MANAGER_HPP

#include "shared/types.hpp" // Enthält das CalibrationData Struct
#include "logik/i_Logik.hpp" // Erlaubt die Übergabe an die Logik-Schicht

class I_CalibrationManager {
public:
    virtual ~I_CalibrationManager() = default;

    /**
     * @brief Setzt den Zeiger auf die Logik-Komponente (Dependency Injection).
     * @note Wichtig, damit der CalibrationManager beim Booten die geladenen Daten
     * aktiv per setCalibrationData() an die Steuerung übergeben kann.
     * @param logik Zeiger auf das i_Logik-Interface.
     */
    virtual void setLogikContext(i_Logik* logik) = 0;

    /**
     * @brief Prueft, ob eine gueltige Kalibrierungsdatei auf dem Standardpfad existiert.
     * @note Wird direkt beim Systemstart aufgerufen, um der FSM die Events
     * DATA_EXIST oder DATA_NOT_EXIST zu liefern.
     * @details Die Datei muss existieren, parsebar sein und alle Pflichtfelder
     * mit plausiblen Werten enthalten.
     * @return true, wenn die Datei existiert und gueltige Kalibrierdaten enthaelt.
     */
    virtual bool fileExists() = 0;

    /**
     * @brief Läuft beim Systemstart und liest die Kalibrierungsdaten aus der Datei.
     * @details Holt die Text-/JSON-Daten von der QNX-Festplatte und konvertiert sie
     * in das typsichere CalibrationData-Struct.
     * @return Das vollständig befüllte CalibrationData-Struct. Falls keine Datei
     * existiert, wird ein Struct mit sicheren Labor-Standardwerten zurückgegeben.
     */
    virtual CalibrationData loadCalibrationData() = 0;

    /**
     * @brief Schreibt die übergebenen Kalibrierungsdaten dauerhaft in die Datei.
     * @note Wird am Ende des Service-Modus aufgerufen, wenn der Bediener "Speichern" drückt.
     * @param data Das aktuelle Datenpaket mit allen Live-ADC-Werten und gelernten Ticks.
     * @return true, wenn der Schreibvorgang auf die Festplatte erfolgreich war, sonst false.
     */
    virtual bool saveCalibrationData(const CalibrationData& data) = 0;
};

#endif // I_CALIBRATION_MANAGER_HPP