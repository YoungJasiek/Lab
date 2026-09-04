#include "LabCollision.h"

namespace Lab {

    std::vector<CollisionBox> LabCollision::getMapSolidBoxes(const LabMap& map) {
        std::vector<CollisionBox> boxes;
        boxes.reserve(map.brushes.size() + map.doors.size());

        // 1. Add all brushes as solid boxes
        for (size_t i = 0; i < map.brushes.size(); ++i) {
            const auto& b = map.brushes[i];
            Vec3 half = b.size * 0.5f;
            CollisionBox box;
            box.min = b.position - half;
            box.max = b.position + half;
            box.tag = EntityTag::World;
            box.entityIndex = static_cast<int>(i);
            boxes.push_back(box);
        }

        // 2. Add doors in their active animated positions
        for (size_t i = 0; i < map.doors.size(); ++i) {
            const auto& d = map.doors[i];
            Vec3 animPos = d.position + d.openOffset * d.currentProgress;
            Vec3 half = d.size * 0.5f;
            CollisionBox box;
            box.min = animPos - half;
            box.max = animPos + half;
            box.tag = EntityTag::Door;
            box.entityIndex = static_cast<int>(i);
            boxes.push_back(box);
        }

        return boxes;
    }

    void LabCollision::moveAndSlide(Vec3& position, Vec3& velocity, bool& isGrounded,
                                    float dt, const std::vector<CollisionBox>& obstacles,
                                    float eyeHeight, float playerRadius,
                                    float playerHeight, float stepHeight) {
        float feetY = position.y - eyeHeight;
        float headY = feetY + playerHeight;

        float targetDx = velocity.x * dt;
        float targetDz = velocity.z * dt;

        // Step-up logic for stairs, curbs, and thresholds
        if (isGrounded && (std::abs(targetDx) > 1e-4f || std::abs(targetDz) > 1e-4f)) {
            Vec3 sweepMin = {
                position.x + targetDx - playerRadius,
                feetY + 0.05f,
                position.z + targetDz - playerRadius
            };
            Vec3 sweepMax = {
                position.x + targetDx + playerRadius,
                feetY + stepHeight,
                position.z + targetDz + playerRadius
            };

            float maxStepY = feetY;
            bool foundStep = false;
            for (const auto& box : obstacles) {
                if (box.intersects(sweepMin, sweepMax)) {
                    if (box.max.y > feetY && box.max.y <= feetY + stepHeight) {
                        maxStepY = std::max(maxStepY, box.max.y);
                        foundStep = true;
                    }
                }
            }

            if (foundStep) {
                // Test if elevated position has enough headroom
                float elevatedFeet = maxStepY + 0.02f;
                float elevatedHead = elevatedFeet + playerHeight;
                Vec3 headTestMin = { position.x - playerRadius, elevatedFeet, position.z - playerRadius };
                Vec3 headTestMax = { position.x + playerRadius, elevatedHead, position.z + playerRadius };
                bool headBlocked = false;
                for (const auto& box : obstacles) {
                    if (box.intersects(headTestMin, headTestMax)) {
                        headBlocked = true;
                        break;
                    }
                }
                if (!headBlocked) {
                    position.y = elevatedFeet + eyeHeight;
                    feetY = elevatedFeet;
                    headY = elevatedHead;
                }
            }
        }

        // ==================== 1. X-AXIS SLIDING RESOLUTION (SWEPT AABB) ====================
        if (std::abs(velocity.x) > 1e-4f) {
            float dx = velocity.x * dt;
            float newX = position.x + dx;

            float minX = std::min(position.x, newX) - playerRadius;
            float maxX = std::max(position.x, newX) + playerRadius;
            Vec3 testMin = { minX, feetY + 0.04f, position.z - playerRadius };
            Vec3 testMax = { maxX, headY - 0.04f, position.z + playerRadius };

            bool hitX = false;
            float clampedX = newX;
            for (const auto& box : obstacles) {
                if (box.intersects(testMin, testMax)) {
                    hitX = true;
                    if (dx > 0.0f) {
                        float stopX = box.min.x - playerRadius - 0.001f;
                        clampedX = std::min(clampedX, stopX);
                    } else {
                        float stopX = box.max.x + playerRadius + 0.001f;
                        clampedX = std::max(clampedX, stopX);
                    }
                }
            }
            if (hitX) {
                position.x = clampedX;
                velocity.x = 0.0f;
            } else {
                position.x = newX;
            }
        }

        // ==================== 2. Z-AXIS SLIDING RESOLUTION (SWEPT AABB) ====================
        if (std::abs(velocity.z) > 1e-4f) {
            float dz = velocity.z * dt;
            float newZ = position.z + dz;

            float minZ = std::min(position.z, newZ) - playerRadius;
            float maxZ = std::max(position.z, newZ) + playerRadius;
            Vec3 testMin = { position.x - playerRadius, feetY + 0.04f, minZ };
            Vec3 testMax = { position.x + playerRadius, headY - 0.04f, maxZ };

            bool hitZ = false;
            float clampedZ = newZ;
            for (const auto& box : obstacles) {
                if (box.intersects(testMin, testMax)) {
                    hitZ = true;
                    if (dz > 0.0f) {
                        float stopZ = box.min.z - playerRadius - 0.001f;
                        clampedZ = std::min(clampedZ, stopZ);
                    } else {
                        float stopZ = box.max.z + playerRadius + 0.001f;
                        clampedZ = std::max(clampedZ, stopZ);
                    }
                }
            }
            if (hitZ) {
                position.z = clampedZ;
                velocity.z = 0.0f;
            } else {
                position.z = newZ;
            }
        }

        // ==================== 3. Y-AXIS VERTICAL RESOLUTION (SWEPT AABB) ====================
        float dy = velocity.y * dt;
        float newY = position.y + dy;
        float newFeetY = newY - eyeHeight;
        float newHeadY = newFeetY + playerHeight;

        float shrink = 0.03f;
        float minY = std::min(feetY, newFeetY);
        float maxY = std::max(headY, newHeadY);
        Vec3 testMin = { position.x - playerRadius + shrink, minY, position.z - playerRadius + shrink };
        Vec3 testMax = { position.x + playerRadius - shrink, maxY, position.z + playerRadius - shrink };

        bool landed = false;
        bool hitCeil = false;
        float highestFloor = -1e9f;
        float lowestCeil = 1e9f;

        for (const auto& box : obstacles) {
            if (box.intersects(testMin, testMax)) {
                if (dy <= 0.0f && box.max.y <= feetY + 0.35f) {
                    landed = true;
                    highestFloor = std::max(highestFloor, box.max.y);
                } else if (dy > 0.0f && box.min.y >= headY - 0.35f) {
                    hitCeil = true;
                    lowestCeil = std::min(lowestCeil, box.min.y);
                }
            }
        }

        if (landed) {
            position.y = highestFloor + eyeHeight;
            velocity.y = 0.0f;
            isGrounded = true;
        } else if (hitCeil) {
            position.y = lowestCeil - (playerHeight - eyeHeight) - 0.002f;
            velocity.y = 0.0f;
        } else {
            position.y = newY;
            isGrounded = false;
        }

        // Ground safety fallback (map baseplate floor)
        if (position.y < eyeHeight) {
            position.y = eyeHeight;
            velocity.y = 0.0f;
            isGrounded = true;
        }

        // ==================== 4. DE-PENETRATION PASS ====================
        resolvePenetration(position, obstacles, eyeHeight, playerRadius, playerHeight);
    }

