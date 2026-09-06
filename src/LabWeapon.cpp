#include "LabWeapon.h"
#include "LabRenderer.h"
#include "LabMap.h"
#include "LabAI.h"
#include "LabArms.h"
#include <cmath>
#include <iostream>

namespace Lab {

    Weapon::Weapon(const WeaponDef& definition, bool startUnlocked)
        : def(definition),
          currentClip(definition.clipSize),
          currentReserve(definition.defaultReserve),
          unlocked(startUnlocked) {
    }

    bool Weapon::canFire(float cooldownTimer) const {
        if (cooldownTimer > 0.0f) return false;
        if (def.isMelee) return true;
        return currentClip > 0;
    }

    bool Weapon::needsReload() const {
        if (def.isMelee) return false;
        return currentClip <= 0 && currentReserve > 0;
    }

    bool Weapon::canReload() const {
        if (def.isMelee) return false;
        return currentClip < def.clipSize && currentReserve > 0;
    }

    int Weapon::reload() {
        if (def.isMelee) return 0;
        int needed = def.clipSize - currentClip;
        int transfer = std::min(needed, currentReserve);
        currentClip += transfer;
        currentReserve -= transfer;
        return transfer;
    }

    void Weapon::addAmmo(int amount) {
        if (def.isMelee) return;
        currentReserve = std::min(def.maxReserve, currentReserve + amount);
    }

