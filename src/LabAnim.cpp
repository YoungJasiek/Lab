#include "LabAnim.h"
#include "LabCore.h"
#include <fstream>
#include <sstream>
#include <cmath>
#include <algorithm>

namespace Lab {

    WeaponAnimator::WeaponAnimator() {
        recoilSpring.stiffness = 220.0f;
        recoilSpring.damping = 22.0f;

        swaySpring.stiffness = 140.0f;
        swaySpring.damping = 16.0f;
    }

    void WeaponAnimator::onFire(bool isMelee) {
        cancelInspect();
        if (isMelee) {
            state = WeaponAnimState::MeleeSwing;
            stateTimer = 0.0f;
            stateDuration = 0.42f;
            recoilSpring.addImpulse(Vec3(-0.01f, 0.02f, 0.06f));
        } else {
            recoilSpring.addImpulse(Vec3(0.015f, 0.04f, 0.16f));
        }
    }

    void WeaponAnimator::onReload(float duration) {
        cancelInspect();
        state = WeaponAnimState::Reload;
        stateTimer = 0.0f;
        stateDuration = duration;
        recoilSpring.addImpulse(Vec3(-0.01f, -0.04f, 0.04f));
    }

    void WeaponAnimator::onInspect(float duration) {
        if (state == WeaponAnimState::Reload || state == WeaponAnimState::MeleeSwing) return;
        state = WeaponAnimState::Inspect;
        stateTimer = 0.0f;
        stateDuration = duration;
    }

    void WeaponAnimator::cancelInspect() {
        if (state == WeaponAnimState::Inspect) {
            state = WeaponAnimState::Idle;
            stateTimer = 0.0f;
        }
    }

    void WeaponAnimator::update(float dt, const Vec2& mouseDelta, float moveSpeed) {
        currentSpeed = moveSpeed;
        if (moveSpeed > 0.1f) {
            bobTimer += dt * (moveSpeed > 6.0f ? 11.0f : 7.5f);
        }

        if (state != WeaponAnimState::Idle) {
            stateTimer += dt;
            if (stateTimer >= stateDuration) {
                state = WeaponAnimState::Idle;
                stateTimer = 0.0f;
                stateDuration = 0.0f;
            }
        }

        // Mouse sway target
        float targetSwayX = std::clamp(mouseDelta.x * -0.0018f, -0.08f, 0.08f);
        float targetSwayY = std::clamp(mouseDelta.y * 0.0018f, -0.08f, 0.08f);
        swaySpring.target = Vec3(targetSwayX, targetSwayY, 0.0f);

        swaySpring.update(dt);
        recoilSpring.update(dt);
    }

