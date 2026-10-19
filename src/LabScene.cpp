#include "LabScene.h"
#include "LabCore.h"
#include "LabRenderer.h"
#include "LabComponents.h"
#include <fstream>
#include <sstream>
#include <filesystem>
#include <iostream>
#include <algorithm>

namespace Lab {

    static std::string resolveScenePath(const std::string& path) {
        if (std::filesystem::exists(path)) return path;
        std::string fname = std::filesystem::path(path).filename().string();
        if (std::filesystem::exists(fname)) return fname;
        if (std::filesystem::exists("assets/maps/" + fname)) return "assets/maps/" + fname;
        if (std::filesystem::exists("../assets/maps/" + fname)) return "../assets/maps/" + fname;
        if (std::filesystem::exists("../../assets/maps/" + fname)) return "../../assets/maps/" + fname;
        if (std::filesystem::exists("assets/maps/" + path)) return "assets/maps/" + path;
        if (std::filesystem::exists("../assets/maps/" + path)) return "../assets/maps/" + path;
        if (std::filesystem::exists("../../assets/maps/" + path)) return "../../assets/maps/" + path;
        return path;
    }

    static std::unordered_map<int, std::string>& getItemPrefabRegistry() {
        static std::unordered_map<int, std::string> s_registry = {
            { 0, "weapon_pipe" },
            { 1, "weapon_pistol" },
            { 2, "weapon_shotgun" },
            { 3, "weapon_m4a4s" },
            { 4, "weapon_sg553" },
            { 5, "weapon_minigun" },
            { 6, "weapon_plasma" },
            { 7, "weapon_railgun" },
            { 8, "weapon_rpg" }
        };
        return s_registry;
    }

    void LabScene::registerItemPrefab(int itemId, const std::string& prefabRef) {
        getItemPrefabRegistry()[itemId] = prefabRef;
    }

    std::string LabScene::weaponIdToPrefab(int weaponId) {
        const auto& reg = getItemPrefabRegistry();
        auto it = reg.find(weaponId);
        if (it != reg.end()) {
            return it->second;
        }
        return "item_" + std::to_string(weaponId);
    }

    int LabScene::prefabToWeaponId(const std::string& prefabRef) {
        const auto& reg = getItemPrefabRegistry();
        for (const auto& [id, name] : reg) {
            if (name == prefabRef) return id;
            if (("prefabs/weapons/" + name + ".prefab") == prefabRef) return id;
            if (name.rfind("weapon_", 0) == 0 && name.substr(7) == prefabRef) return id;
        }

        try {
            return std::stoi(prefabRef);
        } catch (...) {
            return 0;
        }
    }

    bool LabScene::isLegacyMapFile(const std::string& filePath) {
        if (filePath.empty()) return false;

        std::string resolved = resolveScenePath(filePath);
        if (!std::filesystem::exists(resolved)) {
            return false;
        }

        std::filesystem::path p(resolved);
        std::string ext = p.extension().string();
        for (auto& c : ext) c = (char)tolower(c);
        if (ext == ".labscene") {
            return false; // Modern scene is never legacy
        }

        std::ifstream file(resolved);
        if (!file.is_open()) return false;

        std::string line;
        bool foundLegacyToken = false;
        while (std::getline(file, line)) {
            // Trim leading whitespace
            size_t first = line.find_first_not_of(" \t\r\n");
            if (first == std::string::npos) continue;
            std::string trimmed = line.substr(first);

            // If it declares modern LABSCENE_VERSION, it's not legacy
            if (trimmed.find("LABSCENE_VERSION") != std::string::npos) {
                return false;
            }

            // Check for legacy tokens: brush{, brush, spawn_player, weapon_spawner, LABMAP_VERSION
            if (trimmed.find("LABMAP_VERSION") != std::string::npos ||
                trimmed.find("brush{") != std::string::npos ||
                trimmed.rfind("brush ", 0) == 0 ||
                trimmed.rfind("poly_brush", 0) == 0 ||
                trimmed.find("spawn_player") != std::string::npos ||
                trimmed.find("weapon_spawner") != std::string::npos) {
                foundLegacyToken = true;
            }
        }

        return foundLegacyToken;
    }

    std::unique_ptr<LabScene> LabScene::fromLabMap(const LabMap& map) {
        auto scene = std::make_unique<LabScene>();
        scene->metadata = map.metadata;
        scene->environment.name = map.metadata.name;
        scene->environment.author = map.metadata.author;
        scene->environment.ambientColor = map.metadata.ambientColor;
        scene->environment.sunDir = map.metadata.sunDir;
        scene->environment.sunColor = map.metadata.sunColor;
        if (scene->environment.skyboxHdr.empty()) {
            scene->environment.skyboxHdr = map.metadata.cubemapPath;
        }

        scene->spawn = map.spawn;
        scene->brushes = map.brushes;
        scene->props = map.props;
        scene->doors = map.doors;
        scene->triggers = map.triggers;
        scene->waypoints = map.waypoints;
        scene->spawnPoints = map.spawnPoints;
        scene->lights = map.lights;
        scene->sprites = map.sprites;
        scene->cameras = map.cameras;

        scene->weaponSpawners = map.weaponSpawners;
        for (auto& ws : scene->weaponSpawners) {
            if (ws.prefabRef.empty() || ws.prefabRef == "weapon_shotgun") {
                ws.prefabRef = weaponIdToPrefab(ws.weaponId);
            }
        }

        scene->synchronizeMapToEntities();
        return scene;
    }

    void LabScene::applyEnvironment() const {
        Renderer::setSunLight(environment.sunDir, environment.sunColor, environment.ambientColor);
        Renderer::setFog(true, environment.fogColor, 5.0f, environment.fogDistance);
        if (environment.fogHeight > 0.001f) {
            Renderer::setHeightFog(true, environment.fogHeight, 0.04f);
        } else {
            Renderer::setHeightFog(false);
        }
    }

