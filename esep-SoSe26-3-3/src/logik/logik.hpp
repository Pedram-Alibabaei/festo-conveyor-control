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
        BaseState* m_currentState; // Pointer auf den AKTUELLEN Zustand (GoF State Pattern)
        Dispatcher* m_dispatcher;  // Pointer auf den Dispatcher (Postbote)

    public:
        Logik() = default;
        ~Logik() override = default;

        // ====================================================================
        // INITIALISIERUNG & SETTER
        // ====================================================================
        void init(AnlagenPosition pos) override;
        void processEvent(logik::Event e) override;
        void workpieceTypeFromHS(WpType type) override;
        void workpieceInfo(int id, WpType type) override;
        void updateWpListe(WpType type) override;
        void tackt() override;

        // Erfüllung der Kalibrierungs-Schnittstellen aus dem Interface
        void setTrackedSegmentTicks(uint16_t calibMode, uint16_t tickValue) override;
        void receiveRawCalibrationData(uint16_t calibMode, uint16_t adcValue) override;

        // ====================================================================
        // WERKSTÜCKVERFOLGUNG (FSM-Hilfsfunktionen)
        // ====================================================================
        // Dummy-Methoden, damit der RunningState.cpp kompilieren kann
        void addWorkpiece();
        bool dispatchSensorEventToWorkpiece(logik::Event e);


    };

} // namespace logik

#endif // LOGIK_HPP