#pragma once
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <unordered_map>
#include "LabMath.h"

// Forward declaration of Lua state
struct lua_State;

namespace Lab {

    class LabMap;

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

    /// Per-engine mutable runtime state (HUD bridge, host callbacks, active camera).
    /// Owned by each ScriptEngine instance so several engines never share Lua-visible state.
    struct ScriptRuntimeState {
        std::unordered_map<std::string, std::string> hudTexts;
        std::unordered_map<std::string, float> hudValues;
        std::unordered_map<std::string, bool> hudVisibles;
        std::function<void(int)> doorUnlockCallback;
        std::function<void(const std::string&, const std::string&)> serverChatCallback;
        std::function<void(const std::string&)> cameraSwitchCallback;
        std::string activeCameraName;
    };

    class ScriptEngine {
    public:
        /// Runtime state of this engine instance (created lazily).
        ScriptRuntimeState& runtimeState() {
            if (!_state) _state = std::make_shared<ScriptRuntimeState>();
            return *_state;
        }
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
        bool executeBytecode(const std::vector<uint8_t>& bytecode, const std::string& chunkName = "bytecode");

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
        std::string getWeaponNameById(int id) const;

        // Pickup mechanics
        float getMedkitHealAmount() const { return _medkitHeal; }
        int getAmmoBoxAmount() const { return _ammoBoxAmount; }
        float getWeaponPadRespawnTime() const { return _weaponPadRespawnTime; }

        // Callbacks from Lua
        static void registerDoorUnlockCallback(std::function<void(int)> callback);
        static void registerServerChatCallback(std::function<void(const std::string&, const std::string&)> callback);
        static void registerCameraSwitchCallback(std::function<void(const std::string&)> callback);
        static void setActiveCameraName(const std::string& name);
        static const std::string& getActiveCameraName();

        // World & Scene Integration
        void setActiveMap(LabMap* map);
        LabMap* getActiveMap() const { return _activeMap; }

        // Per-frame script tick (invokes OnUpdate(dt) in Lua)
        void update(float dt);

        // Dynamic HUD State Bridge from Lua
        static void setHUDText(const std::string& id, const std::string& text);
        static std::string getHUDText(const std::string& id);
        static void setHUDValue(const std::string& id, float val);
        static float getHUDValue(const std::string& id);
        static void setHUDVisible(const std::string& id, bool visible);
        static bool isHUDVisible(const std::string& id);

        // Mission & Gameplay Rules queried from Lua
        int getMissionCardCount() const { return _missionCardCount; }
        float getMissionTimeLimit() const { return _missionTimeLimit; }
        const std::string& getMissionName() const { return _missionName; }
        const std::string& getMissionStartText() const { return _missionStartText; }

        std::pair<std::string, std::string> callMissionProgress(int cardsCollected, float timeLeft);
        void callOnTriggerEnter(int entityId, int otherEntityId);

        bool isLoaded() const { return _L != nullptr; }

    private:
        lua_State* _L = nullptr;
        std::shared_ptr<ScriptRuntimeState> _state;
        std::string _currentScriptPath;
        LabMap* _activeMap = nullptr;

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
        int _missionCardCount = 3;
        float _missionTimeLimit = 300.0f;
        std::string _missionName = "";
        std::string _missionStartText = "";

        void registerEngineBindings();
        void cacheGameplayTables();
        void loadEmbeddedFallbackScript();
    };

} // namespace Lab