    SceneConversionResult LabScene::convertLegacyMapToScene(const std::string& labmapPath) {
        SceneConversionResult result;
        std::string resolved = resolveScenePath(labmapPath);
        if (!std::filesystem::exists(resolved)) {
            result.errorMessage = "Source map file not found: " + labmapPath;
            return result;
        }

        auto map = LabMap::loadFromFile(resolved);
        if (!map) {
            result.errorMessage = "Failed to parse legacy map: " + resolved;
            return result;
        }

        // 1. Create .backup copy of original
        std::string backupPath = resolved + ".backup";
        try {
            std::filesystem::copy_file(resolved, backupPath, std::filesystem::copy_options::overwrite_existing);
            // Also create without .labmap if ends in .labmap for dual compatibility
            std::string stemBackup = std::filesystem::path(resolved).replace_extension(".backup").string();
            if (stemBackup != backupPath) {
                std::filesystem::copy_file(resolved, stemBackup, std::filesystem::copy_options::overwrite_existing);
            }
            result.backupPath = backupPath;
        } catch (const std::exception& e) {
            LabLog::warn("Could not create map backup: " + std::string(e.what()));
            result.backupPath = backupPath;
        }

        // 2. Build modern LabScene
        auto scene = fromLabMap(*map);

        // 3. Determine destination .labscene path
        std::string scenePath = std::filesystem::path(resolved).replace_extension(".labscene").string();
        if (scene->saveSceneToFile(scenePath)) {
            result.success = true;
            result.scenePath = scenePath;
            LabLog::info("Successfully converted legacy map '" + resolved + "' to modern scene '" + scenePath + "'");
        } else {
            result.errorMessage = "Failed to save modern .labscene to: " + scenePath;
        }

        return result;
    }

