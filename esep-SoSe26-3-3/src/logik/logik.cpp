#include "logik.hpp"

namespace logik {

    Logik::Logik() : m_position(ipc::VORN_ANLAGE_1), m_dispatcher(nullptr) {}

    Logik::~Logik() {}

    void Logik::setDispatcher(I_Dispatcher* dispatcher) {
        m_dispatcher = dispatcher;
    }

    void Logik::init(ipc::AnlagenPosition pos) {
        m_position = pos;
        std::cout << "[Logik] Initialisiert für Position " << pos << std::endl;
    }

    void Logik::processEvent(Event e) {
        if (!m_dispatcher) return;

        switch(e) {
            case Event::START_PRESSED:
                std::cout << "[Logik] START_PRESSED -> Gruen blinkt!" << std::endl;
                m_dispatcher->lightGreenBlinkFast();
                break;
            case Event::START_RELEASED:
                std::cout << "[Logik] START_RELEASED -> Gruen aus!" << std::endl;
                m_dispatcher->lightGreenOff();
                break;
            case Event::RESET_PRESSED:
                std::cout << "[Logik] RESET_PRESSED -> Gelb blinkt!" << std::endl;
                m_dispatcher->lightYellowBlinkFast();
                break;
            case Event::RESET_RELEASED:
                std::cout << "[Logik] RESET_RELEASED -> Gelb aus!" << std::endl;
                m_dispatcher->lightYellowOff();
                break;
            case Event::STOP_PRESSED:
                std::cout << "[Logik] STOP_PRESSED -> Rot blinkt!" << std::endl;
                m_dispatcher->lightRedBlinkSlow();
                break;
            case Event::STOP_RELEASED:
                std::cout << "[Logik] STOP_RELEASED -> Rot aus!" << std::endl;
                m_dispatcher->lightRedOff();
                break;
            case Event::ENTRY_BLOCKED:
                std::cout << "[Logik] ENTRY_BLOCKED -> Motor faehrt RECHTS!" << std::endl;
                m_dispatcher->driveRight();
                m_dispatcher->lightGreenBlinkSlow();
                break;
            case Event::END_BLOCKED:
                std::cout << "[Logik] END_BLOCKED -> Motor STOPP!" << std::endl;
                m_dispatcher->driveStop();
                m_dispatcher->lightGreenOff();
                break;
            case Event::CHUTE_BLOCKED:
                std::cout << "[Logik] CHUTE_BLOCKED -> Motor STOPP!" << std::endl;
                m_dispatcher->driveStop();
                m_dispatcher->lightGreenOff();
                break;
            default:
                break;
        }
    }

    void Logik::workpieceInfo(int id, WpType type) {}
    void Logik::RequestToGiveWp() {}
    void Logik::readyToReceiveWp() {}
    void Logik::updateWpListe(WpType Type) {}
    void Logik::tackt() {}

} // namespace logik
