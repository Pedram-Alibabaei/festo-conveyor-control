/**
 * @file CalibrationManager.cpp
 * @brief Konkrete Implementierung des Key-Value Kalibrierungs-Speichers.
 */

#include "CalibrationManager.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <sys/stat.h>

// ============================================================================
// KONSTRUKTOR
// ============================================================================
CalibrationManager::CalibrationManager(const std::string& defaultFilepath)
    : m_filepath(defaultFilepath) {
}

// ============================================================================
// DATEI-OPERATIONEN
// ============================================================================

void CalibrationManager::loadFromFile(const std::string& filepath) {
    m_filepath = filepath;
    clear(); // RAM vorher leeren, um Altlasten zu entfernen

    std::ifstream file(m_filepath);
    if (!file.is_open()) {
        std::cerr << "[CalibrationManager] Warnung: Datei '" << m_filepath
                  << "' konnte nicht geoeffnet werden. Starte mit leeren Werten." << std::endl;
        return;
    }

    std::string line;
    int lineCount = 0;
    while (std::getline(file, line)) {
        lineCount++;
        std::string key;
        int value = 0;

        if (parseLine(line, key, value)) {
            m_data[key] = value;
        }
    }

    std::cout << "[CalibrationManager] " << m_data.size() << " Werte erfolgreich aus '"
              << m_filepath << "' geladen (Zeilen gesamt: " << lineCount << ")." << std::endl;
}

void CalibrationManager::saveToFile(const std::string& filepath) {
    m_filepath = filepath;
    std::ofstream file(m_filepath);

    if (!file.is_open()) {
        std::cerr << "[CalibrationManager] FEHLER: Konnte Kalibrierungsdatei '"
                  << m_filepath << "' nicht zum Schreiben oeffnen!" << std::endl;
        return;
    }

    // Schreibt alle aktuellen Key-Value-Paare zeilenweise in die Datei
    file << "# ===================================================\n";
    file << "# FESTO MPS Station - Kalibrierungsdaten\n";
    file << "# Automatisch generiert vom CalibrationManager\n";
    file << "# ===================================================\n\n";

    for (const auto& pair : m_data) {
        file << pair.first << "=" << pair.second << "\n";
    }

    file.close();
    std::cout << "[CalibrationManager] " << m_data.size() << " Werte permanent in '"
              << m_filepath << "' gespeichert." << std::endl;
}

bool CalibrationManager::fileExists(const std::string& filepath) {
    struct stat buffer;
    return (stat(filepath.c_str(), &buffer) == 0);
}

void CalibrationManager::load() {
    loadFromFile(m_filepath);
}

void CalibrationManager::save() {
    saveToFile(m_filepath);
}

// ============================================================================
// DATEN-OPERATIONEN (RAM-Zugriff)
// ============================================================================

bool CalibrationManager::hasKey(const std::string& key) {
    return (m_data.find(key) != m_data.end());
}

int CalibrationManager::getInt(const std::string& key, int defaultValue) {
    auto it = m_data.find(key);
    if (it != m_data.end()) {
        return it->second; // Wert gefunden
    }
    // Falls der Wert fehlt, loggen wir eine Warnung und geben den Default zurück
    std::cerr << "[CalibrationManager] Warnung: Schluessel '" << key
              << "' nicht gefunden! Nutze Standardwert: " << defaultValue << std::endl;
    return defaultValue;
}

void CalibrationManager::setInt(const std::string& key, int value) {
    m_data[key] = value;
    std::cout << "[CalibrationManager] RAM-Update: " << key << " = " << value << std::endl;
}

void CalibrationManager::clear() {
    m_data.clear();
}

// ============================================================================
// PRIVATE HILFSMETHODEN (Der Parser)
// ============================================================================

bool CalibrationManager::parseLine(const std::string& line, std::string& outKey, int& outValue) {
    // 1. Leerzeilen oder Kommentare überspringen
    if (line.empty() || line[0] == '#' || line[0] == ';') {
        return false;
    }

    std::stringstream ss(line);
    std::string keyStr;
    std::string valueStr;

    // 2. An dem Gleichheitszeichen '=' trennen
    if (std::getline(ss, keyStr, '=') && std::getline(ss, valueStr)) {

        // Whitespaces am Anfang/Ende vom Key trimmen (Sicherheitshalber)
        keyStr.erase(0, keyStr.find_first_not_of(" \t\r\n"));
        keyStr.erase(keyStr.find_last_not_of(" \t\r\n") + 1);

        outKey = keyStr;

        try {
            // String-Wert in Ganzzahl (int) konvertieren
            outValue = std::stoi(valueStr);
            return true;
        } catch (const std::exception& e) {
            std::cerr << "[CalibrationManager] Parser-Fehler bei Wert von Key '"
                      << keyStr << "': " << e.what() << std::endl;
            return false;
        }
    }

    return false;
}