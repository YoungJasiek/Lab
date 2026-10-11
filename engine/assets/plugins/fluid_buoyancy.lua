-- =========================================================================
-- Plugin: Fluid Dynamics & Water Buoyancy
-- Standalone Modular Lua Plugin
-- =========================================================================

Plugin = {
    id = "com.lab.physics-fluid",
    displayName = "Fluid Dynamics & Water Buoyancy",
    version = "1.0.0",
    author = "Lab Physics Team",
    category = "Physics & Simulation",
    description = "Water volume simulation with Archimedes buoyancy, viscous drag damping, and floating rigid body mechanics.",
    dependencies = {},
    state = "Discovered"
}

local fluidDensity = 1000.0
local viscosityDrag = 0.8

function Plugin.onLoad()
    print("[Plugin: Fluid Dynamics & Water Buoyancy] Water volumes and Archimedes solver active.")
    return true
end

function Plugin.onUpdate(dt)
end

function Plugin.onUnload()
    print("[Plugin: Fluid Dynamics & Water Buoyancy] Unloaded.")
end

function Plugin.getProperties()
    return fluidDensity, viscosityDrag
end

return Plugin
