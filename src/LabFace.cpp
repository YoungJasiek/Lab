#include "LabFace.h"
#include <glad/gl.h>
#include <cmath>
#include <algorithm>
#include <iostream>
#include <fstream>
#include <cstring>

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
        MorphTarget browRaise("Brow_Raise", totalVerts);
        MorphTarget eyesSquint("Eyes_Squint", totalVerts);
        MorphTarget mouthFrown("Mouth_Frown", totalVerts);

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

            // Target 4: "Brow_Raise" -> Eyebrow / forehead elevation for surprise or alertness
            if (yNorm > 0.20f && yNorm < 0.75f && zNorm > 0.45f) {
                float browInfluence = (1.0f - std::abs(yNorm - 0.45f) / 0.30f) * std::clamp(zNorm, 0.0f, 1.0f);
                browInfluence = std::clamp(browInfluence, 0.0f, 1.0f);
                browRaise.positionOffsets[i] = Vec3(0.0f, 0.045f * browInfluence, 0.015f * browInfluence);
                browRaise.normalOffsets[i] = Vec3(0.0f, 0.2f * browInfluence, 0.1f * browInfluence);
            }

            // Target 5: "Eyes_Squint" -> Eye corner tightening and eyelid tension
            if (yNorm > 0.05f && yNorm < 0.40f && zNorm > 0.55f && std::abs(xNorm) > 0.15f) {
                float eyeInfluence = (1.0f - std::abs(yNorm - 0.22f) / 0.18f) * std::clamp(zNorm, 0.0f, 1.0f);
                eyeInfluence = std::clamp(eyeInfluence, 0.0f, 1.0f);
                eyesSquint.positionOffsets[i] = Vec3(-xNorm * 0.02f * eyeInfluence, -0.025f * eyeInfluence, 0.01f * eyeInfluence);
                eyesSquint.normalOffsets[i] = Vec3(0.0f, -0.2f * eyeInfluence, 0.1f * eyeInfluence);
            }

            // Target 6: "Mouth_Frown" -> Mouth corners dragged downward for combat stress / grimace
            if (yNorm >= -0.45f && yNorm <= -0.12f && zNorm > 0.60f && std::abs(xNorm) > 0.10f) {
                float cornerInfluence = (1.0f - std::abs(yNorm - (-0.28f)) / 0.17f) * std::clamp((std::abs(xNorm) - 0.10f) / 0.40f, 0.0f, 1.0f);
                cornerInfluence = std::clamp(cornerInfluence, 0.0f, 1.0f);
                mouthFrown.positionOffsets[i] = Vec3(0.0f, -0.035f * cornerInfluence, 0.0f);
                mouthFrown.normalOffsets[i] = Vec3(0.0f, -0.25f * cornerInfluence, 0.0f);
            }
        }

        mesh->addMorphTarget(jawOpen);
        mesh->addMorphTarget(mouthNarrow);
        mesh->addMorphTarget(mouthSmile);
        mesh->addMorphTarget(browRaise);
        mesh->addMorphTarget(eyesSquint);
        mesh->addMorphTarget(mouthFrown);

        return mesh;
    }

    // =========================================================================
    // DialogAudioClip WAV Loader / Writer
    // =========================================================================

    bool DialogAudioClip::loadWavFile(const std::string& path) {
        std::ifstream f(path, std::ios::binary);
        if (!f.is_open()) return false;

        char riffHeader[12] = {};
        if (!f.read(riffHeader, 12)) return false;
        if (std::memcmp(riffHeader, "RIFF", 4) != 0 || std::memcmp(riffHeader + 8, "WAVE", 4) != 0) {
            return false;
        }

        uint16_t audioFormat = 0;
        uint16_t numChannels = 0;
        uint32_t rate = 0;
        uint16_t bitsPerSample = 0;
        std::vector<uint8_t> rawData;

        while (f && f.peek() != EOF) {
            char chunkId[4] = {};
            uint32_t chunkSize = 0;
            if (!f.read(chunkId, 4)) break;
            if (!f.read(reinterpret_cast<char*>(&chunkSize), sizeof(uint32_t))) break;

            if (std::memcmp(chunkId, "fmt ", 4) == 0) {
                if (chunkSize < 16) return false;
                f.read(reinterpret_cast<char*>(&audioFormat), sizeof(uint16_t));
                f.read(reinterpret_cast<char*>(&numChannels), sizeof(uint16_t));
                f.read(reinterpret_cast<char*>(&rate), sizeof(uint32_t));
                uint32_t byteRate = 0;
                uint16_t blockAlign = 0;
                f.read(reinterpret_cast<char*>(&byteRate), sizeof(uint32_t));
                f.read(reinterpret_cast<char*>(&blockAlign), sizeof(uint16_t));
                f.read(reinterpret_cast<char*>(&bitsPerSample), sizeof(uint16_t));
                if (chunkSize > 16) {
                    f.seekg(chunkSize - 16, std::ios::cur);
                }
            } else if (std::memcmp(chunkId, "data", 4) == 0) {
                rawData.resize(chunkSize);
                if (chunkSize > 0) {
                    f.read(reinterpret_cast<char*>(rawData.data()), chunkSize);
                }
                break;
            } else {
                f.seekg(chunkSize, std::ios::cur);
            }
            if (chunkSize & 1u) {
                f.seekg(1, std::ios::cur);
            }
        }

        if (rawData.empty() || numChannels == 0 || rate == 0 || (audioFormat != 1 && audioFormat != 3)) {
            return false;
        }

        size_t bytesPerSample = bitsPerSample / 8;
        if (bytesPerSample == 0) return false;
        size_t frameSize = bytesPerSample * numChannels;
        size_t frameCount = rawData.size() / frameSize;
        if (frameCount == 0) return false;

        samples.resize(frameCount);
        for (size_t i = 0; i < frameCount; ++i) {
            int32_t accum = 0;
            for (uint16_t ch = 0; ch < numChannels; ++ch) {
                const uint8_t* ptr = rawData.data() + i * frameSize + ch * bytesPerSample;
                int16_t sample16 = 0;
                if (audioFormat == 1 && bitsPerSample == 16) {
                    std::memcpy(&sample16, ptr, sizeof(int16_t));
                } else if (audioFormat == 1 && bitsPerSample == 8) {
                    sample16 = static_cast<int16_t>((static_cast<int>(ptr[0]) - 128) * 256);
                } else if (audioFormat == 1 && bitsPerSample == 24) {
                    int32_t s24 = (static_cast<int32_t>(static_cast<int8_t>(ptr[2])) << 16)
                                | (static_cast<int32_t>(ptr[1]) << 8)
                                | static_cast<int32_t>(ptr[0]);
                    sample16 = static_cast<int16_t>(s24 >> 8);
                } else if (audioFormat == 3 && bitsPerSample == 32) {
                    float flt = 0.0f;
                    std::memcpy(&flt, ptr, sizeof(float));
                    sample16 = static_cast<int16_t>(std::clamp(flt, -1.0f, 1.0f) * 32767.0f);
                }
                accum += sample16;
            }
            samples[i] = static_cast<int16_t>(accum / static_cast<int32_t>(numChannels));
        }

        filepath = path;
        sampleRate = rate;
        durationSeconds = static_cast<float>(frameCount) / static_cast<float>(rate);
        return true;
    }

    bool DialogAudioClip::writeWavFile(const std::string& path, const std::vector<int16_t>& monoSamples, uint32_t rate) {
        if (monoSamples.empty() || rate == 0) return false;
        std::ofstream f(path, std::ios::binary);
        if (!f.is_open()) return false;

        uint32_t dataSize = static_cast<uint32_t>(monoSamples.size() * sizeof(int16_t));
        uint32_t riffSize = 36u + dataSize;
        uint32_t fmtChunkSize = 16u;
        uint16_t audioFormat = 1u; // PCM
        uint16_t numChannels = 1u;
        uint32_t byteRate = rate * sizeof(int16_t);
        uint16_t blockAlign = sizeof(int16_t);
        uint16_t bitsPerSample = 16u;

        f.write("RIFF", 4);
        f.write(reinterpret_cast<const char*>(&riffSize), sizeof(uint32_t));
        f.write("WAVE", 4);
        f.write("fmt ", 4);
        f.write(reinterpret_cast<const char*>(&fmtChunkSize), sizeof(uint32_t));
        f.write(reinterpret_cast<const char*>(&audioFormat), sizeof(uint16_t));
        f.write(reinterpret_cast<const char*>(&numChannels), sizeof(uint16_t));
        f.write(reinterpret_cast<const char*>(&rate), sizeof(uint32_t));
        f.write(reinterpret_cast<const char*>(&byteRate), sizeof(uint32_t));
        f.write(reinterpret_cast<const char*>(&blockAlign), sizeof(uint16_t));
        f.write(reinterpret_cast<const char*>(&bitsPerSample), sizeof(uint16_t));
        f.write("data", 4);
        f.write(reinterpret_cast<const char*>(&dataSize), sizeof(uint32_t));
        f.write(reinterpret_cast<const char*>(monoSamples.data()), dataSize);
        return f.good();
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

        // Calculate Root-Mean-Square (RMS) power and Zero-Crossing Rate (ZCR) for phoneme classification
        double sumSq = 0.0;
        size_t zeroCrossings = 0;
        for (size_t i = 0; i < sampleCount; ++i) {
            double norm = static_cast<double>(samples[i]) / 32768.0;
            sumSq += norm * norm;
            if (i > 0 && ((samples[i - 1] >= 0 && samples[i] < 0) || (samples[i - 1] < 0 && samples[i] >= 0))) {
                ++zeroCrossings;
            }
        }
        double rms = std::sqrt(sumSq / static_cast<double>(sampleCount));
        float zcr = static_cast<float>(zeroCrossings) / static_cast<float>(sampleCount);

        update(static_cast<float>(rms), dt);

        // Refine visemes using spectral ZCR:
        // Low ZCR + high RMS -> rounded vowels (O / U -> Mouth_Narrow)
        // High ZCR -> dental/sibilant consonants (S / T / I -> Mouth_Smile spread, smaller Jaw_Open)
        if (_smoothedEnergy > 0.01f) {
            float vowelRoundness = std::clamp((0.18f - zcr) * 4.5f, 0.0f, 1.0f);
            float sibilance = std::clamp((zcr - 0.18f) * 3.5f, 0.0f, 1.0f);
            _mouthNarrow = std::clamp(_mouthNarrow * 0.5f + vowelRoundness * _jawOpen * 0.65f, 0.0f, 0.85f);
            _mouthSmile = std::clamp(_mouthSmile * 0.5f + sibilance * 0.65f, 0.0f, 0.75f);
        }
    }

    bool LipSyncEvaluator::loadDialogAudio(const std::string& wavFilepath, const std::string& speaker, const std::string& subtitleText) {
        DialogAudioClip loaded;
        if (!loaded.loadWavFile(wavFilepath)) {
            return false;
        }
        _clip = std::move(loaded);
        _playbackTime = 0.0f;
        _playing = false;
        if (!subtitleText.empty() || !speaker.empty()) {
            _clip.subtitles.push_back({ 0.0f, _clip.durationSeconds, speaker, subtitleText });
        }
        return true;
    }

    void LipSyncEvaluator::addSubtitle(float startTime, float endTime, const std::string& speaker, const std::string& text) {
        _clip.subtitles.push_back({ startTime, endTime, speaker, text });
    }

    void LipSyncEvaluator::clearSubtitles() {
        _clip.subtitles.clear();
    }

    void LipSyncEvaluator::playDialog(bool loop) {
        if (!_clip.isValid()) return;
        _playbackTime = 0.0f;
        _playing = true;
        _looping = loop;
    }

    void LipSyncEvaluator::stopDialog() {
        _playing = false;
        _playbackTime = 0.0f;
        update(0.0f, 1.0f);
    }

    void LipSyncEvaluator::seekDialog(float timeSeconds) {
        if (!_clip.isValid()) return;
        _playbackTime = std::clamp(timeSeconds, 0.0f, _clip.durationSeconds);
        evaluateAtTime(_playbackTime, 1.0f / 60.0f);
    }

    void LipSyncEvaluator::evaluateAtTime(float timeSeconds, float dt) {
        if (!_clip.isValid()) {
            update(0.0f, dt);
            return;
        }
        float clampedTime = std::clamp(timeSeconds, 0.0f, _clip.durationSeconds);
        size_t centerSample = static_cast<size_t>(clampedTime * static_cast<float>(_clip.sampleRate));
        size_t windowSamples = std::max<size_t>(64, _clip.sampleRate / 40); // 25 ms acoustic window
        if (centerSample >= _clip.samples.size()) {
            update(0.0f, dt);
            return;
        }
        size_t endSample = std::min(_clip.samples.size(), centerSample + windowSamples);
        processPCM(_clip.samples.data() + centerSample, endSample - centerSample, dt);
    }

    void LipSyncEvaluator::updateDialog(float dt) {
        if (!_playing || !_clip.isValid()) {
            update(0.0f, dt);
            return;
        }
        evaluateAtTime(_playbackTime, dt);
        _playbackTime += dt;
        if (_playbackTime >= _clip.durationSeconds) {
            if (_looping && _clip.durationSeconds > 0.0f) {
                _playbackTime = std::fmod(_playbackTime, _clip.durationSeconds);
            } else {
                _playbackTime = _clip.durationSeconds;
                _playing = false;
            }
        }
    }

    std::string LipSyncEvaluator::getActiveSpeaker() const {
        for (const auto& sub : _clip.subtitles) {
            if (_playbackTime >= sub.startTime && _playbackTime <= sub.endTime) {
                return sub.speaker;
            }
        }
        return "";
    }

    std::string LipSyncEvaluator::getActiveSubtitle() const {
        for (const auto& sub : _clip.subtitles) {
            if (_playbackTime >= sub.startTime && _playbackTime <= sub.endTime) {
                return sub.text;
            }
        }
        return "";
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
