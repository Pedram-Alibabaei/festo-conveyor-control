/**
 * @file Logik.cpp
 * @brief Implementierung des zentralen Sch Schicht-1-Controllers (Zustandsmaschine).
 *
 */

#include "Logik.hpp"
#include "I_CalibrationManager.hpp" // Erlaubt Zugriff auf load/save Methoden
#include <iostream>

namespace logik {

Logik::Logik(i_Dispatcher* dispatcher)
    : m_wpManager(std::make_unique<WorkpieceManager>(this)),
      m_dispatcher(dispatcher),
      m_calibManager(nullptr),
      m_speedFactor(1.0),
      m_anlagenPosition(AnlagenPosition::UNKNOWN_POSITION),
      m_isPartnerChuteFull(false),
      m_isMotorSlow(false) // Sicher initialisiert
{
    std::cout << "[Logik] System-Instanz erfolgreich konstruiert." << std::endl;
}

void Logik::init(AnlagenPosition pos) {
    m_anlagenPosition = pos;
    m_wpManager->setAnlagenPosition(pos);
    std::cout << "[Logik] Initialisiert auf Position: " << static_cast<int>(pos) << std::endl;
}

void Logik::processEvent(logik::Event e) {
    switch (e) {
        case logik::Event::ENTRY_BLOCKED:
            if (m_anlagenPosition == AnlagenPosition::VORN_ANLAGE_1) {
                m_wpManager->addWorkpiece();
            }
            break;

        case logik::Event::ENTRY_CLEARED:
        case logik::Event::HEIGHT_BLOCKED:
        case logik::Event::HEIGHT_CLEARED:
        case logik::Event::HEIGHT_ERROR:
        case logik::Event::METAL_DETECTED:
        case logik::Event::METAL_CLEARED:
        case logik::Event::EJECTOR_BLOCKED:
        case logik::Event::EJECTOR_CLEARED:
        case logik::Event::END_BLOCKED:
        case logik::Event::END_CLEARED:
        case logik::Event::CHUTE_BLOCKED:
        case logik::Event::CHUTE_CLEARED:
            m_wpManager->handleSensorSignal(e);
            break;

        default:
            break;
    }
}

void Logik::workpieceTypeFromHS(WpType type) {
   m_wpManager->handleSensorSignal(logik::Event::HEIGHT_CLEARED, type);
}

void Logik::workpieceInfo(int id, WpType type) {
    if (m_anlagenPosition == AnlagenPosition::HINTEN_ANLAGE_2) {
        m_wpManager->addNetworkWorkpiece(id, type);
    } else {
        std::cerr << "[Logik-Netzwerk] WARNUNG: Anlage 1 hat faelschlicherweise ein workpieceInfo-Event empfangen!" << std::endl;
    }
}

void Logik::updateWpListe(WpType type) {
    m_wpManager->getSequence().forceType(type);
}

void Logik::takt() {

    m_wpManager->takt(m_isMotorSlow);
}

void Logik::setPartnerChuteStatus(bool isFull) {
    m_isPartnerChuteFull = isFull;
    if (m_anlagenPosition == AnlagenPosition::VORN_ANLAGE_1) {
        m_wpManager->setAnlage2Ready(!isFull);
    }
}

void Logik::loadCalibrationData() {
    if (m_calibManager == nullptr) {
        std::cerr << "[Logik] CRITICAL: CalibManager-Zeiger ist nullptr bei loadCalibrationData!" << std::endl;
        return;
    }

    m_calibData = m_calibManager->loadCalibrationData();

    recalculateSlowLimits();

    m_dispatcher->forwardCalibrationToHal(
        m_calibData.adc_emptyBelt,
        m_calibData.adc_flatCenter,
        m_calibData.limit_minSamplesForClass
    );

    m_wpManager->setCalibrationData(m_calibData);
}

void Logik::recalculateSlowLimits() {
    // Geschwindigkeitsfaktor mathematisch aus dem bekannten Segment A -> B ermitteln
    if (m_calibData.ticks_EntryC_to_HeightB_Normal > 0) {
        m_speedFactor = (double)m_calibData.ticks_EntryC_to_HeightB_Slow /
                        (double)m_calibData.ticks_EntryC_to_HeightB_Normal;
    }

    // Fehlende Schleichfahrt-Grenzwerte für die hinteren Abschnitte hochrechnen
    m_calibData.ticks_HeightC_to_EjectorB_Slow = m_calibData.ticks_HeightC_to_EjectorB_Normal * m_speedFactor;
    m_calibData.ticks_EjectorC_to_EndB_Slow    = m_calibData.ticks_EjectorC_to_EndB_Normal * m_speedFactor;

    std::cout << "[Logik] Live-Grenzwerte neu berechnet. Speed-Faktor: " << m_speedFactor << std::endl;
}

void Logik::setLiveAdcCalibration(uint16_t emptyBelt, uint16_t flatCenter, uint32_t minSamples) {
    // Gezieltes Ueberschreiben der ADC-Werte im lokalen Struct
    m_calibData.adc_emptyBelt = emptyBelt;
    m_calibData.adc_flatCenter = flatCenter;
    m_calibData.limit_minSamplesForClass = minSamples;

}

void Logik::saveCurrentCalibration() {
    if (m_calibManager == nullptr) {return;}
    bool success = m_calibManager->saveCalibrationData(m_calibData);
}

} // namespace logik