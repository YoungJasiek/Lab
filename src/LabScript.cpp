#include "LabScript.h"
#include "LabCore.h"
#include "LabAudio.h"
#include "LabInput.h"
#include "LabMap.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <unordered_map>
#include <filesystem>

extern "C" {
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
}

namespace Lab {

    static LabMap* s_activeMap = nullptr;
    static std::unordered_map<std::string, std::string> s_hudTexts;
    static std::unordered_map<std::string, float> s_hudValues;
    static std::unordered_map<std::string, bool> s_hudVisibles;
    static std::function<void(int)> s_doorUnlockCallback = nullptr;
    static std::function<void(const std::string&, const std::string&)> s_serverChatCallback = nullptr;
    static std::string s_activeCameraName = "";
    static std::function<void(const std::string&)> s_cameraSwitchCallback = nullptr;

    void ScriptEngine::registerDoorUnlockCallback(std::function<void(int)> callback) {
        s_doorUnlockCallback = callback;
    }

    void ScriptEngine::registerServerChatCallback(std::function<void(const std::string&, const std::string&)> callback) {
        s_serverChatCallback = callback;
    }

    void ScriptEngine::registerCameraSwitchCallback(std::function<void(const std::string&)> callback) {
        s_cameraSwitchCallback = callback;
    }

    void ScriptEngine::setActiveCameraName(const std::string& name) {
        s_activeCameraName = name;
        if (s_cameraSwitchCallback) s_cameraSwitchCallback(name);
    }

    const std::string& ScriptEngine::getActiveCameraName() {
        return s_activeCameraName;
    }

    void ScriptEngine::setActiveMap(LabMap* map) {
        _activeMap = map;
        s_activeMap = map;
        if (!map) return;

        // Auto-run scripts attached to map entities (brush, prop, door, camera, sprite)
        auto runScriptIfAny = [this](const std::string& sPath) {
            if (sPath.empty()) return;
            std::string resolved = sPath;
            if (!std::filesystem::exists(resolved)) {
                if (std::filesystem::exists("assets/scripts/" + sPath)) resolved = "assets/scripts/" + sPath;
                else if (std::filesystem::exists("../assets/scripts/" + sPath)) resolved = "../assets/scripts/" + sPath;
            }
            if (std::filesystem::exists(resolved)) {
                std::ifstream f(resolved);
                if (f.is_open()) {
                    std::stringstream buf;
                    buf << f.rdbuf();
                    executeString(buf.str());
                }
            }
        };

        for (const auto& b : map->brushes) runScriptIfAny(b.script);
        for (const auto& p : map->props) runScriptIfAny(p.script);
        for (const auto& d : map->doors) runScriptIfAny(d.script);
        for (const auto& c : map->cameras) runScriptIfAny(c.script);
        for (const auto& sp : map->sprites) runScriptIfAny(sp.script);
    }

    void ScriptEngine::setHUDText(const std::string& id, const std::string& text) {
        s_hudTexts[id] = text;
    }

    std::string ScriptEngine::getHUDText(const std::string& id) {
        auto it = s_hudTexts.find(id);
        return it != s_hudTexts.end() ? it->second : "";
    }

    void ScriptEngine::setHUDValue(const std::string& id, float val) {
        s_hudValues[id] = val;
    }

    float ScriptEngine::getHUDValue(const std::string& id) {
        auto it = s_hudValues.find(id);
        return it != s_hudValues.end() ? it->second : 0.0f;
    }

    void ScriptEngine::setHUDVisible(const std::string& id, bool visible) {
        s_hudVisibles[id] = visible;
    }

    bool ScriptEngine::isHUDVisible(const std::string& id) {
        auto it = s_hudVisibles.find(id);
        return it != s_hudVisibles.end() ? it->second : true;
    }

    void ScriptEngine::update(float dt) {
        if (!_L) return;
        lua_getglobal(_L, "OnUpdate");
        if (lua_isfunction(_L, -1)) {
            lua_pushnumber(_L, (lua_Number)dt);
            if (lua_pcall(_L, 1, 0, 0) != LUA_OK) {
                const char* err = lua_tostring(_L, -1);
                LabLog::error(std::string("[Lua OnUpdate Error] ") + (err ? err : "unknown"));
                lua_pop(_L, 1);
            }
        } else {
            lua_pop(_L, 1);
        }
    }

    // --- HUD Lua Callbacks ---
    static int lua_HUDSetText(lua_State* L) {
        const char* id = luaL_checkstring(L, 1);
        const char* text = luaL_checkstring(L, 2);
        if (id && text) {
            ScriptEngine::setHUDText(id, text);
        }
        return 0;
    }

    static int lua_HUDGetText(lua_State* L) {
        const char* id = luaL_checkstring(L, 1);
        std::string res = id ? ScriptEngine::getHUDText(id) : "";
        lua_pushstring(L, res.c_str());
        return 1;
    }

    static int lua_HUDSetValue(lua_State* L) {
        const char* id = luaL_checkstring(L, 1);
        float val = (float)luaL_checknumber(L, 2);
        if (id) {
            ScriptEngine::setHUDValue(id, val);
        }
        return 0;
    }

    static int lua_HUDGetValue(lua_State* L) {
        const char* id = luaL_checkstring(L, 1);
        float val = id ? ScriptEngine::getHUDValue(id) : 0.0f;
        lua_pushnumber(L, (lua_Number)val);
        return 1;
    }

    static int lua_HUDSetVisible(lua_State* L) {
        const char* id = luaL_checkstring(L, 1);
        bool vis = lua_toboolean(L, 2);
        if (id) {
            ScriptEngine::setHUDVisible(id, vis);
        }
        return 0;
    }

