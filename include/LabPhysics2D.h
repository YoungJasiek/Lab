#pragma once
#include <vector>
#include <cstdint>
#include <cmath>
#include <algorithm>
#include "LabMath.h"

namespace Lab {

    enum class BodyType2D {
        Static,
        Kinematic,
        Dynamic
    };

    enum class ColliderType2D {
        Box,
        Circle
    };

    struct AABB2D {
        Vec2 min;
        Vec2 max;

        bool overlaps(const AABB2D& other) const {
            if (max.x < other.min.x || min.x > other.max.x) return false;
            if (max.y < other.min.y || min.y > other.max.y) return false;
            return true;
        }
    };

    struct RaycastHit2D {
        bool hit{false};
        Vec2 point{0.0f, 0.0f};
        Vec2 normal{0.0f, 0.0f};
        float fraction{1.0f};
        size_t bodyId{0};
    };

    struct Contact2D {
        size_t bodyA{0};
        size_t bodyB{0};
        Vec2 normal{0.0f, 0.0f}; // Points from A to B
        float penetration{0.0f};
    };

    struct RigidBody2D {
        size_t id{0};
        BodyType2D type{BodyType2D::Dynamic};
        ColliderType2D colliderType{ColliderType2D::Box};

        Vec2 position{0.0f, 0.0f};
        Vec2 velocity{0.0f, 0.0f};
        Vec2 force{0.0f, 0.0f};

        float rotation{0.0f};       // Radians
        float angularVelocity{0.0f};
        float torque{0.0f};

        float mass{1.0f};
        float invMass{1.0f};
        float inertia{1.0f};
        float invInertia{1.0f};

        float restitution{0.2f};     // Bounciness [0..1]
        float friction{0.3f};        // Friction [0..1]

        Vec2 boxSize{1.0f, 1.0f};    // Full width and height
        float circleRadius{0.5f};

        void setMass(float m) {
            if (type == BodyType2D::Static) {
                mass = 0.0f;
                invMass = 0.0f;
                inertia = 0.0f;
                invInertia = 0.0f;
                return;
            }

            mass = std::max(0.0001f, m);
            invMass = 1.0f / mass;

            if (colliderType == ColliderType2D::Box) {
                inertia = (1.0f / 12.0f) * mass * (boxSize.x * boxSize.x + boxSize.y * boxSize.y);
            } else {
                inertia = 0.5f * mass * circleRadius * circleRadius;
            }
            invInertia = (inertia > 0.0f) ? (1.0f / inertia) : 0.0f;
        }

        void applyForce(const Vec2& f) {
            if (type == BodyType2D::Dynamic) {
                force += f;
            }
        }

        void applyImpulse(const Vec2& impulse) {
            if (type == BodyType2D::Dynamic) {
                velocity += impulse * invMass;
            }
        }

        AABB2D getAABB() const {
            if (colliderType == ColliderType2D::Box) {
                Vec2 half = boxSize * 0.5f;
                return { position - half, position + half };
            } else {
                Vec2 r(circleRadius, circleRadius);
                return { position - r, position + r };
            }
        }
    };

    class PhysicsWorld2D {
    public:
        PhysicsWorld2D(const Vec2& gravity = { 0.0f, -9.81f });
        ~PhysicsWorld2D() = default;

        void setGravity(const Vec2& gravity) { _gravity = gravity; }
        Vec2 getGravity() const { return _gravity; }

        size_t createBoxBody(BodyType2D type, const Vec2& position, const Vec2& size, float mass = 1.0f);
        size_t createCircleBody(BodyType2D type, const Vec2& position, float radius, float mass = 1.0f);

        RigidBody2D* getBody(size_t id);
        const RigidBody2D* getBody(size_t id) const;
        const std::vector<RigidBody2D>& getBodies() const { return _bodies; }
        size_t getBodyCount() const { return _bodies.size(); }
        void clear();

        // Advance simulation by dt seconds
        void step(float dt, int velocityIterations = 4, int positionIterations = 2);

        // Raycast against all colliders in world
        bool raycast(const Vec2& start, const Vec2& direction, float maxDist, RaycastHit2D& outHit) const;

    private:
        void detectCollisions(std::vector<Contact2D>& contacts);
        bool checkCollision(const RigidBody2D& a, const RigidBody2D& b, Contact2D& contact);
        void resolveContacts(const std::vector<Contact2D>& contacts);
        void positionalCorrection(const std::vector<Contact2D>& contacts);

        Vec2 _gravity{ 0.0f, -9.81f };
        std::vector<RigidBody2D> _bodies;
        size_t _nextId{1};
    };

} // namespace Lab
