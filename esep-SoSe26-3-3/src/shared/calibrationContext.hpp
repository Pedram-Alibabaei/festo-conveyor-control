#ifndef CALIBRATION_CONTEXT_HPP
#define CALIBRATION_CONTEXT_HPP

#include <stdint.h>

struct CalibrationContext {
    // --- Höhen-Grenzwerte (ADC-Werte) ---
    // Beispielwerte, müssen an der Anlage gemessen werden!
    
    // Förderband (Leer-Zustand)
    uint16_t beltHeightMin = 0;
    uint16_t beltHeightMax = 150; 

    // Flaches Werkstück
    uint16_t flatHeightMin = 2000;
    uint16_t flatHeightMax = 2300;

    // Hohes Werkstück
    uint16_t tallHeightMin = 2800;
    uint16_t tallHeightMax = 3100;
    
    // Höhe innerhalb der Bohrung (für TALL_DRILLED)
    // Die Bohrung geht nicht komplett durch, der Sensor misst also eine mittlere Höhe
    uint16_t holeHeightMin = 1000;
    uint16_t holeHeightMax = 1500;
};

#endif // CALIBRATION_CONTEXT_HPP