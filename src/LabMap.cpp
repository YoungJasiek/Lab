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
                } else if (token == "brush") {
                    MapBrush b;
                    ss >> b.type >> b.position.x >> b.position.y >> b.position.z
                       >> b.size.x >> b.size.y >> b.size.z
                       >> b.color.x >> b.color.y >> b.color.z;
                    if (ss >> b.texturePath) {
                        if (b.texturePath == "\"\"") b.texturePath = "";
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
                }
            }
        }

        LabLog::info("Loaded .LABMAP: " + map->metadata.name + " (" + std::to_string(map->brushes.size()) + " brushes, " +
                     std::to_string(map->props.size()) + " props, " + std::to_string(map->doors.size()) + " doors)");
        return map;
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

        for (const auto& b : brushes) {
            file << "    brush " << b.type << " "
                 << b.position.x << " " << b.position.y << " " << b.position.z << " "
                 << b.size.x << " " << b.size.y << " " << b.size.z << " "
                 << b.color.x << " " << b.color.y << " " << b.color.z << " "
                 << (b.texturePath.empty() ? "\"\"" : b.texturePath) << "\n";
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
