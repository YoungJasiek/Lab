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
        std::string name = "";
        std::string script = "";
        std::string type = "cube";
        Vec3 position = { 0, 0, 0 };
        Vec3 rotation = { 0, 0, 0 };
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
        std::string name = "";
        std::string script = "";
        std::string modelPath = "";
        Vec3 position = { 0, 0, 0 };
        Vec3 rotation = { 0, 0, 0 };
        Vec3 scale = { 1, 1, 1 };
        Vec3 color = { 1, 1, 1 };
        std::string texturePath = "";

        // Component Attachments & Configurations
        bool hasAudio = false;
        std::string audioSound = "assets/audio/terminal_beep.wav";
        float audioVolume = 1.0f;
        float audioPitch = 1.0f;
        bool audioLoop = false;

        bool hasPhysics = false;
        float physicsMass = 10.0f;
        float physicsRestitution = 0.2f;
        float physicsFriction = 0.5f;
        bool physicsDynamic = true;

        bool hasCloth = false;
        float clothStiffness = 0.9f;

        bool hasWaterVolume = false;
        float waterDensity = 1000.0f;

        bool hasForceField = false;
        int forceFieldType = 0; // 0=Radial, 1=Vortex, 2=Wind
        float forceFieldStrength = 50.0f;
        float forceFieldRadius = 8.0f;

        bool hasVehicle = false;
        float vehicleEngineForce = 2500.0f;

        bool hasDecal = false;
        std::string decalTexture = "blood_splatter.png";
        float decalOpacity = 1.0f;

        bool hasPostProcess = false;
        float ppBloom = 0.45f;
        float ppVignette = 0.25f;
        float ppSaturation = 1.0f;
    };

    struct MapDoor {
        std::string name = "door";
        std::string script = "";
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

    struct MapCamera {
        std::string name = "camera_main";
        std::string script = "";
        Vec3 position = { 0.0f, 2.0f, -5.0f };
        Vec3 rotation = { 0.0f, 0.0f, 0.0f }; // pitch, yaw, roll in degrees
        float fov = 75.0f;
        std::string follow_target = ""; // e.g. "player"
        Vec3 tpp_offset = { 0.6f, 1.8f, -3.5f }; // over-the-shoulder offset
        float collision_radius = 0.3f;
        bool active = true;

        bool orthographic = false;
        float orthoSize = 5.0f;
        float nearClip = 0.3f;
        float farClip = 1000.0f;
        int depth = 0;
        std::string clearFlags = "SolidColor";
        Vec4 backgroundColor = { 0.0f, 0.0f, 0.0f, 1.0f };
        Vec4 viewportRect = { 0.0f, 0.0f, 1.0f, 1.0f };
        uint32_t cullingMask = 0xFFFFFFFF;
        std::string targetTexture = "Screen";
    };

    struct MapSpawn {
        Vec3 position = { 0.0f, 1.8f, 0.0f };
        float yaw = 0.0f;
    };

    enum class SpawnType {
        FFA = 0,        // Deathmatch / Free-for-all neutral spawn (info_player_deathmatch)
        TeamAlpha = 1,  // Team 1 / Alpha (info_player_team1)
        TeamBeta = 2    // Team 2 / Beta (info_player_team2)
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
        int weaponId = 0;
        std::string prefabRef = "spawner_item";
        Vec3 position = { 0.0f, 0.0f, 0.0f };
        float yaw = 0.0f;
        float respawnTime = 60.0f;

        std::string getWeaponName() const {
            if (!prefabRef.empty()) return prefabRef;
            return "Item_" + std::to_string(weaponId);
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

    struct MapSprite2D {
        std::string name = "";
        std::string script = "";
        std::string texturePath = "";
        Vec3 position = { 0.0f, 0.0f, 0.0f };
        Vec2 size = { 1.0f, 1.0f };
        float rotation = 0.0f;
        int layer = 0;
        bool billboard = true; // Faces camera in 3D or fixed in 2D
        Vec4 color = { 1.0f, 1.0f, 1.0f, 1.0f };
        Vec2 uvMin = { 0.0f, 0.0f };
        Vec2 uvMax = { 1.0f, 1.0f };
        std::string animationName = "";
        int frameIndex = 0;
    };

    class LabMap;

    struct MapEntityHandle {
        enum class Type { None, Brush, Prop, Door, Sprite, Light, Camera };
        Type type = Type::None;
        size_t index = 0;
        std::string name = "";
        LabMap* map = nullptr;

        Vec3* getPosition() const;
        Vec3* getRotation() const;
        Vec3* getColor() const;

        bool isValid() const { return type != Type::None && getPosition() != nullptr; }
    };

    class LabMap {
    public:
        virtual ~LabMap() = default;

        MapMetadata metadata;
        MapSpawn spawn; // Legacy fallback single spawn
        std::vector<MapBrush> brushes;
        std::vector<MapProp> props;
        std::vector<MapDoor> doors;
        std::vector<MapSpawnPoint> spawnPoints;
        std::vector<MapWeaponSpawner> weaponSpawners;
        std::vector<MapLight> lights;
        std::vector<MapSprite2D> sprites;
        std::vector<MapCamera> cameras;

        std::vector<MapSpawnPoint> getSpawnsForTeam(GameMode mode, int team) const;
        MapSpawnPoint selectBestSpawn(GameMode mode, int team, const std::vector<Vec3>& enemyPositions = {}) const;

        MapEntityHandle findEntityByName(const std::string& name);

        static std::unique_ptr<LabMap> loadFromFile(const std::string& filePath);
        bool saveToFile(const std::string& filePath) const;
    };

} // namespace Lab
