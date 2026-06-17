/**
 * @file CalibrationManager.cpp
 * @brief Implementierung des CalibrationManager für das QNX-Dateisystem.
 *
 */

#include "CalibrationManager.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <unistd.h>
#include <limits.h>

namespace {

std::string resolveCalibrationPath(const std::string& filePath) {
    if (!filePath.empty() && filePath[0] == '/') {
        return filePath;
    }

    char cwd[PATH_MAX];
    if (getcwd(cwd, sizeof(cwd)) != nullptr) {
        return std::string(cwd) + "/" + filePath;
    }

    return filePath;
}

constexpr std::uint16_t ADC_MAX_VALUE = 4095;
constexpr std::uint32_t MAX_SEGMENT_TICKS = 600000;

bool assignCalibrationValue(const std::string& key, std::uint32_t val, CalibrationData& data) {
    if (key == "adc_emptyBelt") {
        data.adc_emptyBelt = static_cast<std::uint16_t>(val);
        return true;
    }
    if (key == "adc_flatCenter") {
        data.adc_flatCenter = static_cast<std::uint16_t>(val);
        return true;
    }
    if (key == "limit_minSamplesForClass") {
        data.limit_minSamplesForClass = val;
        return true;
    }
    if (key == "ticks_EntryC_to_HeightB_Slow") {
        data.ticks_EntryC_to_HeightB_Slow = val;
        return true;
    }
    if (key == "ticks_HeightB_to_HeightC_Slow") {
        data.ticks_HeightB_to_HeightC_Slow = val;
        return true;
    }
    if (key == "ticks_HeightC_to_EjectorB_Slow") {
        data.ticks_HeightC_to_EjectorB_Slow = val;
        return true;
    }
    if (key == "ticks_EjectorB_to_EjectorC_Slow") {
        data.ticks_EjectorB_to_EjectorC_Slow = val;
        return true;
    }
    if (key == "ticks_EjectorC_to_ChuteB_Slow") {
        data.ticks_EjectorC_to_ChuteB_Slow = val;
        return true;
    }
    if (key == "ticks_EntryB_to_EntryC_Normal") {
        data.ticks_EntryB_to_EntryC_Normal = val;
        return true;
    }
    if (key == "ticks_EntryC_to_HeightB_Normal") {
        data.ticks_EntryC_to_HeightB_Normal = val;
        return true;
    }
    if (key == "ticks_HeightB_to_HeightC_Normal") {
        data.ticks_HeightB_to_HeightC_Normal = val;
        return true;
    }
    if (key == "ticks_HeightC_to_EjectorB_Normal") {
        data.ticks_HeightC_to_EjectorB_Normal = val;
        return true;
    }
    if (key == "ticks_EjectorB_to_EjectorC_Normal") {
        data.ticks_EjectorB_to_EjectorC_Normal = val;
        return true;
    }
    if (key == "ticks_EjectorC_to_EndB_Normal") {
        data.ticks_EjectorC_to_EndB_Normal = val;
        return true;
    }

    return false;
}

bool isCalibrationDataValid(const CalibrationData& data) {
    if (data.adc_emptyBelt == 0 || data.adc_emptyBelt > ADC_MAX_VALUE) {
        std::cerr << "[Calib-Manager] Validierung fehlgeschlagen: adc_emptyBelt="
                  << data.adc_emptyBelt << std::endl;
        return false;
    }
    if (data.adc_flatCenter == 0 || data.adc_flatCenter > ADC_MAX_VALUE) {
        std::cerr << "[Calib-Manager] Validierung fehlgeschlagen: adc_flatCenter="
                  << data.adc_flatCenter << std::endl;
        return false;
    }
    if (data.adc_flatCenter >= data.adc_emptyBelt) {
        std::cerr << "[Calib-Manager] Validierung fehlgeschlagen: adc_flatCenter >= adc_emptyBelt ("
                  << data.adc_flatCenter << " >= " << data.adc_emptyBelt
                  << ", erwartet: flatCenter < emptyBelt)" << std::endl;
        return false;
    }
    if (data.limit_minSamplesForClass == 0) {
        std::cerr << "[Calib-Manager] Validierung fehlgeschlagen: limit_minSamplesForClass=0"
                  << std::endl;
        return false;
    }

    struct SegmentCheck {
        const char* name;
        std::uint32_t value;
    };

    const SegmentCheck segmentTicks[] = {
        {"ticks_EntryB_to_EntryC_Normal", data.ticks_EntryB_to_EntryC_Normal},
        {"ticks_EntryC_to_HeightB_Normal", data.ticks_EntryC_to_HeightB_Normal},
        {"ticks_HeightB_to_HeightC_Normal", data.ticks_HeightB_to_HeightC_Normal},
        {"ticks_HeightC_to_EjectorB_Normal", data.ticks_HeightC_to_EjectorB_Normal},
        {"ticks_EjectorB_to_EjectorC_Normal", data.ticks_EjectorB_to_EjectorC_Normal},
        {"ticks_EjectorC_to_EndB_Normal", data.ticks_EjectorC_to_EndB_Normal},
        {"ticks_EntryC_to_HeightB_Slow", data.ticks_EntryC_to_HeightB_Slow},
        {"ticks_HeightB_to_HeightC_Slow", data.ticks_HeightB_to_HeightC_Slow},
        {"ticks_HeightC_to_EjectorB_Slow", data.ticks_HeightC_to_EjectorB_Slow},
        {"ticks_EjectorB_to_EjectorC_Slow", data.ticks_EjectorB_to_EjectorC_Slow},
        {"ticks_EjectorC_to_ChuteB_Slow", data.ticks_EjectorC_to_ChuteB_Slow}
    };

    for (const SegmentCheck& segment : segmentTicks) {
        if (segment.value == 0 || segment.value > MAX_SEGMENT_TICKS) {
            std::cerr << "[Calib-Manager] Validierung fehlgeschlagen: "
                      << segment.name << "=" << segment.value << std::endl;
            return false;
        }
    }

    return true;
}

bool parseCalibrationFile(std::istream& file, CalibrationData& data) {
    std::string line;
    bool foundKnownKey = false;

    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') {
            continue;
        }

