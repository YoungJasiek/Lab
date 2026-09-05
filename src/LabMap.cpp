#include "LabMap.h"
#include "LabCore.h"
#include <fstream>
#include <sstream>
#include <filesystem>
#include <iostream>

namespace Lab {

    static std::string resolveMapPath(const std::string& path) {
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

    std::unique_ptr<LabMap> LabMap::loadFromFile(const std::string& filePath) {
        std::string resolved = resolveMapPath(filePath);
        std::ifstream file(resolved);
        if (!file.is_open()) {
            LabLog::error("Failed to open map file: " + filePath + " (resolved: " + resolved + ")");
            return nullptr;
        }

        auto map = std::make_unique<LabMap>();
        std::string line;
        std::string currentSection = "";

        while (std::getline(file, line)) {
            // Trim and skip empty lines or comments
            size_t first = line.find_first_not_of(" \t\r\n");
            if (first == std::string::npos) continue;
            if (line[first] == '#' || (line[first] == '/' && line.length() > first + 1 && line[first + 1] == '/')) continue;

            std::stringstream ss(line.substr(first));
            std::string token;
            ss >> token;

            if (token == "LABMAP_VERSION") {
                int ver = 1;
                ss >> ver;
            } else if (token == "METADATA") {
                currentSection = "METADATA";
            } else if (token == "END_METADATA") {
                currentSection = "";
            } else if (token == "ENTITIES") {
                currentSection = "ENTITIES";
            } else if (token == "END_ENTITIES") {
                currentSection = "";
            } else if (currentSection == "METADATA") {
                if (token == "name") {
                    std::string rest;
                    std::getline(ss, rest);
                    size_t q1 = rest.find('"');
                    size_t q2 = rest.rfind('"');
                    if (q1 != std::string::npos && q2 != std::string::npos && q2 > q1) {
                        map->metadata.name = rest.substr(q1 + 1, q2 - q1 - 1);
                    } else {
                        map->metadata.name = rest;
                    }
                } else if (token == "ambient") {
                    ss >> map->metadata.ambientColor.x >> map->metadata.ambientColor.y >> map->metadata.ambientColor.z;
                } else if (token == "sun_dir") {
                    ss >> map->metadata.sunDir.x >> map->metadata.sunDir.y >> map->metadata.sunDir.z;
                } else if (token == "sun_color") {
                    ss >> map->metadata.sunColor.x >> map->metadata.sunColor.y >> map->metadata.sunColor.z;
                }
            } else if (currentSection == "ENTITIES") {
                if (token == "spawn") {
                    ss >> map->spawn.position.x >> map->spawn.position.y >> map->spawn.position.z >> map->spawn.yaw;
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
                    map->spawnPoints.push_back(sp);
                } else if (token == "brush") {
                    MapBrush b;
                    ss >> b.type >> b.position.x >> b.position.y >> b.position.z
                       >> b.size.x >> b.size.y >> b.size.z
                       >> b.color.x >> b.color.y >> b.color.z;
                    if (ss >> b.texturePath) {
                        if (b.texturePath == "\"\"") b.texturePath = "";
                    }
                    if (ss >> b.uvScale.x >> b.uvScale.y) {
                        ss >> b.uvMode;
                    }
                    map->brushes.push_back(b);
                } else if (token == "prop") {
                    MapProp p;
                    ss >> p.modelPath >> p.position.x >> p.position.y >> p.position.z
                       >> p.rotation.x >> p.rotation.y >> p.rotation.z
                       >> p.scale.x >> p.scale.y >> p.scale.z
                       >> p.color.x >> p.color.y >> p.color.z;
                    if (ss >> p.texturePath) {
                        if (p.texturePath == "\"\"") p.texturePath = "";
                    }
                    map->props.push_back(p);
                } else if (token == "door") {
                    MapDoor d;
                    ss >> d.name >> d.position.x >> d.position.y >> d.position.z
                       >> d.size.x >> d.size.y >> d.size.z
                       >> d.openOffset.x >> d.openOffset.y >> d.openOffset.z
                       >> d.color.x >> d.color.y >> d.color.z
                       >> d.openSpeed >> d.triggerRadius;
                    map->doors.push_back(d);
                } else if (token == "weapon_spawner") {
                    MapWeaponSpawner ws;
                    std::string wepToken;
                    ss >> wepToken >> ws.position.x >> ws.position.y >> ws.position.z >> ws.yaw >> ws.respawnTime;
                    if (wepToken == "pipe" || wepToken == "0") ws.weaponId = 0;
                    else if (wepToken == "pistol" || wepToken == "1") ws.weaponId = 1;
                    else if (wepToken == "shotgun" || wepToken == "2") ws.weaponId = 2;
                    else if (wepToken == "m4a4s" || wepToken == "3") ws.weaponId = 3;
                    else if (wepToken == "sg553" || wepToken == "4") ws.weaponId = 4;
                    else if (wepToken == "minigun" || wepToken == "5") ws.weaponId = 5;
                    else if (wepToken == "plasma" || wepToken == "6") ws.weaponId = 6;
                    else if (wepToken == "railgun" || wepToken == "7") ws.weaponId = 7;
                    else if (wepToken == "rpg" || wepToken == "8") ws.weaponId = 8;
                    else {
                        try { ws.weaponId = std::stoi(wepToken); } catch (...) { ws.weaponId = 2; }
                    }
                    if (ws.respawnTime <= 0.0f) ws.respawnTime = 60.0f;
                    map->weaponSpawners.push_back(ws);
                }
            }
        }

        if (map->spawnPoints.empty()) {
            MapSpawnPoint defaultSp;
            defaultSp.entityClass = "info_player_deathmatch";
            defaultSp.position = map->spawn.position;
            defaultSp.yaw = map->spawn.yaw;
            defaultSp.type = SpawnType::FFA;
            map->spawnPoints.push_back(defaultSp);
        } else {
            map->spawn.position = map->spawnPoints[0].position;
            map->spawn.yaw = map->spawnPoints[0].yaw;
        }

        LabLog::info("Loaded .LABMAP: " + map->metadata.name + " (" +
                     std::to_string(map->spawnPoints.size()) + " spawns, " +
                     std::to_string(map->weaponSpawners.size()) + " weapon spawners, " +
                     std::to_string(map->brushes.size()) + " brushes, " +
                     std::to_string(map->props.size()) + " props, " +
                     std::to_string(map->doors.size()) + " doors)");
        return map;
    }

    std::vector<MapSpawnPoint> LabMap::getSpawnsForTeam(GameMode mode, int team) const {
        std::vector<MapSpawnPoint> result;
        if (mode == GameMode::TDM) {
            // Team 1 = Blue / Alpha, Team 0 = Red / Beta
            SpawnType targetType = (team == 1) ? SpawnType::TeamAlpha : SpawnType::TeamBeta;
            for (const auto& sp : spawnPoints) {
                if (sp.type == targetType) result.push_back(sp);
            }
            // Fallback to FFA spawns if this team has none defined
            if (result.empty()) {
                for (const auto& sp : spawnPoints) {
                    if (sp.type == SpawnType::FFA) result.push_back(sp);
                }
            }
        } else {
            // FFA / DM: return neutral FFA spawns
            for (const auto& sp : spawnPoints) {
                if (sp.type == SpawnType::FFA) result.push_back(sp);
            }
            // Fallback to all available spawns if none tagged explicitly FFA
            if (result.empty()) {
                result = spawnPoints;
            }
        }
        if (result.empty()) {
            MapSpawnPoint fallback;
            fallback.position = spawn.position;
            fallback.yaw = spawn.yaw;
            fallback.type = SpawnType::FFA;
            result.push_back(fallback);
        }
        return result;
    }

    MapSpawnPoint LabMap::selectBestSpawn(GameMode mode, int team, const std::vector<Vec3>& enemyPositions) const {
        auto candidates = getSpawnsForTeam(mode, team);
        if (candidates.empty()) {
            MapSpawnPoint fallback;
            fallback.position = spawn.position;
            fallback.yaw = spawn.yaw;
            return fallback;
        }
        if (candidates.size() == 1 || enemyPositions.empty()) {
            int idx = rand() % (int)candidates.size();
            return candidates[idx];
        }

        // Smart anti-spawncamp: pick spawn with maximum minimum distance to all active enemies
        float bestMinDistSq = -1.0f;
        int bestIdx = 0;

        for (int i = 0; i < (int)candidates.size(); ++i) {
            float minDistSq = 1e9f;
            for (const auto& enemy : enemyPositions) {
                float dSq = (candidates[i].position - enemy).lengthSq();
                if (dSq < minDistSq) {
                    minDistSq = dSq;
                }
            }
            if (minDistSq > bestMinDistSq) {
                bestMinDistSq = minDistSq;
                bestIdx = i;
            }
        }

        return candidates[bestIdx];
    }

    bool LabMap::saveToFile(const std::string& filePath) const {
        std::ofstream file(filePath);
        if (!file.is_open()) {
            LabLog::error("Failed to save map to: " + filePath);
            return false;
        }

        file << "LABMAP_VERSION 1\n\n";
        file << "METADATA\n";
        file << "    name \"" << metadata.name << "\"\n";
        file << "    ambient " << metadata.ambientColor.x << " " << metadata.ambientColor.y << " " << metadata.ambientColor.z << "\n";
        file << "    sun_dir " << metadata.sunDir.x << " " << metadata.sunDir.y << " " << metadata.sunDir.z << "\n";
        file << "    sun_color " << metadata.sunColor.x << " " << metadata.sunColor.y << " " << metadata.sunColor.z << "\n";
        file << "END_METADATA\n\n";

        file << "ENTITIES\n";
        file << "    spawn " << spawn.position.x << " " << spawn.position.y << " " << spawn.position.z << " " << spawn.yaw << "\n";

        for (const auto& sp : spawnPoints) {
            std::string typeStr = (sp.type == SpawnType::TeamAlpha) ? "team_alpha" :
                                  (sp.type == SpawnType::TeamBeta)  ? "team_beta"  : "ffa";
            file << "    spawn_point " << sp.entityClass << " "
                 << sp.position.x << " " << sp.position.y << " " << sp.position.z << " "
                 << sp.yaw << " " << typeStr << "\n";
        }

        for (const auto& ws : weaponSpawners) {
            file << "    weapon_spawner " << ws.weaponId << " "
                 << ws.position.x << " " << ws.position.y << " " << ws.position.z << " "
                 << ws.yaw << " " << ws.respawnTime << "\n";
        }

        for (const auto& b : brushes) {
            file << "    brush " << b.type << " "
                 << b.position.x << " " << b.position.y << " " << b.position.z << " "
                 << b.size.x << " " << b.size.y << " " << b.size.z << " "
                 << b.color.x << " " << b.color.y << " " << b.color.z << " "
                 << (b.texturePath.empty() ? "\"\"" : b.texturePath) << " "
                 << b.uvScale.x << " " << b.uvScale.y << " " << b.uvMode << "\n";
        }

        for (const auto& p : props) {
            file << "    prop " << p.modelPath << " "
                 << p.position.x << " " << p.position.y << " " << p.position.z << " "
                 << p.rotation.x << " " << p.rotation.y << " " << p.rotation.z << " "
                 << p.scale.x << " " << p.scale.y << " " << p.scale.z << " "
                 << p.color.x << " " << p.color.y << " " << p.color.z << " "
                 << (p.texturePath.empty() ? "\"\"" : p.texturePath) << "\n";
        }

        for (const auto& d : doors) {
            file << "    door " << d.name << " "
                 << d.position.x << " " << d.position.y << " " << d.position.z << " "
                 << d.size.x << " " << d.size.y << " " << d.size.z << " "
                 << d.openOffset.x << " " << d.openOffset.y << " " << d.openOffset.z << " "
                 << d.color.x << " " << d.color.y << " " << d.color.z << " "
                 << d.openSpeed << " " << d.triggerRadius << "\n";
        }

        file << "END_ENTITIES\n";
        LabLog::info("Successfully saved map to: " + filePath);
        return true;
    }

} // namespace Lab
