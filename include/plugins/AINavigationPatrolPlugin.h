#pragma once

#include "LabPlugin.h"
#include "LabMath.h"
#include <vector>

namespace Lab {

    /// @brief 1 kod 1 plugin: Standalone AI Navmesh & Waypoint Patrol Circuit Plugin
    class LAB_CORE_API AINavigationPatrolPlugin : public ILabPlugin {
    public:
        AINavigationPatrolPlugin();
        virtual ~AINavigationPatrolPlugin() override = default;

        virtual const char* getName() const override { return "AI Navmesh & Waypoint Patrol"; }
        virtual const char* getVersion() const override { return "1.1.2"; }
        virtual bool onLoad() override;
        virtual void onUpdate(float dt) override;
        virtual void onUnload() override;

        void addWaypoint(const Vec3& wp);
        size_t getWaypointCount() const { return m_waypoints.size(); }
        Vec3 getCurrentTarget() const;

    private:
        std::vector<Vec3> m_waypoints;
        size_t m_currentWaypointIdx{0};
    };

} // namespace Lab
