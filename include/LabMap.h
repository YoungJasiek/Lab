#pragma once

#include "LabMath.h"
#include "LabSession.h"
#include "LabRenderer.h"
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

        // Custom polygon vertices and indices for CSG clipped brushes (type == "poly")
        std::vector<Vertex> customVertices;
        std::vector<unsigned int> customIndices;
        mutable std::shared_ptr<Mesh> runtimeMesh;
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
        bool isLocked = false;
    };

    struct MapSpawn {
        Vec3 position = { 0.0f, 1.8f, 0.0f };
        float yaw = 0.0f;
    };

    enum class SpawnType {
        FFA = 0,        // Deathmatch / FFA neutral spawn (info_player_deathmatch)
        TeamAlpha = 1,  // TDM Team 1 / Blue / Combine (info_player_team1)
        TeamBeta = 2    // TDM Team 2 / Red / Rebels (info_player_team2)
    };

    struct MapSpawnPoint {
        std::string entityClass = "info_player_deathmatch";
        Vec3 position = { 0.0f, 1.8f, 0.0f };
        float yaw = 0.0f;
        SpawnType type = SpawnType::FFA;

        std::string getDisplayName() const {
            switch (type) {
                case SpawnType::TeamAlpha: return "Spawn [Team Alpha]";
                case SpawnType::TeamBeta:  return "Spawn [Team Beta]";
                case SpawnType::FFA:
                default:                   return "Spawn [FFA / DM]";
            }
        }
    };

    struct MapWeaponSpawner {
        int weaponId = 2; // 0..8 (maps to WeaponID)
        Vec3 position = { 0.0f, 0.0f, 0.0f };
        float yaw = 0.0f;
        float respawnTime = 60.0f; // Default 60s (1 min)

        std::string getWeaponName() const {
            const char* names[9] = { "Pipe", "Pistol", "Shotgun", "M4A4-S", "SG553", "Minigun", "Plasma Gun", "Railgun", "RPG" };
            if (weaponId >= 0 && weaponId < 9) return names[weaponId];
            return "Weapon";
        }
    };

    enum class MapLightType {
        Point = 0,    // Omni lamp / bulb / sconce
        Spot = 1,     // Spotlight cone
        Directional = 2
    };

    struct MapLight {
        std::string name = "light_omni";
        Vec3 position = { 0.0f, 3.0f, 0.0f };
        Vec3 color = { 1.0f, 0.95f, 0.85f }; // Warm incandescent
        float intensity = 2.5f;
        float radius = 14.0f;
        MapLightType type = MapLightType::Point;
        Vec3 direction = { 0.0f, -1.0f, 0.0f };
        float spotAngle = 45.0f;
    };

    class LabMap {
    public:
        MapMetadata metadata;
        MapSpawn spawn; // Legacy fallback single spawn
        std::vector<MapBrush> brushes;
        std::vector<MapProp> props;
        std::vector<MapDoor> doors;
        std::vector<MapSpawnPoint> spawnPoints;
        std::vector<MapWeaponSpawner> weaponSpawners;
        std::vector<MapLight> lights;

        std::vector<MapSpawnPoint> getSpawnsForTeam(GameMode mode, int team) const;
        MapSpawnPoint selectBestSpawn(GameMode mode, int team, const std::vector<Vec3>& enemyPositions = {}) const;

        static std::unique_ptr<LabMap> loadFromFile(const std::string& filePath);
        bool saveToFile(const std::string& filePath) const;
    };

} // namespace Lab
