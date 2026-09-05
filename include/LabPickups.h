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

    class PickupManager {
    public:
        std::vector<PickupItem> items;
        int nextId = 1;

        void clear() { items.clear(); nextId = 1; }
        void spawnPickup(PickupType type, const Vec3& pos, int value = 0, int weaponId = 1);
        void update(float dt, const Vec3& playerPos, int& outAmmoAdded, float& outHealthAdded, int& outWeaponUnlocked, std::string& outNotification);
        void render() const;
    };

} // namespace Lab