    bool LabScene::saveSceneToFile(const std::string& filePath) const {
        std::ofstream file(filePath);
        if (!file.is_open()) {
            LabLog::error("Failed to save scene to: " + filePath);
            return false;
        }

        file << "LABSCENE_VERSION 1\n\n";

        file << "ENVIRONMENT\n";
        file << "    name \"" << (environment.name.empty() ? metadata.name : environment.name) << "\"\n";
        file << "    author \"" << (environment.author.empty() ? metadata.author : environment.author) << "\"\n";
        file << "    ambient " << environment.ambientColor.x << " " << environment.ambientColor.y << " " << environment.ambientColor.z << "\n";
        file << "    sun_dir " << environment.sunDir.x << " " << environment.sunDir.y << " " << environment.sunDir.z << "\n";
        file << "    sun_color " << environment.sunColor.x << " " << environment.sunColor.y << " " << environment.sunColor.z << "\n";
        file << "    skybox_hdr \"" << environment.skyboxHdr << "\"\n";
        if (!metadata.cubemapPath.empty()) {
            file << "    cubemap " << metadata.cubemapPath << "\n";
        }
        file << "    exposure " << environment.exposure << "\n";
        file << "    fog_distance " << environment.fogDistance << "\n";
        file << "    fog_height " << environment.fogHeight << "\n";
        file << "    fog_color " << environment.fogColor.x << " " << environment.fogColor.y << " " << environment.fogColor.z << "\n";
        file << "END_ENVIRONMENT\n\n";

        file << "STATIC\n";
        for (const auto& b : brushes) {
            if (b.type == "poly" && !b.customVertices.empty()) {
                file << "    static_mesh poly "
                     << b.position.x << " " << b.position.y << " " << b.position.z << " "
                     << b.size.x << " " << b.size.y << " " << b.size.z << " "
                     << b.color.x << " " << b.color.y << " " << b.color.z << " "
                     << (b.texturePath.empty() ? "\"\"" : b.texturePath) << " "
                     << b.uvScale.x << " " << b.uvScale.y << " " << b.uvMode << " "
                     << b.customVertices.size() << " " << b.customIndices.size() << "\n";
                for (const auto& v : b.customVertices) {
                    file << "        v " << v.position.x << " " << v.position.y << " " << v.position.z << " "
                         << v.normal.x << " " << v.normal.y << " " << v.normal.z << " "
                         << v.texCoords.x << " " << v.texCoords.y << "\n";
                }
                file << "        idx";
                for (unsigned int idxVal : b.customIndices) {
                    file << " " << idxVal;
                }
                file << "\n";
                if (b.rotation.x != 0.0f || b.rotation.y != 0.0f || b.rotation.z != 0.0f) {
                    file << "        rot " << b.rotation.x << " " << b.rotation.y << " " << b.rotation.z << "\n";
                }
            } else {
                file << "    static_mesh " << b.type << " "
                     << b.position.x << " " << b.position.y << " " << b.position.z << " "
                     << b.size.x << " " << b.size.y << " " << b.size.z << " "
                     << b.color.x << " " << b.color.y << " " << b.color.z << " "
                     << (b.texturePath.empty() ? "\"\"" : b.texturePath) << " "
                     << b.uvScale.x << " " << b.uvScale.y << " " << b.uvMode;
                if (b.rotation.x != 0.0f || b.rotation.y != 0.0f || b.rotation.z != 0.0f) {
                    file << " " << b.rotation.x << " " << b.rotation.y << " " << b.rotation.z;
                }
                file << "\n";
            }
            if (!b.name.empty()) {
                file << "        name \"" << b.name << "\"\n";
            }
            if (!b.normalMapPath.empty()) {
                file << "        normal_map " << b.normalMapPath << "\n";
            }
            if (!b.script.empty()) {
                file << "        script " << b.script << "\n";
            }
        }
        file << "END_STATIC\n\n";

        file << "ENTITIES\n";
        file << "    entity spawn " << spawn.position.x << " " << spawn.position.y << " " << spawn.position.z << " " << spawn.yaw << "\n";

        for (const auto& sp : spawnPoints) {
            std::string typeStr = (sp.type == SpawnType::TeamAlpha) ? "team_alpha" :
                                  (sp.type == SpawnType::TeamBeta)  ? "team_beta"  : "ffa";
            file << "    entity spawn_point " << sp.entityClass << " "
                 << sp.position.x << " " << sp.position.y << " " << sp.position.z << " "
                 << sp.yaw << " " << typeStr << "\n";
        }

        for (const auto& ws : weaponSpawners) {
            std::string pref = ws.prefabRef.empty() ? weaponIdToPrefab(ws.weaponId) : ws.prefabRef;
            file << "    entity weapon_spawner " << pref << " "
                 << ws.position.x << " " << ws.position.y << " " << ws.position.z << " "
                 << ws.yaw << " " << ws.respawnTime << "\n";
        }

        for (const auto& c : cameras) {
            file << "    entity camera " << (c.name.empty() ? "camera_main" : c.name) << " "
                 << c.position.x << " " << c.position.y << " " << c.position.z << " "
                 << c.rotation.x << " " << c.rotation.y << " " << c.rotation.z << " "
                 << c.fov << " " << (c.follow_target.empty() ? "\"\"" : c.follow_target) << " "
                 << c.tpp_offset.x << " " << c.tpp_offset.y << " " << c.tpp_offset.z << " "
                 << c.collision_radius << " " << (c.active ? 1 : 0) << "\n";
            if (!c.script.empty()) {
                file << "        script " << c.script << "\n";
            }
        }

        for (const auto& p : props) {
            file << "    entity prop " << p.modelPath << " "
                 << p.position.x << " " << p.position.y << " " << p.position.z << " "
                 << p.rotation.x << " " << p.rotation.y << " " << p.rotation.z << " "
                 << p.scale.x << " " << p.scale.y << " " << p.scale.z << " "
                 << p.color.x << " " << p.color.y << " " << p.color.z << " "
                 << (p.texturePath.empty() ? "\"\"" : p.texturePath) << "\n";
            if (!p.name.empty()) {
                file << "        name \"" << p.name << "\"\n";
            }
            if (!p.normalMapPath.empty()) {
                file << "        normal_map " << p.normalMapPath << "\n";
            }
            if (!p.script.empty()) {
                file << "        script " << p.script << "\n";
            }
        }

        for (const auto& d : doors) {
            file << "    entity door " << d.name << " "
                 << d.position.x << " " << d.position.y << " " << d.position.z << " "
                 << d.size.x << " " << d.size.y << " " << d.size.z << " "
                 << d.openOffset.x << " " << d.openOffset.y << " " << d.openOffset.z << " "
                 << d.color.x << " " << d.color.y << " " << d.color.z << " "
                 << d.openSpeed << " " << d.triggerRadius << "\n";
            if (!d.script.empty()) {
                file << "        script " << d.script << "\n";
            }
            if (!d.activator.empty() && d.activator != "proximity") {
                file << "        activator " << d.activator << "\n";
            }
            if (d.requiresKeycard) {
                file << "        keycard " << (d.keycardRequired.empty() ? "keycard_l1" : d.keycardRequired) << "\n";
            }
            if (!d.dependency.empty()) {
                file << "        dependency " << d.dependency << "\n";
            }
            if (d.isLocked) {
                file << "        locked 1\n";
            }
            if (!d.autoClose) {
                file << "        auto_close 0\n";
            }
        }

        for (const auto& tr : triggers) {
            file << "    entity trigger_volume " << (tr.name.empty() ? "trigger_story" : tr.name) << " "
                 << tr.position.x << " " << tr.position.y << " " << tr.position.z << " "
                 << tr.size.x << " " << tr.size.y << " " << tr.size.z << " "
                 << (tr.eventType.empty() ? "story_dialog" : tr.eventType) << " "
                 << (tr.targetBot.empty() ? "\"\"" : tr.targetBot) << " "
                 << (tr.once ? 1 : 0) << " "
                 << (tr.audioPath.empty() ? "\"\"" : tr.audioPath) << " "
                 << "\"" << tr.dialogText << "\"\n";
            if (!tr.script.empty()) {
                file << "        script " << tr.script << "\n";
            }
            if (!tr.targetEntity.empty()) {
                file << "        target_entity " << tr.targetEntity << "\n";
            }
            if (!tr.targetWaypoint.empty()) {
                file << "        target_waypoint " << tr.targetWaypoint << "\n";
            }
            if (!tr.targetAction.empty() && tr.targetAction != "open") {
                file << "        target_action " << tr.targetAction << "\n";
            }
        }

        for (const auto& wp : waypoints) {
            std::string bName = wp.botName.empty() ? "\"\"" : wp.botName;
            if (bName.find(' ') != std::string::npos && bName.front() != '"') {
                bName = "\"" + bName + "\"";
            }
            file << "    entity waypoint " << (wp.name.empty() ? "wp" : wp.name) << " "
                 << wp.position.x << " " << wp.position.y << " " << wp.position.z << " "
                 << bName << " "
                 << wp.orderIndex << " "
                 << (wp.animation.empty() ? "Idle" : wp.animation) << " "
                 << wp.facingYaw << " "
                 << wp.waitDuration << "\n";
            if (!wp.targetTrigger.empty()) {
                file << "        target_trigger " << wp.targetTrigger << "\n";
            }
            if (!wp.triggerAction.empty() && wp.triggerAction != "fire") {
                file << "        trigger_action " << wp.triggerAction << "\n";
            }
        }

        for (const auto& lt : lights) {
            std::string typeStr = (lt.type == MapLightType::Spot) ? "spot" : "point";
            file << "    entity light " << lt.name << " "
                 << lt.position.x << " " << lt.position.y << " " << lt.position.z << " "
                 << lt.color.x << " " << lt.color.y << " " << lt.color.z << " "
                 << lt.intensity << " " << lt.radius << " " << typeStr << "\n";
        }

        for (const auto& sp : sprites) {
            file << "    entity sprite_2d " << (sp.texturePath.empty() ? "\"\"" : sp.texturePath) << " "
                 << sp.position.x << " " << sp.position.y << " " << sp.position.z << " "
                 << sp.size.x << " " << sp.size.y << " "
                 << sp.rotation << " " << sp.layer << " " << (sp.billboard ? 1 : 0) << " "
                 << sp.color.x << " " << sp.color.y << " " << sp.color.z << " " << sp.color.w << " "
                 << sp.uvMin.x << " " << sp.uvMin.y << " " << sp.uvMax.x << " " << sp.uvMax.y << "\n";
            if (!sp.name.empty()) {
                file << "        name \"" << sp.name << "\"\n";
            }
            if (!sp.script.empty()) {
                file << "        script " << sp.script << "\n";
            }
        }

        // ECS Dynamic Entities not backed by legacy arrays
        for (const auto& ent : entities) {
            if (!ent) continue;
            // Check if this entity already matches a prop/camera/light/door
            bool isMapped = false;
            for (const auto& p : props) { if (!p.name.empty() && p.name == ent->name) { isMapped = true; break; } }
            if (!isMapped) { for (const auto& c : cameras) { if (!c.name.empty() && c.name == ent->name) { isMapped = true; break; } } }
            if (!isMapped) { for (const auto& l : lights) { if (!l.name.empty() && l.name == ent->name) { isMapped = true; break; } } }
            if (!isMapped) { for (const auto& d : doors) { if (!d.name.empty() && d.name == ent->name) { isMapped = true; break; } } }

            if (!isMapped) {
                Vec3 pPos = ent->transform ? ent->transform->position : Vec3(0, 0, 0);
                Vec3 pScale = ent->transform ? ent->transform->scale : Vec3(1, 1, 1);
                Vec3 pEuler = ent->transform ? ent->transform->rotation.toEuler() : Vec3(0, 0, 0);
                // Convert radians to degrees
                pEuler = pEuler * (180.0f / 3.14159265f);
                std::string mPath = "";
                std::string tPath = "";
                Vec3 mColor(1, 1, 1);
                auto mc = ent->GetComponent<MeshComponent>();
                if (mc) {
                    mPath = mc->modelPath;
                    tPath = mc->texturePath;
                    mColor = mc->color;
                }
                file << "    entity ecs_node " << (mPath.empty() ? "\"\"" : mPath) << " "
                     << pPos.x << " " << pPos.y << " " << pPos.z << " "
                     << pEuler.x << " " << pEuler.y << " " << pEuler.z << " "
                     << pScale.x << " " << pScale.y << " " << pScale.z << " "
                     << mColor.x << " " << mColor.y << " " << mColor.z << " "
                     << (tPath.empty() ? "\"\"" : tPath) << "\n";
                if (!ent->name.empty()) {
                    file << "        name \"" << ent->name << "\"\n";
                }
                if (!ent->tag.empty()) {
                    file << "        tag \"" << ent->tag << "\"\n";
                }
            }
        }

        file << "END_ENTITIES\n";
        LabLog::info("Successfully saved .labscene to: " + filePath);
        return true;
    }

