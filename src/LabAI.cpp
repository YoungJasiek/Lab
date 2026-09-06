#include "LabAI.h"
#include "LabRenderer.h"
#include "LabCollision.h"
#include "LabPickups.h"
#include "LabChat.h"
#include "LabAudio.h"
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

            // Tactical movement cycle: update strafe direction
            strafeTimer -= dt;
            if (strafeTimer <= 0.0f) {
                strafeTimer = 1.2f + static_cast<float>(rand() % 15) * 0.1f;
                strafeDirection = (rand() % 2 == 0) ? 1 : -1;
            }

            Vec3 forward = Vec3(vecToPlayer.x, 0.0f, vecToPlayer.z).normalized();
            Vec3 right = Vec3(forward.z, 0.0f, -forward.x);

            Vec3 moveVel{ 0.0f, 0.0f, 0.0f };
            if (distToPlayer > 8.0f) {
                state = AIState::Chase;
                moveVel = forward * (moveSpeed * 1.0f) + right * (static_cast<float>(strafeDirection) * moveSpeed * 0.45f);
            } else if (distToPlayer > 3.5f) {
                state = AIState::Attack;
                moveVel = forward * (moveSpeed * 0.35f) + right * (static_cast<float>(strafeDirection) * moveSpeed * 0.85f);
            } else {
                state = AIState::Attack;
                moveVel = -forward * (moveSpeed * 0.75f) + right * (static_cast<float>(strafeDirection) * moveSpeed * 0.6f);
            }

            auto solidBoxes = LabCollision::getMapSolidBoxes(map);
            Vec3 nextPos = position;
            Vec3 tempVel = moveVel;
            bool grounded = true;
            LabCollision::moveAndSlide(nextPos, tempVel, grounded, dt, solidBoxes, 0.0f, 0.35f, 1.85f);
            position = nextPos;
            walkCycle += dt * 8.0f;

            // Combat weapons firing
            shootCooldown -= dt;
            if (shootCooldown <= 0.0f) {
                shootCooldown = shootInterval;
                muzzleFlashTimer = 0.08f;

                Vec3 gunMuzzle = position + Vec3(0.2f, 1.15f, 0.3f);
                AudioEngine::playSound3D(SoundID::SG553Shot, gunMuzzle, 0.85f);
                bool hit = (rand() % 100) < 68;
                Vec3 tracerEnd = playerTargetPos;
                if (!hit) {
                    float missOffset = ((rand() % 100) / 50.0f - 1.0f) * 1.2f;
                    tracerEnd = tracerEnd + Vec3(missOffset, missOffset * 0.5f, -missOffset);
                } else {
                    outDamageToPlayer += 14.0f;
                }

                BulletTracer tr;
                tr.start = gunMuzzle;
                tr.end = tracerEnd;
                tr.color = Vec3(1.0f, 0.35f, 0.2f);
                tr.lifetime = 0.0f;
                tr.maxLifetime = 0.09f;
                tr.thickness = 0.035f;
                outTracers.push_back(tr);
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

        // Update glTF 2.0 Skeletal Animator
        if (animator.getSkeleton()) {
            if (state == AIState::Attack) {
                if (muzzleFlashTimer > 0.0f) {
                    animator.playAnimation("Shoot", false);
                } else {
                    animator.playAnimation("Idle", true);
                }
            } else if (state == AIState::Chase || state == AIState::Patrol) {
                animator.playAnimation("Walk", true);
            } else {
                animator.playAnimation("Idle", true);
            }
            animator.update(dt);
        }
    }

    void CombatBot::initAnimation(std::shared_ptr<Skeleton> skel, const std::vector<AnimationClip>& clips) {
        if (skel) {
            animator.setSkeleton(skel);
            for (const auto& clip : clips) {
                animator.addClip(clip);
            }
            animator.playAnimation("Idle", true);
        }
    }

    bool CombatBot::takeDamage(float damage, bool isHeadshot, const Vec3& knockback) {
        if (state == AIState::Dead) return false;

        float finalDmg = isHeadshot ? (damage * 2.5f) : damage;
        health -= finalDmg;
        hurtTimer = 0.22f;

        if (knockback.lengthSq() > 0.01f) {
            velocity += knockback;
        }

        if (health <= 0.0f) {
            health = 0.0f;
            state = AIState::Dead;
            deaths++;
            respawnTimer = 4.5f;
            rotation.x = -80.0f; // Collapse back onto ground
            if (velocity.lengthSq() < 0.1f) {
                position.y = 0.25f;
            }
            return true; // Just died
        }

        state = AIState::Chase; // Immediately alert bot on hit
        return false;
    }

    void CombatBot::render(const SkinnedMesh* mesh, const Mesh* weaponMesh) const {
        if (state == AIState::Dead) {
            // Render defeated bot on floor
            Renderer::drawCube(position + Vec3(0.0f, 0.15f, 0.0f), Vec3(80.0f, rotation.y, 0.0f),
                               Vec3(0.55f, 0.25f, 1.35f), Vec3(0.18f, 0.18f, 0.20f));
            return;
        }

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

        if (mesh && animator.getSkeleton()) {
            Mat4 botModel = Mat4::translate(bPos) * Mat4::rotate(rotation.y * 3.14159265f / 180.0f, Vec3(0, 1, 0));
            Renderer::drawSkinnedMesh(*mesh, botModel, animator.getSkinMatrices(), armorCol, nullptr, true);

            if (weaponMesh) {
                Mat4 weaponSocket = animator.getSocketTransform("Socket_Weapon", botModel,
                    makeTransform(Vec3(0.0f, -0.05f, 0.02f), Quat::fromEuler(0.1f, -0.2f, 0.0f), Vec3(0.016f, 0.016f, 0.016f)));
                Renderer::drawMesh(*weaponMesh, weaponSocket, Vec3(0.85f, 0.85f, 0.88f), nullptr, true);
            }
        } else {
            float legSwing = std::sin(walkCycle) * 0.28f;
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
        }

        if (muzzleFlashTimer > 0.0f) {
            Vec3 mPos = bPos + Vec3(0.24f, 1.15f, 0.57f);
            Renderer::drawCube(mPos, rotation, Vec3(0.16f, 0.16f, 0.16f), Vec3(1.0f, 0.9f, 0.2f), nullptr, false);
        }

        // 6. Overhead Health Bar
        float hpRatio = std::clamp(health / maxHealth, 0.0f, 1.0f);
        Renderer::drawCube(bPos + Vec3(0.0f, 2.05f, 0.0f), Vec3(0.0f, rotation.y, 0.0f), Vec3(0.7f, 0.06f, 0.03f), Vec3(0.15f, 0.15f, 0.15f), nullptr, false);
        Vec3 hpColor = (team == 0) ? Vec3(0.9f, 0.2f, 0.2f) : Vec3(0.2f, 0.85f, 0.3f);
        Renderer::drawCube(bPos + Vec3(0.0f, 2.05f, 0.01f), Vec3(0.0f, rotation.y, 0.0f), Vec3(0.68f * hpRatio, 0.05f, 0.03f), hpColor, nullptr, false);
    }

    void CombatBot::renderShadow(const SkinnedMesh* mesh, const Mesh* weaponMesh) const {
        if (!isAlive()) return;
        float bodyBob = std::abs(std::sin(walkCycle * 2.0f)) * 0.04f;
        Vec3 bPos = position + Vec3(0.0f, bodyBob, 0.0f);
        Mat4 botModel = Mat4::translate(bPos) * Mat4::rotate(rotation.y * 3.14159265f / 180.0f, Vec3(0, 1, 0));

        if (mesh && animator.getSkeleton()) {
            Renderer::drawShadowSkinnedMesh(*mesh, botModel, animator.getSkinMatrices());
            if (weaponMesh) {
                Mat4 weaponSocket = animator.getSocketTransform("Socket_Weapon", botModel,
                    makeTransform(Vec3(0.0f, -0.05f, 0.02f), Quat::fromEuler(0.1f, -0.2f, 0.0f), Vec3(0.016f, 0.016f, 0.016f)));
                Renderer::drawShadowMesh(*weaponMesh, weaponSocket);
            }
        } else {
            Renderer::drawShadowCube(bPos + Vec3(0.0f, 1.0f, 0.0f), Vec3(0.6f, 1.8f, 0.6f));
        }
    }

    void AIManager::initAssets() {
        if (!skinnedMesh) {
            GLTFLoader::load("assets/animations/bot_walk.gltf", skeleton, animations, skinnedMesh);
            weaponMesh = std::unique_ptr<Mesh>(Mesh::loadSTL("assets/models/pipe.stl"));
        }
    }

    void AIManager::spawnBotsForMap(const LabMap* map, int count, GameMode mode) {
        clear();
        if (count <= 0) return;
        initAssets();

        for (int i = 0; i < count; ++i) {
            int botTeam = (mode == GameMode::TDM) ? (i % 2) : -1;
            std::string bName = "Bot #" + std::to_string(i + 1);

            MapSpawnPoint sp;
            if (map) {
                auto teamSpawns = map->getSpawnsForTeam(mode, botTeam);
                if (!teamSpawns.empty()) {
                    sp = teamSpawns[i % teamSpawns.size()];
                } else {
                    sp.position = map->spawn.position;
                    sp.yaw = map->spawn.yaw;
                }
            } else {
                sp.position = Vec3(-8.0f + (i % 4) * 5.0f, 0.0f, -6.0f + (i / 4) * 6.0f);
                sp.yaw = 0.0f;
            }

            // Slight spatial offset so bots at the same spawn point do not overlap initially
            float offsetX = ((i % 3) - 1) * 0.75f;
            float offsetZ = ((i / 3) - 1) * 0.75f;
            Vec3 botSpawnPos = sp.position + Vec3(offsetX, 0.0f, offsetZ);
            if (botSpawnPos.y >= 1.5f) {
                botSpawnPos.y = std::max(0.0f, botSpawnPos.y - 1.8f);
            }

            float rad = sp.yaw * 3.14159265f / 180.0f;
            Vec3 forward(std::sin(rad), 0.0f, std::cos(rad));
            if (forward.lengthSq() < 0.01f) forward = Vec3(0, 0, 1);
            Vec3 patrolEnd = botSpawnPos + forward * 8.0f;

            bots.emplace_back(i, bName, botSpawnPos, patrolEnd, botTeam);
            bots.back().rotation.y = sp.yaw;
            if (skeleton) {
                bots.back().initAnimation(skeleton, animations);
            }
        }
    }

    void AIManager::spawnBotsForMap(const std::string& mapName, int count, GameMode mode) {
        auto loaded = LabMap::loadFromFile(mapName);
        spawnBotsForMap(loaded.get(), count, mode);
    }

    void AIManager::update(float dt, const Vec3& playerPos, bool isPlayerAlive, int playerTeam,
                           const LabMap& map, std::vector<BulletTracer>& outTracers, float& outDamageToPlayer,
                           PickupManager* pickupMgr, LabChat* chat) {
        auto solidBoxes = LabCollision::getMapSolidBoxes(map);

        for (size_t i = 0; i < bots.size(); ++i) {
            auto& bot = bots[i];
            if (!bot.isAlive()) {
                if (bot.velocity.lengthSq() > 0.01f) {
                    bot.velocity.y -= 14.5f * dt;
                    bot.position += bot.velocity * dt;
                    if (bot.position.y <= 0.25f) {
                        bot.position.y = 0.25f;
                        bot.velocity.y = -bot.velocity.y * 0.25f;
                        bot.velocity.x *= 0.75f;
                        bot.velocity.z *= 0.75f;
                    }
                }
                bot.deathTimer += dt;
                bot.respawnTimer -= dt;
                if (bot.respawnTimer <= 0.0f) {
                    bot.state = AIState::Patrol;
                    bot.health = bot.maxHealth;
                    bot.velocity = Vec3(0, 0, 0);
                    bot.rotation.x = 0.0f;

                    // Collect active enemy positions for anti-spawncamp selection
                    std::vector<Vec3> enemies;
                    if (isPlayerAlive) {
                        if (bot.team == -1 || (playerTeam != -1 && bot.team != playerTeam)) {
                            enemies.push_back(playerPos);
                        }
                    }
                    for (size_t other = 0; other < bots.size(); ++other) {
                        if (other != i && bots[other].isAlive()) {
                            if (bot.team == -1 || bots[other].team != bot.team) {
                                enemies.push_back(bots[other].position);
                            }
                        }
                    }

                    GameMode botMode = (bot.team == -1) ? GameMode::FFA : GameMode::TDM;
                    MapSpawnPoint sp = map.selectBestSpawn(botMode, bot.team, enemies);

                    bot.position = sp.position;
                    bot.rotation = Vec3(0.0f, sp.yaw, 0.0f);
                    bot.patrolStart = sp.position;
                    float rad = sp.yaw * 3.14159265f / 180.0f;
                    Vec3 forward(std::sin(rad), 0.0f, std::cos(rad));
                    if (forward.lengthSq() < 0.01f) forward = Vec3(0, 0, 1);
                    bot.patrolEnd = sp.position + forward * 8.0f;

                    bot.patrolT = 0.0f;
                    bot.patrolDir = 1;
                    bot.shootCooldown = bot.shootInterval;
                    bot.hurtTimer = 0.0f;
                    bot.muzzleFlashTimer = 0.0f;
                }
                continue;
            }

            if (bot.hurtTimer > 0.0f) bot.hurtTimer -= dt;
            if (bot.muzzleFlashTimer > 0.0f) bot.muzzleFlashTimer -= dt;

            // ==================== MULTI-TARGET SELECTION ====================
            // Find closest visible enemy (either the player or another enemy bot)
            float bestDist = 1e9f;
            Vec3 bestTargetPos{ 0.0f, 0.0f, 0.0f };
            bool targetIsPlayer = false;
            int targetBotIdx = -1;

            Vec3 botEye = bot.position + Vec3(0.0f, 1.6f, 0.0f);

            // 1. Evaluate Player
            if (isPlayerAlive && (playerTeam == -1 || playerTeam != bot.team)) {
                Vec3 toPlayer = playerPos - bot.position;
                float d = toPlayer.length();
                if (d <= bot.sightRange) {
                    Vec3 playerEye = playerPos + Vec3(0.0f, 0.8f, 0.0f);
                    if (CombatBot::hasLineOfSight(botEye, playerEye, map)) {
                        bestDist = d;
                        bestTargetPos = playerPos;
                        targetIsPlayer = true;
                    }
                }
            }

            // 2. Evaluate other Bots (FFA: all other bots; TDM: opposing team bots)
            for (size_t j = 0; j < bots.size(); ++j) {
                if (i == j || !bots[j].isAlive()) continue;
                if (bot.team != -1 && bot.team == bots[j].team) continue; // Teammate in TDM

                Vec3 toBot = bots[j].position - bot.position;
                float d = toBot.length();
                if (d <= bot.sightRange && d < bestDist) {
                    Vec3 targetBotEye = bots[j].position + Vec3(0.0f, 1.6f, 0.0f);
                    if (CombatBot::hasLineOfSight(botEye, targetBotEye, map)) {
                        bestDist = d;
                        bestTargetPos = bots[j].position;
                        targetIsPlayer = false;
                        targetBotIdx = static_cast<int>(j);
                    }
                }
            }

            // ==================== TACTICAL COMBAT ENGAGEMENT ====================
            if (bestDist < 1e8f) {
                Vec3 vecToTarget = bestTargetPos - bot.position;
                float dist = vecToTarget.length();

                // Turn to face target
                float targetYaw = std::atan2(vecToTarget.x, vecToTarget.z) * 180.0f / 3.14159265f;
                bot.rotation.y = targetYaw;

                // Update strafe cycle timer
                bot.strafeTimer -= dt;
                if (bot.strafeTimer <= 0.0f) {
                    bot.strafeTimer = 1.2f + static_cast<float>(rand() % 15) * 0.1f;
                    bot.strafeDirection = (rand() % 2 == 0) ? 1 : -1;
                }

                Vec3 forward = Vec3(vecToTarget.x, 0.0f, vecToTarget.z).normalized();
                Vec3 right = Vec3(forward.z, 0.0f, -forward.x);

                // Tactical Movement velocity: Push, Circle, or Backpedal
                Vec3 moveVel{ 0.0f, 0.0f, 0.0f };
                if (dist > 8.0f) {
                    // Aggressive advance while strafing
                    bot.state = AIState::Chase;
                    moveVel = forward * (bot.moveSpeed * 1.0f) + right * (static_cast<float>(bot.strafeDirection) * bot.moveSpeed * 0.45f);
                } else if (dist > 3.5f) {
                    // Mid-range combat: actively circle and strafe with light forward press
                    bot.state = AIState::Attack;
                    moveVel = forward * (bot.moveSpeed * 0.35f) + right * (static_cast<float>(bot.strafeDirection) * bot.moveSpeed * 0.85f);
                } else {
                    // Too close: backpedal and evasive strafe
                    bot.state = AIState::Attack;
                    moveVel = -forward * (bot.moveSpeed * 0.75f) + right * (static_cast<float>(bot.strafeDirection) * bot.moveSpeed * 0.6f);
                }

                // Apply movement with wall sliding collision
                Vec3 nextPos = bot.position;
                Vec3 tempVel = moveVel;
                bool grounded = true;
                LabCollision::moveAndSlide(nextPos, tempVel, grounded, dt, solidBoxes, 0.0f, 0.35f, 1.85f);
                bot.position = nextPos;
                bot.walkCycle += dt * 8.0f;

                // ==================== WEAPONS FIRING ====================
                bot.shootCooldown -= dt;
                if (bot.shootCooldown <= 0.0f) {
                    bot.shootCooldown = bot.shootInterval;
                    bot.muzzleFlashTimer = 0.08f;
                    Vec3 gunMuzzle = bot.position + Vec3(0.2f, 1.15f, 0.3f);
                    bool hit = (rand() % 100) < 68;

                    if (targetIsPlayer) {
                        Vec3 targetHitPos = playerPos + Vec3(0.0f, 0.8f, 0.0f);
                        if (!hit) {
                            float miss = ((rand() % 100) / 50.0f - 1.0f) * 1.2f;
                            targetHitPos = targetHitPos + Vec3(miss, miss * 0.5f, -miss);
                        } else {
                            outDamageToPlayer += 14.0f;
                        }

                        BulletTracer tr;
                        tr.start = gunMuzzle;
                        tr.end = targetHitPos;
                        tr.color = Vec3(1.0f, 0.35f, 0.2f);
                        tr.lifetime = 0.0f;
                        tr.maxLifetime = 0.09f;
                        tr.thickness = 0.035f;
                        outTracers.push_back(tr);
                    } else if (targetBotIdx >= 0 && targetBotIdx < (int)bots.size()) {
                        Vec3 targetHitPos = bots[targetBotIdx].position + Vec3(0.0f, 1.1f, 0.0f);
                        if (!hit) {
                            float miss = ((rand() % 100) / 50.0f - 1.0f) * 1.2f;
                            targetHitPos = targetHitPos + Vec3(miss, miss * 0.5f, -miss);
                        } else {
                            bool isHeadshot = (rand() % 100) < 25;
                            float dmg = isHeadshot ? 50.0f : 25.0f;
                            bool killed = bots[targetBotIdx].takeDamage(dmg, isHeadshot);

                            if (killed) {
                                bot.kills++;
                                if (chat) {
                                    chat->addMessage("[SERVER]", bot.name + " eliminated " + bots[targetBotIdx].name, Vec3(0.85f, 0.45f, 0.2f));
                                }
                                if (pickupMgr) {
                                    pickupMgr->spawnPickup(PickupType::Ammo, bots[targetBotIdx].position + Vec3(0.0f, 0.35f, 0.0f), 36);
                                    if ((rand() % 100) < 50) {
                                        pickupMgr->spawnPickup(PickupType::Medkit, bots[targetBotIdx].position + Vec3(0.3f, 0.35f, -0.3f), 50);
                                    }
                                    if ((rand() % 100) < 40) {
                                        int rWep = 2 + (rand() % 7);
                                        pickupMgr->spawnPickup(PickupType::WeaponDrop, bots[targetBotIdx].position + Vec3(-0.35f, 0.35f, 0.2f), 30, rWep);
                                    }
                                }
                            }
                        }

                        BulletTracer tr;
                        tr.start = gunMuzzle;
                        tr.end = targetHitPos;
                        tr.color = (bot.team == 0) ? Vec3(1.0f, 0.3f, 0.2f) : Vec3(0.2f, 0.6f, 1.0f);
                        tr.lifetime = 0.0f;
                        tr.maxLifetime = 0.09f;
                        tr.thickness = 0.035f;
                        outTracers.push_back(tr);
                    }
                }
            } else {
                // ==================== PATROL NAVIGATION ====================
                bot.state = AIState::Patrol;
                float pathLen = (bot.patrolEnd - bot.patrolStart).length();
                if (pathLen > 0.5f) {
                    bot.patrolT += bot.patrolDir * (bot.moveSpeed / pathLen) * dt;
                    if (bot.patrolT >= 1.0f) {
                        bot.patrolT = 1.0f;
                        bot.patrolDir = -1;
                    } else if (bot.patrolT <= 0.0f) {
                        bot.patrolT = 0.0f;
                        bot.patrolDir = 1;
                    }
                    bot.position = bot.patrolStart * (1.0f - bot.patrolT) + bot.patrolEnd * bot.patrolT;
                    bot.walkCycle += dt * 6.0f;

                    Vec3 dir = (bot.patrolDir > 0) ? (bot.patrolEnd - bot.patrolStart) : (bot.patrolStart - bot.patrolEnd);
                    bot.rotation.y = std::atan2(dir.x, dir.z) * 180.0f / 3.14159265f;
                }
            }
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

    void AIManager::renderShadowPass() const {
        for (const auto& bot : bots) {
            if (bot.isAlive()) {
                bot.renderShadow(skinnedMesh.get(), weaponMesh.get());
            }
        }
    }

    void AIManager::render() const {
        for (const auto& bot : bots) {
            if (bot.isAlive()) {
                bot.render(skinnedMesh.get(), weaponMesh.get());
            }
        }
    }

} // namespace Lab