    std::array<WeaponDef, 9> WeaponSystem::createWeaponDefinitions() {
        std::array<WeaponDef, 9> defs;

        // 1. Pipe (Steel Pipe) - Slot 1 (Melee)
        defs[0].id = WeaponID::Pipe;
        defs[0].slot = 1;
        defs[0].name = "Pipe";
        defs[0].shortName = "PIPE";
        defs[0].modelFile = "assets/models/pipe.stl";
        defs[0].textureFile = "weapon_pipe.bmp";
        defs[0].isMelee = true;
        defs[0].isProjectile = false;
        defs[0].isAutomatic = false;
        defs[0].damage = 65.0f;
        defs[0].headshotMultiplier = 2.0f;
        defs[0].fireRate = 0.45f;
        defs[0].range = 2.6f;
        defs[0].clipSize = 0;
        defs[0].defaultReserve = 0;
        defs[0].maxReserve = 0;
        defs[0].recoilPitch = 0.02f;
        defs[0].recoilKick = 0.08f;

        // 2. Pistolet (Tactical Pistol) - Slot 2
        defs[1].id = WeaponID::Pistol;
        defs[1].slot = 2;
        defs[1].name = "Pistolet";
        defs[1].shortName = "PISTOL";
        defs[1].modelFile = "assets/models/pistol.stl";
        defs[1].textureFile = "weapon_pistol.bmp";
        defs[1].isMelee = false;
        defs[1].isProjectile = false;
        defs[1].isAutomatic = false;
        defs[1].damage = 28.0f;
        defs[1].headshotMultiplier = 3.0f;
        defs[1].fireRate = 0.20f;
        defs[1].bulletsPerShot = 1;
        defs[1].spread = 0.006f;
        defs[1].range = 150.0f;
        defs[1].clipSize = 12;
        defs[1].defaultReserve = 48;
        defs[1].maxReserve = 120;
        defs[1].tracerColor = { 1.0f, 0.95f, 0.45f };
        defs[1].tracerThickness = 0.025f;
        defs[1].tracerLifetime = 0.08f;
        defs[1].recoilPitch = 0.04f;
        defs[1].recoilKick = 0.14f;

        // 3. Strzelba (Tactical Shotgun) - Slot 3
        defs[2].id = WeaponID::Shotgun;
        defs[2].slot = 3;
        defs[2].name = "Strzelba";
        defs[2].shortName = "SHOTGUN";
        defs[2].modelFile = "assets/models/shotgun.stl";
        defs[2].textureFile = "weapon_shotgun.bmp";
        defs[2].isMelee = false;
        defs[2].isProjectile = false;
        defs[2].isAutomatic = false;
        defs[2].damage = 14.0f; // 8 pellets * 14 = 112 dmg
        defs[2].headshotMultiplier = 1.8f;
        defs[2].fireRate = 0.85f;
        defs[2].bulletsPerShot = 8;
        defs[2].spread = 0.055f;
        defs[2].range = 60.0f;
        defs[2].clipSize = 8;
        defs[2].defaultReserve = 32;
        defs[2].maxReserve = 64;
        defs[2].tracerColor = { 1.0f, 0.75f, 0.25f };
        defs[2].tracerThickness = 0.032f;
        defs[2].tracerLifetime = 0.09f;
        defs[2].recoilPitch = 0.08f;
        defs[2].recoilKick = 0.28f;

        // 4. Karabin w stylu M4A4-S - Slot 4
        defs[3].id = WeaponID::M4A4S;
        defs[3].slot = 4;
        defs[3].name = "M4A4-S";
        defs[3].shortName = "M4A4-S";
        defs[3].modelFile = "assets/models/m4a4s.stl";
        defs[3].textureFile = "weapon_m4a4s.bmp";
        defs[3].isMelee = false;
        defs[3].isProjectile = false;
        defs[3].isAutomatic = true;
        defs[3].damage = 34.0f;
        defs[3].headshotMultiplier = 3.0f;
        defs[3].fireRate = 0.095f; // 630 RPM
        defs[3].bulletsPerShot = 1;
        defs[3].spread = 0.012f;
        defs[3].range = 200.0f;
        defs[3].clipSize = 25;
        defs[3].defaultReserve = 100;
        defs[3].maxReserve = 200;
        defs[3].tracerColor = { 0.85f, 0.95f, 1.0f };
        defs[3].tracerThickness = 0.028f;
        defs[3].tracerLifetime = 0.08f;
        defs[3].recoilPitch = 0.032f;
        defs[3].recoilKick = 0.15f;

        // 5. Karabin SG553 - Slot 5
        defs[4].id = WeaponID::SG553;
        defs[4].slot = 5;
        defs[4].name = "SG553";
        defs[4].shortName = "SG553";
        defs[4].modelFile = "assets/models/sg553.stl";
        defs[4].textureFile = "weapon_sg553.bmp";
        defs[4].isMelee = false;
        defs[4].isProjectile = false;
        defs[4].isAutomatic = true;
        defs[4].damage = 40.0f;
        defs[4].headshotMultiplier = 3.0f;
        defs[4].fireRate = 0.11f; // 545 RPM
        defs[4].bulletsPerShot = 1;
        defs[4].spread = 0.007f;
        defs[4].range = 250.0f;
        defs[4].clipSize = 30;
        defs[4].defaultReserve = 90;
        defs[4].maxReserve = 180;
        defs[4].tracerColor = { 1.0f, 0.88f, 0.35f };
        defs[4].tracerThickness = 0.030f;
        defs[4].tracerLifetime = 0.08f;
        defs[4].recoilPitch = 0.038f;
        defs[4].recoilKick = 0.18f;

        // 6. Minigun - Slot 6
        defs[5].id = WeaponID::Minigun;
        defs[5].slot = 6;
        defs[5].name = "Minigun";
        defs[5].shortName = "MINIGUN";
        defs[5].modelFile = "assets/models/minigun.stl";
        defs[5].textureFile = "weapon_minigun.bmp";
        defs[5].isMelee = false;
        defs[5].isProjectile = false;
        defs[5].isAutomatic = true;
        defs[5].damage = 20.0f;
        defs[5].headshotMultiplier = 2.0f;
        defs[5].fireRate = 0.045f; // 1333 RPM
        defs[5].bulletsPerShot = 1;
        defs[5].spread = 0.042f;
        defs[5].range = 140.0f;
        defs[5].clipSize = 150;
        defs[5].defaultReserve = 300;
        defs[5].maxReserve = 600;
        defs[5].tracerColor = { 1.0f, 0.92f, 0.15f };
        defs[5].tracerThickness = 0.036f;
        defs[5].tracerLifetime = 0.07f;
        defs[5].recoilPitch = 0.022f;
        defs[5].recoilKick = 0.10f;

        // 7. Plasma Gun - Slot 7
        defs[6].id = WeaponID::PlasmaGun;
        defs[6].slot = 7;
        defs[6].name = "Plasma Gun";
        defs[6].shortName = "PLASMA";
        defs[6].modelFile = "assets/models/plasma.stl";
        defs[6].textureFile = "weapon_plasma.bmp";
        defs[6].isMelee = false;
        defs[6].isProjectile = true;
        defs[6].isAutomatic = true;
        defs[6].damage = 55.0f;
        defs[6].headshotMultiplier = 2.0f;
        defs[6].fireRate = 0.14f;
        defs[6].bulletsPerShot = 1;
        defs[6].spread = 0.005f;
        defs[6].range = 220.0f;
        defs[6].projectileSpeed = 85.0f;
        defs[6].splashRadius = 2.2f;
        defs[6].splashDamage = 30.0f;
        defs[6].clipSize = 40;
        defs[6].defaultReserve = 120;
        defs[6].maxReserve = 240;
        defs[6].tracerColor = { 0.2f, 0.9f, 1.0f };
        defs[6].tracerThickness = 0.065f;
        defs[6].tracerLifetime = 0.12f;
        defs[6].recoilPitch = 0.03f;
        defs[6].recoilKick = 0.14f;

        // 8. Railgun - Slot 8
        defs[7].id = WeaponID::Railgun;
        defs[7].slot = 8;
        defs[7].name = "Railgun";
        defs[7].shortName = "RAILGUN";
        defs[7].modelFile = "assets/models/railgun.stl";
        defs[7].textureFile = "weapon_railgun.bmp";
        defs[7].isMelee = false;
        defs[7].isProjectile = false;
        defs[7].isAutomatic = false;
        defs[7].damage = 160.0f; // Fatal penetration!
        defs[7].headshotMultiplier = 2.0f;
        defs[7].fireRate = 1.25f;
        defs[7].bulletsPerShot = 1;
        defs[7].spread = 0.0f;
        defs[7].range = 500.0f;
        defs[7].clipSize = 1;
        defs[7].defaultReserve = 15;
        defs[7].maxReserve = 30;
        defs[7].tracerColor = { 0.35f, 0.75f, 1.0f };
        defs[7].tracerThickness = 0.075f;
        defs[7].tracerLifetime = 0.25f;
        defs[7].recoilPitch = 0.12f;
        defs[7].recoilKick = 0.35f;

        // 9. RPG - Slot 9
        defs[8].id = WeaponID::RPG;
        defs[8].slot = 9;
        defs[8].name = "RPG";
        defs[8].shortName = "RPG";
        defs[8].modelFile = "assets/models/rpg.stl";
        defs[8].textureFile = "weapon_rpg.bmp";
        defs[8].isMelee = false;
        defs[8].isProjectile = true;
        defs[8].isAutomatic = false;
        defs[8].damage = 150.0f;
        defs[8].headshotMultiplier = 1.5f;
        defs[8].fireRate = 1.6f;
        defs[8].bulletsPerShot = 1;
        defs[8].spread = 0.008f;
        defs[8].range = 300.0f;
        defs[8].projectileSpeed = 42.0f;
        defs[8].splashRadius = 5.0f;
        defs[8].splashDamage = 100.0f;
        defs[8].clipSize = 1;
        defs[8].defaultReserve = 6;
        defs[8].maxReserve = 12;
        defs[8].tracerColor = { 1.0f, 0.45f, 0.1f };
        defs[8].tracerThickness = 0.09f;
        defs[8].tracerLifetime = 0.18f;
        defs[8].recoilPitch = 0.10f;
        defs[8].recoilKick = 0.30f;

        return defs;
    }

