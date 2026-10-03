#include "plugins/AINavigationPatrolPlugin.h"
#include <iostream>

namespace Lab {

    AINavigationPatrolPlugin::AINavigationPatrolPlugin() {
        m_waypoints = {
            Vec3(-10.0f, 0.0f, -10.0f),
            Vec3(10.0f, 0.0f, -10.0f),
            Vec3(10.0f, 0.0f, 10.0f),
            Vec3(-10.0f, 0.0f, 10.0f)
        };
    }

    bool AINavigationPatrolPlugin::onLoad() {
        LabLog::info("[Plugin: AINavigationPatrolPlugin] Loaded v1.1.2 successfully.");
        return true;
    }

    void AINavigationPatrolPlugin::onUpdate(float dt) {
        (void)dt;
    }

    void AINavigationPatrolPlugin::onUnload() {
        LabLog::info("[Plugin: AINavigationPatrolPlugin] Unloaded.");
    }

    void AINavigationPatrolPlugin::addWaypoint(const Vec3& wp) {
        m_waypoints.push_back(wp);
    }

    Vec3 AINavigationPatrolPlugin::getCurrentTarget() const {
        if (m_waypoints.empty()) return Vec3(0, 0, 0);
        return m_waypoints[m_currentWaypointIdx % m_waypoints.size()];
    }

} // namespace Lab
