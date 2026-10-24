#pragma once

#include <string>
#include <cstdint>
#include <vector>

namespace Lab {

    struct UserProfile {
        std::string name = "Player";
        std::string steamId = "STEAM_0:1:424242";
        uint32_t level = 1;
        uint32_t xp = 0;
        uint32_t kills = 0;
        uint32_t deaths = 0;
        uint32_t wins = 0;
        uint32_t matchesPlayed = 0;
        int preferredTeam = -1; // -1: Auto/FFA, 0: Red (Bravo), 1: Blue (Alpha)
        std::string preferredModel = "guard"; // "guard", "worker", "scientist"
        uint8_t primaryWeapon = 3;   // Default: Assault Rifle (3)
        uint8_t secondaryWeapon = 1; // Default: Pistol (1)

        float getKD() const {
            return deaths > 0 ? (static_cast<float>(kills) / static_cast<float>(deaths)) : static_cast<float>(kills);
        }

        float getWinRate() const {
            return matchesPlayed > 0 ? (static_cast<float>(wins) * 100.0f / static_cast<float>(matchesPlayed)) : 0.0f;
        }

        std::string getRankTitle() const {
            if (level >= 50) return "Global Elite";
            if (level >= 40) return "Master Guardian";
            if (level >= 30) return "Special Agent";
            if (level >= 20) return "Senior Operative";
            if (level >= 10) return "Field Specialist";
            if (level >= 5)  return "Trooper";
            return "Recruit";
        }
    };

    class ProfileManager {
    public:
        static ProfileManager& get();

        UserProfile& getProfile() { return _profile; }
        const UserProfile& getProfile() const { return _profile; }

        bool loadFromFile(const std::string& path = "assets/configs/user_profile.json");
        bool saveToFile(const std::string& path = "assets/configs/user_profile.json");

        void addKill(uint32_t count = 1);
        void addDeath(uint32_t count = 1);
        void recordMatch(bool won);

    private:
        ProfileManager();
        ~ProfileManager() = default;

        UserProfile _profile;
        std::string _currentPath = "assets/configs/user_profile.json";
    };

} // namespace Lab