    static int lua_HUDIsVisible(lua_State* L) {
        const char* id = luaL_checkstring(L, 1);
        bool vis = id ? ScriptEngine::isHUDVisible(id) : true;
        lua_pushboolean(L, vis);
        return 1;
    }

    // --- World & Entity Lua Callbacks ---
    static int lua_EntitySetPosition(lua_State* L) {
        const char* name = nullptr;
        lua_getfield(L, 1, "__name");
        if (lua_isstring(L, -1)) name = lua_tostring(L, -1);
        lua_pop(L, 1);

        float x = (float)luaL_checknumber(L, 2);
        float y = (float)luaL_checknumber(L, 3);
        float z = (float)luaL_checknumber(L, 4);

        if (name && s_activeMap) {
            auto h = s_activeMap->findEntityByName(name);
            if (h.isValid()) {
                *h.position = { x, y, z };
            }
        }
        return 0;
    }

    static int lua_EntityGetPosition(lua_State* L) {
        const char* name = nullptr;
        lua_getfield(L, 1, "__name");
        if (lua_isstring(L, -1)) name = lua_tostring(L, -1);
        lua_pop(L, 1);

        float x = 0.0f, y = 0.0f, z = 0.0f;
        if (name && s_activeMap) {
            auto h = s_activeMap->findEntityByName(name);
            if (h.isValid()) {
                x = h.position->x;
                y = h.position->y;
                z = h.position->z;
            }
        }
        lua_pushnumber(L, x);
        lua_pushnumber(L, y);
        lua_pushnumber(L, z);
        return 3;
    }

    static int lua_EntityMove(lua_State* L) {
        const char* name = nullptr;
        lua_getfield(L, 1, "__name");
        if (lua_isstring(L, -1)) name = lua_tostring(L, -1);
        lua_pop(L, 1);

        float dx = (float)luaL_checknumber(L, 2);
        float dy = (float)luaL_checknumber(L, 3);
        float dz = (float)luaL_checknumber(L, 4);

        if (name && s_activeMap) {
            auto h = s_activeMap->findEntityByName(name);
            if (h.isValid()) {
                h.position->x += dx;
                h.position->y += dy;
                h.position->z += dz;
            }
        }
        return 0;
    }

    static int lua_EntityGetX(lua_State* L) {
        const char* name = nullptr;
        lua_getfield(L, 1, "__name");
        if (lua_isstring(L, -1)) name = lua_tostring(L, -1);
        lua_pop(L, 1);
        float x = 0.0f;
        if (name && s_activeMap) {
            auto h = s_activeMap->findEntityByName(name);
            if (h.isValid()) x = h.position->x;
        }
        lua_pushnumber(L, x);
        return 1;
    }

    static int lua_EntityGetY(lua_State* L) {
        const char* name = nullptr;
        lua_getfield(L, 1, "__name");
        if (lua_isstring(L, -1)) name = lua_tostring(L, -1);
        lua_pop(L, 1);
        float y = 0.0f;
        if (name && s_activeMap) {
            auto h = s_activeMap->findEntityByName(name);
            if (h.isValid()) y = h.position->y;
        }
        lua_pushnumber(L, y);
        return 1;
    }

    static int lua_EntityGetZ(lua_State* L) {
        const char* name = nullptr;
        lua_getfield(L, 1, "__name");
        if (lua_isstring(L, -1)) name = lua_tostring(L, -1);
        lua_pop(L, 1);
        float z = 0.0f;
        if (name && s_activeMap) {
            auto h = s_activeMap->findEntityByName(name);
            if (h.isValid()) z = h.position->z;
        }
        lua_pushnumber(L, z);
        return 1;
    }

    static int lua_EntitySetX(lua_State* L) {
        const char* name = nullptr;
        lua_getfield(L, 1, "__name");
        if (lua_isstring(L, -1)) name = lua_tostring(L, -1);
        lua_pop(L, 1);
        float x = (float)luaL_checknumber(L, 2);
        if (name && s_activeMap) {
            auto h = s_activeMap->findEntityByName(name);
            if (h.isValid()) h.position->x = x;
        }
        return 0;
    }

    static int lua_EntitySetY(lua_State* L) {
        const char* name = nullptr;
        lua_getfield(L, 1, "__name");
        if (lua_isstring(L, -1)) name = lua_tostring(L, -1);
        lua_pop(L, 1);
        float y = (float)luaL_checknumber(L, 2);
        if (name && s_activeMap) {
            auto h = s_activeMap->findEntityByName(name);
            if (h.isValid()) h.position->y = y;
        }
        return 0;
    }

    static int lua_EntitySetZ(lua_State* L) {
        const char* name = nullptr;
        lua_getfield(L, 1, "__name");
        if (lua_isstring(L, -1)) name = lua_tostring(L, -1);
        lua_pop(L, 1);
        float z = (float)luaL_checknumber(L, 2);
        if (name && s_activeMap) {
            auto h = s_activeMap->findEntityByName(name);
            if (h.isValid()) h.position->z = z;
        }
        return 0;
    }

    static int lua_WorldGetEntity(lua_State* L) {
        const char* name = luaL_checkstring(L, 1);
        if (!name || !s_activeMap) {
            lua_pushnil(L);
            return 1;
        }

        auto handle = s_activeMap->findEntityByName(name);
        if (!handle.isValid()) {
            lua_pushnil(L);
            return 1;
        }

        lua_newtable(L);
        lua_pushstring(L, name);
        lua_setfield(L, -2, "__name");

        lua_pushcfunction(L, lua_EntitySetPosition);
        lua_setfield(L, -2, "setPosition");
        lua_pushcfunction(L, lua_EntityGetPosition);
        lua_setfield(L, -2, "getPosition");
        lua_pushcfunction(L, lua_EntityMove);
        lua_setfield(L, -2, "move");
        lua_pushcfunction(L, lua_EntityGetX);
        lua_setfield(L, -2, "getX");
        lua_pushcfunction(L, lua_EntityGetY);
        lua_setfield(L, -2, "getY");
        lua_pushcfunction(L, lua_EntityGetZ);
        lua_setfield(L, -2, "getZ");
        lua_pushcfunction(L, lua_EntitySetX);
        lua_setfield(L, -2, "setX");
        lua_pushcfunction(L, lua_EntitySetY);
        lua_setfield(L, -2, "setY");
        lua_pushcfunction(L, lua_EntitySetZ);
        lua_setfield(L, -2, "setZ");

        return 1;
    }

