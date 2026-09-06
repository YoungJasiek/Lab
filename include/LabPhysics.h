#pragma once
#include <vector>
#include <memory>
#include <string>
#include <functional>
#include <algorithm>
#include "LabMath.h"
#include "LabCombat.h"
#include "LabCollision.h"

namespace Lab {

    class Texture;

    enum class PropType {
        Static,
        Crate,           // Destructible wooden crate
        ExplosiveBarrel, // Red hazardous fuel barrel with radial blast
        DebrisChunk      // Physical shattered debris fragment with finite lifetime
    };

    struct PhysicsMaterial {
        float restitution = 0.30f; // Bounciness
        float friction = 0.55f;    // Surface friction
        float density = 500.0f;    // kg/m^3
    };

    class PhysicsWorld;

    struct RigidBody {
        int id = 0;
        PropType type = PropType::Crate;
        Vec3 position{ 0.0f, 0.0f, 0.0f };
        Quat orientation = Quat::identity();
        Vec3 linearVelocity{ 0.0f, 0.0f, 0.0f };
        Vec3 angularVelocity{ 0.0f, 0.0f, 0.0f }; // Radians per second
        Vec3 forces{ 0.0f, 0.0f, 0.0f };
        Vec3 torques{ 0.0f, 0.0f, 0.0f };

        Vec3 halfExtents{ 0.45f, 0.45f, 0.45f };
        float mass = 20.0f;
        float invMass = 1.0f / 20.0f;
        Vec3 inertiaLocal{ 1.0f, 1.0f, 1.0f };
        Vec3 invInertiaLocal{ 1.0f, 1.0f, 1.0f };

        float linearDamping = 0.985f;
        float angularDamping = 0.965f;
        PhysicsMaterial material;

        bool isStatic = false;
        bool isSleeping = false;
        float sleepTimer = 0.0f;

        float health = 40.0f;
        float maxHealth = 40.0f;
        bool isDestroyed = false;

        Vec3 color{ 1.0f, 1.0f, 1.0f };
        std::string texturePath = "";

        float lifetime = 0.0f;
        float maxLifetime = 6.0f;

        void setBox(const Vec3& halfExt, float m);
        void applyImpulse(const Vec3& impulse, const Vec3& contactPoint);
        void applyCentralImpulse(const Vec3& impulse);
        void applyTorqueImpulse(const Vec3& torqueImpulse);

        Mat4 getTransformMatrix() const;
        void getAABB(Vec3& outMin, Vec3& outMax) const;
        std::vector<Vec3> getCorners() const;

        bool raycast(const Vec3& rayOrigin, const Vec3& rayDir, float& outDist, Vec3& outNormal) const;
    };

    struct ExplosionEvent {
        Vec3 position{ 0.0f, 0.0f, 0.0f };
        float radius = 6.0f;
        float maxDamage = 120.0f;
        float maxImpulse = 400.0f;
        int sourcePropId = -1;
    };

    class PhysicsWorld {
    public:
        std::vector<std::unique_ptr<RigidBody>> bodies;
        Vec3 gravity{ 0.0f, -13.5f, 0.0f };

        // Procedural textures for physics props
        std::unique_ptr<Texture> crateTexture;
        std::unique_ptr<Texture> barrelTexture;
        std::unique_ptr<Texture> debrisTexture;

        // Pending events for game systems (particles, audio, damage)
        std::vector<ExplosionEvent> pendingExplosions;

        void init();
        void clear();

        RigidBody* spawnCrate(const Vec3& pos, const Vec3& halfExt = Vec3(0.45f, 0.45f, 0.45f), float mass = 22.0f);
        RigidBody* spawnExplosiveBarrel(const Vec3& pos, const Vec3& halfExt = Vec3(0.32f, 0.48f, 0.32f), float mass = 30.0f);
        RigidBody* spawnDebris(const Vec3& pos, const Vec3& halfExt, const Vec3& vel, const Vec3& angVel, PropType parentType, float lifetime = 5.0f);

        void spawnCrateDebris(const Vec3& pos, const Vec3& halfExt, const Vec3& impactDir);
        void spawnBarrelDebris(const Vec3& pos, const Vec3& halfExt, const Vec3& impactDir);

        void update(float dt, const std::vector<CollisionBox>& obstacles);
        void applyExplosionImpulse(const Vec3& epicenter, float radius, float maxImpulse, float damage, int sourcePropId = -1);

        bool raycast(const Vec3& rayOrigin, const Vec3& rayDir, RaycastHit& outHit, int excludeId = -1);
        bool takeDamage(int propId, float damage, const Vec3& hitPoint, const Vec3& hitDir);

        void render() const;
        void renderShadow() const;

    private:
        int _nextBodyId = 1;
        void resolveGroundCollision(RigidBody& body, float dt);
        void resolveObstacleCollision(RigidBody& body, const CollisionBox& obstacle, float dt);
        void resolveBodyVsBodyCollision(RigidBody& a, RigidBody& b, float dt);
    };

} // namespace Lab
