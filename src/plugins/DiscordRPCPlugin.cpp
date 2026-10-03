#include "plugins/DiscordRPCPlugin.h"
#include <iostream>

namespace Lab {

    DiscordRPCPlugin::DiscordRPCPlugin() {
        m_statusText = "Lab Studio | Level Design";
    }

    bool DiscordRPCPlugin::onLoad() {
        LabLog::info("[Plugin: DiscordRPCPlugin] Loaded v1.0.0 successfully.");
        return true;
    }

    void DiscordRPCPlugin::onUpdate(float dt) {
        m_sessionTime += dt;
    }

    void DiscordRPCPlugin::onUnload() {
        LabLog::info("[Plugin: DiscordRPCPlugin] Unloaded.");
    }

    void DiscordRPCPlugin::updateActivity(const std::string& workspace, const std::string& levelName) {
        m_statusText = "Lab Studio | " + workspace + " [" + levelName + "] (" + std::to_string((int)m_sessionTime / 60) + "m)";
    }

} // namespace Lab
