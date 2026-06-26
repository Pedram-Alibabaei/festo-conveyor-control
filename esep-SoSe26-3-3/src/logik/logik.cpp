/**
 * @file Logik.cpp
 * @brief Implementierung des zentralen Sch Schicht-1-Controllers (Zustandsmaschine).
 *
 * SoSe2026 Team 3.3
 */

#include "Logik.hpp"
#include "calibrationManager/I_CalibrationManager.hpp"
#include "logik/fsm/common/BaseState.hpp"
#include <memory>
#include "shared/detailLog.hpp"

namespace logik {

    Logik::Logik(I_Dispatcher* dispatcher)
        : m_wpManager(std::make_unique<WorkpieceManager>(this)),
          m_errorHandler(std::make_unique<ErrorHandler>(this)), // 1. ANPASSUNG: ErrorHandler sauber instanziieren
          m_dispatcher(dispatcher),
          m_calibManager(nullptr),
          m_eventManager(nullptr),
          m_anlagenPosition(AnlagenPosition::UNKNOWN_POSITION),
          m_isPartnerChuteFull(false),
          m_isMotorSlow(false),
          m_serviceTickCount(0)
    {
        DETAIL_LOG( "[Logik] System-Instanz inklusive ErrorHandler erfolgreich konstruiert.");
    }

void Logik::init(AnlagenPosition pos) {
    m_anlagenPosition = pos;
    m_wpManager->setAnlagenPosition(pos);
    DETAIL_LOG( "[Logik] Initialisiert auf Position: " << static_cast<int>(pos));
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

bool Logik::isEstopSafetyActive() const {
    return m_eventManager != nullptr && m_eventManager->isEStopActive();
}

    void Logik::workpieceTypeFromHS(WpType type) {
        DETAIL_LOG( "[Logik] HEIGHT_TYPE empfangen: type="
                  << static_cast<int>(type));

        if (m_eventManager != nullptr && m_eventManager->isErrorActive()) {
            DETAIL_LOG( "[Logik] HEIGHT_TYPE ignoriert – Anlage im ErrorState."
                     );
            return;
        }

        if (m_eventManager != nullptr && m_eventManager->isServiceMode()) {
            m_eventManager->handleEvent(logik::Event::HEIGHT_CLEARED);
            return;
        }

        if (m_wpManager != nullptr) {
            m_wpManager->handleSensorSignal(logik::Event::HEIGHT_CLEARED, type);
        }
        if (m_eventManager != nullptr) {
            m_eventManager->setHeightBlocked(false);
            DETAIL_LOG( "[Logik] SEND SET_FAST: trigger=HEIGHT_TYPE/HEIGHT_CLEARED"
                     );
            m_eventManager->handleEvent(logik::Event::SET_FAST);
        }
    }

void Logik::workpieceInfo(int id, WpType type) {
    if (m_anlagenPosition == AnlagenPosition::HINTEN_ANLAGE_2) {
        if (m_wpManager != nullptr) {
            m_wpManager->storePendingNetworkWorkpiece(id, type);
        }
        if (m_eventManager != nullptr
            && !m_eventManager->isServiceMode()
            && !m_eventManager->isErrorActive()
            && !m_eventManager->isEndBlocked()
            && m_wpManager != nullptr
            && m_wpManager->hasPendingNetworkWorkpiece()
            && m_wpManager->isAnlage2Ready()
            && m_wpManager->getFirstWorkpieceAt(WpLocation::AT_END) == nullptr) {
            DETAIL_LOG( "[Logik] Anlage 2: WP-Info von Anlage 1 -> MOTOR_RUN, "
                      << "warte auf ENTRY_BLOCKED.");
            m_eventManager->handleEvent(logik::Event::MOTOR_RUN);
        } else if (m_eventManager != nullptr && !m_eventManager->isServiceMode()) {
            DETAIL_LOG( "[Logik] Anlage 2: WP-Info gespeichert – Motor bleibt aus "
                      << "(Bandende belegt oder nicht bereit).");
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
    if (m_eventManager == nullptr) {
        return;
    }

    m_eventManager->setPartnerChuteStatus(isFull);

    if (m_eventManager->isServiceMode() || m_eventManager->isErrorActive()) {
        return;
    }

    if (isFull) {
        DETAIL_LOG( "[Logik] Partner-Rutsche voll -> WARNING.");
        m_eventManager->handleEvent(logik::Event::WARNING);
    } else if (!m_eventManager->isChuteFull()) {
        DETAIL_LOG( "[Logik] Partner-Rutsche frei (lokal frei) -> WARNING_CLEARED.");
        m_eventManager->handleEvent(logik::Event::WARNING_CLEARED);
    }
}

void Logik::loadCalibrationData() {
    if (m_calibManager == nullptr) {
        std::cerr << "[Logik] CRITICAL: CalibManager-Zeiger ist nullptr bei loadCalibrationData!" << std::endl;
        return;
    }

    if (m_eventManager != nullptr && m_eventManager->isServiceMode()) {
        DETAIL_LOG( "[Logik] loadCalibrationData waehrend Service-Kalibrierung ignoriert."
                 );
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

    notifyPartnerCalibStatus();
}

void Logik::recalculateSlowLimits() {
    double speedFactor = 1.0;
    if (m_calibData.ticks_EntryC_to_HeightB_Normal > 0
        && m_calibData.ticks_EntryC_to_HeightB_Slow > 0) {
        speedFactor = static_cast<double>(m_calibData.ticks_EntryC_to_HeightB_Slow)
                    / static_cast<double>(m_calibData.ticks_EntryC_to_HeightB_Normal);
    }

    m_wpManager->setSpeedFactor(speedFactor);

    DETAIL_LOG( "[Logik] Speed-Faktor aktualisiert: " << speedFactor
              << " (Slow-Ticks ab HeightC werden gemessen).");
}

void Logik::resetServiceTickCounter() {
    m_serviceTickCount = 0;
}

void Logik::incrementServiceTickCounter() {
    ++m_serviceTickCount;
}

std::uint32_t Logik::readAndResetServiceTickCounter() {
    const std::uint32_t value = m_serviceTickCount;
    m_serviceTickCount = 0;
    return value;
}

void Logik::syncCalibrationToWorkpieceManager() {
    m_wpManager->setCalibrationData(m_calibData);
}

void Logik::setLiveAdcCalibration(uint16_t emptyBelt, uint16_t flatCenter, uint32_t minSamples) {
    m_calibData.adc_emptyBelt = emptyBelt;
    m_calibData.adc_flatCenter = flatCenter;
    m_calibData.limit_minSamplesForClass = minSamples;

    // Reicht die Werte auch an das Schieberegister weiter, damit die Daten konsistent sind
    m_wpManager->setCalibrationData(m_calibData);
}

bool Logik::saveCurrentCalibration() {
    if (m_calibManager == nullptr) {
        return false;
    }

    recalculateSlowLimits();

    const bool saved = m_calibManager->saveCalibrationData(m_calibData);
    if (saved) {
        notifyPartnerCalibStatus();
    } else {
        std::cerr << "[Logik] Kalibrierung konnte nicht gespeichert werden." << std::endl;
    }
    return saved;
}

void Logik::notifyPartnerCalibStatus() {
    if (m_dispatcher != nullptr) {
        m_dispatcher->sendLocalCalibStatusToPartner();
    }
}

void Logik::checkCalibrationFile() {
    if (m_calibManager == nullptr) { return; }

    if (m_calibManager->fileExists()) {
        DETAIL_LOG( "[Logik] Gueltige Kalibrierungsdatei gefunden! Feuere Event: DATA_EXIST.");
        this->processEvent(logik::Event::DATA_EXIST);
    }
    else {
        DETAIL_LOG( "[Logik] Keine gueltige Kalibrierungsdatei! Feuere Event: DATA_NOT_EXIST.");
        this->processEvent(logik::Event::DATA_NOT_EXIST);
        notifyPartnerCalibStatus();
    }
}

} // namespace logik
