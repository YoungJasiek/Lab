#pragma once

#include "LabPlugin.h"
#include <string>
#include <vector>

namespace Lab {

    /// @brief 1 kod 1 plugin: Standalone Speedrun Splits & Live Telemetry Overlay Plugin
    class LAB_CORE_API SpeedrunTelemetryPlugin : public ILabPlugin {
    public:
        SpeedrunTelemetryPlugin();
        virtual ~SpeedrunTelemetryPlugin() override = default;

        virtual const char* getName() const override { return "Speedrun & Telemetry Splits"; }
        virtual const char* getVersion() const override { return "1.1.0"; }
        virtual bool onLoad() override;
        virtual void onUpdate(float dt) override;
        virtual void onUnload() override;

        void startTimer();
        void pauseTimer();
        void resetTimer();
        void recordSplit(const std::string& name);

        bool isRunning() const { return m_running; }
        float getElapsedTime() const { return m_elapsedTime; }
        float getBestTime() const { return m_bestTime; }
        float getCurrentSpeed() const { return m_currentSpeed; }
        void setCurrentSpeed(float spd) { m_currentSpeed = spd; }
        const std::vector<std::pair<std::string, float>>& getSplits() const { return m_splits; }

    private:
        bool m_running{false};
        float m_elapsedTime{0.0f};
        float m_bestTime{74.25f};
        float m_currentSpeed{0.0f};
        std::vector<std::pair<std::string, float>> m_splits;
    };

} // namespace Lab
