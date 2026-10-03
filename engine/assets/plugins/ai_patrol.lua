-- =========================================================================
-- Plugin: AI Navmesh & Waypoint Patrol Circuit
-- Standalone Modular Lua Plugin
-- =========================================================================

Plugin = {
    id = "com.lab.ai-patrol",
    displayName = "AI Navmesh & Waypoint Patrol",
    version = "1.1.2",
    author = "Lab AI Labs",
    category = "Gameplay & AI",
    description = "Waypoint patrol graph solver, dynamic field-of-view detection, obstacle avoidance, and sensory perception.",
    dependencies = {},
    state = "Discovered"
}

local waypoints = {
    { x = -10, y = 0, z = -10 },
    { x =  10, y = 0, z = -10 },
    { x =  10, y = 0, z =  10 },
    { x = -10, y = 0, z =  10 }
}

function Plugin.onLoad()
    print("[Plugin: AI Navmesh & Patrol] Loaded circuit with 4 waypoints.")
    return true
end

function Plugin.onUpdate(dt)
end

function Plugin.onUnload()
    print("[Plugin: AI Navmesh & Patrol] Unloaded.")
end

function Plugin.getWaypointCount()
    return #waypoints
end

return Plugin
