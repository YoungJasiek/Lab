#pragma once
#include <vector>
#include <string>
#include "LabMath.h"
#include "LabRenderer.h"
#include "LabExport.h"

namespace Lab {

    enum class PickupType {
        Ammo,
        Medkit,
        ArmorBattery,
        HEVSuit,
        WeaponDrop,
        CustomItem
    };

    enum class SpawnerItemKind {
        Weapon = 0,
        Medkit,
        HealthVial,
        ArmorBattery,
        HEVSuit,
        AmmoBox,
        Custom
    };

    struct SpawnerItemDef {
        std::string tag;         // Unique prefab tag: "weapon_shotgun", "item_battery", "item_suit", etc.
        std::string displayName; // Display name: "Tactical Shotgun", "HEV Armor Battery (+25 AP)"
        std::string category;    // "Weapons", "Health", "Armor", "Ammo", "Modded"
        SpawnerItemKind kind = SpawnerItemKind::Weapon;
        int weaponId = -1;       // 0..8 or -1
        int value = 0;           // Heal / Armor / Ammo amount
    };

    const std::vector<SpawnerItemDef>& GetSpawnerItemDefinitions();
    const SpawnerItemDef* FindSpawnerItemDef(const std::string& tag);
    const SpawnerItemDef* FindSpawnerItemDefByWeaponId(int weaponId);
    void RegisterCustomSpawnerItem(const SpawnerItemDef& def);

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
        std::string itemTag = "weapon_shotgun";
        SpawnerItemKind itemKind = SpawnerItemKind::Weapon;
        int value = 32;
        Vec3 position{ 0.0f, 0.0f, 0.0f };
        float yaw = 0.0f;
        float respawnDuration = 60.0f; // 1 min (60 seconds)
        float respawnTimer = 0.0f;     // 0.0f = ready/present on pad, > 0 = respawning
        float rotationY = 0.0f;
        float bobTime = 0.0f;

        bool isAvailable() const { return respawnTimer <= 0.0f; }
        std::string getItemDisplayName() const;
        void update(float dt);
        void render(const Mesh* customMesh = nullptr, const Texture* customTex = nullptr) const;
    };

    class PickupManager {
    public:
        std::vector<PickupItem> items;
        std::vector<WeaponSpawnPad> weaponPads;
        int nextId = 1;

        void clear() { items.clear(); weaponPads.clear(); nextId = 1; }
        void addWeaponPad(int weaponId, const Vec3& pos, float respawnDuration = 60.0f, float yaw = 0.0f, const std::string& itemTag = "");
        void spawnPickup(PickupType type, const Vec3& pos, int value = 0, int weaponId = 1);
        void update(float dt, const Vec3& playerPos, int& outAmmoAdded, float& outHealthAdded, float& outArmorAdded, int& outWeaponUnlocked, std::string& outNotification, bool& outWeaponRespawned, std::string& outCustomItem);
        void update(float dt, const Vec3& playerPos, int& outAmmoAdded, float& outHealthAdded, int& outWeaponUnlocked, std::string& outNotification, bool& outWeaponRespawned) {
            float dummyArmor = 0.0f;
            std::string dummyCustom;
            update(dt, playerPos, outAmmoAdded, outHealthAdded, dummyArmor, outWeaponUnlocked, outNotification, outWeaponRespawned, dummyCustom);
        }
        void update(float dt, const Vec3& playerPos, int& outAmmoAdded, float& outHealthAdded, int& outWeaponUnlocked, std::string& outNotification) {
            bool dummyRespawn = false;
            update(dt, playerPos, outAmmoAdded, outHealthAdded, outWeaponUnlocked, outNotification, dummyRespawn);
        }
        void render(const std::vector<Mesh*>& weaponMeshes = {}, const std::vector<Texture*>& weaponTextures = {}) const;
    };

} // namespace Lab
