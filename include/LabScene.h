#pragma once

#include "LabMap.h"
#include <string>
#include <vector>
#include <memory>

namespace Lab {

    struct SceneEnvironment {
        std::string name = "Untitled Scene";
        std::string author = "Unknown";
        Vec3 ambientColor = { 0.2f, 0.25f, 0.35f };
        Vec3 sunDir = { -0.4f, -0.8f, -0.4f };
        Vec3 sunColor = { 0.9f, 0.95f, 1.0f };
        std::string skyboxHdr = "";
        float exposure = 1.0f;
        float fogDistance = 100.0f;
        float fogHeight = 0.0f;
        Vec3 fogColor = { 0.5f, 0.6f, 0.7f };
    };

    struct SceneConversionResult {
        bool success = false;
        std::string scenePath;
        std::string backupPath;
        std::string errorMessage;
    };

    class LabScene : public LabMap {
    public:
        SceneEnvironment environment;

        LabScene() = default;
        virtual ~LabScene() = default;

        /// @brief Applies scene environment (sun, ambient, fog, skybox) to renderer
        void applyEnvironment() const;

        /// @brief Converts an existing LabMap into a modern LabScene object
        static std::unique_ptr<LabScene> fromLabMap(const LabMap& map);

        /// @brief Detects if a map file is in legacy .labmap format.
        /// Scans for legacy tokens: "brush{", "brush", "spawn_player", "weapon_spawner", "LABMAP_VERSION".
        /// Returns false if the file has .labscene extension or contains LABSCENE_VERSION.
        static bool isLegacyMapFile(const std::string& filePath);

        /// @brief Converts legacy .labmap to modern .labscene:
        /// - brushes -> static geometry nodes
        /// - rest (props, doors, lights, cameras, spawns) -> scene entities
        /// - weaponId -> prefabRef (e.g. weapon_pipe, weapon_m4a4s)
        /// - writes .labscene alongside original
        /// - creates .backup copy of original
        static SceneConversionResult convertLegacyMapToScene(const std::string& labmapPath);

        /// @brief Saves scene to .labscene file format
        bool saveSceneToFile(const std::string& filePath) const;

        /// @brief Loads scene from .labscene file format
        static std::unique_ptr<LabScene> loadSceneFromFile(const std::string& filePath);

        /// @brief Maps numeric weapon IDs (0..8) to prefab reference names
        static std::string weaponIdToPrefab(int weaponId);

        /// @brief Maps prefab reference names back to legacy weapon IDs
        static int prefabToWeaponId(const std::string& prefabRef);
    };

} // namespace Lab
