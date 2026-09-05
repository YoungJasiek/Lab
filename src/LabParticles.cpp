#include "LabParticles.h"
#include "LabRenderer.h"
#include "LabMap.h"
#include <glad/gl.h>
#include <cstdlib>
#include <algorithm>
#include <iostream>

namespace Lab {

    static const char* particleVertexSrc = R"(
        #version 450 core
        layout (location = 0) in vec3 aPos;
        layout (location = 1) in vec2 aTexCoords;
        layout (location = 2) in vec4 aColor;

        out vec2 TexCoords;
        out vec4 ParticleColor;

        uniform mat4 view;
        uniform mat4 projection;

        void main() {
            TexCoords = aTexCoords;
            ParticleColor = aColor;
            gl_Position = projection * view * vec4(aPos, 1.0);
        }
    )";

    static const char* particleFragmentSrc = R"(
        #version 450 core
        out vec4 FragColor;

        in vec2 TexCoords;
        in vec4 ParticleColor;

        uniform int isAdditive;

        void main() {
            vec2 d = TexCoords * 2.0 - 1.0;
            float distSq = dot(d, d);
            if (distSq > 1.0) discard;

            // Soft circular Hermite falloff
            float radial = clamp(1.0 - distSq, 0.0, 1.0);
            float alpha = ParticleColor.a * radial;
            if (alpha < 0.005) discard;

            if (isAdditive == 1) {
                vec3 glow = ParticleColor.rgb * (1.0 + (1.0 - distSq) * 1.6);
                FragColor = vec4(glow * alpha, alpha);
            } else {
                FragColor = vec4(ParticleColor.rgb, alpha);
            }
        }
    )";

    static float randomFloat(float minVal, float maxVal) {
        float r = static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
        return minVal + r * (maxVal - minVal);
    }

    static Vec3 randomVector(float minVal, float maxVal) {
        return Vec3(
            randomFloat(minVal, maxVal),
            randomFloat(minVal, maxVal),
            randomFloat(minVal, maxVal)
        );
    }

    ParticleSystem::ParticleSystem() {
        _particles.reserve(_maxParticles);
    }

    ParticleSystem::~ParticleSystem() {
        shutdown();
    }

    void ParticleSystem::init() {
        if (_vao == 0) {
            _particleShader = std::make_unique<Shader>(particleVertexSrc, particleFragmentSrc);

            glGenVertexArrays(1, &_vao);
            glGenBuffers(1, &_vbo);

            glBindVertexArray(_vao);
            glBindBuffer(GL_ARRAY_BUFFER, _vbo);

            // Pre-allocate VBO for maximum batch size (e.g. 3000 particles * 6 vertices = 18000 vertices)
            size_t maxVertices = _maxParticles * 6;
            glBufferData(GL_ARRAY_BUFFER, maxVertices * sizeof(ParticleVertex), nullptr, GL_DYNAMIC_DRAW);

            // Position (location = 0)
            glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(ParticleVertex), (void*)offsetof(ParticleVertex, position));
            glEnableVertexAttribArray(0);

            // TexCoords (location = 1)
            glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(ParticleVertex), (void*)offsetof(ParticleVertex, texCoords));
            glEnableVertexAttribArray(1);

            // Color (location = 2)
            glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(ParticleVertex), (void*)offsetof(ParticleVertex, color));
            glEnableVertexAttribArray(2);

            glBindVertexArray(0);
        }
    }

    void ParticleSystem::shutdown() {
        if (_vbo != 0) {
            glDeleteBuffers(1, &_vbo);
            _vbo = 0;
        }
        if (_vao != 0) {
            glDeleteVertexArrays(1, &_vao);
            _vao = 0;
        }
        _particleShader.reset();
        _particles.clear();
    }

    void ParticleSystem::clear() {
        _particles.clear();
    }

    void ParticleSystem::spawnParticle(const Particle& p) {
        if (_particles.size() >= _maxParticles) {
            // Replace oldest particle
            _particles.erase(_particles.begin());
        }
        _particles.push_back(p);
    }

    void ParticleSystem::update(float dt, const LabMap* map) {
        for (auto it = _particles.begin(); it != _particles.end(); ) {
            it->lifetime += dt;
            if (!it->isAlive()) {
                it = _particles.erase(it);
                continue;
            }

            // Atmospheric drag & natural turbulence
            if (it->type == ParticleType::Dust || it->type == ParticleType::Smoke) {
                it->velocity.x *= 0.95f;
                it->velocity.z *= 0.95f;
            } else if (it->type == ParticleType::Frost) {
                // Gentle floating wind sway
                it->velocity.x += std::sin(it->lifetime * 3.5f + it->position.y) * 0.12f * dt;
                it->velocity.z += std::cos(it->lifetime * 2.8f + it->position.x) * 0.12f * dt;
            }

            // Physics integration
            it->velocity += it->acceleration * dt;
            it->position += it->velocity * dt;
            it->rotation += it->rotationSpeed * dt;

            // Physical bounce and surface collision
            if (it->hasCollision) {
                // 1. Floor collision at Y = 0.05
                if (it->position.y < 0.05f) {
                    it->position.y = 0.05f;
                    it->velocity.y = -it->velocity.y * 0.42f; // restitution
                    it->velocity.x *= 0.65f;                  // friction
                    it->velocity.z *= 0.65f;
                    it->bounceCount--;
                    if (it->bounceCount <= 0) {
                        it->velocity = { 0.0f, 0.0f, 0.0f };
                        it->acceleration = { 0.0f, 0.0f, 0.0f };
                        it->hasCollision = false;
                    }
                }

                // 2. Solid Map Brush collision
                if (map && it->bounceCount > 0) {
                    for (const auto& b : map->brushes) {
                        Vec3 half = b.size * 0.5f;
                        Vec3 bMin = b.position - half;
                        Vec3 bMax = b.position + half;

                        if (it->position.x >= bMin.x && it->position.x <= bMax.x &&
                            it->position.y >= bMin.y && it->position.y <= bMax.y &&
                            it->position.z >= bMin.z && it->position.z <= bMax.z) {
                            
                            // Rebound along fastest exit axis
                            float dx1 = std::abs(it->position.x - bMin.x);
                            float dx2 = std::abs(it->position.x - bMax.x);
                            float dy1 = std::abs(it->position.y - bMin.y);
                            float dy2 = std::abs(it->position.y - bMax.y);
                            float dz1 = std::abs(it->position.z - bMin.z);
                            float dz2 = std::abs(it->position.z - bMax.z);

                            float minPen = std::min({ dx1, dx2, dy1, dy2, dz1, dz2 });
                            if (minPen == dx1) { it->position.x = bMin.x; it->velocity.x = -it->velocity.x * 0.5f; }
                            else if (minPen == dx2) { it->position.x = bMax.x; it->velocity.x = -it->velocity.x * 0.5f; }
                            else if (minPen == dy1) { it->position.y = bMin.y; it->velocity.y = -it->velocity.y * 0.5f; }
                            else if (minPen == dy2) { it->position.y = bMax.y; it->velocity.y = -it->velocity.y * 0.5f; }
                            else if (minPen == dz1) { it->position.z = bMin.z; it->velocity.z = -it->velocity.z * 0.5f; }
                            else { it->position.z = bMax.z; it->velocity.z = -it->velocity.z * 0.5f; }

                            it->bounceCount--;
                            if (it->bounceCount <= 0) {
                                it->velocity = { 0.0f, 0.0f, 0.0f };
                                it->acceleration = { 0.0f, 0.0f, 0.0f };
                                it->hasCollision = false;
                            }
                            break;
                        }
                    }
                }
            }

            ++it;
        }
    }

    void ParticleSystem::buildQuadVertices(const Particle& p, const Vec3& camRight, const Vec3& camUp,
                                          std::vector<ParticleVertex>& batch) {
        float s = p.getCurrentSize();
        float ang = p.rotation;
        Vec3 r = camRight * s;
        Vec3 u = camUp * s;

        if (std::abs(ang) > 0.001f) {
            float cosA = std::cos(ang);
            float sinA = std::sin(ang);
            Vec3 rotR = (camRight * cosA - camUp * sinA) * s;
            Vec3 rotU = (camRight * sinA + camUp * cosA) * s;
            r = rotR;
            u = rotU;
        }

        Vec3 col = p.getCurrentColor();
        float a = p.getCurrentAlpha();
        Vec4 color4{ col.x, col.y, col.z, a };

        Vec3 pos = p.position;
        Vec3 c0 = pos - r - u;
        Vec3 c1 = pos + r - u;
        Vec3 c2 = pos + r + u;
        Vec3 c3 = pos - r + u;

        // Two triangles forming billboard quad
        batch.push_back({ c0, Vec2(0.0f, 0.0f), color4 });
        batch.push_back({ c1, Vec2(1.0f, 0.0f), color4 });
        batch.push_back({ c2, Vec2(1.0f, 1.0f), color4 });

        batch.push_back({ c0, Vec2(0.0f, 0.0f), color4 });
        batch.push_back({ c2, Vec2(1.0f, 1.0f), color4 });
        batch.push_back({ c3, Vec2(0.0f, 1.0f), color4 });
    }

    void ParticleSystem::render(const Camera& camera) {
        if (_particles.empty()) return;
        if (!_particleShader || _vao == 0) init();

        Vec3 camRight = camera.getRight();
        Vec3 camUp = camera.getUp();

        _vertexBatchOpaque.clear();
        _vertexBatchAdditive.clear();

        // Sort / bucket particles by blending mode
        for (const auto& p : _particles) {
            if (p.isAdditive) {
                buildQuadVertices(p, camRight, camUp, _vertexBatchAdditive);
            } else {
                buildQuadVertices(p, camRight, camUp, _vertexBatchOpaque);
            }
        }

        glDepthMask(GL_FALSE); // Disable depth write for volumetric translucency
        glEnable(GL_BLEND);

        _particleShader->use();
        _particleShader->setMat4("view", camera.getViewMatrix());
        _particleShader->setMat4("projection", camera.getProjectionMatrix());

        glBindVertexArray(_vao);
        glBindBuffer(GL_ARRAY_BUFFER, _vbo);

        // Pass 1: Opaque / Regular Alpha Blending (Dust, Smoke, Blood, Frost)
        if (!_vertexBatchOpaque.empty()) {
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            _particleShader->setInt("isAdditive", 0);

            glBufferSubData(GL_ARRAY_BUFFER, 0, _vertexBatchOpaque.size() * sizeof(ParticleVertex), _vertexBatchOpaque.data());
            glDrawArrays(GL_TRIANGLES, 0, (GLsizei)_vertexBatchOpaque.size());
        }

        // Pass 2: Additive Blending (Fire, Sparks, Plasma energy)
        if (!_vertexBatchAdditive.empty()) {
            glBlendFunc(GL_SRC_ALPHA, GL_ONE);
            _particleShader->setInt("isAdditive", 1);

            glBufferSubData(GL_ARRAY_BUFFER, 0, _vertexBatchAdditive.size() * sizeof(ParticleVertex), _vertexBatchAdditive.data());
            glDrawArrays(GL_TRIANGLES, 0, (GLsizei)_vertexBatchAdditive.size());
        }

        glBindVertexArray(0);
        glDepthMask(GL_TRUE); // Restore standard depth write
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }

    void ParticleSystem::spawnImpact(const Vec3& hitPos, const Vec3& hitNormal, SurfaceType surface) {
        // 1. Spark spray along reflection normal
        int sparkCount = (surface == SurfaceType::Metal) ? 18 : 10;
        for (int i = 0; i < sparkCount; ++i) {
            Particle p;
            p.type = ParticleType::Spark;
            p.position = hitPos + hitNormal * 0.02f;
            Vec3 spreadDir = (hitNormal + randomVector(-0.75f, 0.75f)).normalized();
            p.velocity = spreadDir * randomFloat(4.5f, 11.0f);
            p.acceleration = Vec3(0.0f, -14.0f, 0.0f);
            p.startColor = (surface == SurfaceType::Metal) ? Vec3(1.0f, 0.95f, 0.6f) : Vec3(1.0f, 0.75f, 0.25f);
            p.endColor = Vec3(1.0f, 0.35f, 0.05f);
            p.startAlpha = 1.0f;
            p.endAlpha = 0.0f;
            p.startSize = randomFloat(0.025f, 0.05f);
            p.endSize = 0.01f;
            p.lifetime = 0.0f;
            p.maxLifetime = randomFloat(0.25f, 0.65f);
            p.isAdditive = true;
            p.hasCollision = true;
            p.bounceCount = 2;
            spawnParticle(p);
        }

        // 2. Concrete/Rock Dust puff
        int dustCount = (surface == SurfaceType::Concrete) ? 5 : 3;
        for (int i = 0; i < dustCount; ++i) {
            Particle p;
            p.type = ParticleType::Dust;
            p.position = hitPos + hitNormal * 0.05f + randomVector(-0.06f, 0.06f);
            p.velocity = (hitNormal * randomFloat(0.6f, 1.8f)) + randomVector(-0.35f, 0.35f);
            p.acceleration = Vec3(0.0f, 0.15f, 0.0f); // Slight upward drift
            p.startColor = Vec3(0.72f, 0.70f, 0.65f);
            p.endColor = Vec3(0.55f, 0.54f, 0.50f);
            p.startAlpha = randomFloat(0.55f, 0.85f);
            p.endAlpha = 0.0f;
            p.startSize = randomFloat(0.08f, 0.14f);
            p.endSize = randomFloat(0.35f, 0.65f);
            p.lifetime = 0.0f;
            p.maxLifetime = randomFloat(0.6f, 1.3f);
            p.rotation = randomFloat(0.0f, 6.28f);
            p.rotationSpeed = randomFloat(-1.2f, 1.2f);
            p.isAdditive = false;
            p.hasCollision = false;
            spawnParticle(p);
        }

        // 3. Concrete debris chunks
        for (int i = 0; i < 4; ++i) {
            Particle p;
            p.type = ParticleType::Debris;
            p.position = hitPos + hitNormal * 0.02f;
            Vec3 dir = (hitNormal + randomVector(-0.6f, 0.6f)).normalized();
            p.velocity = dir * randomFloat(2.5f, 6.5f);
            p.acceleration = Vec3(0.0f, -16.0f, 0.0f);
            p.startColor = Vec3(0.35f, 0.35f, 0.38f);
            p.endColor = Vec3(0.20f, 0.20f, 0.22f);
            p.startAlpha = 1.0f;
            p.endAlpha = 0.0f;
            p.startSize = randomFloat(0.035f, 0.065f);
            p.endSize = p.startSize;
            p.lifetime = 0.0f;
            p.maxLifetime = randomFloat(0.7f, 1.4f);
            p.rotation = randomFloat(0.0f, 6.28f);
            p.rotationSpeed = randomFloat(-5.0f, 5.0f);
            p.isAdditive = false;
            p.hasCollision = true;
            p.bounceCount = 3;
            spawnParticle(p);
        }
    }

    void ParticleSystem::spawnBlood(const Vec3& hitPos, const Vec3& hitNormal, bool isHeadshot) {
        int dropCount = isHeadshot ? 28 : 14;
        float speedMultiplier = isHeadshot ? 1.6f : 1.0f;

        // 1. Dripping and spraying blood droplets
        for (int i = 0; i < dropCount; ++i) {
            Particle p;
            p.type = ParticleType::Blood;
            p.position = hitPos + randomVector(-0.05f, 0.05f);
            Vec3 sprayDir = (hitNormal + randomVector(-0.6f, 0.6f)).normalized();
            p.velocity = sprayDir * randomFloat(2.0f, 6.0f) * speedMultiplier;
            p.acceleration = Vec3(0.0f, -14.0f, 0.0f);
            p.startColor = Vec3(0.65f, 0.04f, 0.04f); // Vivid crimson
            p.endColor = Vec3(0.35f, 0.01f, 0.01f);   // Coagulated dark red
            p.startAlpha = 0.95f;
            p.endAlpha = 0.0f;
            p.startSize = randomFloat(0.03f, 0.07f);
            p.endSize = randomFloat(0.04f, 0.09f);
            p.lifetime = 0.0f;
            p.maxLifetime = randomFloat(0.6f, 1.5f);
            p.isAdditive = false;
            p.hasCollision = true;
            p.bounceCount = 1;
            spawnParticle(p);
        }

        // 2. Volumetric Blood Mist Cloud
        int mistCount = isHeadshot ? 8 : 4;
        for (int i = 0; i < mistCount; ++i) {
            Particle p;
            p.type = ParticleType::Blood;
            p.position = hitPos + randomVector(-0.08f, 0.08f);
            p.velocity = (hitNormal * randomFloat(0.5f, 1.8f)) + randomVector(-0.4f, 0.4f);
            p.acceleration = Vec3(0.0f, -1.5f, 0.0f);
            p.startColor = Vec3(0.72f, 0.06f, 0.06f);
            p.endColor = Vec3(0.28f, 0.02f, 0.02f);
            p.startAlpha = randomFloat(0.6f, 0.85f);
            p.endAlpha = 0.0f;
            p.startSize = randomFloat(0.08f, 0.15f);
            p.endSize = randomFloat(0.30f, 0.65f);
            p.lifetime = 0.0f;
            p.maxLifetime = randomFloat(0.45f, 0.95f);
            p.rotation = randomFloat(0.0f, 6.28f);
            p.rotationSpeed = randomFloat(-1.5f, 1.5f);
            p.isAdditive = false;
            p.hasCollision = false;
            spawnParticle(p);
        }
    }

    void ParticleSystem::spawnExplosion(const Vec3& center, float radius, const Vec3& color) {
        // 1. High-energy Flame Core Burst (Additive)
        int flameCount = static_cast<int>(radius * 8.0f);
        for (int i = 0; i < flameCount; ++i) {
            Particle p;
            p.type = ParticleType::Fire;
            p.position = center + randomVector(-0.25f, 0.25f);
            Vec3 dir = randomVector(-1.0f, 1.0f).normalized();
            p.velocity = dir * randomFloat(3.0f, 14.0f);
            p.acceleration = Vec3(0.0f, 2.5f, 0.0f); // Thermal rising
            p.startColor = Vec3(1.0f, 0.95f, 0.4f);
            p.endColor = color * 0.7f;
            p.startAlpha = 1.0f;
            p.endAlpha = 0.0f;
            p.startSize = randomFloat(0.25f, 0.55f);
            p.endSize = randomFloat(0.85f, 1.6f);
            p.lifetime = 0.0f;
            p.maxLifetime = randomFloat(0.35f, 0.85f);
            p.rotation = randomFloat(0.0f, 6.28f);
            p.rotationSpeed = randomFloat(-3.0f, 3.0f);
            p.isAdditive = true;
            p.hasCollision = false;
            spawnParticle(p);
        }

        // 2. Heavy Volumetric Black/Dark Smoke (Opaque Alpha)
        int smokeCount = static_cast<int>(radius * 7.0f);
        for (int i = 0; i < smokeCount; ++i) {
            Particle p;
            p.type = ParticleType::Smoke;
            p.position = center + randomVector(-0.4f, 0.4f);
            Vec3 dir = Vec3(randomFloat(-1.0f, 1.0f), randomFloat(0.4f, 1.8f), randomFloat(-1.0f, 1.0f)).normalized();
            p.velocity = dir * randomFloat(2.0f, 7.0f);
            p.acceleration = Vec3(0.0f, 1.8f, 0.0f); // Upward smoke buoyancy
            p.startColor = Vec3(0.22f, 0.22f, 0.24f);
            p.endColor = Vec3(0.08f, 0.08f, 0.10f);
            p.startAlpha = randomFloat(0.7f, 0.95f);
            p.endAlpha = 0.0f;
            p.startSize = randomFloat(0.35f, 0.75f);
            p.endSize = randomFloat(1.4f, 2.8f);
            p.lifetime = 0.0f;
            p.maxLifetime = randomFloat(1.2f, 2.5f);
            p.rotation = randomFloat(0.0f, 6.28f);
            p.rotationSpeed = randomFloat(-0.8f, 0.8f);
            p.isAdditive = false;
            p.hasCollision = false;
            spawnParticle(p);
        }

        // 3. Fiery Shrapnel & Sparks
        int shrapnelCount = static_cast<int>(radius * 9.0f);
        for (int i = 0; i < shrapnelCount; ++i) {
            Particle p;
            p.type = ParticleType::Spark;
            p.position = center + randomVector(-0.15f, 0.15f);
            Vec3 dir = randomVector(-1.0f, 1.0f).normalized();
            p.velocity = dir * randomFloat(8.0f, 22.0f);
            p.acceleration = Vec3(0.0f, -18.0f, 0.0f);
            p.startColor = Vec3(1.0f, 0.8f, 0.2f);
            p.endColor = Vec3(1.0f, 0.25f, 0.05f);
            p.startAlpha = 1.0f;
            p.endAlpha = 0.0f;
            p.startSize = randomFloat(0.04f, 0.09f);
            p.endSize = 0.015f;
            p.lifetime = 0.0f;
            p.maxLifetime = randomFloat(0.6f, 1.4f);
            p.isAdditive = true;
            p.hasCollision = true;
            p.bounceCount = 3;
            spawnParticle(p);
        }
    }

    void ParticleSystem::spawnMuzzleEffect(const Vec3& muzzlePos, const Vec3& forwardDir, WeaponID weaponId) {
        switch (weaponId) {
            case WeaponID::Pipe:
                break;

            case WeaponID::Pistol: {
                // Gunsmoke puff
                Particle p;
                p.type = ParticleType::Smoke;
                p.position = muzzlePos;
                p.velocity = forwardDir * 1.8f + Vec3(0.0f, 0.4f, 0.0f);
                p.acceleration = Vec3(0.0f, 0.2f, 0.0f);
                p.startColor = Vec3(0.85f, 0.85f, 0.88f);
                p.endColor = Vec3(0.4f, 0.4f, 0.45f);
                p.startAlpha = 0.6f;
                p.endAlpha = 0.0f;
                p.startSize = 0.06f;
                p.endSize = 0.24f;
                p.lifetime = 0.0f;
                p.maxLifetime = 0.35f;
                p.rotation = randomFloat(0.0f, 6.28f);
                p.rotationSpeed = randomFloat(-2.0f, 2.0f);
                p.isAdditive = false;
                spawnParticle(p);

                // Small ejection spark
                Particle s;
                s.type = ParticleType::Spark;
                s.position = muzzlePos;
                s.velocity = forwardDir * 4.0f + randomVector(-0.8f, 0.8f);
                s.acceleration = Vec3(0.0f, -10.0f, 0.0f);
                s.startColor = Vec3(1.0f, 0.9f, 0.3f);
                s.endColor = Vec3(1.0f, 0.4f, 0.1f);
                s.startAlpha = 1.0f;
                s.endAlpha = 0.0f;
                s.startSize = 0.03f;
                s.endSize = 0.01f;
                s.lifetime = 0.0f;
                s.maxLifetime = 0.15f;
                s.isAdditive = true;
                spawnParticle(s);
                break;
            }

            case WeaponID::Shotgun: {
                // Heavy cone of muzzle smoke
                for (int i = 0; i < 4; ++i) {
                    Particle p;
                    p.type = ParticleType::Smoke;
                    p.position = muzzlePos + randomVector(-0.04f, 0.04f);
                    p.velocity = forwardDir * randomFloat(2.5f, 5.5f) + randomVector(-0.6f, 0.6f);
                    p.acceleration = Vec3(0.0f, 0.3f, 0.0f);
                    p.startColor = Vec3(0.80f, 0.80f, 0.82f);
                    p.endColor = Vec3(0.35f, 0.35f, 0.40f);
                    p.startAlpha = 0.75f;
                    p.endAlpha = 0.0f;
                    p.startSize = randomFloat(0.08f, 0.16f);
                    p.endSize = randomFloat(0.45f, 0.85f);
                    p.lifetime = 0.0f;
                    p.maxLifetime = randomFloat(0.45f, 0.85f);
                    p.rotation = randomFloat(0.0f, 6.28f);
                    p.rotationSpeed = randomFloat(-2.5f, 2.5f);
                    p.isAdditive = false;
                    spawnParticle(p);
                }
                // Hot shotgun embers
                for (int i = 0; i < 8; ++i) {
                    Particle s;
                    s.type = ParticleType::Spark;
                    s.position = muzzlePos;
                    s.velocity = forwardDir * randomFloat(5.0f, 12.0f) + randomVector(-1.5f, 1.5f);
                    s.acceleration = Vec3(0.0f, -14.0f, 0.0f);
                    s.startColor = Vec3(1.0f, 0.8f, 0.2f);
                    s.endColor = Vec3(1.0f, 0.3f, 0.05f);
                    s.startAlpha = 1.0f;
                    s.endAlpha = 0.0f;
                    s.startSize = randomFloat(0.03f, 0.06f);
                    s.endSize = 0.01f;
                    s.lifetime = 0.0f;
                    s.maxLifetime = randomFloat(0.2f, 0.45f);
                    s.isAdditive = true;
                    s.hasCollision = true;
                    spawnParticle(s);
                }
                break;
            }

            case WeaponID::M4A4S: {
                // Silenced suppressed puff
                Particle p;
                p.type = ParticleType::Smoke;
                p.position = muzzlePos;
                p.velocity = forwardDir * 1.2f;
                p.startColor = Vec3(0.4f, 0.45f, 0.5f);
                p.endColor = Vec3(0.2f, 0.2f, 0.25f);
                p.startAlpha = 0.35f;
                p.endAlpha = 0.0f;
                p.startSize = 0.04f;
                p.endSize = 0.15f;
                p.lifetime = 0.0f;
                p.maxLifetime = 0.20f;
                p.isAdditive = false;
                spawnParticle(p);
                break;
            }

            case WeaponID::SG553:
            case WeaponID::Minigun: {
                // Rapid muzzle smoke + sparks
                Particle p;
                p.type = ParticleType::Smoke;
                p.position = muzzlePos;
                p.velocity = forwardDir * 2.2f + randomVector(-0.25f, 0.25f);
                p.acceleration = Vec3(0.0f, 0.3f, 0.0f);
                p.startColor = Vec3(0.75f, 0.75f, 0.8f);
                p.endColor = Vec3(0.3f, 0.3f, 0.35f);
                p.startAlpha = 0.55f;
                p.endAlpha = 0.0f;
                p.startSize = 0.06f;
                p.endSize = 0.28f;
                p.lifetime = 0.0f;
                p.maxLifetime = 0.3f;
                p.isAdditive = false;
                spawnParticle(p);

                for (int i = 0; i < 3; ++i) {
                    Particle s;
                    s.type = ParticleType::Spark;
                    s.position = muzzlePos;
                    s.velocity = forwardDir * randomFloat(6.0f, 15.0f) + randomVector(-1.2f, 1.2f);
                    s.acceleration = Vec3(0.0f, -12.0f, 0.0f);
                    s.startColor = Vec3(1.0f, 0.95f, 0.4f);
                    s.endColor = Vec3(1.0f, 0.35f, 0.05f);
                    s.startAlpha = 1.0f;
                    s.endAlpha = 0.0f;
                    s.startSize = 0.035f;
                    s.endSize = 0.01f;
                    s.lifetime = 0.0f;
                    s.maxLifetime = 0.22f;
                    s.isAdditive = true;
                    spawnParticle(s);
                }
                break;
            }

            case WeaponID::PlasmaGun: {
                // Cyan ionized plasma discharge
                for (int i = 0; i < 5; ++i) {
                    Particle p;
                    p.type = ParticleType::Plasma;
                    p.position = muzzlePos + randomVector(-0.06f, 0.06f);
                    p.velocity = forwardDir * randomFloat(3.0f, 7.0f) + randomVector(-0.8f, 0.8f);
                    p.startColor = Vec3(0.3f, 0.95f, 1.0f);
                    p.endColor = Vec3(0.1f, 0.4f, 0.9f);
                    p.startAlpha = 1.0f;
                    p.endAlpha = 0.0f;
                    p.startSize = randomFloat(0.06f, 0.12f);
                    p.endSize = 0.02f;
                    p.lifetime = 0.0f;
                    p.maxLifetime = 0.25f;
                    p.isAdditive = true;
                    spawnParticle(p);
                }
                break;
            }

            case WeaponID::Railgun: {
                // Electric blue magnetic discharge
                for (int i = 0; i < 10; ++i) {
                    Particle p;
                    p.type = ParticleType::Spark;
                    p.position = muzzlePos + randomVector(-0.08f, 0.08f);
                    p.velocity = forwardDir * randomFloat(8.0f, 20.0f) + randomVector(-2.0f, 2.0f);
                    p.acceleration = Vec3(0.0f, -4.0f, 0.0f);
                    p.startColor = Vec3(0.5f, 0.85f, 1.0f);
                    p.endColor = Vec3(0.1f, 0.3f, 0.95f);
                    p.startAlpha = 1.0f;
                    p.endAlpha = 0.0f;
                    p.startSize = randomFloat(0.04f, 0.08f);
                    p.endSize = 0.015f;
                    p.lifetime = 0.0f;
                    p.maxLifetime = 0.35f;
                    p.isAdditive = true;
                    spawnParticle(p);
                }
                break;
            }

            case WeaponID::RPG: {
                // Rocket backblast and muzzle cloud
                for (int i = 0; i < 6; ++i) {
                    Particle p;
                    p.type = ParticleType::Smoke;
                    p.position = muzzlePos + randomVector(-0.1f, 0.1f);
                    p.velocity = forwardDir * randomFloat(3.0f, 7.0f) + randomVector(-0.8f, 0.8f);
                    p.acceleration = Vec3(0.0f, 0.5f, 0.0f);
                    p.startColor = Vec3(0.7f, 0.7f, 0.75f);
                    p.endColor = Vec3(0.2f, 0.2f, 0.25f);
                    p.startAlpha = 0.85f;
                    p.endAlpha = 0.0f;
                    p.startSize = randomFloat(0.12f, 0.24f);
                    p.endSize = randomFloat(0.6f, 1.2f);
                    p.lifetime = 0.0f;
                    p.maxLifetime = randomFloat(0.6f, 1.1f);
                    p.isAdditive = false;
                    spawnParticle(p);
                }
                break;
            }

            default:
                break;
        }
    }

    void ParticleSystem::spawnProjectileTrail(const Vec3& pos, WeaponID weaponId) {
        if (weaponId == WeaponID::RPG) {
            // Rocket exhaust smoke
            Particle s;
            s.type = ParticleType::Smoke;
            s.position = pos + randomVector(-0.04f, 0.04f);
            s.velocity = randomVector(-0.25f, 0.25f) + Vec3(0.0f, 0.15f, 0.0f);
            s.startColor = Vec3(0.65f, 0.65f, 0.68f);
            s.endColor = Vec3(0.25f, 0.25f, 0.28f);
            s.startAlpha = 0.65f;
            s.endAlpha = 0.0f;
            s.startSize = 0.08f;
            s.endSize = 0.38f;
            s.lifetime = 0.0f;
            s.maxLifetime = 0.45f;
            s.isAdditive = false;
            spawnParticle(s);

            // Small flame flicker
            Particle f;
            f.type = ParticleType::Fire;
            f.position = pos;
            f.velocity = randomVector(-0.35f, 0.35f);
            f.startColor = Vec3(1.0f, 0.85f, 0.3f);
            f.endColor = Vec3(1.0f, 0.3f, 0.05f);
            f.startAlpha = 0.9f;
            f.endAlpha = 0.0f;
            f.startSize = 0.06f;
            f.endSize = 0.14f;
            f.lifetime = 0.0f;
            f.maxLifetime = 0.12f;
            f.isAdditive = true;
            spawnParticle(f);
        } else if (weaponId == WeaponID::PlasmaGun) {
            // Plasma energy orb trail
            Particle p;
            p.type = ParticleType::Plasma;
            p.position = pos + randomVector(-0.03f, 0.03f);
            p.velocity = randomVector(-0.4f, 0.4f);
            p.startColor = Vec3(0.2f, 0.9f, 1.0f);
            p.endColor = Vec3(0.05f, 0.3f, 0.8f);
            p.startAlpha = 0.95f;
            p.endAlpha = 0.0f;
            p.startSize = 0.08f;
            p.endSize = 0.02f;
            p.lifetime = 0.0f;
            p.maxLifetime = 0.18f;
            p.isAdditive = true;
            spawnParticle(p);
        }
    }

    void ParticleSystem::spawnBeamSparks(const Vec3& start, const Vec3& end, const Vec3& color, int count) {
        Vec3 dir = end - start;
        for (int i = 0; i < count; ++i) {
            float t = randomFloat(0.05f, 0.95f);
            Vec3 pos = start + dir * t + randomVector(-0.12f, 0.12f);

            Particle p;
            p.type = ParticleType::Spark;
            p.position = pos;
            p.velocity = randomVector(-2.5f, 2.5f);
            p.acceleration = Vec3(0.0f, -2.0f, 0.0f);
            p.startColor = color;
            p.endColor = color * 0.4f;
            p.startAlpha = 1.0f;
            p.endAlpha = 0.0f;
            p.startSize = randomFloat(0.03f, 0.07f);
            p.endSize = 0.01f;
            p.lifetime = 0.0f;
            p.maxLifetime = randomFloat(0.15f, 0.35f);
            p.isAdditive = true;
            spawnParticle(p);
        }
    }

    void ParticleSystem::spawnAmbientWeather(const Vec3& playerPos, int count, bool isCryo) {
        for (int i = 0; i < count; ++i) {
            Particle p;
            p.type = ParticleType::Frost;
            // Spawn in a sphere around player
            p.position = playerPos + Vec3(
                randomFloat(-16.0f, 16.0f),
                randomFloat(1.0f, 8.0f),
                randomFloat(-16.0f, 16.0f)
            );

            if (isCryo) {
                // Cold snow / ice crystal
                p.velocity = Vec3(randomFloat(-0.8f, 0.8f), randomFloat(-1.2f, -0.4f), randomFloat(-0.8f, 0.8f));
                p.acceleration = Vec3(0.0f, -0.2f, 0.0f);
                p.startColor = Vec3(0.85f, 0.95f, 1.0f);
                p.endColor = Vec3(0.70f, 0.85f, 0.95f);
                p.startAlpha = randomFloat(0.4f, 0.75f);
                p.endAlpha = 0.0f;
                p.startSize = randomFloat(0.02f, 0.05f);
                p.endSize = p.startSize;
                p.lifetime = 0.0f;
                p.maxLifetime = randomFloat(4.0f, 7.5f);
                p.isAdditive = false;
                p.hasCollision = true;
                p.bounceCount = 1;
            } else {
                // Floating dust mote in industrial facility
                p.velocity = Vec3(randomFloat(-0.2f, 0.2f), randomFloat(-0.1f, 0.1f), randomFloat(-0.2f, 0.2f));
                p.acceleration = Vec3(0.0f, 0.0f, 0.0f);
                p.startColor = Vec3(0.8f, 0.78f, 0.72f);
                p.endColor = Vec3(0.6f, 0.58f, 0.52f);
                p.startAlpha = randomFloat(0.25f, 0.55f);
                p.endAlpha = 0.0f;
                p.startSize = randomFloat(0.015f, 0.035f);
                p.endSize = p.startSize;
                p.lifetime = 0.0f;
                p.maxLifetime = randomFloat(3.5f, 6.0f);
                p.isAdditive = false;
                p.hasCollision = false;
            }

            spawnParticle(p);
        }
    }

} // namespace Lab
