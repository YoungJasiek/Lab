#pragma once
#include <vector>
#include <string>
#include <cmath>
#include <algorithm>
#include "LabMath.h"

namespace Lab {

    // Procedural Spring-damper physics for realistic weapon recoil & sway
    class SpringDamper {
    public:
        Vec3 position{0.0f, 0.0f, 0.0f};
        Vec3 velocity{0.0f, 0.0f, 0.0f};
        Vec3 target{0.0f, 0.0f, 0.0f};
        float stiffness = 160.0f;
        float damping = 18.0f;

        void update(float dt) {
            float remaining = std::clamp(dt, 0.0f, 2.0f);
            constexpr float maxStep = 0.008f;
            while (remaining > 0.0f) {
                float step = std::min(remaining, maxStep);
                Vec3 force = (target - position) * stiffness;
                force = force - (velocity * damping);
                velocity = velocity + (force * step);
                position = position + (velocity * step);
                remaining -= step;
            }
        }

        void addImpulse(const Vec3& impulse) {
            velocity = velocity + impulse;
        }
    };

    enum class WeaponAnimState {
        Idle,
        Fire,
        Reload,
        Inspect,
        MeleeSwing
    };

    // Procedural FPS Viewmodel Controller (Half-Life 2 style weapon sway, bobbing, recoil,
    // state-driven multi-phase reload, cinematic inspect, and melee swing kinematics)
    class WeaponAnimator {
    public:
        SpringDamper recoilSpring;
        SpringDamper swaySpring;
        float bobTimer = 0.0f;
        float currentSpeed = 0.0f;

        WeaponAnimState state = WeaponAnimState::Idle;
        float stateTimer = 0.0f;
        float stateDuration = 0.0f;

        WeaponAnimator();

        void onFire(bool isMelee = false);
        void onReload(float duration = 1.8f);
        void onInspect(float duration = 2.4f);
        void cancelInspect();

        bool isReloading() const { return state == WeaponAnimState::Reload; }
        bool isInspecting() const { return state == WeaponAnimState::Inspect; }
        bool isMeleeSwinging() const { return state == WeaponAnimState::MeleeSwing; }
        float getStateProgress() const { return stateDuration > 0.0f ? std::clamp(stateTimer / stateDuration, 0.0f, 1.0f) : 0.0f; }

        void update(float dt, const Vec2& mouseDelta, float moveSpeed);

        Vec3 calculatePositionOffset(const Vec3& defaultOffset) const;
        Vec3 calculateRotationOffset(const Vec3& defaultRot) const;

        // Kinematic offset for support (left) hand during reload cycle
        Vec3 getLeftHandReloadOffset() const;
        Vec3 getLeftHandReloadRotation() const;
        bool isReloadMagazineVisible() const;
    };

    // glTF 2.0 / Blender Animation Keyframe structures
    struct AnimKeyframe {
        float time = 0.0f;
        Vec3 translation{0.0f, 0.0f, 0.0f};
        Vec3 rotationEuler{0.0f, 0.0f, 0.0f};
        Vec3 scale{1.0f, 1.0f, 1.0f};
    };

    struct AnimationTrack {
        std::string nodeName;
        std::vector<AnimKeyframe> keyframes;

        AnimKeyframe sample(float t) const {
            if (keyframes.empty()) return AnimKeyframe{};
            if (keyframes.size() == 1 || t <= keyframes.front().time) return keyframes.front();
            if (t >= keyframes.back().time) return keyframes.back();

            for (size_t i = 0; i + 1 < keyframes.size(); ++i) {
                if (t >= keyframes[i].time && t <= keyframes[i+1].time) {
                    float factor = (t - keyframes[i].time) / (keyframes[i+1].time - keyframes[i].time);
                    AnimKeyframe k;
                    k.time = t;
                    k.translation = keyframes[i].translation * (1.0f - factor) + keyframes[i+1].translation * factor;
                    k.rotationEuler = keyframes[i].rotationEuler * (1.0f - factor) + keyframes[i+1].rotationEuler * factor;
                    k.scale = keyframes[i].scale * (1.0f - factor) + keyframes[i+1].scale * factor;
                    return k;
                }
            }
            return keyframes.back();
        }
    };

    class SkeletalAnimation {
    public:
        std::string name;
        float duration = 1.0f;
        std::vector<AnimationTrack> tracks;

        // Native glTF / GLB / Blender JSON animation loader
        static bool loadGLTFAnimation(const std::string& filepath, SkeletalAnimation& outAnim);
    };

    // AI Bot Animated Entity
    class AnimatedBot {
    public:
        std::string name;
        Vec3 position{0.0f, 0.0f, 0.0f};
        Vec3 rotation{0.0f, 0.0f, 0.0f};
        float walkCycle = 0.0f;
        float speed = 2.0f;
        int patrolDirection = 1;
        float patrolMinX = -8.0f;
        float patrolMaxX = 8.0f;

        void update(float dt) {
            position.x += patrolDirection * speed * dt;
            if (position.x > patrolMaxX) {
                position.x = patrolMaxX;
                patrolDirection = -1;
                rotation.y = 180.0f;
            } else if (position.x < patrolMinX) {
                position.x = patrolMinX;
                patrolDirection = 1;
                rotation.y = 0.0f;
            }
            walkCycle += dt * 6.0f;
        }
    };

}
