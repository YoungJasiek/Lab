#include "LabCollision.h"
#include "LabScene.h"
#include "LabComponents.h"

namespace Lab {

    std::vector<CollisionBox> LabCollision::getMapSolidBoxes(const LabMap& map) {
        std::vector<CollisionBox> boxes;
        boxes.reserve(map.brushes.size() + map.doors.size() + map.props.size());

        // 1. Add all solid brushes (static scene walls, floors, ceilings, pillars)
        for (size_t i = 0; i < map.brushes.size(); ++i) {
            const auto& b = map.brushes[i];
            if (!b.collidable || b.type == "trigger" || b.name.rfind("trigger_", 0) == 0) {
                continue;
            }
            CollisionBox box;
            if (b.type == "poly" && !b.customVertices.empty()) {
                Vec3 vMin = b.customVertices[0].position + b.position;
                Vec3 vMax = vMin;
                for (const auto& v : b.customVertices) {
                    Vec3 wp = v.position + b.position;
                    vMin.x = std::min(vMin.x, wp.x);
                    vMin.y = std::min(vMin.y, wp.y);
                    vMin.z = std::min(vMin.z, wp.z);
                    vMax.x = std::max(vMax.x, wp.x);
                    vMax.y = std::max(vMax.y, wp.y);
                    vMax.z = std::max(vMax.z, wp.z);
                }
                box.min = vMin;
                box.max = vMax;
            } else {
                Vec3 half = b.size * 0.5f;
                float ry = std::fmod(std::abs(b.rotation.y), 180.0f);
                if (std::abs(ry - 90.0f) < 35.0f) {
                    std::swap(half.x, half.z);
                }
                box.min = b.position - half;
                box.max = b.position + half;
            }
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

        // 3. Dynamically generate collision boxes for solid 3D props in the scene
        for (size_t i = 0; i < map.props.size(); ++i) {
            const auto& p = map.props[i];
            if (!p.collidable || p.hasDecal || p.hasWaterVolume || p.hasForceField) {
                continue;
            }
            // Non-blocking ceiling fixtures and flat wall decals do not block player locomotion
            if (p.modelPath.find("wall_stain") != std::string::npos ||
                p.modelPath.find("ceiling_lamp") != std::string::npos ||
                p.modelPath.find("fluorescent") != std::string::npos) {
                continue;
            }

            Vec3 half = Vec3(std::abs(p.scale.x) * 0.5f, std::abs(p.scale.y) * 0.5f, std::abs(p.scale.z) * 0.5f);
            if (p.modelPath.find("lab_door") != std::string::npos || p.modelPath.find("rollershutter_door") != std::string::npos) {
                half = Vec3(0.95f * std::abs(p.scale.x), 1.15f * std::abs(p.scale.y), 0.14f * std::abs(p.scale.z));
            } else if (p.modelPath.find("lab_locker") != std::string::npos || p.modelPath.find("worn_metal_rack") != std::string::npos) {
                half = Vec3(0.68f * std::abs(p.scale.x), 0.92f * std::abs(p.scale.y), 0.26f * std::abs(p.scale.z));
            } else if (p.modelPath.find("corridor_pipe") != std::string::npos || p.modelPath.find("industrial_pipes") != std::string::npos) {
                half = Vec3(0.16f * std::abs(p.scale.x), 0.22f * std::abs(p.scale.y), 1.50f * std::abs(p.scale.z));
            }

            float ry = std::fmod(std::abs(p.rotation.y), 180.0f);
            if (std::abs(ry - 90.0f) < 35.0f) {
                std::swap(half.x, half.z);
            }

            CollisionBox box;
            box.min = p.position - half;
            box.max = p.position + half;
            box.tag = EntityTag::Prop;
            box.entityIndex = static_cast<int>(i);
            boxes.push_back(box);
        }

        // 4. Add dynamic ECS scene entity BoxColliders if map is a LabScene
        if (const auto* scene = dynamic_cast<const LabScene*>(&map)) {
            for (size_t i = 0; i < scene->entities.size(); ++i) {
                const auto& ent = scene->entities[i];
                if (!ent || !ent->active || !ent->transform) continue;
                auto* col = ent->GetComponent<BoxCollider>();
                if (col && col->enabled && !col->isTrigger) {
                    Vec3 center = ent->transform->position + col->offset;
                    Vec3 half = Vec3(
                        col->size.x * std::abs(ent->transform->scale.x) * 0.5f,
                        col->size.y * std::abs(ent->transform->scale.y) * 0.5f,
                        col->size.z * std::abs(ent->transform->scale.z) * 0.5f
                    );
                    CollisionBox box;
                    box.min = center - half;
                    box.max = center + half;
                    box.tag = EntityTag::World;
                    box.entityIndex = static_cast<int>(i);
                    boxes.push_back(box);
                }
            }
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

    std::vector<int> LabCollision::checkTriggerVolumes(const Vec3& position, LabMap& map,
                                                       float eyeHeight, float playerRadius,
                                                       float playerHeight) {
        std::vector<int> fired;
        float feetY = position.y - eyeHeight;
        float headY = feetY + playerHeight;
        Vec3 pMin = { position.x - playerRadius, feetY, position.z - playerRadius };
        Vec3 pMax = { position.x + playerRadius, headY, position.z + playerRadius };

        for (size_t i = 0; i < map.triggers.size(); ++i) {
            auto& tr = map.triggers[i];
            if (tr.once && tr.triggered) continue;

            Vec3 half = tr.size * 0.5f;
            CollisionBox box;
            box.min = tr.position - half;
            box.max = tr.position + half;
            if (box.intersects(pMin, pMax)) {
                tr.triggered = true;
                fired.push_back(static_cast<int>(i));
            }
        }
        return fired;
    }

} // namespace Lab

