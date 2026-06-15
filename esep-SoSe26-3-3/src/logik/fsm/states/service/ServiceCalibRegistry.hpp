#ifndef SERVICE_CALIB_REGISTRY_HPP
#define SERVICE_CALIB_REGISTRY_HPP

#include <memory>

namespace logik {

class ServiceCalibSubStateBase;

/** Gespeicherter Service-Kalibrierungs-Unterzustand fuer Wiederherstellung nach ConnectionLost. */
enum class ServiceCalibPhase {
    None,
    InitEmpty,
    MotorRightSlow,
    SlowResetTick,
    WaitHeightCleared,
    WaitEjectorBlocked,
    WaitEjectorCleared,
    WaitChuteBlocked,
    SlowCalibDone,
    NormalStart,
    NormalWaitHeightCleared,
    NormalWaitEjectorBlocked,
    NormalWaitEjectorCleared,
    NormalWaitEndBlocked,
    WaitWpRemoved,
    CalibFinished
};

class ServiceCalibRegistry {
public:
    static bool hasSavedPhase();
    static ServiceCalibPhase savedPhase();

    static void savePhase(const ServiceCalibSubStateBase* sub);
    static void clear();

    static std::unique_ptr<ServiceCalibSubStateBase> createSavedSubState();

    /** True waehrend entry() nach ConnectionLost-Restore – keine Neustarts. */
    static bool isResuming();
    static void setResuming(bool value);

    /** ADC fuer Normal-Pfad nach Restore wieder scharf schalten. */
    static bool needsServiceTimingAdc(ServiceCalibPhase phase);
};

} // namespace logik

#endif // SERVICE_CALIB_REGISTRY_HPP
