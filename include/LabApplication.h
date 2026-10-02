#pragma once
#include "LabCore.h"

namespace Lab {

/// @brief Universal interface for any application or game running on top of Lab Engine.
/// Decouples engine core from game-specific mechanics and storylines.
class LAB_CORE_API ILabApplication {
public:
    virtual ~ILabApplication() = default;

    /// @brief Called once upon engine startup to initialize game-specific systems and assets.
    /// @return True if initialization succeeded, false to abort.
    virtual bool onInit() = 0;

    /// @brief Called every frame for game logic updates.
    /// @param dt Delta time in seconds.
    virtual void onUpdate(float dt) = 0;

    /// @brief Called every frame to issue render passes.
    virtual void onRender() = 0;

    /// @brief Called on engine shutdown for graceful resource cleanup.
    virtual void onShutdown() = 0;
};

} // namespace Lab
