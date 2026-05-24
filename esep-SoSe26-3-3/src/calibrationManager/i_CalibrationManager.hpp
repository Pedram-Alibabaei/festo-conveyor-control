/**
 * @file I_CalibrationManager.hpp
 * @brief Flexibles Interface fuer die persistente Werte-Kalibrierung (Key-Value-Store).
 */

#ifndef I_CALIBRATION_MANAGER_HPP
#define I_CALIBRATION_MANAGER_HPP

#include <string>

namespace calib {
    // ========================================================================
    // ZENTRALE SCHLÜSSEL-NAMEN (Exakt nach deiner Event-Kette)
    // ========================================================================

    // 1. HÖHENMESSUNG (ADC-Werte & Fenster)
    const std::string BELT_HEIGHT_CENTER    = "Adc_BeltHeightCenter";
    const std::string FLAT_CENTER           = "Adc_FlatCenter";
    const std::string TRIGGER_VALUE         = "Adc_TriggerValue";
    const std::string RELEASE_TRIGGER_VALUE = "Adc_ReleaseTriggerValue";

    const std::string FLAT              = "Adc_FlatMin";

    const std::string TALL             = "Adc_TallMin";

    const std::string DRILLED           = "Adc_DrilledMin";


    // 2. ZEIT-TRACKING (Logische Band-Takte / Ticks)

    // --> Aus Durchlauf 1 (Normalgeschwindigkeit):
    const std::string TICKS_ENTRY_C_TO_HEIGHT_B  = "Ticks_EntryCleared_to_HeightBlocked";

    // --> Aus Durchlauf 2 (Langsame Geschwindigkeit):
    const std::string TICKS_HEIGHT_B_TO_HEIGHT_C = "Ticks_HeightBlocked_to_HeightCleared"; // Entspricht MUSTER_SAMPLE_COUNT
    const std::string TICKS_HEIGHT_C_TO_EJECTOR_B = "Ticks_HeightCleared_to_EjectorBlocked";
    const std::string TICKS_EJECTOR_B_TO_EJECTOR_C = "Ticks_EjectorBlocked_to_EjectorCleared";
    const std::string TICKS_EJECTOR_C_TO_END_B     = "Ticks_EjectorCleared_to_EndBlocked";
    const std::string TICKS_END_B_TO_END_C     = "Ticks_EndBlocked_to_EndCleared";

    // 3. ABGELEITETE FILTER-LIMITS (Aus Muster-Sample-Count berechnet)
    const std::string MIN_SAMPLES_FOR_CLASS  = "Limit_MinSamplesForClass";
    const std::string TRIGGER_SAMPLE_COUNT   = "Limit_TriggerSampleCount";
    const std::string TALL_CONFIRM_COUNT     = "Limit_TallConfirmCount";
    const std::string DRILLED_CONFIRM_COUNT  = "Limit_DrilledConfirmCount";
}

class i_CalibrationManager {
public:
    virtual ~i_CalibrationManager() = default;

    // ========================================================================
    // DATEI-OPERATIONEN (Erstellen, Suchen, Laden, Speichern)
    // ========================================================================

    /**
     * @brief Sucht und laedt die Textdatei. Falls sie nicht existiert, wird sie automatisch erstellt.
     * @param filepath Der Pfad zur Datei im QNX-System (z.B. "/root/calib.txt").
     */
    virtual void loadFromFile(const std::string& filepath) = 0;

    /**
     * @brief Speichert alle aktuellen Key-Value-Paare permanent in die Textdatei.
     */
    virtual void saveToFile(const std::string& filepath) = 0;

    /**
     * @brief Prueft, ob eine Kalibrierungsdatei an diesem Pfad existiert (Suchen).
     */
    virtual bool fileExists(const std::string& filepath) = 0;


    // ========================================================================
    // DATEN-OPERATIONEN (Lesen, Hinzufuegen, Bearbeiten, Suchen)
    // ========================================================================

    /**
     * @brief Sucht nach einem bestimmten Schluessel in den geladenen Daten.
     * @return true, wenn der Wert existiert, ansonsten false.
     */
    virtual bool hasKey(const std::string& key) = 0;

    /**
     * @brief Liest einen ganzzahligen Wert aus.
     * @param key Der eindeutige Name des Wertes aus dem calib:: Namespace.
     * @param defaultValue Rueckgabewert, falls der Schluessel nicht existiert.
     */
    virtual int getInt(const std::string& key, int defaultValue = 0) = 0;

    /**
     * @brief Fuegt einen neuen Wert hinzu ODER bearbeitet einen bestehenden Wert im RAM.
     * @param key Der eindeutige Name des Wertes.
     * @param value Der neue zu speichernde Wert.
     */
    virtual void setInt(const std::string& key, int value) = 0;

    /**
     * @brief Loescht alle aktuellen Werte aus dem temporaeren Speicher.
     */
    virtual void clear() = 0;
};

#endif // I_CALIBRATION_MANAGER_HPP