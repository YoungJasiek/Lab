#pragma once
#include <vector>
#include <string>
#include <unordered_map>
#include <memory>
#include "LabMath.h"

namespace Lab {

    class Shader;
    class Texture;

    // --- Skinned Vertex with 4-bone influence ---
    struct SkinnedVertex {
        Vec3 position;
        Vec3 normal;
        Vec2 texCoords;
        Vec3 color;
        unsigned int boneIDs[4];
        float boneWeights[4];

        SkinnedVertex(const Vec3& p = { 0, 0, 0 },
                      const Vec3& n = { 0, 1, 0 },
                      const Vec2& uv = { 0, 0 },
                      const Vec3& c = { 1, 1, 1 },
                      const unsigned int* ids = nullptr,
                      const float* weights = nullptr)
            : position(p), normal(n), texCoords(uv), color(c) {
            for (int i = 0; i < 4; ++i) {
                boneIDs[i] = ids ? ids[i] : 0;
                boneWeights[i] = weights ? weights[i] : (i == 0 ? 1.0f : 0.0f);
            }
        }
    };

    // --- Skinned Mesh (GPU VAO with DSA / Core Profile layout) ---
    class SkinnedMesh {
    public:
        SkinnedMesh(const std::vector<SkinnedVertex>& vertices, const std::vector<unsigned int>& indices);
        ~SkinnedMesh();

        SkinnedMesh(const SkinnedMesh&) = delete;
        SkinnedMesh& operator=(const SkinnedMesh&) = delete;
        SkinnedMesh(SkinnedMesh&& other) noexcept;
        SkinnedMesh& operator=(SkinnedMesh&& other) noexcept;

        void draw() const;
        int getIndexCount() const { return _indexCount; }

        Vec3 getMinBounds() const { return _minBounds; }
        Vec3 getMaxBounds() const { return _maxBounds; }
        float getHeight() const { return _maxBounds.y - _minBounds.y; }
        float getBaseScale(float targetHeight = 1.85f) const {
            float h = getHeight();
            if (h <= 0.001f) return 1.0f;
            return targetHeight / h;
        }

    private:
        unsigned int _vao = 0;
        unsigned int _vbo = 0;
        unsigned int _ebo = 0;
        int _indexCount = 0;
        Vec3 _minBounds{0.0f, 0.0f, 0.0f};
        Vec3 _maxBounds{0.0f, 0.0f, 0.0f};
    };

    // --- Bone Definition ---
    struct Bone {
        std::string name;
        int index = -1;
        int parentIndex = -1;
        std::vector<int> children;

        Vec3 bindPos = { 0, 0, 0 };
        Quat bindRot = Quat::identity();
        Vec3 bindScale = { 1, 1, 1 };

        Mat4 localBindMatrix = Mat4::identity();
        Mat4 inverseBindMatrix = Mat4::identity();
    };

    // --- Skeletal Rig & Hierarchy ---
    class Skeleton {
    public:
        Skeleton() = default;

        int addBone(const std::string& name, int parentIndex = -1,
                    const Vec3& localPos = { 0, 0, 0 },
                    const Quat& localRot = Quat::identity(),
                    const Vec3& localScale = { 1, 1, 1 });

        int findBoneIndex(const std::string& name) const;
        const Bone* getBone(int index) const;
        const Bone* getBone(const std::string& name) const;
        size_t getBoneCount() const { return _bones.size(); }

        void computeBindPose();
        void setInverseBindMatrix(int boneIndex, const Mat4& invBind);

        // Evaluates global world matrices and GPU skin matrices from per-bone local transforms
        void evaluate(const std::vector<Mat4>& localTransforms,
                      std::vector<Mat4>& outGlobalTransforms,
                      std::vector<Mat4>& outSkinMatrices) const;

