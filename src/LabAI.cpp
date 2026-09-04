#include "LabAI.h"
#include "LabRenderer.h"
#include <cmath>
#include <algorithm>

namespace Lab {

    CombatBot::CombatBot(int botId, const std::string& botName, const Vec3& spawnPos, const Vec3& pEnd, int botTeam)
        : id(botId), name(botName), position(spawnPos), team(botTeam),
          patrolStart(spawnPos), patrolEnd(pEnd) {
    }

    void CombatBot::getHitboxes(Vec3& headMin, Vec3& headMax, Vec3& bodyMin, Vec3& bodyMax) const {
        Vec3 headCenter = position + Vec3(0.0f, 1.68f, 0.0f);
        Vec3 headHalf   = Vec3(0.24f, 0.22f, 0.24f);
        headMin = headCenter - headHalf;
        headMax = headCenter + headHalf;

        Vec3 bodyCenter = position + Vec3(0.0f, 0.85f, 0.0f);
        Vec3 bodyHalf   = Vec3(0.38f, 0.65f, 0.38f);
        bodyMin = bodyCenter - bodyHalf;
        bodyMax = bodyCenter + bodyHalf;
    }

    bool CombatBot::hasLineOfSight(const Vec3& from, const Vec3& to, const LabMap& map) {
        Vec3 toVec = to - from;
        float dist = toVec.length();
        if (dist < 0.001f) return true;

        Vec3 rayDir = toVec / dist;
        float hitT = 0.0f;

        // Test solid brushes
        for (const auto& b : map.brushes) {
            Vec3 half = b.size * 0.5f;
            if (Raycast::rayIntersectAABB(from, rayDir, b.position - half, b.position + half, hitT)) {
                if (hitT > 0.1f && hitT < dist - 0.2f) {
                    return false; // Obstructed by wall
                }
            }
        }
        return true;
    }

    void CombatBot::update(float dt, const Vec3& playerPos, const LabMap& map,
                           std::vector<BulletTracer>& outTracers, float& outDamageToPlayer) {
        if (state == AIState::Dead) {
            deathTimer += dt;
            respawnTimer -= dt;
            if (respawnTimer <= 0.0f) {
                // Respawn bot at patrol start
                state = AIState::Patrol;
                health = maxHealth;
                position = patrolStart;
                rotation.x = 0.0f;
                patrolT = 0.0f;
                patrolDir = 1;
                shootCooldown = shootInterval;
                hurtTimer = 0.0f;
                muzzleFlashTimer = 0.0f;
            }
            return;
        }

        if (hurtTimer > 0.0f) hurtTimer -= dt;
        if (muzzleFlashTimer > 0.0f) muzzleFlashTimer -= dt;

        Vec3 vecToPlayer = playerPos - position;
        float distToPlayer = vecToPlayer.length();
        Vec3 eyePos = position + Vec3(0.0f, 1.6f, 0.0f);
        Vec3 playerTargetPos = playerPos + Vec3(0.0f, 0.8f, 0.0f);

        bool canSee = (distToPlayer <= sightRange) && hasLineOfSight(eyePos, playerTargetPos, map);

        if (canSee) {
            // Face player
            float targetYaw = std::atan2(vecToPlayer.x, vecToPlayer.z) * 180.0f / 3.14159265f;
            rotation.y = targetYaw;

            if (distToPlayer > attackRange) {
                state = AIState::Chase;
                Vec3 moveDir = Vec3(vecToPlayer.x, 0.0f, vecToPlayer.z).normalized();
                position = position + (moveDir * (moveSpeed * dt));
                walkCycle += dt * 8.0f;
            } else {
                state = AIState::Attack;
                shootCooldown -= dt;
                if (shootCooldown <= 0.0f) {
                    shootCooldown = shootInterval;
                    muzzleFlashTimer = 0.08f;

                    // Bullet tracer from bot weapon muzzle
                    Vec3 gunMuzzle = position + Vec3(0.2f, 1.15f, 0.3f);
                    bool hit = (rand() % 100) < 70; // 70% accuracy

                    Vec3 tracerEnd = playerTargetPos;
                    if (!hit) {
                        float missOffset = ((rand() % 100) / 50.0f - 1.0f) * 1.5f;
                        tracerEnd = tracerEnd + Vec3(missOffset, missOffset * 0.5f, -missOffset);
                    } else {
                        outDamageToPlayer += 14.0f; // Deal 14 damage to player
                    }

                    BulletTracer tr;
                    tr.start = gunMuzzle;
                    tr.end = tracerEnd;
                    tr.color = Vec3(1.0f, 0.35f, 0.2f); // Red/Orange enemy tracer
                    tr.lifetime = 0.0f;
                    tr.maxLifetime = 0.09f;
                    outTracers.push_back(tr);
                }
            }
        } else {
            // Return to / continue patrol
            state = AIState::Patrol;
            float pathLen = (patrolEnd - patrolStart).length();
            if (pathLen > 0.5f) {
                patrolT += patrolDir * (moveSpeed / pathLen) * dt;
                if (patrolT >= 1.0f) {
                    patrolT = 1.0f;
                    patrolDir = -1;
                } else if (patrolT <= 0.0f) {
                    patrolT = 0.0f;
                    patrolDir = 1;
                }
                position = patrolStart * (1.0f - patrolT) + patrolEnd * patrolT;
                walkCycle += dt * 6.0f;

                Vec3 dir = (patrolDir > 0) ? (patrolEnd - patrolStart) : (patrolStart - patrolEnd);
                rotation.y = std::atan2(dir.x, dir.z) * 180.0f / 3.14159265f;
            }
        }
    }

