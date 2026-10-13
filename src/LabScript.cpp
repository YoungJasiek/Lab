#include "LabScript.h"
#include "LabCore.h"
#include "LabAudio.h"
#include "LabInput.h"
#include "LabMap.h"
#include "LabScene.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cstring>
#include <unordered_map>
#include <filesystem>

#ifndef _WIN32
    #include <strings.h>
    #define _stricmp strcasecmp
#endif

extern "C" {
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
}

namespace Lab {

    static LabMap* s_activeMap = nullptr;

    // Service-locator for the host-facing static API: points at the runtime state of the
    // most recently initialised engine. Lua bindings never use it; they resolve their own engine.
    static std::shared_ptr<ScriptRuntimeState>& currentRuntimeStatePtr() {
        static std::shared_ptr<ScriptRuntimeState> s_current = std::make_shared<ScriptRuntimeState>();
        return s_current;
    }

    static ScriptRuntimeState& currentRuntimeState() {
        return *currentRuntimeStatePtr();
    }

    static ScriptEngine* getEngineFromLua(lua_State* L) {
        if (!L) return nullptr;
        lua_getfield(L, LUA_REGISTRYINDEX, "__lab_script_engine");
        auto* eng = static_cast<ScriptEngine*>(lua_touserdata(L, -1));
        lua_pop(L, 1);
        return eng;
    }

    static ScriptRuntimeState& runtimeStateForLua(lua_State* L) {
        if (ScriptEngine* eng = getEngineFromLua(L)) return eng->runtimeState();
        return currentRuntimeState();
    }

    static LabMap* getActiveMapForLua(lua_State* L) {
        if (ScriptEngine* eng = getEngineFromLua(L)) {
            if (eng->getActiveMap()) return eng->getActiveMap();
        }
        return s_activeMap;
    }

    void ScriptEngine::registerDoorUnlockCallback(std::function<void(int)> callback) {
        currentRuntimeState().doorUnlockCallback = std::move(callback);
    }

    void ScriptEngine::registerServerChatCallback(std::function<void(const std::string&, const std::string&)> callback) {
        currentRuntimeState().serverChatCallback = std::move(callback);
    }

    void ScriptEngine::registerCameraSwitchCallback(std::function<void(const std::string&)> callback) {
        currentRuntimeState().cameraSwitchCallback = std::move(callback);
    }

    void ScriptEngine::setActiveCameraName(const std::string& name) {
        auto& st = currentRuntimeState();
        st.activeCameraName = name;
        if (st.cameraSwitchCallback) st.cameraSwitchCallback(name);
    }

    const std::string& ScriptEngine::getActiveCameraName() {
        return currentRuntimeState().activeCameraName;
    }

