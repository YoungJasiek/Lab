#include "LabPickups.h"
#include <cmath>
#include <algorithm>

namespace Lab {

    static std::vector<SpawnerItemDef>& GetMutableSpawnerRegistry() {
        static std::vector<SpawnerItemDef> s_registry = {
            // Weapons
            { "weapon_pipe", "Pipe (Melee)", "Weapons", SpawnerItemKind::Weapon, 0, 0 },
            { "weapon_pistol", "Tactical Pistol", "Weapons", SpawnerItemKind::Weapon, 1, 32 },
            { "weapon_shotgun", "Tactical Shotgun", "Weapons", SpawnerItemKind::Weapon, 2, 24 },
            { "weapon_m4a4s", "Silenced Carbine M4A4-S", "Weapons", SpawnerItemKind::Weapon, 3, 60 },
            { "weapon_sg553", "Scoped Rifle SG553", "Weapons", SpawnerItemKind::Weapon, 4, 60 },
            { "weapon_minigun", "Rotary Vulcan Cannon", "Weapons", SpawnerItemKind::Weapon, 5, 150 },
            { "weapon_plasma", "Plasma Pulse Cannon", "Weapons", SpawnerItemKind::Weapon, 6, 40 },
            { "weapon_railgun", "Gauss Kinetic Railgun", "Weapons", SpawnerItemKind::Weapon, 7, 20 },
            { "weapon_rpg", "Rocket Propelled Grenade", "Weapons", SpawnerItemKind::Weapon, 8, 5 },

            // Health & Medical
            { "item_healthkit", "First-Aid Medkit (+50 HP)", "Health", SpawnerItemKind::Medkit, -1, 50 },
            { "item_healthvial", "Health Vial (+25 HP)", "Health", SpawnerItemKind::HealthVial, -1, 25 },
            { "item_megahealth", "Mega Health Booster (+100 HP)", "Health", SpawnerItemKind::Medkit, -1, 100 },

            // Armor & HEV
            { "item_battery", "HEV Armor Battery (+25 AP)", "Armor", SpawnerItemKind::ArmorBattery, -1, 25 },
            { "item_armor", "Combat Kevlar Vest (+50 AP)", "Armor", SpawnerItemKind::ArmorBattery, -1, 50 },
            { "item_suit", "HEV Hazard Suit (+100 AP)", "Armor", SpawnerItemKind::HEVSuit, -1, 100 },

            // Ammunition
            { "item_ammo", "Military Ammo Box (+36)", "Ammo", SpawnerItemKind::AmmoBox, -1, 36 },
            { "item_ammo_heavy", "Heavy Ammo Crate (+100)", "Ammo", SpawnerItemKind::AmmoBox, -1, 100 },

            // Modded / Custom
            { "item_custom", "Custom Modded Entity", "Modded", SpawnerItemKind::Custom, -1, 50 }
        };
        return s_registry;
    }

    const std::vector<SpawnerItemDef>& GetSpawnerItemDefinitions() {
        return GetMutableSpawnerRegistry();
    }

    const SpawnerItemDef* FindSpawnerItemDef(const std::string& tag) {
        if (tag.empty()) return nullptr;
        for (const auto& item : GetMutableSpawnerRegistry()) {
            if (item.tag == tag) return &item;
            if (item.tag == ("weapon_" + tag)) return &item;
            if (item.tag == ("item_" + tag)) return &item;
            if (tag.rfind("weapon_", 0) == 0 && item.tag == tag.substr(7)) return &item;
            if (tag.rfind("item_", 0) == 0 && item.tag == tag.substr(5)) return &item;
        }
        return nullptr;
    }

    const SpawnerItemDef* FindSpawnerItemDefByWeaponId(int weaponId) {
        for (const auto& item : GetMutableSpawnerRegistry()) {
            if (item.kind == SpawnerItemKind::Weapon && item.weaponId == weaponId) {
                return &item;
            }
        }
        return nullptr;
    }

