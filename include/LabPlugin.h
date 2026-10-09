#pragma once
#include "LabCore.h"
#include <string>
#include <vector>
#include <memory>
#include <filesystem>

#if defined(_WIN32)
    #define LAB_PLUGIN_EXPORT extern "C" __declspec(dllexport)
#else
    #define LAB_PLUGIN_EXPORT extern "C" __attribute__((visibility("default")))
#endif

namespace Lab {

constexpr uint32_t LAB_PLUGIN_API_VERSION = 1;

/// @brief Universal plugin / addon interface for extensible engine subsystems.
/// Shared dynamic libraries (.dll on Windows, .so on Linux) export `CreatePlugin()`
class LAB_CORE_API ILabPlugin {
public:
    virtual ~ILabPlugin() = default;

    /// @brief Unique identifier of the plugin.
    virtual const char* getName() const = 0;

    /// @brief Semantic version string (e.g. "1.0.0").
    virtual const char* getVersion() const = 0;

    /// @brief Engine Plugin API version for ABI verification.
    virtual uint32_t getApiVersion() const { return LAB_PLUGIN_API_VERSION; }

    /// @brief Called when the plugin is loaded into the engine.
    /// @return True on successful initialization, false on failure.
    virtual bool onLoad() = 0;

    /// @brief Called every frame during engine update loop.
    /// @param dt Delta time in seconds.
    virtual void onUpdate(float dt) = 0;

    /// @brief Editor GUI hook allowing plugins to render custom panels/tools without engine hardcoding
    virtual void onRenderGUI() {}

    /// @brief Called before plugin is unloaded for resource deallocation.
    virtual void onUnload() = 0;
};

typedef ILabPlugin* (*CreatePluginFn)();

/// @brief Thread-safe manager governing plugin lifecycles, execution order and queries.
class LAB_CORE_API PluginManager {
public:
    static PluginManager& get();

    /// @brief Registers and loads a new plugin instance.
    bool registerPlugin(std::shared_ptr<ILabPlugin> plugin);

    /// @brief Dynamically loads a shared plugin library (.dll on Windows, .so on Linux)
    bool loadDynamicPlugin(const std::string& libraryPath);

    /// @brief Automatically discovers and loads all dynamic plugins from a directory (default "plugins/")
    size_t loadPluginsFromDirectory(const std::string& directoryPath = "plugins");

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

    /// @brief Read-only list of all currently active plugins
    const std::vector<std::shared_ptr<ILabPlugin>>& getAllPlugins() const { return m_plugins; }

private:
    PluginManager() = default;
    ~PluginManager();
    PluginManager(const PluginManager&) = delete;
    PluginManager& operator=(const PluginManager&) = delete;

    std::vector<std::shared_ptr<ILabPlugin>> m_plugins;
};

} // namespace Lab
