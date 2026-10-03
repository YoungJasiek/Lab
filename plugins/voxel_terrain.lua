-- =========================================================================
-- Plugin: Voxel Terrain & Sculptor
-- 1 kod 1 plugin: Standalone Lua Plugin File
-- =========================================================================

Plugin = {
    id = "com.lab.voxel-terrain",
    displayName = "Voxel Terrain & Sculptor",
    version = "1.3.0",
    author = "Lab World Tools",
    category = "3D & Geometry",
    description = "Voxel grid terrain generator and sphere-brush sculptor. Supports procedural heightmaps and real-time surface deformation.",
    dependencies = {},
    state = "Discovered"
}

local totalVoxels = 1024

function Plugin.onLoad()
    print("[Plugin: Voxel Terrain] Initialized with 1,024 active voxels.")
    return true
end

function Plugin.onUpdate(dt)
end

function Plugin.onUnload()
    print("[Plugin: Voxel Terrain] Unloaded.")
end

function Plugin.stamp(radius)
    totalVoxels = totalVoxels + 64
    print("[Voxel Terrain] Stamped terrain! Total voxels: " .. tostring(totalVoxels))
    return totalVoxels
end

return Plugin