    static int lua_WorldMoveEntity(lua_State* L) {
        const char* name = luaL_checkstring(L, 1);
        float dx = (float)luaL_checknumber(L, 2);
        float dy = (float)luaL_checknumber(L, 3);
        float dz = (float)luaL_checknumber(L, 4);

        if (name && s_activeMap) {
            auto h = s_activeMap->findEntityByName(name);
            if (h.isValid()) {
                h.position->x += dx;
                h.position->y += dy;
                h.position->z += dz;
            }
        }
        return 0;
    }

    // --- Camera Lua Callbacks ---
    static int lua_CameraSetActive(lua_State* L) {
        const char* name = luaL_checkstring(L, 1);
        if (name) {
            ScriptEngine::setActiveCameraName(name);
            if (s_activeMap) {
                for (auto& c : s_activeMap->cameras) {
                    c.active = (c.name == name);
                }
            }
        }
        return 0;
    }

    static int lua_CameraGetActive(lua_State* L) {
        lua_pushstring(L, ScriptEngine::getActiveCameraName().c_str());
        return 1;
    }

    static int lua_CameraSetFov(lua_State* L) {
        const char* name = luaL_checkstring(L, 1);
        float fov = (float)luaL_checknumber(L, 2);
        if (name && s_activeMap) {
            for (auto& c : s_activeMap->cameras) {
                if (c.name == name) {
                    c.fov = fov;
                    break;
                }
            }
        }
        return 0;
    }

    static int lua_CameraSetPosition(lua_State* L) {
        const char* name = luaL_checkstring(L, 1);
        float x = (float)luaL_checknumber(L, 2);
        float y = (float)luaL_checknumber(L, 3);
        float z = (float)luaL_checknumber(L, 4);
        if (name && s_activeMap) {
            for (auto& c : s_activeMap->cameras) {
                if (c.name == name) {
                    c.position = Vec3(x, y, z);
                    break;
                }
            }
        }
        return 0;
    }

    static int lua_CameraSetRotation(lua_State* L) {
        const char* name = luaL_checkstring(L, 1);
        float pitch = (float)luaL_checknumber(L, 2);
        float yaw = (float)luaL_checknumber(L, 3);
        float roll = (float)luaL_checknumber(L, 4);
        if (name && s_activeMap) {
            for (auto& c : s_activeMap->cameras) {
                if (c.name == name) {
                    c.rotation = Vec3(pitch, yaw, roll);
                    break;
                }
            }
        }
        return 0;
    }

    static int lua_CameraGet(lua_State* L) {
        const char* name = luaL_checkstring(L, 1);
        if (name && s_activeMap) {
            for (const auto& c : s_activeMap->cameras) {
                if (c.name == name) {
                    lua_newtable(L);
                    lua_pushstring(L, c.name.c_str()); lua_setfield(L, -2, "name");
                    lua_pushnumber(L, c.fov); lua_setfield(L, -2, "fov");
                    lua_pushboolean(L, c.active); lua_setfield(L, -2, "active");
                    lua_pushstring(L, c.follow_target.c_str()); lua_setfield(L, -2, "follow_target");
                    lua_pushnumber(L, c.position.x); lua_setfield(L, -2, "x");
                    lua_pushnumber(L, c.position.y); lua_setfield(L, -2, "y");
                    lua_pushnumber(L, c.position.z); lua_setfield(L, -2, "z");
                    lua_pushnumber(L, c.rotation.x); lua_setfield(L, -2, "pitch");
                    lua_pushnumber(L, c.rotation.y); lua_setfield(L, -2, "yaw");
                    lua_pushnumber(L, c.rotation.z); lua_setfield(L, -2, "roll");
                    return 1;
                }
            }
        }
        lua_pushnil(L);
        return 1;
    }

    // --- Input Lua Callbacks ---
    static int lua_InputRegisterAction(lua_State* L) {
        const char* name = luaL_checkstring(L, 1);
        const char* desc = luaL_optstring(L, 2, "");
        if (name) {
            InputActionManager::instance().registerAction(name, desc);
            if (lua_gettop(L) >= 3) {
                if (lua_isstring(L, 3)) {
                    int k = InputActionManager::stringToKey(lua_tostring(L, 3));
                    if (k != 0) InputActionManager::instance().bindKeyToAction(name, k);
                } else if (lua_isinteger(L, 3)) {
                    InputActionManager::instance().bindKeyToAction(name, (int)lua_tointeger(L, 3));
                }
            }
            if (lua_gettop(L) >= 4 && lua_isstring(L, 4)) {
                GamepadButton btn = InputActionManager::stringToGamepadButton(lua_tostring(L, 4));
                InputActionManager::instance().bindGamepadButtonToAction(name, btn);
            }
        }
        return 0;
    }

