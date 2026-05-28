//
// Created by bahoz on 23.05.2026.
//

#ifndef LOGIK_HPP
#define LOGIK_HPP

#include "i_Logik.hpp"
#include "shared/types.hpp"
#include "shared/logikEvent.hpp"

// Vorwärtsdeklaration statt schwerem #include,
// da m_dispatcher nur ein Zeiger (Pointer) ist.
class I_Dispatcher;

namespace logik {

    class Logik : public i_Logik {
    private:
        AnlagenPosition m_position;
        bool m_isCalibrated;
        // Variablen für den 200-Takt-Zähler beim Testen
        bool m_countTicks;
        uint32_t m_tickCounter;

        // Zeiger auf das Dispatcher-Interface
        I_Dispatcher* m_dispatcher;

    public:
        Logik();
        ~Logik() override; // Fehlerhaftes Leerzeichen/Semikolon entfernt

        // Setter für die Dependency Injection in der main.cpp
        void setDispatcher(I_Dispatcher* disp) { m_dispatcher = disp; }

        // Implementierung der Methoden aus dem Interface:
        void init(AnlagenPosition pos) override;
        void processEvent(logik::Event e) override;
        void workpieceTypeFromHS(WpType type) override;
        void workpieceInfo(int id, WpType type) override;
        void updateWpListe(WpType type) override;
        void tackt() override;

        // Erfüllung der Kalibrierungs-Schnittstellen aus dem Interface
        void setTrackedSegmentTicks(uint16_t calibMode, uint16_t tickValue) override;
        void receiveRawCalibrationData(uint16_t calibMode, uint16_t adcValue) override;
    };

} // namespace logik

#endif // LOGIK_HPP