        // Abschnitts-Header wie [Normal-Speed-Ticks] ueberspringen
        if (line.front() == '[') {
            continue;
        }

        std::istringstream is_line(line);
        std::string key;
        if (!std::getline(is_line, key, '=')) {
            continue;
        }

        std::string valueStr;
        if (!std::getline(is_line, valueStr) || valueStr.empty()) {
            std::cerr << "[Calib-Manager] WARN: Leerer Wert fuer Schluessel '" << key << "'." << std::endl;
            return false;
        }

        try {
            const std::uint32_t val = static_cast<std::uint32_t>(std::stoul(valueStr));
            if (assignCalibrationValue(key, val, data)) {
                foundKnownKey = true;
            }
        } catch (const std::invalid_argument&) {
            std::cerr << "[Calib-Manager] WARN: Ungueltiger Zahlenwert fuer '" << key << "': "
                      << valueStr << std::endl;
            return false;
        } catch (const std::out_of_range&) {
            std::cerr << "[Calib-Manager] WARN: Zahlenwert ausserhalb des Bereichs fuer '" << key << "': "
                      << valueStr << std::endl;
            return false;
        }
    }

    return foundKnownKey;
}

} // namespace

const char CalibrationManager::CALIBRATION_FILE_NAME[] = "festo_calabrationt_team_3_3.txt";

CalibrationManager::CalibrationManager(const std::string& filePath)
    : m_logik(nullptr),
      m_filePath(resolveCalibrationPath(filePath))
{
    std::cout << "[Calib-Manager] Instanziert mit Pfad: " << m_filePath << std::endl;
}