    std::unique_ptr<LabScene> LabScene::loadSceneFromFile(const std::string& filePath) {
        if (filePath.empty()) return nullptr;
        std::string resolved = resolveScenePath(filePath);
        std::string ext = std::filesystem::path(resolved).extension().string();
        for (auto& c : ext) c = static_cast<char>(::tolower(static_cast<unsigned char>(c)));
        if (ext != ".labscene" && ext != ".labmap" && ext != ".backup") {
            LabLog::error("Rejected non-scene file extension: " + filePath);
            return nullptr;
        }

        std::ifstream file(resolved);
        if (!file.is_open()) {
            LabLog::error("Failed to open scene file: " + filePath + " (resolved: " + resolved + ")");
            return nullptr;
        }

        auto scene = std::make_unique<LabScene>();
        std::string line;
        std::string currentSection = "";
        bool hasValidSceneStructure = false;
        enum class LastParsedEntity { None, Brush, Prop, Door, Camera, Sprite, Light, Trigger, Waypoint };
        LastParsedEntity lastParsed = LastParsedEntity::None;

        while (std::getline(file, line)) {
            size_t first = line.find_first_not_of(" \t\r\n");
            if (first == std::string::npos) continue;
            if (line[first] == '#' || (line[first] == '/' && line.length() > first + 1 && line[first + 1] == '/')) continue;

            std::stringstream ss(line.substr(first));
            std::string token;
            ss >> token;

            if (token == "LABSCENE_VERSION" || token == "LABMAP_VERSION") {
                int ver = 1;
                ss >> ver;
                hasValidSceneStructure = true;
            } else if (token == "ENVIRONMENT" || token == "METADATA") {
                currentSection = "ENVIRONMENT";
                hasValidSceneStructure = true;
            } else if (token == "END_ENVIRONMENT" || token == "END_METADATA") {
                currentSection = "";
            } else if (token == "STATIC") {
                currentSection = "STATIC";
                hasValidSceneStructure = true;
            } else if (token == "END_STATIC") {
                currentSection = "";
            } else if (token == "ENTITIES") {
                currentSection = "ENTITIES";
                hasValidSceneStructure = true;
            } else if (token == "END_ENTITIES") {
                currentSection = "";
            } else if (currentSection == "ENVIRONMENT") {
                if (token == "name") {
                    std::string rest;
                    std::getline(ss, rest);
                    size_t q1 = rest.find('"'), q2 = rest.rfind('"');
                    if (q1 != std::string::npos && q2 != std::string::npos && q2 > q1) {
                        scene->environment.name = rest.substr(q1 + 1, q2 - q1 - 1);
                    } else {
                        scene->environment.name = rest;
                    }
                    scene->metadata.name = scene->environment.name;
                } else if (token == "author") {
                    std::string rest;
                    std::getline(ss, rest);
                    size_t q1 = rest.find('"'), q2 = rest.rfind('"');
                    if (q1 != std::string::npos && q2 != std::string::npos && q2 > q1) {
                        scene->environment.author = rest.substr(q1 + 1, q2 - q1 - 1);
                    } else {
                        scene->environment.author = rest;
                    }
                    scene->metadata.author = scene->environment.author;
                } else if (token == "ambient") {
                    ss >> scene->environment.ambientColor.x >> scene->environment.ambientColor.y >> scene->environment.ambientColor.z;
                    scene->metadata.ambientColor = scene->environment.ambientColor;
                } else if (token == "sun_dir") {
                    ss >> scene->environment.sunDir.x >> scene->environment.sunDir.y >> scene->environment.sunDir.z;
                    scene->metadata.sunDir = scene->environment.sunDir;
                } else if (token == "sun_color") {
                    ss >> scene->environment.sunColor.x >> scene->environment.sunColor.y >> scene->environment.sunColor.z;
                    scene->metadata.sunColor = scene->environment.sunColor;
                } else if (token == "skybox_hdr" || token == "skybox") {
                    std::string rest;
                    std::getline(ss, rest);
                    size_t q1 = rest.find('"'), q2 = rest.rfind('"');
                    if (q1 != std::string::npos && q2 != std::string::npos && q2 > q1) {
                        scene->environment.skyboxHdr = rest.substr(q1 + 1, q2 - q1 - 1);
                    } else {
                        std::stringstream srest(rest);
                        srest >> scene->environment.skyboxHdr;
                    }
                } else if (token == "cubemap") {
                    ss >> scene->metadata.cubemapPath;
                    if (scene->metadata.cubemapPath.size() >= 2 &&
                        scene->metadata.cubemapPath.front() == '"' &&
                        scene->metadata.cubemapPath.back() == '"') {
                        scene->metadata.cubemapPath = scene->metadata.cubemapPath.substr(1, scene->metadata.cubemapPath.size() - 2);
                    }
                } else if (token == "exposure") {
                    ss >> scene->environment.exposure;
                } else if (token == "fog_distance") {
                    ss >> scene->environment.fogDistance;
                } else if (token == "fog_height") {
                    ss >> scene->environment.fogHeight;
                } else if (token == "fog_color") {
                    ss >> scene->environment.fogColor.x >> scene->environment.fogColor.y >> scene->environment.fogColor.z;
                }
            } else if (currentSection == "STATIC") {
                if (token == "static_mesh" || token == "static" || token == "brush") {
                    std::string bType;
                    ss >> bType;
                    if (bType == "poly") {
                        MapBrush b;
                        b.type = "poly";
                        size_t vCount = 0, iCount = 0;
                        ss >> b.position.x >> b.position.y >> b.position.z
                           >> b.size.x >> b.size.y >> b.size.z
                           >> b.color.x >> b.color.y >> b.color.z;
                        if (ss >> b.texturePath) {
                            if (b.texturePath == "\"\"") b.texturePath = "";
                        }
                        if (ss >> b.uvScale.x >> b.uvScale.y) {
                            ss >> b.uvMode;
                        }
                        if (ss >> vCount >> iCount) {
                            for (size_t vi = 0; vi < vCount && std::getline(file, line); ++vi) {
                                std::stringstream vss(line);
                                std::string vTok;
                                vss >> vTok;
                                if (vTok == "v") {
                                    Vec3 vp, vn;
                                    Vec2 vt;
                                    vss >> vp.x >> vp.y >> vp.z >> vn.x >> vn.y >> vn.z >> vt.x >> vt.y;
                                    b.customVertices.push_back(Vertex(vp, vn, vt, b.color));
                                }
                            }
                            if (std::getline(file, line)) {
                                std::stringstream iss(line);
                                std::string idxTok;
                                iss >> idxTok;
                                for (size_t ii = 0; ii < iCount; ++ii) {
                                    unsigned int idxVal = 0;
                                    if (iss >> idxVal) b.customIndices.push_back(idxVal);
                                }
                            }
                        }
                        scene->brushes.push_back(b);
                        lastParsed = LastParsedEntity::Brush;
                    } else {
                        MapBrush b;
                        b.type = bType;
                        ss >> b.position.x >> b.position.y >> b.position.z
                           >> b.size.x >> b.size.y >> b.size.z
                           >> b.color.x >> b.color.y >> b.color.z;
                        if (ss >> b.texturePath) {
                            if (b.texturePath == "\"\"") b.texturePath = "";
                        }
                        if (ss >> b.uvScale.x >> b.uvScale.y) {
                            ss >> b.uvMode;
                        }
                        if (ss >> b.rotation.x >> b.rotation.y >> b.rotation.z) {
                            // Rotation inline
                        }
                        scene->brushes.push_back(b);
                        lastParsed = LastParsedEntity::Brush;
                    }
                } else if (token == "rot" || token == "rotation") {
                    Vec3 r(0, 0, 0);
                    if (ss >> r.x >> r.y >> r.z) {
                        if (!scene->brushes.empty() && lastParsed == LastParsedEntity::Brush) {
                            scene->brushes.back().rotation = r;
                        }
                    }
                } else if (token == "normal_map") {
                    std::string nPath;
                    if (ss >> nPath && !scene->brushes.empty()) {
                        if (nPath == "\"\"") nPath = "";
                        scene->brushes.back().normalMapPath = nPath;
                    }
                } else if (token == "name") {
                    std::string rest;
                    std::getline(ss, rest);
                    size_t q1 = rest.find('"'), q2 = rest.rfind('"');
                    if (q1 != std::string::npos && q2 != std::string::npos && q2 > q1) {
                        if (!scene->brushes.empty()) scene->brushes.back().name = rest.substr(q1 + 1, q2 - q1 - 1);
                    }
                } else if (token == "script") {
                    std::string sPath;
                    if (ss >> sPath && !scene->brushes.empty()) {
                        if (sPath == "\"\"") sPath = "";
                        scene->brushes.back().script = sPath;
                    }
                }
            } else if (currentSection == "ENTITIES") {
                if (token == "entity") {
                    ss >> token; // consume 'entity' prefix
                }

                if (token == "spawn") {
                    ss >> scene->spawn.position.x >> scene->spawn.position.y >> scene->spawn.position.z >> scene->spawn.yaw;
                } else if (token == "spawn_point") {
                    MapSpawnPoint sp;
                    std::string typeStr;
                    ss >> sp.entityClass >> sp.position.x >> sp.position.y >> sp.position.z >> sp.yaw >> typeStr;
                    if (typeStr == "team_alpha" || typeStr == "1" || sp.entityClass == "info_player_team1") {
                        sp.type = SpawnType::TeamAlpha;
                    } else if (typeStr == "team_beta" || typeStr == "2" || sp.entityClass == "info_player_team2") {
                        sp.type = SpawnType::TeamBeta;
                    } else {
                        sp.type = SpawnType::FFA;
                    }
                    scene->spawnPoints.push_back(sp);
                } else if (token == "weapon_spawner") {
                    MapWeaponSpawner ws;
                    std::string pref;
                    ss >> pref >> ws.position.x >> ws.position.y >> ws.position.z >> ws.yaw >> ws.respawnTime;
                    ws.prefabRef = pref;
                    ws.weaponId = prefabToWeaponId(pref);
                    if (ws.respawnTime <= 0.0f) ws.respawnTime = 60.0f;
                    scene->weaponSpawners.push_back(ws);
                    lastParsed = LastParsedEntity::None;
                } else if (token == "prop") {
                    MapProp p;
                    ss >> p.modelPath >> p.position.x >> p.position.y >> p.position.z
                       >> p.rotation.x >> p.rotation.y >> p.rotation.z
                       >> p.scale.x >> p.scale.y >> p.scale.z
                       >> p.color.x >> p.color.y >> p.color.z;
                    if (ss >> p.texturePath) {
                        if (p.texturePath == "\"\"") p.texturePath = "";
                    }
                    scene->props.push_back(p);
                    lastParsed = LastParsedEntity::Prop;
                } else if (token == "door") {
                    MapDoor d;
                    ss >> d.name >> d.position.x >> d.position.y >> d.position.z
                       >> d.size.x >> d.size.y >> d.size.z
                       >> d.openOffset.x >> d.openOffset.y >> d.openOffset.z
                       >> d.color.x >> d.color.y >> d.color.z
                       >> d.openSpeed >> d.triggerRadius;
                    scene->doors.push_back(d);
                    lastParsed = LastParsedEntity::Door;
                } else if (token == "trigger_volume" || token == "trigger") {
                    MapTrigger tr;
                    int onceInt = 1;
                    ss >> tr.name >> tr.position.x >> tr.position.y >> tr.position.z
                       >> tr.size.x >> tr.size.y >> tr.size.z
                       >> tr.eventType >> tr.targetBot >> onceInt;
                    tr.once = (onceInt != 0);
                    if (tr.targetBot == "\"\"") tr.targetBot = "";
                    if (ss >> tr.audioPath) {
                        if (tr.audioPath == "\"\"") tr.audioPath = "";
                    }
                    std::string rest;
                    std::getline(ss, rest);
                    size_t q1 = rest.find('"'), q2 = rest.rfind('"');
                    if (q1 != std::string::npos && q2 != std::string::npos && q2 > q1) {
                        tr.dialogText = rest.substr(q1 + 1, q2 - q1 - 1);
                    }
                    scene->triggers.push_back(tr);
                    lastParsed = LastParsedEntity::Trigger;
                } else if (token == "waypoint" || token == "path_node") {
                    MapWaypoint wp;
                    ss >> wp.name >> wp.position.x >> wp.position.y >> wp.position.z;
                    std::string botToken;
                    ss >> botToken;
                    if (!botToken.empty() && botToken.front() == '"') {
                        if (botToken.back() == '"' && botToken.length() > 1) {
                            wp.botName = (botToken == "\"\"") ? "" : botToken.substr(1, botToken.length() - 2);
                        } else {
                            std::string remaining;
                            while (ss >> remaining) {
                                botToken += " " + remaining;
                                if (!remaining.empty() && remaining.back() == '"') break;
                            }
                            if (botToken.length() >= 2 && botToken.back() == '"') {
                                wp.botName = botToken.substr(1, botToken.length() - 2);
                            } else {
                                wp.botName = botToken;
                            }
                        }
                    } else {
                        wp.botName = (botToken == "\"\"") ? "" : botToken;
                    }
                    ss >> wp.orderIndex;
                    std::string anim;
                    float yaw = 0.0f, waitDur = 2.0f;
                    if (ss >> anim) {
                        if (anim != "\"\"") wp.animation = anim;
                    }
                    if (ss >> yaw) {
                        wp.facingYaw = yaw;
                    }
                    if (ss >> waitDur) {
                        wp.waitDuration = waitDur;
                    }
                    scene->waypoints.push_back(wp);
                    lastParsed = LastParsedEntity::Waypoint;
                } else if (token == "camera") {
                    MapCamera c;
                    int act = 1;
                    ss >> c.name
                       >> c.position.x >> c.position.y >> c.position.z
                       >> c.rotation.x >> c.rotation.y >> c.rotation.z
                       >> c.fov >> c.follow_target
                       >> c.tpp_offset.x >> c.tpp_offset.y >> c.tpp_offset.z
                       >> c.collision_radius >> act;
                    if (c.follow_target == "\"\"") c.follow_target = "";
                    c.active = (act != 0);
                    scene->cameras.push_back(c);
                    lastParsed = LastParsedEntity::Camera;
                } else if (token == "light" || token == "light_omni" || token == "light_spot") {
                    MapLight lt;
                    std::string firstArg;
                    if (ss >> firstArg) {
                        bool isNumber = (!firstArg.empty() && (std::isdigit(firstArg[0]) || firstArg[0] == '-' || firstArg[0] == '+' || firstArg[0] == '.'));
                        if (isNumber) {
                            try {
                                lt.position.x = std::stof(firstArg);
                                lt.name = token;
                            } catch (...) {
                                lt.name = firstArg;
                                ss >> lt.position.x;
                            }
                        } else {
                            lt.name = firstArg;
                            ss >> lt.position.x;
                        }
                    }
                    ss >> lt.position.y >> lt.position.z
                       >> lt.color.x >> lt.color.y >> lt.color.z
                       >> lt.intensity >> lt.radius;
                    std::string typeStr;
                    if (ss >> typeStr) {
                        if (typeStr == "spot" || token == "light_spot") lt.type = MapLightType::Spot;
                        else lt.type = MapLightType::Point;
                    } else {
                        lt.type = (token == "light_spot") ? MapLightType::Spot : MapLightType::Point;
                    }
                    if (lt.intensity <= 0.0f) lt.intensity = 2.0f;
                    if (lt.radius <= 0.0f) lt.radius = 12.0f;
                    scene->lights.push_back(lt);
                    lastParsed = LastParsedEntity::Light;
                } else if (token == "sprite_2d" || token == "sprite") {
                    MapSprite2D sp;
                    int bb = 1;
                    ss >> sp.texturePath
                       >> sp.position.x >> sp.position.y >> sp.position.z
                       >> sp.size.x >> sp.size.y
                       >> sp.rotation >> sp.layer >> bb
                       >> sp.color.x >> sp.color.y >> sp.color.z >> sp.color.w
                       >> sp.uvMin.x >> sp.uvMin.y >> sp.uvMax.x >> sp.uvMax.y;
                    sp.billboard = (bb != 0);
                    if (sp.size.x <= 0.0f) sp.size.x = 1.0f;
                    if (sp.size.y <= 0.0f) sp.size.y = 1.0f;
                    scene->sprites.push_back(sp);
                    lastParsed = LastParsedEntity::Sprite;
                } else if (token == "ecs_node") {
                    std::string mPath, tPath;
                    Vec3 pos, rotDeg, scl, col;
                    ss >> mPath >> pos.x >> pos.y >> pos.z
                       >> rotDeg.x >> rotDeg.y >> rotDeg.z
                       >> scl.x >> scl.y >> scl.z
                       >> col.x >> col.y >> col.z;
                    if (ss >> tPath) {
                        if (tPath == "\"\"") tPath = "";
                    }
                    if (mPath == "\"\"") mPath = "";
                    auto ent = scene->CreateEntity("ecs_node", "Node");
                    ent->transform->position = pos;
                    ent->transform->rotation = Quat::fromEuler(
                        rotDeg.x * (3.14159265f / 180.0f),
                        rotDeg.y * (3.14159265f / 180.0f),
                        rotDeg.z * (3.14159265f / 180.0f)
                    );
                    ent->transform->scale = scl;
                    if (!mPath.empty() || !tPath.empty()) {
                        auto mc = ent->AddComponent<MeshComponent>(mPath, tPath);
                        mc->color = col;
                    }
                    ent->syncToSceneNode();
                    lastParsed = LastParsedEntity::None;
                } else if (token == "normal_map") {
                    std::string nPath;
                    if (ss >> nPath) {
                        if (nPath == "\"\"") nPath = "";
                        if (lastParsed == LastParsedEntity::Prop && !scene->props.empty()) {
                            scene->props.back().normalMapPath = nPath;
                        } else if (lastParsed == LastParsedEntity::Brush && !scene->brushes.empty()) {
                            scene->brushes.back().normalMapPath = nPath;
                        }
                    }
                } else if (token == "dialog_text") {
                    std::string rest;
                    std::getline(ss, rest);
                    size_t q1 = rest.find('"'), q2 = rest.rfind('"');
                    if (q1 != std::string::npos && q2 != std::string::npos && q2 > q1 && lastParsed == LastParsedEntity::Trigger && !scene->triggers.empty()) {
                        scene->triggers.back().dialogText = rest.substr(q1 + 1, q2 - q1 - 1);
                    }
                } else if (token == "tag") {
                    std::string rest;
                    std::getline(ss, rest);
                    size_t q1 = rest.find('"'), q2 = rest.rfind('"');
                    std::string t;
                    if (q1 != std::string::npos && q2 != std::string::npos && q2 > q1) t = rest.substr(q1 + 1, q2 - q1 - 1);
                    else { std::stringstream tss(rest); tss >> t; }
                    if (!t.empty() && !scene->entities.empty()) {
                        scene->entities.back()->tag = t;
                    }
                } else if (token == "script") {
                    std::string sPath;
                    if (ss >> sPath) {
                        if (sPath == "\"\"") sPath = "";
                        if (lastParsed == LastParsedEntity::Prop && !scene->props.empty()) scene->props.back().script = sPath;
                        else if (lastParsed == LastParsedEntity::Door && !scene->doors.empty()) scene->doors.back().script = sPath;
                        else if (lastParsed == LastParsedEntity::Trigger && !scene->triggers.empty()) scene->triggers.back().script = sPath;
                        else if (lastParsed == LastParsedEntity::Camera && !scene->cameras.empty()) scene->cameras.back().script = sPath;
                        else if (lastParsed == LastParsedEntity::Sprite && !scene->sprites.empty()) scene->sprites.back().script = sPath;
                    }
                } else if (token == "name") {
                    std::string rest;
                    std::getline(ss, rest);
                    size_t q1 = rest.find('"'), q2 = rest.rfind('"');
                    std::string n;
                    if (q1 != std::string::npos && q2 != std::string::npos && q2 > q1) n = rest.substr(q1 + 1, q2 - q1 - 1);
                    else { std::stringstream nss(rest); nss >> n; }
                    if (!n.empty()) {
                        if (lastParsed == LastParsedEntity::Prop && !scene->props.empty()) scene->props.back().name = n;
                        else if (lastParsed == LastParsedEntity::Door && !scene->doors.empty()) scene->doors.back().name = n;
                        else if (lastParsed == LastParsedEntity::Trigger && !scene->triggers.empty()) scene->triggers.back().name = n;
                        else if (lastParsed == LastParsedEntity::Waypoint && !scene->waypoints.empty()) scene->waypoints.back().name = n;
                        else if (lastParsed == LastParsedEntity::Camera && !scene->cameras.empty()) scene->cameras.back().name = n;
                        else if (lastParsed == LastParsedEntity::Sprite && !scene->sprites.empty()) scene->sprites.back().name = n;
                        else if (!scene->entities.empty()) scene->entities.back()->name = n;
                    }
                } else if (token == "activator") {
                    std::string act;
                    if (ss >> act && lastParsed == LastParsedEntity::Door && !scene->doors.empty()) {
                        scene->doors.back().activator = act;
                    }
                } else if (token == "keycard") {
                    std::string kc;
                    if (ss >> kc && lastParsed == LastParsedEntity::Door && !scene->doors.empty()) {
                        scene->doors.back().keycardRequired = kc;
                        scene->doors.back().requiresKeycard = true;
                    }
                } else if (token == "dependency") {
                    std::string dep;
                    if (ss >> dep && lastParsed == LastParsedEntity::Door && !scene->doors.empty()) {
                        scene->doors.back().dependency = dep;
                    }
                } else if (token == "locked") {
                    int lk = 0;
                    if (ss >> lk && lastParsed == LastParsedEntity::Door && !scene->doors.empty()) {
                        scene->doors.back().isLocked = (lk != 0);
                    }
                } else if (token == "auto_close") {
                    int ac = 1;
                    if (ss >> ac && lastParsed == LastParsedEntity::Door && !scene->doors.empty()) {
                        scene->doors.back().autoClose = (ac != 0);
                    }
                } else if (token == "target_entity") {
                    std::string te;
                    if (ss >> te && lastParsed == LastParsedEntity::Trigger && !scene->triggers.empty()) {
                        scene->triggers.back().targetEntity = te;
                    }
                } else if (token == "target_waypoint") {
                    std::string tw;
                    if (ss >> tw && lastParsed == LastParsedEntity::Trigger && !scene->triggers.empty()) {
                        scene->triggers.back().targetWaypoint = tw;
                    }
                } else if (token == "target_action") {
                    std::string ta;
                    if (ss >> ta && lastParsed == LastParsedEntity::Trigger && !scene->triggers.empty()) {
                        scene->triggers.back().targetAction = ta;
                    }
                } else if (token == "target_trigger") {
                    std::string tt;
                    if (ss >> tt && lastParsed == LastParsedEntity::Waypoint && !scene->waypoints.empty()) {
                        scene->waypoints.back().targetTrigger = tt;
                    }
                } else if (token == "trigger_action") {
                    std::string tra;
                    if (ss >> tra && lastParsed == LastParsedEntity::Waypoint && !scene->waypoints.empty()) {
                        scene->waypoints.back().triggerAction = tra;
                    }
                } else if (token == "rot" || token == "rotation") {
                    Vec3 r(0, 0, 0);
                    if (ss >> r.x >> r.y >> r.z) {
                        if (lastParsed == LastParsedEntity::Prop && !scene->props.empty()) scene->props.back().rotation = r;
                        else if (lastParsed == LastParsedEntity::Camera && !scene->cameras.empty()) scene->cameras.back().rotation = r;
                    }
                }
            }
        }

        if (!hasValidSceneStructure) {
            LabLog::error("Rejected invalid or empty scene file: " + filePath);
            return nullptr;
        }

        if (scene->spawnPoints.empty()) {
            MapSpawnPoint defaultSp;
            defaultSp.entityClass = "info_player_deathmatch";
            defaultSp.position = scene->spawn.position;
            defaultSp.yaw = scene->spawn.yaw;
            defaultSp.type = SpawnType::FFA;
            scene->spawnPoints.push_back(defaultSp);
        } else {
            scene->spawn.position = scene->spawnPoints[0].position;
            scene->spawn.yaw = scene->spawnPoints[0].yaw;
        }

        scene->synchronizeMapToEntities();

        LabLog::info("Loaded .LABSCENE: " + scene->metadata.name + " (" +
                     std::to_string(scene->spawnPoints.size()) + " spawns, " +
                     std::to_string(scene->weaponSpawners.size()) + " weapon spawners, " +
                     std::to_string(scene->lights.size()) + " lights, " +
                     std::to_string(scene->brushes.size()) + " static meshes, " +
                     std::to_string(scene->props.size()) + " props, " +
                     std::to_string(scene->doors.size()) + " doors, " +
                     std::to_string(scene->triggers.size()) + " triggers, " +
                     std::to_string(scene->waypoints.size()) + " waypoints)");
        return scene;
    }

