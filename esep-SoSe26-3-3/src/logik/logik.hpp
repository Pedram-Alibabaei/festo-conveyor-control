#ifndef LOGIK_HPP
#define LOGIK_HPP

#include "i_logik.hpp" // KORREKT: In QNX immer strikt auf Gross-/Kleinschreibung achten!
#include "shared/types.hpp"
#include "shared/logikEvent.hpp"

// Vorwärtsdeklarationen, um Include-Kreisläufe zu verhindern
class Dispatcher;

namespace logik {
    class BaseState; // Vorwärtsdeklaration für das State Pattern

    class Logik : public i_Logik {
    private:
        AnlagenPosition m_position;
        BaseState* m_currentState; // Pointer auf den AKTUELLEN Zustand (GoF State Pattern)
        Dispatcher* m_dispatcher;  // Pointer auf den Dispatcher (Postbote)

    public:
        Logik();
        ~Logik() override;

        // ====================================================================
        // INITIALISIERUNG & SETTER
        // ====================================================================
        void init(AnlagenPosition pos) override;

        // NEU: Setter für den Dispatcher (wird in der main.cpp aufgerufen)
        void setDispatcher(Dispatcher* dispatcher) {
            m_dispatcher = dispatcher;
        }

        // NEU: Getter für die States, damit diese Aktoren schalten können
        Dispatcher* getDispatcher() {
            return m_dispatcher;
        }

        AnlagenPosition getPosition() const {
                    return m_position;
                }

        // ====================================================================
        // STATE PATTERN (GoF) KERNFUNKTIONEN
        // ====================================================================

        // Wechselt den Zustand. Löscht den alten und startet den neuen.
        void transitionTo(BaseState* newState);

        // Die wichtigste Methode: Überschreibt das alte switch-case!
        void processEvent(logik::Event e) override;

        // ====================================================================
        // LOGIK-INTERFACE IMPLEMENTIERUNG
        // ====================================================================
        void workpieceTypeFromHS(WpType type) override;
        void workpieceInfo(int id, WpType type) override;
        void updateWpListe(WpType type) override;
        void tackt() override;
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
