#include "LabScript.h"
#include "LabCore.h"
#include "LabAudio.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>

extern "C" {
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
}

namespace Lab {

    static std::function<void(int)> s_doorUnlockCallback = nullptr;
    static std::function<void(const std::string&, const std::string&)> s_serverChatCallback = nullptr;

    void ScriptEngine::registerDoorUnlockCallback(std::function<void(int)> callback) {
        s_doorUnlockCallback = callback;
    }

    void ScriptEngine::registerServerChatCallback(std::function<void(const std::string&, const std::string&)> callback) {
        s_serverChatCallback = callback;
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
          _weaponDefs(std::move(other._weaponDefs)) {
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

} // namespace Lab
