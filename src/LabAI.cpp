#include "LabAI.h"
#include "LabRenderer.h"
#include "LabCollision.h"
#include "LabPickups.h"
#include "LabChat.h"
#include "LabAudio.h"
#include <cmath>
#include <algorithm>
#include <fstream>
#include <sstream>
#include <filesystem>

namespace Lab {

    struct BotWeaponCombatStats {
        float interval;
        int clipSize;
        float reloadTime;
        SoundID sound;
        float damage;
        float range;
        Vec3 tracerColor;
        float tracerThickness;
        float tracerLifetime;
    };

    static BotWeaponCombatStats getBotWeaponStats(WeaponID wep) {
        switch (wep) {
            case WeaponID::Pipe:
                return { 0.45f, 1, 0.0f, SoundID::PipeSwing, 35.0f, 2.5f, Vec3(1.0f, 1.0f, 1.0f), 0.01f, 0.04f };
            case WeaponID::Pistol:
                return { 0.22f, 12, 1.8f, SoundID::PistolShot, 22.0f, 45.0f, Vec3(1.0f, 0.95f, 0.45f), 0.025f, 0.08f };
            case WeaponID::Shotgun:
                return { 0.75f, 8, 2.4f, SoundID::ShotgunShot, 14.0f, 30.0f, Vec3(1.0f, 0.7f, 0.2f), 0.035f, 0.09f };
            case WeaponID::M4A4S:
                return { 0.11f, 30, 2.2f, SoundID::M4A4SShot, 26.0f, 65.0f, Vec3(0.9f, 0.85f, 0.35f), 0.025f, 0.07f };
            case WeaponID::SG553:
                return { 0.13f, 30, 2.5f, SoundID::SG553Shot, 30.0f, 75.0f, Vec3(1.0f, 0.6f, 0.2f), 0.030f, 0.08f };
            case WeaponID::Minigun:
                return { 0.065f, 100, 3.5f, SoundID::MinigunShot, 16.0f, 55.0f, Vec3(1.0f, 0.85f, 0.2f), 0.035f, 0.06f };
            case WeaponID::PlasmaGun:
                return { 0.16f, 25, 2.0f, SoundID::PlasmaShot, 32.0f, 50.0f, Vec3(0.2f, 0.85f, 1.0f), 0.055f, 0.12f };
            case WeaponID::Railgun:
                return { 1.10f, 5, 2.8f, SoundID::RailgunShot, 85.0f, 120.0f, Vec3(0.3f, 0.9f, 1.0f), 0.065f, 0.18f };
            case WeaponID::RPG:
                return { 1.40f, 1, 2.6f, SoundID::RPGLaunch, 95.0f, 80.0f, Vec3(1.0f, 0.45f, 0.15f), 0.080f, 0.22f };
            default:
                return { 0.20f, 30, 2.0f, SoundID::PistolShot, 20.0f, 50.0f, Vec3(1.0f, 0.9f, 0.4f), 0.03f, 0.08f };
        }
    }