    WeaponSystem::WeaponSystem() {
        init();
    }

    void WeaponSystem::init() {
        auto defs = createWeaponDefinitions();
        for (size_t i = 0; i < 9; ++i) {
            // Starting weapons: Pistol and Pipe are granted at start
            bool startUnlocked = (i == (size_t)WeaponID::Pipe || i == (size_t)WeaponID::Pistol);
            _weapons[i] = Weapon(defs[i], startUnlocked);
        }
        _currentWeapon = WeaponID::Pistol;
        _previousWeapon = WeaponID::Pipe;
        _fireCooldown = 0.0f;
        _switchTimer = 0.0f;
        _hudSelectorTimer = 0.0f;
        _minigunSpinAngle = 0.0f;
        _projectiles.clear();
    }

    void WeaponSystem::reset() {
        init();
    }

    bool WeaponSystem::switchWeapon(WeaponID id) {
        int idx = (int)id;
        if (idx < 0 || idx >= 9) return false;
        if (!_weapons[idx].unlocked) return false;
        if (id == _currentWeapon) return true;

        _previousWeapon = _currentWeapon;
        _currentWeapon = id;
        _switchTimer = _switchDuration;
        _fireCooldown = _switchDuration;
        _hudSelectorTimer = 3.0f;
        return true;
    }

    bool WeaponSystem::nextWeapon() {
        int cur = (int)_currentWeapon;
        for (int step = 1; step < 9; ++step) {
            int nextIdx = (cur + step) % 9;
            if (_weapons[nextIdx].unlocked) {
                return switchWeapon((WeaponID)nextIdx);
            }
        }
        return false;
    }

    bool WeaponSystem::prevWeapon() {
        int cur = (int)_currentWeapon;
        for (int step = 1; step < 9; ++step) {
            int prevIdx = (cur - step + 9) % 9;
            if (_weapons[prevIdx].unlocked) {
                return switchWeapon((WeaponID)prevIdx);
            }
        }
        return false;
    }

    bool WeaponSystem::quickSwitch() {
        if (_previousWeapon != _currentWeapon && _weapons[(int)_previousWeapon].unlocked) {
            return switchWeapon(_previousWeapon);
        }
        return nextWeapon();
    }

