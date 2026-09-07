#include "LabNetwork.h"
#include <iostream>
#include <chrono>
#include <thread>
#include <csignal>
#include <string>

static bool g_serverRunning = true;

static void signalHandler(int /*signum*/) {
    std::cout << "\n[Server] Shutdown signal received. Stopping dedicated server...\n";
    g_serverRunning = false;
}

int main(int argc, char* argv[]) {
    std::cout << "=======================================================\n";
    std::cout << "   FROZEN-LIFE : LAB HEADLESS DEDICATED SERVER v1.0   \n";
    std::cout << "=======================================================\n";

    Lab::ServerConfig config;

    // 1. Locate config file from arguments or defaults
    std::string configPath = "server.cfg";
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if ((arg == "-config" || arg == "--config" || arg == "-c") && i + 1 < argc) {
            configPath = argv[++i];
        }
    }

    // 2. Load configuration file
    if (!config.loadFromFile(configPath)) {
        if (!config.loadFromFile("assets/configs/server.cfg")) {
            std::cout << "[Server] No existing configuration found. Generating default '" << configPath << "'...\n";
            config.saveToFile(configPath);
        }
    }

    // 3. Command-line parameters override config file values
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if ((arg == "-port" || arg == "--port" || arg == "-p") && i + 1 < argc) {
            config.port = static_cast<uint16_t>(std::stoi(argv[++i]));
        } else if ((arg == "-map" || arg == "--map" || arg == "-m") && i + 1 < argc) {
            config.mapName = argv[++i];
        } else if ((arg == "-mode" || arg == "--mode") && i + 1 < argc) {
            config.gameMode = argv[++i];
        } else if ((arg == "-name" || arg == "--name") && i + 1 < argc) {
            config.serverName = argv[++i];
        } else if ((arg == "-maxplayers" || arg == "--maxplayers") && i + 1 < argc) {
            config.maxPlayers = static_cast<uint16_t>(std::stoi(argv[++i]));
        } else if ((arg == "-tickrate" || arg == "--tickrate") && i + 1 < argc) {
            config.tickrate = static_cast<uint16_t>(std::stoi(argv[++i]));
        } else if ((arg == "-bots" || arg == "--bots") && i + 1 < argc) {
            config.botCount = std::stoi(argv[++i]);
            config.enableBots = (config.botCount > 0);
        } else if (arg == "-lan" || arg == "--lan") {
            config.lanMode = true;
        } else if (arg == "-help" || arg == "--help" || arg == "/?") {
            std::cout << "Usage: LabServer.exe [-config <file.cfg>] [-port <27015>] [-map <map_name>] [-mode <FFA|DM|TDM>] [-bots <count>] [-lan]\n";
            return 0;
        }
    }

    std::signal(SIGINT, signalHandler);
#ifdef SIGBREAK
    std::signal(SIGBREAK, signalHandler);
#endif

    if (!Lab::NetworkSystem::init()) {
        std::cerr << "[Server] Critical: Failed to initialize network system.\n";
        return 1;
    }

    Lab::DedicatedServer server;
    if (!server.start(config)) {
        std::cerr << "[Server] Critical: Could not bind to port " << config.port << ".\n";
        Lab::NetworkSystem::shutdown();
        return 1;
    }

    std::cout << "[Config] Loaded server configuration:\n";
    std::cout << "  - Config File:    " << configPath << "\n";
    std::cout << "  - Server Name:    " << config.serverName << "\n";
    std::cout << "  - UDP Port:       " << config.port << "\n";
    std::cout << "  - Active Map:     " << config.mapName << "\n";
    std::cout << "  - Game Mode:      " << config.gameMode << "\n";
    std::cout << "  - Max Players:    " << config.maxPlayers << "\n";
    std::cout << "  - Target Tickrate:" << config.tickrate << " Hz\n";
    std::cout << "  - AI Bots:        " << config.botCount << " (" << (config.enableBots ? "Enabled" : "Disabled") << ")\n";
    std::cout << "  - Frag Limit:     " << config.fragLimit << " kills\n";
    std::cout << "  - LAN Broadcast:  " << (config.lanMode ? "Active" : "Disabled") << "\n";
    std::cout << "=======================================================\n";
    std::cout << "[Server] Ready. Listening on UDP 0.0.0.0:" << config.port << ".\n";
    std::cout << "[Server] Press Ctrl+C to stop server.\n\n";

    using Clock = std::chrono::steady_clock;
    auto lastTime = Clock::now();
    auto lastStatusTime = lastTime;
    uint32_t lastReportTick = 0;

    const auto targetFrameDuration = std::chrono::microseconds(1000000 / (config.tickrate > 0 ? config.tickrate : 64));

    while (g_serverRunning && server.isRunning()) {
        auto now = Clock::now();
        std::chrono::duration<float> deltaSec = now - lastTime;
        lastTime = now;

        server.tick(deltaSec.count());

        // Periodic heartbeat report every 10 seconds
        if (std::chrono::duration_cast<std::chrono::seconds>(now - lastStatusTime).count() >= 10) {
            uint32_t currentTick = server.getServerTick();
            uint32_t ticksProcessed = currentTick - lastReportTick;
            float actualHz = static_cast<float>(ticksProcessed) / 10.0f;

            std::cout << "[Heartbeat] Server Tick: " << currentTick 
                      << " | Actual Tickrate: " << actualHz << " Hz"
                      << " | Connected Players: " << server.getClientCount() << "\n";

            lastStatusTime = now;
            lastReportTick = currentTick;
        }

        // Sleep remainder of tick to prevent 100% CPU spinning
        auto workDuration = Clock::now() - now;
        if (workDuration < targetFrameDuration) {
            std::this_thread::sleep_for(targetFrameDuration - workDuration);
        }
    }

    server.stop();
    Lab::NetworkSystem::shutdown();
    std::cout << "[Server] Dedicated Server clean exit.\n";
    return 0;
}
