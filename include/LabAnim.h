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
            Vec3 force = (target - position) * stiffness;
            force = force - (velocity * damping);
            velocity = velocity + (force * dt);
            position = position + (velocity * dt);
        }

        void addImpulse(const Vec3& impulse) {
            velocity = velocity + impulse;
        }
    };

    // Procedural FPS Viewmodel Controller (Half-Life 2 style weapon sway, bobbing, recoil)
    class WeaponAnimator {
    public:
        SpringDamper recoilSpring;
        SpringDamper swaySpring;
        float bobTimer = 0.0f;
        float currentSpeed = 0.0f;

        WeaponAnimator() {
            recoilSpring.stiffness = 220.0f;
            recoilSpring.damping = 22.0f;

            swaySpring.stiffness = 140.0f;
            swaySpring.damping = 16.0f;
        }

        void onFire() {
            // Strong kick backwards and upwards
            recoilSpring.addImpulse(Vec3(0.015f, 0.04f, 0.16f));
        }

        void update(float dt, const Vec2& mouseDelta, float moveSpeed) {
            currentSpeed = moveSpeed;
            if (moveSpeed > 0.1f) {
                bobTimer += dt * (moveSpeed > 6.0f ? 11.0f : 7.5f);
            }

            // Mouse sway target
            float targetSwayX = std::clamp(mouseDelta.x * -0.0018f, -0.08f, 0.08f);
            float targetSwayY = std::clamp(mouseDelta.y * 0.0018f, -0.08f, 0.08f);
            swaySpring.target = Vec3(targetSwayX, targetSwayY, 0.0f);

            swaySpring.update(dt);
            recoilSpring.update(dt);
        }

        Vec3 calculatePositionOffset(const Vec3& defaultOffset) const {
            float bobX = 0.0f;
            float bobY = 0.0f;
            if (currentSpeed > 0.1f) {
                bobX = std::cos(bobTimer * 0.5f) * 0.025f;
                bobY = std::abs(std::sin(bobTimer)) * 0.022f;
            }

            return defaultOffset 
                + swaySpring.position 
                + Vec3(bobX, -bobY, 0.0f) 
                + recoilSpring.position;
        }

        Vec3 calculateRotationOffset(const Vec3& defaultRot) const {
            // Recoil pitch kick + roll sway
            float kickPitch = recoilSpring.position.y * 80.0f;
            float swayRoll = swaySpring.position.x * 35.0f;
            return defaultRot + Vec3(kickPitch, 0.0f, swayRoll);
        }
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
