#pragma once

#include "LabPlugin.h"
#include <algorithm>

namespace Lab {

    /// @brief Standalone Modular Arcade & Raycast Vehicle Suspension Dynamics Plugin
    class LAB_CORE_API ArcadeVehiclePlugin : public ILabPlugin {
    public:
        ArcadeVehiclePlugin();
        virtual ~ArcadeVehiclePlugin() override = default;

        virtual const char* getName() const override { return "Arcade & Raycast Vehicle Dynamics"; }
        virtual const char* getVersion() const override { return "2.1.0"; }
        virtual bool onLoad() override;
        virtual void onUpdate(float dt) override;
        virtual void onUnload() override;

        void setThrottle(float t) { m_throttle = t; }
        void setSteer(float s) { m_steer = s; }
        float getSpeedKmh() const { return m_speedKmh; }
        float getRpm() const { return m_rpm; }

    private:
        float m_throttle{0.0f};
        float m_steer{0.0f};
        float m_speedKmh{0.0f};
        float m_rpm{1000.0f};
    };

} // namespace Lab
