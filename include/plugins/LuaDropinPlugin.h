#pragma once

#include "LabPlugin.h"
#include <string>
#include <filesystem>

struct lua_State;

namespace Lab {

/// @brief Single-file drop-in Lua plugin. 1 file in assets/plugins/ = 1 plugin.
/// Each instance owns an isolated lua_State, so global `Plugin` tables never collide.
/// Enable/disable = onLoad/onUnload. Deleting the file removes the plugin. No engine recompile.
class LAB_CORE_API LuaDropinPlugin : public ILabPlugin {
public:
    explicit LuaDropinPlugin(std::string filePath);
    ~LuaDropinPlugin() override;

    LuaDropinPlugin(const LuaDropinPlugin&) = delete;
    LuaDropinPlugin& operator=(const LuaDropinPlugin&) = delete;

    const char* getName() const override { return m_displayName.c_str(); }
    const char* getVersion() const override { return m_version.c_str(); }

    bool onLoad() override;
    void onUpdate(float dt) override;
    void onUnload() override;

    /// @brief Re-reads manifest fields (id/displayName/version/...) without executing logic.
    /// @return false when file is missing or has no valid `Plugin` table.
    static bool parseManifest(const std::string& filePath, std::string& outId,
                              std::string& outDisplayName, std::string& outVersion,
                              std::string& outAuthor, std::string& outCategory,
                              std::string& outDescription);

    const std::string& getFilePath() const { return m_filePath; }
    const std::string& getPluginId() const { return m_pluginId; }
    std::filesystem::file_time_type getLoadedWriteTime() const { return m_loadedWriteTime; }

    /// @brief True when file on disk changed since last load.
    bool isOutOfDate() const;
    /// @brief Full reload from disk (onUnload + onLoad + manifest refresh).
    bool reloadFromDisk();

private:
    bool callPluginMethod(const char* method);
    bool callPluginUpdate(float dt);
    void readManifestIntoMembers();
    void registerMinimalBindings();

    lua_State* m_L{nullptr};
    std::string m_filePath;
    std::string m_pluginId;
    std::string m_displayName{"Unnamed Plugin"};
    std::string m_version{"1.0.0"};
    std::filesystem::file_time_type m_loadedWriteTime{};
    bool m_loaded{false};
};

} // namespace Lab