    static int lua_InputRegisterAxis(lua_State* L) {
        const char* name = luaL_checkstring(L, 1);
        const char* desc = luaL_optstring(L, 2, "");
        if (name) {
            InputActionManager::instance().registerAxis(name, desc);
            if (lua_gettop(L) >= 4) {
                int posKey = lua_isstring(L, 3) ? InputActionManager::stringToKey(lua_tostring(L, 3)) : (int)lua_tointeger(L, 3);
                int negKey = lua_isstring(L, 4) ? InputActionManager::stringToKey(lua_tostring(L, 4)) : (int)lua_tointeger(L, 4);
                if (posKey != 0 || negKey != 0) {
                    InputActionManager::instance().bindKeysToAxis(name, posKey, negKey);
                }
            }
            if (lua_gettop(L) >= 5 && lua_isstring(L, 5)) {
                GamepadAxis ga = InputActionManager::stringToGamepadAxis(lua_tostring(L, 5));
                InputActionManager::instance().bindGamepadAxisToAxis(name, ga);
            }
        }
        return 0;
    }

    static int lua_InputIsActionPressed(lua_State* L) {
        const char* name = luaL_checkstring(L, 1);
        bool pressed = name ? InputActionManager::instance().isActionPressed(name) : false;
        lua_pushboolean(L, pressed);
        return 1;
    }

    static int lua_InputIsActionJustPressed(lua_State* L) {
        const char* name = luaL_checkstring(L, 1);
        bool pressed = name ? InputActionManager::instance().isActionJustPressed(name) : false;
        lua_pushboolean(L, pressed);
        return 1;
    }

    static int lua_InputIsActionJustReleased(lua_State* L) {
        const char* name = luaL_checkstring(L, 1);
        bool rel = name ? InputActionManager::instance().isActionJustReleased(name) : false;
        lua_pushboolean(L, rel);
        return 1;
    }

    static int lua_InputGetAxis(lua_State* L) {
        const char* name = luaL_checkstring(L, 1);
        float val = name ? InputActionManager::instance().getAxis(name) : 0.0f;
        lua_pushnumber(L, (lua_Number)val);
        return 1;
    }

    static int lua_InputBindKey(lua_State* L) {
        const char* act = luaL_checkstring(L, 1);
        if (act) {
            int k = lua_isstring(L, 2) ? InputActionManager::stringToKey(lua_tostring(L, 2)) : (int)lua_tointeger(L, 2);
            InputActionManager::instance().bindKeyToAction(act, k);
        }
        return 0;
    }

    static int lua_InputBindGamepadButton(lua_State* L) {
        const char* act = luaL_checkstring(L, 1);
        const char* btn = luaL_checkstring(L, 2);
        if (act && btn) {
            InputActionManager::instance().bindGamepadButtonToAction(act, InputActionManager::stringToGamepadButton(btn));
        }
        return 0;
    }

    static int lua_InputBindGamepadAxis(lua_State* L) {
        const char* ax = luaL_checkstring(L, 1);
        const char* btn = luaL_checkstring(L, 2);
        if (ax && btn) {
            InputActionManager::instance().bindGamepadAxisToAxis(ax, InputActionManager::stringToGamepadAxis(btn));
        }
        return 0;
    }

    static int lua_InputSaveBindings(lua_State* L) {
        const char* path = luaL_checkstring(L, 1);
        bool ok = path ? InputActionManager::instance().saveToFile(path) : false;
        lua_pushboolean(L, ok);
        return 1;
    }

    static int lua_InputLoadBindings(lua_State* L) {
        const char* path = luaL_checkstring(L, 1);
        bool ok = path ? InputActionManager::instance().loadFromFile(path) : false;
        lua_pushboolean(L, ok);
        return 1;
    }

    // --- Lua C Function Callbacks ---
    static int lua_LabLog(lua_State* L) {
        const char* msg = luaL_checkstring(L, 1);
        if (msg) {
            LabLog::info(std::string("[Lua] ") + msg);
        }
        return 0;
    }

    static int lua_PlaySound(lua_State* L) {
        int soundId = (int)luaL_checkinteger(L, 1);
        float volume = (float)luaL_optnumber(L, 2, 1.0);
        AudioEngine::playSound((SoundID)soundId, volume);
        return 0;
    }

    static int lua_UnlockDoor(lua_State* L) {
        int doorIndex = (int)luaL_checkinteger(L, 1);
        if (s_doorUnlockCallback) {
            s_doorUnlockCallback(doorIndex);
        }
        return 0;
    }

    static int lua_AddChatMessage(lua_State* L) {
        const char* sender = luaL_checkstring(L, 1);
        const char* text = luaL_checkstring(L, 2);
        if (s_serverChatCallback && sender && text) {
            s_serverChatCallback(sender, text);
        }
        return 0;
    }

    // --- ScriptEngine Implementation ---
    ScriptEngine::ScriptEngine() = default;

    ScriptEngine::~ScriptEngine() {
        shutdown();
    }

    ScriptEngine::ScriptEngine(ScriptEngine&& other) noexcept
        : _L(other._L),
          _currentScriptPath(std::move(other._currentScriptPath)),
          _maxHealth(other._maxHealth),
          _startArmor(other._startArmor),
          _armorAbsorptionRatio(other._armorAbsorptionRatio),
          _respawnTime(other._respawnTime),
          _barrelDamageMultiplier(other._barrelDamageMultiplier),
          _retinalScanDuration(other._retinalScanDuration),
          _retinalAuthorizedUser(std::move(other._retinalAuthorizedUser)),
          _retinalClearanceLevel(other._retinalClearanceLevel),
          _medkitHeal(other._medkitHeal),
          _ammoBoxAmount(other._ammoBoxAmount),
          _weaponPadRespawnTime(other._weaponPadRespawnTime),
          _weaponDefs(std::move(other._weaponDefs)),
          _missionCardCount(other._missionCardCount),
          _missionTimeLimit(other._missionTimeLimit),
          _missionName(std::move(other._missionName)),
          _missionStartText(std::move(other._missionStartText)) {
        other._L = nullptr;
    }