    bool CombatBot::takeDamage(float damage, bool isHeadshot) {
        if (state == AIState::Dead) return false;

        float finalDmg = isHeadshot ? (damage * 2.5f) : damage;
        health -= finalDmg;
        hurtTimer = 0.22f;

        if (health <= 0.0f) {
            health = 0.0f;
            state = AIState::Dead;
            deaths++;
            respawnTimer = 4.5f;
            rotation.x = -80.0f; // Collapse back onto ground
            position.y = 0.25f;
            return true; // Just died
        }

        state = AIState::Chase; // Immediately alert bot on hit
        return false;
    }

    void CombatBot::render() const {
        if (state == AIState::Dead) {
            // Render defeated bot on floor
            Renderer::drawCube(position + Vec3(0.0f, 0.15f, 0.0f), Vec3(80.0f, rotation.y, 0.0f),
                               Vec3(0.55f, 0.25f, 1.35f), Vec3(0.18f, 0.18f, 0.20f));
            return;
        }

        float legSwing = std::sin(walkCycle) * 0.28f;
        float bodyBob = std::abs(std::sin(walkCycle * 2.0f)) * 0.04f;
        Vec3 bPos = position + Vec3(0.0f, bodyBob, 0.0f);

        // Armor plate colors based on damage and team
        Vec3 armorCol = Vec3(0.20f, 0.24f, 0.28f);
        if (hurtTimer > 0.0f) {
            armorCol = Vec3(0.9f, 0.2f, 0.2f); // Red flash when shot
        } else if (team == 0) {
            armorCol = Vec3(0.48f, 0.18f, 0.18f); // Red Team
        } else if (team == 1) {
            armorCol = Vec3(0.18f, 0.32f, 0.52f); // Blue Team
        }

        // Glowing Visor
        Vec3 visorCol = Vec3(0.2f, 0.85f, 1.0f); // Cold Cyan
        if (state == AIState::Attack) {
            visorCol = Vec3(1.0f, 0.15f, 0.1f); // Aggressive Red
        } else if (state == AIState::Chase) {
            visorCol = Vec3(1.0f, 0.65f, 0.1f); // Alert Amber
        }

        // 1. Torso
        Renderer::drawCube(bPos + Vec3(0.0f, 1.15f, 0.0f), rotation, Vec3(0.52f, 0.72f, 0.36f), armorCol);
        // 2. Chest Armor plate
        Renderer::drawCube(bPos + Vec3(0.0f, 1.22f, 0.05f), rotation, Vec3(0.44f, 0.46f, 0.30f), Vec3(0.14f, 0.16f, 0.18f));
        // 3. Head & Visor
        Renderer::drawCube(bPos + Vec3(0.0f, 1.68f, 0.0f), rotation, Vec3(0.32f, 0.28f, 0.32f), Vec3(0.12f, 0.14f, 0.17f));
        Renderer::drawCube(bPos + Vec3(0.0f, 1.68f, 0.17f), rotation, Vec3(0.24f, 0.08f, 0.04f), visorCol, nullptr, false);
        // 4. Animated Legs
        Renderer::drawCube(bPos + Vec3(-0.16f, 0.45f, legSwing), rotation, Vec3(0.13f, 0.8f, 0.15f), Vec3(0.15f, 0.15f, 0.17f));
        Renderer::drawCube(bPos + Vec3(0.16f, 0.45f, -legSwing), rotation, Vec3(0.13f, 0.8f, 0.15f), Vec3(0.15f, 0.15f, 0.17f));

        // 5. Combat Assault Rifle
        Vec3 gunPos = bPos + Vec3(0.24f, 1.15f, 0.25f);
        Renderer::drawCube(gunPos, rotation, Vec3(0.08f, 0.12f, 0.55f), Vec3(0.09f, 0.09f, 0.11f));
        if (muzzleFlashTimer > 0.0f) {
            Renderer::drawCube(gunPos + Vec3(0.0f, 0.0f, 0.32f), rotation, Vec3(0.16f, 0.16f, 0.16f), Vec3(1.0f, 0.9f, 0.2f), nullptr, false);
        }

        // 6. Overhead Health Bar
        float hpRatio = std::clamp(health / maxHealth, 0.0f, 1.0f);
        Renderer::drawCube(bPos + Vec3(0.0f, 2.05f, 0.0f), Vec3(0.0f, rotation.y, 0.0f), Vec3(0.7f, 0.06f, 0.03f), Vec3(0.15f, 0.15f, 0.15f), nullptr, false);
        Vec3 hpColor = (team == 0) ? Vec3(0.9f, 0.2f, 0.2f) : Vec3(0.2f, 0.85f, 0.3f);
        Renderer::drawCube(bPos + Vec3(0.0f, 2.05f, 0.01f), Vec3(0.0f, rotation.y, 0.0f), Vec3(0.68f * hpRatio, 0.05f, 0.03f), hpColor, nullptr, false);
    }

