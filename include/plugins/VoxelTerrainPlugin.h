#pragma once

#include "LabPlugin.h"
#include "LabMath.h"

namespace Lab {

    /// @brief 1 kod 1 plugin: Standalone Voxel Grid Terrain & Sculptor Plugin
    class LAB_CORE_API VoxelTerrainPlugin : public ILabPlugin {
    public:
        VoxelTerrainPlugin();
        virtual ~VoxelTerrainPlugin() override = default;

        virtual const char* getName() const override { return "Voxel Terrain & Sculptor"; }
        virtual const char* getVersion() const override { return "1.3.0"; }
        virtual bool onLoad() override;
        virtual void onUpdate(float dt) override;
        virtual void onUnload() override;

        int stampTerrain(const Vec3& center, float radius, float heightDelta);
        int getVoxelCount() const { return m_totalVoxels; }

    private:
        int m_totalVoxels{1024};
    };

} // namespace Lab
