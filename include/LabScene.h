#pragma once

#include "LabMap.h"
#include "LabEntity.h"
#include "LabCamera.h"
#include "LabSceneNode.h"
#include <string>
#include <vector>
#include <memory>
#include <algorithm>

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
        using Scene = LabScene;
        SceneEnvironment environment;
        std::vector<std::shared_ptr<Entity>> entities;

        LabScene() = default;
        virtual ~LabScene() = default;

        std::shared_ptr<Entity> CreateEntity(const std::string& name = "Entity", const std::string& tag = "") {
            auto ent = std::make_shared<Entity>(name, tag);
            entities.push_back(ent);
            return ent;
        }

        std::shared_ptr<SceneNode> getRootNode() {
            if (!m_rootNode) {
                m_rootNode = std::make_shared<SceneNode>("SceneRoot");
            }
            return m_rootNode;
        }

        std::shared_ptr<Entity> CreateEntityFromNode(std::shared_ptr<SceneNode> node) {
            if (!node) return nullptr;
            auto ent = CreateEntity(node->getName());
            ent->setSceneNode(node);
            node->setEntity(ent.get());
            for (const auto& childNode : node->getChildren()) {
                if (childNode) {
                    auto childEnt = CreateEntityFromNode(childNode);
                    if (childEnt) {
                        childEnt->setParent(ent);
                    }
                }
            }
            return ent;
        }

        std::shared_ptr<Entity> InstantiatePrefab(const Prefab& prefab, const std::string& name = "") {
            auto node = prefab.instantiate();
            if (!node) return nullptr;
            if (!name.empty()) node->setName(name);
            return CreateEntityFromNode(node);
        }

        void syncSceneGraph() {
            auto root = getRootNode();
            for (auto& ent : entities) {
                if (!ent) continue;
                ent->syncToSceneNode();
                auto node = ent->getSceneNode();
                if (node && !node->getParent()) {
                    root->addChild(node);
                }
            }
        }

        void synchronizeMapToEntities();
        void synchronizeEntitiesToMap();

        Entity* FindByTag(const std::string& tag) const {
            for (const auto& ent : entities) {
                if (ent && ent->tag == tag) return ent.get();
            }
            return nullptr;
        }

        std::vector<Entity*> FindAllByTag(const std::string& tag) const {
            std::vector<Entity*> res;
            for (const auto& ent : entities) {
                if (ent && ent->tag == tag) res.push_back(ent.get());
            }
            return res;
        }

        Entity* FindByName(const std::string& name) const {
            for (const auto& ent : entities) {
                if (ent && ent->name == name) return ent.get();
            }
            return nullptr;
        }

        Camera* GetMainCamera() const {
            for (const auto& ent : entities) {
                if (ent && ent->tag == "MainCamera") {
                    Camera* cam = ent->GetComponent<Camera>();
                    if (cam && cam->enabled) return cam;
                }
            }
            Camera* best = nullptr;
            for (const auto& ent : entities) {
                if (!ent) continue;
                Camera* cam = ent->GetComponent<Camera>();
                if (cam && cam->enabled) {
                    if (!best || cam->depth < best->depth) {
                        best = cam;
                    }
                }
            }
            return best;
        }

        std::vector<Camera*> GetCamerasSortedByDepth() const {
            std::vector<Camera*> cams;
            for (const auto& ent : entities) {
                if (!ent) continue;
                Camera* cam = ent->GetComponent<Camera>();
                if (cam && cam->enabled) cams.push_back(cam);
            }
            std::sort(cams.begin(), cams.end(), [](const Camera* a, const Camera* b) {
                return a->depth < b->depth;
            });
            return cams;
        }

        void Update(float dt) {
            for (auto& ent : entities) {
                if (ent) ent->Update(dt);
            }
        }

        void LateUpdate(float dt) {
            for (auto& ent : entities) {
                if (ent) ent->LateUpdate(dt);
            }
        }

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

        /// @brief Registers custom mapping between numeric item/weapon ID and prefab reference
        static void registerItemPrefab(int itemId, const std::string& prefabRef);

        /// @brief Maps numeric weapon/item IDs to prefab reference names (data-driven registry)
        static std::string weaponIdToPrefab(int weaponId);

        /// @brief Maps prefab reference names back to item/weapon IDs
        static int prefabToWeaponId(const std::string& prefabRef);

    private:
        std::shared_ptr<SceneNode> m_rootNode;
    };

    using Scene = LabScene;

} // namespace Lab
