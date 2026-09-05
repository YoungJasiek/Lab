#pragma once
#include <vector>
#include <string>
#include <memory>
#include <cmath>
#include "LabMath.h"
#include "LabCamera.h"
#include "LabWeapon.h"

namespace Lab {

    class LabMap;
    class Shader;

    enum class ParticleType {
        Spark,      // High speed, glowing, bouncing, additive
        Dust,       // Expanding, slow rising, fading out
        Blood,      // Crimson splatter, falls with gravity
        Smoke,      // Volumetric soft smoke puff, rising and expanding
        Fire,       // Intense glowing flame burst
        Plasma,     // High-energy ionized particle
        Debris,     // Solid tumbling stone/metal fragment
        Frost       // Drifting snow/frost crystal
    };

    enum class SurfaceType {
        Concrete,
        Metal,
        Ice,
        Wood,
        Flesh
    };

    struct Particle {
        Vec3 position{ 0.0f, 0.0f, 0.0f };
        Vec3 velocity{ 0.0f, 0.0f, 0.0f };
        Vec3 acceleration{ 0.0f, -9.81f, 0.0f };

        Vec3 startColor{ 1.0f, 1.0f, 1.0f };
        Vec3 endColor{ 1.0f, 1.0f, 1.0f };
        float startAlpha = 1.0f;
        float endAlpha = 0.0f;

        float startSize = 0.1f;
        float endSize = 0.2f;

        float lifetime = 0.0f;
        float maxLifetime = 1.0f;

        float rotation = 0.0f;
        float rotationSpeed = 0.0f;

        bool isAdditive = false;
        bool hasCollision = false;
        int bounceCount = 2;
        ParticleType type = ParticleType::Spark;

        bool isAlive() const { return lifetime < maxLifetime; }
        float getProgress() const { return (maxLifetime > 0.0f) ? std::clamp(lifetime / maxLifetime, 0.0f, 1.0f) : 1.0f; }

        Vec3 getCurrentColor() const {
            float t = getProgress();
            return startColor * (1.0f - t) + endColor * t;
        }

        float getCurrentAlpha() const {
            float t = getProgress();
            return startAlpha * (1.0f - t) + endAlpha * t;
        }

        float getCurrentSize() const {
            float t = getProgress();
            return startSize * (1.0f - t) + endSize * t;
        }
    };

    struct ParticleVertex {
        Vec3 position;
        Vec2 texCoords;
        Vec4 color; // r, g, b, a
    };

    class ParticleSystem {
    public:
        ParticleSystem();
        ~ParticleSystem();

        void init();
        void shutdown();
        void clear();

        void update(float dt, const LabMap* map = nullptr);
        void render(const Camera& camera);

        // Core Emitters & Presets
        void spawnParticle(const Particle& p);

        // Surface Bullet Impact (Concrete dust + Spark spray + Debris chunks)
        void spawnImpact(const Vec3& hitPos, const Vec3& hitNormal, SurfaceType surface = SurfaceType::Concrete);

        // Biological Blood Splatter (Drops + directional mist)
        void spawnBlood(const Vec3& hitPos, const Vec3& hitNormal, bool isHeadshot = false);

        // Explosive Detonation (Fire burst + heavy smoke + fiery shrapnel)
        void spawnExplosion(const Vec3& center, float radius = 4.0f, const Vec3& color = { 1.0f, 0.5f, 0.1f });

        // Weapon Muzzle Blast & Gunsmoke
        void spawnMuzzleEffect(const Vec3& muzzlePos, const Vec3& forwardDir, WeaponID weaponId);

        // Projectile Flying Trail (RPG smoke/flame, Plasma energy trail)
        void spawnProjectileTrail(const Vec3& pos, WeaponID weaponId);

        // Ionized Beam Sparks (Railgun & Energy discharges)
        void spawnBeamSparks(const Vec3& start, const Vec3& end, const Vec3& color, int count = 12);

        // Atmospheric Weather & Ambient Dust / Snow
        void spawnAmbientWeather(const Vec3& playerPos, int count = 2, bool isCryo = true);

        size_t getActiveCount() const { return _particles.size(); }
        size_t getMaxParticles() const { return _maxParticles; }

    private:
        std::vector<Particle> _particles;
        size_t _maxParticles = 3000;

        unsigned int _vao = 0;
        unsigned int _vbo = 0;
        std::unique_ptr<Shader> _particleShader;

        std::vector<ParticleVertex> _vertexBatchOpaque;   // Alpha blend (dust, smoke, blood)
        std::vector<ParticleVertex> _vertexBatchAdditive; // Additive blend (fire, sparks, plasma)

        void buildQuadVertices(const Particle& p, const Vec3& camRight, const Vec3& camUp,
                               std::vector<ParticleVertex>& batch);
    };

} // namespace Lab