    void RegisterCustomSpawnerItem(const SpawnerItemDef& def) {
        auto& reg = GetMutableSpawnerRegistry();
        for (auto& item : reg) {
            if (item.tag == def.tag) {
                item = def;
                return;
            }
        }
        reg.push_back(def);
    }

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
        } else if (type == PickupType::ArmorBattery) {
            // HEV Armor Battery
            Renderer::drawCube(Vec3(position.x, currentY, position.z), rot,
                               Vec3(0.24f, 0.32f, 0.24f), Vec3(0.18f, 0.20f, 0.24f), nullptr, false);
            Renderer::drawCube(Vec3(position.x, currentY + 0.14f, position.z), rot,
                               Vec3(0.26f, 0.06f, 0.26f), Vec3(1.0f, 0.55f, 0.0f), nullptr, false);
            Renderer::drawCube(Vec3(position.x, currentY - 0.14f, position.z), rot,
                               Vec3(0.26f, 0.06f, 0.26f), Vec3(1.0f, 0.55f, 0.0f), nullptr, false);
            Renderer::drawCube(Vec3(position.x, currentY, position.z), rot,
                               Vec3(0.14f, 0.18f, 0.26f), Vec3(0.1f, 0.9f, 1.0f), nullptr, false);
        } else if (type == PickupType::HEVSuit) {
            // HEV Hazard Suit
            Renderer::drawCube(Vec3(position.x, currentY, position.z), rot,
                               Vec3(0.38f, 0.42f, 0.24f), Vec3(0.16f, 0.18f, 0.20f), nullptr, false);
            Renderer::drawCube(Vec3(position.x, currentY + 0.04f, position.z), rot,
                               Vec3(0.40f, 0.26f, 0.16f), Vec3(1.0f, 0.52f, 0.02f), nullptr, false);
            Renderer::drawCube(Vec3(position.x, currentY + 0.02f, position.z), rot,
                               Vec3(0.14f, 0.12f, 0.28f), Vec3(0.2f, 0.85f, 1.0f), nullptr, false);
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
        } else {
            // Custom item
            Renderer::drawCube(Vec3(position.x, currentY, position.z), rot,
                               Vec3(0.36f, 0.36f, 0.36f), Vec3(0.6f, 0.2f, 0.9f), nullptr, false);
            Renderer::drawWireCube(Vec3(position.x, currentY, position.z),
                                   Vec3(0.42f, 0.42f, 0.42f), Vec3(0.9f, 0.3f, 1.0f));
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
            else if (type == PickupType::ArmorBattery) item.value = 25;
            else if (type == PickupType::HEVSuit) item.value = 100;
            else item.value = 40;
        } else {
            item.value = value;
        }

        items.push_back(item);
    }

    void WeaponSpawnPad::update(float dt) {
        rotationY += dt * 65.0f;
        if (rotationY >= 360.0f) rotationY -= 360.0f;
        bobTime += dt * 2.8f;
    }

    std::string WeaponSpawnPad::getItemDisplayName() const {
        const auto* def = FindSpawnerItemDef(itemTag);
        if (def) return def->displayName;
        if (itemKind == SpawnerItemKind::Weapon) {
            const auto* wdef = FindSpawnerItemDefByWeaponId(weaponId);
            if (wdef) return wdef->displayName;
        }
        return itemTag.empty() ? ("Item_" + std::to_string(weaponId)) : itemTag;
    }

    void WeaponSpawnPad::render(const Mesh* customMesh, const Texture* customTex) const {
        // Base ground pedestal
        Renderer::drawCube(Vec3(position.x, position.y + 0.04f, position.z), Vec3(0, yaw, 0),
                           Vec3(1.35f, 0.08f, 1.35f), Vec3(0.16f, 0.18f, 0.22f), nullptr, false);
        // Emissive neon ring: Cyan when ready, Dim Red when respawning
        Vec3 ringColor = isAvailable() ? Vec3(0.2f, 0.75f, 0.95f) : Vec3(0.55f, 0.15f, 0.15f);
        Renderer::drawCube(Vec3(position.x, position.y + 0.082f, position.z), Vec3(0, yaw, 0),
                           Vec3(1.15f, 0.01f, 1.15f), ringColor, nullptr, false);

        if (isAvailable()) {
            float currentY = position.y + 0.38f + std::sin(bobTime) * 0.06f;
            Vec3 rot = { 0.0f, rotationY, 0.0f };

            if (itemKind == SpawnerItemKind::Medkit || itemKind == SpawnerItemKind::HealthVial) {
                // White medical case with red cross
                Renderer::drawCube(Vec3(position.x, currentY, position.z), rot,
                                   Vec3(0.34f, 0.24f, 0.20f), Vec3(0.95f, 0.95f, 0.95f), nullptr, false);
                Renderer::drawCube(Vec3(position.x, currentY, position.z), rot,
                                   Vec3(0.08f, 0.18f, 0.22f), Vec3(0.95f, 0.15f, 0.15f), nullptr, false);
                Renderer::drawCube(Vec3(position.x, currentY, position.z), rot,
                                   Vec3(0.22f, 0.08f, 0.22f), Vec3(0.95f, 0.15f, 0.15f), nullptr, false);
            } else if (itemKind == SpawnerItemKind::ArmorBattery) {
                // HEV Armor Battery
                Renderer::drawCube(Vec3(position.x, currentY, position.z), rot,
                                   Vec3(0.24f, 0.32f, 0.24f), Vec3(0.18f, 0.20f, 0.24f), nullptr, false);
                Renderer::drawCube(Vec3(position.x, currentY + 0.14f, position.z), rot,
                                   Vec3(0.26f, 0.06f, 0.26f), Vec3(1.0f, 0.55f, 0.0f), nullptr, false);
                Renderer::drawCube(Vec3(position.x, currentY - 0.14f, position.z), rot,
                                   Vec3(0.26f, 0.06f, 0.26f), Vec3(1.0f, 0.55f, 0.0f), nullptr, false);
                Renderer::drawCube(Vec3(position.x, currentY, position.z), rot,
                                   Vec3(0.14f, 0.18f, 0.26f), Vec3(0.1f, 0.9f, 1.0f), nullptr, false);
            } else if (itemKind == SpawnerItemKind::HEVSuit) {
                // HEV Hazard Suit
                Renderer::drawCube(Vec3(position.x, currentY, position.z), rot,
                                   Vec3(0.38f, 0.42f, 0.24f), Vec3(0.16f, 0.18f, 0.20f), nullptr, false);
                Renderer::drawCube(Vec3(position.x, currentY + 0.04f, position.z), rot,
                                   Vec3(0.40f, 0.26f, 0.16f), Vec3(1.0f, 0.52f, 0.02f), nullptr, false);
                Renderer::drawCube(Vec3(position.x, currentY + 0.02f, position.z), rot,
                                   Vec3(0.14f, 0.12f, 0.28f), Vec3(0.2f, 0.85f, 1.0f), nullptr, false);
            } else if (itemKind == SpawnerItemKind::AmmoBox) {
                // Olive ammo crate
                Renderer::drawCube(Vec3(position.x, currentY, position.z), rot,
                                   Vec3(0.36f, 0.22f, 0.22f), Vec3(0.22f, 0.35f, 0.18f), nullptr, false);
                Renderer::drawCube(Vec3(position.x, currentY, position.z), rot,
                                   Vec3(0.38f, 0.05f, 0.24f), Vec3(0.95f, 0.82f, 0.15f), nullptr, false);
            } else if (itemKind == SpawnerItemKind::Custom) {
                // Custom modded item
                Renderer::drawCube(Vec3(position.x, currentY, position.z), rot,
                                   Vec3(0.36f, 0.36f, 0.36f), Vec3(0.6f, 0.2f, 0.9f), nullptr, false);
                Renderer::drawWireCube(Vec3(position.x, currentY, position.z),
                                       Vec3(0.42f, 0.42f, 0.42f), Vec3(0.9f, 0.3f, 1.0f));
            } else {
                // Weapon representation
                if (customMesh) {
                    Renderer::drawMesh(*customMesh, Vec3(position.x, currentY, position.z), rot,
                                       Vec3(0.018f, 0.018f, 0.018f), Vec3(1, 1, 1), customTex);
                } else {
                    switch (weaponId) {
                        case 0: // Pipe
                            Renderer::drawCube(Vec3(position.x, currentY, position.z), rot + Vec3(15.0f, 0, 25.0f),
                                               Vec3(0.06f, 0.06f, 0.75f), Vec3(0.6f, 0.6f, 0.65f), customTex, false);
                            break;
                        case 1: // Pistol
                            Renderer::drawCube(Vec3(position.x, currentY, position.z), rot,
                                               Vec3(0.08f, 0.14f, 0.28f), Vec3(0.22f, 0.24f, 0.28f), customTex, false);
                            break;
                        case 2: // Shotgun
                            Renderer::drawCube(Vec3(position.x, currentY, position.z), rot,
                                               Vec3(0.09f, 0.12f, 0.82f), Vec3(0.35f, 0.25f, 0.18f), customTex, false);
                            break;
                        case 3: // M4A4-S
                            Renderer::drawCube(Vec3(position.x, currentY, position.z), rot,
                                               Vec3(0.08f, 0.16f, 0.76f), Vec3(0.2f, 0.35f, 0.3f), customTex, false);
                            break;
                        case 4: // SG553
                            Renderer::drawCube(Vec3(position.x, currentY, position.z), rot,
                                               Vec3(0.08f, 0.18f, 0.80f), Vec3(0.3f, 0.4f, 0.25f), customTex, false);
                            break;
                        case 5: // Minigun
                            Renderer::drawCube(Vec3(position.x, currentY, position.z), rot,
                                               Vec3(0.18f, 0.22f, 0.90f), Vec3(0.18f, 0.18f, 0.20f), customTex, false);
                            break;
                        case 6: // Plasma Gun
                            Renderer::drawCube(Vec3(position.x, currentY, position.z), rot,
                                               Vec3(0.14f, 0.18f, 0.70f), Vec3(0.2f, 0.6f, 0.9f), customTex, false);
                            break;
                        case 7: // Railgun
                            Renderer::drawCube(Vec3(position.x, currentY, position.z), rot,
                                               Vec3(0.10f, 0.14f, 0.95f), Vec3(0.4f, 0.75f, 1.0f), customTex, false);
                            break;
                        case 8: // RPG
                        default:
                            Renderer::drawCube(Vec3(position.x, currentY, position.z), rot,
                                               Vec3(0.12f, 0.14f, 0.98f), Vec3(0.3f, 0.35f, 0.2f), customTex, false);
                            break;
                    }
                }
            }
        } else {
            // Pulsing holographic respawn ring
            float ratio = std::clamp(1.0f - (respawnTimer / respawnDuration), 0.05f, 1.0f);
            Renderer::drawWireCube(Vec3(position.x, position.y + 0.20f, position.z),
                                   Vec3(0.9f * ratio, 0.05f, 0.9f * ratio), Vec3(0.3f, 0.8f, 1.0f));
        }
    }

    void PickupManager::addWeaponPad(int weaponId, const Vec3& pos, float respawnDuration, float yaw, const std::string& itemTag) {
        WeaponSpawnPad pad;
        pad.id = nextId++;
        pad.weaponId = weaponId;
        pad.position = pos;
        pad.yaw = yaw;
        pad.respawnDuration = (respawnDuration > 0.0f) ? respawnDuration : 60.0f; // Default 1 min
        pad.respawnTimer = 0.0f; // Available on map load
        pad.rotationY = static_cast<float>(rand() % 360);
        pad.bobTime = static_cast<float>((rand() % 100) / 10.0f);

        if (!itemTag.empty()) {
            pad.itemTag = itemTag;
            const auto* def = FindSpawnerItemDef(itemTag);
            if (def) {
                pad.itemKind = def->kind;
                pad.value = def->value;
                if (def->kind == SpawnerItemKind::Weapon && def->weaponId >= 0) {
                    pad.weaponId = def->weaponId;
                }
            }
        } else {
            const auto* def = FindSpawnerItemDefByWeaponId(weaponId);
            if (def) {
                pad.itemTag = def->tag;
                pad.itemKind = def->kind;
                pad.value = def->value;
            } else {
                pad.itemTag = "weapon_" + std::to_string(weaponId);
                pad.itemKind = SpawnerItemKind::Weapon;
                pad.value = 32;
            }
        }

        weaponPads.push_back(pad);
    }

    void PickupManager::update(float dt, const Vec3& playerPos, int& outAmmoAdded, float& outHealthAdded, float& outArmorAdded, int& outWeaponUnlocked, std::string& outNotification, bool& outWeaponRespawned, std::string& outCustomItem) {
        outWeaponRespawned = false;
        outCustomItem.clear();

        // 1. Temporary pickups (dropped by dead bots)
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
                } else if (item.type == PickupType::ArmorBattery) {
                    outArmorAdded += static_cast<float>(item.value);
                    item.collected = true;
                    outNotification = "+ " + std::to_string(item.value) + " AP (BATTERY CHARGED)";
                } else if (item.type == PickupType::HEVSuit) {
                    outArmorAdded += static_cast<float>(item.value);
                    item.collected = true;
                    outNotification = "+ HEV SUIT ACQUIRED (+100 AP)";
                } else if (item.type == PickupType::WeaponDrop) {
                    outWeaponUnlocked = item.weaponId;
                    outAmmoAdded += item.value;
                    item.collected = true;
                    const char* wNames[9] = { "PIPE", "PISTOL", "SHOTGUN", "M4A4-S", "SG553", "MINIGUN", "PLASMA GUN", "RAILGUN", "RPG" };
                    std::string wNameStr = (item.weaponId >= 0 && item.weaponId < 9) ? wNames[item.weaponId] : "NEW WEAPON";
                    outNotification = "+ " + wNameStr + " ACQUIRED!";
                } else {
                    outCustomItem = "custom_drop";
                    item.collected = true;
                    outNotification = "+ MOD ITEM COLLECTED";
                }
            }
        }

        items.erase(std::remove_if(items.begin(), items.end(),
                                   [](const PickupItem& it) { return it.collected; }),
                    items.end());

        // 2. Weapon & Item Spawn Pads (World persistent spawners with cooldown)
        for (auto& pad : weaponPads) {
            pad.update(dt);

            if (pad.isAvailable()) {
                Vec3 diff = playerPos - pad.position;
                float distSq = diff.x * diff.x + diff.z * diff.z;
                float dy = std::abs(diff.y);

                if (distSq < (1.8f * 1.8f) && dy < 2.0f) {
                    pad.respawnTimer = pad.respawnDuration;

                    if (pad.itemKind == SpawnerItemKind::Weapon) {
                        outWeaponUnlocked = pad.weaponId;
                        outAmmoAdded += 32;
                        outNotification = "+ " + pad.getItemDisplayName() + " COLLECTED! (RESPAWN IN " + std::to_string((int)pad.respawnDuration) + "s)";
                    } else if (pad.itemKind == SpawnerItemKind::Medkit || pad.itemKind == SpawnerItemKind::HealthVial) {
                        outHealthAdded += static_cast<float>(pad.value);
                        outNotification = "+ " + pad.getItemDisplayName() + " (+ " + std::to_string(pad.value) + " HP)";
                    } else if (pad.itemKind == SpawnerItemKind::ArmorBattery || pad.itemKind == SpawnerItemKind::HEVSuit) {
                        outArmorAdded += static_cast<float>(pad.value);
                        outNotification = "+ " + pad.getItemDisplayName() + " (+ " + std::to_string(pad.value) + " AP)";
                    } else if (pad.itemKind == SpawnerItemKind::AmmoBox) {
                        outAmmoAdded += pad.value;
                        outNotification = "+ " + pad.getItemDisplayName() + " (+ " + std::to_string(pad.value) + " AMMO)";
                    } else {
                        outCustomItem = pad.itemTag;
                        outNotification = "+ " + pad.getItemDisplayName() + " ACQUIRED!";
                    }
                }
            } else {
                pad.respawnTimer -= dt;
                if (pad.respawnTimer <= 0.0f) {
                    pad.respawnTimer = 0.0f;
                    outWeaponRespawned = true;
                }
            }
        }
    }

    void PickupManager::render(const std::vector<Mesh*>& weaponMeshes, const std::vector<Texture*>& weaponTextures) const {
        for (const auto& item : items) {
            item.render();
        }
        for (const auto& pad : weaponPads) {
            const Mesh* m = (pad.weaponId >= 0 && pad.weaponId < (int)weaponMeshes.size()) ? weaponMeshes[pad.weaponId] : nullptr;
            const Texture* t = (pad.weaponId >= 0 && pad.weaponId < (int)weaponTextures.size()) ? weaponTextures[pad.weaponId] : nullptr;
            pad.render(m, t);
        }
    }

} // namespace Lab