        // --- Bone Socket Attachment for rigid STL weapon models ---
        // Returns world transformation matrix for attaching an STL weapon to a bone
        Mat4 getSocketTransform(int boneIndex,
                                const Mat4& modelTransform,
                                const std::vector<Mat4>& globalTransforms,
                                const Mat4& socketOffset = Mat4::identity()) const;

        Mat4 getSocketTransform(const std::string& boneName,
                                const Mat4& modelTransform,
                                const std::vector<Mat4>& globalTransforms,
                                const Mat4& socketOffset = Mat4::identity()) const;

    private:
        std::vector<Bone> _bones;
        std::unordered_map<std::string, int> _nameToIndex;
    };

    // --- Keyframe Animation Track ---
    struct KeyframeVec3 {
        float time = 0.0f;
        Vec3 value;
    };

    struct KeyframeQuat {
        float time = 0.0f;
        Quat value;
    };

    struct BoneAnimationTrack {
        int boneIndex = -1;
        std::string boneName;
        std::vector<KeyframeVec3> translationKeys;
        std::vector<KeyframeQuat> rotationKeys;
        std::vector<KeyframeVec3> scaleKeys;

        Vec3 sampleTranslation(float time) const;
        Quat sampleRotation(float time) const;
        Vec3 sampleScale(float time) const;
    };

    // --- Animation Clip ---
    class AnimationClip {
    public:
        std::string name;
        float duration = 0.0f; // in seconds
        std::vector<BoneAnimationTrack> tracks;

        void sample(float time, bool loop, const Skeleton& skeleton, std::vector<Mat4>& outLocalTransforms) const;
    };

    // --- Animation Controller (StateMachine & Blending) ---
    class Animator {
    public:
        Animator() = default;

        void setSkeleton(std::shared_ptr<Skeleton> skeleton);
        std::shared_ptr<Skeleton> getSkeleton() const { return _skeleton; }

        void addClip(const AnimationClip& clip);
        bool hasClip(const std::string& name) const;
        const AnimationClip* getClip(const std::string& name) const;

        void playAnimation(const std::string& name, bool loop = true, float blendDuration = 0.15f);
        void update(float dt);

        const std::vector<Mat4>& getSkinMatrices() const { return _skinMatrices; }
        const std::vector<Mat4>& getGlobalTransforms() const { return _globalTransforms; }

        const std::string& getCurrentAnimationName() const { return _currentClipName; }
        float getCurrentTime() const { return _currentTime; }
        float getProgress() const;

        // Attaches an STL weapon model to a bone socket on this animated character
        Mat4 getSocketTransform(const std::string& boneName,
                                const Mat4& modelTransform,
                                const Mat4& socketOffset = Mat4::identity()) const;

    private:
        std::shared_ptr<Skeleton> _skeleton;
        std::unordered_map<std::string, AnimationClip> _clips;

        std::string _currentClipName;
        std::string _previousClipName;
        float _currentTime = 0.0f;
        float _previousTime = 0.0f;
        float _blendTimer = 0.0f;
        float _blendDuration = 0.0f;
        bool _looping = true;

        std::vector<Mat4> _globalTransforms;
        std::vector<Mat4> _skinMatrices;
    };

    // --- glTF 2.0 Loader & Procedural Rig Generator ---
    class GLTFLoader {
    public:
        // Loads a standard glTF 2.0 / GLB file
        static bool load(const std::string& path,
                         std::shared_ptr<Skeleton>& outSkeleton,
                         std::vector<AnimationClip>& outAnimations,
                         std::unique_ptr<SkinnedMesh>& outMesh);

        // Procedural generator for a fully rigged combat humanoid soldier/bot
        // with Idle, Walk, Shoot, Melee_Swing, and Inspect animations + weapon bone socket
        static void createProceduralCombatBot(std::shared_ptr<Skeleton>& outSkeleton,
                                              std::vector<AnimationClip>& outAnimations,
                                              std::unique_ptr<SkinnedMesh>& outMesh);
    };

}