    bool WeaponSystem::equipSlot(int slotNumber) {
        int idx = slotNumber - 1;
        if (idx >= 0 && idx < 9) {
            return switchWeapon((WeaponID)idx);
        }
        return false;
    }

    void WeaponSystem::unlockWeapon(WeaponID id, bool autoEquip) {
        int idx = (int)id;
        if (idx >= 0 && idx < 9) {
            _weapons[idx].unlocked = true;
            if (autoEquip) {
                switchWeapon(id);
            }
        }
    }

    void WeaponSystem::unlockAll() {
        for (auto& w : _weapons) {
            w.unlocked = true;
        }
        _hudSelectorTimer = 3.0f;
    }

    bool WeaponSystem::isUnlocked(WeaponID id) const {
        int idx = (int)id;
        return (idx >= 0 && idx < 9 && _weapons[idx].unlocked);
    }

    Weapon& WeaponSystem::getActiveWeapon() {
        return _weapons[(int)_currentWeapon];
    }

    const Weapon& WeaponSystem::getActiveWeapon() const {
        return _weapons[(int)_currentWeapon];
    }

    const WeaponDef& WeaponSystem::getActiveDef() const {
        return _weapons[(int)_currentWeapon].def;
    }

    Weapon& WeaponSystem::getWeapon(WeaponID id) {
        return _weapons[(int)id];
    }

    const Weapon& WeaponSystem::getWeapon(WeaponID id) const {
        return _weapons[(int)id];
    }

    void WeaponSystem::update(float dt) {
        if (_fireCooldown > 0.0f) _fireCooldown -= dt;
        if (_switchTimer > 0.0f) _switchTimer -= dt;
        if (_hudSelectorTimer > 0.0f) _hudSelectorTimer -= dt;

        // Update Projectiles
        for (auto it = _projectiles.begin(); it != _projectiles.end(); ) {
            it->lifetime += dt;
            it->position += it->velocity * dt;
            if (it->lifetime >= it->maxLifetime || !it->active) {
                it = _projectiles.erase(it);
            } else {
                ++it;
            }
        }
    }

    void WeaponSystem::spawnProjectile(const Vec3& origin, const Vec3& direction) {
        const auto& def = getActiveDef();
        if (!def.isProjectile) return;

        Projectile p;
        p.position = origin;
        p.velocity = direction.normalized() * def.projectileSpeed;
        p.weaponId = def.id;
        p.damage = def.damage;
        p.splashRadius = def.splashRadius;
        p.splashDamage = def.splashDamage;
        p.color = def.tracerColor;
        p.lifetime = 0.0f;
        p.maxLifetime = 6.0f;
        p.active = true;

        _projectiles.push_back(p);
    }

    void WeaponSystem::applyScriptOverrides(int weaponId, float damage, float fireRate, int clipSize, int maxReserve, float splashDamage, float splashRadius) {
        if (weaponId < 0 || weaponId >= 9) return;
        auto& wep = _weapons[weaponId];
        if (damage > 0.0f) wep.def.damage = damage;
        if (fireRate > 0.0f) wep.def.fireRate = fireRate;
        if (clipSize > 0) wep.def.clipSize = clipSize;
        if (maxReserve > 0) wep.def.maxReserve = maxReserve;
        if (splashDamage > 0.0f) wep.def.splashDamage = splashDamage;
        if (splashRadius > 0.0f) wep.def.splashRadius = splashRadius;
    }

