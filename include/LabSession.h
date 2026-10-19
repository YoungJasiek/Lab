#pragma once
#include <string>
#include <cstdint>

namespace Lab {

    enum class GameMode {
        FFA,    // Free For All
        DM,     // Deathmatch
        TDM,    // Team Deathmatch
        Campaign // Singleplayer Campaign / Story Map
    };

    enum class MenuScreen {
        Main,
        Singleplayer,
        MultiSelect,
        HostGame,
        JoinGame,
        CharacterStudio
    };

    enum class NetworkMode {
        LAN = 0,    // Local Area Network broadcast & discovery
        P2P = 1     // Peer-to-Peer / Direct IP connection
    };

    struct GameSessionConfig {
        std::string mapPath = "assets/maps/facility_alpha.labscene";
        GameMode mode = GameMode::FFA;
        bool enableBots = false;
        int botCount = 0;
        int fragLimit = 25;
        int timeLimitMinutes = 10;
        int botDifficulty = 1; // 0 = Easy, 1 = Normal, 2 = Hard
        NetworkMode networkMode = NetworkMode::LAN;
        uint16_t port = 27015;
        std::string serverName = "Lab Arena [LAN]";
        std::string configPath = "assets/configs/server.cfg";
        std::string roomCode = "7788";

        std::string getModeString() const {
            switch (mode) {
                case GameMode::FFA: return "FFA (Free For All)";
                case GameMode::DM:  return "DM (Deathmatch)";
                case GameMode::TDM: return "TDM (Team Deathmatch)";
                case GameMode::Campaign: return "Campaign";
                default: return "FFA";
            }
        }
    };

} // namespace Lab
