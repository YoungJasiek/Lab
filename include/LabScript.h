#pragma once
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include "LabMath.h"

// Forward declaration of Lua state
struct lua_State;

namespace Lab {

    struct DamageResult {
        float finalDamage = 0.0f;
        float absorbedByArmor = 0.0f;
        bool isLethal = false;
    };

    struct WeaponScriptDef {
        int id = 0;
        std::string key;
        std::string name;
        float damage = 20.0f;
        float fireRate = 0.2f;
        int clipSize = 20;
        int maxReserve = 120;
        bool isMelee = false;
        bool isAutomatic = false;
        bool isProjectile = false;
        float splashDamage = 0.0f;
        float splashRadius = 0.0f;
        int slot = 1;
        float recoilPitch = 0.02f;
        float recoilKick = 0.04f;
    };

    class ScriptEngine {
    public:
        ScriptEngine();
        ~ScriptEngine();

        // Non-copyable, movable (RAII)
        ScriptEngine(const ScriptEngine&) = delete;
        ScriptEngine& operator=(const ScriptEngine&) = delete;
        ScriptEngine(ScriptEngine&& other) noexcept;
        ScriptEngine& operator=(ScriptEngine&& other) noexcept;

        bool init(const std::string& scriptPath = "assets/scripts/game_mechanics.lua");
        void shutdown();
        bool reload();

        bool executeString(const std::string& luaCode);

        // --- Game Mechanics Queried from Lua ---
        float getPlayerMaxHealth() const { return _maxHealth; }
        float getPlayerStartArmor() const { return _startArmor; }
        float getArmorAbsorptionRatio() const { return _armorAbsorptionRatio; }
        float getPlayerRespawnTime() const { return _respawnTime; }
        float getBarrelDamageMultiplier() const { return _barrelDamageMultiplier; }

        // Retinal Scanner mechanics
        float getRetinalScanDuration() const { return _retinalScanDuration; }
        const std::string& getRetinalAuthorizedUser() const { return _retinalAuthorizedUser; }
        int getRetinalClearanceLevel() const { return _retinalClearanceLevel; }

        // Evaluates incoming damage through Lua function
        DamageResult calculateDamage(float incomingDamage, float currentArmor, float currentHealth);

        // Weapon definitions loaded from Lua
        const std::vector<WeaponScriptDef>& getWeaponDefinitions() const { return _weaponDefs; }
        const WeaponScriptDef* getWeaponDefById(int id) const;

        // Pickup mechanics
        float getMedkitHealAmount() const { return _medkitHeal; }
        int getAmmoBoxAmount() const { return _ammoBoxAmount; }
        float getWeaponPadRespawnTime() const { return _weaponPadRespawnTime; }

        // Callbacks from Lua
        static void registerDoorUnlockCallback(std::function<void(int)> callback);
        static void registerServerChatCallback(std::function<void(const std::string&, const std::string&)> callback);

        bool isLoaded() const { return _L != nullptr; }

    private:
        lua_State* _L = nullptr;
        std::string _currentScriptPath;

        // Cached Lua game parameters
        float _maxHealth = 150.0f;
        float _startArmor = 50.0f;
        float _armorAbsorptionRatio = 0.70f;
        float _respawnTime = 4.0f;
        float _barrelDamageMultiplier = 0.75f;

        float _retinalScanDuration = 1.25f;
        std::string _retinalAuthorizedUser = "DR. VANCE";
        int _retinalClearanceLevel = 3;

        float _medkitHeal = 50.0f;
        int _ammoBoxAmount = 36;
        float _weaponPadRespawnTime = 60.0f;

        std::vector<WeaponScriptDef> _weaponDefs;

        void registerEngineBindings();
        void cacheGameplayTables();
        void loadEmbeddedFallbackScript();
    };

} // namespace Lab
