#include "LabSkeletal.h"
#include <glad/gl.h>
#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>
#include <cmath>

namespace Lab {

    // --- SkinnedMesh Implementation ---

    SkinnedMesh::SkinnedMesh(const std::vector<SkinnedVertex>& vertices, const std::vector<unsigned int>& indices)
        : _indexCount(static_cast<int>(indices.size())) {
        if (!vertices.empty()) {
            _minBounds = vertices[0].position;
            _maxBounds = vertices[0].position;
            for (const auto& v : vertices) {
                _minBounds.x = std::min(_minBounds.x, v.position.x);
                _minBounds.y = std::min(_minBounds.y, v.position.y);
                _minBounds.z = std::min(_minBounds.z, v.position.z);
                _maxBounds.x = std::max(_maxBounds.x, v.position.x);
                _maxBounds.y = std::max(_maxBounds.y, v.position.y);
                _maxBounds.z = std::max(_maxBounds.z, v.position.z);
            }
        }

        glGenVertexArrays(1, &_vao);
        glGenBuffers(1, &_vbo);
        glGenBuffers(1, &_ebo);

        glBindVertexArray(_vao);

        glBindBuffer(GL_ARRAY_BUFFER, _vbo);
        glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(SkinnedVertex), vertices.data(), GL_STATIC_DRAW);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, _ebo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);

        // Location 0: vec3 position
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(SkinnedVertex), reinterpret_cast<void*>(offsetof(SkinnedVertex, position)));

        // Location 1: vec3 normal
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(SkinnedVertex), reinterpret_cast<void*>(offsetof(SkinnedVertex, normal)));

        // Location 2: vec2 texCoords
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(SkinnedVertex), reinterpret_cast<void*>(offsetof(SkinnedVertex, texCoords)));

        // Location 3: vec3 color
        glEnableVertexAttribArray(3);
        glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, sizeof(SkinnedVertex), reinterpret_cast<void*>(offsetof(SkinnedVertex, color)));

        // Location 4: uvec4 boneIDs (must use glVertexAttribIPointer for integer types)
        glEnableVertexAttribArray(4);
        glVertexAttribIPointer(4, 4, GL_UNSIGNED_INT, sizeof(SkinnedVertex), reinterpret_cast<void*>(offsetof(SkinnedVertex, boneIDs)));

        // Location 5: vec4 boneWeights
        glEnableVertexAttribArray(5);
        glVertexAttribPointer(5, 4, GL_FLOAT, GL_FALSE, sizeof(SkinnedVertex), reinterpret_cast<void*>(offsetof(SkinnedVertex, boneWeights)));

        glBindVertexArray(0);
    }

    SkinnedMesh::~SkinnedMesh() {
        if (_ebo) glDeleteBuffers(1, &_ebo);
        if (_vbo) glDeleteBuffers(1, &_vbo);
        if (_vao) glDeleteVertexArrays(1, &_vao);
    }

    SkinnedMesh::SkinnedMesh(SkinnedMesh&& other) noexcept
        : _vao(other._vao), _vbo(other._vbo), _ebo(other._ebo), _indexCount(other._indexCount),
          _minBounds(other._minBounds), _maxBounds(other._maxBounds) {
        other._vao = 0;
        other._vbo = 0;
        other._ebo = 0;
        other._indexCount = 0;
    }

    SkinnedMesh& SkinnedMesh::operator=(SkinnedMesh&& other) noexcept {
        if (this != &other) {
            if (_ebo) glDeleteBuffers(1, &_ebo);
            if (_vbo) glDeleteBuffers(1, &_vbo);
            if (_vao) glDeleteVertexArrays(1, &_vao);

            _vao = other._vao;
            _vbo = other._vbo;
            _ebo = other._ebo;
            _indexCount = other._indexCount;
            _minBounds = other._minBounds;
            _maxBounds = other._maxBounds;

            other._vao = 0;
            other._vbo = 0;
            other._ebo = 0;
            other._indexCount = 0;
        }
        return *this;
    }

    void SkinnedMesh::draw() const {
        if (_vao && _indexCount > 0) {
            glBindVertexArray(_vao);
            glDrawElements(GL_TRIANGLES, _indexCount, GL_UNSIGNED_INT, nullptr);
            glBindVertexArray(0);
        }
    }

    // --- Skeleton Implementation ---

    int Skeleton::addBone(const std::string& name, int parentIndex,
                          const Vec3& localPos, const Quat& localRot, const Vec3& localScale) {
        int idx = static_cast<int>(_bones.size());
        Bone b;
        b.name = name;
        b.index = idx;
        b.parentIndex = parentIndex;
        b.bindPos = localPos;
        b.bindRot = localRot;
        b.bindScale = localScale;
        b.localBindMatrix = makeTransform(localPos, localRot, localScale);

        if (parentIndex >= 0 && parentIndex < idx) {
            _bones[parentIndex].children.push_back(idx);
        }

        _bones.push_back(b);
        _nameToIndex[name] = idx;
        return idx;
    }

    int Skeleton::findBoneIndex(const std::string& name) const {
        auto it = _nameToIndex.find(name);
        return (it != _nameToIndex.end()) ? it->second : -1;
    }

    const Bone* Skeleton::getBone(int index) const {
        if (index >= 0 && index < static_cast<int>(_bones.size())) {
            return &_bones[index];
        }
        return nullptr;
    }

    const Bone* Skeleton::getBone(const std::string& name) const {
        int idx = findBoneIndex(name);
        return getBone(idx);
    }

    void Skeleton::computeBindPose() {
        std::vector<Mat4> globalBindMatrices(_bones.size(), Mat4::identity());
        for (size_t i = 0; i < _bones.size(); ++i) {
            Bone& b = _bones[i];
            b.localBindMatrix = makeTransform(b.bindPos, b.bindRot, b.bindScale);
            if (b.parentIndex >= 0 && b.parentIndex < static_cast<int>(i)) {
                globalBindMatrices[i] = globalBindMatrices[b.parentIndex] * b.localBindMatrix;
            } else {
                globalBindMatrices[i] = b.localBindMatrix;
            }
            b.inverseBindMatrix = globalBindMatrices[i].inverse();
        }
    }

    void Skeleton::setInverseBindMatrix(int boneIndex, const Mat4& invBind) {
        if (boneIndex >= 0 && boneIndex < static_cast<int>(_bones.size())) {
            _bones[boneIndex].inverseBindMatrix = invBind;
        }
    }

    void Skeleton::evaluate(const std::vector<Mat4>& localTransforms,
                            std::vector<Mat4>& outGlobalTransforms,
                            std::vector<Mat4>& outSkinMatrices) const {
        size_t count = _bones.size();
        outGlobalTransforms.resize(count);
        outSkinMatrices.resize(count);

        for (size_t i = 0; i < count; ++i) {
            const Bone& b = _bones[i];
            Mat4 localM = (i < localTransforms.size()) ? localTransforms[i] : b.localBindMatrix;

            if (b.parentIndex >= 0 && b.parentIndex < static_cast<int>(i)) {
                outGlobalTransforms[i] = outGlobalTransforms[b.parentIndex] * localM;
            } else {
                outGlobalTransforms[i] = localM;
            }

            outSkinMatrices[i] = outGlobalTransforms[i] * b.inverseBindMatrix;
        }
    }

    Mat4 Skeleton::getSocketTransform(int boneIndex,
                                      const Mat4& modelTransform,
                                      const std::vector<Mat4>& globalTransforms,
                                      const Mat4& socketOffset) const {
        if (boneIndex >= 0 && boneIndex < static_cast<int>(globalTransforms.size())) {
            return modelTransform * globalTransforms[boneIndex] * socketOffset;
        }
        return modelTransform * socketOffset;
    }

    Mat4 Skeleton::getSocketTransform(const std::string& boneName,
                                      const Mat4& modelTransform,
                                      const std::vector<Mat4>& globalTransforms,
                                      const Mat4& socketOffset) const {
        int idx = findBoneIndex(boneName);
        if (idx < 0 && (boneName == "Socket_Weapon" || boneName == "WeaponSocket" || boneName == "RightHand")) {
            for (size_t b = 0; b < _bones.size(); ++b) {
                const std::string& n = _bones[b].name;
                if (n.find("RightHand") != std::string::npos ||
                    n.find("RightForeArm") != std::string::npos ||
                    n.find("Hand_R") != std::string::npos ||
                    n.find("Gun") != std::string::npos) {
                    idx = static_cast<int>(b);
                    break;
                }
            }
        }
        return getSocketTransform(idx, modelTransform, globalTransforms, socketOffset);
    }

    // --- BoneAnimationTrack Sampling ---

    Vec3 BoneAnimationTrack::sampleTranslation(float time) const {
        if (translationKeys.empty()) return { 0.0f, 0.0f, 0.0f };
        if (translationKeys.size() == 1 || time <= translationKeys.front().time) {
            return translationKeys.front().value;
        }
        if (time >= translationKeys.back().time) {
            return translationKeys.back().value;
        }

        for (size_t i = 0; i + 1 < translationKeys.size(); ++i) {
            if (time >= translationKeys[i].time && time <= translationKeys[i + 1].time) {
                float dt = translationKeys[i + 1].time - translationKeys[i].time;
                float alpha = (dt > 1e-5f) ? (time - translationKeys[i].time) / dt : 0.0f;
                return Vec3::lerp(translationKeys[i].value, translationKeys[i + 1].value, alpha);
            }
        }
        return translationKeys.back().value;
    }

    Quat BoneAnimationTrack::sampleRotation(float time) const {
        if (rotationKeys.empty()) return Quat::identity();
        if (rotationKeys.size() == 1 || time <= rotationKeys.front().time) {
            return rotationKeys.front().value;
        }
        if (time >= rotationKeys.back().time) {
            return rotationKeys.back().value;
        }

        for (size_t i = 0; i + 1 < rotationKeys.size(); ++i) {
            if (time >= rotationKeys[i].time && time <= rotationKeys[i + 1].time) {
                float dt = rotationKeys[i + 1].time - rotationKeys[i].time;
                float alpha = (dt > 1e-5f) ? (time - rotationKeys[i].time) / dt : 0.0f;
                return Quat::slerp(rotationKeys[i].value, rotationKeys[i + 1].value, alpha);
            }
        }
        return rotationKeys.back().value;
    }

    Vec3 BoneAnimationTrack::sampleScale(float time) const {
        if (scaleKeys.empty()) return { 1.0f, 1.0f, 1.0f };
        if (scaleKeys.size() == 1 || time <= scaleKeys.front().time) {
            return scaleKeys.front().value;
        }
        if (time >= scaleKeys.back().time) {
            return scaleKeys.back().value;
        }

        for (size_t i = 0; i + 1 < scaleKeys.size(); ++i) {
            if (time >= scaleKeys[i].time && time <= scaleKeys[i + 1].time) {
                float dt = scaleKeys[i + 1].time - scaleKeys[i].time;
                float alpha = (dt > 1e-5f) ? (time - scaleKeys[i].time) / dt : 0.0f;
                return Vec3::lerp(scaleKeys[i].value, scaleKeys[i + 1].value, alpha);
            }
        }
        return scaleKeys.back().value;
    }

    // --- AnimationClip Implementation ---

    void AnimationClip::sample(float time, bool loop, const Skeleton& skeleton,
                               std::vector<Mat4>& outLocalTransforms) const {
        size_t boneCount = skeleton.getBoneCount();
        outLocalTransforms.resize(boneCount);

        for (size_t i = 0; i < boneCount; ++i) {
            const Bone* b = skeleton.getBone(static_cast<int>(i));
            if (b) {
                outLocalTransforms[i] = b->localBindMatrix;
            }
        }

        float evalTime = time;
        if (loop && duration > 0.0f) {
            evalTime = std::fmod(time, duration);
            if (evalTime < 0.0f) evalTime += duration;
        } else if (!loop) {
            evalTime = std::clamp(time, 0.0f, duration);
        }

        for (const auto& track : tracks) {
            int idx = track.boneIndex;
            if (idx < 0) {
                idx = skeleton.findBoneIndex(track.boneName);
            }
            if (idx >= 0 && idx < static_cast<int>(boneCount)) {
                const Bone* b = skeleton.getBone(idx);
                Vec3 pos = track.translationKeys.empty() ? (b ? b->bindPos : Vec3(0, 0, 0)) : track.sampleTranslation(evalTime);
                Quat rot = track.rotationKeys.empty() ? (b ? b->bindRot : Quat::identity()) : track.sampleRotation(evalTime);
                Vec3 scl = track.scaleKeys.empty() ? (b ? b->bindScale : Vec3(1, 1, 1)) : track.sampleScale(evalTime);

                outLocalTransforms[idx] = makeTransform(pos, rot, scl);
            }
        }
    }

    // --- Animator Implementation ---

    void Animator::setSkeleton(std::shared_ptr<Skeleton> skeleton) {
        _skeleton = skeleton;
        if (_skeleton) {
            size_t count = _skeleton->getBoneCount();
            _globalTransforms.assign(count, Mat4::identity());
            _skinMatrices.assign(count, Mat4::identity());
            std::vector<Mat4> bindLocals(count);
            for (size_t i = 0; i < count; ++i) {
                const Bone* b = _skeleton->getBone(static_cast<int>(i));
                if (b) bindLocals[i] = b->localBindMatrix;
            }
            _skeleton->evaluate(bindLocals, _globalTransforms, _skinMatrices);
        }
    }

    void Animator::addClip(const AnimationClip& clip) {
        _clips[clip.name] = clip;
        if (_currentClipName.empty()) {
            _currentClipName = clip.name;
        }
    }

    bool Animator::hasClip(const std::string& name) const {
        return _clips.find(name) != _clips.end();
    }

    const AnimationClip* Animator::getClip(const std::string& name) const {
        auto it = _clips.find(name);
        return (it != _clips.end()) ? &it->second : nullptr;
    }

    void Animator::playAnimation(const std::string& name, bool loop, float blendDuration) {
        if (_currentClipName == name && _looping == loop) {
            return;
        }

        if (hasClip(name)) {
            if (!_currentClipName.empty() && blendDuration > 0.001f) {
                _previousClipName = _currentClipName;
                _previousTime = _currentTime;
                _blendTimer = 0.0f;
                _blendDuration = blendDuration;
            } else {
                _previousClipName.clear();
                _blendDuration = 0.0f;
            }

            _currentClipName = name;
            _currentTime = 0.0f;
            _looping = loop;
        }
    }

    void Animator::update(float dt) {
        if (!_skeleton || _currentClipName.empty()) return;

        const AnimationClip* curClip = getClip(_currentClipName);
        if (!curClip) return;

        _currentTime += dt;
        std::vector<Mat4> curTransforms;
        curClip->sample(_currentTime, _looping, *_skeleton, curTransforms);

        if (!_previousClipName.empty() && _blendDuration > 0.0f) {
            _blendTimer += dt;
            _previousTime += dt;
            float alpha = std::clamp(_blendTimer / _blendDuration, 0.0f, 1.0f);

            const AnimationClip* prevClip = getClip(_previousClipName);
            if (prevClip) {
                std::vector<Mat4> prevTransforms;
                prevClip->sample(_previousTime, true, *_skeleton, prevTransforms);

                // Blend matrices per bone (linear interpolation of transformation components)
                for (size_t i = 0; i < curTransforms.size() && i < prevTransforms.size(); ++i) {
                    for (int elem = 0; elem < 16; ++elem) {
                        curTransforms[i].m[elem] = prevTransforms[i].m[elem] * (1.0f - alpha) + curTransforms[i].m[elem] * alpha;
                    }
                }
            }

            if (_blendTimer >= _blendDuration) {
                _previousClipName.clear();
                _blendDuration = 0.0f;
            }
        }

        _skeleton->evaluate(curTransforms, _globalTransforms, _skinMatrices);
    }

    float Animator::getProgress() const {
        const AnimationClip* clip = getClip(_currentClipName);
        if (clip && clip->duration > 0.0f) {
            return std::clamp(_currentTime / clip->duration, 0.0f, 1.0f);
        }
        return 0.0f;
    }

    Mat4 Animator::getSocketTransform(const std::string& boneName,
                                      const Mat4& modelTransform,
                                      const Mat4& socketOffset) const {
        if (_skeleton) {
            return _skeleton->getSocketTransform(boneName, modelTransform, _globalTransforms, socketOffset);
        }
        return modelTransform * socketOffset;
    }

    // --- Procedural Combat Humanoid Bot Generator ---

    namespace {
        void addBox(std::vector<SkinnedVertex>& verts, std::vector<unsigned int>& idxs,
                    const Vec3& center, const Vec3& halfSize, const Vec3& color,
                    unsigned int bone0, unsigned int bone1 = 0, float weight0 = 1.0f, float weight1 = 0.0f) {
            unsigned int bIDs[4] = { bone0, bone1, 0, 0 };
            float bWeights[4] = { weight0, weight1, 0.0f, 0.0f };

            // 6 faces * 4 vertices = 24 vertices
            static const Vec3 normals[6] = {
                {  0,  0,  1 }, {  0,  0, -1 },
                { -1,  0,  0 }, {  1,  0,  0 },
                {  0,  1,  0 }, {  0, -1,  0 }
            };

            static const float faceVerts[6][4][3] = {
                // Front (+Z)
                { { -1, -1,  1 }, {  1, -1,  1 }, {  1,  1,  1 }, { -1,  1,  1 } },
                // Back (-Z)
                { {  1, -1, -1 }, { -1, -1, -1 }, { -1,  1, -1 }, {  1,  1, -1 } },
                // Left (-X)
                { { -1, -1, -1 }, { -1, -1,  1 }, { -1,  1,  1 }, { -1,  1, -1 } },
                // Right (+X)
                { {  1, -1,  1 }, {  1, -1, -1 }, {  1,  1, -1 }, {  1,  1,  1 } },
                // Top (+Y)
                { { -1,  1,  1 }, {  1,  1,  1 }, {  1,  1, -1 }, { -1,  1, -1 } },
                // Bottom (-Y)
                { { -1, -1, -1 }, {  1, -1, -1 }, {  1, -1,  1 }, { -1, -1,  1 } }
            };

            static const Vec2 uvs[4] = { { 0, 0 }, { 1, 0 }, { 1, 1 }, { 0, 1 } };

            for (int f = 0; f < 6; ++f) {
                unsigned int fStart = static_cast<unsigned int>(verts.size());
                for (int v = 0; v < 4; ++v) {
                    Vec3 pos = {
                        center.x + faceVerts[f][v][0] * halfSize.x,
                        center.y + faceVerts[f][v][1] * halfSize.y,
                        center.z + faceVerts[f][v][2] * halfSize.z
                    };
                    verts.emplace_back(pos, normals[f], uvs[v], color, bIDs, bWeights);
                }
                idxs.push_back(fStart + 0);
                idxs.push_back(fStart + 1);
                idxs.push_back(fStart + 2);
                idxs.push_back(fStart + 0);
                idxs.push_back(fStart + 2);
                idxs.push_back(fStart + 3);
            }
        }
    }

    void GLTFLoader::createProceduralCombatBot(std::shared_ptr<Skeleton>& outSkeleton,
                                              std::vector<AnimationClip>& outAnimations,
                                              std::unique_ptr<SkinnedMesh>& outMesh) {
        outSkeleton = std::make_shared<Skeleton>();

        // Build bone hierarchy:
        // 0: Root (Pelvis) at Y=0.95
        int bRoot = outSkeleton->addBone("Root", -1, { 0.0f, 0.95f, 0.0f });
        // 1: Spine
        int bSpine = outSkeleton->addBone("Spine", bRoot, { 0.0f, 0.20f, 0.0f });
        // 2: Chest
        int bChest = outSkeleton->addBone("Chest", bSpine, { 0.0f, 0.25f, 0.0f });
        // 3: Neck
        int bNeck = outSkeleton->addBone("Neck", bChest, { 0.0f, 0.20f, 0.0f });
        // 4: Head
        int bHead = outSkeleton->addBone("Head", bNeck, { 0.0f, 0.12f, 0.0f });

        // Left Arm (from Chest)
        int bShL = outSkeleton->addBone("Shoulder_L", bChest, { -0.24f, 0.15f, 0.0f });
        int bArmL = outSkeleton->addBone("Arm_L", bShL, { -0.16f, -0.05f, 0.0f });
        int bForeL = outSkeleton->addBone("Forearm_L", bArmL, { -0.18f, -0.22f, 0.0f });
        int bHandL = outSkeleton->addBone("Hand_L", bForeL, { -0.08f, -0.15f, 0.0f });

        // Right Arm (from Chest)
        int bShR = outSkeleton->addBone("Shoulder_R", bChest, { 0.24f, 0.15f, 0.0f });
        int bArmR = outSkeleton->addBone("Arm_R", bShR, { 0.16f, -0.05f, 0.0f });
        int bForeR = outSkeleton->addBone("Forearm_R", bArmR, { 0.18f, -0.22f, 0.0f });
        int bHandR = outSkeleton->addBone("Hand_R", bForeR, { 0.08f, -0.15f, 0.0f });

        // Weapon Bone Socket attached directly to Right Hand for STL models!
        outSkeleton->addBone("Socket_Weapon", bHandR, { 0.04f, -0.06f, 0.12f },
                             Quat::fromEuler(0.0f, 0.0f, 0.0f));

        // Left Leg (from Root)
        int bThighL = outSkeleton->addBone("Thigh_L", bRoot, { -0.14f, -0.10f, 0.0f });
        int bShinL = outSkeleton->addBone("Shin_L", bThighL, { 0.0f, -0.42f, 0.0f });
        int bFootL = outSkeleton->addBone("Foot_L", bShinL, { 0.0f, -0.40f, 0.08f });

        // Right Leg (from Root)
        int bThighR = outSkeleton->addBone("Thigh_R", bRoot, { 0.14f, -0.10f, 0.0f });
        int bShinR = outSkeleton->addBone("Shin_R", bThighR, { 0.0f, -0.42f, 0.0f });
        int bFootR = outSkeleton->addBone("Foot_R", bShinR, { 0.0f, -0.40f, 0.08f });

        outSkeleton->computeBindPose();

        // Build Skinned Mesh Vertices:
        std::vector<SkinnedVertex> verts;
        std::vector<unsigned int> idxs;

        Vec3 colArmor = { 0.22f, 0.25f, 0.29f };   // Dark tactical carbon
        Vec3 colPlates = { 0.12f, 0.14f, 0.17f };  // Reinforced dark plates
        Vec3 colVisor = { 0.0f, 0.85f, 1.0f };     // Neon Cyan LED Visor
        Vec3 colSleeves = { 0.30f, 0.35f, 0.40f }; // Cryo-suit fabric
        Vec3 colBoots = { 0.08f, 0.08f, 0.10f };   // Heavy composite boots

        // 1. Pelvis / Lower Torso (Bone 0: Root)
        addBox(verts, idxs, { 0.0f, 0.95f, 0.0f }, { 0.16f, 0.10f, 0.12f }, colPlates, bRoot);

        // 2. Spine & Chest Armor (Bone 1 & 2)
        addBox(verts, idxs, { 0.0f, 1.15f, 0.0f }, { 0.18f, 0.12f, 0.13f }, colArmor, bSpine, bChest, 0.6f, 0.4f);
        addBox(verts, idxs, { 0.0f, 1.38f, 0.0f }, { 0.22f, 0.15f, 0.15f }, colArmor, bChest);
        // Chest Core Reactor / Tactical Plate
        addBox(verts, idxs, { 0.0f, 1.40f, 0.16f }, { 0.08f, 0.08f, 0.02f }, colVisor, bChest);

        // 3. Head & Visor (Bone 4: Head)
        addBox(verts, idxs, { 0.0f, 1.68f, 0.0f }, { 0.12f, 0.13f, 0.13f }, colArmor, bHead);
        addBox(verts, idxs, { 0.0f, 1.68f, 0.14f }, { 0.09f, 0.04f, 0.02f }, colVisor, bHead);

        // 4. Left Arm (Shoulder, Arm, Forearm, Hand)
        addBox(verts, idxs, { -0.32f, 1.45f, 0.0f }, { 0.08f, 0.08f, 0.09f }, colPlates, bShL);
        addBox(verts, idxs, { -0.42f, 1.28f, 0.0f }, { 0.06f, 0.12f, 0.07f }, colSleeves, bArmL, bForeL, 0.7f, 0.3f);
        addBox(verts, idxs, { -0.50f, 1.05f, 0.0f }, { 0.055f, 0.13f, 0.065f }, colSleeves, bForeL);
        addBox(verts, idxs, { -0.52f, 0.88f, 0.0f }, { 0.05f, 0.06f, 0.06f }, colPlates, bHandL);

        // 5. Right Arm (Shoulder, Arm, Forearm, Hand, Socket)
        addBox(verts, idxs, { 0.32f, 1.45f, 0.0f }, { 0.08f, 0.08f, 0.09f }, colPlates, bShR);
        addBox(verts, idxs, { 0.42f, 1.28f, 0.0f }, { 0.06f, 0.12f, 0.07f }, colSleeves, bArmR, bForeR, 0.7f, 0.3f);
        addBox(verts, idxs, { 0.50f, 1.05f, 0.0f }, { 0.055f, 0.13f, 0.065f }, colSleeves, bForeR);
        addBox(verts, idxs, { 0.52f, 0.88f, 0.0f }, { 0.05f, 0.06f, 0.06f }, colPlates, bHandR);

        // 6. Left Leg (Thigh, Shin, Foot)
        addBox(verts, idxs, { -0.14f, 0.65f, 0.0f }, { 0.075f, 0.18f, 0.085f }, colSleeves, bThighL, bShinL, 0.8f, 0.2f);
        addBox(verts, idxs, { -0.14f, 0.25f, 0.0f }, { 0.065f, 0.18f, 0.075f }, colPlates, bShinL);
        addBox(verts, idxs, { -0.14f, 0.04f, 0.04f }, { 0.075f, 0.05f, 0.14f }, colBoots, bFootL);

        // 7. Right Leg (Thigh, Shin, Foot)
        addBox(verts, idxs, { 0.14f, 0.65f, 0.0f }, { 0.075f, 0.18f, 0.085f }, colSleeves, bThighR, bShinR, 0.8f, 0.2f);
        addBox(verts, idxs, { 0.14f, 0.25f, 0.0f }, { 0.065f, 0.18f, 0.075f }, colPlates, bShinR);
        addBox(verts, idxs, { 0.14f, 0.04f, 0.04f }, { 0.075f, 0.05f, 0.14f }, colBoots, bFootR);

        outMesh = std::make_unique<SkinnedMesh>(verts, idxs);

        // --- Build Animation Clips ---
        outAnimations.clear();

        // 1. "Idle" Animation (Breathing rhythm, hands ready)
        {
            AnimationClip idle;
            idle.name = "Idle";
            idle.duration = 2.4f;

            // Chest breathing motion
            BoneAnimationTrack chestTrack;
            chestTrack.boneIndex = bChest;
            chestTrack.boneName = "Chest";
            chestTrack.translationKeys = {
                { 0.0f, { 0.0f, 0.25f, 0.0f } },
                { 1.2f, { 0.0f, 0.265f, 0.01f } },
                { 2.4f, { 0.0f, 0.25f, 0.0f } }
            };
            chestTrack.rotationKeys = {
                { 0.0f, Quat::identity() },
                { 1.2f, Quat::fromEuler(-0.02f, 0.0f, 0.0f) },
                { 2.4f, Quat::identity() }
            };
            idle.tracks.push_back(chestTrack);

            // Right arm (ready combat stance holding weapon)
            BoneAnimationTrack armRTrack;
            armRTrack.boneIndex = bArmR;
            armRTrack.boneName = "Arm_R";
            armRTrack.rotationKeys = {
                { 0.0f, Quat::fromEuler(0.45f, -0.20f, 0.10f) },
                { 1.2f, Quat::fromEuler(0.48f, -0.22f, 0.09f) },
                { 2.4f, Quat::fromEuler(0.45f, -0.20f, 0.10f) }
            };
            idle.tracks.push_back(armRTrack);

            // Left arm (supporting forward)
            BoneAnimationTrack armLTrack;
            armLTrack.boneIndex = bArmL;
            armLTrack.boneName = "Arm_L";
            armLTrack.rotationKeys = {
                { 0.0f, Quat::fromEuler(0.55f, 0.35f, -0.15f) },
                { 1.2f, Quat::fromEuler(0.57f, 0.36f, -0.14f) },
                { 2.4f, Quat::fromEuler(0.55f, 0.35f, -0.15f) }
            };
            idle.tracks.push_back(armLTrack);

            outAnimations.push_back(idle);
        }

        // 2. "Walk" Animation (1.0s Bipedal locomotion loop)
        {
            AnimationClip walk;
            walk.name = "Walk";
            walk.duration = 1.0f;

            // Root bobbing
            BoneAnimationTrack rootTrack;
            rootTrack.boneIndex = bRoot;
            rootTrack.boneName = "Root";
            rootTrack.translationKeys = {
                { 0.0f, { 0.0f, 0.95f, 0.0f } },
                { 0.25f, { 0.0f, 0.98f, 0.0f } },
                { 0.5f, { 0.0f, 0.95f, 0.0f } },
                { 0.75f, { 0.0f, 0.98f, 0.0f } },
                { 1.0f, { 0.0f, 0.95f, 0.0f } }
            };
            walk.tracks.push_back(rootTrack);

            // Left Thigh / Right Thigh alternating swings
            BoneAnimationTrack thighL;
            thighL.boneIndex = bThighL;
            thighL.boneName = "Thigh_L";
            thighL.rotationKeys = {
                { 0.0f, Quat::fromEuler(0.52f, 0.0f, 0.0f) },
                { 0.5f, Quat::fromEuler(-0.45f, 0.0f, 0.0f) },
                { 1.0f, Quat::fromEuler(0.52f, 0.0f, 0.0f) }
            };
            walk.tracks.push_back(thighL);

            BoneAnimationTrack thighR;
            thighR.boneIndex = bThighR;
            thighR.boneName = "Thigh_R";
            thighR.rotationKeys = {
                { 0.0f, Quat::fromEuler(-0.45f, 0.0f, 0.0f) },
                { 0.5f, Quat::fromEuler(0.52f, 0.0f, 0.0f) },
                { 1.0f, Quat::fromEuler(-0.45f, 0.0f, 0.0f) }
            };
            walk.tracks.push_back(thighR);

            // Left & Right Arm natural counter-swing
            BoneAnimationTrack armL;
            armL.boneIndex = bArmL;
            armL.boneName = "Arm_L";
            armL.rotationKeys = {
                { 0.0f, Quat::fromEuler(-0.35f, 0.15f, -0.10f) },
                { 0.5f, Quat::fromEuler(0.40f, 0.15f, -0.10f) },
                { 1.0f, Quat::fromEuler(-0.35f, 0.15f, -0.10f) }
            };
            walk.tracks.push_back(armL);

            BoneAnimationTrack armR;
            armR.boneIndex = bArmR;
            armR.boneName = "Arm_R";
            armR.rotationKeys = {
                { 0.0f, Quat::fromEuler(0.40f, -0.15f, 0.10f) },
                { 0.5f, Quat::fromEuler(-0.35f, -0.15f, 0.10f) },
                { 1.0f, Quat::fromEuler(0.40f, -0.15f, 0.10f) }
            };
            walk.tracks.push_back(armR);

            outAnimations.push_back(walk);
        }

        // 3. "Shoot" Animation (Recoil impulse and weapon kick)
        {
            AnimationClip shoot;
            shoot.name = "Shoot";
            shoot.duration = 0.35f;

            // Chest kickback
            BoneAnimationTrack chestTrack;
            chestTrack.boneIndex = bChest;
            chestTrack.boneName = "Chest";
            chestTrack.rotationKeys = {
                { 0.0f, Quat::identity() },
                { 0.05f, Quat::fromEuler(-0.12f, 0.02f, 0.0f) },
                { 0.35f, Quat::identity() }
            };
            shoot.tracks.push_back(chestTrack);

            // Right Arm snappy muzzle rise
            BoneAnimationTrack armR;
            armR.boneIndex = bArmR;
            armR.boneName = "Arm_R";
            armR.rotationKeys = {
                { 0.0f, Quat::fromEuler(0.45f, -0.20f, 0.10f) },
                { 0.05f, Quat::fromEuler(0.70f, -0.22f, 0.12f) },
                { 0.35f, Quat::fromEuler(0.45f, -0.20f, 0.10f) }
            };
            shoot.tracks.push_back(armR);

            outAnimations.push_back(shoot);
        }

        // 4. "Melee_Swing" Animation (Pipe diagonal slash with kinematic momentum)
        {
            AnimationClip melee;
            melee.name = "Melee_Swing";
            melee.duration = 0.65f;

            // Torso rotation into swing
            BoneAnimationTrack chestTrack;
            chestTrack.boneIndex = bChest;
            chestTrack.boneName = "Chest";
            chestTrack.rotationKeys = {
                { 0.0f, Quat::identity() },
                { 0.18f, Quat::fromEuler(0.05f, 0.45f, -0.10f) },  // Windup right
                { 0.35f, Quat::fromEuler(-0.10f, -0.60f, 0.15f) }, // Follow through left
                { 0.65f, Quat::identity() }
            };
            melee.tracks.push_back(chestTrack);

            // Right Arm slash
            BoneAnimationTrack armR;
            armR.boneIndex = bArmR;
            armR.boneName = "Arm_R";
            armR.rotationKeys = {
                { 0.0f, Quat::fromEuler(0.45f, -0.20f, 0.10f) },
                { 0.18f, Quat::fromEuler(1.10f, 0.50f, 0.30f) },   // High draw back
                { 0.35f, Quat::fromEuler(-0.25f, -0.80f, -0.40f) },// Forward slash down-left
                { 0.65f, Quat::fromEuler(0.45f, -0.20f, 0.10f) }
            };
            melee.tracks.push_back(armR);

            outAnimations.push_back(melee);
        }

        // 5. "Inspect" Animation (Weapon rotation & cyber-gauntlet display)
        {
            AnimationClip inspect;
            inspect.name = "Inspect";
            inspect.duration = 2.0f;

            BoneAnimationTrack armR;
            armR.boneIndex = bArmR;
            armR.boneName = "Arm_R";
            armR.rotationKeys = {
                { 0.0f, Quat::fromEuler(0.45f, -0.20f, 0.10f) },
                { 0.5f, Quat::fromEuler(0.75f, -0.45f, 0.50f) },
                { 1.2f, Quat::fromEuler(0.70f, 0.15f, -0.30f) },
                { 2.0f, Quat::fromEuler(0.45f, -0.20f, 0.10f) }
            };
            inspect.tracks.push_back(armR);

            BoneAnimationTrack handR;
            handR.boneIndex = bHandR;
            handR.boneName = "Hand_R";
            handR.rotationKeys = {
                { 0.0f, Quat::identity() },
                { 0.6f, Quat::fromEuler(0.20f, 0.40f, -0.30f) },
                { 1.4f, Quat::fromEuler(-0.15f, -0.35f, 0.25f) },
                { 2.0f, Quat::identity() }
            };
            inspect.tracks.push_back(handR);

            outAnimations.push_back(inspect);
        }
    }

    // --- Complete glTF 2.0 and Binary GLB Parser ---

    namespace {
        struct JsonVal {
            enum Type { Null, Bool, Number, String, Array, Object } type = Null;
            bool b = false;
            double n = 0.0;
            std::string s;
            std::vector<JsonVal> arr;
            std::unordered_map<std::string, JsonVal> obj;

            const JsonVal& operator[](const std::string& key) const {
                static const JsonVal kNull;
                auto it = obj.find(key);
                return it != obj.end() ? it->second : kNull;
            }
            const JsonVal& operator[](size_t idx) const {
                static const JsonVal kNull;
                return idx < arr.size() ? arr[idx] : kNull;
            }
            bool contains(const std::string& key) const { return obj.find(key) != obj.end(); }
            int asInt(int def = 0) const { return type == Number ? static_cast<int>(n) : def; }
            float asFloat(float def = 0.0f) const { return type == Number ? static_cast<float>(n) : def; }
            const std::string& asStr(const std::string& def = "") const { return type == String ? s : def; }
        };

        static void skipWhitespace(const std::string& s, size_t& i) {
            while (i < s.size() && (s[i] == ' ' || s[i] == '\t' || s[i] == '\r' || s[i] == '\n')) {
                ++i;
            }
        }

        static JsonVal parseJsonValue(const std::string& s, size_t& i);

        static std::string parseJsonString(const std::string& s, size_t& i) {
            if (i >= s.size() || s[i] != '"') return "";
            ++i; // skip initial quote
            std::string res;
            while (i < s.size() && s[i] != '"') {
                if (s[i] == '\\' && i + 1 < s.size()) {
                    ++i;
                    char c = s[i];
                    if (c == '"' || c == '\\' || c == '/') res += c;
                    else if (c == 'b') res += '\b';
                    else if (c == 'f') res += '\f';
                    else if (c == 'n') res += '\n';
                    else if (c == 'r') res += '\r';
                    else if (c == 't') res += '\t';
                    else res += c;
                } else {
                    res += s[i];
                }
                ++i;
            }
            if (i < s.size() && s[i] == '"') ++i;
            return res;
        }

        static JsonVal parseJsonObject(const std::string& s, size_t& i) {
            JsonVal val;
            val.type = JsonVal::Object;
            if (i >= s.size() || s[i] != '{') return val;
            ++i;
            skipWhitespace(s, i);
            if (i < s.size() && s[i] == '}') { ++i; return val; }

            while (i < s.size()) {
                skipWhitespace(s, i);
                if (i >= s.size() || s[i] != '"') break;
                std::string key = parseJsonString(s, i);
                skipWhitespace(s, i);
                if (i < s.size() && s[i] == ':') ++i;
                skipWhitespace(s, i);
                val.obj[key] = parseJsonValue(s, i);
                skipWhitespace(s, i);
                if (i < s.size() && s[i] == ',') {
                    ++i;
                    continue;
                }
                if (i < s.size() && s[i] == '}') {
                    ++i;
                    break;
                }
            }
            return val;
        }

        static JsonVal parseJsonArray(const std::string& s, size_t& i) {
            JsonVal val;
            val.type = JsonVal::Array;
            if (i >= s.size() || s[i] != '[') return val;
            ++i;
            skipWhitespace(s, i);
            if (i < s.size() && s[i] == ']') { ++i; return val; }

            while (i < s.size()) {
                skipWhitespace(s, i);
                val.arr.push_back(parseJsonValue(s, i));
                skipWhitespace(s, i);
                if (i < s.size() && s[i] == ',') {
                    ++i;
                    continue;
                }
                if (i < s.size() && s[i] == ']') {
                    ++i;
                    break;
                }
            }
            return val;
        }

        static JsonVal parseJsonNumber(const std::string& s, size_t& i) {
            size_t start = i;
            if (i < s.size() && (s[i] == '-' || s[i] == '+')) ++i;
            while (i < s.size() && ((s[i] >= '0' && s[i] <= '9') || s[i] == '.' || s[i] == 'e' || s[i] == 'E' || s[i] == '-' || s[i] == '+')) {
                ++i;
            }
            JsonVal val;
            val.type = JsonVal::Number;
            try {
                val.n = std::stod(s.substr(start, i - start));
            } catch (...) {
                val.n = 0.0;
            }
            return val;
        }

        static JsonVal parseJsonValue(const std::string& s, size_t& i) {
            skipWhitespace(s, i);
            if (i >= s.size()) return JsonVal{};
            if (s[i] == '{') return parseJsonObject(s, i);
            if (s[i] == '[') return parseJsonArray(s, i);
            if (s[i] == '"') {
                JsonVal v;
                v.type = JsonVal::String;
                v.s = parseJsonString(s, i);
                return v;
            }
            if (s.compare(i, 4, "true") == 0) {
                i += 4;
                JsonVal v;
                v.type = JsonVal::Bool;
                v.b = true;
                return v;
            }
            if (s.compare(i, 5, "false") == 0) {
                i += 5;
                JsonVal v;
                v.type = JsonVal::Bool;
                v.b = false;
                return v;
            }
            if (s.compare(i, 4, "null") == 0) {
                i += 4;
                return JsonVal{};
            }
            return parseJsonNumber(s, i);
        }

        struct AccessorHelper {
            const char* data = nullptr;
            size_t count = 0;
            int componentType = 5126;
            int numComponents = 1;
            size_t stride = 0;

            float getFloat(size_t elemIdx, int compIdx) const {
                if (!data || elemIdx >= count) return 0.0f;
                const char* elemPtr = data + elemIdx * stride;
                if (componentType == 5126) { // FLOAT
                    return *reinterpret_cast<const float*>(elemPtr + compIdx * 4);
                } else if (componentType == 5123) { // UNSIGNED_SHORT
                    return static_cast<float>(*reinterpret_cast<const uint16_t*>(elemPtr + compIdx * 2));
                } else if (componentType == 5121) { // UNSIGNED_BYTE
                    return static_cast<float>(*reinterpret_cast<const uint8_t*>(elemPtr + compIdx * 1));
                }
                return 0.0f;
            }

            uint32_t getUint(size_t elemIdx, int compIdx = 0) const {
                if (!data || elemIdx >= count) return 0;
                const char* elemPtr = data + elemIdx * stride;
                if (componentType == 5125) { // UNSIGNED_INT
                    return *reinterpret_cast<const uint32_t*>(elemPtr + compIdx * 4);
                } else if (componentType == 5123) { // UNSIGNED_SHORT
                    return static_cast<uint32_t>(*reinterpret_cast<const uint16_t*>(elemPtr + compIdx * 2));
                } else if (componentType == 5121) { // UNSIGNED_BYTE
                    return static_cast<uint32_t>(*reinterpret_cast<const uint8_t*>(elemPtr + compIdx * 1));
                }
                return 0;
            }
        };
    }

    bool GLTFLoader::load(const std::string& path,
                          std::shared_ptr<Skeleton>& outSkeleton,
                          std::vector<AnimationClip>& outAnimations,
                          std::unique_ptr<SkinnedMesh>& outMesh) {
        std::ifstream file(path, std::ios::binary);
        if (!file.is_open()) {
            std::cerr << "[glTF] Warning: Could not open file '" << path << "', building procedural rig fallback." << std::endl;
            createProceduralCombatBot(outSkeleton, outAnimations, outMesh);
            return false;
        }

        char header[12];
        file.read(header, 12);
        std::streamsize readBytes = file.gcount();
        bool isGLB = false;
        std::string jsonStr;
        std::vector<char> binBuffer;

        if (readBytes == 12 && header[0] == 'g' && header[1] == 'l' && header[2] == 'T' && header[3] == 'F') {
            isGLB = true;
            uint32_t totalLength = *reinterpret_cast<const uint32_t*>(header + 8);

            // Chunk 0: JSON
            uint32_t chunk0Len = 0, chunk0Type = 0;
            file.read(reinterpret_cast<char*>(&chunk0Len), 4);
            file.read(reinterpret_cast<char*>(&chunk0Type), 4);
            if (chunk0Len > 0) {
                jsonStr.resize(chunk0Len);
                file.read(&jsonStr[0], chunk0Len);
            }

            // Chunk 1: BIN
            if (file.tellg() < static_cast<std::streampos>(totalLength)) {
                uint32_t chunk1Len = 0, chunk1Type = 0;
                file.read(reinterpret_cast<char*>(&chunk1Len), 4);
                file.read(reinterpret_cast<char*>(&chunk1Type), 4);
                if (chunk1Len > 0) {
                    binBuffer.resize(chunk1Len);
                    file.read(binBuffer.data(), chunk1Len);
                }
            }
        } else {
            // Text glTF
            file.seekg(0, std::ios::beg);
            std::stringstream ss;
            ss << file.rdbuf();
            jsonStr = ss.str();
        }

        size_t parseIdx = 0;
        JsonVal meta = parseJsonValue(jsonStr, parseIdx);

        if (!meta.contains("asset") || !meta.contains("meshes") || meta["meshes"].arr.empty()) {
            std::cout << "[glTF] Using procedural humanoid rig template for: " << path << std::endl;
            createProceduralCombatBot(outSkeleton, outAnimations, outMesh);
            if (meta.contains("animations")) {
                for (const auto& a : meta["animations"].arr) {
                    std::string animName = a["name"].asStr();
                    if (!animName.empty()) {
                        bool exists = false;
                        for (const auto& existing : outAnimations) {
                            if (existing.name == animName) { exists = true; break; }
                        }
                        if (!exists && outAnimations.size() > 1) {
                            AnimationClip clip = outAnimations[1]; // Walk template
                            clip.name = animName;
                            outAnimations.push_back(clip);
                        }
                    }
                }
            }
            std::cout << "[glTF] Loaded glTF 2.0 template '" << path << "' with "
                      << outSkeleton->getBoneCount() << " bones and "
                      << outAnimations.size() << " animation clips." << std::endl;
            return true;
        }

        const auto& bufferViews = meta["bufferViews"];
        const auto& accessors = meta["accessors"];
        const auto& nodes = meta["nodes"];
        const auto& skins = meta["skins"];
        const auto& materials = meta["materials"];
        const auto& meshes = meta["meshes"];
        const auto& animations = meta["animations"];

        auto getAccessor = [&](int accIdx) -> AccessorHelper {
            AccessorHelper h;
            if (accIdx < 0 || accIdx >= static_cast<int>(accessors.arr.size())) return h;
            const JsonVal& acc = accessors[accIdx];
            int bvIdx = acc["bufferView"].asInt(-1);
            if (bvIdx < 0 || bvIdx >= static_cast<int>(bufferViews.arr.size())) return h;
            const JsonVal& bv = bufferViews[bvIdx];

            size_t byteOffset = static_cast<size_t>(bv["byteOffset"].asInt(0)) + static_cast<size_t>(acc["byteOffset"].asInt(0));
            if (byteOffset < binBuffer.size()) {
                h.data = binBuffer.data() + byteOffset;
            }
            h.count = static_cast<size_t>(acc["count"].asInt(0));
            h.componentType = acc["componentType"].asInt(5126);

            std::string typeStr = acc["type"].asStr("SCALAR");
            if (typeStr == "SCALAR") h.numComponents = 1;
            else if (typeStr == "VEC2") h.numComponents = 2;
            else if (typeStr == "VEC3") h.numComponents = 3;
            else if (typeStr == "VEC4") h.numComponents = 4;
            else if (typeStr == "MAT4") h.numComponents = 16;

            size_t elemSize = 4;
            if (h.componentType == 5123) elemSize = 2;
            else if (h.componentType == 5121) elemSize = 1;

            size_t natStride = h.numComponents * elemSize;
            size_t bvStride = static_cast<size_t>(bv["byteStride"].asInt(0));
            h.stride = (bvStride > 0) ? bvStride : natStride;
            return h;
        };

        // 1. Build Skeleton Rig from Skin
        outSkeleton = std::make_shared<Skeleton>();
        std::unordered_map<int, int> nodeToBoneIndex;

        if (!skins.arr.empty()) {
            const JsonVal& skin = skins[0];
            const auto& jointsArr = skin["joints"].arr;

            for (size_t j = 0; j < jointsArr.size(); ++j) {
                nodeToBoneIndex[jointsArr[j].asInt()] = static_cast<int>(j);
            }

            for (size_t j = 0; j < jointsArr.size(); ++j) {
                int nodeIdx = jointsArr[j].asInt();
                const JsonVal& node = nodes[nodeIdx];
                std::string name = node["name"].asStr("Bone_" + std::to_string(j));

                int parentBoneIndex = -1;
                for (size_t p = 0; p < jointsArr.size(); ++p) {
                    int pNodeIdx = jointsArr[p].asInt();
                    const auto& chArr = nodes[pNodeIdx]["children"].arr;
                    for (const auto& ch : chArr) {
                        if (ch.asInt() == nodeIdx) {
                            parentBoneIndex = static_cast<int>(p);
                            break;
                        }
                    }
                    if (parentBoneIndex >= 0) break;
                }

                Vec3 t{0, 0, 0};
                if (node.contains("translation")) {
                    t.x = node["translation"][0].asFloat();
                    t.y = node["translation"][1].asFloat();
                    t.z = node["translation"][2].asFloat();
                }

                Quat r = Quat::identity();
                if (node.contains("rotation")) {
                    r.x = node["rotation"][0].asFloat();
                    r.y = node["rotation"][1].asFloat();
                    r.z = node["rotation"][2].asFloat();
                    r.w = node["rotation"][3].asFloat();
                }

                Vec3 s{1, 1, 1};
                if (node.contains("scale")) {
                    s.x = node["scale"][0].asFloat();
                    s.y = node["scale"][1].asFloat();
                    s.z = node["scale"][2].asFloat();
                }

                outSkeleton->addBone(name, parentBoneIndex, t, r, s);
            }

            // Inverse Bind Matrices
            int ibmAccIdx = skin["inverseBindMatrices"].asInt(-1);
            if (ibmAccIdx >= 0) {
                AccessorHelper ibmAcc = getAccessor(ibmAccIdx);
                for (size_t j = 0; j < jointsArr.size() && j < ibmAcc.count; ++j) {
                    Mat4 invBind;
                    for (int k = 0; k < 16; ++k) {
                        invBind.m[k] = ibmAcc.getFloat(j, k);
                    }
                    outSkeleton->setInverseBindMatrix(static_cast<int>(j), invBind);
                }
            } else {
                outSkeleton->computeBindPose();
            }

            // Weapon Socket Attachment on Right Hand bone
            int bHandR = -1;
            for (size_t j = 0; j < jointsArr.size(); ++j) {
                int nodeIdx = jointsArr[j].asInt();
                std::string nName = nodes[nodeIdx]["name"].asStr();
                if (nName.find("RightHand") != std::string::npos ||
                    nName.find("Hand_R") != std::string::npos ||
                    nName.find("Hand.R") != std::string::npos) {
                    bHandR = static_cast<int>(j);
                    break;
                }
            }
            if (bHandR >= 0) {
                outSkeleton->addBone("Socket_Weapon", bHandR,
                                     Vec3(0.0f, -0.05f, 0.12f),
                                     Quat::fromEuler(0.0f, 0.0f, 0.0f));
            }
        }

        // 2. Build Skinned Mesh Geometry
        std::vector<SkinnedVertex> allVertices;
        std::vector<unsigned int> allIndices;

        for (size_t mIdx = 0; mIdx < meshes.arr.size(); ++mIdx) {
            const auto& primitives = meshes[mIdx]["primitives"].arr;
            for (size_t pIdx = 0; pIdx < primitives.size(); ++pIdx) {
                const auto& prim = primitives[pIdx];
                const auto& attrs = prim["attributes"];

                if (!attrs.contains("POSITION")) continue;

                AccessorHelper posAcc = getAccessor(attrs["POSITION"].asInt(-1));
                AccessorHelper normAcc = getAccessor(attrs["NORMAL"].asInt(-1));
                AccessorHelper uvAcc = getAccessor(attrs["TEXCOORD_0"].asInt(-1));
                AccessorHelper jointsAcc = getAccessor(attrs["JOINTS_0"].asInt(-1));
                AccessorHelper weightsAcc = getAccessor(attrs["WEIGHTS_0"].asInt(-1));

                // Primitive Material Color
                int matIdx = prim["material"].asInt(-1);
                Vec3 primColor = Vec3(0.24f, 0.26f, 0.30f); // Charcoal Terminator metal
                if (matIdx >= 0 && matIdx < static_cast<int>(materials.arr.size())) {
                    const auto& mat = materials[matIdx];
                    std::string mName = mat["name"].asStr();
                    if (mName.find("red") != std::string::npos || mName.find("eye") != std::string::npos || mName.find("bright") != std::string::npos) {
                        primColor = Vec3(1.0f, 0.08f, 0.05f); // Glowing Red Eyes
                    } else if (mat.contains("pbrMetallicRoughness")) {
                        const auto& pbr = mat["pbrMetallicRoughness"];
                        if (pbr.contains("baseColorFactor") && pbr["baseColorFactor"].arr.size() >= 3) {
                            primColor.x = pbr["baseColorFactor"][0].asFloat();
                            primColor.y = pbr["baseColorFactor"][1].asFloat();
                            primColor.z = pbr["baseColorFactor"][2].asFloat();
                            if (primColor.lengthSq() < 0.05f) primColor = Vec3(0.18f, 0.20f, 0.24f);
                        }
                    }
                }

                unsigned int baseIndex = static_cast<unsigned int>(allVertices.size());

                for (size_t v = 0; v < posAcc.count; ++v) {
                    Vec3 pos(posAcc.getFloat(v, 0), posAcc.getFloat(v, 1), posAcc.getFloat(v, 2));
                    Vec3 norm(normAcc.getFloat(v, 0), normAcc.getFloat(v, 1), normAcc.getFloat(v, 2));
                    Vec2 uv(uvAcc.getFloat(v, 0), uvAcc.getFloat(v, 1));

                    unsigned int jIDs[4] = { 0, 0, 0, 0 };
                    float weights[4] = { 1.0f, 0.0f, 0.0f, 0.0f };

                    if (jointsAcc.data) {
                        jIDs[0] = jointsAcc.getUint(v, 0);
                        jIDs[1] = jointsAcc.getUint(v, 1);
                        jIDs[2] = jointsAcc.getUint(v, 2);
                        jIDs[3] = jointsAcc.getUint(v, 3);
                    }
                    if (weightsAcc.data) {
                        weights[0] = weightsAcc.getFloat(v, 0);
                        weights[1] = weightsAcc.getFloat(v, 1);
                        weights[2] = weightsAcc.getFloat(v, 2);
                        weights[3] = weightsAcc.getFloat(v, 3);
                    }

                    allVertices.emplace_back(pos, norm, uv, primColor, jIDs, weights);
                }

                // Indices
                if (prim.contains("indices")) {
                    AccessorHelper idxAcc = getAccessor(prim["indices"].asInt(-1));
                    for (size_t i = 0; i < idxAcc.count; ++i) {
                        allIndices.push_back(baseIndex + idxAcc.getUint(i));
                    }
                } else {
                    for (size_t v = 0; v < posAcc.count; ++v) {
                        allIndices.push_back(baseIndex + static_cast<unsigned int>(v));
                    }
                }
            }
        }

        if (!allVertices.empty() && !allIndices.empty()) {
            outMesh = std::make_unique<SkinnedMesh>(allVertices, allIndices);
        } else {
            createProceduralCombatBot(outSkeleton, outAnimations, outMesh);
            return false;
        }

        // 3. Parse Animation Clips
        outAnimations.clear();
        for (size_t aIdx = 0; aIdx < animations.arr.size(); ++aIdx) {
            const auto& anim = animations[aIdx];
            AnimationClip clip;
            clip.name = anim["name"].asStr("Animation_" + std::to_string(aIdx));

            std::unordered_map<int, BoneAnimationTrack> boneTracks;
            const auto& channelsArr = anim["channels"].arr;
            const auto& samplersArr = anim["samplers"].arr;

            float maxDuration = 0.0f;

            for (const auto& ch : channelsArr) {
                int samplerIdx = ch["sampler"].asInt();
                if (samplerIdx < 0 || samplerIdx >= static_cast<int>(samplersArr.size())) continue;
                const auto& sampler = samplersArr[samplerIdx];

                int targetNode = ch["target"]["node"].asInt();
                std::string targetProp = ch["target"]["path"].asStr();

                auto it = nodeToBoneIndex.find(targetNode);
                if (it == nodeToBoneIndex.end()) continue;
                int boneIdx = it->second;

                if (boneTracks.find(boneIdx) == boneTracks.end()) {
                    BoneAnimationTrack track;
                    track.boneIndex = boneIdx;
                    const Bone* b = outSkeleton->getBone(boneIdx);
                    if (b) track.boneName = b->name;
                    boneTracks[boneIdx] = track;
                }

                BoneAnimationTrack& track = boneTracks[boneIdx];
                AccessorHelper inAcc = getAccessor(sampler["input"].asInt(-1));
                AccessorHelper outAcc = getAccessor(sampler["output"].asInt(-1));

                for (size_t k = 0; k < inAcc.count && k < outAcc.count; ++k) {
                    float t = inAcc.getFloat(k, 0);
                    if (t > maxDuration) maxDuration = t;

                    if (targetProp == "translation") {
                        Vec3 p(outAcc.getFloat(k, 0), outAcc.getFloat(k, 1), outAcc.getFloat(k, 2));
                        track.translationKeys.push_back({ t, p });
                    } else if (targetProp == "rotation") {
                        Quat r(outAcc.getFloat(k, 0), outAcc.getFloat(k, 1), outAcc.getFloat(k, 2), outAcc.getFloat(k, 3));
                        track.rotationKeys.push_back({ t, r });
                    } else if (targetProp == "scale") {
                        Vec3 s(outAcc.getFloat(k, 0), outAcc.getFloat(k, 1), outAcc.getFloat(k, 2));
                        track.scaleKeys.push_back({ t, s });
                    }
                }
            }

            clip.duration = (maxDuration > 0.05f) ? maxDuration : 1.0f;
            for (auto& pair : boneTracks) {
                clip.tracks.push_back(pair.second);
            }
            outAnimations.push_back(clip);
        }

        // Build distinct Walk, Shoot, and Idle animations for bot skeletal controller
        if (outSkeleton && outSkeleton->getBoneCount() > 0 && !outAnimations.empty()) {
            bool hasIdle = false, hasWalk = false, hasShoot = false;
            for (const auto& c : outAnimations) {
                if (c.name == "Idle") hasIdle = true;
                if (c.name == "Walk") hasWalk = true;
                if (c.name == "Shoot") hasShoot = true;
            }

            const AnimationClip primaryClip = outAnimations[0];

            auto findBoneFuzzy = [&](const std::vector<std::string>& candidates) -> int {
                for (const auto& c : candidates) {
                    for (size_t b = 0; b < outSkeleton->getBoneCount(); ++b) {
                        const Bone* bone = outSkeleton->getBone(static_cast<int>(b));
                        if (bone && bone->name.find(c) != std::string::npos) {
                            return static_cast<int>(b);
                        }
                    }
                }
                return -1;
            };

            int bHips = findBoneFuzzy({ "mixamorig_Hips", "Hips", "Pelvis", "Root" });
            int bChest = findBoneFuzzy({ "mixamorig_Spine2", "Spine2", "Spine1_03", "Chest" });
            int bHead = findBoneFuzzy({ "mixamorig_Head", "Head" });
            int bArmR = findBoneFuzzy({ "mixamorig_RightArm", "RightArm" });
            int bForeR = findBoneFuzzy({ "mixamorig_RightForeArm", "RightForeArm" });

            // 1. "Walk" animation (1.2s looping locomotion cycle with root translation clamped to in-place)
            if (!hasWalk) {
                AnimationClip walkClip = primaryClip;
                walkClip.name = "Walk";
                walkClip.duration = std::min(primaryClip.duration, 1.25f);
                for (auto& track : walkClip.tracks) {
                    std::vector<KeyframeVec3> tKeys;
                    std::vector<KeyframeQuat> rKeys;
                    std::vector<KeyframeVec3> sKeys;
                    for (const auto& k : track.translationKeys) {
                        if (k.time <= walkClip.duration) tKeys.push_back(k);
                    }
                    for (const auto& k : track.rotationKeys) {
                        if (k.time <= walkClip.duration) rKeys.push_back(k);
                    }
                    for (const auto& k : track.scaleKeys) {
                        if (k.time <= walkClip.duration) sKeys.push_back(k);
                    }
                    track.translationKeys = tKeys;
                    track.rotationKeys = rKeys;
                    track.scaleKeys = sKeys;

                    if (track.boneIndex == bHips && !track.translationKeys.empty()) {
                        Vec3 baseRoot = track.translationKeys[0].value;
                        for (auto& k : track.translationKeys) {
                            k.value.x = baseRoot.x;
                            k.value.z = baseRoot.z;
                        }
                    }
                }
                outAnimations.push_back(walkClip);
            }

            // 2. "Idle" animation (calm ready stance sampled from base pose with subtle breathing)
            if (!hasIdle) {
                AnimationClip idleClip;
                idleClip.name = "Idle";
                idleClip.duration = 2.4f;

                for (const auto& track : primaryClip.tracks) {
                    BoneAnimationTrack idleTrack;
                    idleTrack.boneIndex = track.boneIndex;
                    idleTrack.boneName = track.boneName;

                    Quat baseRot = track.rotationKeys.empty() ? Quat::identity() : track.rotationKeys[0].value;
                    Vec3 basePos = track.translationKeys.empty() ? Vec3(0, 0, 0) : track.translationKeys[0].value;

                    if (track.boneIndex == bChest) {
                        idleTrack.rotationKeys = {
                            { 0.0f, baseRot },
                            { 1.2f, baseRot * Quat::fromEuler(-0.04f, 0.0f, 0.0f) },
                            { 2.4f, baseRot }
                        };
                    } else if (track.boneIndex == bHead) {
                        idleTrack.rotationKeys = {
                            { 0.0f, baseRot },
                            { 0.7f, baseRot * Quat::fromEuler(0.0f, 0.06f, 0.0f) },
                            { 1.7f, baseRot * Quat::fromEuler(0.0f, -0.06f, 0.0f) },
                            { 2.4f, baseRot }
                        };
                    } else {
                        idleTrack.rotationKeys = {
                            { 0.0f, baseRot },
                            { 2.4f, baseRot }
                        };
                    }

                    if (!track.translationKeys.empty()) {
                        idleTrack.translationKeys = {
                            { 0.0f, basePos },
                            { 2.4f, basePos }
                        };
                    }
                    idleClip.tracks.push_back(idleTrack);
                }
                outAnimations.push_back(idleClip);
            }

            // 3. "Shoot" animation (snappy recoil muzzle rise and recovery)
            if (!hasShoot) {
                AnimationClip shootClip;
                shootClip.name = "Shoot";
                shootClip.duration = 0.32f;

                for (const auto& track : primaryClip.tracks) {
                    BoneAnimationTrack sTrack;
                    sTrack.boneIndex = track.boneIndex;
                    sTrack.boneName = track.boneName;

                    Quat baseRot = track.rotationKeys.empty() ? Quat::identity() : track.rotationKeys[0].value;
                    Vec3 basePos = track.translationKeys.empty() ? Vec3(0, 0, 0) : track.translationKeys[0].value;

                    if (track.boneIndex == bArmR) {
                        sTrack.rotationKeys = {
                            { 0.0f, baseRot },
                            { 0.04f, baseRot * Quat::fromEuler(0.35f, -0.04f, 0.0f) },
                            { 0.14f, baseRot * Quat::fromEuler(0.12f, -0.01f, 0.0f) },
                            { 0.32f, baseRot }
                        };
                    } else if (track.boneIndex == bForeR) {
                        sTrack.rotationKeys = {
                            { 0.0f, baseRot },
                            { 0.04f, baseRot * Quat::fromEuler(0.22f, 0.0f, 0.0f) },
                            { 0.32f, baseRot }
                        };
                    } else if (track.boneIndex == bChest) {
                        sTrack.rotationKeys = {
                            { 0.0f, baseRot },
                            { 0.04f, baseRot * Quat::fromEuler(-0.10f, 0.0f, 0.0f) },
                            { 0.18f, baseRot * Quat::fromEuler(-0.02f, 0.0f, 0.0f) },
                            { 0.32f, baseRot }
                        };
                    } else if (track.boneIndex == bHead) {
                        sTrack.rotationKeys = {
                            { 0.0f, baseRot },
                            { 0.05f, baseRot * Quat::fromEuler(0.06f, 0.0f, 0.0f) },
                            { 0.32f, baseRot }
                        };
                    } else {
                        sTrack.rotationKeys = {
                            { 0.0f, baseRot },
                            { 0.32f, baseRot }
                        };
                    }

                    if (!track.translationKeys.empty()) {
                        sTrack.translationKeys = {
                            { 0.0f, basePos },
                            { 0.32f, basePos }
                        };
                    }
                    shootClip.tracks.push_back(sTrack);
                }
                outAnimations.push_back(shootClip);
            }
        }

        std::cout << "[glTF] Successfully loaded " << (isGLB ? "GLB binary" : "glTF") << " asset '" << path << "' with "
                  << outSkeleton->getBoneCount() << " bones, "
                  << allVertices.size() << " vertices, and "
                  << outAnimations.size() << " animation clips." << std::endl;
        return true;
    }

}
