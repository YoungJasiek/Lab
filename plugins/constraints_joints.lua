-- =========================================================================
-- Plugin: Constraints & Joint Physics
-- Standalone Modular Lua Plugin
-- =========================================================================

Plugin = {
    id = "com.lab.physics-constraints",
    displayName = "Constraints & Joint Physics",
    version = "1.0.0",
    author = "Lab Physics Team",
    category = "Physics & Simulation",
    description = "Distance, BallSocket, Hinge, and Slider constraints solver for ragdolls, doors, chains, and articulated mechanisms.",
    dependencies = {},
    state = "Discovered"
}

local activeJoints = 0

function Plugin.onLoad()
    print("[Plugin: Constraints & Joint Physics] Multi-body constraint solver initialized.")
    return true
end

function Plugin.onUpdate(dt)
end

function Plugin.onUnload()
    print("[Plugin: Constraints & Joint Physics] Unloaded.")
end

function Plugin.getJointCount()
    return activeJoints
end

return Plugin
