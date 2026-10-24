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
        CharacterStudio,
        Addons,
        Options,
        Achievements,
        LoadingMap
    };

    enum class NetworkMode {
        LAN = 0,    // Local Area Network broadcast & discovery
        P2P = 1     // Peer-to-Peer / Direct IP connection
    };

    struct GameSessionConfig {
        std::string mapPath = "assets/maps/facility_alpha.lmap";
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

    struct LoadingSequenceState {
        bool active = false;
        float progress = 0.0f;
        int stageIndex = 0;
        std::string statusText = "Initializing connection...";
        std::string fastDlCurrentFile = "";
        size_t fastDlBytesDownloaded = 0;
        size_t fastDlTotalBytes = 0;
        bool isMultiplayer = false;
        GameSessionConfig pendingConfig;

        void begin(const GameSessionConfig& cfg, bool multiplayer = false) {
            active = true;
            progress = 0.0f;
            stageIndex = 0;
            isMultiplayer = multiplayer;
            pendingConfig = cfg;
            statusText = multiplayer ? "Connecting to server..." : "Preparing local game server...";
            fastDlCurrentFile = (cfg.mapPath.find(".lmap") != std::string::npos) ? cfg.mapPath : (cfg.mapPath + ".lmap");
            fastDlTotalBytes = 4259840;
            fastDlBytesDownloaded = 0;
        }

        bool step(float dt) {
            if (!active) return false;
            progress = (progress + dt * 1.35f > 1.0f) ? 1.0f : (progress + dt * 1.35f);
            double dlRatio = (static_cast<double>(progress) * 1.4 > 1.0) ? 1.0 : (static_cast<double>(progress) * 1.4);
            fastDlBytesDownloaded = static_cast<size_t>(static_cast<double>(fastDlTotalBytes) * dlRatio);
            if (progress < 0.20f) {
                stageIndex = 0;
                statusText = isMultiplayer ? "Connecting to server & handshaking..." : "Initializing local physics & AI subsystem...";
            } else if (progress < 0.50f) {
                stageIndex = 1;
                statusText = "FastDL: Verifying & synchronizing map packages (.lmap / .lpak)...";
            } else if (progress < 0.75f) {
                stageIndex = 2;
                statusText = "Parsing BSP geometry, lightmaps & static props...";
            } else if (progress < 0.95f) {
                stageIndex = 3;
                statusText = "Precaching 70-bone Guard skeletal rig & audio banks...";
            } else {
                stageIndex = 4;
                statusText = "Sending client info & spawning player...";
            }
            return (progress >= 1.0f);
        }
    };

} // namespace Lab
