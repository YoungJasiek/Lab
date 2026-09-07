#pragma once
#include <string>

namespace Lab {

    enum class GameMode {
        FFA,    // Free For All
        DM,     // Deathmatch
        TDM     // Team Deathmatch
    };

    enum class MenuScreen {
        Main,
        Singleplayer,
        MultiSelect,
        HostGame,
        JoinGame,
        CharacterStudio
    };

    enum class HostType {
        DedicatedLabServer = 0, // Spawns LabServer.exe (external dedicated process)
        ListenLAN = 1          // In-process LAN / P2P Listen Server
    };

    struct GameSessionConfig {
        std::string mapPath = "assets/maps/facility_alpha.labmap";
        GameMode mode = GameMode::FFA;
        bool enableBots = true;
        int botCount = 4;
        int fragLimit = 25;
        int timeLimitMinutes = 10;
        int botDifficulty = 1; // 0 = Easy, 1 = Normal, 2 = Hard
        HostType hostType = HostType::DedicatedLabServer;
        uint16_t port = 27015;
        std::string serverName = "Lab Dedicated Arena [LAN]";
        std::string configPath = "server.cfg";

        std::string getModeString() const {
            switch (mode) {
                case GameMode::FFA: return "FFA (Free For All)";
                case GameMode::DM:  return "DM (Deathmatch)";
                case GameMode::TDM: return "TDM (Team Deathmatch)";
                default: return "FFA";
            }
        }
    };

} // namespace Lab
