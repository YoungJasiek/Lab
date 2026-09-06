#include "LabFace.h"
#include <glad/gl.h>
#include <cmath>
#include <algorithm>
#include <iostream>

namespace Lab {

    // =========================================================================
    // FacialMesh Implementation
    // =========================================================================

    FacialMesh::FacialMesh() = default;

    FacialMesh::~FacialMesh() {
        shutdown();
    }

    FacialMesh::FacialMesh(FacialMesh&& other) noexcept
        : _baseVertices(std::move(other._baseVertices)),
          _deformedVertices(std::move(other._deformedVertices)),
          _indices(std::move(other._indices)),
          _targets(std::move(other._targets)),
          _weights(std::move(other._weights)),
          _nameToIndex(std::move(other._nameToIndex)),
          _vao(other._vao), _vbo(other._vbo), _ebo(other._ebo),
          _dirty(other._dirty) {
        other._vao = other._vbo = other._ebo = 0;
    }

    FacialMesh& FacialMesh::operator=(FacialMesh&& other) noexcept {
        if (this != &other) {
            shutdown();
            _baseVertices = std::move(other._baseVertices);
            _deformedVertices = std::move(other._deformedVertices);
            _indices = std::move(other._indices);
            _targets = std::move(other._targets);
            _weights = std::move(other._weights);
            _nameToIndex = std::move(other._nameToIndex);
            _vao = other._vao;
            _vbo = other._vbo;
            _ebo = other._ebo;
            _dirty = other._dirty;

            other._vao = other._vbo = other._ebo = 0;
        }
        return *this;
    }

    void FacialMesh::init(const std::vector<Vertex>& baseVertices, const std::vector<unsigned int>& indices) {
        shutdown();
        _baseVertices = baseVertices;
        _deformedVertices = baseVertices;
        _indices = indices;
        _targets.clear();
        _weights.clear();
        _nameToIndex.clear();

        // Direct State Access (OpenGL 4.5+) VAO, VBO, EBO setup
        glCreateVertexArrays(1, &_vao);
        glCreateBuffers(1, &_vbo);
        glCreateBuffers(1, &_ebo);

        // Upload initial vertex and index data
        glNamedBufferData(_vbo, sizeof(Vertex) * _deformedVertices.size(), _deformedVertices.data(), GL_DYNAMIC_DRAW);
        glNamedBufferData(_ebo, sizeof(unsigned int) * _indices.size(), _indices.data(), GL_STATIC_DRAW);

        glVertexArrayElementBuffer(_vao, _ebo);
        glVertexArrayVertexBuffer(_vao, 0, _vbo, 0, sizeof(Vertex));

        // Attrib 0: Position
        glEnableVertexArrayAttrib(_vao, 0);
        glVertexArrayAttribFormat(_vao, 0, 3, GL_FLOAT, GL_FALSE, offsetof(Vertex, position));
        glVertexArrayAttribBinding(_vao, 0, 0);

        // Attrib 1: Normal
        glEnableVertexArrayAttrib(_vao, 1);
        glVertexArrayAttribFormat(_vao, 1, 3, GL_FLOAT, GL_FALSE, offsetof(Vertex, normal));
        glVertexArrayAttribBinding(_vao, 1, 0);

        // Attrib 2: TexCoords
        glEnableVertexArrayAttrib(_vao, 2);
        glVertexArrayAttribFormat(_vao, 2, 2, GL_FLOAT, GL_FALSE, offsetof(Vertex, texCoords));
        glVertexArrayAttribBinding(_vao, 2, 0);

        // Attrib 3: Color
        glEnableVertexArrayAttrib(_vao, 3);
        glVertexArrayAttribFormat(_vao, 3, 3, GL_FLOAT, GL_FALSE, offsetof(Vertex, color));
        glVertexArrayAttribBinding(_vao, 3, 0);

        _dirty = false;
    }

    void FacialMesh::shutdown() {
        if (_vao) {
            glDeleteVertexArrays(1, &_vao);
            _vao = 0;
        }
        if (_vbo) {
            glDeleteBuffers(1, &_vbo);
            _vbo = 0;
        }
        if (_ebo) {
            glDeleteBuffers(1, &_ebo);
            _ebo = 0;
        }
        _baseVertices.clear();
        _deformedVertices.clear();
        _indices.clear();
        _targets.clear();
        _weights.clear();
        _nameToIndex.clear();
    }

