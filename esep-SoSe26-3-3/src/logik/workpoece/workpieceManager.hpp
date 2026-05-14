/**
* @file WorkpieceManager.hpp
 *
 * @brief Klassenbeschreibung: WorkpieceManager
 *
 * Die Klasse WorkpieceManager ist für das Material-Tracking (Materialverfolgung)
 * innerhalb der Sortieranlage zuständig. Sie verwaltet alle Werkstücke, die sich
 * aktuell physisch auf dem Förderband befinden, nach dem FIFO-Prinzip (First In, First Out).
 *
 * Hauptaufgaben (Zuständigkeiten):
 * - Ablaufsteuerung (Tracking): Registrierung neuer Werkstücke beim Einlauf und
 *   Löschung beim Verlassen des Bandes (Auswurf oder erfolgreiche Übergabe).
 * - Datenzuordnung: Sicherstellung, dass asynchrone Sensordaten (wie die spätere
 *   Höhenmessung) exakt dem richtigen, neu eingelaufenen Werkstück zugeordnet werden.
 * - Entscheidungsbereitstellung: Bereitstellung des jeweils vordersten (ältesten)
 *   Werkstücks für die Weiche, damit die Logik rechtzeitig die canPass-Entscheidung abrufen kann.
 */
#ifndef WORKPIECE_MANAGER_HPP
#define WORKPIECE_MANAGER_HPP

#include <deque>
#include "Workpiece.hpp"

class workpieceManager {
private:
    // Die Warteschlange, die alle aktuell auf dem Band befindlichen Werkstücke hält
    std::deque<workpiece> m_queue;

public:
    // 1. Einlauf: Neues Werkstück betritt das Band (Event: LB_ENTRY_BLOCKED)
    void addWorkpiece() {
        // Erstellt ein neues Werkstück (ID wird automatisch vergeben) und packt es ans Ende
        m_queue.push_back(workpiece());
    }

    // 2. Höhensensor: Trägt die gemessenen Daten in das NEUESTE Werkstück ein
    void updateLatestWorkpiece(WPType type, bool canPass) {
        if (!m_queue.empty()) {
            m_queue.back().setType(type);
            m_queue.back().setCanPass(canPass);
        }
    }

    // 3. Weiche / Auslauf: Holt das ÄLTESTE Werkstück (das gerade vorne ankommt)
    // Wichtig: Rückgabe als Referenz (&), damit wir live Daten lesen können
    Workpiece& getFrontWorkpiece() {
        return m_queue.front();
    }

    // 4. Bandende / Auswurf: Entfernt das vorderste Werkstück endgültig aus dem System
    void removeFrontWorkpiece() {
        if (!m_queue.empty()) {
            m_queue.pop_front();
        }
    }

    // Hilfsfunktion für die Logik
    bool isEmpty() const {
        return m_queue.empty();
    }
    
    // Komplett-Reset (z.B. wenn die Reset-Taste gedrückt wird)
    void clearAll() {
        m_queue.clear();
    }
};

#endif // WORKPIECE_MANAGER_HPP