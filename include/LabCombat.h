#pragma once
#include <vector>
#include <string>
#include <algorithm>
#include "LabMath.h"

namespace Lab {

    enum class EntityTag {
        None = 0,
        World,       // Solid brushes / geometry
        Door,        // Dynamic map doors
        Prop,        // Static props
        Bot,         // AI Combat Bot
        Player,      // Local player
        Medkit,      // Health pickup (Sprint 2)
        AmmoDrop     // Ammo pickup (Sprint 2)
    };

    struct RaycastHit {
        bool hit = false;
        float distance = 1e9f;
        Vec3 point{ 0.0f, 0.0f, 0.0f };
        Vec3 normal{ 0.0f, 1.0f, 0.0f };
        EntityTag tag = EntityTag::None;
        int entityIndex = -1;
        bool isHeadshot = false;
    };

    struct BulletTracer {
        Vec3 start{ 0.0f, 0.0f, 0.0f };
        Vec3 end{ 0.0f, 0.0f, 0.0f };
        Vec3 color{ 1.0f, 0.85f, 0.3f };
        float lifetime = 0.0f;
        float maxLifetime = 0.08f;
        float thickness = 0.035f;

        bool isExpired() const { return lifetime >= maxLifetime; }
    };

    class Raycast {
    public:
        // Fast branchless slab algorithm for Ray-AABB intersection with surface normal extraction
        static bool rayIntersectAABB(const Vec3& rayOrigin, const Vec3& rayDir,
                                     const Vec3& boxMin, const Vec3& boxMax,
                                     float& tOut, Vec3* outNormal = nullptr);
    };

} // namespace Lab
