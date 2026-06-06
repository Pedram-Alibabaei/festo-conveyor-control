#ifndef ESTOP_STATES_HPP
#define ESTOP_STATES_HPP

#include "BaseState.hpp"
#include "logik/logik.hpp"

namespace logik {

    // Zustand 1: Nur der eigene (lokale) Pilztaster ist gedrueckt
    class EStopLocalActiveState : public BaseState {
    public:
        void onEnter() override;
        void handle(Event e) override;
    };

    // Zustand 2: Nur der Pilztaster der anderen Anlage ist gedrueckt
    class EStopPartnerActiveState : public BaseState {
    public:
        void onEnter() override;
        void handle(Event e) override;
    };

    // Zustand 3: Beide Pilztaster (Lokal & Partner) sind gedrueckt
    class EStopBothActiveState : public BaseState {
    public:
        void onEnter() override;
        void handle(Event e) override;
    };

    // Zustand 4: Beide Taster sind entriegelt (gezogen), Anlage wartet auf Quittierung (Reset)
    class EStopUnlockedState : public BaseState {
    public:
        void onEnter() override;
        void handle(Event e) override;
        void executeReset(); // <--- GENAU HIER HINZUFÜGEN!
    };
    // Zustand 5: Virtueller Not-Halt durch Netzwerkabbruch
    class ConnectionLostState : public BaseState {
    public:
        void onEnter() override;
        void handle(Event e) override;
    };
} // namespace logik

#endif // ESTOP_STATES_HPP