    ScriptEngine& ScriptEngine::operator=(ScriptEngine&& other) noexcept {
        if (this != &other) {
            shutdown();
            _L = other._L;
            other._L = nullptr;
            _currentScriptPath = std::move(other._currentScriptPath);
            _maxHealth = other._maxHealth;
            _startArmor = other._startArmor;
            _armorAbsorptionRatio = other._armorAbsorptionRatio;
            _respawnTime = other._respawnTime;
            _barrelDamageMultiplier = other._barrelDamageMultiplier;
            _retinalScanDuration = other._retinalScanDuration;
            _retinalAuthorizedUser = std::move(other._retinalAuthorizedUser);
            _retinalClearanceLevel = other._retinalClearanceLevel;
            _medkitHeal = other._medkitHeal;
            _ammoBoxAmount = other._ammoBoxAmount;
            _weaponPadRespawnTime = other._weaponPadRespawnTime;
            _weaponDefs = std::move(other._weaponDefs);
            _missionCardCount = other._missionCardCount;
            _missionTimeLimit = other._missionTimeLimit;
            _missionName = std::move(other._missionName);
            _missionStartText = std::move(other._missionStartText);
        }
        return *this;
    }

    void ScriptEngine::registerEngineBindings() {
        if (!_L) return;

        // Register table 'Lab'
        lua_newtable(_L);

        lua_pushcfunction(_L, lua_LabLog);
        lua_setfield(_L, -2, "log");

        lua_pushcfunction(_L, lua_PlaySound);
        lua_setfield(_L, -2, "playSound");

        lua_pushcfunction(_L, lua_UnlockDoor);
        lua_setfield(_L, -2, "unlockDoor");

        lua_pushcfunction(_L, lua_AddChatMessage);
        lua_setfield(_L, -2, "addChatMessage");

        lua_setglobal(_L, "Lab");

        // Register table 'Input'
        lua_newtable(_L);
        lua_pushcfunction(_L, lua_InputRegisterAction);
        lua_setfield(_L, -2, "registerAction");
        lua_pushcfunction(_L, lua_InputRegisterAxis);
        lua_setfield(_L, -2, "registerAxis");
        lua_pushcfunction(_L, lua_InputIsActionPressed);
        lua_setfield(_L, -2, "isActionPressed");
        lua_pushcfunction(_L, lua_InputIsActionJustPressed);
        lua_setfield(_L, -2, "isActionJustPressed");
        lua_pushcfunction(_L, lua_InputIsActionJustReleased);
        lua_setfield(_L, -2, "isActionJustReleased");
        lua_pushcfunction(_L, lua_InputGetAxis);
        lua_setfield(_L, -2, "getAxis");
        lua_pushcfunction(_L, lua_InputBindKey);
        lua_setfield(_L, -2, "bindKey");
        lua_pushcfunction(_L, lua_InputBindGamepadButton);
        lua_setfield(_L, -2, "bindGamepadButton");
        lua_pushcfunction(_L, lua_InputBindGamepadAxis);
        lua_setfield(_L, -2, "bindGamepadAxis");
        lua_pushcfunction(_L, lua_InputSaveBindings);
        lua_setfield(_L, -2, "saveBindings");
        lua_pushcfunction(_L, lua_InputLoadBindings);
        lua_setfield(_L, -2, "loadBindings");
        lua_setglobal(_L, "Input");

        // Register table 'HUD'
        lua_newtable(_L);
        lua_pushcfunction(_L, lua_HUDSetText);
        lua_setfield(_L, -2, "setText");
        lua_pushcfunction(_L, lua_HUDGetText);
        lua_setfield(_L, -2, "getText");
        lua_pushcfunction(_L, lua_HUDSetValue);
        lua_setfield(_L, -2, "setValue");
        lua_pushcfunction(_L, lua_HUDGetValue);
        lua_setfield(_L, -2, "getValue");
        lua_pushcfunction(_L, lua_HUDSetVisible);
        lua_setfield(_L, -2, "setVisible");
        lua_pushcfunction(_L, lua_HUDIsVisible);
        lua_setfield(_L, -2, "isVisible");
        lua_setglobal(_L, "HUD");

        // Register table 'World'
        lua_newtable(_L);
        lua_pushcfunction(_L, lua_WorldGetEntity);
        lua_setfield(_L, -2, "getEntity");
        lua_pushcfunction(_L, lua_WorldMoveEntity);
        lua_setfield(_L, -2, "moveEntity");
        lua_setglobal(_L, "World");

        // Register table 'Camera'
        lua_newtable(_L);
        lua_pushcfunction(_L, lua_CameraSetActive);
        lua_setfield(_L, -2, "setActive");
        lua_pushcfunction(_L, lua_CameraGetActive);
        lua_setfield(_L, -2, "getActive");
        lua_pushcfunction(_L, lua_CameraSetFov);
        lua_setfield(_L, -2, "setFov");
        lua_pushcfunction(_L, lua_CameraSetPosition);
        lua_setfield(_L, -2, "setPosition");
        lua_pushcfunction(_L, lua_CameraSetRotation);
        lua_setfield(_L, -2, "setRotation");
        lua_pushcfunction(_L, lua_CameraGet);
        lua_setfield(_L, -2, "get");
        lua_setglobal(_L, "Camera");

        // Global camera helpers
        lua_pushcfunction(_L, lua_CameraSetActive);
        lua_setglobal(_L, "setActiveCamera");
        lua_pushcfunction(_L, lua_CameraGetActive);
        lua_setglobal(_L, "getActiveCamera");
    }

