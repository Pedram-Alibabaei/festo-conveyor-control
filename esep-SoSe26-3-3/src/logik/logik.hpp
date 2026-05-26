//
// Created by bahoz on 23.05.2026.
//

#ifndef LOGIK_HPP
#define LOGIK_HPP

#include "i_Logik.hpp" // Header-Namen angepasst (kleines i)
#include "shared/types.hpp"
#include "shared/logikEvent.hpp"

namespace logik { // In den Namespace eingebettet für main.cpp Compatibility

    class Logik : public i_Logik {
    private:
        AnlagenPosition m_position;
        // Hier kommen später deine Zustandsmaschinen (FSMs) und der WorkpieceManager rein

    public:
        Logik() = default;
        ~Logik() override = default;

        // Implementierung der Methoden aus dem Interface:
        void init(AnlagenPosition pos) override;
        void processEvent(logik::Event e) override;
        void workpieceTypeFromHS(WpType type) override;
        void workpieceInfo(int id, WpType type) override;
        void updateWpListe(WpType type) override;
        void tackt() override;

        // NEU: Diese beiden fehlten noch, um das Interface komplett zu erfüllen
        void setTrackedSegmentTicks(uint16_t calibMode, uint16_t tickValue) override;
        void receiveRawCalibrationData(uint16_t calibMode, uint16_t adcValue) override;
    };

} // namespace logik

#endif // LOGIK_HPP