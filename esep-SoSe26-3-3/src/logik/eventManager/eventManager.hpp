/**
 * @file eventManager.hpp
 * @brief Zeitliche Filterung, Entprellung und Vorverarbeitung von Hardware-Signalen.
 *
 * SoSe2026 Team 3.3
 */

#ifndef EVENT_MANAGER_HPP
#define EVENT_MANAGER_HPP

#include "shared/types.hpp"
#include "shared/logikEvent.hpp"
#include <chrono>
#include <memory>

namespace logik {

 class Logik;
 class WorkpieceManager;
 class ErrorHandler;
 class Fsm;
 class BaseState;

 class EventManager {
 private:
  Logik* m_context; ///< Zeiger auf die übergeordnete Schaltzentrale (Nicht-besitzend)

  // --- KORREKTUR: Raw-Pointer statt unique_ptr (Ownership liegt bei der Logik-Klasse!) ---
  WorkpieceManager* m_wpManager; ///< Adresse des Schieberegisters aus dem Logik-Kontext
  ErrorHandler* m_errorHandler; ///< Adresse des ErrorHandlers aus dem Logik-Kontext

  std::unique_ptr<Fsm> m_fsm;

  // --- Motor- und Positions-Flags ---
  bool m_isMotorOn;
  bool m_isMotorSlow;
  AnlagenPosition m_anlagenPosition;

  // E-Stop Zustandssicherung
  bool m_isEStopActive;   ///< true, wenn der Not-Aus gedrueckt ist
  bool m_isErrorActive;   ///< true, wenn ein Anlagenfehler (z.B. Rutsche voll) aktiv ist
  bool m_isEntryFrei;     ///< true, wenn der Bereich zwischen Einlauf und Hoehenmessung frei ist

  bool m_isChuteFull;         ///< true, wenn die eigene Rutsche voll ist (> 1 Sek blockiert)
  bool m_isPartnerChuteFull;  ///< true, wenn die Rutsche der Partner-Station voll ist (vom Netzwerk)

  // --- Taster-Zeitmessung ---
  std::chrono::steady_clock::time_point m_startPressedStartTime;
  const std::chrono::milliseconds START_LONG_THRESHOLD;

  // --- Bandende-Schutz ---
  bool m_isEndBlocked;

  bool m_isServiceMode;

  std::chrono::steady_clock::time_point m_partnerStartPressedTime;

 private:
  void routeToErrorHandler(logik::Event e);
  void routeToWorkpieceManager(logik::Event e);
  void routeToFsm(logik::Event e);

 public:
  /**
   * @brief Konstruktor.
   * @param context Zeiger auf die uebegeordnete Logik.
   */
  explicit EventManager(Logik* context);
  ~EventManager();

  // Verhindert Kopieren
  EventManager(const EventManager&) = delete;
  EventManager& operator=(const EventManager&) = delete;

  /**
   * @brief Initialisiert den EventManager und verknuepft die Geschwisterkomponenten live.
   */
  void init(AnlagenPosition pos);

  void handleEvent(logik::Event e);

  /**
   * @brief Deine exakte, geschützte Takt-Verarbeitung für den digitalen Zwilling.
   */
  void handleTakt();

  void transitionFsmState(std::unique_ptr<BaseState> nextState);

  // --- Getters und Setters ---
  void setMotorOn(bool on) { m_isMotorOn = on; }
  bool isMotorOn() const { return m_isMotorOn; }

  void setMotorSlow(bool slow) { m_isMotorSlow = slow; }
  bool isMotorSlow() const { return m_isMotorSlow; }

  void setEndBlocked(bool blocked) { m_isEndBlocked = blocked; }
  bool isEndBlocked() const { return m_isEndBlocked; }

  void setEStopActive(bool active) { m_isEStopActive = active; }
  bool isEStopActive() const { return m_isEStopActive; }

  void setErrorActive(bool active) { m_isErrorActive = active; }
  bool isErrorActive() const { return m_isErrorActive; }

  void setEntryFrei(bool frei) { m_isEntryFrei = frei; }
  bool isEntryFrei() const { return m_isEntryFrei; }

  void setChuteFull(bool isFull) { m_isChuteFull = isFull; }
  bool isChuteFull() const { return m_isChuteFull; }

  void setPartnerChuteStatus(bool isFull);
  bool isPartnerChuteFull() const { return m_isPartnerChuteFull; }

  void setServiceMode(bool active) { m_isServiceMode = active; }
  bool isServiceMode() const { return m_isServiceMode; }
 };

} // namespace logik

#endif // EVENT_MANAGER_HPP