    void ScriptEngine::loadEmbeddedFallbackScript() {
        const char* fallbackLua = R"(
            -- ==========================================================
            -- Frozen-Life: Gameplay Mechanics Script (Lua 5.4)
            -- ==========================================================

            PlayerRules = {
                maxHealth = 150.0,
                startSuitArmor = 50.0,
                armorAbsorptionRatio = 0.70,
                respawnTime = 4.0,
                barrelDamageMultiplier = 0.75
            }

            RetinalScanner = {
                scanDuration = 1.25,
                authorizedUser = "DR. VANCE",
                clearanceLevel = 3,
                targetDoorIndex = 0
            }

            Pickups = {
                MedkitHeal = 50.0,
                AmmoBoxAmount = 36,
                WeaponPadRespawnTime = 60.0
            }

            Weapons = {
                Pipe = { id = 0, name = "RURKA METALOWA", damage = 55.0, range = 2.6, fireRate = 0.55, isMelee = true, slot = 1, recoilPitch = 0.04, recoilKick = 0.05 },
                Pistol = { id = 1, name = "PISTOLET 9MM", damage = 24.0, fireRate = 0.22, clipSize = 18, reserve = 144, slot = 2, recoilPitch = 0.035, recoilKick = 0.045 },
                Shotgun = { id = 2, name = "STRZELBA SPAS-12", damage = 14.0, fireRate = 0.85, clipSize = 8, reserve = 64, slot = 3, recoilPitch = 0.09, recoilKick = 0.12 },
                M4A4S = { id = 3, name = "KARABIN M4A4-S", damage = 28.0, fireRate = 0.11, clipSize = 30, reserve = 180, isAuto = true, slot = 4, recoilPitch = 0.038, recoilKick = 0.05 },
                SG553 = { id = 4, name = "KARABIN SG553", damage = 35.0, fireRate = 0.14, clipSize = 30, reserve = 150, isAuto = true, slot = 5, recoilPitch = 0.045, recoilKick = 0.06 },
                Minigun = { id = 5, name = "MINIGUN VULCAN", damage = 22.0, fireRate = 0.065, clipSize = 150, reserve = 450, isAuto = true, slot = 6, recoilPitch = 0.022, recoilKick = 0.035 },
                PlasmaGun = { id = 6, name = "PLAZMA GUN", damage = 45.0, splashDamage = 25.0, splashRadius = 3.5, fireRate = 0.28, clipSize = 25, reserve = 100, isProj = true, slot = 7, recoilPitch = 0.035, recoilKick = 0.05 },
                Railgun = { id = 7, name = "RAILGUN", damage = 135.0, fireRate = 1.4, clipSize = 5, reserve = 25, slot = 8, recoilPitch = 0.12, recoilKick = 0.18 },
                RPG = { id = 8, name = "WYRZUTNIA RPG", damage = 120.0, splashDamage = 75.0, splashRadius = 5.5, fireRate = 1.2, clipSize = 1, reserve = 12, isProj = true, slot = 9, recoilPitch = 0.14, recoilKick = 0.22 }
            }

            -- Pure Lua damage calculation function
            function CalculateDamage(incomingDamage, currentArmor, currentHealth)
                local absorbRatio = PlayerRules.armorAbsorptionRatio or 0.70
                local absorbed = 0.0
                if currentArmor > 0.0 then
                    absorbed = math.min(currentArmor, incomingDamage * absorbRatio)
                end
                local finalDmg = incomingDamage - absorbed
                local isLethal = (currentHealth - finalDmg <= 0.0)
                return finalDmg, absorbed, isLethal
            end

            -- Event triggered when retinal scan finishes
            function OnRetinalScanComplete(scannerId, linkedDoorIndex)
                Lab.log("Retinal scan verified for " .. RetinalScanner.authorizedUser .. " on door " .. tostring(linkedDoorIndex))
                Lab.unlockDoor(linkedDoorIndex)
                Lab.playSound(25) -- SoundID::AccessGranted
                Lab.addChatMessage("[SECURITY]", "Retinal scan verified: Door " .. tostring(linkedDoorIndex) .. " unlocked for " .. RetinalScanner.authorizedUser)
                return true
            end
        )";

