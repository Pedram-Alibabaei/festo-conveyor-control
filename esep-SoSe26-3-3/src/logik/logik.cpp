#include "logik.hpp"
#include "states/BaseState.hpp"
#include "states/IdleState.hpp"

namespace logik {

    Logik::Logik() : m_position(ipc::VORN_ANLAGE_1), m_dispatcher(nullptr), m_currentState(nullptr), m_nextWpId(0) {
        transitionTo(new IdleState());
    }


    Logik::~Logik() {
        if (m_currentState) delete m_currentState;
    }

    void Logik::setDispatcher(I_Dispatcher* dispatcher) {
        m_dispatcher = dispatcher;
    }

    void Logik::transitionTo(BaseState* state) {
        if (m_currentState != nullptr) {
            m_currentState->onExit();
            delete m_currentState;
        }
        m_currentState = state;
        if (m_currentState != nullptr) {
            m_currentState->setContext(this);
            m_currentState->onEnter();
        }
    }

    void Logik::addWorkpiece() {
        int id = m_nextWpId++;
        m_workpieces.emplace_back(id);
        std::cout << "[Logik] Werkstueck registriert. ID zugewiesen: " << id << std::endl;
        
        if (m_dispatcher) {
            m_dispatcher->workpieceInfo(id, WpType::UNKNOWN); // Inform dispatcher/UI about the new WP
        }
    }

    bool Logik::dispatchSensorEventToWorkpiece(Event e) {
        for (auto& wp : m_workpieces) {
            if (wp.expectsEvent(e)) {
                wp.consumeEvent(e);
                std::cout << "[Logik] Event konsumiert von WP ID: " << wp.getId() << std::endl;
                return true;
            }
        }
        return false;
    }


    void Logik::init(ipc::AnlagenPosition pos) {
        m_position = pos;
        std::cout << "[Logik] Initialisiert für Position " << pos << std::endl;
    }

    void Logik::processEvent(Event e) {
        if (!m_dispatcher || !m_currentState) return;
        m_currentState->handle(e);
    }

    void Logik::workpieceInfo(int id, WpType type) {}
    void Logik::RequestToGiveWp() {}
    void Logik::readyToReceiveWp() {}
    void Logik::updateWpListe(WpType Type) {}
    
    void Logik::tackt() {
        for (auto& wp : m_workpieces) {
            if (wp.updateTackt()) {
                std::cout << "[Logik] ERROR: Timeout bei WP ID: " << wp.getId() << std::endl;
                // TODO: Event in die Queue pushen um den ErrorState aufzurufen
            }
        }
    }

} // namespace logik
