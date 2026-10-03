#pragma once

#include "LabPlugin.h"
#include "LabMath.h"
#include <string>
#include <vector>

namespace Lab {

    struct MapBrush;
    using LabBrush = MapBrush;

    /// @brief 1 kod 1 plugin: Standalone Procedural Level & Dungeon Generator Plugin
    class LAB_CORE_API ProceduralLevelPlugin : public ILabPlugin {
    public:
        ProceduralLevelPlugin();
        virtual ~ProceduralLevelPlugin() override = default;

        virtual const char* getName() const override { return "Procedural Level Generator"; }
        virtual const char* getVersion() const override { return "1.2.0"; }
        virtual bool onLoad() override;
        virtual void onUpdate(float dt) override;
        virtual void onUnload() override;

        /// @brief Generates a full 3D room with floor, walls, doorway cutout and structural pillars
        int generateRoom(std::vector<LabBrush>& outBrushes, const Vec3& center, float width = 16.0f, float height = 8.0f, float depth = 16.0f, int seed = 42);

        int getRoomsGenerated() const { return m_roomsGenerated; }
        int getTotalBrushesCreated() const { return m_brushesCreated; }

    private:
        int m_roomsGenerated{0};
        int m_brushesCreated{0};
    };

} // namespace Lab
