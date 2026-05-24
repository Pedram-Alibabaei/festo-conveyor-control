//
// Created by bahoz on 23.05.2026.
//

#ifndef LOGIK_HPP
#define LOGIK_HPP

#include "I_Logik.hpp"
#include "shared/types.hpp"
#include "shared/logikEvent.hpp"

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

        void updateWpListe(WpType type) override; // WpType großgeschrieben wie im Interface

        void tackt() override;
    };


#endif // LOGIK_HPP