    int FacialMesh::addMorphTarget(const MorphTarget& target) {
        int idx = static_cast<int>(_targets.size());
        _targets.push_back(target);
        _weights.push_back(0.0f);
        _nameToIndex[target.name] = idx;
        return idx;
    }

    int FacialMesh::findMorphTarget(const std::string& name) const {
        auto it = _nameToIndex.find(name);
        return (it != _nameToIndex.end()) ? it->second : -1;
    }

    void FacialMesh::setWeight(const std::string& name, float weight) {
        int idx = findMorphTarget(name);
        if (idx >= 0) {
            setWeight(idx, weight);
        }
    }

    void FacialMesh::setWeight(int targetIndex, float weight) {
        if (targetIndex >= 0 && targetIndex < static_cast<int>(_weights.size())) {
            float clamped = std::clamp(weight, 0.0f, 1.0f);
            if (std::abs(_weights[targetIndex] - clamped) > 1e-4f) {
                _weights[targetIndex] = clamped;
                _dirty = true;
            }
        }
    }

    float FacialMesh::getWeight(const std::string& name) const {
        int idx = findMorphTarget(name);
        return (idx >= 0) ? getWeight(idx) : 0.0f;
    }

    float FacialMesh::getWeight(int targetIndex) const {
        if (targetIndex >= 0 && targetIndex < static_cast<int>(_weights.size())) {
            return _weights[targetIndex];
        }
        return 0.0f;
    }

    void FacialMesh::evaluate() {
        if (!_dirty) return;

        size_t vCount = _baseVertices.size();
        for (size_t i = 0; i < vCount; ++i) {
            Vec3 pos = _baseVertices[i].position;
            Vec3 norm = _baseVertices[i].normal;

            for (size_t t = 0; t < _targets.size(); ++t) {
                float w = _weights[t];
                if (w > 1e-4f && i < _targets[t].positionOffsets.size()) {
                    pos = pos + _targets[t].positionOffsets[i] * w;
                    if (i < _targets[t].normalOffsets.size()) {
                        norm = norm + _targets[t].normalOffsets[i] * w;
                    }
                }
            }

            _deformedVertices[i].position = pos;
            _deformedVertices[i].normal = norm.normalized();
        }

        updateGPU();
        _dirty = false;
    }

    void FacialMesh::updateGPU() {
        if (_vbo && !_deformedVertices.empty()) {
            glNamedBufferSubData(_vbo, 0, sizeof(Vertex) * _deformedVertices.size(), _deformedVertices.data());
        }
    }

