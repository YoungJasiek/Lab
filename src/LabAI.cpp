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
        float refillTime;
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
        if (name.find("Guard") != std::string::npos || name.find("guard") != std::string::npos ||
            name.find("Barney") != std::string::npos || name.find("Security") != std::string::npos ||
            name.find("Ally") != std::string::npos || name.find("Friendly") != std::string::npos) {
            isFriendly = true;
            equippedWeapon = WeaponID::Pistol;
            moveSpeed = 2.4f;
        }
        auto stats = getBotWeaponStats(equippedWeapon);
        shootInterval = stats.interval;
        shootCooldown = shootInterval;
        maxClipAmmo = stats.clipSize;
        ammoInClip = maxClipAmmo;
        refillTimer = 0.0f;
        shootAnimTimer = 0.0f;
    }

    void CombatBot::assignWaypoints(const std::vector<Vec3>& pathNodes, bool loopPath) {
        waypoints = pathNodes;
        waypointData.clear();
        waypointWaitTimer = 0.0f;
        currentWaypointIndex = 0;
        loopWaypoints = loopPath;
        if (!waypoints.empty()) {
            patrolStart = waypoints.front();
            patrolEnd = waypoints.back();
        }
    }

    void CombatBot::assignWaypoints(const std::vector<MapWaypoint>& pathNodes, bool loopPath) {
        waypointData = pathNodes;
        waypoints.clear();
        waypoints.reserve(pathNodes.size());
        for (const auto& wp : pathNodes) {
            waypoints.push_back(wp.position);
        }
        waypointWaitTimer = 0.0f;
        currentWaypointIndex = 0;
        loopWaypoints = loopPath;
        if (!waypoints.empty()) {
            patrolStart = waypoints.front();
            patrolEnd = waypoints.back();
        }
    }

    void CombatBot::startStoryEvent(const std::string& subtitle, const std::string& audioFile, const std::vector<Vec3>& pathNodes) {
        std::vector<MapWaypoint> wps;
        wps.reserve(pathNodes.size());
        for (size_t i = 0; i < pathNodes.size(); ++i) {
            MapWaypoint wp;
            wp.position = pathNodes[i];
            wp.orderIndex = static_cast<int>(i);
            wps.push_back(wp);
        }
        startStoryEvent(subtitle, audioFile, wps);
    }

    void CombatBot::startStoryEvent(const std::string& subtitle, const std::string& audioFile, const std::vector<MapWaypoint>& pathNodes) {
        activeSubtitle = subtitle;
        activeAudioPath = audioFile.empty() ? "assets/audio/bot_corridor_dialog.wav" : audioFile;
        state = AIState::Talk;

        bool loaded = lipSync.loadDialogAudio(activeAudioPath, name, activeSubtitle);
        if (!loaded) {
            lipSync.clearSubtitles();
            lipSync.addSubtitle(0.0f, 3.0f, name, activeSubtitle);
        }
        lipSync.playDialog(false);
        talkTimer = (lipSync.getDialogDuration() > 0.1f) ? lipSync.getDialogDuration() : 3.0f;

        // Play 3D positional bot voice audio
        AudioEngine::playCustomSound3D(activeAudioPath, position + Vec3(0.0f, 1.65f, 0.0f), 1.0f, 2.0f, 38.0f);

        if (!pathNodes.empty()) {
            assignWaypoints(pathNodes, false);
            storyPathActive = true;
        } else if (!waypoints.empty()) {
            currentWaypointIndex = 0;
            storyPathActive = true;
        }

        if (animator.getSkeleton()) {
            animator.playAnimation("Talk", true, 0.1f);
        }
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
            if (!b.collidable || b.type == "trigger") continue;
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
                refillTimer = 0.0f;
                shootAnimTimer = 0.0f;
                hurtTimer = 0.0f;
                muzzleFlashTimer = 0.0f;
            }
            return;
        }

        if (hurtTimer > 0.0f) hurtTimer -= dt;
        if (muzzleFlashTimer > 0.0f) muzzleFlashTimer -= dt;
        if (shootAnimTimer > 0.0f) shootAnimTimer -= dt;

        if (refillTimer > 0.0f) {
            refillTimer -= dt;
            if (refillTimer <= 0.0f) {
                ammoInClip = maxClipAmmo;
            }
        }

        // Update Lip-Sync & blendshapes/visemes when talking
        if (talkTimer > 0.0f) {
            talkTimer -= dt;
            lipSync.updateDialog(dt);
            if (!lipSync.hasDialogAudio()) {
                float synthAmp = 0.5f + 0.5f * std::sin((3.0f - talkTimer) * 18.0f);
                lipSync.update(synthAmp, dt);
            }
            if (faceMesh && lodLevel < 2) {
                lipSync.applyTo(*faceMesh);
                faceMesh->evaluate();
            }
            animator.setJawAngle(lipSync.getJawOpen() * 0.38f);
            if (talkTimer <= 0.0f) {
                talkTimer = 0.0f;
                activeSubtitle.clear();
                animator.setJawAngle(0.0f);
                if (state == AIState::Talk) {
                    state = AIState::Patrol;
                }
            }
        } else {
            animator.setJawAngle(0.0f);
        }

        Vec3 vecToPlayer = playerPos - position;
        float distToPlayer = vecToPlayer.length();

        // Level of Detail (LOD) computation based on camera distance
        if (distToPlayer < 12.0f) {
            lodLevel = 0; // High detail (<12m): 60/120 FPS full skeletal updates, full lip-sync
        } else if (distToPlayer < 28.0f) {
            lodLevel = 1; // Medium detail (12-28m): 2-frame throttled updates
        } else {
            lodLevel = 2; // Low detail (>28m): 4-frame throttled updates, shadow/accessory optimization
        }

        Vec3 eyePos = position + Vec3(0.0f, 1.6f, 0.0f);
        Vec3 playerTargetPos = playerPos + Vec3(0.0f, 0.8f, 0.0f);
        bool canSee = (!isFriendly && state != AIState::Talk && !storyPathActive) &&
                      (distToPlayer <= sightRange) && hasLineOfSight(eyePos, playerTargetPos, map);
        bool movedThisStep = false;

        if (isFriendly && waypoints.empty() && !storyPathActive && distToPlayer <= 7.0f && hasLineOfSight(eyePos, playerTargetPos, map)) {
            // Half-Life Guard: turns calmly to face the player, does not attack
            float targetYaw = std::atan2(vecToPlayer.x, vecToPlayer.z) * 180.0f / 3.14159265f;
            rotation.y = targetYaw;
            if (state != AIState::Talk && !storyPathActive) {
                state = AIState::Patrol;
            }
        } else if (canSee) {
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
            movedThisStep = true;

            // Combat weapons firing with weapon cadence, reload, and human recoil arc
            shootCooldown -= dt;
            if (shootCooldown <= 0.0f && refillTimer <= 0.0f) {
                auto stats = getBotWeaponStats(equippedWeapon);
                if (ammoInClip <= 0 && stats.refillTime > 0.0f) {
                    refillTimer = stats.refillTime;
                    AudioEngine::playSound3D(SoundID::Refill, position + Vec3(0.0f, 1.2f, 0.0f), 0.75f);
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
        } else if (!waypoints.empty() && (storyPathActive || state == AIState::Patrol || state == AIState::Talk)) {
            if (talkTimer <= 0.0f) state = AIState::Patrol;
            int wpIdx = std::clamp(currentWaypointIndex, 0, static_cast<int>(waypoints.size()) - 1);
            Vec3 targetNode = waypoints[wpIdx];
            Vec3 toNode = Vec3(targetNode.x - position.x, 0.0f, targetNode.z - position.z);
            float distNode = toNode.length();
            if (distNode > 0.25f) {
                Vec3 dir = toNode / distNode;
                float step = std::min(distNode, moveSpeed * dt);
                position.x += dir.x * step;
                position.z += dir.z * step;
                rotation.y = std::atan2(dir.x, dir.z) * 180.0f / 3.14159265f;
                walkCycle += dt * 6.0f;
                movedThisStep = true;
            } else {
                if (wpIdx < static_cast<int>(waypointData.size())) {
                    const auto& wData = waypointData[wpIdx];
                    if (waypointWaitTimer > 0.0f) {
                        waypointWaitTimer -= dt;
                        rotation.y = wData.facingYaw;
                        if (animator.getSkeleton() && !wData.animation.empty() && animator.getCurrentAnimationName() != wData.animation) {
                            animator.playAnimation(wData.animation, true, 0.2f);
                        }
                        if (waypointWaitTimer <= 0.0f) {
                            if (currentWaypointIndex + 1 < static_cast<int>(waypoints.size())) {
                                currentWaypointIndex++;
                            } else if (loopWaypoints) {
                                currentWaypointIndex = 0;
                            } else {
                                storyPathActive = false;
                            }
                        }
                    } else {
                        // Just arrived at waypoint: fire linked trigger if set
                        if (!wData.targetTrigger.empty()) {
                            auto& mutableMap = const_cast<LabMap&>(map);
                            for (auto& tr : mutableMap.triggers) {
                                if (tr.name == wData.targetTrigger) {
                                    AIManager::executeTrigger(tr, mutableMap, nullptr);
                                }
                            }
                        }

                        if (wData.waitDuration > 0.05f) {
                            waypointWaitTimer = wData.waitDuration;
                            rotation.y = wData.facingYaw;
                            if (animator.getSkeleton() && !wData.animation.empty()) {
                                animator.playAnimation(wData.animation, true, 0.2f);
                            }
                        } else {
                            rotation.y = wData.facingYaw;
                            if (animator.getSkeleton() && !wData.animation.empty()) {
                                animator.playAnimation(wData.animation, true, 0.2f);
                            }
                            if (currentWaypointIndex + 1 < static_cast<int>(waypoints.size())) {
                                currentWaypointIndex++;
                            } else if (loopWaypoints) {
                                currentWaypointIndex = 0;
                            } else {
                                storyPathActive = false;
                            }
                        }
                    }
                } else {
                    if (currentWaypointIndex + 1 < static_cast<int>(waypoints.size())) {
                        currentWaypointIndex++;
                    } else if (loopWaypoints) {
                        currentWaypointIndex = 0;
                    } else {
                        storyPathActive = false;
                    }
                }
            }
        } else {
            // Return to / continue patrol
            if (talkTimer <= 0.0f) state = AIState::Patrol;
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
                movedThisStep = true;

                Vec3 dir = (patrolDir > 0) ? (patrolEnd - patrolStart) : (patrolStart - patrolEnd);
                rotation.y = std::atan2(dir.x, dir.z) * 180.0f / 3.14159265f;
            }
        }

        // Emit 3D spatial footstep audio while moving
        if (movedThisStep) {
            footstepTimer -= dt;
            if (footstepTimer <= 0.0f) {
                footstepTimer = (state == AIState::Chase) ? 0.36f : 0.48f;
                AudioEngine::playCustomSound3D("assets/audio/bot_footstep_3d.wav", position, 0.65f, 1.5f, 28.0f);
                AudioEngine::playSound3D(SoundID::FootstepMetal, position, 0.50f, 1.0f, 1.5f, 28.0f);
            }
        }

        // Update glTF 2.0 / FBX Skeletal Animator (Death, Hit, Reload, Shoot recoil, Talk, Walk, Idle)
        if (animator.getSkeleton()) {
            if (state == AIState::Dead) {
                animator.playAnimation(deathAnim, false);
            } else if (hurtTimer > 0.0f) {
                animator.playAnimation("Hit Reaction", false, 0.05f);
            } else if (refillTimer > 0.0f) {
                animator.playAnimation("Reload", false, 0.1f);
            } else if (shootAnimTimer > 0.0f) {
                if (equippedWeapon == WeaponID::Pipe) {
                    animator.playAnimation("Heavy Weapon Swing", false, 0.05f);
                } else {
                    animator.playAnimation(animator.hasClip("Firing Rifle") ? "Firing Rifle" : "Attack", false, 0.05f);
                }
            } else if (storyPathActive && waypointWaitTimer > 0.0f && !waypointData.empty()) {
                int curWp = std::clamp(currentWaypointIndex, 0, static_cast<int>(waypointData.size()) - 1);
                if (curWp >= 0 && curWp < static_cast<int>(waypointData.size()) && !waypointData[curWp].animation.empty()) {
                    animator.playAnimation(waypointData[curWp].animation, true, 0.2f);
                } else {
                    animator.playAnimation("Idle", true, 0.15f);
                }
            } else if (state == AIState::Talk && !movedThisStep) {
                animator.playAnimation("Talk", true, 0.1f);
            } else if (state == AIState::Chase) {
                animator.playAnimation("Run Forward", true, 0.1f);
            } else if (state == AIState::Attack) {
                if (distToPlayer < 3.5f) {
                    animator.playAnimation("Sprint Backward", true, 0.1f);
                } else if (std::abs(strafeDirection) > 0) {
                    if (equippedWeapon == WeaponID::Pistol) {
                        animator.playAnimation("Pistol Strafe", true, 0.1f);
                    } else {
                        animator.playAnimation("Strafing", true, 0.1f);
                    }
                } else if (distToPlayer > 8.0f) {
                    animator.playAnimation("Walk", true, 0.1f);
                } else {
                    if (equippedWeapon == WeaponID::Pistol && animator.hasClip("Pistol Idle")) {
                        animator.playAnimation("Pistol Idle", true, 0.15f);
                    } else {
                        animator.playAnimation("Idle", true, 0.15f);
                    }
                }
            } else if ((state == AIState::Patrol || state == AIState::Talk) && movedThisStep) {
                animator.playAnimation("Walk", true, 0.1f);
            } else {
                if (equippedWeapon == WeaponID::Pistol && animator.hasClip("Pistol Idle")) {
                    animator.playAnimation("Pistol Idle", true, 0.15f);
                } else {
                    animator.playAnimation("Idle", true, 0.15f);
                }
            }
            lodFrameSkip++;
            int skipThreshold = (lodLevel == 0) ? 1 : ((lodLevel == 1) ? 2 : 4);
            if (lodFrameSkip >= skipThreshold) {
                animator.update(dt * static_cast<float>(skipThreshold));
                lodFrameSkip = 0;
            }
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
            deathTimer = 0.0f;
            rotation.x = 0.0f;

            // Context-sensitive death animation based on hit location & knockback angle
            float rad = rotation.y * 3.14159265f / 180.0f;
            Vec3 botForward(std::sin(rad), 0.0f, std::cos(rad));
            Vec3 kbFlat(knockback.x, 0.0f, knockback.z);
            float dot = (kbFlat.lengthSq() > 0.01f) ? Vec3::dot(botForward, kbFlat.normalized()) : 0.0f;

            if (isHeadshot) {
                deathAnim = "Death From Front Headshot";
            } else if (dot > 0.2f) {
                deathAnim = "Death From The Back";
            } else {
                deathAnim = "Rifle Death";
            }

            if (animator.getSkeleton()) {
                animator.playAnimation(deathAnim, false, 0.05f);
            }

            if (velocity.lengthSq() < 0.1f) {
                position.y = 0.25f;
            }
            return true; // Just died
        }

        state = AIState::Chase; // Immediately alert bot on hit
        return false;
    }

    void CombatBot::render(const SkinnedMesh* mesh, const Mesh* weaponMesh,
                           const BotWeaponConfig* botWepCfg, const Texture* weaponTex,
                           const Texture* botTex) const {
        if (state == AIState::Dead) {
            if (mesh && animator.getSkeleton()) {
                float botScale = mesh->getBaseScale(1.85f);
                float yOffset = -mesh->getMinBounds().y * botScale;
                Mat4 botModel = Mat4::translate(position + Vec3(0.0f, yOffset, 0.0f)) *
                                Mat4::rotate(rotation.y * 3.14159265f / 180.0f, Vec3(0, 1, 0)) *
                                Mat4::scale(Vec3(botScale, botScale, botScale));
                Vec3 corpseCol = Vec3(0.18f, 0.18f, 0.20f);
                Renderer::drawSkinnedMesh(*mesh, botModel, animator.getSkinMatrices(), corpseCol, botTex, true);
                return;
            } else {
                // Fallback cube
                Renderer::drawCube(position + Vec3(0.0f, 0.15f, 0.0f), Vec3(80.0f, rotation.y, 0.0f),
                                   Vec3(0.55f, 0.25f, 1.35f), Vec3(0.18f, 0.18f, 0.20f));
                return;
            }
        }

        float bodyBob = std::abs(std::sin(walkCycle * 2.0f)) * 0.04f;
        Vec3 bPos = position + Vec3(0.0f, bodyBob, 0.0f);
        if (bPos.y >= 1.5f && bPos.y <= 1.95f) {
            bPos.y = std::max(0.0f, bPos.y - 1.8f);
        }

        // Appearance and color tint based on archetype & team
        Vec3 armorCol = Vec3(1.0f, 1.0f, 1.0f);
        if (archetype == BotArchetype::Guard) {
            armorCol = Vec3(1.0f, 1.0f, 1.0f); // Full diffuse texture brilliance
        } else if (archetype == BotArchetype::Scientist) {
            armorCol = Vec3(0.92f, 0.94f, 0.98f); // Clean white laboratory coat tint
        } else if (archetype == BotArchetype::Worker) {
            armorCol = Vec3(0.95f, 0.65f, 0.18f); // Industrial hazard jumpsuit tint
        } else if (archetype == BotArchetype::CombatSynth) {
            if (team == 0) {
                armorCol = Vec3(0.48f, 0.18f, 0.18f); // Red Team Synth
            } else if (team == 1) {
                armorCol = Vec3(0.18f, 0.32f, 0.52f); // Blue Team Synth
            } else {
                armorCol = Vec3(0.24f, 0.28f, 0.32f); // Charcoal FFA Synth
            }
        }

        if (hurtTimer > 0.0f) {
            armorCol = Vec3(0.95f, 0.25f, 0.25f); // Bright red hit flash
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
            Renderer::drawSkinnedMesh(*mesh, botModel, animator.getSkinMatrices(), armorCol, botTex, true);

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
            // 1. Cylindrical Armored Torso & Chest Plate
            Renderer::drawCylinder(bPos + Vec3(0.0f, 1.15f, 0.0f), rotation, Vec3(0.50f, 0.72f, 0.36f), armorCol);
            Renderer::drawCylinder(bPos + Vec3(0.0f, 1.22f, 0.04f), rotation, Vec3(0.44f, 0.46f, 0.32f), Vec3(0.14f, 0.16f, 0.19f));
            // 2. Cylindrical Combat Helmet & Glowing Visor
            Renderer::drawCylinder(bPos + Vec3(0.0f, 1.68f, 0.0f), rotation, Vec3(0.30f, 0.28f, 0.30f), Vec3(0.12f, 0.14f, 0.17f));
            Renderer::drawCube(bPos + Vec3(0.0f, 1.68f, 0.15f), rotation, Vec3(0.22f, 0.07f, 0.04f), visorCol, nullptr, false);
            // 3. Cylindrical Articulated Legs
            Renderer::drawCylinder(bPos + Vec3(-0.15f, 0.45f, legSwing), rotation, Vec3(0.14f, 0.80f, 0.15f), Vec3(0.15f, 0.16f, 0.18f));
            Renderer::drawCylinder(bPos + Vec3(0.15f, 0.45f, -legSwing), rotation, Vec3(0.14f, 0.80f, 0.15f), Vec3(0.15f, 0.16f, 0.18f));

            // 4. Bot Held Weapon
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
                Renderer::drawCube(gunPos, rotation, Vec3(0.07f, 0.11f, 0.52f), Vec3(0.12f, 0.13f, 0.15f));
            }
        }

        if (muzzleFlashTimer > 0.0f) {
            float yawRad = rotation.y * 3.14159265f / 180.0f;
            Vec3 botFwd(std::sin(yawRad), 0.0f, std::cos(yawRad));
            Vec3 botRight(botFwd.z, 0.0f, -botFwd.x);
            Vec3 mPos = bPos + Vec3(0.0f, 1.18f, 0.0f) + botRight * 0.22f + botFwd * 0.58f;
            Renderer::drawMuzzleFlash(mPos, botFwd, 0.24f, Vec3(1.0f, 0.85f, 0.25f));
            Renderer::addPointLight(mPos, Vec3(1.0f, 0.78f, 0.22f), 2.2f, 5.5f);
        }
    }

    void CombatBot::renderShadow(const SkinnedMesh* mesh, const Mesh* weaponMesh,
                                 const BotWeaponConfig* botWepCfg) const {
        if (state == AIState::Dead) {
            if (deathTimer > 4.0f) return;
            if (mesh && animator.getSkeleton()) {
                float botScale = mesh->getBaseScale(1.85f);
                float yOffset = -mesh->getMinBounds().y * botScale;
                Mat4 botModel = Mat4::translate(position + Vec3(0.0f, yOffset, 0.0f)) *
                                Mat4::rotate(rotation.y * 3.14159265f / 180.0f, Vec3(0, 1, 0)) *
                                Mat4::scale(Vec3(botScale, botScale, botScale));
                Renderer::drawShadowSkinnedMesh(*mesh, botModel, animator.getSkinMatrices());
            }
            return;
        }
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
            // At LOD 2 (> 28m), skip small weapon shadow pass to optimize shadow mapping performance
            if (weaponMesh && lodLevel < 2) {
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
            Renderer::drawShadowCylinder(bPos + Vec3(0.0f, 1.0f, 0.0f), Vec3(0.0f, 0.0f, 0.0f), Vec3(0.6f, 1.8f, 0.6f));
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
                { { -1, -1,  1 }, {  1, -1,  1 }, {  1,  1,  1 }, { -1,  1,  1 } },
                { {  1, -1, -1 }, { -1, -1, -1 }, { -1,  1, -1 }, {  1,  1, -1 } },
                { { -1, -1, -1 }, { -1, -1,  1 }, { -1,  1,  1 }, { -1,  1, -1 } },
                { {  1, -1,  1 }, {  1, -1, -1 }, {  1,  1, -1 }, {  1,  1,  1 } },
                { { -1,  1,  1 }, {  1,  1,  1 }, {  1,  1, -1 }, { -1,  1, -1 } },
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

        void addCylinderZToMesh(std::vector<Vertex>& verts, std::vector<unsigned int>& idxs,
                                const Vec3& center, const Vec3& size, const Vec3& color = Vec3(1, 1, 1)) {
            const int segs = 16;
            const float pi2 = 6.283185307f;
            Vec3 half = size * 0.5f;
            unsigned int baseIdx = static_cast<unsigned int>(verts.size());

            for (int i = 0; i <= segs; ++i) {
                float u = (float)i / (float)segs;
                float a = u * pi2;
                float cx = std::cos(a);
                float sy = std::sin(a);
                Vec3 n(cx, sy, 0.0f);

                verts.push_back({ { center.x + cx * half.x, center.y + sy * half.y, center.z - half.z }, n, { u, 0.0f }, color });
                verts.push_back({ { center.x + cx * half.x, center.y + sy * half.y, center.z + half.z }, n, { u, 1.0f }, color });
            }
            for (int i = 0; i < segs; ++i) {
                unsigned int b0 = baseIdx + i * 2;
                unsigned int t0 = b0 + 1;
                unsigned int b1 = b0 + 2;
                unsigned int t1 = b0 + 3;
                idxs.push_back(b0); idxs.push_back(b1); idxs.push_back(t1);
                idxs.push_back(b0); idxs.push_back(t1); idxs.push_back(t0);
            }
        }

        std::unique_ptr<Mesh> createProceduralWeaponMesh(WeaponID id) {
            std::vector<Vertex> verts;
            std::vector<unsigned int> idxs;

            switch (id) {
                case WeaponID::Pistol:
                    addBoxToMesh(verts, idxs, { 0.0f, 0.02f, -0.06f }, { 0.036f, 0.048f, 0.18f }, { 0.45f, 0.48f, 0.52f });
                    addCylinderZToMesh(verts, idxs, { 0.0f, 0.024f, -0.08f }, { 0.024f, 0.024f, 0.20f }, { 0.22f, 0.24f, 0.27f });
                    addBoxToMesh(verts, idxs, { 0.0f, -0.06f, 0.01f }, { 0.032f, 0.11f, 0.048f }, { 0.14f, 0.15f, 0.17f });
                    break;
                case WeaponID::Shotgun:
                    addBoxToMesh(verts, idxs, { 0.0f, 0.01f, 0.04f }, { 0.055f, 0.082f, 0.26f }, { 0.24f, 0.26f, 0.30f });
                    addCylinderZToMesh(verts, idxs, { 0.0f, 0.028f, -0.28f }, { 0.042f, 0.042f, 0.44f }, { 0.18f, 0.19f, 0.22f });
                    addCylinderZToMesh(verts, idxs, { 0.0f, -0.018f, -0.24f }, { 0.038f, 0.038f, 0.36f }, { 0.30f, 0.32f, 0.36f });
                    addCylinderZToMesh(verts, idxs, { 0.0f, -0.018f, -0.18f }, { 0.058f, 0.052f, 0.16f }, { 0.16f, 0.17f, 0.19f });
                    addBoxToMesh(verts, idxs, { 0.0f, -0.035f, 0.24f }, { 0.045f, 0.095f, 0.22f }, { 0.18f, 0.18f, 0.20f });
                    break;
                case WeaponID::SG553:
                    addBoxToMesh(verts, idxs, { 0.0f, 0.01f, 0.02f }, { 0.050f, 0.088f, 0.36f }, { 0.26f, 0.30f, 0.26f });
                    addCylinderZToMesh(verts, idxs, { 0.0f, 0.025f, -0.32f }, { 0.032f, 0.032f, 0.38f }, { 0.18f, 0.19f, 0.21f });
                    addCylinderZToMesh(verts, idxs, { 0.0f, 0.075f, -0.02f }, { 0.040f, 0.040f, 0.18f }, { 0.15f, 0.16f, 0.18f });
                    addBoxToMesh(verts, idxs, { 0.0f, -0.095f, -0.03f }, { 0.032f, 0.13f, 0.075f }, { 0.18f, 0.19f, 0.21f });
                    addBoxToMesh(verts, idxs, { 0.0f, -0.02f, 0.26f }, { 0.042f, 0.080f, 0.18f }, { 0.22f, 0.25f, 0.23f });
                    break;
                case WeaponID::Minigun:
                    addCylinderZToMesh(verts, idxs, { 0.0f, 0.0f, 0.08f }, { 0.14f, 0.14f, 0.30f }, { 0.22f, 0.23f, 0.26f });
                    addCylinderZToMesh(verts, idxs, { 0.0f, 0.0f, -0.28f }, { 0.11f, 0.11f, 0.48f }, { 0.32f, 0.34f, 0.38f });
                    addBoxToMesh(verts, idxs, { 0.0f, 0.11f, 0.02f }, { 0.038f, 0.075f, 0.26f }, { 0.15f, 0.16f, 0.18f });
                    addBoxToMesh(verts, idxs, { -0.075f, -0.05f, 0.08f }, { 0.075f, 0.11f, 0.16f }, { 0.40f, 0.35f, 0.18f });
                    break;
                case WeaponID::PlasmaGun:
                    addBoxToMesh(verts, idxs, { 0.0f, 0.01f, 0.02f }, { 0.085f, 0.11f, 0.40f }, { 0.20f, 0.25f, 0.32f });
                    addCylinderZToMesh(verts, idxs, { 0.0f, 0.025f, -0.10f }, { 0.095f, 0.095f, 0.18f }, { 0.20f, 0.85f, 1.00f });
                    addCylinderZToMesh(verts, idxs, { 0.0f, 0.015f, -0.32f }, { 0.068f, 0.068f, 0.26f }, { 0.30f, 0.35f, 0.42f });
                    addBoxToMesh(verts, idxs, { 0.0f, -0.075f, 0.10f }, { 0.065f, 0.095f, 0.11f }, { 0.15f, 0.18f, 0.22f });
                    break;
                case WeaponID::RPG:
                    addCylinderZToMesh(verts, idxs, { 0.0f, 0.03f, 0.02f }, { 0.082f, 0.082f, 0.72f }, { 0.24f, 0.28f, 0.20f });
                    addCylinderZToMesh(verts, idxs, { 0.0f, 0.03f, -0.38f }, { 0.128f, 0.128f, 0.20f }, { 0.28f, 0.34f, 0.22f });
                    addCylinderZToMesh(verts, idxs, { 0.0f, 0.03f, -0.50f }, { 0.038f, 0.038f, 0.07f }, { 0.82f, 0.82f, 0.86f });
                    addCylinderZToMesh(verts, idxs, { 0.0f, 0.03f, 0.40f }, { 0.112f, 0.112f, 0.08f }, { 0.18f, 0.18f, 0.20f });
                    addBoxToMesh(verts, idxs, { 0.0f, -0.07f, 0.02f }, { 0.038f, 0.12f, 0.055f }, { 0.15f, 0.15f, 0.17f });
                    break;
                default:
                    addCylinderZToMesh(verts, idxs, { 0.0f, 0.0f, 0.0f }, { 0.055f, 0.055f, 0.50f }, { 0.35f, 0.35f, 0.38f });
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
            bool loaded = GLTFLoader::load("assets/models/guard.glb", skeleton, animations, skinnedMesh);
            if (!loaded || !skinnedMesh) {
                loaded = GLTFLoader::load("assets/models/t-800_run.glb", skeleton, animations, skinnedMesh);
            }
            if (!loaded || !skinnedMesh) {
                loaded = GLTFLoader::load("assets/inwork/t-800_run.glb", skeleton, animations, skinnedMesh);
            }
            if (!loaded || !skinnedMesh) {
                GLTFLoader::createProceduralCombatBot(skeleton, animations, skinnedMesh);
            }

            // Ingest all FBX / binary animations into the AI bot skeleton
            FBXLoader::loadAllFromDirectory("assets/animations", animations, skeleton.get());

            // Ensure canonical animation clip aliases (Walk, Shoot, Idle) exist
            bool hasShoot = false, hasWalk = false, hasIdle = false;
            const AnimationClip* attackSource = nullptr;
            const AnimationClip* walkSource = nullptr;
            const AnimationClip* idleSource = nullptr;
            for (const auto& c : animations) {
                if (c.name == "Shoot") hasShoot = true;
                if (c.name == "Walk") hasWalk = true;
                if (c.name == "Idle") hasIdle = true;
                if (c.name == "Attack" || c.name == "Firing Rifle") attackSource = &c;
                if (c.name == "Walking" || c.name == "Run Forward") walkSource = &c;
                if (c.name == "Pistol Idle") idleSource = &c;
            }
            if (!hasShoot && attackSource) {
                AnimationClip sc = *attackSource;
                sc.name = "Shoot";
                animations.push_back(sc);
            }
            if (!hasWalk && walkSource) {
                AnimationClip wc = *walkSource;
                wc.name = "Walk";
                animations.push_back(wc);
            }
            if (!hasIdle && idleSource) {
                AnimationClip ic = *idleSource;
                ic.name = "Idle";
                animations.push_back(ic);
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

            // Character Textures
            guardTexture = std::make_unique<Texture>("guard.bmp");
            if (!guardTexture || guardTexture->getId() == 0) {
                guardTexture = std::make_unique<Texture>("guard_body.bmp");
            }
            synthTexture = std::make_unique<Texture>("metal_hull.bmp");
        }
        if (!sharedFaceMesh) {
            sharedFaceMesh = FacialMesh::createProceduralHead();
        }
    }

    const Texture* AIManager::getBotTexture(BotArchetype type) const {
        if (type == BotArchetype::Guard || type == BotArchetype::Scientist || type == BotArchetype::Worker) {
            if (guardTexture && guardTexture->getId() != 0) {
                return guardTexture.get();
            }
        }
        if (synthTexture && synthTexture->getId() != 0) {
            return synthTexture.get();
        }
        return guardTexture ? guardTexture.get() : nullptr;
    }

    void AIManager::spawnBotsForMap(const LabMap* map, int count, GameMode mode) {
        clear();
        if (count <= 0) return;
        initAssets();

        for (int i = 0; i < count; ++i) {
            int botTeam = (mode == GameMode::TDM) ? (i % 2) : -1;
            std::string bName = (i == 0 && map && !map->waypoints.empty() && !map->waypoints[0].botName.empty())
                                ? map->waypoints[0].botName
                                : ("Combat Synth #" + std::to_string(i + 1));
            if (count == 1 || (map && (!map->triggers.empty() || !map->waypoints.empty()) && i == 0)) {
                bName = "Guard";
            }

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

            // Determine Archetype
            BotArchetype botArchetype = BotArchetype::CombatSynth;
            bool isGuard = (bName.find("Guard") != std::string::npos || bName.find("guard") != std::string::npos ||
                            bName.find("Security") != std::string::npos || bName.find("Barney") != std::string::npos ||
                            mode == GameMode::Campaign);
            bool isScientist = (bName.find("Scientist") != std::string::npos || bName.find("scientist") != std::string::npos ||
                                bName.find("Kleiner") != std::string::npos || bName.find("Doctor") != std::string::npos);
            bool isWorker = (bName.find("Worker") != std::string::npos || bName.find("worker") != std::string::npos ||
                             bName.find("Engineer") != std::string::npos || bName.find("Tech") != std::string::npos);

            WeaponID assignedWep = static_cast<WeaponID>((i + 2) % 9);
            float botSpeed = 3.2f;
            bool friendly = false;

            if (isGuard) {
                botArchetype = BotArchetype::Guard;
                botTeam = 1; // Friendly team
                assignedWep = WeaponID::Pistol; // Half-Life Security Guard pistol
                botSpeed = 2.4f; // Calm Half-Life patrol pace
                friendly = true;
                if (bName.find("Synth") != std::string::npos || bName.find("Bot #") != std::string::npos) {
                    bName = "Guard";
                }
            } else if (isScientist) {
                botArchetype = BotArchetype::Scientist;
                botTeam = 1;
                assignedWep = WeaponID::Pistol;
                botSpeed = 2.0f;
                friendly = true;
            } else if (isWorker) {
                botArchetype = BotArchetype::Worker;
                botTeam = 1;
                assignedWep = WeaponID::Pipe;
                botSpeed = 2.2f;
                friendly = true;
            } else {
                botArchetype = BotArchetype::CombatSynth;
                if (bName.rfind("Bot #", 0) == 0) {
                    bName = "Combat Synth #" + std::to_string(i + 1);
                }
            }

            bots.emplace_back(i, bName, botSpawnPos, patrolEnd, botTeam, assignedWep);
            bots.back().archetype = botArchetype;
            bots.back().isFriendly = friendly;
            bots.back().moveSpeed = botSpeed;
            bots.back().rotation.y = sp.yaw;
            bots.back().faceMesh = sharedFaceMesh;
            if (skeleton) {
                bots.back().initAnimation(skeleton, animations);
            }

            // Bind map waypoints assigned to this bot (or matching archetype / designation)
            if (map && !map->waypoints.empty()) {
                std::vector<MapWaypoint> matched;
                for (const auto& wp : map->waypoints) {
                    bool match = false;
                    if (wp.botName.empty() || wp.botName == "(any bot)" || wp.botName == "Any") {
                        if (i == 0) match = true;
                    } else if (wp.botName == bName) {
                        match = true;
                    } else if (botArchetype == BotArchetype::Guard && (wp.botName == "Guard" || wp.botName == "Bot_Guard" || wp.botName == "Security")) {
                        match = true;
                    } else if (botArchetype == BotArchetype::Scientist && (wp.botName == "Scientist" || wp.botName == "Doctor" || wp.botName == "Researcher")) {
                        match = true;
                    } else if (botArchetype == BotArchetype::Worker && (wp.botName == "Worker" || wp.botName == "Engineer" || wp.botName == "Tech")) {
                        match = true;
                    } else if (botArchetype == BotArchetype::CombatSynth) {
                        if (wp.botName == "CombatSynth" || wp.botName == "Synth" || wp.botName == "Bot") match = true;
                        if ((wp.botName == "Synth #1" || wp.botName == "Combat Synth #1") && i == 0) match = true;
                        if ((wp.botName == "Synth #2" || wp.botName == "Combat Synth #2") && i == 1) match = true;
                    }
                    if (match) {
                        matched.push_back(wp);
                    }
                }
                std::sort(matched.begin(), matched.end(), [](const MapWaypoint& a, const MapWaypoint& b) {
                    return a.orderIndex < b.orderIndex;
                });
                if (!matched.empty()) {
                    bots.back().assignWaypoints(matched, true);
                }
            }
        }
    }

    void AIManager::spawnBotsForMap(const std::string& mapName, int count, GameMode mode) {
        auto loaded = LabMap::loadFromFile(mapName);
        spawnBotsForMap(loaded.get(), count, mode);
    }

    bool AIManager::executeTrigger(MapTrigger& tr, LabMap& map, LabChat* chat) {
        if (tr.once && tr.triggered) return false;
        tr.triggered = true;

        // 1. Target entity manipulation (e.g. Doors, mechanisms)
        if (!tr.targetEntity.empty()) {
            for (auto& d : map.doors) {
                if (d.name == tr.targetEntity) {
                    if (tr.targetAction == "unlock") {
                        d.isLocked = false;
                    } else if (tr.targetAction == "lock") {
                        d.isLocked = true;
                        d.isOpen = false;
                    } else if (tr.targetAction == "close") {
                        d.isOpen = false;
                    } else if (tr.targetAction == "toggle") {
                        d.isOpen = !d.isOpen;
                        if (d.isOpen) d.isLocked = false;
                    } else { // "open" default
                        d.isLocked = false;
                        d.isOpen = true;
                    }
                }
            }
        }

        // 2. Chat / Subtitles notification
        if (!tr.dialogText.empty() && chat) {
            chat->addMessage("[" + (tr.targetBot.empty() ? "System" : tr.targetBot) + "]", tr.dialogText, Vec3(0.25f, 0.90f, 0.45f));
        }

        return true;
    }

    bool AIManager::triggerStoryEventOnBot(const std::string& targetBotName, const std::string& subtitle,
                                           const std::string& audioFile, const LabMap& map) {
        if (bots.empty()) {
            spawnBotsForMap(&map, 1, GameMode::FFA);
        }
        if (bots.empty()) return false;

        int targetIdx = 0;
        for (size_t i = 0; i < bots.size(); ++i) {
            if (!targetBotName.empty() && (bots[i].name == targetBotName ||
                (bots[i].archetype == BotArchetype::Guard && targetBotName == "Guard") ||
                (bots[i].archetype == BotArchetype::Scientist && targetBotName == "Scientist") ||
                (bots[i].archetype == BotArchetype::Worker && targetBotName == "Worker"))) {
                targetIdx = static_cast<int>(i);
                break;
            }
        }

        std::vector<MapWaypoint> matched;
        for (const auto& wp : map.waypoints) {
            if (wp.botName.empty() || wp.botName == targetBotName || wp.botName == bots[targetIdx].name ||
                (bots[targetIdx].archetype == BotArchetype::Guard && (wp.botName == "Guard" || wp.botName == "Bot_Guard"))) {
                matched.push_back(wp);
            }
        }
        if (matched.empty() && !map.waypoints.empty()) {
            matched = map.waypoints;
        }
        std::sort(matched.begin(), matched.end(), [](const MapWaypoint& a, const MapWaypoint& b) {
            return a.orderIndex < b.orderIndex;
        });

        bots[targetIdx].startStoryEvent(subtitle, audioFile, matched);
        return true;
    }

    void AIManager::update(float dt, const Vec3& playerPos, bool isPlayerAlive, int playerTeam,
                           const LabMap& map, std::vector<BulletTracer>& outTracers, float& outDamageToPlayer,
                           PickupManager* pickupMgr, LabChat* chat) {
        auto solidBoxes = LabCollision::getMapSolidBoxes(map);

        for (size_t i = 0; i < bots.size(); ++i) {
            auto& bot = bots[i];
            float distToPlayer = (bot.position - playerPos).length();
            if (distToPlayer < 12.0f) {
                bot.lodLevel = 0;
            } else if (distToPlayer < 28.0f) {
                bot.lodLevel = 1;
            } else {
                bot.lodLevel = 2;
            }

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
                if (bot.animator.getSkeleton()) {
                    bot.animator.update(dt);
                }
                if (bot.respawnTimer <= 0.0f) {
                    bot.state = AIState::Patrol;
                    bot.health = bot.maxHealth;
                    bot.velocity = Vec3(0, 0, 0);
                    bot.rotation.x = 0.0f;
                    bot.deathTimer = 0.0f;
                    bot.deathAnim = "Death";
                    if (bot.animator.getSkeleton()) {
                        bot.animator.playAnimation("Walk", true, 0.1f);
                    }

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
                    bot.refillTimer = 0.0f;
                    bot.shootAnimTimer = 0.0f;
                    bot.hurtTimer = 0.0f;
                    bot.muzzleFlashTimer = 0.0f;
                }
                continue;
            }

            if (bot.hurtTimer > 0.0f) bot.hurtTimer -= dt;
            if (bot.muzzleFlashTimer > 0.0f) bot.muzzleFlashTimer -= dt;
            if (bot.shootAnimTimer > 0.0f) bot.shootAnimTimer -= dt;

            if (bot.refillTimer > 0.0f) {
                bot.refillTimer -= dt;
                if (bot.refillTimer <= 0.0f) {
                    bot.ammoInClip = bot.maxClipAmmo;
                }
            }

            // Update Lip-Sync & blendshapes/visemes when talking
            if (bot.talkTimer > 0.0f) {
                bot.talkTimer -= dt;
                bot.lipSync.updateDialog(dt);
                if (!bot.lipSync.hasDialogAudio()) {
                    float synthAmp = 0.5f + 0.5f * std::sin((3.0f - bot.talkTimer) * 18.0f);
                    bot.lipSync.update(synthAmp, dt);
                }
                if (bot.faceMesh) {
                    bot.lipSync.applyTo(*bot.faceMesh);
                    bot.faceMesh->evaluate();
                }
                bot.animator.setJawAngle(bot.lipSync.getJawOpen() * 0.38f);
                if (bot.talkTimer <= 0.0f) {
                    bot.talkTimer = 0.0f;
                    bot.activeSubtitle.clear();
                    bot.animator.setJawAngle(0.0f);
                    if (bot.state == AIState::Talk) {
                        bot.state = AIState::Patrol;
                    }
                }
            } else {
                bot.animator.setJawAngle(0.0f);
            }

            // ==================== MULTI-TARGET SELECTION ====================
            // Find closest visible enemy (either the player or another enemy bot)
            float bestDist = 1e9f;
            Vec3 bestTargetPos{ 0.0f, 0.0f, 0.0f };
            bool targetIsPlayer = false;
            int targetBotIdx = -1;

            Vec3 botEye = bot.position + Vec3(0.0f, 1.6f, 0.0f);

            if (bot.state != AIState::Talk && !bot.storyPathActive) {
                // 1. Evaluate Player
                if (!bot.isFriendly && isPlayerAlive && (playerTeam == -1 || playerTeam != bot.team)) {
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
            }

            bool movedThisStep = false;

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
                movedThisStep = true;

                // ==================== WEAPONS FIRING ====================
                bot.shootCooldown -= dt;
                if (bot.shootCooldown <= 0.0f && bot.refillTimer <= 0.0f) {
                    auto stats = getBotWeaponStats(bot.equippedWeapon);
                    if (bot.ammoInClip <= 0 && stats.refillTime > 0.0f) {
                        bot.refillTimer = stats.refillTime;
                        AudioEngine::playSound3D(SoundID::Refill, bot.position + Vec3(0.0f, 1.2f, 0.0f), 0.75f);
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
                                Vec3 shotDir = (targetHitPos - gunMuzzle).normalized();
                                Vec3 kb = shotDir * (stats.damage * 0.15f) + Vec3(0.0f, 1.0f, 0.0f);
                                bool killed = bots[targetBotIdx].takeDamage(dmg, isHeadshot, kb);

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
            } else if (!bot.waypoints.empty() && (bot.storyPathActive || bot.state == AIState::Patrol || bot.state == AIState::Talk)) {
                // ==================== STORY MODE / WAYPOINT PATH NAVIGATION ====================
                if (bot.talkTimer <= 0.0f) bot.state = AIState::Patrol;
                int wpIdx = std::clamp(bot.currentWaypointIndex, 0, static_cast<int>(bot.waypoints.size()) - 1);
                Vec3 targetNode = bot.waypoints[wpIdx];
                Vec3 toNode = Vec3(targetNode.x - bot.position.x, 0.0f, targetNode.z - bot.position.z);
                float distNode = toNode.length();
                if (distNode > 0.25f) {
                    Vec3 dir = toNode / distNode;
                    float step = std::min(distNode, bot.moveSpeed * dt);
                    Vec3 nextPos = Vec3(bot.position.x + dir.x * step, bot.position.y, bot.position.z + dir.z * step);
                    Vec3 pVel = { 0.0f, -12.0f * dt, 0.0f };
                    bool grounded = true;
                    LabCollision::moveAndSlide(nextPos, pVel, grounded, dt, solidBoxes, 0.0f, 0.35f, 1.85f);
                    bot.position.x = bot.position.x + dir.x * step;
                    bot.position.y = nextPos.y;
                    bot.position.z = bot.position.z + dir.z * step;
                    bot.rotation.y = std::atan2(dir.x, dir.z) * 180.0f / 3.14159265f;
                    bot.walkCycle += dt * 6.0f;
                    movedThisStep = true;
                } else {
                    if (wpIdx < static_cast<int>(bot.waypointData.size())) {
                        const auto& wData = bot.waypointData[wpIdx];
                        if (bot.waypointWaitTimer > 0.0f) {
                            bot.waypointWaitTimer -= dt;
                            bot.rotation.y = wData.facingYaw;
                            if (bot.animator.getSkeleton() && !wData.animation.empty() && bot.animator.getCurrentAnimationName() != wData.animation) {
                                bot.animator.playAnimation(wData.animation, true, 0.2f);
                            }
                            if (bot.waypointWaitTimer <= 0.0f) {
                                if (bot.currentWaypointIndex + 1 < static_cast<int>(bot.waypoints.size())) {
                                    bot.currentWaypointIndex++;
                                } else if (bot.loopWaypoints) {
                                    bot.currentWaypointIndex = 0;
                                } else {
                                    bot.storyPathActive = false;
                                }
                            }
                        } else {
                            if (!wData.targetTrigger.empty()) {
                                auto& mutableMap = const_cast<LabMap&>(map);
                                for (auto& tr : mutableMap.triggers) {
                                    if (tr.name == wData.targetTrigger) {
                                        executeTrigger(tr, mutableMap, chat);
                                    }
                                }
                            }

                            if (wData.waitDuration > 0.05f) {
                                bot.waypointWaitTimer = wData.waitDuration;
                                bot.rotation.y = wData.facingYaw;
                                if (bot.animator.getSkeleton() && !wData.animation.empty()) {
                                    bot.animator.playAnimation(wData.animation, true, 0.2f);
                                }
                            } else {
                                bot.rotation.y = wData.facingYaw;
                                if (bot.animator.getSkeleton() && !wData.animation.empty()) {
                                    bot.animator.playAnimation(wData.animation, true, 0.2f);
                                }
                                if (bot.currentWaypointIndex + 1 < static_cast<int>(bot.waypoints.size())) {
                                    bot.currentWaypointIndex++;
                                } else if (bot.loopWaypoints) {
                                    bot.currentWaypointIndex = 0;
                                } else {
                                    bot.storyPathActive = false;
                                }
                            }
                        }
                    } else {
                        if (bot.currentWaypointIndex + 1 < static_cast<int>(bot.waypoints.size())) {
                            bot.currentWaypointIndex++;
                        } else if (bot.loopWaypoints) {
                            bot.currentWaypointIndex = 0;
                        } else {
                            bot.storyPathActive = false;
                        }
                    }
                }
            } else {
                // ==================== PATROL NAVIGATION ====================
                if (bot.talkTimer <= 0.0f) bot.state = AIState::Patrol;
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
                    movedThisStep = true;
                }
            }

            // Emit 3D spatial footstep audio while moving
            if (movedThisStep) {
                bot.footstepTimer -= dt;
                if (bot.footstepTimer <= 0.0f) {
                    bot.footstepTimer = (bot.state == AIState::Chase) ? 0.36f : 0.48f;
                    AudioEngine::playCustomSound3D("assets/audio/bot_footstep_3d.wav", bot.position, 0.65f, 1.5f, 28.0f);
                    AudioEngine::playSound3D(SoundID::FootstepMetal, bot.position, 0.50f, 1.0f, 1.5f, 28.0f);
                }
            }

            // Update bot skeletal animator (Death, Hit, Reload, Shoot recoil, Talk, Walk, Idle)
            if (bot.animator.getSkeleton()) {
                if (bot.state == AIState::Dead) {
                    bot.animator.playAnimation(bot.deathAnim, false);
                } else if (bot.hurtTimer > 0.0f) {
                    bot.animator.playAnimation("Hit Reaction", false, 0.05f);
                } else if (bot.refillTimer > 0.0f) {
                    bot.animator.playAnimation("Reload", false, 0.1f);
                } else if (bot.shootAnimTimer > 0.0f) {
                    if (bot.equippedWeapon == WeaponID::Pipe) {
                        bot.animator.playAnimation("Heavy Weapon Swing", false, 0.05f);
                    } else {
                        bot.animator.playAnimation("Attack", false, 0.05f);
                    }
                } else if (bot.storyPathActive && bot.waypointWaitTimer > 0.0f && !bot.waypointData.empty()) {
                    int curWp = std::clamp(bot.currentWaypointIndex, 0, static_cast<int>(bot.waypointData.size()) - 1);
                    if (curWp >= 0 && curWp < static_cast<int>(bot.waypointData.size()) && !bot.waypointData[curWp].animation.empty()) {
                        bot.animator.playAnimation(bot.waypointData[curWp].animation, true, 0.2f);
                    } else {
                        bot.animator.playAnimation("Idle", true, 0.15f);
                    }
                } else if (bot.state == AIState::Talk && !movedThisStep) {
                    bot.animator.playAnimation("Talk", true, 0.1f);
                } else if (bot.state == AIState::Chase) {
                    bot.animator.playAnimation("Run Forward", true, 0.1f);
                } else if (bot.state == AIState::Attack) {
                    if (bestDist < 3.5f) {
                        bot.animator.playAnimation("Sprint Backward", true, 0.1f);
                    } else if (std::abs(bot.strafeDirection) > 0) {
                        if (bot.equippedWeapon == WeaponID::Pistol) {
                            bot.animator.playAnimation("Pistol Strafe", true, 0.1f);
                        } else {
                            bot.animator.playAnimation("Strafing", true, 0.1f);
                        }
                    } else if (bestDist > 8.0f) {
                        bot.animator.playAnimation("Walk", true, 0.1f);
                    } else {
                        if (bot.equippedWeapon == WeaponID::Pistol && bot.animator.hasClip("Pistol Idle")) {
                            bot.animator.playAnimation("Pistol Idle", true, 0.15f);
                        } else {
                            bot.animator.playAnimation("Idle", true, 0.15f);
                        }
                    }
                } else if ((bot.state == AIState::Patrol || bot.state == AIState::Talk) && movedThisStep) {
                    bot.animator.playAnimation("Walk", true, 0.1f);
                } else {
                    if (bot.equippedWeapon == WeaponID::Pistol && bot.animator.hasClip("Pistol Idle")) {
                        bot.animator.playAnimation("Pistol Idle", true, 0.15f);
                    } else {
                        bot.animator.playAnimation("Idle", true, 0.15f);
                    }
                }
                bot.lodFrameSkip++;
                int skipThreshold = (bot.lodLevel == 0) ? 1 : ((bot.lodLevel == 1) ? 2 : 4);
                if (bot.lodFrameSkip >= skipThreshold) {
                    bot.animator.update(dt * static_cast<float>(skipThreshold));
                    bot.lodFrameSkip = 0;
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
                const Texture* bTex = getBotTexture(bot.archetype);
                const BotWeaponConfig* bCfg = &botWeaponConfigs[wIdx];
                bot.render(skinnedMesh.get(), wMesh, bCfg, wTex, bTex);
            }
        }
    }

} // namespace Lab
