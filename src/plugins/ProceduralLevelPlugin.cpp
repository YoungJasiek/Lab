#include "plugins/ProceduralLevelPlugin.h"
#include "LabMap.h"
#include <iostream>

namespace Lab {

    ProceduralLevelPlugin::ProceduralLevelPlugin() = default;

    bool ProceduralLevelPlugin::onLoad() {
        LabLog::info("[Plugin: ProceduralLevelPlugin] Loaded v1.2.0 successfully.");
        return true;
    }

    void ProceduralLevelPlugin::onUpdate(float dt) {
        (void)dt;
    }

    void ProceduralLevelPlugin::onUnload() {
        LabLog::info("[Plugin: ProceduralLevelPlugin] Unloaded.");
    }

    int ProceduralLevelPlugin::generateRoom(std::vector<LabBrush>& outBrushes, const Vec3& center, float width, float height, float depth, int seed) {
        (void)seed;
        int countBefore = (int)outBrushes.size();
        float wallThick = 0.5f;

        // 1. Floor
        LabBrush floorBrush;
        floorBrush.name = "ProcFloor_" + std::to_string(m_roomsGenerated);
        floorBrush.type = "cube";
        floorBrush.position = Vec3(center.x, center.y - wallThick * 0.5f, center.z);
        floorBrush.size = Vec3(width, wallThick, depth);
        floorBrush.texturePath = "assets/textures/concrete.bmp";
        floorBrush.uvScale = Vec2(0.25f, 0.25f);
        outBrushes.push_back(floorBrush);

        // 2. Ceiling
        LabBrush ceilingBrush;
        ceilingBrush.name = "ProcCeiling_" + std::to_string(m_roomsGenerated);
        ceilingBrush.type = "cube";
        ceilingBrush.position = Vec3(center.x, center.y + height + wallThick * 0.5f, center.z);
        ceilingBrush.size = Vec3(width, wallThick, depth);
        ceilingBrush.texturePath = "assets/textures/concrete.bmp";
        ceilingBrush.uvScale = Vec2(0.25f, 0.25f);
        outBrushes.push_back(ceilingBrush);

        // 3. North Wall
        LabBrush northWall;
        northWall.name = "ProcWall_North_" + std::to_string(m_roomsGenerated);
        northWall.type = "cube";
        northWall.position = Vec3(center.x, center.y + height * 0.5f, center.z + depth * 0.5f);
        northWall.size = Vec3(width, height, wallThick);
        northWall.texturePath = "assets/textures/wall_brick.bmp";
        northWall.uvScale = Vec2(0.25f, 0.25f);
        outBrushes.push_back(northWall);

        // 4. South Wall (with doorway)
        float doorWidth = 3.0f;
        float segWidth = (width - doorWidth) * 0.5f;

        LabBrush southWallL;
        southWallL.name = "ProcWall_South_L_" + std::to_string(m_roomsGenerated);
        southWallL.type = "cube";
        southWallL.position = Vec3(center.x - doorWidth * 0.5f - segWidth * 0.5f, center.y + height * 0.5f, center.z - depth * 0.5f);
        southWallL.size = Vec3(segWidth, height, wallThick);
        southWallL.texturePath = "assets/textures/wall_brick.bmp";
        southWallL.uvScale = Vec2(0.25f, 0.25f);
        outBrushes.push_back(southWallL);

        LabBrush southWallR;
        southWallR.name = "ProcWall_South_R_" + std::to_string(m_roomsGenerated);
        southWallR.type = "cube";
        southWallR.position = Vec3(center.x + doorWidth * 0.5f + segWidth * 0.5f, center.y + height * 0.5f, center.z - depth * 0.5f);
        southWallR.size = Vec3(segWidth, height, wallThick);
        southWallR.texturePath = "assets/textures/wall_brick.bmp";
        southWallR.uvScale = Vec2(0.25f, 0.25f);
        outBrushes.push_back(southWallR);

        // 5. West Wall
        LabBrush westWall;
        westWall.name = "ProcWall_West_" + std::to_string(m_roomsGenerated);
        westWall.type = "cube";
        westWall.position = Vec3(center.x - width * 0.5f, center.y + height * 0.5f, center.z);
        westWall.size = Vec3(wallThick, height, depth);
        westWall.texturePath = "assets/textures/wall_brick.bmp";
        westWall.uvScale = Vec2(0.25f, 0.25f);
        outBrushes.push_back(westWall);

        // 6. East Wall
        LabBrush eastWall;
        eastWall.name = "ProcWall_East_" + std::to_string(m_roomsGenerated);
        eastWall.type = "cube";
        eastWall.position = Vec3(center.x + width * 0.5f, center.y + height * 0.5f, center.z);
        eastWall.size = Vec3(wallThick, height, depth);
        eastWall.texturePath = "assets/textures/wall_brick.bmp";
        eastWall.uvScale = Vec2(0.25f, 0.25f);
        outBrushes.push_back(eastWall);

        // 7. Structural Corner Pillars
        float px = width * 0.35f;
        float pz = depth * 0.35f;
        for (int i = 0; i < 2; ++i) {
            float sgnX = (i == 0) ? -1.0f : 1.0f;
            LabBrush pillar;
            pillar.name = "ProcPillar_" + std::to_string(m_roomsGenerated) + "_" + std::to_string(i + 1);
            pillar.type = "cube";
            pillar.position = Vec3(center.x + sgnX * px, center.y + height * 0.5f, center.z + pz);
            pillar.size = Vec3(1.2f, height, 1.2f);
            pillar.texturePath = "assets/textures/rock.bmp";
            pillar.uvScale = Vec2(0.25f, 0.25f);
            outBrushes.push_back(pillar);
        }

        int added = (int)outBrushes.size() - countBefore;
        m_roomsGenerated++;
        m_brushesCreated += added;
        LabLog::info("ProceduralLevelPlugin: Generated room #" + std::to_string(m_roomsGenerated) + " with " + std::to_string(added) + " CSG brushes.");
        return added;
    }

} // namespace Lab
