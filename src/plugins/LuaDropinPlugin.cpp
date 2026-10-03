#include "plugins/LuaDropinPlugin.h"
#include "LabCore.h"

extern "C" {
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
}

namespace Lab {

namespace {

// NOTE: LabCore must NOT depend on LabScript (link order: LabScript -> LabCore),
// so drop-in states get only Lua stdlib + Lab.log here. HUD/Input/World bridges
// are registered later from LabScript side via plain C function pointers.

int lua_LabLog(lua_State* L) {
    const char* msg = lua_tostring(L, 1);
    LabLog::info(std::string("[LuaPlugin] ") + (msg ? msg : ""));
    return 0;
}

} // namespace

LuaDropinPlugin::LuaDropinPlugin(std::string filePath)
    : m_filePath(std::move(filePath)) {
    readManifestIntoMembers();
}

LuaDropinPlugin::~LuaDropinPlugin() {
    if (m_L) {
        lua_close(m_L);
        m_L = nullptr;
    }
}

void LuaDropinPlugin::readManifestIntoMembers() {
    std::string id, name, ver, author, cat, desc;
    if (parseManifest(m_filePath, id, name, ver, author, cat, desc)) {
        m_pluginId = id;
        m_displayName = name;
        m_version = ver;
    } else {
        // Fallback: derive id from filename so a broken file still shows up as Error.
        std::string stem = std::filesystem::path(m_filePath).stem().string();
        m_pluginId = "com.lab.script." + stem;
        m_displayName = "Script: " + stem;
        m_version = "1.0.0";
    }
}

bool LuaDropinPlugin::parseManifest(const std::string& filePath, std::string& outId,
                                    std::string& outDisplayName, std::string& outVersion,
                                    std::string& outAuthor, std::string& outCategory,
                                    std::string& outDescription) {
    lua_State* L = luaL_newstate();
    if (!L) return false;
    luaL_openlibs(L);
    if (luaL_dofile(L, filePath.c_str()) != LUA_OK) {
        lua_close(L);
        return false;
    }
    lua_getglobal(L, "Plugin");
    if (!lua_istable(L, -1)) { lua_close(L); return false; }

    auto field = [&](const char* name, const std::string& fb) {
        lua_getfield(L, -1, name);
        const char* s = lua_tostring(L, -1);
        std::string r = s ? s : fb;
        lua_pop(L, 1);
        return r;
    };

    std::string stem = std::filesystem::path(filePath).stem().string();
    outId = field("id", "com.lab.script." + stem);
    outDisplayName = field("displayName", "Script: " + stem);
    outVersion = field("version", "1.0.0");
    outAuthor = field("author", "Community");
    outCategory = field("category", "Scripting & Dev");
    outDescription = field("description", "Drop-in Lua plugin: " + filePath);
    lua_pop(L, 1);
    lua_close(L);
    return !outId.empty();
}

void LuaDropinPlugin::registerMinimalBindings() {
    // Lab.log -> engine log (LabCore only, no LabScript dependency)
    lua_newtable(m_L);
    lua_pushcfunction(m_L, lua_LabLog);
    lua_setfield(m_L, -2, "log");
    lua_setglobal(m_L, "Lab");
}

bool LuaDropinPlugin::onLoad() {
    if (m_loaded) return true;
    if (!std::filesystem::exists(m_filePath)) {
        LabLog::error("LuaDropinPlugin: file missing: " + m_filePath);
        return false;
    }
    m_L = luaL_newstate();
    if (!m_L) return false;
    luaL_openlibs(m_L);
    registerMinimalBindings();

    if (luaL_dofile(m_L, m_filePath.c_str()) != LUA_OK) {
        const char* err = lua_tostring(m_L, -1);
        LabLog::error("LuaDropinPlugin '" + m_filePath + "': " + (err ? err : "load error"));
        lua_close(m_L);
        m_L = nullptr;
        return false;
    }

    readManifestIntoMembers();
    std::error_code ec;
    m_loadedWriteTime = std::filesystem::last_write_time(m_filePath, ec);

    if (!callPluginMethod("onLoad")) {
        LabLog::error("LuaDropinPlugin '" + m_displayName + "' onLoad() failed.");
        lua_close(m_L);
        m_L = nullptr;
        return false;
    }
    m_loaded = true;
    LabLog::info("LuaDropinPlugin loaded: '" + m_displayName + "' (" + m_pluginId + ") from " + m_filePath);
    return true;
}

void LuaDropinPlugin::onUpdate(float dt) {
    if (!m_loaded || !m_L) return;
    callPluginUpdate(dt);
}

void LuaDropinPlugin::onUnload() {
    if (m_L) {
        callPluginMethod("onUnload");
        lua_close(m_L);
        m_L = nullptr;
    }
    m_loaded = false;
}

bool LuaDropinPlugin::callPluginMethod(const char* method) {
    if (!m_L) return false;
    lua_getglobal(m_L, "Plugin");
    if (!lua_istable(m_L, -1)) { lua_pop(m_L, 1); return false; }
    lua_getfield(m_L, -1, method);
    if (!lua_isfunction(m_L, -1)) { lua_pop(m_L, 2); return true; } // missing hook = OK
    lua_remove(m_L, -2); // drop Plugin table, keep function
    if (lua_pcall(m_L, 0, 1, 0) != LUA_OK) {
        const char* err = lua_tostring(m_L, -1);
        LabLog::error("LuaDropinPlugin '" + m_displayName + "." + method + "': " + (err ? err : "pcall error"));
        lua_pop(m_L, 1);
        return false;
    }
    bool ok = true;
    if (lua_isboolean(m_L, -1)) ok = lua_toboolean(m_L, -1) != 0;
    lua_pop(m_L, 1);
    return ok;
}

bool LuaDropinPlugin::callPluginUpdate(float dt) {
    if (!m_L) return false;
    lua_getglobal(m_L, "Plugin");
    if (!lua_istable(m_L, -1)) { lua_pop(m_L, 1); return false; }
    lua_getfield(m_L, -1, "onUpdate");
    if (!lua_isfunction(m_L, -1)) { lua_pop(m_L, 2); return true; }
    lua_remove(m_L, -2);
    lua_pushnumber(m_L, dt);
    if (lua_pcall(m_L, 1, 0, 0) != LUA_OK) {
        const char* err = lua_tostring(m_L, -1);
        LabLog::error("LuaDropinPlugin '" + m_displayName + ".onUpdate': " + (err ? err : "pcall error"));
        lua_pop(m_L, 1);
        return false;
    }
    return true;
}

bool LuaDropinPlugin::isOutOfDate() const {
    std::error_code ec;
    auto t = std::filesystem::last_write_time(m_filePath, ec);
    if (ec) return true; // deleted or unreadable -> treat as changed so scan removes it
    return t != m_loadedWriteTime;
}

bool LuaDropinPlugin::reloadFromDisk() {
    onUnload();
    readManifestIntoMembers();
    return onLoad();
}

} // namespace Lab
