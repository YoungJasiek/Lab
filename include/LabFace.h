#pragma once
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <cstdint>
#include "LabMath.h"
#include "LabRenderer.h"

namespace Lab {

    // --- Morph Target (Blend Shape Vertex Displacements) ---
    struct MorphTarget {
        std::string name;
        std::vector<Vec3> positionOffsets;
        std::vector<Vec3> normalOffsets;

        MorphTarget() = default;
        MorphTarget(const std::string& targetName, size_t vertexCount)
            : name(targetName), positionOffsets(vertexCount, Vec3(0, 0, 0)), normalOffsets(vertexCount, Vec3(0, 0, 0)) {}
    };

    // --- Deformable Facial Mesh with Blend Shapes ---
    class FacialMesh {
    public:
        FacialMesh();
        ~FacialMesh();

        // Non-copyable, movable (RAII)
        FacialMesh(const FacialMesh&) = delete;
        FacialMesh& operator=(const FacialMesh&) = delete;
        FacialMesh(FacialMesh&& other) noexcept;
        FacialMesh& operator=(FacialMesh&& other) noexcept;

        // Initialize with base geometry
        void init(const std::vector<Vertex>& baseVertices, const std::vector<unsigned int>& indices);
        void shutdown();

        // Target management
        int addMorphTarget(const MorphTarget& target);
        int findMorphTarget(const std::string& name) const;
        void setWeight(const std::string& name, float weight);
        void setWeight(int targetIndex, float weight);
        float getWeight(const std::string& name) const;
        float getWeight(int targetIndex) const;
        size_t getTargetCount() const { return _targets.size(); }

        // Blend shape evaluation & GPU VBO update
        void evaluate();
        void draw() const;

        size_t getVertexCount() const { return _baseVertices.size(); }
        size_t getIndexCount() const { return _indices.size(); }
        const std::vector<Vertex>& getDeformedVertices() const { return _deformedVertices; }

        // Procedural generator: creates a detailed 3D humanoid combat head with expressive facial landmarks
        // (Jaw, Lips, Brow, Cheeks) and predefined Blend Shapes ("Jaw_Open", "Mouth_Narrow", "Mouth_Smile")
        static std::unique_ptr<FacialMesh> createProceduralHead();

    private:
        std::vector<Vertex> _baseVertices;
        std::vector<Vertex> _deformedVertices;
        std::vector<unsigned int> _indices;

        std::vector<MorphTarget> _targets;
        std::vector<float> _weights;
        std::unordered_map<std::string, int> _nameToIndex;

        unsigned int _vao = 0;
        unsigned int _vbo = 0;
        unsigned int _ebo = 0;
        bool _dirty = false;

        void updateGPU();
    };

    // --- Real-time Audio Phoneme & Lip-Sync Evaluator ---
    class LipSyncEvaluator {
    public:
        LipSyncEvaluator(float attack = 24.0f, float decay = 14.0f, float sensitivity = 2.4f);

        // Update state based on raw audio amplitude in [0.0, 1.0]
        void update(float audioAmplitude, float dt);

        // Process 16-bit PCM audio buffer to compute RMS energy and drive lip sync
        void processPCM(const int16_t* samples, size_t sampleCount, float dt);

        // Drive a FacialMesh directly with evaluated weights
        void applyTo(FacialMesh& mesh) const;

        // Accessors
        float getJawOpen() const { return _jawOpen; }
        float getMouthNarrow() const { return _mouthNarrow; }
        float getMouthSmile() const { return _mouthSmile; }
        float getSmoothedEnergy() const { return _smoothedEnergy; }

        // Procedural dialog track generator: returns amplitude series simulating spoken radio commands
        // ("Dr. Vance: Access Granted", "Warning: Sector Compromised", etc.)
        static std::vector<float> generateSpeechTrack(float durationSeconds, float syllablesPerSecond = 4.2f, unsigned int seed = 1337);

    private:
        float _attackRate;
        float _decayRate;
        float _sensitivity;

        float _smoothedEnergy = 0.0f;
        float _jawOpen = 0.0f;
        float _mouthNarrow = 0.0f;
        float _mouthSmile = 0.0f;
    };

} // namespace Lab
