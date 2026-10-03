#include "LabPackage.h"
#include "plugins/ProceduralLevelPlugin.h"
#include "plugins/SpeedrunTelemetryPlugin.h"
#include "plugins/RetroPostProcessPlugin.h"
#include "plugins/DialogueQuestPlugin.h"
#include "plugins/VoxelTerrainPlugin.h"
#include "plugins/AINavigationPatrolPlugin.h"
#include "plugins/ArcadeVehiclePlugin.h"
#include "plugins/DiscordRPCPlugin.h"
#include <iostream>
#include <filesystem>
#include <fstream>

namespace Lab {

    void PackageManager::registerBuiltinPackages() {
        // 1 kod 1 plugin: Each plugin is its own standalone class & file!

        // 1. Procedural Level Generator
        {
            PackageManifest p;
            p.id = "com.lab.proc-gen";
            p.displayName = "Procedural Level Generator";
            p.version = "1.2.0";
            p.author = "Lab Core Architect";
            p.category = "3D & Geometry";
            p.description = "Procedural room, corridor and platform builder. Generates complete level layouts with seed control, doorway alignment and structural pillars.";
            p.state = PackageState::Discovered;
            auto impl = std::make_shared<ProceduralLevelPlugin>();
            registerPackage(p, impl);
            enablePackage(p.id);
        }

        // 2. Speedrun & Telemetry Splits
        {
            PackageManifest p;
            p.id = "com.lab.speedrun-timer";
            p.displayName = "Speedrun & Telemetry Splits";
            p.version = "1.1.0";
            p.author = "Lab Competitive Team";
            p.category = "Gameplay & HUD";
            p.description = "Live speedrun split timer, velocity speedometer (m/s), sector segment comparisons, and delta (+/-) visual HUD overlays.";
            p.state = PackageState::Discovered;
            auto impl = std::make_shared<SpeedrunTelemetryPlugin>();
            registerPackage(p, impl);
            enablePackage(p.id);
        }

        // 3. Retro CRT & Post-Process FX
        {
            PackageManifest p;
            p.id = "com.lab.retro-fx";
            p.displayName = "Retro CRT & Post-Process FX";
            p.version = "2.0.1";
            p.author = "Lab VFX Group";
            p.category = "Rendering & VFX";
            p.description = "Retro style shader effects suite: CRT scanlines, chromatic aberration, vignette, pixelation filter, and film grain simulation.";
            p.state = PackageState::Discovered;
            auto impl = std::make_shared<RetroPostProcessPlugin>();
            registerPackage(p, impl);
            enablePackage(p.id);
        }

        // 4. Dialogue & Quest System
        {
            PackageManifest p;
            p.id = "com.lab.dialogue-quest";
            p.displayName = "Dialogue & Quest System";
            p.version = "2.1.0";
            p.author = "Lab Narrative Systems";
            p.category = "Scripting & Dev";
            p.description = "Interactive NPC dialogue branches, quest objective trackers, event triggers, and Lua script hook integration.";
            p.state = PackageState::Discovered;
            auto impl = std::make_shared<DialogueQuestPlugin>();
            registerPackage(p, impl);
            enablePackage(p.id);
        }

        // 5. Voxel Terrain & Sculptor
        {
            PackageManifest p;
            p.id = "com.lab.voxel-terrain";
            p.displayName = "Voxel Terrain & Sculptor";
            p.version = "1.3.0";
            p.author = "Lab World Tools";
            p.category = "3D & Geometry";
            p.description = "Voxel grid terrain generator and sphere-brush sculptor. Supports procedural heightmaps and real-time surface deformation.";
            p.state = PackageState::Discovered;
            auto impl = std::make_shared<VoxelTerrainPlugin>();
            registerPackage(p, impl);
        }

        // 6. AI Navmesh & Waypoint Patrol
        {
            PackageManifest p;
            p.id = "com.lab.ai-patrol";
            p.displayName = "AI Navmesh & Waypoint Patrol";
            p.version = "1.1.2";
            p.author = "Lab AI Labs";
            p.category = "Gameplay & AI";
            p.description = "Waypoint patrol graph solver, dynamic field-of-view detection, obstacle avoidance, and sensory perception.";
            p.state = PackageState::Discovered;
            auto impl = std::make_shared<AINavigationPatrolPlugin>();
            registerPackage(p, impl);
        }

        // 7. Arcade & Raycast Vehicle Dynamics
        {
            PackageManifest p;
            p.id = "com.lab.vehicle-arcade";
            p.displayName = "Arcade & Raycast Vehicle Dynamics";
            p.version = "2.1.0";
            p.author = "Lab Physics Team";
            p.category = "Physics & Vehicles";
            p.description = "Spring-damper suspension solver, tire traction curves, engine RPM simulation, and chassis weight transfer.";
            p.state = PackageState::Discovered;
            auto impl = std::make_shared<ArcadeVehiclePlugin>();
            registerPackage(p, impl);
        }

        // 8. Discord Rich Presence & Social
        {
            PackageManifest p;
            p.id = "com.lab.discord-rpc";
            p.displayName = "Discord Rich Presence & Social";
            p.version = "1.0.0";
            p.author = "Community Extensions";
            p.category = "Tools & Utilities";
            p.description = "Discord RPC status integration displaying active editor workspace, level name, and session statistics.";
            p.state = PackageState::Discovered;
            auto impl = std::make_shared<DiscordRPCPlugin>();
            registerPackage(p, impl);
        }

        // Scan assets/plugins directory for dynamic Lua scripts
        scanPluginDirectory("assets/plugins");
    }

    void PackageManager::scanPluginDirectory(const std::string& directoryPath) {
        if (!std::filesystem::exists(directoryPath)) return;

        for (const auto& entry : std::filesystem::directory_iterator(directoryPath)) {
            if (entry.is_regular_file() && entry.path().extension() == ".lua") {
                std::string fname = entry.path().stem().string();
                std::string pkgId = "com.lab.script." + fname;
                
                // If not already registered, register as discovered script plugin
                if (m_packages.find(pkgId) == m_packages.end() && m_packages.find("com.lab." + fname) == m_packages.end()) {
                    PackageManifest p;
                    p.id = pkgId;
                    p.displayName = "Script: " + fname;
                    p.version = "1.0.0";
                    p.author = "Lua Script Plugin";
                    p.category = "Scripting & Dev";
                    p.description = "Dynamic Lua add-on loaded from " + entry.path().string();
                    p.state = PackageState::Discovered;
                    registerPackage(p);
                }
            }
        }
    }

} // namespace Lab