        executeString(fallbackLua);
    }

    bool ScriptEngine::init(const std::string& scriptPath) {
        shutdown();

        _L = luaL_newstate();
        if (!_L) {
            LabLog::error("Failed to allocate Lua state!");
            return false;
        }

        luaL_openlibs(_L);
        registerEngineBindings();

        _currentScriptPath = scriptPath;

        // Check if user script exists on disk
        std::ifstream file(scriptPath);
        if (file.is_open()) {
            std::stringstream buffer;
            buffer << file.rdbuf();
            std::string code = buffer.str();
            if (!executeString(code)) {
                LabLog::warn("Failed executing Lua file: " + scriptPath + " - using fallback script.");
                loadEmbeddedFallbackScript();
            } else {
                LabLog::info("Loaded game mechanics script from: " + scriptPath);
            }
        } else {
            loadEmbeddedFallbackScript();
        }

        cacheGameplayTables();
        return true;
    }

    void ScriptEngine::shutdown() {
        if (_L) {
            lua_close(_L);
            _L = nullptr;
        }
        _weaponDefs.clear();
    }

    bool ScriptEngine::reload() {
        return init(_currentScriptPath);
    }

    bool ScriptEngine::executeString(const std::string& luaCode) {
        if (!_L) return false;
        int err = luaL_dostring(_L, luaCode.c_str());
        if (err != LUA_OK) {
            const char* errStr = lua_tostring(_L, -1);
            LabLog::error(std::string("Lua Error: ") + (errStr ? errStr : "Unknown"));
            lua_pop(_L, 1);
            return false;
        }
        return true;
    }

    bool ScriptEngine::executeBytecode(const std::vector<uint8_t>& bytecode, const std::string& chunkName) {
        if (!_L || bytecode.empty()) return false;
        int status = luaL_loadbuffer(_L, reinterpret_cast<const char*>(bytecode.data()), bytecode.size(), chunkName.c_str());
        if (status != LUA_OK) {
            const char* errStr = lua_tostring(_L, -1);
            LabLog::error(std::string("Lua LoadBuffer Error: ") + (errStr ? errStr : "Unknown"));
            lua_pop(_L, 1);
            return false;
        }
        status = lua_pcall(_L, 0, LUA_MULTRET, 0);
        if (status != LUA_OK) {
            const char* errStr = lua_tostring(_L, -1);
            LabLog::error(std::string("Lua PCall Error: ") + (errStr ? errStr : "Unknown"));
            lua_pop(_L, 1);
            return false;
        }
        return true;
    }

    void ScriptEngine::cacheGameplayTables() {
        if (!_L) return;

        // 1. PlayerRules table
        lua_getglobal(_L, "PlayerRules");
        if (lua_istable(_L, -1)) {
            lua_getfield(_L, -1, "maxHealth");
            if (lua_isnumber(_L, -1)) _maxHealth = (float)lua_tonumber(_L, -1);
            lua_pop(_L, 1);

            lua_getfield(_L, -1, "startSuitArmor");
            if (lua_isnumber(_L, -1)) _startArmor = (float)lua_tonumber(_L, -1);
            lua_pop(_L, 1);

            lua_getfield(_L, -1, "armorAbsorptionRatio");
            if (lua_isnumber(_L, -1)) _armorAbsorptionRatio = (float)lua_tonumber(_L, -1);
            lua_pop(_L, 1);

            lua_getfield(_L, -1, "respawnTime");
            if (lua_isnumber(_L, -1)) _respawnTime = (float)lua_tonumber(_L, -1);
            lua_pop(_L, 1);

            lua_getfield(_L, -1, "barrelDamageMultiplier");
            if (lua_isnumber(_L, -1)) _barrelDamageMultiplier = (float)lua_tonumber(_L, -1);
            lua_pop(_L, 1);
        }
        lua_pop(_L, 1);

        // 2. RetinalScanner table
        lua_getglobal(_L, "RetinalScanner");
        if (lua_istable(_L, -1)) {
            lua_getfield(_L, -1, "scanDuration");
            if (lua_isnumber(_L, -1)) _retinalScanDuration = (float)lua_tonumber(_L, -1);
            lua_pop(_L, 1);

            lua_getfield(_L, -1, "authorizedUser");
            if (lua_isstring(_L, -1)) _retinalAuthorizedUser = lua_tostring(_L, -1);
            lua_pop(_L, 1);

            lua_getfield(_L, -1, "clearanceLevel");
            if (lua_isinteger(_L, -1)) _retinalClearanceLevel = (int)lua_tointeger(_L, -1);
            lua_pop(_L, 1);
        }
        lua_pop(_L, 1);

        // 3. Pickups table
        lua_getglobal(_L, "Pickups");
        if (lua_istable(_L, -1)) {
            lua_getfield(_L, -1, "MedkitHeal");
            if (lua_isnumber(_L, -1)) _medkitHeal = (float)lua_tonumber(_L, -1);
            lua_pop(_L, 1);

            lua_getfield(_L, -1, "AmmoBoxAmount");
            if (lua_isinteger(_L, -1)) _ammoBoxAmount = (int)lua_tointeger(_L, -1);
            lua_pop(_L, 1);

            lua_getfield(_L, -1, "WeaponPadRespawnTime");
            if (lua_isnumber(_L, -1)) _weaponPadRespawnTime = (float)lua_tonumber(_L, -1);
            lua_pop(_L, 1);
        }
        lua_pop(_L, 1);

        // 4. Weapons table
        _weaponDefs.clear();
        lua_getglobal(_L, "Weapons");
        if (lua_istable(_L, -1)) {
            lua_pushnil(_L);
            while (lua_next(_L, -2) != 0) {
                if (lua_istable(_L, -1)) {
                    WeaponScriptDef def;
                    if (lua_isstring(_L, -2)) def.key = lua_tostring(_L, -2);

                    lua_getfield(_L, -1, "id");
                    if (lua_isinteger(_L, -1)) def.id = (int)lua_tointeger(_L, -1);
                    lua_pop(_L, 1);

                    lua_getfield(_L, -1, "name");
                    if (lua_isstring(_L, -1)) def.name = lua_tostring(_L, -1);
                    lua_pop(_L, 1);

                    lua_getfield(_L, -1, "damage");
                    if (lua_isnumber(_L, -1)) def.damage = (float)lua_tonumber(_L, -1);
                    lua_pop(_L, 1);

                    lua_getfield(_L, -1, "fireRate");
                    if (lua_isnumber(_L, -1)) def.fireRate = (float)lua_tonumber(_L, -1);
                    lua_pop(_L, 1);

                    lua_getfield(_L, -1, "clipSize");
                    if (lua_isinteger(_L, -1)) def.clipSize = (int)lua_tointeger(_L, -1);
                    lua_pop(_L, 1);

                    lua_getfield(_L, -1, "reserve");
                    if (lua_isinteger(_L, -1)) def.maxReserve = (int)lua_tointeger(_L, -1);
                    lua_pop(_L, 1);

                    lua_getfield(_L, -1, "slot");
                    if (lua_isinteger(_L, -1)) def.slot = (int)lua_tointeger(_L, -1);
                    lua_pop(_L, 1);

                    lua_getfield(_L, -1, "isMelee");
                    if (lua_isboolean(_L, -1)) def.isMelee = lua_toboolean(_L, -1) != 0;
                    lua_pop(_L, 1);

                    lua_getfield(_L, -1, "isAuto");
                    if (lua_isboolean(_L, -1)) def.isAutomatic = lua_toboolean(_L, -1) != 0;
                    lua_pop(_L, 1);

                    lua_getfield(_L, -1, "isProj");
                    if (lua_isboolean(_L, -1)) def.isProjectile = lua_toboolean(_L, -1) != 0;
                    lua_pop(_L, 1);

                    lua_getfield(_L, -1, "splashDamage");
                    if (lua_isnumber(_L, -1)) def.splashDamage = (float)lua_tonumber(_L, -1);
                    lua_pop(_L, 1);

                    lua_getfield(_L, -1, "splashRadius");
                    if (lua_isnumber(_L, -1)) def.splashRadius = (float)lua_tonumber(_L, -1);
                    lua_pop(_L, 1);

                    lua_getfield(_L, -1, "recoilPitch");
                    if (lua_isnumber(_L, -1)) def.recoilPitch = (float)lua_tonumber(_L, -1);
                    lua_pop(_L, 1);

                    lua_getfield(_L, -1, "recoilKick");
                    if (lua_isnumber(_L, -1)) def.recoilKick = (float)lua_tonumber(_L, -1);
                    lua_pop(_L, 1);

                    _weaponDefs.push_back(def);
                }
                lua_pop(_L, 1);
            }
        }
        lua_pop(_L, 1);

        // Sort weapon definitions by ID ascending
        std::sort(_weaponDefs.begin(), _weaponDefs.end(), [](const auto& a, const auto& b) {
            return a.id < b.id;
        });

        // 5. Mission table (rules & objectives)
        lua_getglobal(_L, "Mission");
        if (lua_istable(_L, -1)) {
            lua_getfield(_L, -1, "cardCount");
            if (lua_isnumber(_L, -1)) _missionCardCount = (int)lua_tonumber(_L, -1);
            lua_pop(_L, 1);

            lua_getfield(_L, -1, "timeLimit");
            if (lua_isnumber(_L, -1)) _missionTimeLimit = (float)lua_tonumber(_L, -1);
            lua_pop(_L, 1);

            lua_getfield(_L, -1, "name");
            if (lua_isstring(_L, -1)) _missionName = lua_tostring(_L, -1);
            lua_pop(_L, 1);

            lua_getfield(_L, -1, "startText");
            if (lua_isstring(_L, -1)) _missionStartText = lua_tostring(_L, -1);
            lua_pop(_L, 1);
        }
        lua_pop(_L, 1);
    }

    std::pair<std::string, std::string> ScriptEngine::callMissionProgress(int cardsCollected, float timeLeft) {
        if (!_L) return {"", ""};
        lua_getglobal(_L, "MissionProgress");
        if (lua_isfunction(_L, -1)) {
            lua_pushinteger(_L, cardsCollected);
            lua_pushnumber(_L, (lua_Number)timeLeft);
            if (lua_pcall(_L, 2, 2, 0) == LUA_OK) {
                std::string status = lua_tostring(_L, -2) ? lua_tostring(_L, -2) : "";
                std::string msg = lua_tostring(_L, -1) ? lua_tostring(_L, -1) : "";
                lua_pop(_L, 2);
                return {status, msg};
            } else {
                const char* err = lua_tostring(_L, -1);
                LabLog::error(std::string("[Lua MissionProgress Error] ") + (err ? err : "unknown"));
                lua_pop(_L, 1);
            }
        } else {
            lua_pop(_L, 1);
        }
        return {"", ""};
    }

    void ScriptEngine::callOnTriggerEnter(int entityId, int otherEntityId) {
        if (!_L) return;
        lua_getglobal(_L, "OnTriggerEnter");
        if (lua_isfunction(_L, -1)) {
            lua_pushinteger(_L, entityId);
            lua_pushinteger(_L, otherEntityId);
            if (lua_pcall(_L, 2, 0, 0) != LUA_OK) {
                const char* err = lua_tostring(_L, -1);
                LabLog::error(std::string("[Lua OnTriggerEnter Error] ") + (err ? err : "unknown"));
                lua_pop(_L, 1);
            }
        } else {
            lua_pop(_L, 1);
        }
    }

    DamageResult ScriptEngine::calculateDamage(float incomingDamage, float currentArmor, float currentHealth) {
        DamageResult res;

        if (_L) {
            lua_getglobal(_L, "CalculateDamage");
            if (lua_isfunction(_L, -1)) {
                lua_pushnumber(_L, incomingDamage);
                lua_pushnumber(_L, currentArmor);
                lua_pushnumber(_L, currentHealth);
                if (lua_pcall(_L, 3, 3, 0) == LUA_OK) {
                    res.finalDamage = (float)lua_tonumber(_L, -3);
                    res.absorbedByArmor = (float)lua_tonumber(_L, -2);
                    res.isLethal = lua_toboolean(_L, -1) != 0;
                    lua_pop(_L, 3);
                    return res;
                } else {
                    lua_pop(_L, 1); // pop error message
                }
            } else {
                lua_pop(_L, 1);
            }
        }

        // C++ fallback calculation if Lua call failed
        if (currentArmor > 0.0f) {
            res.absorbedByArmor = std::min(currentArmor, incomingDamage * _armorAbsorptionRatio);
        }
        res.finalDamage = incomingDamage - res.absorbedByArmor;
        res.isLethal = (currentHealth - res.finalDamage <= 0.0f);
        return res;
    }

    const WeaponScriptDef* ScriptEngine::getWeaponDefById(int id) const {
        for (const auto& w : _weaponDefs) {
            if (w.id == id) return &w;
        }
        return nullptr;
    }

    std::string ScriptEngine::getWeaponNameById(int id) const {
        const auto* def = getWeaponDefById(id);
        if (def && !def->name.empty()) return def->name;
        const char* names[9] = { "Pipe", "Pistol", "Shotgun", "M4A4-S", "SG553", "Minigun", "Plasma Gun", "Railgun", "RPG" };
        if (id >= 0 && id < 9) return names[id];
        return "Weapon";
    }

} // namespace Lab
