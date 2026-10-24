#include "LabProfile.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <filesystem>

namespace Lab {

    ProfileManager& ProfileManager::get() {
        static ProfileManager s_instance;
        return s_instance;
    }

    ProfileManager::ProfileManager() {
        if (!loadFromFile(_currentPath)) {
            // Attempt parent directory relative paths if running from build/
            if (std::filesystem::exists("../assets/configs/user_profile.json")) {
                loadFromFile("../assets/configs/user_profile.json");
            } else if (std::filesystem::exists("../../assets/configs/user_profile.json")) {
                loadFromFile("../../assets/configs/user_profile.json");
            } else {
                saveToFile(_currentPath);
            }
        }
    }

    static std::string extractJsonString(const std::string& content, const std::string& key, const std::string& defVal) {
        std::string pattern = "\"" + key + "\"";
        size_t pos = content.find(pattern);
        if (pos == std::string::npos) return defVal;
        size_t colon = content.find(':', pos);
        if (colon == std::string::npos) return defVal;
        size_t q1 = content.find('\"', colon);
        if (q1 == std::string::npos) return defVal;
        size_t q2 = content.find('\"', q1 + 1);
        if (q2 == std::string::npos) return defVal;
        return content.substr(q1 + 1, q2 - q1 - 1);
    }

    static int64_t extractJsonInt(const std::string& content, const std::string& key, int64_t defVal) {
        std::string pattern = "\"" + key + "\"";
        size_t pos = content.find(pattern);
        if (pos == std::string::npos) return defVal;
        size_t colon = content.find(':', pos);
        if (colon == std::string::npos) return defVal;
        size_t start = colon + 1;
        while (start < content.size() && (content[start] == ' ' || content[start] == '\t' || content[start] == '\n' || content[start] == '\r')) {
            start++;
        }
        if (start >= content.size()) return defVal;
        size_t end = start;
        if (content[end] == '-') end++;
        while (end < content.size() && std::isdigit(static_cast<unsigned char>(content[end]))) {
            end++;
        }
        if (end == start) return defVal;
        try {
            return std::stoll(content.substr(start, end - start));
        } catch (...) {
            return defVal;
        }
    }

    bool ProfileManager::loadFromFile(const std::string& path) {
        _currentPath = path;
        std::ifstream file(path);
        if (!file.is_open()) return false;

        std::stringstream ss;
        ss << file.rdbuf();
        std::string content = ss.str();

        _profile.name = extractJsonString(content, "name", "Player");
        _profile.steamId = extractJsonString(content, "steamId", "STEAM_0:1:424242");
        _profile.preferredModel = extractJsonString(content, "preferredModel", "guard");

        _profile.level = static_cast<uint32_t>(extractJsonInt(content, "level", 1));
        _profile.xp = static_cast<uint32_t>(extractJsonInt(content, "xp", 0));
        _profile.kills = static_cast<uint32_t>(extractJsonInt(content, "kills", 0));
        _profile.deaths = static_cast<uint32_t>(extractJsonInt(content, "deaths", 0));
        _profile.wins = static_cast<uint32_t>(extractJsonInt(content, "wins", 0));
        _profile.matchesPlayed = static_cast<uint32_t>(extractJsonInt(content, "matchesPlayed", 0));
        _profile.preferredTeam = static_cast<int>(extractJsonInt(content, "preferredTeam", -1));
        _profile.primaryWeapon = static_cast<uint8_t>(extractJsonInt(content, "primaryWeapon", 3));
        _profile.secondaryWeapon = static_cast<uint8_t>(extractJsonInt(content, "secondaryWeapon", 1));

        return true;
    }

    bool ProfileManager::saveToFile(const std::string& path) {
        _currentPath = path;
        std::filesystem::path p(path);
        if (p.has_parent_path()) {
            std::filesystem::create_directories(p.parent_path());
        }

        std::ofstream file(path);
        if (!file.is_open()) return false;

        file << "{\n";
        file << "  \"name\": \"" << _profile.name << "\",\n";
        file << "  \"steamId\": \"" << _profile.steamId << "\",\n";
        file << "  \"level\": " << _profile.level << ",\n";
        file << "  \"xp\": " << _profile.xp << ",\n";
        file << "  \"kills\": " << _profile.kills << ",\n";
        file << "  \"deaths\": " << _profile.deaths << ",\n";
        file << "  \"wins\": " << _profile.wins << ",\n";
        file << "  \"matchesPlayed\": " << _profile.matchesPlayed << ",\n";
        file << "  \"preferredTeam\": " << _profile.preferredTeam << ",\n";
        file << "  \"preferredModel\": \"" << _profile.preferredModel << "\",\n";
        file << "  \"primaryWeapon\": " << static_cast<int>(_profile.primaryWeapon) << ",\n";
        file << "  \"secondaryWeapon\": " << static_cast<int>(_profile.secondaryWeapon) << "\n";
        file << "}\n";

        return true;
    }

    void ProfileManager::addKill(uint32_t count) {
        _profile.kills += count;
        _profile.xp += 100 * count;
        _profile.level = 1 + (_profile.xp / 1000);
        saveToFile(_currentPath);
    }

    void ProfileManager::addDeath(uint32_t count) {
        _profile.deaths += count;
        saveToFile(_currentPath);
    }

    void ProfileManager::recordMatch(bool won) {
        _profile.matchesPlayed++;
        if (won) {
            _profile.wins++;
            _profile.xp += 500;
        } else {
            _profile.xp += 150;
        }
        _profile.level = 1 + (_profile.xp / 1000);
        saveToFile(_currentPath);
    }

} // namespace Lab