    void FacialMesh::draw() const {
        if (_vao && !_indices.empty()) {
            glBindVertexArray(_vao);
            glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(_indices.size()), GL_UNSIGNED_INT, nullptr);
            glBindVertexArray(0);
        }
    }

    // =========================================================================
    // Procedural Combat Humanoid Head with Blend Shapes
    // =========================================================================

    std::unique_ptr<FacialMesh> FacialMesh::createProceduralHead() {
        auto mesh = std::make_unique<FacialMesh>();

        // Generate a 3D faceted head mesh with realistic facial landmark regions
        const int latRings = 16;
        const int lonSegments = 24;
        const float radiusX = 0.22f; // Width of head
        const float radiusY = 0.28f; // Height of head (chin to skull crown)
        const float radiusZ = 0.24f; // Depth of head

        std::vector<Vertex> baseVertices;
        std::vector<unsigned int> indices;

        for (int r = 0; r <= latRings; ++r) {
            float v = static_cast<float>(r) / static_cast<float>(latRings);
            float phi = v * 3.14159265f; // [0, PI]

            for (int s = 0; s <= lonSegments; ++s) {
                float u = static_cast<float>(s) / static_cast<float>(lonSegments);
                float theta = u * 2.0f * 3.14159265f; // [0, 2PI]

                // Ellipsoid base coordinate
                float sx = std::sin(phi) * std::sin(theta);
                float sy = std::cos(phi);
                float sz = std::sin(phi) * std::cos(theta);

                // Sculpt face features: flatten back of head, taper chin, shape nose & mouth
                float yVal = sy * radiusY;
                float xVal = sx * radiusX;
                float zVal = sz * radiusZ;

                // Jaw tapering (lower hemisphere and front facing)
                if (sy < -0.1f && sz > 0.0f) {
                    float jawFactor = (-sy - 0.1f) / 0.9f;
                    xVal *= (1.0f - jawFactor * 0.28f); // Narrow jaw toward chin
                    zVal += jawFactor * 0.035f;        // Chin protrusion
                }

                // Nose bridge & tip protrusion (middle face, front facing)
                if (sy > -0.15f && sy < 0.15f && sz > 0.6f) {
                    float noseFactor = (1.0f - std::abs(sy) / 0.15f) * (1.0f - std::min(1.0f, std::abs(sx) / 0.25f));
                    zVal += noseFactor * 0.055f;
                }

                Vec3 pos(xVal, yVal + 0.15f, zVal);
                Vec3 norm(sx, sy, sz);
                norm = norm.normalized();

                Vec3 col(0.85f, 0.72f, 0.65f); // Base skin tone
                // Subtle lips coloration (lower front region)
                if (sy >= -0.45f && sy <= -0.15f && sz > 0.75f && std::abs(sx) < 0.35f) {
                    col = Vec3(0.82f, 0.45f, 0.45f); // Natural lip tone
                }

                Vertex vert;
                vert.position = pos;
                vert.normal = norm;
                vert.texCoords = Vec2(u, v);
                vert.color = col;

                baseVertices.push_back(vert);
            }
        }

        // Indices
        for (int r = 0; r < latRings; ++r) {
            for (int s = 0; s < lonSegments; ++s) {
                int first = r * (lonSegments + 1) + s;
                int second = first + lonSegments + 1;

                indices.push_back(first);
                indices.push_back(second);
                indices.push_back(first + 1);

                indices.push_back(second);
                indices.push_back(second + 1);
                indices.push_back(first + 1);
            }
        }

        mesh->init(baseVertices, indices);

        // --- Build Morph Targets ---
        size_t totalVerts = baseVertices.size();
        MorphTarget jawOpen("Jaw_Open", totalVerts);
        MorphTarget mouthNarrow("Mouth_Narrow", totalVerts);
        MorphTarget mouthSmile("Mouth_Smile", totalVerts);

        for (size_t i = 0; i < totalVerts; ++i) {
            const auto& v = baseVertices[i];
            float yNorm = (v.position.y - 0.15f) / radiusY;
            float zNorm = v.position.z / radiusZ;
            float xNorm = v.position.x / radiusX;

            // Target 1: "Jaw_Open" -> Lower face / chin drops down and rotates back
            if (yNorm < -0.15f && zNorm > -0.1f) {
                float intensity = (-yNorm - 0.15f) / 0.85f;
                intensity = std::clamp(intensity, 0.0f, 1.0f);
                intensity = intensity * intensity; // Smooth quadratic falloff

                jawOpen.positionOffsets[i] = Vec3(0.0f, -0.095f * intensity, -0.025f * intensity);
                jawOpen.normalOffsets[i] = Vec3(0.0f, -0.3f * intensity, 0.1f * intensity);
            }

            // Target 2: "Mouth_Narrow" -> Lips compress laterally and push forward (O / U viseme)
            if (yNorm >= -0.50f && yNorm <= -0.10f && zNorm > 0.65f) {
                float mouthInfluence = (1.0f - std::abs(yNorm - (-0.30f)) / 0.20f) * (1.0f - std::min(1.0f, std::abs(xNorm) / 0.65f));
                mouthInfluence = std::clamp(mouthInfluence, 0.0f, 1.0f);

                mouthNarrow.positionOffsets[i] = Vec3(-xNorm * 0.038f * mouthInfluence, 0.0f, 0.045f * mouthInfluence);
                mouthNarrow.normalOffsets[i] = Vec3(-xNorm * 0.2f * mouthInfluence, 0.0f, 0.3f * mouthInfluence);
            }

            // Target 3: "Mouth_Smile" -> Lip corners raise upward and widen (Smile / grimace)
            if (yNorm >= -0.45f && yNorm <= -0.12f && zNorm > 0.60f && std::abs(xNorm) > 0.10f) {
                float cornerInfluence = (1.0f - std::abs(yNorm - (-0.28f)) / 0.17f) * std::clamp((std::abs(xNorm) - 0.10f) / 0.40f, 0.0f, 1.0f);
                cornerInfluence = std::clamp(cornerInfluence, 0.0f, 1.0f);

                float sideSign = (xNorm > 0.0f) ? 1.0f : -1.0f;
                mouthSmile.positionOffsets[i] = Vec3(sideSign * 0.032f * cornerInfluence, 0.035f * cornerInfluence, 0.010f * cornerInfluence);
                mouthSmile.normalOffsets[i] = Vec3(sideSign * 0.15f * cornerInfluence, 0.25f * cornerInfluence, 0.0f);
            }
        }

        mesh->addMorphTarget(jawOpen);
        mesh->addMorphTarget(mouthNarrow);
        mesh->addMorphTarget(mouthSmile);

        return mesh;
    }

    // =========================================================================
    // LipSyncEvaluator Implementation
    // =========================================================================

    LipSyncEvaluator::LipSyncEvaluator(float attack, float decay, float sensitivity)
        : _attackRate(attack), _decayRate(decay), _sensitivity(sensitivity) {
    }

    void LipSyncEvaluator::update(float audioAmplitude, float dt) {
        float rawTarget = std::clamp(audioAmplitude, 0.0f, 1.0f);

        // Envelope follower with asymmetrical attack / decay smoothing
        if (rawTarget > _smoothedEnergy) {
            _smoothedEnergy += (rawTarget - _smoothedEnergy) * std::min(1.0f, _attackRate * dt);
        } else {
            _smoothedEnergy += (rawTarget - _smoothedEnergy) * std::min(1.0f, _decayRate * dt);
        }
        _smoothedEnergy = std::clamp(_smoothedEnergy, 0.0f, 1.0f);

        // Dynamic viseme weight synthesis:
        // Jaw Open corresponds to bulk phonetic acoustic energy (open vowels: A, E, O)
        _jawOpen = std::clamp(_smoothedEnergy * _sensitivity, 0.0f, 1.0f);

        // Mouth narrowness / pucker oscillates across vowel transition harmonics
        float formantOsc = std::sin(_smoothedEnergy * 3.14159f * 2.2f);
        _mouthNarrow = std::clamp(formantOsc * 0.55f * _smoothedEnergy, 0.0f, 0.75f);

        // Smile / stress inflection for energetic speech accents
        _mouthSmile = std::clamp((_smoothedEnergy - 0.45f) * 0.75f, 0.0f, 0.60f);
    }

    void LipSyncEvaluator::processPCM(const int16_t* samples, size_t sampleCount, float dt) {
        if (!samples || sampleCount == 0) {
            update(0.0f, dt);
            return;
        }

        // Calculate Root-Mean-Square (RMS) power of PCM chunk
        double sumSq = 0.0;
        for (size_t i = 0; i < sampleCount; ++i) {
            double norm = static_cast<double>(samples[i]) / 32768.0;
            sumSq += norm * norm;
        }
        double rms = std::sqrt(sumSq / static_cast<double>(sampleCount));
        update(static_cast<float>(rms), dt);
    }

    void LipSyncEvaluator::applyTo(FacialMesh& mesh) const {
        mesh.setWeight("Jaw_Open", _jawOpen);
        mesh.setWeight("Mouth_Narrow", _mouthNarrow);
        mesh.setWeight("Mouth_Smile", _mouthSmile);
        mesh.evaluate();
    }

    std::vector<float> LipSyncEvaluator::generateSpeechTrack(float durationSeconds, float syllablesPerSecond, unsigned int seed) {
        int sampleRate = 60; // 60 FPS animation sample points
        int totalFrames = static_cast<int>(std::max(1.0f, durationSeconds * sampleRate));
        std::vector<float> track(totalFrames, 0.0f);

        unsigned int lcg = seed;
        auto nextRand = [&lcg]() -> float {
            lcg = lcg * 1664525u + 1013904223u;
            return static_cast<float>(lcg & 0xFFFF) / 65535.0f;
        };

        float syllablePeriod = 1.0f / std::max(1.0f, syllablesPerSecond);
        for (int i = 0; i < totalFrames; ++i) {
            float t = static_cast<float>(i) / static_cast<float>(sampleRate);

            // Syllable rhythmic pulse
            float syllablePhase = std::fmod(t, syllablePeriod) / syllablePeriod;
            float pulse = std::sin(syllablePhase * 3.14159265f);
            pulse = std::max(0.0f, pulse);

            // Natural word break pauses every ~0.8 to 1.4 seconds
            float wordCycle = std::fmod(t, 1.2f);
            float wordPause = (wordCycle > 0.95f) ? 0.05f : 1.0f;

            // Micro-variations in syllable volume
            float volumeVariation = 0.75f + 0.25f * std::sin(t * 7.5f + nextRand() * 0.1f);

            track[i] = std::clamp(pulse * wordPause * volumeVariation, 0.0f, 1.0f);
        }

        return track;
    }

} // namespace Lab
