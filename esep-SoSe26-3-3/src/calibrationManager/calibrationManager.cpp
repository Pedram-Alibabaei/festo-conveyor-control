/**
 * @file CalibrationManager.cpp
 * @brief Implementierung des CalibrationManager für das QNX-Dateisystem.
 *
 */

#include "calibrationManager.hpp"
#include <iostream>
#include <fstream>
#include <sstream>

CalibrationManager::CalibrationManager(const std::string& filePath)
    : m_logik(nullptr),
      m_filePath(filePath)
{
    std::cout << "[Calib-Manager] Instanziert mit Pfad: " << m_filePath << std::endl;
}

void CalibrationManager::setLogikContext(i_Logik* logik) {
    m_logik = logik;
}

bool CalibrationManager::fileExists() {
    std::ifstream file(m_filePath);
    return file.good(); // Liefert true, wenn die Datei geoeffnet werden kann und existiert
}

CalibrationData CalibrationManager::loadCalibrationData() {
    CalibrationData data; // Wird mit Standard-Nullen initialisiert

    std::ifstream file(m_filePath);
    if (!file.is_open()) {
        std::cerr << "[Calib-Manager] ERROR: Datei konnte nicht geoeffnet werden: " << m_filePath << std::endl;
        return data;
    }

    std::string line;
    while (std::getline(file, line)) {
        // Ignoriere Leerzeilen oder Kommentare
        if (line.empty() || line[0] == '#') {
            continue;
        }

        std::istringstream is_line(line);
        std::string key;
        if (std::getline(is_line, key, '=')) {
            std::string valueStr;
            if (std::getline(is_line, valueStr)) {

                // Konvertierung des Strings in Zahlenwerte mittels std::stoul
                std::uint32_t val = static_cast<std::uint32_t>(std::stoul(valueStr));

                // Zuordnung zu den entsprechenden Struct-Feldern
                if (key == "adc_emptyBelt")                       data.adc_emptyBelt = static_cast<std::uint16_t>(val);
                else if (key == "adc_flatCenter")                 data.adc_flatCenter = static_cast<std::uint16_t>(val);
                else if (key == "limit_minSamplesForClass")       data.limit_minSamplesForClass = val;
                else if (key == "ticks_EntryC_to_HeightB_Slow")   data.ticks_EntryC_to_HeightB_Slow = val;
                else if (key == "ticks_HeightB_to_HeightC_Slow")  data.ticks_HeightB_to_HeightC_Slow = val;
                else if (key == "ticks_EntryC_to_HeightB_Normal") data.ticks_EntryC_to_HeightB_Normal = val;
                else if (key == "ticks_HeightB_to_HeightC_Normal") data.ticks_HeightB_to_HeightC_Normal = val;
                else if (key == "ticks_HeightC_to_EjectorB_Normal") data.ticks_HeightC_to_EjectorB_Normal = val;
                else if (key == "ticks_EjectorB_to_EjectorC_Normal") data.ticks_EjectorB_to_EjectorC_Normal = val;
                else if (key == "ticks_EjectorC_to_EndB_Normal")   data.ticks_EjectorC_to_EndB_Normal = val;
                else if (key == "ticks_EndB_to_EndC_Normal")       data.ticks_EndB_to_EndC_Normal = val;
            }
        }
    }

    file.close();
    std::cout << "[Calib-Manager] Kalibrierungsdatei erfolgreich geparst und geladen." << std::endl;
    return data;
}

bool CalibrationManager::saveCalibrationData(const CalibrationData& data) {
    std::ofstream file(m_filePath);

    if (!file.is_open()) {
        std::cerr << "[Calib-Manager] CRITICAL: Datei zum Speichern konnte nicht erstellt werden: "
                  << m_filePath << std::endl;
        return false;
    }

    // Header für die Textdatei schreiben (erleichtert die Lesbarkeit auf dem QNX-Terminal)
    file << "# ==================\n";
    file << "# Kalibrierungsdaten\n";
    file << "# SoSe2026 Team 3.3 \n";
    file << "# ==================\n\n";

    file << "[Analog-Thresholds]\n";
    file << "adc_emptyBelt=" << data.adc_emptyBelt << "\n";
    file << "adc_flatCenter=" << data.adc_flatCenter << "\n";
    file << "limit_minSamplesForClass=" << data.limit_minSamplesForClass << "\n\n";

    file << "[Normal-Speed-Ticks]\n";
    file << "ticks_EntryC_to_HeightB_Normal=" << data.ticks_EntryC_to_HeightB_Normal << "\n";
    file << "ticks_HeightB_to_HeightC_Normal=" << data.ticks_HeightB_to_HeightC_Normal << "\n";
    file << "ticks_HeightC_to_EjectorB_Normal=" << data.ticks_HeightC_to_EjectorB_Normal << "\n";
    file << "ticks_EjectorB_to_EjectorC_Normal=" << data.ticks_EjectorB_to_EjectorC_Normal << "\n";
    file << "ticks_EjectorC_to_EndB_Normal=" << data.ticks_EjectorC_to_EndB_Normal << "\n";
    file << "ticks_EndB_to_EndC_Normal=" << data.ticks_EndB_to_EndC_Normal << "\n\n";

    file << "[Slow-Speed-Base-Ticks]\n";
    file << "ticks_EntryC_to_HeightB_Slow=" << data.ticks_EntryC_to_HeightB_Slow << "\n";
    file << "ticks_HeightB_to_HeightC_Slow=" << data.ticks_HeightB_to_HeightC_Slow << "\n";

    file.close();
    std::cout << "[Calib-Manager] Alle Kalibrierungsdaten dauerhaft gesichert in: " << m_filePath << std::endl;
    return true;
}
