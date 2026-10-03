#include "plugins/RetroPostProcessPlugin.h"
#include <iostream>

namespace Lab {

    RetroPostProcessPlugin::RetroPostProcessPlugin() = default;

    bool RetroPostProcessPlugin::onLoad() {
        LabLog::info("[Plugin: RetroPostProcessPlugin] Loaded v2.0.1 successfully.");
        return true;
    }

    void RetroPostProcessPlugin::onUpdate(float dt) {
        m_phase += dt * 3.14159f;
        if (m_phase > 6.283185f) m_phase -= 6.283185f;
    }

    void RetroPostProcessPlugin::onUnload() {
        LabLog::info("[Plugin: RetroPostProcessPlugin] Unloaded.");
    }

} // namespace Lab
