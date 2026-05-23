#ifndef WORKPIECE_HPP
#define WORKPIECE_HPP

#include "i_logik.h"
#include "shared/logikEvent.hpp"

namespace logik {

enum class WpLocation {
    AT_ENTRY,             // Gerade am Einlauf (Lichtschranke blockiert)
    TRANSIT_TO_HEIGHT,    // Auf dem Weg zum Höhensensor
    AT_HEIGHT_SENSOR,     // Genau unter dem Höhensensor (wird gerade vermessen)
    TRANSIT_TO_EJECTOR,   // Auf dem Weg zur Weiche
    AT_EJECTOR,           // An der Weiche (Entscheidungspunkt)
    TRANSIT_TO_END,       // Auf dem Weg zum Ende (darf passieren)
    AT_END,               // Am Bandende (Übergabe-bereit an Anlage 2)
    IN_CHUTE              // In der Rutsche
};

class Workpiece {
private:
    int m_id;
    WpType m_type;
    WpLocation m_location;
    int m_tacktCounter;

public:
    Workpiece(int id) : m_id(id), m_type(WpType::UNKNOWN), m_location(WpLocation::AT_ENTRY), m_tacktCounter(0) {}

    int getId() const { return m_id; }
    void setType(WpType type) { m_type = type; }
    WpType getType() const { return m_type; }
    WpLocation getLocation() const { return m_location; }

    bool expectsEvent(Event e) const {
        switch (m_location) {
            case WpLocation::AT_ENTRY:
                return e == Event::ENTRY_CLEARED;
            case WpLocation::TRANSIT_TO_HEIGHT:
                return e == Event::HEIGHT_BLOCKED;
            case WpLocation::AT_HEIGHT_SENSOR:
                return e == Event::HEIGHT_CLEARED;
            case WpLocation::TRANSIT_TO_EJECTOR:
                return e == Event::METAL_DETECTED || e == Event::EJECTOR_BLOCKED || e == Event::CHUTE_BLOCKED;
            case WpLocation::AT_EJECTOR:
                return e == Event::EJECTOR_CLEARED || e == Event::CHUTE_BLOCKED;
            case WpLocation::TRANSIT_TO_END:
                return e == Event::END_BLOCKED;
            case WpLocation::AT_END:
                return e == Event::END_CLEARED;
            default:
                return false;
        }
    }

    void consumeEvent(Event e) {
        m_tacktCounter = 0; // Reset timer for next segment
        switch (e) {
            case Event::ENTRY_CLEARED:
                m_location = WpLocation::TRANSIT_TO_HEIGHT;
                break;
            case Event::HEIGHT_BLOCKED:
                m_location = WpLocation::AT_HEIGHT_SENSOR;
                break;
            case Event::HEIGHT_CLEARED:
                m_location = WpLocation::TRANSIT_TO_EJECTOR;
                break;
            case Event::EJECTOR_BLOCKED:
                m_location = WpLocation::AT_EJECTOR;
                break;
            case Event::EJECTOR_CLEARED:
                m_location = WpLocation::TRANSIT_TO_END;
                break;
            case Event::END_BLOCKED:
                m_location = WpLocation::AT_END;
                break;
            case Event::CHUTE_BLOCKED:
                m_location = WpLocation::IN_CHUTE;
                break;
            default:
                break;
        }
    }

    // Gibt true zurück, wenn das Werkstück in dieser Zone ein Timeout hatte (Fehlerfall)
    bool updateTackt() {
        m_tacktCounter++;
        // TODO: Genaue Timeout-Grenzwerte in Ticks definieren. 
        // Fürs Erste geben wir false zurück (kein Timeout), bis wir die genauen Bandgeschwindigkeiten kennen.
        // if (m_location == WpLocation::TRANSIT_TO_HEIGHT && m_tacktCounter > MAX_TICK_HEIGHT) return true;
        return false; 
    }
};

} // namespace logik

#endif // WORKPIECE_HPP
