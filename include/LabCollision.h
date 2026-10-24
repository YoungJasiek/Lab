#pragma once
#include <vector>
#include <algorithm>
#include <cmath>
#include "LabMath.h"
#include "LabMap.h"
#include "LabCombat.h"

namespace Lab {

    struct CollisionBox {
        Vec3 min{ 0.0f, 0.0f, 0.0f };
        Vec3 max{ 0.0f, 0.0f, 0.0f };
        EntityTag tag = EntityTag::World;
        int entityIndex = -1;

        bool intersects(const Vec3& bMin, const Vec3& bMax) const {
            return (min.x <= bMax.x && max.x >= bMin.x) &&
                   (min.y <= bMax.y && max.y >= bMin.y) &&
                   (min.z <= bMax.z && max.z >= bMin.z);
        }
    };

    class LabCollision {
    public:
        // Extract all active solid collision boxes from the map (brushes and active doors)
        static std::vector<CollisionBox> getMapSolidBoxes(const LabMap& map);

        // Perform Source-style Axis-Separated AABB Move & Slide resolution
        // Resolves horizontal wall sliding, vertical gravity/jumping, and step-up for stairs/curbs
        static void moveAndSlide(Vec3& position, Vec3& velocity, bool& isGrounded,
                                 float dt, const std::vector<CollisionBox>& obstacles,
                                 float eyeHeight = 1.70f, float playerRadius = 0.35f,
                                 float playerHeight = 1.85f, float stepHeight = 0.35f);

        // De-penetration pass to cleanly pop player out of any intersecting geometry
        static void resolvePenetration(Vec3& position, const std::vector<CollisionBox>& obstacles,
                                       float eyeHeight = 1.70f, float playerRadius = 0.35f,
                                       float playerHeight = 1.85f);

        // Check if the player's AABB overlaps any active trigger volumes in the map.
        // Marks triggered one-shot volumes and returns the indices of newly activated triggers.
        static std::vector<int> checkTriggerVolumes(const Vec3& position, LabMap& map,
                                                    float eyeHeight = 1.70f, float playerRadius = 0.35f,
                                                    float playerHeight = 1.85f);
    };

} // namespace Lab

