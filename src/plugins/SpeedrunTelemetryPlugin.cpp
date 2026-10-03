#include "plugins/SpeedrunTelemetryPlugin.h"
#include <iostream>

namespace Lab {

    SpeedrunTelemetryPlugin::SpeedrunTelemetryPlugin() = default;

    bool SpeedrunTelemetryPlugin::onLoad() {
        LabLog::info("[Plugin: SpeedrunTelemetryPlugin] Loaded v1.1.0 successfully.");
        return true;
    }

    void SpeedrunTelemetryPlugin::onUpdate(float dt) {
        if (m_running) {
            m_elapsedTime += dt;
        }
    }

    void SpeedrunTelemetryPlugin::onUnload() {
        LabLog::info("[Plugin: SpeedrunTelemetryPlugin] Unloaded.");
    }

    void SpeedrunTelemetryPlugin::startTimer() {
        m_running = true;
    }

    void SpeedrunTelemetryPlugin::pauseTimer() {
        m_running = false;
    }

    void SpeedrunTelemetryPlugin::resetTimer() {
        m_running = false;
        m_elapsedTime = 0.0f;
        m_splits.clear();
    }

    void SpeedrunTelemetryPlugin::recordSplit(const std::string& name) {
        m_splits.push_back({ name, m_elapsedTime });
        if (m_elapsedTime < m_bestTime) {
            m_bestTime = m_elapsedTime;
        }
    }

} // namespace Lab
