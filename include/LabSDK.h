#pragma once
#include "LabExport.h"
#include <string>
#include <memory>

#define LAB_SDK_VERSION_MAJOR 1
#define LAB_SDK_VERSION_MINOR 0
#define LAB_SDK_VERSION_PATCH 0
#define LAB_SDK_VERSION_STRING "1.0.0"
#define LAB_SDK_NAME "Frozen-Life SDK (Lab Engine)"

namespace Lab {

    class Engine;
    class Renderer;

    /// @brief Information descriptor for a game or mod built on the Frozen-Life SDK.
    struct GameManifest {
        std::string gameTitle = "Frozen-Life";
        std::string gameDir = "frozen_life";
        std::string developer = "Lab Team";
        std::string version = "1.0.0";
        std::string defaultMap = "assets/maps/site14_chapter1.labscene";
        bool isMultiplayerCapable = true;
    };

    /// @brief Universal interface for game logic modules running on top of the Frozen-Life SDK.
    /// Following the Source SDK model (engine vs client/server), this interface encapsulates
    /// weapons, player state, HUD rules, and bot behaviors without polluting core engine DLLs.
    class IGameModule {
    public:
        virtual ~IGameModule() = default;

        /// @brief Called on startup to initialize game systems, weapons, and assets.
        virtual bool onInit(Engine* engine) = 0;

        /// @brief Called every tick for game simulation, bot AI, and player physics.
        virtual void onUpdate(float dt) = 0;

        /// @brief Called during the main 3D scene render pass (viewmodels, world props, muzzle flashes).
        virtual void onRender3D(Renderer* renderer) = 0;

        /// @brief Called during the 2D HUD / VGUI render pass.
        virtual void onRenderUI(Renderer* renderer) = 0;

        /// @brief Called on shutdown to save states, scores, and cleanup resources.
        virtual void onShutdown() = 0;

        /// @brief Returns the manifest describing this game or mod.
        virtual const GameManifest& getManifest() const = 0;
    };

} // namespace Lab
