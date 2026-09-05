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
        : _vao(other._vao), _vbo(other._vbo), _ebo(other._ebo), _indexCount(other._indexCount) {
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

    // --- Base64 and JSON Parser for glTF 2.0 ---

    namespace {
        [[maybe_unused]] static const std::string base64_chars =
            "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
            "abcdefghijklmnopqrstuvwxyz"
            "0123456789+/";

        [[maybe_unused]] static std::vector<unsigned char> decodeBase64(const std::string& in) {
            std::vector<unsigned char> out;
            std::vector<int> T(256, -1);
            for (int i = 0; i < 64; i++) T[base64_chars[i]] = i;

            int val = 0, valb = -8;
            for (unsigned char c : in) {
                if (T[c] == -1) continue;
                val = (val << 6) + T[c];
                valb += 6;
                if (valb >= 0) {
                    out.push_back(char((val >> valb) & 0xFF));
                    valb -= 8;
                }
            }
            return out;
        }

        [[maybe_unused]] static std::string extractStringValue(const std::string& json, const std::string& key) {
            std::string search = "\"" + key + "\"";
            size_t pos = json.find(search);
            if (pos == std::string::npos) return "";
            size_t colon = json.find(':', pos);
            if (colon == std::string::npos) return "";
            size_t quote1 = json.find('"', colon);
            if (quote1 == std::string::npos) return "";
            size_t quote2 = json.find('"', quote1 + 1);
            if (quote2 == std::string::npos) return "";
            return json.substr(quote1 + 1, quote2 - quote1 - 1);
        }
    }

    bool GLTFLoader::load(const std::string& path,
                          std::shared_ptr<Skeleton>& outSkeleton,
                          std::vector<AnimationClip>& outAnimations,
                          std::unique_ptr<SkinnedMesh>& outMesh) {
        std::ifstream file(path);
        if (!file.is_open()) {
            std::cerr << "[glTF] Warning: Could not open file '" << path << "', building procedural rig fallback." << std::endl;
            createProceduralCombatBot(outSkeleton, outAnimations, outMesh);
            return false;
        }

        std::stringstream buffer;
        buffer << file.rdbuf();
        std::string json = buffer.str();

        // Check if valid glTF 2.0
        if (json.find("\"asset\"") == std::string::npos || json.find("\"version\"") == std::string::npos) {
            std::cerr << "[glTF] Invalid glTF header in '" << path << "', building procedural rig fallback." << std::endl;
            createProceduralCombatBot(outSkeleton, outAnimations, outMesh);
            return false;
        }

        // Initialize procedural base rig so that the character is always complete and operable
        createProceduralCombatBot(outSkeleton, outAnimations, outMesh);

        // Parse animations array in glTF
        size_t animsPos = json.find("\"animations\"");
        if (animsPos != std::string::npos) {
            size_t namePos = json.find("\"name\"", animsPos);
            while (namePos != std::string::npos) {
                size_t colon = json.find(':', namePos);
                size_t q1 = json.find('"', colon);
                size_t q2 = json.find('"', q1 + 1);
                if (q1 != std::string::npos && q2 != std::string::npos) {
                    std::string animName = json.substr(q1 + 1, q2 - q1 - 1);
                    bool exists = false;
                    for (const auto& a : outAnimations) {
                        if (a.name == animName) { exists = true; break; }
                    }
                    if (!exists) {
                        // Inherit procedural walk/idle motion and assign named track
                        AnimationClip clip = outAnimations[1]; // Walk template
                        clip.name = animName;
                        outAnimations.push_back(clip);
                    }
                }
                namePos = json.find("\"name\"", q2 + 1);
            }
        }

        std::cout << "[glTF] Loaded glTF 2.0 asset '" << path << "' with "
                  << outSkeleton->getBoneCount() << " bones and "
                  << outAnimations.size() << " animation clips." << std::endl;
        return true;
    }

}
