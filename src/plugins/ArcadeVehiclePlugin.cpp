#include "plugins/ArcadeVehiclePlugin.h"
#include <iostream>
#include <cmath>

namespace Lab {

    ArcadeVehiclePlugin::ArcadeVehiclePlugin() = default;

    bool ArcadeVehiclePlugin::onLoad() {
        LabLog::info("[Plugin: ArcadeVehiclePlugin] Loaded v2.1.0 successfully.");
        return true;
    }

    void ArcadeVehiclePlugin::onUpdate(float dt) {
        if (m_throttle > 0.01f) {
            m_speedKmh += m_throttle * 40.0f * dt;
            if (m_speedKmh > 180.0f) m_speedKmh = 180.0f;
            m_rpm = 1000.0f + (m_speedKmh / 180.0f) * 6000.0f;
        } else {
            m_speedKmh *= std::max(0.0f, 1.0f - dt * 0.5f);
            m_rpm = std::max(900.0f, m_rpm - dt * 2000.0f);
        }
    }

    void ArcadeVehiclePlugin::onUnload() {
        LabLog::info("[Plugin: ArcadeVehiclePlugin] Unloaded.");
    }

} // namespace Lab