    void LabScene::synchronizeMapToEntities() {
        // 1. Props -> Entities
        for (size_t i = 0; i < props.size(); ++i) {
            const auto& p = props[i];
            std::string eName = p.name.empty() ? ("prop_" + std::to_string(i)) : p.name;
            if (!FindByName(eName)) {
                auto ent = CreateEntity(eName, "Prop");
                ent->transform->position = p.position;
                ent->transform->rotation = Quat::fromEuler(
                    p.rotation.x * (3.14159265f / 180.0f),
                    p.rotation.y * (3.14159265f / 180.0f),
                    p.rotation.z * (3.14159265f / 180.0f)
                );
                ent->transform->scale = p.scale;
                auto meshComp = ent->AddComponent<MeshComponent>(p.modelPath, p.texturePath);
                meshComp->color = p.color;
                if (p.hasCloth) ent->AddComponent<ClothComponent>();
                if (p.hasWaterVolume) ent->AddComponent<WaterVolumeComponent>();
                if (p.hasForceField) ent->AddComponent<ForceFieldComponent>();
                if (p.hasVehicle) ent->AddComponent<VehicleComponent>();
                if (p.hasDecal) ent->AddComponent<DecalComponent>();
                if (p.hasPostProcess) ent->AddComponent<PostProcessComponent>();
                if (p.hasAudio) ent->AddComponent<AudioSourceComponent>();
                ent->syncToSceneNode();
            }
        }

        // 2. Cameras -> Entities
        for (size_t i = 0; i < cameras.size(); ++i) {
            const auto& c = cameras[i];
            std::string eName = c.name.empty() ? ("camera_" + std::to_string(i)) : c.name;
            if (!FindByName(eName)) {
                auto ent = CreateEntity(eName, c.active ? "MainCamera" : "Camera");
                ent->transform->position = c.position;
                auto camComp = ent->AddComponent<Camera>();
                camComp->fieldOfView = c.fov;
                camComp->enabled = c.active;
                ent->syncToSceneNode();
            }
        }

        // 3. Lights -> Entities
        for (size_t i = 0; i < lights.size(); ++i) {
            const auto& l = lights[i];
            std::string eName = l.name.empty() ? ("light_" + std::to_string(i)) : l.name;
            if (!FindByName(eName)) {
                auto ent = CreateEntity(eName, "Light");
                ent->transform->position = l.position;
                auto lComp = ent->AddComponent<LightComponent>();
                lComp->color = l.color;
                lComp->intensity = l.intensity;
                lComp->radius = l.radius;
                lComp->type = (l.type == MapLightType::Spot) ? LightType::Spot : LightType::Point;
                ent->syncToSceneNode();
            }
        }

        // 4. Doors -> Entities
        for (size_t i = 0; i < doors.size(); ++i) {
            const auto& d = doors[i];
            std::string eName = d.name.empty() ? ("door_" + std::to_string(i)) : d.name;
            if (!FindByName(eName)) {
                auto ent = CreateEntity(eName, "Door");
                ent->transform->position = d.position;
                ent->transform->scale = d.size;
                ent->syncToSceneNode();
            }
        }

        syncSceneGraph();
    }

    void LabScene::synchronizeEntitiesToMap() {
        for (auto& ent : entities) {
            if (!ent || !ent->transform) continue;
            ent->syncFromSceneNode();
            auto handle = findEntityByName(ent->name);
            if (handle.isValid()) {
                if (Vec3* p = handle.getPosition()) *p = ent->transform->position;
            }
        }
    }

} // namespace Lab