void CalibrationManager::setLogikContext(i_Logik* logik) {
    m_logik = logik;
}

bool CalibrationManager::readValidCalibration(CalibrationData& out) {
    if (m_validDataCached) {
        out = m_cachedData;
        return true;
    }

    std::ifstream file(m_filePath);
    if (!file.is_open()) {
        return false;
    }

    CalibrationData data;
    if (!parseCalibrationFile(file, data) || !isCalibrationDataValid(data)) {
        return false;
    }

    m_cachedData = data;
    m_validDataCached = true;
    out = data;
    return true;
}

bool CalibrationManager::fileExists() {
    CalibrationData data;
    if (!readValidCalibration(data)) {
        std::ifstream probe(m_filePath);
        if (probe.is_open()) {
            std::cerr << "[Calib-Manager] Datei vorhanden, aber Kalibrierdaten ungueltig: "
                      << m_filePath << std::endl;
        }
        return false;
    }

    return true;
}

CalibrationData CalibrationManager::loadCalibrationData() {
    CalibrationData data;
    if (readValidCalibration(data)) {
        std::cout << "[Calib-Manager] Kalibrierungsdatei erfolgreich geparst und validiert." << std::endl;
        return data;
    }

    std::ifstream file(m_filePath);
    if (!file.is_open()) {
        std::cerr << "[Calib-Manager] ERROR: Datei konnte nicht geoeffnet werden: " << m_filePath << std::endl;
    } else {
        std::cerr << "[Calib-Manager] ERROR: Kalibrierungsdatei fehlt oder enthaelt ungueltige Werte." << std::endl;
    }

    return CalibrationData{};
}

bool CalibrationManager::saveCalibrationData(const CalibrationData& data) {
    if (!isCalibrationDataValid(data)) {
        std::cerr << "[Calib-Manager] CRITICAL: Speichern abgebrochen, Kalibrierdaten ungueltig." << std::endl;
        return false;
    }

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
    file << "ticks_EntryB_to_EntryC_Normal=" << data.ticks_EntryB_to_EntryC_Normal << "\n";
    file << "ticks_EntryC_to_HeightB_Normal=" << data.ticks_EntryC_to_HeightB_Normal << "\n";
    file << "ticks_HeightB_to_HeightC_Normal=" << data.ticks_HeightB_to_HeightC_Normal << "\n";
    file << "ticks_HeightC_to_EjectorB_Normal=" << data.ticks_HeightC_to_EjectorB_Normal << "\n";
    file << "ticks_EjectorB_to_EjectorC_Normal=" << data.ticks_EjectorB_to_EjectorC_Normal << "\n";
    file << "ticks_EjectorC_to_EndB_Normal=" << data.ticks_EjectorC_to_EndB_Normal << "\n\n";

    file << "[Slow-Speed-Ticks]\n";
    file << "ticks_EntryC_to_HeightB_Slow=" << data.ticks_EntryC_to_HeightB_Slow << "\n";
    file << "ticks_HeightB_to_HeightC_Slow=" << data.ticks_HeightB_to_HeightC_Slow << "\n";
    file << "ticks_HeightC_to_EjectorB_Slow=" << data.ticks_HeightC_to_EjectorB_Slow << "\n";
    file << "ticks_EjectorB_to_EjectorC_Slow=" << data.ticks_EjectorB_to_EjectorC_Slow << "\n";
    file << "ticks_EjectorC_to_ChuteB_Slow=" << data.ticks_EjectorC_to_ChuteB_Slow << "\n";

    file.flush();
    file.close();

    if (!file.good()) {
        std::cerr << "[Calib-Manager] CRITICAL: Schreiben fehlgeschlagen: " << m_filePath << std::endl;
        return false;
    }

    m_cachedData = data;
    m_validDataCached = true;
    std::cout << "[Calib-Manager] Alle Kalibrierungsdaten dauerhaft gesichert in: " << m_filePath << std::endl;
    return true;
}
