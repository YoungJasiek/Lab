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

    uint16_t port = Lab::DEFAULT_SERVER_PORT;
    std::string mapName = "facility_alpha.labmap";

    // Parse command line arguments
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-port" && i + 1 < argc) {
            port = static_cast<uint16_t>(std::stoi(argv[++i]));
        } else if (arg == "-map" && i + 1 < argc) {
            mapName = argv[++i];
        } else if (arg == "-help" || arg == "--help" || arg == "/?") {
            std::cout << "Usage: LabServer.exe [-port <27015>] [-map <map_name>]\n";
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
    if (!server.start(port, mapName)) {
        std::cerr << "[Server] Critical: Could not bind to port " << port << ".\n";
        Lab::NetworkSystem::shutdown();
        return 1;
    }

    std::cout << "[Server] Ready. Listening on UDP 0.0.0.0:" << port << ".\n";
    std::cout << "[Server] Press Ctrl+C to stop server.\n\n";

    using Clock = std::chrono::steady_clock;
    auto lastTime = Clock::now();
    auto lastStatusTime = lastTime;
    uint32_t lastReportTick = 0;

    const auto targetFrameDuration = std::chrono::microseconds(15625); // ~64 Hz

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
