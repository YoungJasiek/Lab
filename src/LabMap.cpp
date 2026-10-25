#include "LabPickups.h"
#include "LabMap.h"
#include "LabScene.h"
#include "LabCore.h"
#include "LabPak.h"
#include "LabVFS.h"
#include <fstream>
#include <sstream>
#include <filesystem>
#include <iostream>

namespace Lab {

    std::string MapWeaponSpawner::getWeaponName() const {
        if (!prefabRef.empty()) {
            const auto* def = FindSpawnerItemDef(prefabRef);
            if (def) return def->displayName;
            return prefabRef;
        }
        const auto* def = FindSpawnerItemDefByWeaponId(weaponId);
        if (def) return def->displayName;
        return "Item_" + std::to_string(weaponId);
    }


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

        // Fallback between .lmap, .labmap, and .labscene if only one exists on disk
        std::filesystem::path p(path);
        std::string ext = p.extension().string();
        for (auto& c : ext) c = static_cast<char>(::tolower(static_cast<unsigned char>(c)));
        if (ext == ".lmap" || ext == ".labscene" || ext == ".labmap") {
            const char* altExts[] = { ".lmap", ".labmap", ".labscene" };
            for (const char* ae : altExts) {
                std::filesystem::path altPath = p;
                altPath.replace_extension(ae);
                std::string altFname = altPath.filename().string();
                if (std::filesystem::exists(altPath)) return altPath.string();
                if (std::filesystem::exists("assets/maps/" + altFname)) return "assets/maps/" + altFname;
                if (std::filesystem::exists("../assets/maps/" + altFname)) return "../assets/maps/" + altFname;
                if (std::filesystem::exists("../../assets/maps/" + altFname)) return "../../assets/maps/" + altFname;
            }
        }
        return path;
    }

    std::unique_ptr<LabMap> LabMap::loadFromFile(const std::string& filePath) {
        if (filePath.empty()) return nullptr;
        std::string resolved = resolveMapPath(filePath);

        std::string ext = std::filesystem::path(resolved).extension().string();
        for (auto& c : ext) c = static_cast<char>(::tolower(static_cast<unsigned char>(c)));
        if (ext != ".lmap" && ext != ".labscene" && ext != ".labmap" && ext != ".backup") {
            LabLog::error("Rejected non-map file extension: " + filePath);
            return nullptr;
        }
        std::ifstream file(resolved, std::ios::binary);
        if (!file.is_open()) {
            LabLog::error("Failed to open map file: " + filePath + " (resolved: " + resolved + ")");
            return nullptr;
        }

        BakedMapHeader bakedHeader{};
        bool isBaked = false;
        std::string sceneData;

        if (file.read(reinterpret_cast<char*>(&bakedHeader), sizeof(BakedMapHeader))) {
            if (bakedHeader.magic == 0x50414D4C && bakedHeader.version == 1) { // "LMAP"
                isBaked = true;
                file.seekg(static_cast<std::streamoff>(bakedHeader.sceneOffset), std::ios::beg);
                std::vector<uint8_t> sceneBytes(bakedHeader.sceneLength);
                if (file.read(reinterpret_cast<char*>(sceneBytes.data()), static_cast<std::streamsize>(bakedHeader.sceneLength))) {
                    if (bakedHeader.flags & BAKED_MAP_ENCRYPTED) {
                        LabPak::cryptBuffer(sceneBytes, bakedHeader.sceneOffset);
                    }
                    if (bakedHeader.flags & BAKED_MAP_COMPRESSED) {
                        std::vector<uint8_t> decompressed;
                        if (!LabPak::decompressBuffer(sceneBytes, decompressed)) {
                            LabLog::error("[LabMap] Failed to decompress baked .lmap: " + resolved);
                            return nullptr;
                        }
                        sceneBytes = std::move(decompressed);
                    }
                    uint32_t computed = LabPak::calculateCRC32(sceneBytes);
                    if (bakedHeader.checksum != 0 && computed != bakedHeader.checksum) {
                        LabLog::error("[LabMap] Checksum mismatch in baked .lmap: " + resolved);
                        return nullptr;
                    }
                    sceneData.assign(reinterpret_cast<const char*>(sceneBytes.data()), sceneBytes.size());
                }

                // Handle embedded asset package if present
                if (bakedHeader.embeddedPakLength > 0 && (bakedHeader.flags & BAKED_MAP_EMBEDDED_ASSETS)) {
                    file.seekg(static_cast<std::streamoff>(bakedHeader.embeddedPakOffset), std::ios::beg);
                    std::vector<uint8_t> pakBytes(bakedHeader.embeddedPakLength);
                    if (file.read(reinterpret_cast<char*>(pakBytes.data()), static_cast<std::streamsize>(bakedHeader.embeddedPakLength))) {
                        std::filesystem::path tempPak = std::filesystem::temp_directory_path() / ("map_embedded_" + std::filesystem::path(resolved).stem().string() + ".lpak");
                        std::ofstream pakOut(tempPak, std::ios::binary);
                        if (pakOut.is_open()) {
                            pakOut.write(reinterpret_cast<const char*>(pakBytes.data()), static_cast<std::streamsize>(pakBytes.size()));
                            pakOut.close();
                            VFS::get().mountPak(tempPak);
                        }
                    }
                }
            }
        }

        if (!isBaked) {
            if (ext == ".labscene") {
                return LabScene::loadSceneFromFile(resolved);
            }
            file.clear();
            file.seekg(0, std::ios::beg);
            sceneData.assign((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        }

        {
            std::istringstream peek(sceneData);
            std::string peekLine;
            while (std::getline(peek, peekLine)) {
                size_t pFirst = peekLine.find_first_not_of(" \t\r\n");
                if (pFirst == std::string::npos || peekLine[pFirst] == '#') continue;
                if (peekLine.find("LABSCENE_VERSION") != std::string::npos) {
                    return LabScene::loadSceneFromString(sceneData, resolved);
                }
                break;
            }
        }

        std::istringstream stream(sceneData);
        auto map = std::make_unique<LabMap>();
        std::string line;
        std::string currentSection = "";
        bool hasValidMapStructure = false;
        enum class LastParsedEntity { None, Brush, Prop, Door, Camera, Sprite, Light, Trigger, Waypoint };
        LastParsedEntity lastParsed = LastParsedEntity::None;

        while (std::getline(stream, line)) {
            // Trim and skip empty lines or comments
            size_t first = line.find_first_not_of(" \t\r\n");
            if (first == std::string::npos) continue;
            if (line[first] == '#' || (line[first] == '/' && line.length() > first + 1 && line[first + 1] == '/')) continue;

            std::stringstream ss(line.substr(first));
            std::string token;
            ss >> token;

            if (token == "LABMAP_VERSION" || token == "LMAP_VERSION") {
                int ver = 1;
                ss >> ver;
                hasValidMapStructure = true;
            } else if (token == "METADATA") {
                currentSection = "METADATA";
                hasValidMapStructure = true;
            } else if (token == "END_METADATA") {
                currentSection = "";
            } else if (token == "ENTITIES") {
                currentSection = "ENTITIES";
                hasValidMapStructure = true;
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
                } else if (token == "cubemap") {
                    ss >> map->metadata.cubemapPath;
                    if (map->metadata.cubemapPath == "\"\"") map->metadata.cubemapPath = "";
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
                    if (ss >> b.rotation.x >> b.rotation.y >> b.rotation.z) {
                        // Rotation inline
                    }
                    map->brushes.push_back(b);
                    lastParsed = LastParsedEntity::Brush;
                } else if (token == "poly_brush") {
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
                        for (size_t vi = 0; vi < vCount && std::getline(stream, line); ++vi) {
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
                        if (std::getline(stream, line)) {
                            std::stringstream iss(line);
                            std::string idxTok;
                            iss >> idxTok;
                            for (size_t ii = 0; ii < iCount; ++ii) {
                                unsigned int idxVal = 0;
                                if (iss >> idxVal) b.customIndices.push_back(idxVal);
                            }
                        }
                    }
                    map->brushes.push_back(b);
                    lastParsed = LastParsedEntity::Brush;
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
                    lastParsed = LastParsedEntity::Prop;
                } else if (token == "door") {
                    MapDoor d;
                    ss >> d.name >> d.position.x >> d.position.y >> d.position.z
                       >> d.size.x >> d.size.y >> d.size.z
                       >> d.openOffset.x >> d.openOffset.y >> d.openOffset.z
                       >> d.color.x >> d.color.y >> d.color.z
                       >> d.openSpeed >> d.triggerRadius;
                    map->doors.push_back(d);
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
                    map->triggers.push_back(tr);
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
                    map->waypoints.push_back(wp);
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
                    map->cameras.push_back(c);
                    lastParsed = LastParsedEntity::Camera;
                } else if (token == "normal_map") {
                    std::string nPath;
                    if (ss >> nPath) {
                        if (nPath == "\"\"") nPath = "";
                        if (lastParsed == LastParsedEntity::Brush && !map->brushes.empty()) {
                            map->brushes.back().normalMapPath = nPath;
                        } else if (lastParsed == LastParsedEntity::Prop && !map->props.empty()) {
                            map->props.back().normalMapPath = nPath;
                        }
                    }
                } else if (token == "dialog_text") {
                    std::string rest;
                    std::getline(ss, rest);
                    size_t q1 = rest.find('"'), q2 = rest.rfind('"');
                    if (q1 != std::string::npos && q2 != std::string::npos && q2 > q1 && lastParsed == LastParsedEntity::Trigger && !map->triggers.empty()) {
                        map->triggers.back().dialogText = rest.substr(q1 + 1, q2 - q1 - 1);
                    }
                } else if (token == "script") {
                    std::string sPath;
                    if (ss >> sPath) {
                        if (sPath == "\"\"") sPath = "";
                        if (lastParsed == LastParsedEntity::Brush && !map->brushes.empty()) {
                            map->brushes.back().script = sPath;
                        } else if (lastParsed == LastParsedEntity::Prop && !map->props.empty()) {
                            map->props.back().script = sPath;
                        } else if (lastParsed == LastParsedEntity::Door && !map->doors.empty()) {
                            map->doors.back().script = sPath;
                        } else if (lastParsed == LastParsedEntity::Trigger && !map->triggers.empty()) {
                            map->triggers.back().script = sPath;
                        } else if (lastParsed == LastParsedEntity::Camera && !map->cameras.empty()) {
                            map->cameras.back().script = sPath;
                        } else if (lastParsed == LastParsedEntity::Sprite && !map->sprites.empty()) {
                            map->sprites.back().script = sPath;
                        }
                    }
                } else if (token == "rot" || token == "rotation") {
                    Vec3 r(0, 0, 0);
                    if (ss >> r.x >> r.y >> r.z) {
                        if (lastParsed == LastParsedEntity::Brush && !map->brushes.empty()) {
                            map->brushes.back().rotation = r;
                        } else if (lastParsed == LastParsedEntity::Prop && !map->props.empty()) {
                            map->props.back().rotation = r;
                        } else if (lastParsed == LastParsedEntity::Camera && !map->cameras.empty()) {
                            map->cameras.back().rotation = r;
                        }
                    }
                } else if (token == "name") {
                    std::string rest;
                    std::getline(ss, rest);
                    size_t q1 = rest.find('"');
                    size_t q2 = rest.rfind('"');
                    std::string n;
                    if (q1 != std::string::npos && q2 != std::string::npos && q2 > q1) {
                        n = rest.substr(q1 + 1, q2 - q1 - 1);
                    } else {
                        std::stringstream nss(rest);
                        nss >> n;
                    }
                    if (!n.empty()) {
                        if (lastParsed == LastParsedEntity::Brush && !map->brushes.empty()) {
                            map->brushes.back().name = n;
                        } else if (lastParsed == LastParsedEntity::Prop && !map->props.empty()) {
                            map->props.back().name = n;
                        } else if (lastParsed == LastParsedEntity::Door && !map->doors.empty()) {
                            map->doors.back().name = n;
                        } else if (lastParsed == LastParsedEntity::Trigger && !map->triggers.empty()) {
                            map->triggers.back().name = n;
                        } else if (lastParsed == LastParsedEntity::Waypoint && !map->waypoints.empty()) {
                            map->waypoints.back().name = n;
                        } else if (lastParsed == LastParsedEntity::Camera && !map->cameras.empty()) {
                            map->cameras.back().name = n;
                        } else if (lastParsed == LastParsedEntity::Sprite && !map->sprites.empty()) {
                            map->sprites.back().name = n;
                        }
                    }
                } else if (token == "activator") {
                    std::string act;
                    if (ss >> act && lastParsed == LastParsedEntity::Door && !map->doors.empty()) {
                        map->doors.back().activator = act;
                    }
                } else if (token == "keycard") {
                    std::string kc;
                    if (ss >> kc && lastParsed == LastParsedEntity::Door && !map->doors.empty()) {
                        map->doors.back().keycardRequired = kc;
                        map->doors.back().requiresKeycard = true;
                    }
                } else if (token == "dependency") {
                    std::string dep;
                    if (ss >> dep && lastParsed == LastParsedEntity::Door && !map->doors.empty()) {
                        map->doors.back().dependency = dep;
                    }
                } else if (token == "locked") {
                    int lk = 0;
                    if (ss >> lk && lastParsed == LastParsedEntity::Door && !map->doors.empty()) {
                        map->doors.back().isLocked = (lk != 0);
                    }
                } else if (token == "auto_close") {
                    int ac = 1;
                    if (ss >> ac && lastParsed == LastParsedEntity::Door && !map->doors.empty()) {
                        map->doors.back().autoClose = (ac != 0);
                    }
                } else if (token == "target_entity") {
                    std::string te;
                    if (ss >> te && lastParsed == LastParsedEntity::Trigger && !map->triggers.empty()) {
                        map->triggers.back().targetEntity = te;
                    }
                } else if (token == "target_waypoint") {
                    std::string tw;
                    if (ss >> tw && lastParsed == LastParsedEntity::Trigger && !map->triggers.empty()) {
                        map->triggers.back().targetWaypoint = tw;
                    }
                } else if (token == "target_action") {
                    std::string ta;
                    if (ss >> ta && lastParsed == LastParsedEntity::Trigger && !map->triggers.empty()) {
                        map->triggers.back().targetAction = ta;
                    }
                } else if (token == "target_trigger") {
                    std::string tt;
                    if (ss >> tt && lastParsed == LastParsedEntity::Waypoint && !map->waypoints.empty()) {
                        map->waypoints.back().targetTrigger = tt;
                    }
                } else if (token == "trigger_action") {
                    std::string tra;
                    if (ss >> tra && lastParsed == LastParsedEntity::Waypoint && !map->waypoints.empty()) {
                        map->waypoints.back().triggerAction = tra;
                    }
                } else if (token == "weapon_spawner") {
                    MapWeaponSpawner ws;
                    std::string wepToken;
                    ss >> wepToken >> ws.position.x >> ws.position.y >> ws.position.z >> ws.yaw >> ws.respawnTime;
                    ws.prefabRef = wepToken;
                    const auto* def = FindSpawnerItemDef(wepToken);
                    if (def) {
                        ws.weaponId = def->weaponId;
                    } else {
                        if (wepToken == "pipe" || wepToken == "0") { ws.weaponId = 0; ws.prefabRef = "weapon_pipe"; }
                        else if (wepToken == "pistol" || wepToken == "1") { ws.weaponId = 1; ws.prefabRef = "weapon_pistol"; }
                        else if (wepToken == "shotgun" || wepToken == "2") { ws.weaponId = 2; ws.prefabRef = "weapon_shotgun"; }
                        else if (wepToken == "m4a4s" || wepToken == "3") { ws.weaponId = 3; ws.prefabRef = "weapon_m4a4s"; }
                        else if (wepToken == "sg553" || wepToken == "4") { ws.weaponId = 4; ws.prefabRef = "weapon_sg553"; }
                        else if (wepToken == "minigun" || wepToken == "5") { ws.weaponId = 5; ws.prefabRef = "weapon_minigun"; }
                        else if (wepToken == "plasma" || wepToken == "6") { ws.weaponId = 6; ws.prefabRef = "weapon_plasma"; }
                        else if (wepToken == "railgun" || wepToken == "7") { ws.weaponId = 7; ws.prefabRef = "weapon_railgun"; }
                        else if (wepToken == "rpg" || wepToken == "8") { ws.weaponId = 8; ws.prefabRef = "weapon_rpg"; }
                        else {
                            try { ws.weaponId = std::stoi(wepToken); } catch (...) { ws.weaponId = 2; }
                        }
                    }
                    if (ws.respawnTime <= 0.0f) ws.respawnTime = 60.0f;
                    map->weaponSpawners.push_back(ws);
                    lastParsed = LastParsedEntity::None;
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
                    map->lights.push_back(lt);
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
                    map->sprites.push_back(sp);
                    lastParsed = LastParsedEntity::Sprite;
                }
            }
        }

        if (!hasValidMapStructure) {
            LabLog::error("Rejected invalid or empty map file: " + filePath);
            return nullptr;
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
                     std::to_string(map->lights.size()) + " lights, " +
                     std::to_string(map->brushes.size()) + " brushes, " +
                     std::to_string(map->props.size()) + " props, " +
                     std::to_string(map->doors.size()) + " doors, " +
                     std::to_string(map->triggers.size()) + " triggers, " +
                     std::to_string(map->waypoints.size()) + " waypoints)");
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

    static void writeMapEntities(const LabMap& map, std::ostream& out) {
        out << "METADATA\n";
        out << "    name \"" << map.metadata.name << "\"\n";
        out << "    ambient " << map.metadata.ambientColor.x << " " << map.metadata.ambientColor.y << " " << map.metadata.ambientColor.z << "\n";
        out << "    sun_dir " << map.metadata.sunDir.x << " " << map.metadata.sunDir.y << " " << map.metadata.sunDir.z << "\n";
        out << "    sun_color " << map.metadata.sunColor.x << " " << map.metadata.sunColor.y << " " << map.metadata.sunColor.z << "\n";
        if (!map.metadata.cubemapPath.empty()) {
            out << "    cubemap " << map.metadata.cubemapPath << "\n";
        }
        out << "END_METADATA\n\n";

        out << "ENTITIES\n";
        out << "    spawn " << map.spawn.position.x << " " << map.spawn.position.y << " " << map.spawn.position.z << " " << map.spawn.yaw << "\n";

        for (const auto& sp : map.spawnPoints) {
            std::string typeStr = (sp.type == SpawnType::TeamAlpha) ? "team_alpha" :
                                  (sp.type == SpawnType::TeamBeta)  ? "team_beta"  : "ffa";
            out << "    spawn_point " << sp.entityClass << " "
                 << sp.position.x << " " << sp.position.y << " " << sp.position.z << " "
                 << sp.yaw << " " << typeStr << "\n";
        }

        for (const auto& ws : map.weaponSpawners) {
            out << "    weapon_spawner " << ws.weaponId << " "
                 << ws.position.x << " " << ws.position.y << " " << ws.position.z << " "
                 << ws.yaw << " " << ws.respawnTime << "\n";
        }

        for (const auto& c : map.cameras) {
            out << "    camera " << (c.name.empty() ? "camera_main" : c.name) << " "
                 << c.position.x << " " << c.position.y << " " << c.position.z << " "
                 << c.rotation.x << " " << c.rotation.y << " " << c.rotation.z << " "
                 << c.fov << " " << (c.follow_target.empty() ? "\"\"" : c.follow_target) << " "
                 << c.tpp_offset.x << " " << c.tpp_offset.y << " " << c.tpp_offset.z << " "
                 << c.collision_radius << " " << (c.active ? 1 : 0) << "\n";
            if (!c.script.empty()) {
                out << "        script " << c.script << "\n";
            }
        }

        for (const auto& b : map.brushes) {
            if (b.type == "poly" && !b.customVertices.empty()) {
                out << "    poly_brush "
                     << b.position.x << " " << b.position.y << " " << b.position.z << " "
                     << b.size.x << " " << b.size.y << " " << b.size.z << " "
                     << b.color.x << " " << b.color.y << " " << b.color.z << " "
                     << (b.texturePath.empty() ? "\"\"" : b.texturePath) << " "
                     << b.uvScale.x << " " << b.uvScale.y << " " << b.uvMode << " "
                     << b.customVertices.size() << " " << b.customIndices.size() << "\n";
                for (const auto& v : b.customVertices) {
                    out << "        v " << v.position.x << " " << v.position.y << " " << v.position.z << " "
                         << v.normal.x << " " << v.normal.y << " " << v.normal.z << " "
                         << v.texCoords.x << " " << v.texCoords.y << "\n";
                }
                out << "        idx";
                for (unsigned int idxVal : b.customIndices) {
                    out << " " << idxVal;
                }
                out << "\n";
                if (b.rotation.x != 0.0f || b.rotation.y != 0.0f || b.rotation.z != 0.0f) {
                    out << "        rot " << b.rotation.x << " " << b.rotation.y << " " << b.rotation.z << "\n";
                }
            } else {
                out << "    brush " << b.type << " "
                     << b.position.x << " " << b.position.y << " " << b.position.z << " "
                     << b.size.x << " " << b.size.y << " " << b.size.z << " "
                     << b.color.x << " " << b.color.y << " " << b.color.z << " "
                     << (b.texturePath.empty() ? "\"\"" : b.texturePath) << " "
                     << b.uvScale.x << " " << b.uvScale.y << " " << b.uvMode;
                if (b.rotation.x != 0.0f || b.rotation.y != 0.0f || b.rotation.z != 0.0f) {
                    out << " " << b.rotation.x << " " << b.rotation.y << " " << b.rotation.z;
                }
                out << "\n";
            }
            if (!b.name.empty()) {
                out << "        name \"" << b.name << "\"\n";
            }
            if (!b.normalMapPath.empty()) {
                out << "        normal_map " << b.normalMapPath << "\n";
            }
            if (!b.script.empty()) {
                out << "        script " << b.script << "\n";
            }
        }

        for (const auto& p : map.props) {
            out << "    prop " << p.modelPath << " "
                 << p.position.x << " " << p.position.y << " " << p.position.z << " "
                 << p.rotation.x << " " << p.rotation.y << " " << p.rotation.z << " "
                 << p.scale.x << " " << p.scale.y << " " << p.scale.z << " "
                 << p.color.x << " " << p.color.y << " " << p.color.z << " "
                 << (p.texturePath.empty() ? "\"\"" : p.texturePath) << "\n";
            if (!p.name.empty()) {
                out << "        name \"" << p.name << "\"\n";
            }
            if (!p.normalMapPath.empty()) {
                out << "        normal_map " << p.normalMapPath << "\n";
            }
            if (!p.script.empty()) {
                out << "        script " << p.script << "\n";
            }
        }

        for (const auto& d : map.doors) {
            out << "    door " << d.name << " "
                 << d.position.x << " " << d.position.y << " " << d.position.z << " "
                 << d.size.x << " " << d.size.y << " " << d.size.z << " "
                 << d.openOffset.x << " " << d.openOffset.y << " " << d.openOffset.z << " "
                 << d.color.x << " " << d.color.y << " " << d.color.z << " "
                 << d.openSpeed << " " << d.triggerRadius << "\n";
            if (!d.script.empty()) {
                out << "        script " << d.script << "\n";
            }
            if (!d.activator.empty() && d.activator != "proximity") {
                out << "        activator " << d.activator << "\n";
            }
            if (d.requiresKeycard) {
                out << "        keycard " << (d.keycardRequired.empty() ? "keycard_l1" : d.keycardRequired) << "\n";
            }
            if (!d.dependency.empty()) {
                out << "        dependency " << d.dependency << "\n";
            }
            if (d.isLocked) {
                out << "        locked 1\n";
            }
            if (!d.autoClose) {
                out << "        auto_close 0\n";
            }
        }

        for (const auto& tr : map.triggers) {
            out << "    trigger_volume " << (tr.name.empty() ? "trigger_story" : tr.name) << " "
                 << tr.position.x << " " << tr.position.y << " " << tr.position.z << " "
                 << tr.size.x << " " << tr.size.y << " " << tr.size.z << " "
                 << (tr.eventType.empty() ? "story_dialog" : tr.eventType) << " "
                 << (tr.targetBot.empty() ? "\"\"" : tr.targetBot) << " "
                 << (tr.once ? 1 : 0) << " "
                 << (tr.audioPath.empty() ? "\"\"" : tr.audioPath) << " "
                 << "\"" << tr.dialogText << "\"\n";
            if (!tr.script.empty()) {
                out << "        script " << tr.script << "\n";
            }
            if (!tr.targetEntity.empty()) {
                out << "        target_entity " << tr.targetEntity << "\n";
            }
            if (!tr.targetWaypoint.empty()) {
                out << "        target_waypoint " << tr.targetWaypoint << "\n";
            }
            if (!tr.targetAction.empty() && tr.targetAction != "open") {
                out << "        target_action " << tr.targetAction << "\n";
            }
        }

        for (const auto& wp : map.waypoints) {
            std::string bName = wp.botName.empty() ? "\"\"" : wp.botName;
            if (bName.find(' ') != std::string::npos && bName.front() != '"') {
                bName = "\"" + bName + "\"";
            }
            out << "    waypoint " << (wp.name.empty() ? "wp" : wp.name) << " "
                 << wp.position.x << " " << wp.position.y << " " << wp.position.z << " "
                 << bName << " "
                 << wp.orderIndex << " "
                 << (wp.animation.empty() ? "Idle" : wp.animation) << " "
                 << wp.facingYaw << " "
                 << wp.waitDuration << "\n";
            if (!wp.targetTrigger.empty()) {
                out << "        target_trigger " << wp.targetTrigger << "\n";
            }
            if (!wp.triggerAction.empty() && wp.triggerAction != "fire") {
                out << "        trigger_action " << wp.triggerAction << "\n";
            }
        }

        for (const auto& lt : map.lights) {
            std::string typeStr = (lt.type == MapLightType::Spot) ? "spot" : "point";
            out << "    light " << lt.name << " "
                 << lt.position.x << " " << lt.position.y << " " << lt.position.z << " "
                 << lt.color.x << " " << lt.color.y << " " << lt.color.z << " "
                 << lt.intensity << " " << lt.radius << " " << typeStr << "\n";
        }

        for (const auto& sp : map.sprites) {
            out << "    sprite_2d " << (sp.texturePath.empty() ? "\"\"" : sp.texturePath) << " "
                 << sp.position.x << " " << sp.position.y << " " << sp.position.z << " "
                 << sp.size.x << " " << sp.size.y << " "
                 << sp.rotation << " " << sp.layer << " " << (sp.billboard ? 1 : 0) << " "
                 << sp.color.x << " " << sp.color.y << " " << sp.color.z << " " << sp.color.w << " "
                 << sp.uvMin.x << " " << sp.uvMin.y << " " << sp.uvMax.x << " " << sp.uvMax.y << "\n";
            if (!sp.name.empty()) {
                out << "        name \"" << sp.name << "\"\n";
            }
            if (!sp.script.empty()) {
                out << "        script " << sp.script << "\n";
            }
        }

    }

    bool LabMap::saveToFile(const std::string& filePath) const {
        std::string ext = std::filesystem::path(filePath).extension().string();
        for (auto& c : ext) c = static_cast<char>(::tolower(static_cast<unsigned char>(c)));
        if (ext == ".lmap") {
            return bakeToFile(filePath, true);
        }
        if (ext == ".labscene") {
            auto sc = LabScene::fromLabMap(*this);
            return sc->saveSceneToFile(filePath);
        }

        std::ofstream file(filePath);
        if (!file.is_open()) {
            LabLog::error("Failed to save map to: " + filePath);
            return false;
        }

        file << "LABMAP_VERSION 1\n\n";
        writeMapEntities(*this, file);
        file << "END_ENTITIES\n";
        LabLog::info("Successfully saved map to: " + filePath);
        return true;
    }

    bool LabMap::bakeToFile(const std::string& filePath, bool encrypt,
                            const std::vector<std::pair<std::string, std::filesystem::path>>& embeddedAssets) const {
        std::ostringstream sceneStream;
        if (const auto* asScene = dynamic_cast<const LabScene*>(this)) {
            asScene->saveSceneToStream(sceneStream);
        } else {
            auto sc = LabScene::fromLabMap(*this);
            sc->saveSceneToStream(sceneStream);
        }
        std::string sceneStr = sceneStream.str();

        std::vector<uint8_t> rawBytes(sceneStr.begin(), sceneStr.end());
        uint32_t checksum = LabPak::calculateCRC32(rawBytes);
        std::vector<uint8_t> sceneBytes = LabPak::compressBuffer(rawBytes);

        std::vector<uint8_t> pakBytes;
        if (!embeddedAssets.empty()) {
            std::filesystem::path tempPak = std::filesystem::temp_directory_path() / "temp_bake_embedded.lpak";
            if (LabPak::createPackage(tempPak, embeddedAssets, encrypt)) {
                std::ifstream pakIn(tempPak, std::ios::binary | std::ios::ate);
                if (pakIn.is_open()) {
                    std::streamsize ps = pakIn.tellg();
                    if (ps > 0) {
                        pakIn.seekg(0, std::ios::beg);
                        pakBytes.resize(static_cast<size_t>(ps));
                        pakIn.read(reinterpret_cast<char*>(pakBytes.data()), ps);
                    }
                    pakIn.close();
                }
                std::error_code ec;
                std::filesystem::remove(tempPak, ec);
            }
        }

        BakedMapHeader header;
        header.magic = 0x50414D4C; // "LMAP"
        header.version = 1;
        header.flags = (encrypt ? static_cast<uint32_t>(BAKED_MAP_ENCRYPTED) : 0u) |
                       (!pakBytes.empty() ? static_cast<uint32_t>(BAKED_MAP_EMBEDDED_ASSETS) : 0u) |
                       static_cast<uint32_t>(BAKED_MAP_AUTHENTICATED) |
                       static_cast<uint32_t>(BAKED_MAP_COMPRESSED);
        header.checksum = checksum;
        header.sceneOffset = sizeof(BakedMapHeader);
        header.sceneLength = sceneBytes.size();
        header.embeddedPakOffset = header.sceneOffset + header.sceneLength;
        header.embeddedPakLength = pakBytes.size();

        if (encrypt) {
            LabPak::cryptBuffer(sceneBytes, header.sceneOffset);
        }

        std::filesystem::create_directories(std::filesystem::path(filePath).parent_path());
        std::ofstream out(filePath, std::ios::binary);
        if (!out.is_open()) {
            LabLog::error("[LabMap] Failed to open file for baking: " + filePath);
            return false;
        }

        out.write(reinterpret_cast<const char*>(&header), sizeof(header));
        out.write(reinterpret_cast<const char*>(sceneBytes.data()), static_cast<std::streamsize>(sceneBytes.size()));
        if (!pakBytes.empty()) {
            out.write(reinterpret_cast<const char*>(pakBytes.data()), static_cast<std::streamsize>(pakBytes.size()));
        }
        out.close();

        LabLog::info("[LabMap] Successfully baked map: " + filePath +
                     " (Compressed: " + std::to_string(header.sceneLength) + " B / Raw: " +
                     std::to_string(rawBytes.size()) + " B, Embedded Assets: " +
                     std::to_string(header.embeddedPakLength) + " B, Encrypted: " + (encrypt ? "YES" : "NO") + ")");
        return true;
    }

    bool LabMap::verifyMapIntegrity(const std::string& filePath, uint32_t expectedChecksum) {
        std::string resolved = resolveMapPath(filePath);
        std::ifstream file(resolved, std::ios::binary);
        if (!file.is_open()) return false;

        BakedMapHeader header;
        if (file.read(reinterpret_cast<char*>(&header), sizeof(header))) {
            if (header.magic == 0x50414D4C && header.version == 1) {
                if (expectedChecksum != 0 && header.checksum != expectedChecksum) {
                    return false;
                }
                file.seekg(static_cast<std::streamoff>(header.sceneOffset), std::ios::beg);
                std::vector<uint8_t> sceneBytes(header.sceneLength);
                if (file.read(reinterpret_cast<char*>(sceneBytes.data()), static_cast<std::streamsize>(header.sceneLength))) {
                    if (header.flags & BAKED_MAP_ENCRYPTED) {
                        LabPak::cryptBuffer(sceneBytes, header.sceneOffset);
                    }
                    if (header.flags & BAKED_MAP_COMPRESSED) {
                        std::vector<uint8_t> decompressed;
                        if (!LabPak::decompressBuffer(sceneBytes, decompressed)) {
                            return false;
                        }
                        sceneBytes = std::move(decompressed);
                    }
                    uint32_t computed = LabPak::calculateCRC32(sceneBytes);
                    return computed == header.checksum;
                }
                return false;
            }
        }

        // Text map fallback: calculate CRC of file
        file.clear();
        file.seekg(0, std::ios::beg);
        std::vector<uint8_t> textBytes((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        uint32_t textCrc = LabPak::calculateCRC32(textBytes);
        if (expectedChecksum != 0) {
            return textCrc == expectedChecksum;
        }
        return true;
    }

    bool LabMap::isBakedBinaryMap(const std::filesystem::path& filePath) {
        std::error_code ec;
        if (!std::filesystem::exists(filePath, ec) || !std::filesystem::is_regular_file(filePath, ec)) {
            return false;
        }
        std::ifstream file(filePath, std::ios::binary);
        if (!file.is_open()) return false;
        BakedMapHeader header{};
        if (!file.read(reinterpret_cast<char*>(&header), sizeof(header))) {
            return false;
        }
        return header.magic == 0x50414D4C &&
               header.version == 1 &&
               (header.flags & BAKED_MAP_ENCRYPTED) != 0 &&
               (header.flags & BAKED_MAP_COMPRESSED) != 0;
    }

    size_t LabMap::bakeUnbakedMapsInDirectory(const std::filesystem::path& mapsDir) {
        std::error_code ec;
        if (!std::filesystem::exists(mapsDir, ec) || !std::filesystem::is_directory(mapsDir, ec)) {
            return 0;
        }

        std::vector<std::filesystem::path> candidates;
        for (const auto& entry : std::filesystem::directory_iterator(mapsDir, ec)) {
            if (!entry.is_regular_file(ec)) continue;
            std::string ext = entry.path().extension().string();
            for (char& c : ext) c = static_cast<char>(::tolower(static_cast<unsigned char>(c)));
            if (ext == ".lmap" || ext == ".labscene" || ext == ".labmap") {
                candidates.push_back(entry.path());
            }
        }

        size_t bakedCount = 0;
        for (const auto& srcPath : candidates) {
            if (!std::filesystem::exists(srcPath, ec)) continue;
            std::string ext = srcPath.extension().string();
            for (char& c : ext) c = static_cast<char>(::tolower(static_cast<unsigned char>(c)));

            std::filesystem::path targetLmap = srcPath;
            targetLmap.replace_extension(".lmap");

            // Check if target .lmap is already a baked (compressed + encrypted) binary container
            if (ext == ".lmap" && isBakedBinaryMap(targetLmap)) {
                continue;
            }
            if ((ext == ".labscene" || ext == ".labmap") && isBakedBinaryMap(targetLmap)) {
                auto srcTime = std::filesystem::last_write_time(srcPath, ec);
                auto lmapTime = std::filesystem::last_write_time(targetLmap, ec);
                if (!ec && lmapTime >= srcTime) {
                    continue;
                }
            }

            auto loaded = loadFromFile(srcPath.string());
            if (loaded && loaded->bakeToFile(targetLmap.string(), true)) {
                bakedCount++;
            }
        }
        return bakedCount;
    }

    bool LabMap::isOfficialMap(const std::string& filePath) {
        std::filesystem::path p(filePath);
        std::string generic = p.generic_string();
        for (char& c : generic) c = static_cast<char>(::tolower(static_cast<unsigned char>(c)));
        return generic.find("assets/maps/") != std::string::npos;
    }

    std::string LabMap::getQuarantinedDownloadPath(const std::string& serverHost, const std::string& mapName, uint32_t mapCrc) {
        std::string safeHost = serverHost;
        for (char& c : safeHost) {
            if (!isalnum(static_cast<unsigned char>(c)) && c != '.' && c != '_') c = '_';
        }
        std::filesystem::path mapP(mapName);
        std::string safeName = mapP.filename().string();
        for (char& c : safeName) {
            if (!isalnum(static_cast<unsigned char>(c)) && c != '.' && c != '_') c = '_';
        }
        if (safeName.empty()) safeName = "unnamed.lmap";
        if (safeName.find(".lmap") == std::string::npos) safeName += ".lmap";

        char hexBuf[16];
        snprintf(hexBuf, sizeof(hexBuf), "%08X", mapCrc);

        std::filesystem::path dir = std::filesystem::path("download/maps") / (safeHost + "_" + hexBuf);
        std::filesystem::create_directories(dir);
        return (dir / safeName).generic_string();
    }

    Vec3* MapEntityHandle::getPosition() const {
        if (!map) return nullptr;
        switch (type) {
            case Type::Brush:    return (index < map->brushes.size())   ? &map->brushes[index].position   : nullptr;
            case Type::Prop:     return (index < map->props.size())     ? &map->props[index].position     : nullptr;
            case Type::Door:     return (index < map->doors.size())     ? &map->doors[index].position     : nullptr;
            case Type::Camera:   return (index < map->cameras.size())   ? &map->cameras[index].position   : nullptr;
            case Type::Sprite:   return (index < map->sprites.size())   ? &map->sprites[index].position   : nullptr;
            case Type::Light:    return (index < map->lights.size())    ? &map->lights[index].position    : nullptr;
            case Type::Trigger:  return (index < map->triggers.size())  ? &map->triggers[index].position  : nullptr;
            case Type::Waypoint: return (index < map->waypoints.size()) ? &map->waypoints[index].position : nullptr;
            default:             return nullptr;
        }
    }

    Vec3* MapEntityHandle::getRotation() const {
        if (!map) return nullptr;
        switch (type) {
            case Type::Brush:  return (index < map->brushes.size()) ? &map->brushes[index].rotation : nullptr;
            case Type::Prop:   return (index < map->props.size())   ? &map->props[index].rotation   : nullptr;
            case Type::Camera: return (index < map->cameras.size()) ? &map->cameras[index].rotation : nullptr;
            default:           return nullptr;
        }
    }

    Vec3* MapEntityHandle::getColor() const {
        if (!map) return nullptr;
        switch (type) {
            case Type::Brush:  return (index < map->brushes.size()) ? &map->brushes[index].color : nullptr;
            case Type::Prop:   return (index < map->props.size())   ? &map->props[index].color   : nullptr;
            case Type::Door:   return (index < map->doors.size())   ? &map->doors[index].color   : nullptr;
            case Type::Light:  return (index < map->lights.size())  ? &map->lights[index].color  : nullptr;
            default:           return nullptr;
        }
    }

    MapEntityHandle LabMap::findEntityByName(const std::string& name) {
        if (name.empty()) return {};

        for (size_t i = 0; i < brushes.size(); ++i) {
            if (brushes[i].name == name) {
                return { MapEntityHandle::Type::Brush, i, brushes[i].name, this };
            }
        }
        for (size_t i = 0; i < props.size(); ++i) {
            if (props[i].name == name) {
                return { MapEntityHandle::Type::Prop, i, props[i].name, this };
            }
        }
        for (size_t i = 0; i < doors.size(); ++i) {
            if (doors[i].name == name) {
                return { MapEntityHandle::Type::Door, i, doors[i].name, this };
            }
        }
        for (size_t i = 0; i < triggers.size(); ++i) {
            if (triggers[i].name == name) {
                return { MapEntityHandle::Type::Trigger, i, triggers[i].name, this };
            }
        }
        for (size_t i = 0; i < waypoints.size(); ++i) {
            if (waypoints[i].name == name) {
                return { MapEntityHandle::Type::Waypoint, i, waypoints[i].name, this };
            }
        }
        for (size_t i = 0; i < cameras.size(); ++i) {
            if (cameras[i].name == name) {
                return { MapEntityHandle::Type::Camera, i, cameras[i].name, this };
            }
        }
        for (size_t i = 0; i < sprites.size(); ++i) {
            if (sprites[i].name == name) {
                return { MapEntityHandle::Type::Sprite, i, sprites[i].name, this };
            }
        }
        for (size_t i = 0; i < lights.size(); ++i) {
            if (lights[i].name == name) {
                return { MapEntityHandle::Type::Light, i, lights[i].name, this };
            }
        }
        return {};
    }

} // namespace Lab
