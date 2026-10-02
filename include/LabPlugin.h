#pragma once
#include "LabCore.h"
#include <string>
#include <vector>
#include <memory>

namespace Lab {

/// @brief Universal plugin / addon interface for extensible engine subsystems.
/// Allows adding features (2D physics, 3D Jolt, Vehicles, Water) without bloating engine core.
class LAB_CORE_API ILabPlugin {
public:
    virtual ~ILabPlugin() = default;

    /// @brief Unique identifier of the plugin.
    virtual const char* getName() const = 0;

    /// @brief Semantic version string (e.g. "1.0.0").
    virtual const char* getVersion() const = 0;

    /// @brief Called when the plugin is loaded into the engine.
    /// @return True on successful initialization, false on failure.
    virtual bool onLoad() = 0;

    /// @brief Called every frame during engine update loop.
    /// @param dt Delta time in seconds.
    virtual void onUpdate(float dt) = 0;

    /// @brief Called before plugin is unloaded for resource deallocation.
    virtual void onUnload() = 0;
};

/// @brief Thread-safe manager governing plugin lifecycles, execution order and queries.
class LAB_CORE_API PluginManager {
public:
    static PluginManager& get();

    /// @brief Registers and loads a new plugin instance.
    bool registerPlugin(std::shared_ptr<ILabPlugin> plugin);

    /// @brief Unloads and removes a registered plugin by name.
    void unregisterPlugin(const std::string& name);

    /// @brief Ticks all active plugins.
    void updateAll(float dt);

    /// @brief Gracefully unloads all active plugins in reverse order.
    void unloadAll();

    /// @brief Returns the number of currently active plugins.
    size_t getPluginCount() const;

    /// @brief Finds a registered plugin by name, returning nullptr if not found.
    std::shared_ptr<ILabPlugin> getPlugin(const std::string& name);

private:
    PluginManager() = default;
    ~PluginManager();
    PluginManager(const PluginManager&) = delete;
    PluginManager& operator=(const PluginManager&) = delete;

    std::vector<std::shared_ptr<ILabPlugin>> m_plugins;
};

} // namespace Lab