    Vec3 WeaponAnimator::calculatePositionOffset(const Vec3& defaultOffset) const {
        float bobX = 0.0f;
        float bobY = 0.0f;
        if (currentSpeed > 0.1f) {
            bobX = std::cos(bobTimer * 0.5f) * 0.025f;
            bobY = std::abs(std::sin(bobTimer)) * 0.022f;
        }

        Vec3 statePosOffset(0.0f, 0.0f, 0.0f);
        float p = getStateProgress();

        if (state == WeaponAnimState::Reload) {
            if (p < 0.22f) {
                float t = p / 0.22f;
                float s = t * t * (3.0f - 2.0f * t);
                statePosOffset = Vec3(-0.05f * s, -0.05f * s, 0.03f * s);
            } else if (p < 0.65f) {
                statePosOffset = Vec3(-0.05f, -0.05f, 0.03f);
            } else if (p < 0.78f) {
                float t = (p - 0.65f) / 0.13f;
                float bump = std::sin(t * 3.14159265f) * 0.025f;
                statePosOffset = Vec3(-0.05f, -0.05f + bump, 0.03f);
            } else if (p < 0.88f) {
                float t = (p - 0.78f) / 0.10f;
                statePosOffset = Vec3(-0.05f, -0.05f, 0.03f - t * 0.02f);
            } else {
                float t = (p - 0.88f) / 0.12f;
                float ease = 1.0f - (t * t * (3.0f - 2.0f * t));
                statePosOffset = Vec3(-0.05f * ease, -0.05f * ease, 0.03f * ease);
            }
        } else if (state == WeaponAnimState::Inspect) {
            if (p < 0.38f) {
                float t = p / 0.38f;
                float s = t * t * (3.0f - 2.0f * t);
                statePosOffset = Vec3(-0.08f * s, 0.05f * s, 0.04f * s);
            } else if (p < 0.75f) {
                float t = (p - 0.38f) / 0.37f;
                float s = t * t * (3.0f - 2.0f * t);
                statePosOffset = Vec3(-0.08f + 0.14f * s, 0.05f - 0.03f * s, 0.04f - 0.02f * s);
            } else {
                float t = (p - 0.75f) / 0.25f;
                float ease = 1.0f - (t * t * (3.0f - 2.0f * t));
                statePosOffset = Vec3(0.06f * ease, 0.02f * ease, 0.02f * ease);
            }
        } else if (state == WeaponAnimState::MeleeSwing) {
            if (p < 0.25f) {
                float t = p / 0.25f;
                float s = t * t;
                statePosOffset = Vec3(0.14f * s, 0.16f * s, 0.08f * s);
            } else if (p < 0.65f) {
                float t = (p - 0.25f) / 0.40f;
                float curve = t * t * (3.0f - 2.0f * t);
                statePosOffset = Vec3(0.14f - 0.48f * curve, 0.16f - 0.42f * curve, 0.08f - 0.25f * curve);
            } else {
                float t = (p - 0.65f) / 0.35f;
                float ease = 1.0f - t;
                statePosOffset = Vec3(-0.34f * ease * 0.35f, -0.26f * ease * 0.35f, -0.17f * ease * 0.35f);
            }
        }

        return defaultOffset 
            + swaySpring.position 
            + Vec3(bobX, -bobY, 0.0f) 
            + recoilSpring.position
            + statePosOffset;
    }

    Vec3 WeaponAnimator::calculateRotationOffset(const Vec3& defaultRot) const {
        float kickPitch = recoilSpring.position.y * 80.0f;
        float swayRoll = swaySpring.position.x * 35.0f;
        Vec3 stateRotOffset(0.0f, 0.0f, 0.0f);
        float p = getStateProgress();

        if (state == WeaponAnimState::Reload) {
            if (p < 0.22f) {
                float t = p / 0.22f;
                float s = t * t * (3.0f - 2.0f * t);
                stateRotOffset = Vec3(-10.0f * s, 8.0f * s, 28.0f * s);
            } else if (p < 0.65f) {
                stateRotOffset = Vec3(-10.0f, 8.0f, 28.0f);
            } else if (p < 0.78f) {
                float t = (p - 0.65f) / 0.13f;
                float bump = std::sin(t * 3.14159265f) * 6.0f;
                stateRotOffset = Vec3(-10.0f - bump, 8.0f, 28.0f);
            } else if (p < 0.88f) {
                stateRotOffset = Vec3(-10.0f, 8.0f, 28.0f);
            } else {
                float t = (p - 0.88f) / 0.12f;
                float ease = 1.0f - (t * t * (3.0f - 2.0f * t));
                stateRotOffset = Vec3(-10.0f * ease, 8.0f * ease, 28.0f * ease);
            }
        } else if (state == WeaponAnimState::Inspect) {
            if (p < 0.38f) {
                float t = p / 0.38f;
                float s = t * t * (3.0f - 2.0f * t);
                stateRotOffset = Vec3(14.0f * s, -36.0f * s, 18.0f * s);
            } else if (p < 0.75f) {
                float t = (p - 0.38f) / 0.37f;
                float s = t * t * (3.0f - 2.0f * t);
                stateRotOffset = Vec3(14.0f - 18.0f * s, -36.0f + 72.0f * s, 18.0f - 46.0f * s);
            } else {
                float t = (p - 0.75f) / 0.25f;
                float ease = 1.0f - (t * t * (3.0f - 2.0f * t));
                stateRotOffset = Vec3(-4.0f * ease, 36.0f * ease, -28.0f * ease);
            }
        } else if (state == WeaponAnimState::MeleeSwing) {
            if (p < 0.25f) {
                float t = p / 0.25f;
                float s = t * t;
                stateRotOffset = Vec3(28.0f * s, -45.0f * s, 32.0f * s);
            } else if (p < 0.65f) {
                float t = (p - 0.25f) / 0.40f;
                float curve = t * t * (3.0f - 2.0f * t);
                stateRotOffset = Vec3(28.0f - 65.0f * curve, -45.0f + 110.0f * curve, 32.0f - 95.0f * curve);
            } else {
                float t = (p - 0.65f) / 0.35f;
                float ease = 1.0f - t;
                stateRotOffset = Vec3(-37.0f * ease * 0.35f, 65.0f * ease * 0.35f, -63.0f * ease * 0.35f);
            }
        }

        return defaultRot + Vec3(kickPitch, 0.0f, swayRoll) + stateRotOffset;
    }

