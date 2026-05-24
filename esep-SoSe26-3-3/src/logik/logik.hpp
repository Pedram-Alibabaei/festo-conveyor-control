//
// Created by bahoz on 23.05.2026.
//

#ifndef LOGIK_HPP
#define LOGIK_HPP

#include "i_logik.hpp"
#include "shared/logikEvent.hpp"
#include "shared/types.hpp"


class I_Dispatcher;

namespace logik {

class BaseState;

class Logik : public i_Logik {
private:
  AnlagenPosition m_position;
  BaseState *m_currentState;
  I_Dispatcher *m_dispatcher;
  int m_wpCount;
  int m_nextWpId;

public:
  Logik();
  ~Logik() override;

  // Implementierung der Methoden aus dem Interface:
  void init(AnlagenPosition pos) override;

  void processEvent(logik::Event e) override;

  void workpieceTypeFromHS(WpType type) override;

  void workpieceInfo(int id, WpType type) override;

  void RequestToGiveWp() override;

  void readyToReceiveWp() override;

  void updateWpListe(WpType type) override;

  void tackt() override;

  // FSM und Testing Methoden:
  void setDispatcher(I_Dispatcher *dispatcher);
  I_Dispatcher *getDispatcher() const;
  void changeState(BaseState *newState);

  int getWorkpieceCount() const;
  int getNextWpId() const;

  void addWorkpiece();    // Optional, falls vom Zustand gebraucht
  void removeWorkpiece(); // Optional
};

} // namespace logik

#endif // LOGIK_HPP