    CombatBot::CombatBot(int botId, const std::string& botName, const Vec3& spawnPos, const Vec3& pEnd, int botTeam, WeaponID weapon)
        : id(botId), name(botName), position(spawnPos), team(botTeam), equippedWeapon(weapon),
          patrolStart(spawnPos), patrolEnd(pEnd) {
        auto stats = getBotWeaponStats(equippedWeapon);
        shootInterval = stats.interval;
        shootCooldown = shootInterval;
        maxClipAmmo = stats.clipSize;
        ammoInClip = maxClipAmmo;
        reloadTimer = 0.0f;
        shootAnimTimer = 0.0f;
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
                if (patrolStart.y >= 1.5f) {
                    patrolStart.y = std::max(0.0f, patrolStart.y - 1.8f);
                }
                position = patrolStart;
                rotation.x = 0.0f;
                patrolT = 0.0f;
                patrolDir = 1;
                auto stats = getBotWeaponStats(equippedWeapon);
                shootInterval = stats.interval;
                shootCooldown = shootInterval;
                maxClipAmmo = stats.clipSize;
                ammoInClip = maxClipAmmo;
                reloadTimer = 0.0f;
                shootAnimTimer = 0.0f;
                hurtTimer = 0.0f;
                muzzleFlashTimer = 0.0f;
            }
            return;
        }

        if (hurtTimer > 0.0f) hurtTimer -= dt;
        if (muzzleFlashTimer > 0.0f) muzzleFlashTimer -= dt;
        if (shootAnimTimer > 0.0f) shootAnimTimer -= dt;

        if (reloadTimer > 0.0f) {
            reloadTimer -= dt;
            if (reloadTimer <= 0.0f) {
                ammoInClip = maxClipAmmo;
            }
        }

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

            // Combat weapons firing with weapon cadence, reload, and human recoil arc
            shootCooldown -= dt;
            if (shootCooldown <= 0.0f && reloadTimer <= 0.0f) {
                auto stats = getBotWeaponStats(equippedWeapon);
                if (ammoInClip <= 0 && stats.reloadTime > 0.0f) {
                    reloadTimer = stats.reloadTime;
                    AudioEngine::playSound3D(SoundID::Reload, position + Vec3(0.0f, 1.2f, 0.0f), 0.75f);
                } else {
                    if (ammoInClip > 0) ammoInClip--;
                    shootCooldown = stats.interval;
                    muzzleFlashTimer = 0.08f;
                    shootAnimTimer = 0.28f; // Human-like shooting recoil arc

                    Vec3 gunMuzzle = position + Vec3(0.2f, 1.15f, 0.3f);
                    AudioEngine::playSound3D(stats.sound, gunMuzzle, 0.85f);
                    bool hit = (rand() % 100) < 68;
                    Vec3 tracerEnd = playerTargetPos;
                    if (!hit) {
                        float missOffset = ((rand() % 100) / 50.0f - 1.0f) * 1.2f;
                        tracerEnd = tracerEnd + Vec3(missOffset, missOffset * 0.5f, -missOffset);
                    } else {
                        outDamageToPlayer += stats.damage;
                    }

                    BulletTracer tr;
                    tr.start = gunMuzzle;
                    tr.end = tracerEnd;
                    tr.color = stats.tracerColor;
                    tr.lifetime = 0.0f;
                    tr.maxLifetime = stats.tracerLifetime;
                    tr.thickness = stats.tracerThickness;
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

        // Update glTF 2.0 Skeletal Animator (Idle, Walk, Shoot recoil, Reload)
        if (animator.getSkeleton()) {
            if (reloadTimer > 0.0f) {
                animator.playAnimation("Reload", false);
            } else if (shootAnimTimer > 0.0f) {
                animator.playAnimation("Shoot", false);
            } else if (state == AIState::Chase || state == AIState::Patrol || (state == AIState::Attack && (distToPlayer > 8.0f || distToPlayer < 3.5f))) {
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

    void CombatBot::render(const SkinnedMesh* mesh, const Mesh* weaponMesh,
                           const BotWeaponConfig* botWepCfg, const Texture* weaponTex) const {
        if (state == AIState::Dead) {
            // Render defeated bot on floor
            Renderer::drawCube(position + Vec3(0.0f, 0.15f, 0.0f), Vec3(80.0f, rotation.y, 0.0f),
                               Vec3(0.55f, 0.25f, 1.35f), Vec3(0.18f, 0.18f, 0.20f));
            return;
        }

        float bodyBob = std::abs(std::sin(walkCycle * 2.0f)) * 0.04f;
        Vec3 bPos = position + Vec3(0.0f, bodyBob, 0.0f);
        if (bPos.y >= 1.5f && bPos.y <= 1.95f) {
            bPos.y = std::max(0.0f, bPos.y - 1.8f);
        }

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
            float botScale = mesh->getBaseScale(1.85f);
            float yOffset = -mesh->getMinBounds().y * botScale;
            Mat4 botModel = Mat4::translate(bPos + Vec3(0.0f, yOffset, 0.0f)) *
                            Mat4::rotate(rotation.y * 3.14159265f / 180.0f, Vec3(0, 1, 0)) *
                            Mat4::scale(Vec3(botScale, botScale, botScale));
            Renderer::drawSkinnedMesh(*mesh, botModel, animator.getSkinMatrices(), armorCol, nullptr, true);

            if (weaponMesh) {
                float invBotScale = (botScale > 0.00001f) ? (1.0f / botScale) : 1.0f;
                Vec3 offset(0.0f, -0.05f, 0.02f);
                Vec3 rot(5.73f, -11.46f, 0.0f);
                Vec3 scale(1.0f, 1.0f, 1.0f);
                if (botWepCfg) {
                    offset = botWepCfg->offset;
                    rot = botWepCfg->rotation;
                    scale = botWepCfg->scale;
                }
                Vec3 socketPos = offset * invBotScale;
                Quat socketRot = Quat::fromEuler(rot.x * 3.14159265f / 180.0f,
                                                 rot.y * 3.14159265f / 180.0f,
                                                 rot.z * 3.14159265f / 180.0f);
                Vec3 weaponScale = Vec3(0.016f * scale.x * invBotScale,
                                        0.016f * scale.y * invBotScale,
                                        0.016f * scale.z * invBotScale);
                Mat4 weaponSocket = animator.getSocketTransform("Socket_Weapon", botModel,
                    makeTransform(socketPos, socketRot, weaponScale));
                Renderer::drawMesh(*weaponMesh, weaponSocket, Vec3(0.92f, 0.92f, 0.95f), weaponTex, true);
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

            // 5. Bot Held Weapon
            if (weaponMesh) {
                Vec3 gunPos = bPos + Vec3(0.24f, 1.15f, 0.25f);
                Vec3 offset(0.0f, 0.0f, 0.0f);
                Vec3 rot(0.0f, 0.0f, 0.0f);
                Vec3 scale(1.0f, 1.0f, 1.0f);
                if (botWepCfg) {
                    offset = botWepCfg->offset;
                    rot = botWepCfg->rotation;
                    scale = botWepCfg->scale;
                }
                Renderer::drawMesh(*weaponMesh, gunPos + offset, rotation + rot, Vec3(0.45f * scale.x, 0.45f * scale.y, 0.45f * scale.z), Vec3(0.92f, 0.92f, 0.95f), weaponTex, true);
            } else {
                Vec3 gunPos = bPos + Vec3(0.24f, 1.15f, 0.25f);
                Renderer::drawCube(gunPos, rotation, Vec3(0.08f, 0.12f, 0.55f), Vec3(0.09f, 0.09f, 0.11f));
            }
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

    void CombatBot::renderShadow(const SkinnedMesh* mesh, const Mesh* weaponMesh,
                                 const BotWeaponConfig* botWepCfg) const {
        if (!isAlive()) return;
        float bodyBob = std::abs(std::sin(walkCycle * 2.0f)) * 0.04f;
        Vec3 bPos = position + Vec3(0.0f, bodyBob, 0.0f);
        if (bPos.y >= 1.5f && bPos.y <= 1.95f) {
            bPos.y = std::max(0.0f, bPos.y - 1.8f);
        }

        if (mesh && animator.getSkeleton()) {
            float botScale = mesh->getBaseScale(1.85f);
            float yOffset = -mesh->getMinBounds().y * botScale;
            Mat4 botModel = Mat4::translate(bPos + Vec3(0.0f, yOffset, 0.0f)) *
                            Mat4::rotate(rotation.y * 3.14159265f / 180.0f, Vec3(0, 1, 0)) *
                            Mat4::scale(Vec3(botScale, botScale, botScale));
            Renderer::drawShadowSkinnedMesh(*mesh, botModel, animator.getSkinMatrices());
            if (weaponMesh) {
                float invBotScale = (botScale > 0.00001f) ? (1.0f / botScale) : 1.0f;
                Vec3 offset(0.0f, -0.05f, 0.02f);
                Vec3 rot(5.73f, -11.46f, 0.0f);
                Vec3 scale(1.0f, 1.0f, 1.0f);
                if (botWepCfg) {
                    offset = botWepCfg->offset;
                    rot = botWepCfg->rotation;
                    scale = botWepCfg->scale;
                }
                Vec3 socketPos = offset * invBotScale;
                Quat socketRot = Quat::fromEuler(rot.x * 3.14159265f / 180.0f,
                                                 rot.y * 3.14159265f / 180.0f,
                                                 rot.z * 3.14159265f / 180.0f);
                Vec3 weaponScale = Vec3(0.016f * scale.x * invBotScale,
                                        0.016f * scale.y * invBotScale,
                                        0.016f * scale.z * invBotScale);
                Mat4 weaponSocket = animator.getSocketTransform("Socket_Weapon", botModel,
                    makeTransform(socketPos, socketRot, weaponScale));
                Renderer::drawShadowMesh(*weaponMesh, weaponSocket);
            }
        } else {
            Renderer::drawShadowCube(bPos + Vec3(0.0f, 1.0f, 0.0f), Vec3(0.6f, 1.8f, 0.6f));
        }
    }

    namespace {
        void addBoxToMesh(std::vector<Vertex>& verts, std::vector<unsigned int>& idxs,
                          const Vec3& center, const Vec3& size, const Vec3& color = Vec3(1, 1, 1)) {
            Vec3 half = size * 0.5f;
            static const Vec3 normals[6] = {
                {  0,  0,  1 }, {  0,  0, -1 },
                { -1,  0,  0 }, {  1,  0,  0 },
                {  0,  1,  0 }, {  0, -1,  0 }
            };
            static const float faceVerts[6][4][3] = {
                // Front (+Z)
                { { -1, -1,  1 }, {  1, -1,  1 }, {  1,  1,  1 }, { -1,  1,  1 } },
                // Back (-Z)
                { {  1, -1, -1 }, { -1, -1, -1 }, { -1,  1, -1 }, {  1,  1, -1 } },
                // Left (-X)
                { { -1, -1, -1 }, { -1, -1,  1 }, { -1,  1,  1 }, { -1,  1, -1 } },
                // Right (+X)
                { {  1, -1,  1 }, {  1, -1, -1 }, {  1,  1, -1 }, {  1,  1,  1 } },
                // Top (+Y)
                { { -1,  1,  1 }, {  1,  1,  1 }, {  1,  1, -1 }, { -1,  1, -1 } },
                // Bottom (-Y)
                { { -1, -1, -1 }, {  1, -1, -1 }, {  1, -1,  1 }, { -1, -1,  1 } }
            };
            static const Vec2 uvs[4] = { { 0, 0 }, { 1, 0 }, { 1, 1 }, { 0, 1 } };

            for (int f = 0; f < 6; ++f) {
                unsigned int startIdx = static_cast<unsigned int>(verts.size());
                for (int v = 0; v < 4; ++v) {
                    Vertex vert;
                    vert.position = {
                        center.x + faceVerts[f][v][0] * half.x,
                        center.y + faceVerts[f][v][1] * half.y,
                        center.z + faceVerts[f][v][2] * half.z
                    };
                    vert.normal = normals[f];
                    vert.texCoords = uvs[v];
                    vert.color = color;
                    verts.push_back(vert);
                }
                idxs.push_back(startIdx + 0);
                idxs.push_back(startIdx + 1);
                idxs.push_back(startIdx + 2);
                idxs.push_back(startIdx + 0);
                idxs.push_back(startIdx + 2);
                idxs.push_back(startIdx + 3);
            }
        }

        std::unique_ptr<Mesh> createProceduralWeaponMesh(WeaponID id) {
            std::vector<Vertex> verts;
            std::vector<unsigned int> idxs;

            switch (id) {
                case WeaponID::Pistol:
                    addBoxToMesh(verts, idxs, { 0.0f, 0.02f, -0.06f }, { 0.038f, 0.052f, 0.18f }, { 0.22f, 0.24f, 0.27f });
                    addBoxToMesh(verts, idxs, { 0.0f, -0.06f, 0.01f }, { 0.032f, 0.11f, 0.048f }, { 0.14f, 0.15f, 0.17f });
                    addBoxToMesh(verts, idxs, { 0.0f, -0.025f, -0.02f }, { 0.022f, 0.035f, 0.035f }, { 0.35f, 0.35f, 0.38f });
                    break;
                case WeaponID::Shotgun:
                    addBoxToMesh(verts, idxs, { 0.0f, 0.01f, 0.04f }, { 0.055f, 0.082f, 0.26f }, { 0.24f, 0.26f, 0.30f });
                    addBoxToMesh(verts, idxs, { 0.0f, 0.025f, -0.28f }, { 0.048f, 0.038f, 0.42f }, { 0.18f, 0.19f, 0.22f });
                    addBoxToMesh(verts, idxs, { 0.0f, -0.02f, -0.24f }, { 0.042f, 0.032f, 0.36f }, { 0.30f, 0.32f, 0.36f });
                    addBoxToMesh(verts, idxs, { 0.0f, -0.02f, -0.18f }, { 0.060f, 0.050f, 0.14f }, { 0.45f, 0.28f, 0.15f });
                    addBoxToMesh(verts, idxs, { 0.0f, -0.035f, 0.24f }, { 0.045f, 0.095f, 0.22f }, { 0.45f, 0.28f, 0.15f });
                    break;
                case WeaponID::SG553:
                    addBoxToMesh(verts, idxs, { 0.0f, 0.01f, 0.02f }, { 0.050f, 0.088f, 0.36f }, { 0.26f, 0.30f, 0.26f });
                    addBoxToMesh(verts, idxs, { 0.0f, 0.025f, -0.32f }, { 0.030f, 0.030f, 0.36f }, { 0.18f, 0.19f, 0.21f });
                    addBoxToMesh(verts, idxs, { 0.0f, 0.075f, -0.02f }, { 0.038f, 0.042f, 0.16f }, { 0.15f, 0.16f, 0.18f });
                    addBoxToMesh(verts, idxs, { 0.0f, -0.095f, -0.03f }, { 0.032f, 0.13f, 0.075f }, { 0.18f, 0.19f, 0.21f });
                    addBoxToMesh(verts, idxs, { 0.0f, -0.02f, 0.26f }, { 0.042f, 0.080f, 0.18f }, { 0.22f, 0.25f, 0.23f });
                    break;
                case WeaponID::Minigun:
                    addBoxToMesh(verts, idxs, { 0.0f, 0.0f, 0.08f }, { 0.14f, 0.15f, 0.30f }, { 0.22f, 0.23f, 0.26f });
                    addBoxToMesh(verts, idxs, { 0.0f, 0.0f, -0.28f }, { 0.12f, 0.12f, 0.48f }, { 0.32f, 0.34f, 0.38f });
                    addBoxToMesh(verts, idxs, { 0.0f, 0.11f, 0.02f }, { 0.038f, 0.075f, 0.26f }, { 0.15f, 0.16f, 0.18f });
                    addBoxToMesh(verts, idxs, { -0.075f, -0.05f, 0.08f }, { 0.075f, 0.11f, 0.16f }, { 0.40f, 0.35f, 0.18f });
                    break;
                case WeaponID::PlasmaGun:
                    addBoxToMesh(verts, idxs, { 0.0f, 0.01f, 0.02f }, { 0.085f, 0.11f, 0.40f }, { 0.20f, 0.25f, 0.32f });
                    addBoxToMesh(verts, idxs, { 0.0f, 0.025f, -0.10f }, { 0.105f, 0.075f, 0.16f }, { 0.20f, 0.85f, 1.00f });
                    addBoxToMesh(verts, idxs, { 0.0f, 0.015f, -0.32f }, { 0.075f, 0.055f, 0.26f }, { 0.30f, 0.35f, 0.42f });
                    addBoxToMesh(verts, idxs, { 0.0f, -0.075f, 0.10f }, { 0.065f, 0.095f, 0.11f }, { 0.15f, 0.18f, 0.22f });
                    break;
                case WeaponID::RPG:
                    addBoxToMesh(verts, idxs, { 0.0f, 0.03f, 0.02f }, { 0.078f, 0.078f, 0.72f }, { 0.24f, 0.28f, 0.20f });
                    addBoxToMesh(verts, idxs, { 0.0f, 0.03f, -0.38f }, { 0.125f, 0.125f, 0.20f }, { 0.28f, 0.34f, 0.22f });
                    addBoxToMesh(verts, idxs, { 0.0f, 0.03f, -0.50f }, { 0.038f, 0.038f, 0.07f }, { 0.82f, 0.82f, 0.86f });
                    addBoxToMesh(verts, idxs, { 0.0f, 0.03f, 0.40f }, { 0.110f, 0.110f, 0.08f }, { 0.18f, 0.18f, 0.20f });
                    addBoxToMesh(verts, idxs, { 0.0f, -0.07f, 0.02f }, { 0.038f, 0.12f, 0.055f }, { 0.15f, 0.15f, 0.17f });
                    break;
                default:
                    addBoxToMesh(verts, idxs, { 0.0f, 0.0f, 0.0f }, { 0.05f, 0.08f, 0.50f }, { 0.35f, 0.35f, 0.38f });
                    break;
            }

            return std::make_unique<Mesh>(verts, idxs);
        }
    }

    void AIManager::loadConfig(const std::string& path) {
        std::vector<std::string> candidates = {
            path,
            "assets/configs/character_studio.cfg",
            "build/Release/assets/configs/character_studio.cfg",
            "../assets/configs/character_studio.cfg",
            "../../assets/configs/character_studio.cfg"
        };
        std::string actualPath;
        for (const auto& c : candidates) {
            try {
                if (std::filesystem::exists(c)) {
                    actualPath = c;
                    break;
                }
            } catch (...) {}
        }
        if (actualPath.empty()) return;

        std::ifstream in(actualPath);
        if (!in.is_open()) return;

        std::string line;
        std::string currentSection;
        auto parseVec3 = [](const std::string& s, const Vec3& def) -> Vec3 {
            std::stringstream ss(s);
            std::string p;
            Vec3 v = def;
            if (std::getline(ss, p, ',')) v.x = std::stof(p);
            if (std::getline(ss, p, ',')) v.y = std::stof(p);
            if (std::getline(ss, p, ',')) v.z = std::stof(p);
            return v;
        };

        while (std::getline(in, line)) {
            if (line.empty() || line[0] == '#' || line[0] == ';') continue;
            if (line.front() == '[' && line.back() == ']') {
                currentSection = line.substr(1, line.size() - 2);
                continue;
            }
            size_t eqPos = line.find('=');
            if (eqPos == std::string::npos) continue;
            std::string key = line.substr(0, eqPos);
            std::string val = line.substr(eqPos + 1);

            if (currentSection.rfind("WeaponGrip_", 0) == 0) {
                int id = std::stoi(currentSection.substr(11));
                if (id >= 0 && id < 9) {
                    if (key == "BotOffset") botWeaponConfigs[id].offset = parseVec3(val, botWeaponConfigs[id].offset);
                    else if (key == "BotRotation") botWeaponConfigs[id].rotation = parseVec3(val, botWeaponConfigs[id].rotation);
                    else if (key == "BotScale") botWeaponConfigs[id].scale = parseVec3(val, botWeaponConfigs[id].scale);
                }
            } else if (currentSection == "BotWeapon") {
                for (int i = 0; i < 9; ++i) {
                    if (key == "Offset") botWeaponConfigs[i].offset = parseVec3(val, botWeaponConfigs[i].offset);
                    else if (key == "Rotation") botWeaponConfigs[i].rotation = parseVec3(val, botWeaponConfigs[i].rotation);
                    else if (key == "Scale") botWeaponConfigs[i].scale = parseVec3(val, botWeaponConfigs[i].scale);
                }
            }
        }
    }

    void AIManager::setBotWeaponConfig(WeaponID id, const BotWeaponConfig& cfg) {
        int idx = std::clamp(static_cast<int>(id), 0, 8);
        botWeaponConfigs[idx] = cfg;
    }

    const BotWeaponConfig& AIManager::getBotWeaponConfig(WeaponID id) const {
        int idx = std::clamp(static_cast<int>(id), 0, 8);
        return botWeaponConfigs[idx];
    }

    void AIManager::initAssets() {
        if (!skinnedMesh) {
            bool loaded = GLTFLoader::load("assets/models/t-800_run.glb", skeleton, animations, skinnedMesh);
            if (!loaded || !skinnedMesh) {
                loaded = GLTFLoader::load("assets/inwork/t-800_run.glb", skeleton, animations, skinnedMesh);
            }
            if (!loaded || !skinnedMesh) {
                GLTFLoader::createProceduralCombatBot(skeleton, animations, skinnedMesh);
            }

            // Load bot weapon configuration from studio profile
            loadConfig("assets/configs/character_studio.cfg");

            // Weapon 0: Pipe (STL)
            weaponMeshes[0] = std::unique_ptr<Mesh>(Mesh::loadSTL("assets/models/pipe.stl"));
            weaponTextures[0] = std::make_unique<Texture>("weapon_pipe.bmp");

            // Weapon 1: Pistol (Procedural 3D Mesh)
            weaponMeshes[1] = createProceduralWeaponMesh(WeaponID::Pistol);
            weaponTextures[1] = std::make_unique<Texture>("weapon_pistol.bmp");

            // Weapon 2: Shotgun (Procedural 3D Mesh)
            weaponMeshes[2] = createProceduralWeaponMesh(WeaponID::Shotgun);
            weaponTextures[2] = std::make_unique<Texture>("weapon_shotgun.bmp");

            // Weapon 3: M4A4-S Tactical Carbine (STL)
            Mesh* m4Mesh = Mesh::loadSTL("assets/models/Model.stl");
            if (!m4Mesh) m4Mesh = Mesh::loadSTL("assets/models/m4a4s.stl");
            weaponMeshes[3] = m4Mesh ? std::unique_ptr<Mesh>(m4Mesh) : createProceduralWeaponMesh(WeaponID::M4A4S);
            weaponTextures[3] = std::make_unique<Texture>("weapon_m4a4s.bmp");

            // Weapon 4: SG553 Scoped Rifle (Procedural 3D Mesh)
            weaponMeshes[4] = createProceduralWeaponMesh(WeaponID::SG553);
            weaponTextures[4] = std::make_unique<Texture>("weapon_sg553.bmp");

            // Weapon 5: Rotary Minigun (Procedural 3D Mesh)
            weaponMeshes[5] = createProceduralWeaponMesh(WeaponID::Minigun);
            weaponTextures[5] = std::make_unique<Texture>("weapon_minigun.bmp");

            // Weapon 6: Plasma Gun (Procedural 3D Mesh)
            weaponMeshes[6] = createProceduralWeaponMesh(WeaponID::PlasmaGun);
            weaponTextures[6] = std::make_unique<Texture>("weapon_plasma.bmp");

            // Weapon 7: Railgun (STL)
            Mesh* rgMesh = Mesh::loadSTL("assets/models/railgun.stl");
            weaponMeshes[7] = rgMesh ? std::unique_ptr<Mesh>(rgMesh) : createProceduralWeaponMesh(WeaponID::Railgun);
            weaponTextures[7] = std::make_unique<Texture>("weapon_railgun.bmp");

            // Weapon 8: RPG Rocket Launcher (Procedural 3D Mesh)
            weaponMeshes[8] = createProceduralWeaponMesh(WeaponID::RPG);
            weaponTextures[8] = std::make_unique<Texture>("weapon_rpg.bmp");

            // Backwards compatibility pointer
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

            // Distribute diverse arsenal across bots: Shotgun, M4A4S, SG553, Minigun, Plasma, Railgun, RPG, Pipe, Pistol
            WeaponID assignedWep = static_cast<WeaponID>((i + 2) % 9);
            bots.emplace_back(i, bName, botSpawnPos, patrolEnd, botTeam, assignedWep);
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

                    Vec3 botSpawnPos = sp.position;
                    if (botSpawnPos.y >= 1.5f) {
                        botSpawnPos.y = std::max(0.0f, botSpawnPos.y - 1.8f);
                    }

                    bot.position = botSpawnPos;
                    bot.rotation = Vec3(0.0f, sp.yaw, 0.0f);
                    bot.patrolStart = botSpawnPos;
                    float rad = sp.yaw * 3.14159265f / 180.0f;
                    Vec3 forward(std::sin(rad), 0.0f, std::cos(rad));
                    if (forward.lengthSq() < 0.01f) forward = Vec3(0, 0, 1);
                    bot.patrolEnd = botSpawnPos + forward * 8.0f;

                    bot.patrolT = 0.0f;
                    bot.patrolDir = 1;
                    auto stats = getBotWeaponStats(bot.equippedWeapon);
                    bot.shootInterval = stats.interval;
                    bot.shootCooldown = bot.shootInterval;
                    bot.maxClipAmmo = stats.clipSize;
                    bot.ammoInClip = bot.maxClipAmmo;
                    bot.reloadTimer = 0.0f;
                    bot.shootAnimTimer = 0.0f;
                    bot.hurtTimer = 0.0f;
                    bot.muzzleFlashTimer = 0.0f;
                }
                continue;
            }

            if (bot.hurtTimer > 0.0f) bot.hurtTimer -= dt;
            if (bot.muzzleFlashTimer > 0.0f) bot.muzzleFlashTimer -= dt;
            if (bot.shootAnimTimer > 0.0f) bot.shootAnimTimer -= dt;

            if (bot.reloadTimer > 0.0f) {
                bot.reloadTimer -= dt;
                if (bot.reloadTimer <= 0.0f) {
                    bot.ammoInClip = bot.maxClipAmmo;
                }
            }

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

                // Apply movement with wall sliding collision and gravity
                Vec3 nextPos = bot.position;
                Vec3 tempVel = moveVel;
                tempVel.y -= 12.0f * dt;
                bool grounded = true;
                LabCollision::moveAndSlide(nextPos, tempVel, grounded, dt, solidBoxes, 0.0f, 0.35f, 1.85f);
                bot.position = nextPos;
                bot.walkCycle += dt * 8.0f;

                // ==================== WEAPONS FIRING ====================
                bot.shootCooldown -= dt;
                if (bot.shootCooldown <= 0.0f && bot.reloadTimer <= 0.0f) {
                    auto stats = getBotWeaponStats(bot.equippedWeapon);
                    if (bot.ammoInClip <= 0 && stats.reloadTime > 0.0f) {
                        bot.reloadTimer = stats.reloadTime;
                        AudioEngine::playSound3D(SoundID::Reload, bot.position + Vec3(0.0f, 1.2f, 0.0f), 0.75f);
                    } else {
                        if (bot.ammoInClip > 0) bot.ammoInClip--;
                        bot.shootCooldown = stats.interval;
                        bot.muzzleFlashTimer = 0.08f;
                        bot.shootAnimTimer = 0.28f; // Human-like shooting recoil arc

                        Vec3 gunMuzzle = bot.position + Vec3(0.2f, 1.15f, 0.3f);
                        AudioEngine::playSound3D(stats.sound, gunMuzzle, 0.85f);
                        bool hit = (rand() % 100) < 68;

                        if (targetIsPlayer) {
                            Vec3 targetHitPos = playerPos + Vec3(0.0f, 0.8f, 0.0f);
                            if (!hit) {
                                float miss = ((rand() % 100) / 50.0f - 1.0f) * 1.2f;
                                targetHitPos = targetHitPos + Vec3(miss, miss * 0.5f, -miss);
                            } else {
                                outDamageToPlayer += stats.damage;
                            }

                            BulletTracer tr;
                            tr.start = gunMuzzle;
                            tr.end = targetHitPos;
                            tr.color = stats.tracerColor;
                            tr.lifetime = 0.0f;
                            tr.maxLifetime = stats.tracerLifetime;
                            tr.thickness = stats.tracerThickness;
                            outTracers.push_back(tr);
                        } else if (targetBotIdx >= 0 && targetBotIdx < (int)bots.size()) {
                            Vec3 targetHitPos = bots[targetBotIdx].position + Vec3(0.0f, 1.1f, 0.0f);
                            if (!hit) {
                                float miss = ((rand() % 100) / 50.0f - 1.0f) * 1.2f;
                                targetHitPos = targetHitPos + Vec3(miss, miss * 0.5f, -miss);
                            } else {
                                bool isHeadshot = (rand() % 100) < 25;
                                float dmg = isHeadshot ? (stats.damage * 2.2f) : stats.damage;
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
                                            int rWep = static_cast<int>(bots[targetBotIdx].equippedWeapon);
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
                            tr.maxLifetime = stats.tracerLifetime;
                            tr.thickness = stats.tracerThickness;
                            outTracers.push_back(tr);
                        }
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
                    Vec3 targetPos = bot.patrolStart * (1.0f - bot.patrolT) + bot.patrolEnd * bot.patrolT;
                    Vec3 dir = (bot.patrolDir > 0) ? (bot.patrolEnd - bot.patrolStart) : (bot.patrolStart - bot.patrolEnd);
                    bot.rotation.y = std::atan2(dir.x, dir.z) * 180.0f / 3.14159265f;

                    // Clamp to ground level using solid obstacles
                    Vec3 nextPos = targetPos;
                    Vec3 pVel = { 0.0f, -12.0f * dt, 0.0f };
                    bool grounded = true;
                    LabCollision::moveAndSlide(nextPos, pVel, grounded, dt, solidBoxes, 0.0f, 0.35f, 1.85f);
                    bot.position.x = targetPos.x;
                    bot.position.y = nextPos.y;
                    bot.position.z = targetPos.z;
                    bot.walkCycle += dt * 6.0f;
                }
            }

            // Update bot skeletal animator (Idle, Walk, Shoot recoil, Reload)
            if (bot.animator.getSkeleton()) {
                if (bot.reloadTimer > 0.0f) {
                    bot.animator.playAnimation("Reload", false);
                } else if (bot.shootAnimTimer > 0.0f) {
                    bot.animator.playAnimation("Shoot", false);
                } else if (bot.state == AIState::Chase || bot.state == AIState::Patrol || (bot.state == AIState::Attack && (bestDist > 8.0f || bestDist < 3.5f || std::abs(bot.strafeDirection) > 0))) {
                    bot.animator.playAnimation("Walk", true);
                } else {
                    bot.animator.playAnimation("Idle", true);
                }
                bot.animator.update(dt);
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
                int wIdx = std::clamp(static_cast<int>(bot.equippedWeapon), 0, 8);
                const Mesh* wMesh = weaponMeshes[wIdx] ? weaponMeshes[wIdx].get() : weaponMesh.get();
                const BotWeaponConfig* bCfg = &botWeaponConfigs[wIdx];
                bot.renderShadow(skinnedMesh.get(), wMesh, bCfg);
            }
        }
    }

    void AIManager::render() const {
        for (const auto& bot : bots) {
            if (bot.isAlive()) {
                int wIdx = std::clamp(static_cast<int>(bot.equippedWeapon), 0, 8);
                const Mesh* wMesh = weaponMeshes[wIdx] ? weaponMeshes[wIdx].get() : weaponMesh.get();
                const Texture* wTex = weaponTextures[wIdx].get();
                const BotWeaponConfig* bCfg = &botWeaponConfigs[wIdx];
                bot.render(skinnedMesh.get(), wMesh, bCfg, wTex);
            }
        }
    }

} // namespace Lab