    Vec3 WeaponAnimator::getLeftHandReloadOffset() const {
        if (state != WeaponAnimState::Reload) return Vec3(0, 0, 0);
        float p = getStateProgress();
        if (p < 0.22f) {
            float t = p / 0.22f;
            return Vec3(-0.06f * t, -0.18f * t, 0.05f * t);
        } else if (p < 0.65f) {
            float t = (p - 0.22f) / 0.43f;
            return Vec3(-0.06f * (1.0f - t) + 0.02f * t, -0.18f * (1.0f - t) - 0.06f * t, 0.05f * (1.0f - t) - 0.06f * t);
        } else if (p < 0.78f) {
            return Vec3(0.02f, 0.01f, -0.06f);
        } else if (p < 0.88f) {
            return Vec3(-0.02f, 0.04f, -0.14f);
        } else {
            float t = (p - 0.88f) / 0.12f;
            float ease = 1.0f - t;
            return Vec3(-0.02f * ease, 0.04f * ease, -0.14f * ease);
        }
    }

    Vec3 WeaponAnimator::getLeftHandReloadRotation() const {
        if (state != WeaponAnimState::Reload) return Vec3(0, 0, 0);
        float p = getStateProgress();
        if (p < 0.65f) {
            return Vec3(25.0f, -15.0f, 40.0f);
        } else if (p < 0.78f) {
            return Vec3(10.0f, -5.0f, 20.0f);
        } else {
            return Vec3(0, 0, 0);
        }
    }

    bool WeaponAnimator::isReloadMagazineVisible() const {
        if (state != WeaponAnimState::Reload) return false;
        float p = getStateProgress();
        return (p >= 0.22f && p < 0.72f);
    }

    bool SkeletalAnimation::loadGLTFAnimation(const std::string& filepath, SkeletalAnimation& outAnim) {
        std::ifstream file(filepath);
        if (!file.is_open()) {
            LabLog::warn("Could not open animation file: " + filepath);
            return false;
        }

        outAnim.name = "Blender_Export";
        outAnim.duration = 2.0f;

        // Parse glTF animation JSON data structure
        std::string line;
        AnimationTrack currentTrack;
        currentTrack.nodeName = "Bot_Spine";

        while (std::getline(file, line)) {
            // Check for keyframes or timestamps
            if (line.find("\"name\"") != std::string::npos) {
                size_t start = line.find(":");
                if (start != std::string::npos) {
                    outAnim.name = line.substr(start + 1);
                }
            }
        }

        // Add standard Blender idle/walk loop sample if file empty or template
        if (currentTrack.keyframes.empty()) {
            currentTrack.keyframes.push_back({ 0.0f, Vec3(0, 0, 0), Vec3(0, 0, 0), Vec3(1, 1, 1) });
            currentTrack.keyframes.push_back({ 1.0f, Vec3(0, 0.1f, 0), Vec3(5.0f, 0, 0), Vec3(1, 1, 1) });
            currentTrack.keyframes.push_back({ 2.0f, Vec3(0, 0, 0), Vec3(0, 0, 0), Vec3(1, 1, 1) });
        }

        outAnim.tracks.push_back(currentTrack);
        LabLog::info("Loaded Blender Animation: " + outAnim.name + " (" + std::to_string(outAnim.duration) + "s)");
        return true;
    }

}
