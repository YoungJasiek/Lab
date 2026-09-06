#pragma once
#include <vector>
#include <string>
#include <memory>
#include <array>
#include <cmath>
#include <algorithm>
#include <iostream>
#include "LabMath.h"
#include "LabCamera.h"
#include "LabCombat.h"
#include "LabAnim.h"

namespace Lab {

    class Texture;
    class Mesh;
    class LabMap;
    class AIManager;

    enum class WeaponID : int {
        Pipe = 0,       // 1. Pipe (Melee / Broń biała) - Startowa
        Pistol = 1,     // 2. Pistolet (Tactical Pistol) - Startowy
        Shotgun = 2,    // 3. Strzelba (Tactical Shotgun)
        M4A4S = 3,      // 4. Karabin w stylu M4A4-S (Silenced Carbine)
        SG553 = 4,      // 5. Karabin SG553 (Scoped Battle Rifle)
        Minigun = 5,    // 6. Minigun (Rotary Vulcan Cannon)
        PlasmaGun = 6,  // 7. Plasma Gun (Pulse Energy Projectile)
        Railgun = 7,    // 8. Railgun (Gauss Kinetic Accelerator)
        RPG = 8,        // 9. RPG (Rocket Propelled Grenade)
        Count = 9
    };

    struct WeaponDef {
        WeaponID id = WeaponID::Pistol;
        int slot = 2; // 1 to 9
        std::string name = "Pistolet";
        std::string shortName = "PISTOL";
        std::string modelFile = "assets/models/pistol.stl";
        std::string textureFile = "weapon_pistol.bmp";
        
        bool isMelee = false;
        bool isProjectile = false;
        bool isAutomatic = false;

        float damage = 28.0f;
        float headshotMultiplier = 3.0f;
        float fireRate = 0.20f; // Seconds between attacks
        int bulletsPerShot = 1;
        float spread = 0.006f;
        float range = 150.0f;

        int clipSize = 12;
        int defaultReserve = 48;
        int maxReserve = 120;

        Vec3 tracerColor{ 1.0f, 0.95f, 0.45f };
        float tracerThickness = 0.025f;
        float tracerLifetime = 0.08f;

        float recoilPitch = 0.035f;
        float recoilKick = 0.12f;

        // Projectile specific parameters (Plasma / RPG)
        float projectileSpeed = 0.0f;
        float splashRadius = 0.0f;
        float splashDamage = 0.0f;
    };

    struct Projectile {
        Vec3 position{ 0.0f, 0.0f, 0.0f };
        Vec3 velocity{ 0.0f, 0.0f, 0.0f };
        WeaponID weaponId = WeaponID::RPG;
        float damage = 140.0f;
        float splashRadius = 5.0f;
        float splashDamage = 100.0f;
        Vec3 color{ 1.0f, 0.5f, 0.1f };
        float lifetime = 0.0f;
        float maxLifetime = 5.0f;
        bool active = true;
    };

    class Weapon {
    public:
        WeaponDef def;
        int currentClip = 0;
        int currentReserve = 0;
        bool unlocked = false;

        Weapon() = default;
        explicit Weapon(const WeaponDef& definition, bool startUnlocked = false);

        bool canFire(float cooldownTimer) const;
        bool needsReload() const;
        bool canReload() const;
        int reload();
        void addAmmo(int amount);
    };

    class WeaponSystem {
    public:
        WeaponSystem();

        void init();
        void reset();

        bool switchWeapon(WeaponID id);
        bool nextWeapon(); // Mouse scroll down
        bool prevWeapon(); // Mouse scroll up
        bool quickSwitch(); // Q key
        bool equipSlot(int slotNumber); // 1..9

        void unlockWeapon(WeaponID id, bool autoEquip = false);
        void unlockAll();
        bool isUnlocked(WeaponID id) const;

        Weapon& getActiveWeapon();
        const Weapon& getActiveWeapon() const;
        const WeaponDef& getActiveDef() const;
        WeaponID getActiveId() const { return _currentWeapon; }
        WeaponID getPreviousId() const { return _previousWeapon; }

        Weapon& getWeapon(WeaponID id);
        const Weapon& getWeapon(WeaponID id) const;

        float getFireCooldown() const { return _fireCooldown; }
        void setFireCooldown(float cd) { _fireCooldown = cd; }

        float getSwitchTimer() const { return _switchTimer; }
        float getHudSelectorTimer() const { return _hudSelectorTimer; }

        float getMinigunSpinAngle() const { return _minigunSpinAngle; }
        void addMinigunSpin(float deltaSpin) { _minigunSpinAngle += deltaSpin; }

        std::vector<Projectile>& getProjectiles() { return _projectiles; }
        const std::vector<Projectile>& getProjectiles() const { return _projectiles; }

        void update(float dt);
        void spawnProjectile(const Vec3& origin, const Vec3& direction);
        void applyScriptOverrides(int weaponId, float damage, float fireRate, int clipSize, int maxReserve, float splashDamage, float splashRadius);

        // Viewmodel procedural and STL renderer
        void renderViewModel(const Camera& camera, WeaponAnimator& animator,
                             Texture* texture, Mesh* stlMesh, float muzzleFlash);

        // Static factory of all 9 weapon definitions
        static std::array<WeaponDef, 9> createWeaponDefinitions();

    private:
        std::array<Weapon, 9> _weapons;
        WeaponID _currentWeapon = WeaponID::Pistol;
        WeaponID _previousWeapon = WeaponID::Pipe;
        float _fireCooldown = 0.0f;
        float _switchTimer = 0.0f;
        float _switchDuration = 0.25f;
        float _hudSelectorTimer = 0.0f;
        float _minigunSpinAngle = 0.0f;
        std::vector<Projectile> _projectiles;

        void drawProceduralWeapon(WeaponID id, const Vec3& basePos, const Vec3& rot, Texture* tex, float muzzleFlash);
    };

} // namespace Lab