    void WeaponSystem::drawProceduralWeapon(WeaponID id, const Vec3& basePos, const Vec3& rot, Texture* tex, float muzzleFlash, const Vec3& scale) {
        auto drawCube = [&](const Vec3& pos, const Vec3& r, const Vec3& size, const Vec3& col, Texture* t = nullptr, bool lit = true) {
            Vec3 rel = pos - basePos;
            Vec3 scaledPos = basePos + Vec3(rel.x * scale.x, rel.y * scale.y, rel.z * scale.z);
            Vec3 scaledSize = Vec3(size.x * scale.x, size.y * scale.y, size.z * scale.z);
            Renderer::drawCube(scaledPos, r, scaledSize, col, t, lit);
        };
        switch (id) {
            case WeaponID::Pipe: { // 1. PIPE (Steel Pipe)
                // Main heavy iron pipe angled diagonally
                Vec3 pRot = rot + Vec3(8.0f, -12.0f, 18.0f);
                drawCube(basePos + Vec3(0.02f, 0.04f, 0.05f), pRot, { 0.065f, 0.065f, 0.82f }, { 0.55f, 0.55f, 0.58f }, tex);
                // End coupling collar
                drawCube(basePos + Vec3(0.02f, 0.04f, -0.32f), pRot, { 0.082f, 0.082f, 0.08f }, { 0.45f, 0.45f, 0.48f }, tex);
                // Hand grip tape wrap
                drawCube(basePos + Vec3(0.02f, 0.04f, 0.28f), pRot, { 0.072f, 0.072f, 0.24f }, { 0.25f, 0.22f, 0.20f });
                break;
            }
            case WeaponID::Pistol: { // 2. PISTOLET (Tactical Pistol)
                Vec3 pRot = rot + Vec3(0.0f, -4.0f, 0.0f);
                // Slide
                drawCube(basePos + Vec3(0.0f, 0.02f, -0.04f), pRot, { 0.062f, 0.075f, 0.32f }, { 0.22f, 0.24f, 0.28f }, tex);
                // Front sight blade
                drawCube(basePos + Vec3(0.0f, 0.068f, -0.18f), pRot, { 0.015f, 0.022f, 0.02f }, { 0.9f, 0.2f, 0.2f });
                // Polymer grip handle
                drawCube(basePos + Vec3(0.0f, -0.09f, 0.06f), pRot + Vec3(16.0f, 0.0f, 0.0f), { 0.055f, 0.16f, 0.08f }, { 0.12f, 0.12f, 0.14f });
                // Trigger guard
                drawCube(basePos + Vec3(0.0f, -0.04f, 0.01f), pRot, { 0.04f, 0.05f, 0.06f }, { 0.18f, 0.18f, 0.20f });
                if (muzzleFlash > 0.0f) {
                    drawCube(basePos + Vec3(0.0f, 0.02f, -0.22f), pRot, { 0.12f, 0.12f, 0.12f }, { 1.0f, 0.9f, 0.3f }, nullptr, false);
                }
                break;
            }
            case WeaponID::Shotgun: { // 3. STRZELBA (Tactical Shotgun)
                Vec3 sRot = rot + Vec3(-2.0f, -4.0f, 0.0f);
                // Main heavy receiver
                drawCube(basePos + Vec3(0.0f, 0.02f, 0.05f), sRot, { 0.095f, 0.13f, 0.38f }, { 0.20f, 0.22f, 0.25f }, tex);
                // Upper barrel
                drawCube(basePos + Vec3(0.0f, 0.065f, -0.25f), sRot, { 0.065f, 0.065f, 0.52f }, { 0.18f, 0.18f, 0.20f }, tex);
                // Underbarrel magazine tube
                drawCube(basePos + Vec3(0.0f, 0.0f, -0.20f), sRot, { 0.055f, 0.055f, 0.44f }, { 0.25f, 0.26f, 0.28f });
                // Sliding pump forend
                drawCube(basePos + Vec3(0.0f, 0.0f, -0.16f), sRot, { 0.08f, 0.08f, 0.20f }, { 0.12f, 0.12f, 0.14f }, tex);
                // Stock / Grip
                drawCube(basePos + Vec3(0.0f, -0.08f, 0.18f), sRot + Vec3(18.0f, 0.0f, 0.0f), { 0.065f, 0.15f, 0.10f }, { 0.10f, 0.10f, 0.12f });
                if (muzzleFlash > 0.0f) {
                    drawCube(basePos + Vec3(0.0f, 0.065f, -0.54f), sRot, { 0.25f, 0.25f, 0.25f }, { 1.0f, 0.8f, 0.2f }, nullptr, false);
                }
                break;
            }
            case WeaponID::M4A4S: { // 4. M4A4-S (Silenced Carbine)
                Vec3 mRot = rot + Vec3(-1.0f, -4.0f, 0.0f);
                // Main receiver
                drawCube(basePos + Vec3(0.0f, 0.02f, 0.02f), mRot, { 0.085f, 0.12f, 0.42f }, { 0.16f, 0.18f, 0.22f }, tex);
                // Top carry handle / Picatinny rail
                drawCube(basePos + Vec3(0.0f, 0.09f, -0.04f), mRot, { 0.04f, 0.045f, 0.30f }, { 0.12f, 0.14f, 0.16f });
                // Curved 25-round magazine
                drawCube(basePos + Vec3(0.0f, -0.12f, 0.02f), mRot + Vec3(-12.0f, 0.0f, 0.0f), { 0.05f, 0.18f, 0.10f }, { 0.10f, 0.12f, 0.15f });
                // Handguard
                drawCube(basePos + Vec3(0.0f, 0.03f, -0.24f), mRot, { 0.075f, 0.085f, 0.22f }, { 0.18f, 0.20f, 0.22f }, tex);
                // Prominent cylindrical Silencer Suppressor!
                drawCube(basePos + Vec3(0.0f, 0.03f, -0.46f), mRot, { 0.068f, 0.068f, 0.26f }, { 0.08f, 0.10f, 0.12f }, tex);
                // Pistol grip
                drawCube(basePos + Vec3(0.0f, -0.10f, 0.16f), mRot + Vec3(18.0f, 0.0f, 0.0f), { 0.055f, 0.16f, 0.08f }, { 0.12f, 0.12f, 0.14f });
                // Discreet subsonic suppressed muzzle smoke/flash
                if (muzzleFlash > 0.0f) {
                    drawCube(basePos + Vec3(0.0f, 0.03f, -0.60f), mRot, { 0.08f, 0.08f, 0.08f }, { 0.7f, 0.85f, 1.0f }, nullptr, false);
                }
                break;
            }
            case WeaponID::SG553: { // 5. SG553 (Scoped Battle Rifle)
                Vec3 gRot = rot + Vec3(-1.5f, -4.0f, 0.0f);
                // Military tactical olive receiver
                drawCube(basePos + Vec3(0.0f, 0.02f, 0.02f), gRot, { 0.09f, 0.13f, 0.44f }, { 0.22f, 0.28f, 0.22f }, tex);
                // Elevated optical ACOG scope tube
                drawCube(basePos + Vec3(0.0f, 0.11f, -0.02f), gRot, { 0.055f, 0.055f, 0.24f }, { 0.14f, 0.15f, 0.16f });
                // Scope amber optic lens
                drawCube(basePos + Vec3(0.0f, 0.11f, 0.10f), gRot, { 0.048f, 0.048f, 0.01f }, { 1.0f, 0.75f, 0.1f }, nullptr, false);
                // Handguard with vents
                drawCube(basePos + Vec3(0.0f, 0.03f, -0.26f), gRot, { 0.078f, 0.085f, 0.24f }, { 0.20f, 0.26f, 0.20f }, tex);
                // Muzzle brake
                drawCube(basePos + Vec3(0.0f, 0.03f, -0.42f), gRot, { 0.05f, 0.05f, 0.08f }, { 0.12f, 0.12f, 0.14f });
                // Translucent angled magazine
                drawCube(basePos + Vec3(0.0f, -0.13f, 0.04f), gRot + Vec3(-16.0f, 0.0f, 0.0f), { 0.052f, 0.19f, 0.10f }, { 0.35f, 0.32f, 0.20f });
                if (muzzleFlash > 0.0f) {
                    drawCube(basePos + Vec3(0.0f, 0.03f, -0.48f), gRot, { 0.18f, 0.18f, 0.18f }, { 1.0f, 0.85f, 0.2f }, nullptr, false);
                }
                break;
            }
            case WeaponID::Minigun: { // 6. MINIGUN (Rotary Chaingun)
                Vec3 rRot = rot + Vec3(2.0f, -4.0f, 0.0f);
                // Heavy motor housing body
                drawCube(basePos + Vec3(0.0f, 0.0f, 0.08f), rRot, { 0.18f, 0.18f, 0.36f }, { 0.18f, 0.19f, 0.22f }, tex);
                // Top spade carry handle
                drawCube(basePos + Vec3(0.0f, 0.14f, 0.08f), rRot, { 0.05f, 0.10f, 0.22f }, { 0.12f, 0.12f, 0.14f });
                // Side ammo feed chute / box
                drawCube(basePos + Vec3(-0.12f, -0.04f, 0.06f), rRot, { 0.10f, 0.12f, 0.18f }, { 0.75f, 0.65f, 0.15f });

                // 6 Rotating Titanium Barrels
                float spinRad = _minigunSpinAngle * 3.14159265f / 180.0f;
                float barrelRadius = 0.055f;
                for (int b = 0; b < 6; ++b) {
                    float angle = spinRad + (b * 3.14159265f / 3.0f);
                    float bx = std::cos(angle) * barrelRadius;
                    float by = std::sin(angle) * barrelRadius;
                    drawCube(basePos + Vec3(bx, by, -0.30f), rRot, { 0.026f, 0.026f, 0.52f }, { 0.15f, 0.16f, 0.18f });
                }
                // Front barrel stabilizer ring
                drawCube(basePos + Vec3(0.0f, 0.0f, -0.52f), rRot, { 0.14f, 0.14f, 0.03f }, { 0.25f, 0.25f, 0.28f });
                if (muzzleFlash > 0.0f) {
                    drawCube(basePos + Vec3(0.0f, 0.0f, -0.62f), rRot, { 0.32f, 0.32f, 0.32f }, { 1.0f, 0.95f, 0.3f }, nullptr, false);
                }
                break;
            }
            case WeaponID::PlasmaGun: { // 7. PLASMA GUN (Pulse Energy Cannon)
                Vec3 plRot = rot + Vec3(-1.0f, -4.0f, 0.0f);
                // Sci-fi angular chassis
                drawCube(basePos + Vec3(0.0f, 0.01f, 0.02f), plRot, { 0.12f, 0.15f, 0.46f }, { 0.16f, 0.18f, 0.24f }, tex);
                // Central glowing plasma chamber
                drawCube(basePos + Vec3(0.0f, 0.02f, -0.05f), plRot, { 0.09f, 0.09f, 0.20f }, { 0.1f, 0.85f, 1.0f }, nullptr, false);
                // Upper & lower cooling radiator fins
                drawCube(basePos + Vec3(0.0f, 0.10f, -0.15f), plRot, { 0.06f, 0.03f, 0.28f }, { 0.2f, 0.7f, 0.95f });
                drawCube(basePos + Vec3(0.0f, -0.07f, -0.15f), plRot, { 0.06f, 0.03f, 0.28f }, { 0.2f, 0.7f, 0.95f });
                // Plasma emitter nozzle
                drawCube(basePos + Vec3(0.0f, 0.01f, -0.32f), plRot, { 0.08f, 0.08f, 0.10f }, { 0.15f, 0.9f, 1.0f }, nullptr, false);
                if (muzzleFlash > 0.0f) {
                    drawCube(basePos + Vec3(0.0f, 0.01f, -0.42f), plRot, { 0.26f, 0.26f, 0.26f }, { 0.2f, 0.95f, 1.0f }, nullptr, false);
                }
                break;
            }
            case WeaponID::Railgun: { // 8. RAILGUN (Gauss Kinetic Accelerator)
                Vec3 rgRot = rot + Vec3(-1.0f, -4.0f, 0.0f);
                // Heavy scientific frame
                drawCube(basePos + Vec3(0.0f, 0.02f, 0.08f), rgRot, { 0.10f, 0.14f, 0.44f }, { 0.25f, 0.26f, 0.30f }, tex);
                // Top and bottom magnetic copper acceleration rails
                drawCube(basePos + Vec3(0.0f, 0.065f, -0.26f), rgRot, { 0.035f, 0.03f, 0.54f }, { 0.85f, 0.45f, 0.18f });
                drawCube(basePos + Vec3(0.0f, -0.025f, -0.26f), rgRot, { 0.035f, 0.03f, 0.54f }, { 0.85f, 0.45f, 0.18f });
                // Induction coil wraps with glowing electric core
                for (int c = 0; c < 4; ++c) {
                    float cz = -0.10f - c * 0.12f;
                    drawCube(basePos + Vec3(0.0f, 0.02f, cz), rgRot, { 0.085f, 0.095f, 0.035f }, { 0.3f, 0.7f, 1.0f }, nullptr, false);
                }
                // Rear capacitor battery cell
                drawCube(basePos + Vec3(0.0f, -0.08f, 0.18f), rgRot, { 0.07f, 0.12f, 0.14f }, { 0.2f, 0.8f, 0.9f });
                if (muzzleFlash > 0.0f) {
                    drawCube(basePos + Vec3(0.0f, 0.02f, -0.56f), rgRot, { 0.20f, 0.20f, 0.35f }, { 0.4f, 0.85f, 1.0f }, nullptr, false);
                }
                break;
            }
            case WeaponID::RPG: { // 9. RPG (Rocket Propelled Grenade)
                Vec3 rpgRot = rot + Vec3(-3.0f, -6.0f, 2.0f);
                // Olive drab launcher tube
                drawCube(basePos + Vec3(0.0f, 0.06f, 0.02f), rpgRot, { 0.09f, 0.09f, 0.72f }, { 0.25f, 0.30f, 0.20f }, tex);
                // Wooden heat protection sleeve
                drawCube(basePos + Vec3(0.0f, 0.06f, 0.06f), rpgRot, { 0.102f, 0.102f, 0.24f }, { 0.45f, 0.28f, 0.14f });
                // Rear exhaust venturi cone
                drawCube(basePos + Vec3(0.0f, 0.06f, 0.40f), rpgRot, { 0.12f, 0.12f, 0.08f }, { 0.18f, 0.18f, 0.20f });
                // Front PG-7V Rocket Warhead (olive cone + silver detonator tip)
                drawCube(basePos + Vec3(0.0f, 0.06f, -0.38f), rpgRot, { 0.14f, 0.14f, 0.18f }, { 0.30f, 0.38f, 0.22f }, tex);
                drawCube(basePos + Vec3(0.0f, 0.06f, -0.49f), rpgRot, { 0.04f, 0.04f, 0.08f }, { 0.85f, 0.85f, 0.90f });
                // Trigger and optical sight bracket
                drawCube(basePos + Vec3(0.0f, -0.06f, 0.0f), rpgRot + Vec3(14.0f, 0.0f, 0.0f), { 0.045f, 0.14f, 0.06f }, { 0.15f, 0.15f, 0.17f });
                drawCube(basePos + Vec3(0.06f, 0.12f, -0.08f), rpgRot, { 0.04f, 0.06f, 0.08f }, { 0.2f, 0.2f, 0.22f });
                if (muzzleFlash > 0.0f) {
                    drawCube(basePos + Vec3(0.0f, 0.06f, -0.58f), rpgRot, { 0.35f, 0.35f, 0.35f }, { 1.0f, 0.5f, 0.1f }, nullptr, false);
                }
                break;
            }
            default:
                break;
        }
    }

