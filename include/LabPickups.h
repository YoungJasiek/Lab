#pragma once
#include <vector>
#include <string>
#include "LabMath.h"
#include "LabRenderer.h"

namespace Lab {

    enum class PickupType {
        Ammo,
        Medkit,
        WeaponDrop
    };

    struct PickupItem {
        int id = 0;
        PickupType type = PickupType::Ammo;
        int weaponId = 1; // 0..8 (maps to WeaponID)
        Vec3 position{ 0.0f, 0.0f, 0.0f };
        float rotationY = 0.0f;
        float bobTime = 0.0f;
        int value = 36;
        bool collected = false;

        void update(float dt);
        void render() const;
    };

    struct WeaponSpawnPad {
        int id = 0;
        int weaponId = 2;              // 0..8 (maps to WeaponID)
        Vec3 position{ 0.0f, 0.0f, 0.0f };
        float yaw = 0.0f;
        float respawnDuration = 60.0f; // 1 min (60 seconds)
        float respawnTimer = 0.0f;     // 0.0f = ready/present on pad, > 0 = respawning
        float rotationY = 0.0f;
        float bobTime = 0.0f;

        bool isAvailable() const { return respawnTimer <= 0.0f; }
        void update(float dt);
        void render(const Mesh* customMesh = nullptr, const Texture* customTex = nullptr) const;
    };

    class PickupManager {
    public:
        std::vector<PickupItem> items;
        std::vector<WeaponSpawnPad> weaponPads;
        int nextId = 1;

        void clear() { items.clear(); weaponPads.clear(); nextId = 1; }
        void addWeaponPad(int weaponId, const Vec3& pos, float respawnDuration = 60.0f, float yaw = 0.0f);
        void spawnPickup(PickupType type, const Vec3& pos, int value = 0, int weaponId = 1);
        void update(float dt, const Vec3& playerPos, int& outAmmoAdded, float& outHealthAdded, int& outWeaponUnlocked, std::string& outNotification, bool& outWeaponRespawned);
        void update(float dt, const Vec3& playerPos, int& outAmmoAdded, float& outHealthAdded, int& outWeaponUnlocked, std::string& outNotification) {
            bool dummyRespawn = false;
            update(dt, playerPos, outAmmoAdded, outHealthAdded, outWeaponUnlocked, outNotification, dummyRespawn);
        }
        void render(const std::vector<Mesh*>& weaponMeshes = {}, const std::vector<Texture*>& weaponTextures = {}) const;
    };

} // namespace Lab
