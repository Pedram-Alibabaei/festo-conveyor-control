/**
 * @file Logik.cpp
 * @brief Implementierung des zentralen Sch Schicht-1-Controllers (Zustandsmaschine).
 *
 * SoSe2026 Team 3.3
 */

#include "Logik.hpp"
#include "calibrationManager/I_CalibrationManager.hpp"
#include "fsm/states/BaseState.hpp"
#include <iostream>
#include <memory>

namespace logik {

    Logik::Logik(I_Dispatcher* dispatcher)
        : m_wpManager(std::make_unique<WorkpieceManager>(this)),
          m_errorHandler(std::make_unique<ErrorHandler>(this)), // 1. ANPASSUNG: ErrorHandler sauber instanziieren
          m_dispatcher(dispatcher),
          m_calibManager(nullptr),
          m_eventManager(nullptr),
          m_anlagenPosition(AnlagenPosition::UNKNOWN_POSITION),
          m_isPartnerChuteFull(false),
          m_isMotorSlow(false)
    {
        std::cout << "[Logik] System-Instanz inklusive ErrorHandler erfolgreich konstruiert." << std::endl;
    }

void Logik::init(AnlagenPosition pos) {
    m_anlagenPosition = pos;
    m_wpManager->setAnlagenPosition(pos);
    std::cout << "[Logik] Initialisiert auf Position: " << static_cast<int>(pos) << std::endl;
}

void Logik::transitionTo(BaseState* nextState) {
    if (m_eventManager != nullptr) {
        m_eventManager->transitionFsmState(std::unique_ptr<BaseState>(nextState));
    } else {
        delete nextState;
    }
}

// ========================================================================
// DEIN PERFEKTER EVENT-ROUTER (KORRIGIERT AUF PFEIL-OPERATOR)
// ========================================================================
void Logik::processEvent(logik::Event e) {
    if (m_eventManager != nullptr) {
        m_eventManager->handleEvent(e);
    }
}

    void Logik::workpieceTypeFromHS(WpType type) {
        if (m_eventManager != nullptr) {
           m_eventManager->handleEvent(logik::Event::HEIGHT_CLEARED);
            m_wpManager->handleSensorSignal(logik::Event::HEIGHT_CLEARED, type);
        }
    }

void Logik::workpieceInfo(int id, WpType type) {
    if (m_anlagenPosition == AnlagenPosition::HINTEN_ANLAGE_2) {
        if (m_wpManager != nullptr) {
            m_wpManager->addNetworkWorkpiece(id, type);
        }
    }
}

void Logik::updateWpListe(WpType type) {
    m_wpManager->getSequence().forceType(type);
}

void Logik::takt() {
    // Die Logik leitet JEDEN Herzschlag stur an den EventManager weiter
    if (m_eventManager != nullptr) {
        m_eventManager->handleTakt();
    }

    // Der ErrorHandler muss IMMER laufen, um z.B. CHUTE_CLEARED im Stand zu erkennen
    if (m_errorHandler != nullptr) {
        m_errorHandler->takt();
    }
}

void Logik::setPartnerChuteStatus(bool isFull) {
    if (m_eventManager != nullptr) {
        m_eventManager->setPartnerChuteStatus(isFull);
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
    double m_speedFactor;
    if (m_calibData.ticks_EntryC_to_HeightB_Normal > 0) {
       m_speedFactor = (double)m_calibData.ticks_EntryC_to_HeightB_Slow /
                        (double)m_calibData.ticks_EntryC_to_HeightB_Normal;
    }
        m_wpManager->setSpeedFactor(m_speedFactor);
    m_calibData.ticks_HeightC_to_EjectorB_Slow = m_calibData.ticks_HeightC_to_EjectorB_Normal * m_speedFactor;
    m_calibData.ticks_EjectorC_to_EndB_Slow    = m_calibData.ticks_EjectorC_to_EndB_Normal * m_speedFactor;

    std::cout << "[Logik] Live-Grenzwerte neu berechnet. Speed-Faktor: " << m_speedFactor << std::endl;
}

void Logik::setLiveAdcCalibration(uint16_t emptyBelt, uint16_t flatCenter, uint32_t minSamples) {
    m_calibData.adc_emptyBelt = emptyBelt;
    m_calibData.adc_flatCenter = flatCenter;
    m_calibData.limit_minSamplesForClass = minSamples;

    // Reicht die Werte auch an das Schieberegister weiter, damit die Daten konsistent sind
    m_wpManager->setCalibrationData(m_calibData);
}

void Logik::saveCurrentCalibration() {
    if (m_calibManager == nullptr) { return; }

    // Limits vor dem Speichern aktualisieren
    recalculateSlowLimits();

    m_calibManager->saveCalibrationData(m_calibData);
}

void Logik::checkCalibrationFile() {
    if (m_calibManager == nullptr) { return; }

    if (m_calibManager->fileExists()) {
        std::cout << "[Logik] Kalibrierungsdatei wurde gefunden! Feuere Event: DATA_EXIST." << std::endl;
        this->processEvent(logik::Event::DATA_EXIST);
        loadCalibrationData();
    }
    else {
        std::cout << "[Logik] Keine Kalibrierungsdatei vorhanden! Feuere Event: DATA_NOT_EXIST." << std::endl;
        this->processEvent(logik::Event::DATA_NOT_EXIST);
    }
}

} // namespace logik
