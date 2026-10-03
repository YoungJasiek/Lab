#include "plugins/VoxelTerrainPlugin.h"
#include <iostream>

namespace Lab {

    VoxelTerrainPlugin::VoxelTerrainPlugin() = default;

    bool VoxelTerrainPlugin::onLoad() {
        LabLog::info("[Plugin: VoxelTerrainPlugin] Loaded v1.3.0 successfully.");
        return true;
    }

    void VoxelTerrainPlugin::onUpdate(float dt) {
        (void)dt;
    }

    void VoxelTerrainPlugin::onUnload() {
        LabLog::info("[Plugin: VoxelTerrainPlugin] Unloaded.");
    }

    int VoxelTerrainPlugin::stampTerrain(const Vec3& center, float radius, float heightDelta) {
        (void)center; (void)radius; (void)heightDelta;
        m_totalVoxels += 64;
        return m_totalVoxels;
    }

} // namespace Lab