    void ScriptEngine::setActiveMap(LabMap* map) {
        _activeMap = map;
        s_activeMap = map;
        if (!map) return;

        // Auto-run scripts attached to map entities (brush, prop, door, camera, sprite)
        auto runScriptIfAny = [this](const std::string& sPath) {
            if (sPath.empty()) return;
            std::filesystem::path normPath = std::filesystem::path(sPath).lexically_normal();
            if (normPath.has_root_name()) return;
            for (const auto& part : normPath) {
                if (part == "..") return;
            }
            std::string resolved = normPath.string();
            if (!std::filesystem::exists(resolved)) {
                if (std::filesystem::exists("assets/scripts/" + resolved)) resolved = "assets/scripts/" + resolved;
                else if (std::filesystem::exists("../assets/scripts/" + resolved)) resolved = "../assets/scripts/" + resolved;
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
        currentRuntimeState().hudTexts[id] = text;
    }

    std::string ScriptEngine::getHUDText(const std::string& id) {
        const auto& m = currentRuntimeState().hudTexts;
        auto it = m.find(id);
        return it != m.end() ? it->second : "";
    }

    void ScriptEngine::setHUDValue(const std::string& id, float val) {
        currentRuntimeState().hudValues[id] = val;
    }

    float ScriptEngine::getHUDValue(const std::string& id) {
        const auto& m = currentRuntimeState().hudValues;
        auto it = m.find(id);
        return it != m.end() ? it->second : 0.0f;
    }

    void ScriptEngine::setHUDVisible(const std::string& id, bool visible) {
        currentRuntimeState().hudVisibles[id] = visible;
    }

    bool ScriptEngine::isHUDVisible(const std::string& id) {
        const auto& m = currentRuntimeState().hudVisibles;
        auto it = m.find(id);
        return it != m.end() ? it->second : true;
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
            runtimeStateForLua(L).hudTexts[id] = text;
        }
        return 0;
    }

    static int lua_HUDGetText(lua_State* L) {
        const char* id = luaL_checkstring(L, 1);
        std::string res;
        if (id) { const auto& m = runtimeStateForLua(L).hudTexts; auto it = m.find(id); if (it != m.end()) res = it->second; }
        lua_pushstring(L, res.c_str());
        return 1;
    }

    static int lua_HUDSetValue(lua_State* L) {
        const char* id = luaL_checkstring(L, 1);
        float val = (float)luaL_checknumber(L, 2);
        if (id) {
            runtimeStateForLua(L).hudValues[id] = val;
        }
        return 0;
    }

    static int lua_HUDGetValue(lua_State* L) {
        const char* id = luaL_checkstring(L, 1);
        float val = 0.0f;
        if (id) { const auto& m = runtimeStateForLua(L).hudValues; auto it = m.find(id); if (it != m.end()) val = it->second; }
        lua_pushnumber(L, (lua_Number)val);
        return 1;
    }

    static int lua_HUDSetVisible(lua_State* L) {
        const char* id = luaL_checkstring(L, 1);
        bool vis = lua_toboolean(L, 2);
        if (id) {
            runtimeStateForLua(L).hudVisibles[id] = vis;
        }
        return 0;
    }

    static int lua_HUDIsVisible(lua_State* L) {
        const char* id = luaL_checkstring(L, 1);
        bool vis = true;
        if (id) { const auto& m = runtimeStateForLua(L).hudVisibles; auto it = m.find(id); if (it != m.end()) vis = it->second; }
        lua_pushboolean(L, vis);
        return 1;
    }

    static Vec3* resolveEntityPositionPtr(lua_State* L, const char* name) {
        if (!name) return nullptr;
        LabMap* map = getActiveMapForLua(L);
        if (!map) return nullptr;
        if (auto* scene = dynamic_cast<LabScene*>(map)) {
            if (Entity* ent = scene->FindByName(name)) {
                if (ent->transform) return &ent->transform->position;
            }
        }
        auto h = map->findEntityByName(name);
        return h.getPosition();
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

        if (Vec3* pos = resolveEntityPositionPtr(L, name)) {
            *pos = { x, y, z };
        }
        return 0;
    }

    static int lua_EntityGetPosition(lua_State* L) {
        const char* name = nullptr;
        lua_getfield(L, 1, "__name");
        if (lua_isstring(L, -1)) name = lua_tostring(L, -1);
        lua_pop(L, 1);

        float x = 0.0f, y = 0.0f, z = 0.0f;
        if (const Vec3* pos = resolveEntityPositionPtr(L, name)) {
            x = pos->x;
            y = pos->y;
            z = pos->z;
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

        if (Vec3* pos = resolveEntityPositionPtr(L, name)) {
            pos->x += dx;
            pos->y += dy;
            pos->z += dz;
        }
        return 0;
    }

    static int lua_EntityGetX(lua_State* L) {
        const char* name = nullptr;
        lua_getfield(L, 1, "__name");
        if (lua_isstring(L, -1)) name = lua_tostring(L, -1);
        lua_pop(L, 1);
        float x = 0.0f;
        if (const Vec3* pos = resolveEntityPositionPtr(L, name)) x = pos->x;
        lua_pushnumber(L, x);
        return 1;
    }

    static int lua_EntityGetY(lua_State* L) {
        const char* name = nullptr;
        lua_getfield(L, 1, "__name");
        if (lua_isstring(L, -1)) name = lua_tostring(L, -1);
        lua_pop(L, 1);
        float y = 0.0f;
        if (const Vec3* pos = resolveEntityPositionPtr(L, name)) y = pos->y;
        lua_pushnumber(L, y);
        return 1;
    }

    static int lua_EntityGetZ(lua_State* L) {
        const char* name = nullptr;
        lua_getfield(L, 1, "__name");
        if (lua_isstring(L, -1)) name = lua_tostring(L, -1);
        lua_pop(L, 1);
        float z = 0.0f;
        if (const Vec3* pos = resolveEntityPositionPtr(L, name)) z = pos->z;
        lua_pushnumber(L, z);
        return 1;
    }

    static int lua_EntitySetX(lua_State* L) {
        const char* name = nullptr;
        lua_getfield(L, 1, "__name");
        if (lua_isstring(L, -1)) name = lua_tostring(L, -1);
        lua_pop(L, 1);
        float x = (float)luaL_checknumber(L, 2);
        if (Vec3* pos = resolveEntityPositionPtr(L, name)) pos->x = x;
        return 0;
    }

    static int lua_EntitySetY(lua_State* L) {
        const char* name = nullptr;
        lua_getfield(L, 1, "__name");
        if (lua_isstring(L, -1)) name = lua_tostring(L, -1);
        lua_pop(L, 1);
        float y = (float)luaL_checknumber(L, 2);
        if (Vec3* pos = resolveEntityPositionPtr(L, name)) pos->y = y;
        return 0;
    }

    static int lua_EntitySetZ(lua_State* L) {
        const char* name = nullptr;
        lua_getfield(L, 1, "__name");
        if (lua_isstring(L, -1)) name = lua_tostring(L, -1);
        lua_pop(L, 1);
        float z = (float)luaL_checknumber(L, 2);
        if (Vec3* pos = resolveEntityPositionPtr(L, name)) pos->z = z;
        return 0;
    }

    static int lua_WorldGetEntity(lua_State* L) {
        const char* name = luaL_checkstring(L, 1);
        if (!name || !resolveEntityPositionPtr(L, name)) {
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

        if (Vec3* pos = resolveEntityPositionPtr(L, name)) {
            pos->x += dx;
            pos->y += dy;
            pos->z += dz;
        }
        return 0;
    }

    // --- Camera Lua Callbacks ---
    static int lua_CameraSetActive(lua_State* L) {
        const char* name = luaL_checkstring(L, 1);
        if (name) {
            { auto& st = runtimeStateForLua(L); st.activeCameraName = name; if (st.cameraSwitchCallback) st.cameraSwitchCallback(name); }
            if (s_activeMap) {
                for (auto& c : s_activeMap->cameras) {
                    c.active = (c.name == name);
                }
            }
        }
        return 0;
    }

    static int lua_CameraGetActive(lua_State* L) {
        lua_pushstring(L, runtimeStateForLua(L).activeCameraName.c_str());
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
        float val = 0.0f;
        if (name) {
            if (_stricmp(name, "MouseX") == 0) {
                val = Input::GetAxis("MouseX");
            } else if (_stricmp(name, "MouseY") == 0) {
                val = Input::GetAxis("MouseY");
            } else {
                val = InputActionManager::instance().getAxis(name);
            }
        }
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
        if (auto& st = runtimeStateForLua(L); st.doorUnlockCallback) {
            st.doorUnlockCallback(doorIndex);
        }
        return 0;
    }

    static int lua_AddChatMessage(lua_State* L) {
        const char* sender = luaL_checkstring(L, 1);
        const char* text = luaL_checkstring(L, 2);
        if (auto& st = runtimeStateForLua(L); st.serverChatCallback && sender && text) {
            st.serverChatCallback(sender, text);
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
          _state(std::move(other._state)),
          _currentScriptPath(std::move(other._currentScriptPath)),
          _activeMap(other._activeMap),
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
        other._activeMap = nullptr;
        if (_L) {
            lua_pushlightuserdata(_L, this);
            lua_setfield(_L, LUA_REGISTRYINDEX, "__lab_script_engine");
        }
    }

    ScriptEngine& ScriptEngine::operator=(ScriptEngine&& other) noexcept {
        if (this != &other) {
            shutdown();
            _L = other._L;
            other._L = nullptr;
            _state = std::move(other._state);
            _currentScriptPath = std::move(other._currentScriptPath);
            _activeMap = other._activeMap;
            other._activeMap = nullptr;
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
            if (_L) {
                lua_pushlightuserdata(_L, this);
                lua_setfield(_L, LUA_REGISTRYINDEX, "__lab_script_engine");
            }
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

        // --- Register Lua ECS, Scene, Camera and Math Bindings ---
        const char* ecsLuaInit = R"(
            quat = {
                fromEuler = function(pitch, yaw, roll)
                    pitch = pitch or 0
                    yaw = yaw or 0
                    roll = roll or 0
                    local degToRad = 3.14159265 / 180.0
                    local p = pitch * degToRad * 0.5
                    local y = yaw * degToRad * 0.5
                    local r = roll * degToRad * 0.5
                    local cp = math.cos(p)
                    local sp = math.sin(p)
                    local cy = math.cos(y)
                    local sy = math.sin(y)
                    local cr = math.cos(r)
                    local sr = math.sin(r)
                    return {
                        x = sp * cy * cr + cp * sy * sr,
                        y = cp * sy * cr - sp * cy * sr,
                        z = cp * cy * sr + sp * sy * cr,
                        w = cp * cy * cr - sp * sy * sr
                    }
                end
            }
            Quat = quat

            Vec3 = {
                Lerp = function(a, b, t)
                    t = math.max(0, math.min(1, t))
                    return {
                        a[1] + (b[1] - a[1]) * t,
                        a[2] + (b[2] - a[2]) * t,
                        a[3] + (b[3] - a[3]) * t
                    }
                end,
                lerp = function(a, b, t) return Vec3.Lerp(a, b, t) end
            }

            input = {
                mousePosition = { 0, 0, 0 },
                getAxis = function(name)
                    if Input and Input.getAxis then
                        return Input.getAxis(name)
                    end
                    return 0.0
                end
            }

            local _entitiesByTag = {}
            local _entitiesByName = {}

            function _createEntityTable(name, tag)
                local ent = {
                    name = name or "Entity",
                    tag = tag or "",
                    transform = {
                        position = {0, 0, 0},
                        rotation = {0, 0, 0, 1},
                        localRotation = {0, 0, 0, 1},
                        scale = {1, 1, 1},
                        lookAt = function(self, target)
                            local dx = (target[1] or 0) - (self.position[1] or 0)
                            local dy = (target[2] or 0) - (self.position[2] or 0)
                            local dz = (target[3] or 0) - (self.position[3] or 0)
                            local len = math.sqrt(dx*dx + dy*dy + dz*dz)
                            if len > 0.0001 then
                                local pitch = math.asin(math.max(-1, math.min(1, dy / len))) * (180.0 / 3.14159265)
                                local yaw = math.atan2(-dx, -dz) * (180.0 / 3.14159265)
                                self.localRotation = quat.fromEuler(pitch, yaw, 0)
                                self.rotation = self.localRotation
                            end
                        end,
                        LookAt = function(self, target) self:lookAt(target) end,
                        rotate = function(self, axis, angleDeg)
                        end,
                        Rotate = function(self, axis, angleDeg) self:rotate(axis, angleDeg) end
                    },
                    _components = {}
                }

                function ent:getComponent(compName)
                    if compName == "Camera" then
                        if not self._components["Camera"] then
                            self._components["Camera"] = {
                                fieldOfView = 60.0,
                                nearClipPlane = 0.3,
                                farClipPlane = 1000.0,
                                orthographic = false,
                                orthographicSize = 5.0,
                                backgroundColor = {0, 0, 0, 1},
                                clearFlags = "SolidColor",
                                cullingMask = 0xFFFFFFFF,
                                depth = 0,
                                rect = {0.0, 0.0, 1.0, 1.0},
                                targetTexture = 0,
                                screenToWorldPoint = function(self, screenPos)
                                    return { screenPos[1] or 0, screenPos[2] or 0, screenPos[3] or 10 }
                                end,
                                ScreenToWorldPoint = function(self, sp) return self:screenToWorldPoint(sp) end,
                                worldToScreenPoint = function(self, worldPos)
                                    return { worldPos[1] or 0, worldPos[2] or 0, worldPos[3] or 0 }
                                end,
                                WorldToScreenPoint = function(self, wp) return self:worldToScreenPoint(wp) end,
                                viewportPointToRay = function(self, vp)
                                    return { origin = ent.transform.position, direction = {0, 0, -1} }
                                end,
                                ViewportPointToRay = function(self, vp) return self:viewportPointToRay(vp) end
                            }
                        end
                        return self._components["Camera"]
                    end
                    return self._components[compName]
                end
                function ent:GetComponent(compName) return self:getComponent(compName) end
                function ent:addComponent(compName) return self:getComponent(compName) end
                function ent:AddComponent(compName) return self:getComponent(compName) end
                function ent:addTag(t)
                    self.tag = t
                    _entitiesByTag[t] = self
                end
                function ent:AddTag(t) self:addTag(t) end

                return ent
            end

            scene = scene or {}
            function scene.findByTag(selfOrTag, maybeTag)
                local tag = maybeTag or selfOrTag
                if not _entitiesByTag[tag] then
                    _entitiesByTag[tag] = _createEntityTable("Entity_" .. tostring(tag), tag)
                end
                return _entitiesByTag[tag]
            end
            scene.FindByTag = scene.findByTag

            function scene.findByName(selfOrName, maybeName)
                local name = maybeName or selfOrName
                if not _entitiesByName[name] then
                    _entitiesByName[name] = _createEntityTable(name, "")
                end
                return _entitiesByName[name]
            end
            scene.FindByName = scene.findByName

            function scene.createEntity(selfOrName, maybeName)
                local name = maybeName or selfOrName or "Entity"
                local e = _createEntityTable(name, "")
                _entitiesByName[name] = e
                return e
            end
            scene.CreateEntity = scene.createEntity

            entity = _createEntityTable("DefaultEntity", "")
            _entitiesByTag["MainCamera"] = _createEntityTable("MainCamera", "MainCamera")
        )";
        luaL_dostring(_L, ecsLuaInit);
    }

    void ScriptEngine::loadEmbeddedFallbackScript() {
        const char* fallbackLua = R"(
            -- ==========================================================
            -- Lab Engine: Universal Gameplay Mechanics Script (Lua 5.4)
            -- ==========================================================

            PlayerRules = {
                maxHealth = 100.0,
                startSuitArmor = 0.0,
                armorAbsorptionRatio = 0.70,
                respawnTime = 3.0,
                barrelDamageMultiplier = 1.0
            }

            RetinalScanner = {
                scanDuration = 1.0,
                authorizedUser = "SECURITY_ADMIN",
                clearanceLevel = 1,
                targetDoorIndex = 0
            }

            Pickups = {
                MedkitHeal = 50.0,
                AmmoBoxAmount = 30,
                WeaponPadRespawnTime = 30.0
            }

            Weapons = {
                Item0 = { id = 0, name = "WEAPON_0", damage = 50.0, range = 2.5, fireRate = 0.5, isMelee = true, slot = 1, recoilPitch = 0.02, recoilKick = 0.03 },
                Item1 = { id = 1, name = "WEAPON_1", damage = 25.0, fireRate = 0.2, clipSize = 15, reserve = 150, slot = 2, recoilPitch = 0.03, recoilKick = 0.04 },
                Item2 = { id = 2, name = "WEAPON_2", damage = 80.0, fireRate = 0.8, clipSize = 8, reserve = 64, slot = 3, recoilPitch = 0.08, recoilKick = 0.10 }
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
                Lab.playSound(25)
                Lab.addChatMessage("[SECURITY]", "Access granted: Door " .. tostring(linkedDoorIndex) .. " unlocked.")
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

        lua_pushlightuserdata(_L, this);
        lua_setfield(_L, LUA_REGISTRYINDEX, "__lab_script_engine");

        if (!_state) {
            // Fresh per-engine state; inherit host callbacks registered before init.
            _state = std::make_shared<ScriptRuntimeState>();
            const ScriptRuntimeState& prev = currentRuntimeState();
            _state->doorUnlockCallback = prev.doorUnlockCallback;
            _state->serverChatCallback = prev.serverChatCallback;
            _state->cameraSwitchCallback = prev.cameraSwitchCallback;
        }
        currentRuntimeStatePtr() = _state;

        luaL_openlibs(_L);

        // Sandbox Lua VM: strip dangerous OS command execution and raw host filesystem access
        lua_getglobal(_L, "os");
        if (lua_istable(_L, -1)) {
            const char* blockedOs[] = { "execute", "remove", "rename", "exit", "setenv", "tmpname" };
            for (const char* fn : blockedOs) {
                lua_pushnil(_L);
                lua_setfield(_L, -2, fn);
            }
        }
        lua_pop(_L, 1);

        lua_pushnil(_L);
        lua_setglobal(_L, "io");
        lua_pushnil(_L);
        lua_setglobal(_L, "dofile");
        lua_pushnil(_L);
        lua_setglobal(_L, "loadfile");

        lua_getglobal(_L, "package");
        if (lua_istable(_L, -1)) {
            lua_pushnil(_L);
            lua_setfield(_L, -2, "loadlib");
        }
        lua_pop(_L, 1);

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
        return "WEAPON_" + std::to_string(id);
    }

} // namespace Lab