    void AIManager::spawnBotsForMap(const std::string& mapName, int count, GameMode mode) {
        (void)mapName;
        clear();
        if (count <= 0) return;

        struct SpawnNode {
            Vec3 start;
            Vec3 end;
        };

        std::vector<SpawnNode> nodes = {
            { Vec3(0.0f, 0.0f, -8.0f),   Vec3(6.0f, 0.0f, -8.0f) },
            { Vec3(-10.0f, 0.0f, 0.0f),  Vec3(-10.0f, 0.0f, 8.0f) },
            { Vec3(10.0f, 0.0f, 4.0f),   Vec3(10.0f, 0.0f, -4.0f) },
            { Vec3(-6.0f, 0.0f, -14.0f), Vec3(6.0f, 0.0f, -14.0f) },
            { Vec3(-12.0f, 0.0f, 10.0f), Vec3(-2.0f, 0.0f, 10.0f) },
            { Vec3(12.0f, 0.0f, 10.0f),  Vec3(12.0f, 0.0f, 2.0f) },
            { Vec3(-7.0f, 0.0f, -5.0f),  Vec3(-7.0f, 0.0f, 5.0f) },
            { Vec3(7.0f, 0.0f, -5.0f),   Vec3(7.0f, 0.0f, 5.0f) }
        };

        int toSpawn = std::min(count, (int)nodes.size());
        for (int i = 0; i < toSpawn; ++i) {
            int botTeam = (mode == GameMode::TDM) ? (i % 2) : -1;
            std::string bName = "Bot #" + std::to_string(i + 1);
            bots.emplace_back(i, bName, nodes[i].start, nodes[i].end, botTeam);
        }
    }

    void AIManager::update(float dt, const Vec3& playerPos, const LabMap& map,
                           std::vector<BulletTracer>& outTracers, float& outDamageToPlayer) {
        for (auto& bot : bots) {
            bot.update(dt, playerPos, map, outTracers, outDamageToPlayer);
        }
    }

    bool AIManager::testRaycast(const Vec3& rayOrigin, const Vec3& rayDir, RaycastHit& outHit, int excludeBotId) {
        bool hitAny = false;
        Vec3 hitNorm;

        for (auto& bot : bots) {
            if (!bot.isAlive() || bot.id == excludeBotId) continue;

            Vec3 headMin, headMax, bodyMin, bodyMax;
            bot.getHitboxes(headMin, headMax, bodyMin, bodyMax);

            float tHead = 0.0f;
            if (Raycast::rayIntersectAABB(rayOrigin, rayDir, headMin, headMax, tHead, &hitNorm)) {
                if (tHead < outHit.distance) {
                    outHit.hit = true;
                    outHit.distance = tHead;
                    outHit.point = rayOrigin + rayDir * tHead;
                    outHit.normal = hitNorm;
                    outHit.tag = EntityTag::Bot;
                    outHit.entityIndex = bot.id;
                    outHit.isHeadshot = true;
                    hitAny = true;
                }
            }

            float tBody = 0.0f;
            if (Raycast::rayIntersectAABB(rayOrigin, rayDir, bodyMin, bodyMax, tBody, &hitNorm)) {
                if (tBody < outHit.distance) {
                    outHit.hit = true;
                    outHit.distance = tBody;
                    outHit.point = rayOrigin + rayDir * tBody;
                    outHit.normal = hitNorm;
                    outHit.tag = EntityTag::Bot;
                    outHit.entityIndex = bot.id;
                    outHit.isHeadshot = false;
                    hitAny = true;
                }
            }
        }
        return hitAny;
    }

    void AIManager::render() const {
        for (const auto& bot : bots) {
            bot.render();
        }
    }

} // namespace Lab
