#pragma once

#include "LabPlugin.h"
#include <string>

namespace Lab {

    /// @brief 1 kod 1 plugin: Standalone Discord Rich Presence & Social Integration Plugin
    class LAB_CORE_API DiscordRPCPlugin : public ILabPlugin {
    public:
        DiscordRPCPlugin();
        virtual ~DiscordRPCPlugin() override = default;

        virtual const char* getName() const override { return "Discord Rich Presence & Social"; }
        virtual const char* getVersion() const override { return "1.0.0"; }
        virtual bool onLoad() override;
        virtual void onUpdate(float dt) override;
        virtual void onUnload() override;

        void updateActivity(const std::string& workspace, const std::string& levelName);
        const std::string& getStatusText() const { return m_statusText; }

    private:
        std::string m_statusText;
        float m_sessionTime{0.0f};
    };

} // namespace Lab
