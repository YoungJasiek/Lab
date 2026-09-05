#include "LabPickups.h"
#include <cmath>
#include <algorithm>

namespace Lab {

    void PickupItem::update(float dt) {
        rotationY += dt * 75.0f;
        if (rotationY >= 360.0f) rotationY -= 360.0f;
        bobTime += dt * 3.2f;
    }

    void PickupItem::render() const {
        float currentY = position.y + std::sin(bobTime) * 0.08f;
        Vec3 rot = { 0.0f, rotationY, 0.0f };

        if (type == PickupType::Ammo) {
            // Olive-drab military ammo box
            Renderer::drawCube(Vec3(position.x, currentY, position.z), rot,
                               Vec3(0.36f, 0.22f, 0.22f), Vec3(0.22f, 0.35f, 0.18f), nullptr, false);
            // Yellow warning / caliber stripe
            Renderer::drawCube(Vec3(position.x, currentY, position.z), rot,
                               Vec3(0.38f, 0.05f, 0.24f), Vec3(0.95f, 0.82f, 0.15f), nullptr, false);
            // Shiny bullet clip top
            Renderer::drawCube(Vec3(position.x, currentY + 0.12f, position.z), rot,
                               Vec3(0.14f, 0.04f, 0.16f), Vec3(1.0f, 0.88f, 0.35f), nullptr, false);
        } else if (type == PickupType::Medkit) {
            // White medical case
            Renderer::drawCube(Vec3(position.x, currentY, position.z), rot,
                               Vec3(0.34f, 0.24f, 0.20f), Vec3(0.95f, 0.95f, 0.95f), nullptr, false);
            // Red cross - vertical bar
            Renderer::drawCube(Vec3(position.x, currentY, position.z), rot,
                               Vec3(0.08f, 0.18f, 0.22f), Vec3(0.95f, 0.15f, 0.15f), nullptr, false);
            // Red cross - horizontal bar
            Renderer::drawCube(Vec3(position.x, currentY, position.z), rot,
                               Vec3(0.22f, 0.08f, 0.22f), Vec3(0.95f, 0.15f, 0.15f), nullptr, false);
        } else if (type == PickupType::WeaponDrop) {
            // High-tech weapon drop crate
            Renderer::drawCube(Vec3(position.x, currentY, position.z), rot,
                               Vec3(0.48f, 0.18f, 0.26f), Vec3(0.18f, 0.20f, 0.25f), nullptr, false);
            // Glowing neon hazard border
            Renderer::drawCube(Vec3(position.x, currentY, position.z), rot,
                               Vec3(0.50f, 0.04f, 0.28f), Vec3(1.0f, 0.8f, 0.1f), nullptr, false);
            // Hologram weapon marker on top
            Renderer::drawCube(Vec3(position.x, currentY + 0.16f, position.z), rot,
                               Vec3(0.36f, 0.06f, 0.08f), Vec3(0.3f, 0.85f, 1.0f), nullptr, false);
        }
    }

    void PickupManager::spawnPickup(PickupType type, const Vec3& pos, int value, int weaponId) {
        PickupItem item;
        item.id = nextId++;
        item.type = type;
        item.weaponId = weaponId;
        item.position = pos;
        item.rotationY = static_cast<float>(rand() % 360);
        item.bobTime = static_cast<float>((rand() % 100) / 10.0f);
        item.collected = false;

        if (value <= 0) {
            if (type == PickupType::Ammo) item.value = 36;
            else if (type == PickupType::Medkit) item.value = 50;
            else item.value = 40;
        } else {
            item.value = value;
        }

        items.push_back(item);
    }

    void PickupManager::update(float dt, const Vec3& playerPos, int& outAmmoAdded, float& outHealthAdded, int& outWeaponUnlocked, std::string& outNotification) {
        for (auto& item : items) {
            item.update(dt);

            Vec3 diff = playerPos - item.position;
            float distSq = diff.x * diff.x + diff.z * diff.z;
            float dy = std::abs(diff.y);

            // Trigger radius: 1.6m horizontal, 2.0m vertical
            if (distSq < (1.6f * 1.6f) && dy < 2.0f) {
                if (item.type == PickupType::Ammo) {
                    outAmmoAdded += item.value;
                    item.collected = true;
                    outNotification = "+ " + std::to_string(item.value) + " AMMO COLLECTED";
                } else if (item.type == PickupType::Medkit) {
                    outHealthAdded += static_cast<float>(item.value);
                    item.collected = true;
                    outNotification = "+ " + std::to_string(item.value) + " HP RESTORED";
                } else if (item.type == PickupType::WeaponDrop) {
                    outWeaponUnlocked = item.weaponId;
                    outAmmoAdded += item.value;
                    item.collected = true;
                    const char* wNames[9] = { "PIPE", "PISTOL", "SHOTGUN", "M4A4-S", "SG553", "MINIGUN", "PLASMA GUN", "RAILGUN", "RPG" };
                    std::string wNameStr = (item.weaponId >= 0 && item.weaponId < 9) ? wNames[item.weaponId] : "NEW WEAPON";
                    outNotification = "+ " + wNameStr + " ACQUIRED!";
                }
            }
        }

        items.erase(std::remove_if(items.begin(), items.end(),
                                   [](const PickupItem& it) { return it.collected; }),
                    items.end());
    }

    void PickupManager::render() const {
        for (const auto& item : items) {
            item.render();
        }
    }

} // namespace Lab
