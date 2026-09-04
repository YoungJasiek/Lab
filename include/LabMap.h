#pragma once

#include "LabMath.h"
#include <string>
#include <vector>
#include <memory>

namespace Lab {

    struct MapMetadata {
        std::string name = "Untitled Sector";
        std::string author = "Unknown";
        Vec3 ambientColor = { 0.2f, 0.25f, 0.35f };
        Vec3 sunDir = { -0.4f, -0.8f, -0.4f };
        Vec3 sunColor = { 0.9f, 0.95f, 1.0f };
    };

    struct MapBrush {
        std::string type = "cube";
        Vec3 position = { 0, 0, 0 };
        Vec3 size = { 1, 1, 1 };
        Vec3 color = { 1, 1, 1 };
        std::string texturePath = "";
        Vec2 uvScale = { 0.25f, 0.25f };
        int uvMode = 1;
    };

    struct MapProp {
        std::string modelPath = "";
        Vec3 position = { 0, 0, 0 };
        Vec3 rotation = { 0, 0, 0 };
        Vec3 scale = { 1, 1, 1 };
        Vec3 color = { 1, 1, 1 };
        std::string texturePath = "";
    };

    struct MapDoor {
        std::string name = "door";
        Vec3 position = { 0, 0, 0 };
        Vec3 size = { 2.0f, 3.5f, 0.3f };
        Vec3 openOffset = { 0.0f, 3.5f, 0.0f };
        Vec3 color = { 0.3f, 0.35f, 0.4f };
        float openSpeed = 3.0f;
        float triggerRadius = 4.0f;
        
        // Runtime animation state
        float currentProgress = 0.0f;
        bool isOpen = false;
    };

    struct MapSpawn {
        Vec3 position = { 0.0f, 1.8f, 0.0f };
        float yaw = 0.0f;
    };

    class LabMap {
    public:
        MapMetadata metadata;
        MapSpawn spawn;
        std::vector<MapBrush> brushes;
        std::vector<MapProp> props;
        std::vector<MapDoor> doors;

        static std::unique_ptr<LabMap> loadFromFile(const std::string& filePath);
        bool saveToFile(const std::string& filePath) const;
    };

} // namespace Lab
