#ifndef FAKE_LOGIK_HPP
#define FAKE_LOGIK_HPP

#include "logik/i_logik.hpp"

#include <cstdint>
#include <vector>

class FakeLogik : public i_Logik {
public:
    std::vector<logik::Event> events;
    std::vector<WpType> hsTypes;
    std::vector<std::pair<int, WpType>> wpInfos;
    std::vector<WpType> wpListUpdates;
    int taktCount = 0;
    bool partnerChuteFull = false;
    bool partnerChuteSet = false;
    uint16_t liveEmptyBelt = 0;
    uint16_t liveFlatCenter = 0;
    uint32_t liveMinSamples = 0;
    bool liveAdcSet = false;

    void clear() {
        events.clear();
        hsTypes.clear();
        wpInfos.clear();
        wpListUpdates.clear();
        taktCount = 0;
        partnerChuteSet = false;
        liveAdcSet = false;
    }

    void init(AnlagenPosition) override {}

    void setCalibrationManager(I_CalibrationManager*) override {}

    void processEvent(logik::Event e) override {
        events.push_back(e);
    }

    void workpieceTypeFromHS(WpType type) override {
        hsTypes.push_back(type);
    }

    void workpieceInfo(int id, WpType type) override {
        wpInfos.emplace_back(id, type);
    }

    void updateWpListe(WpType type) override {
        wpListUpdates.push_back(type);
    }

    void takt() override {
        ++taktCount;
    }

    void setPartnerChuteStatus(bool isFull) override {
        partnerChuteFull = isFull;
        partnerChuteSet = true;
    }

    void setLiveAdcCalibration(uint16_t emptyBelt, uint16_t flatCenter, uint32_t minSamples) override {
        liveEmptyBelt = emptyBelt;
        liveFlatCenter = flatCenter;
        liveMinSamples = minSamples;
        liveAdcSet = true;
    }

    bool hadEvent(logik::Event e) const {
        for (logik::Event ev : events) {
            if (ev == e) {
                return true;
            }
        }
        return false;
    }
};

#endif // FAKE_LOGIK_HPP