    void LabCollision::resolvePenetration(Vec3& position, const std::vector<CollisionBox>& obstacles,
                                          float eyeHeight, float playerRadius,
                                          float playerHeight) {
        for (int iter = 0; iter < 3; ++iter) {
            float feetY = position.y - eyeHeight;
            float headY = feetY + playerHeight;

            Vec3 pMin = { position.x - playerRadius, feetY, position.z - playerRadius };
            Vec3 pMax = { position.x + playerRadius, headY, position.z + playerRadius };

            bool hadPenetration = false;
            for (const auto& box : obstacles) {
                if (box.intersects(pMin, pMax)) {
                    hadPenetration = true;
                    float dx1 = pMax.x - box.min.x;
                    float dx2 = box.max.x - pMin.x;
                    float penX = (dx1 < dx2) ? -dx1 : dx2;

                    float dz1 = pMax.z - box.min.z;
                    float dz2 = box.max.z - pMin.z;
                    float penZ = (dz1 < dz2) ? -dz1 : dz2;

                    float dy1 = pMax.y - box.min.y;
                    float dy2 = box.max.y - pMin.y;
                    float penY = (dy1 < dy2) ? -dy1 : dy2;

                    float absX = std::abs(penX);
                    float absY = std::abs(penY);
                    float absZ = std::abs(penZ);

                    // Push out along minimum overlap axis
                    if (absX <= absY && absX <= absZ) {
                        position.x += (penX > 0 ? penX + 0.002f : penX - 0.002f);
                    } else if (absZ <= absX && absZ <= absY) {
                        position.z += (penZ > 0 ? penZ + 0.002f : penZ - 0.002f);
                    } else {
                        position.y += (penY > 0 ? penY + 0.002f : penY - 0.002f);
                    }
                    break;
                }
            }
            if (!hadPenetration) break;
        }
    }

} // namespace Lab