    void WeaponSystem::renderViewModel(const Camera& camera, WeaponAnimator& animator,
                                       Texture* texture, Mesh* stlMesh, float muzzleFlash,
                                       const Vec3* rightSocketPos,
                                       const Vec3* rightSocketRot,
                                       const Vec3* leftSocketPos,
                                       const Vec3* leftSocketRot,
                                       const Vec3* tintColor,
                                       const Vec3* weaponOffset,
                                       const Vec3* weaponRotation,
                                       const Vec3* weaponScale,
                                       float uvScale,
                                       bool lockHands) {
        Renderer::beginViewModel();

        // Base idle viewmodel position (lower right screen quadrant, classic FPS framing)
        Vec3 defaultPos = { 0.26f, -0.22f, -0.46f };
        Vec3 defaultRot = { 0.0f, -3.5f, 0.0f };

        // Lower weapon when switching
        if (_switchTimer > 0.0f) {
            float switchRatio = _switchTimer / _switchDuration;
            defaultPos.y -= std::sin(switchRatio * 3.14159265f) * 0.25f;
            defaultRot.x -= std::sin(switchRatio * 3.14159265f) * 25.0f;
        }

        Vec3 gunBasePos = animator.calculatePositionOffset(defaultPos);
        Vec3 gunRot = animator.calculateRotationOffset(defaultRot);

        // Apply weapon translation & rotation offset from studio configuration
        Vec3 finalPos = gunBasePos + (weaponOffset ? *weaponOffset : Vec3(0.0f, 0.0f, 0.0f));
        Vec3 finalRot = gunRot + (weaponRotation ? *weaponRotation : Vec3(0.0f, 0.0f, 0.0f));
        Vec3 userScale = weaponScale ? *weaponScale : Vec3(1.0f, 1.0f, 1.0f);

        // If lockHands is enabled, hands remain anchored to base posture (only weapon model translates/rotates)
        Vec3 armsPos = lockHands ? gunBasePos : finalPos;
        Vec3 armsRot = lockHands ? gunRot : finalRot;

        // 1. Render First-Person Tactical Arms & Hands (kinematically bound to armsPos & armsRot with socket overrides)
        static ViewModelArms s_viewmodelArms;
        s_viewmodelArms.render(armsPos, armsRot, _currentWeapon, animator, nullptr,
                              rightSocketPos, rightSocketRot, leftSocketPos, leftSocketRot);

        Vec3 finalTint = tintColor ? *tintColor : Vec3(1.0f, 1.0f, 1.0f);

        // 2. Render Weapon Model (Custom STL or Procedural)
        if (stlMesh) {
            // User provided custom STL model with unified base scale normalization (WYSIWYG with Character Studio)
            Vec3 stlPos = finalPos;
            Vec3 stlRot = finalRot;
            float baseScale = stlMesh->getBaseScale(0.70f);
            Vec3 stlScale = { baseScale * userScale.x, baseScale * userScale.y, baseScale * userScale.z };
            Renderer::drawMesh(*stlMesh, stlPos, stlRot, stlScale, finalTint, texture, true, uvScale);
            if (muzzleFlash > 0.0f) {
                Renderer::drawCube(finalPos + Vec3(0.0f, 0.05f * userScale.y, -0.45f * userScale.z), finalRot, { 0.18f * userScale.x, 0.18f * userScale.y, 0.18f * userScale.z }, { 1.0f, 0.85f, 0.2f }, nullptr, false);
            }
        } else {
            // Stylized procedural viewmodel with weapon-specific texture and scale
            drawProceduralWeapon(_currentWeapon, finalPos, finalRot, texture, muzzleFlash, userScale);
        }

        Renderer::endViewModel(camera);
    }

} // namespace Lab
