#pragma once
#include <vector>
#include <string>
#include "LabMath.h"
#include "LabMap.h"
#include "LabCombat.h"
#include "LabSession.h"
#include "LabSkeletal.h"
#include "LabRenderer.h"
#include "LabWeapon.h"
#include "LabStudio.h"

namespace Lab {

    enum class AIState {
        Patrol,
        Alert,
        Chase,
        Attack,
        Hurt,
        Dead
    };

    class CombatBot {
    public:
        int id = 0;
        std::string name = "Combat Synth";
        Vec3 position{ 0.0f, 0.0f, 0.0f };
        Vec3 rotation{ 0.0f, 0.0f, 0.0f }; // yaw in rotation.y
        float health = 100.0f;
        float maxHealth = 100.0f;
        int team = -1; // -1 = FFA, 0 = Red Team, 1 = Blue Team
        AIState state = AIState::Patrol;
        WeaponID equippedWeapon = WeaponID::Pipe;

        // Patrol waypoints
        Vec3 patrolStart{ 0.0f, 0.0f, 0.0f };
        Vec3 patrolEnd{ 0.0f, 0.0f, 0.0f };
        float patrolT = 0.0f;
        int patrolDir = 1;
        float moveSpeed = 3.2f;

        // Combat tuning
        float sightRange = 28.0f;
        float attackRange = 22.0f;
        float shootInterval = 0.75f;
        float shootCooldown = 0.0f;
        float reactionTimer = 0.25f;
        float hurtTimer = 0.0f;
        float muzzleFlashTimer = 0.0f;
        float shootAnimTimer = 0.0f;
        float reloadTimer = 0.0f;
        int ammoInClip = 30;
        int maxClipAmmo = 30;
        float deathTimer = 0.0f;
        float walkCycle = 0.0f;
        int kills = 0;
        int deaths = 0;
        float respawnTimer = 0.0f;
        float strafeTimer = 0.0f;
        int strafeDirection = 1; // -1 = Left, +1 = Right

        // glTF 2.0 Skeletal Animation Controller
        Animator animator;

        // Kinetic momentum & ragdoll knockback
        Vec3 velocity{ 0.0f, 0.0f, 0.0f };
        void applyImpulse(const Vec3& impulse) { velocity += impulse; }

        CombatBot() = default;
        CombatBot(int botId, const std::string& botName, const Vec3& spawnPos, const Vec3& pEnd, int botTeam = -1, WeaponID weapon = WeaponID::Pipe);

        void initAnimation(std::shared_ptr<Skeleton> skel, const std::vector<AnimationClip>& clips);
        bool isAlive() const { return health > 0.0f && state != AIState::Dead; }

        // Bounding boxes for headshot and torso hitscan
        void getHitboxes(Vec3& headMin, Vec3& headMax, Vec3& bodyMin, Vec3& bodyMax) const;

        // Line of sight check against world geometry
        static bool hasLineOfSight(const Vec3& from, const Vec3& to, const LabMap& map);

        void update(float dt, const Vec3& playerPos, const LabMap& map, std::vector<BulletTracer>& outTracers, float& outDamageToPlayer);
        bool takeDamage(float damage, bool isHeadshot, const Vec3& knockback = Vec3(0.0f, 0.0f, 0.0f));
        void render(const SkinnedMesh* mesh = nullptr, const Mesh* weaponMesh = nullptr,
                    const BotWeaponConfig* botWepCfg = nullptr, const Texture* weaponTex = nullptr) const;
        void renderShadow(const SkinnedMesh* mesh = nullptr, const Mesh* weaponMesh = nullptr,
                          const BotWeaponConfig* botWepCfg = nullptr) const;
    };

    class PickupManager;
    class LabChat;

    class AIManager {
    public:
        std::vector<CombatBot> bots;
        std::shared_ptr<Skeleton> skeleton;
        std::vector<AnimationClip> animations;
        std::unique_ptr<SkinnedMesh> skinnedMesh;
        std::unique_ptr<Mesh> weaponMesh;
        std::array<std::unique_ptr<Mesh>, 9> weaponMeshes;
        std::array<std::unique_ptr<Texture>, 9> weaponTextures;
        std::array<BotWeaponConfig, 9> botWeaponConfigs;

        void initAssets();
        void loadConfig(const std::string& path = "assets/configs/character_studio.cfg");
        void setBotWeaponConfig(WeaponID id, const BotWeaponConfig& cfg);
        const BotWeaponConfig& getBotWeaponConfig(WeaponID id) const;

        void clear() { bots.clear(); }
        void spawnBotsForMap(const LabMap* map, int count, GameMode mode);
        void spawnBotsForMap(const std::string& mapName, int count, GameMode mode);
        void update(float dt, const Vec3& playerPos, bool isPlayerAlive, int playerTeam,
                    const LabMap& map, std::vector<BulletTracer>& outTracers, float& outDamageToPlayer,
                    PickupManager* pickupMgr = nullptr, LabChat* chat = nullptr);
        void update(float dt, const Vec3& playerPos, const LabMap& map,
                    std::vector<BulletTracer>& outTracers, float& outDamageToPlayer) {
            update(dt, playerPos, true, -1, map, outTracers, outDamageToPlayer, nullptr, nullptr);
        }
        bool testRaycast(const Vec3& rayOrigin, const Vec3& rayDir, RaycastHit& outHit, int excludeBotId = -1);
        void renderShadowPass() const;
        void render() const;
    };

} // namespace Lab
