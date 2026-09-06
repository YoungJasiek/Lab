#include "LabDecals.h"
#include <glad/gl.h>
#include <cmath>
#include <algorithm>
#include <cstdlib>
#include <iostream>

namespace Lab {

    static const char* decalVS = R"(
        #version 450 core
        layout (location = 0) in vec3 aPos;
        layout (location = 1) in vec3 aNormal;
        layout (location = 2) in vec2 aTexCoords;
        layout (location = 3) in vec3 aColor;

        uniform mat4 view;
        uniform mat4 projection;

        out vec2 TexCoords;
        out float DecalAlpha;

        void main() {
            TexCoords = aTexCoords;
            DecalAlpha = aColor.r;
            gl_Position = projection * view * vec4(aPos, 1.0);
        }
    )";

    static const char* decalFS = R"(
        #version 450 core
        in vec2 TexCoords;
        in float DecalAlpha;

        out vec4 FragColor;

        uniform sampler2D decalTex;

        void main() {
            vec4 tex = texture(decalTex, TexCoords);
            float finalAlpha = tex.a * DecalAlpha;
            if (finalAlpha < 0.01) discard;
            FragColor = vec4(tex.rgb, finalAlpha);
        }
    )";

    DecalSystem::DecalSystem() = default;

    DecalSystem::~DecalSystem() {
        shutdown();
    }

    void DecalSystem::init() {
        if (_initialized) return;
        initGPU();
        initTextures();
        _initialized = true;
    }

    void DecalSystem::shutdown() {
        if (!_initialized) return;
        if (_vao) { glDeleteVertexArrays(1, &_vao); _vao = 0; }
        if (_vbo) { glDeleteBuffers(1, &_vbo); _vbo = 0; }
        if (_ebo) { glDeleteBuffers(1, &_ebo); _ebo = 0; }
        _decalShader.reset();
        _texConcreteHole.reset();
        _texMetalHole.reset();
        _texBlood.reset();
        _texScorch.reset();
        _decals.clear();
        _initialized = false;
    }

    void DecalSystem::initGPU() {
        _decalShader = std::make_unique<Shader>(decalVS, decalFS);

        glGenVertexArrays(1, &_vao);
        glGenBuffers(1, &_vbo);
        glGenBuffers(1, &_ebo);

        glBindVertexArray(_vao);
        glBindBuffer(GL_ARRAY_BUFFER, _vbo);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, _ebo);

        // Position: location 0
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, position));

        // Normal: location 1
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, normal));

        // TexCoords: location 2
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, texCoords));

        // Color (alpha): location 3
        glEnableVertexAttribArray(3);
        glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, color));

        glBindVertexArray(0);
    }

    void DecalSystem::initTextures() {
        const int S = 64;
        std::vector<unsigned char> img(S * S * 4, 0);

        // 1. Concrete Bullet Hole
        for (int y = 0; y < S; ++y) {
            for (int x = 0; x < S; ++x) {
                int idx = (y * S + x) * 4;
                float dx = x - 31.5f;
                float dy = y - 31.5f;
                float r = std::sqrt(dx * dx + dy * dy);
                float angle = std::atan2(dy, dx);
                float crack = std::sin(angle * 7.0f) * 2.0f + std::sin(angle * 13.0f) * 1.5f;
                float rEff = r + crack;

                if (rEff < 5.0f) {
                    img[idx + 0] = 25; img[idx + 1] = 25; img[idx + 2] = 28; img[idx + 3] = 250;
                } else if (rEff < 14.0f) {
                    float t = (rEff - 5.0f) / 9.0f;
                    img[idx + 0] = (unsigned char)(40 + 100 * t);
                    img[idx + 1] = (unsigned char)(40 + 105 * t);
                    img[idx + 2] = (unsigned char)(45 + 110 * t);
                    img[idx + 3] = (unsigned char)(250 - 70 * t);
                } else if (rEff < 28.0f) {
                    float t = (rEff - 14.0f) / 14.0f;
                    float fade = (1.0f - t) * (1.0f - t);
                    img[idx + 0] = 120; img[idx + 1] = 125; img[idx + 2] = 130;
                    img[idx + 3] = (unsigned char)(180.0f * fade);
                } else {
                    img[idx + 0] = 0; img[idx + 1] = 0; img[idx + 2] = 0; img[idx + 3] = 0;
                }
            }
        }
        _texConcreteHole = std::make_unique<Texture>(img.data(), S, S, 4);

        // 2. Metal Bullet Hole
        std::fill(img.begin(), img.end(), (unsigned char)0);
        for (int y = 0; y < S; ++y) {
            for (int x = 0; x < S; ++x) {
                int idx = (y * S + x) * 4;
                float dx = x - 31.5f;
                float dy = y - 31.5f;
                float r = std::sqrt(dx * dx + dy * dy);

                if (r < 4.0f) {
                    img[idx + 0] = 12; img[idx + 1] = 14; img[idx + 2] = 18; img[idx + 3] = 255;
                } else if (r < 10.0f) {
                    img[idx + 0] = 215; img[idx + 1] = 220; img[idx + 2] = 230; img[idx + 3] = 255;
                } else if (r < 22.0f) {
                    float t = (r - 10.0f) / 12.0f;
                    float fade = 1.0f - t;
                    img[idx + 0] = (unsigned char)(70 * fade);
                    img[idx + 1] = (unsigned char)(80 * fade);
                    img[idx + 2] = (unsigned char)(110 * fade);
                    img[idx + 3] = (unsigned char)(200 * fade);
                } else {
                    img[idx + 0] = 0; img[idx + 1] = 0; img[idx + 2] = 0; img[idx + 3] = 0;
                }
            }
        }
        _texMetalHole = std::make_unique<Texture>(img.data(), S, S, 4);

        // 3. Blood Splatter
        std::fill(img.begin(), img.end(), (unsigned char)0);
        for (int y = 0; y < S; ++y) {
            for (int x = 0; x < S; ++x) {
                int idx = (y * S + x) * 4;
                float dx = x - 31.5f;
                float dy = y - 31.5f;
                float r = std::sqrt(dx * dx + dy * dy);
                float angle = std::atan2(dy, dx);
                float splatter = std::sin(angle * 5.0f) * 4.0f + std::sin(angle * 11.0f) * 3.0f + (float)((x * 17 + y * 31) % 7);
                float rEff = r + splatter;

                if (rEff < 16.0f) {
                    img[idx + 0] = (unsigned char)(145 + ((x ^ y) & 7));
                    img[idx + 1] = 12;
                    img[idx + 2] = 18;
                    img[idx + 3] = 245;
                } else if (rEff < 26.0f) {
                    float t = (rEff - 16.0f) / 10.0f;
                    img[idx + 0] = 110;
                    img[idx + 1] = 8;
                    img[idx + 2] = 14;
                    img[idx + 3] = (unsigned char)(220 * (1.0f - t));
                } else {
                    img[idx + 0] = 0; img[idx + 1] = 0; img[idx + 2] = 0; img[idx + 3] = 0;
                }
            }
        }
        _texBlood = std::make_unique<Texture>(img.data(), S, S, 4);

        // 4. Explosive Scorch
        std::fill(img.begin(), img.end(), (unsigned char)0);
        for (int y = 0; y < S; ++y) {
            for (int x = 0; x < S; ++x) {
                int idx = (y * S + x) * 4;
                float dx = x - 31.5f;
                float dy = y - 31.5f;
                float r = std::sqrt(dx * dx + dy * dy);
                float angle = std::atan2(dy, dx);
                float blastTendril = std::sin(angle * 9.0f) * 3.5f + std::sin(angle * 17.0f) * 2.0f;
                float rEff = r + blastTendril;

                if (rEff < 12.0f) {
                    img[idx + 0] = 15; img[idx + 1] = 15; img[idx + 2] = 15; img[idx + 3] = 240;
                } else if (rEff < 30.0f) {
                    float t = (rEff - 12.0f) / 18.0f;
                    float fade = (1.0f - t) * (1.0f - t);
                    img[idx + 0] = 25; img[idx + 1] = 22; img[idx + 2] = 20;
                    img[idx + 3] = (unsigned char)(220.0f * fade);
                } else {
                    img[idx + 0] = 0; img[idx + 1] = 0; img[idx + 2] = 0; img[idx + 3] = 0;
                }
            }
        }
        _texScorch = std::make_unique<Texture>(img.data(), S, S, 4);
    }

    void DecalSystem::buildDecalGeometry(DecalInstance& decal) {
        Vec3 norm = decal.normal.normalized();
        if (norm.lengthSq() < 0.001f) norm = Vec3(0, 1, 0);

        Vec3 ref = (std::abs(norm.y) < 0.9f) ? Vec3(0, 1, 0) : Vec3(1, 0, 0);
        Vec3 t0 = Vec3::cross(norm, ref).normalized();
        Vec3 b0 = Vec3::cross(norm, t0).normalized();

        float angle = (float)(rand() % 360) * 3.14159265f / 180.0f;
        float c = std::cos(angle);
        float s = std::sin(angle);
        Vec3 T = t0 * c + b0 * s;
        Vec3 B = Vec3::cross(norm, T).normalized();

        float hs = decal.size * 0.5f;
        Vec3 offset = norm * 0.0025f; // 2.5mm offset prevents z-fighting

        decal.vertices.clear();
        decal.indices.clear();

        Vec3 p0 = decal.position + (-T - B) * hs + offset;
        Vec3 p1 = decal.position + ( T - B) * hs + offset;
        Vec3 p2 = decal.position + ( T + B) * hs + offset;
        Vec3 p3 = decal.position + (-T + B) * hs + offset;

        Vec3 col(decal.alpha, 1.0f, 1.0f);

        decal.vertices.push_back(Vertex(p0, norm, Vec2(0.0f, 0.0f), col));
        decal.vertices.push_back(Vertex(p1, norm, Vec2(1.0f, 0.0f), col));
        decal.vertices.push_back(Vertex(p2, norm, Vec2(1.0f, 1.0f), col));
        decal.vertices.push_back(Vertex(p3, norm, Vec2(0.0f, 1.0f), col));

        decal.indices = { 0, 1, 2,  0, 2, 3 };
    }

    void DecalSystem::spawnDecal(DecalType type, const Vec3& position, const Vec3& normal, float size, float maxLifetime) {
        init();

        if (_decals.size() >= _maxDecals) {
            _decals.erase(_decals.begin());
        }

        DecalInstance d;
        d.type = type;
        d.position = position;
        d.normal = normal;
        d.size = size;
        d.lifetime = 0.0f;
        d.maxLifetime = maxLifetime;
        d.alpha = 1.0f;

        buildDecalGeometry(d);
        _decals.push_back(std::move(d));
    }

    void DecalSystem::spawnExplosionScorch(const Vec3& center, float radius, const std::vector<MapBrush>& brushes) {
        init();

        // Project main scorch on floor below explosion
        spawnDecal(DecalType::ExplosiveScorch, Vec3(center.x, 0.0f, center.z), Vec3(0.0f, 1.0f, 0.0f), radius, 90.0f);

        // Project onto nearby vertical wall faces if within radius
        for (const auto& b : brushes) {
            Vec3 half = b.size * 0.5f;
            Vec3 bMin = b.position - half;
            Vec3 bMax = b.position + half;

            // Check distance to bounding box
            Vec3 closestPt(
                std::clamp(center.x, bMin.x, bMax.x),
                std::clamp(center.y, bMin.y, bMax.y),
                std::clamp(center.z, bMin.z, bMax.z)
            );

            float dist = (center - closestPt).length();
            if (dist < radius * 0.9f && dist > 0.05f) {
                Vec3 norm = (center - closestPt).normalized();
                // Snap normal to dominant axis
                if (std::abs(norm.x) > std::abs(norm.y) && std::abs(norm.x) > std::abs(norm.z)) {
                    norm = Vec3(norm.x > 0 ? 1.0f : -1.0f, 0, 0);
                } else if (std::abs(norm.z) > std::abs(norm.y)) {
                    norm = Vec3(0, 0, norm.z > 0 ? 1.0f : -1.0f);
                } else {
                    norm = Vec3(0, norm.y > 0 ? 1.0f : -1.0f, 0);
                }
                float wallScorchSize = (radius - dist) * 1.2f;
                if (wallScorchSize > 0.4f) {
                    spawnDecal(DecalType::ExplosiveScorch, closestPt, norm, wallScorchSize, 80.0f);
                }
            }
        }
    }

    void DecalSystem::update(float dt) {
        for (auto& d : _decals) {
            d.lifetime += dt;
            if (d.lifetime > d.maxLifetime - 5.0f) {
                float remaining = std::max(0.0f, d.maxLifetime - d.lifetime);
                d.alpha = remaining / 5.0f;
                for (auto& v : d.vertices) {
                    v.color.x = d.alpha;
                }
            }
        }

        _decals.erase(
            std::remove_if(_decals.begin(), _decals.end(), [](const DecalInstance& d) {
                return d.lifetime >= d.maxLifetime;
            }),
            _decals.end()
        );
    }

    void DecalSystem::render(const Camera& cam) {
        if (_decals.empty()) return;
        init();

        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDepthMask(GL_FALSE);
        glEnable(GL_POLYGON_OFFSET_FILL);
        glPolygonOffset(-2.0f, -2.0f);

        _decalShader->use();
        _decalShader->setMat4("view", cam.getViewMatrix());
        _decalShader->setMat4("projection", cam.getProjectionMatrix());
        _decalShader->setInt("decalTex", 0);

        glBindVertexArray(_vao);

        // Batch decals per DecalType
        for (int typeIdx = 0; typeIdx < 4; ++typeIdx) {
            DecalType curType = (DecalType)typeIdx;
            std::vector<Vertex> batchVerts;
            std::vector<unsigned int> batchIndices;

            for (const auto& d : _decals) {
                if (d.type != curType) continue;

                unsigned int baseIndex = (unsigned int)batchVerts.size();
                batchVerts.insert(batchVerts.end(), d.vertices.begin(), d.vertices.end());
                for (unsigned int idxVal : d.indices) {
                    batchIndices.push_back(baseIndex + idxVal);
                }
            }

            if (batchIndices.empty()) continue;

            Texture* tex = nullptr;
            switch (curType) {
                case DecalType::BulletHoleConcrete: tex = _texConcreteHole.get(); break;
                case DecalType::BulletHoleMetal:    tex = _texMetalHole.get(); break;
                case DecalType::BloodSplatter:      tex = _texBlood.get(); break;
                case DecalType::ExplosiveScorch:    tex = _texScorch.get(); break;
            }

            if (tex) tex->bind(0);

            glBindBuffer(GL_ARRAY_BUFFER, _vbo);
            glBufferData(GL_ARRAY_BUFFER, batchVerts.size() * sizeof(Vertex), batchVerts.data(), GL_DYNAMIC_DRAW);

            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, _ebo);
            glBufferData(GL_ELEMENT_ARRAY_BUFFER, batchIndices.size() * sizeof(unsigned int), batchIndices.data(), GL_DYNAMIC_DRAW);

            glDrawElements(GL_TRIANGLES, (GLsizei)batchIndices.size(), GL_UNSIGNED_INT, 0);
        }

        glBindVertexArray(0);

        glDisable(GL_POLYGON_OFFSET_FILL);
        glDepthMask(GL_TRUE);
        glDisable(GL_BLEND);
    }

    void DecalSystem::clear() {
        _decals.clear();
    }

} // namespace Lab
