#pragma once
#include <vector>
#include <array>
#include <string>
#include <memory>
#include "LabMath.h"
#include "LabAnim.h"
#include "LabSkeletal.h"

namespace Lab {

    class Texture;
    enum class WeaponID : int;

    // Tactical first-person Guard Viewmodel arms and skeletal rig system
    // Provides 24-bone articulated FPP kinematics compatible with 3D models,
    // Site-14 Security Guard sleeve & Kevlar bracer styling, and multi-joint phalanges.
    class ViewModelArms {
    public:
        ViewModelArms();
        ~ViewModelArms() = default;

        // Render both arms and hands relative to active weapon transform (with optional custom socket overrides)
        void render(const Vec3& weaponBasePos, const Vec3& weaponRot,
                    WeaponID weaponId,
                    Texture* sleeveTexture = nullptr,
                    const Vec3* rightSocketOffset = nullptr,
                    const Vec3* rightSocketRot = nullptr,
                    const Vec3* leftSocketOffset = nullptr,
                    const Vec3* leftSocketRot = nullptr,
                    const Vec3* supportHandOffset = nullptr,
                    const Vec3* supportHandRot = nullptr);

        // Render melee motion ribbon during pipe slash
        void renderMeleeTrail(const Vec3& pipeTipPos, const Vec3& pipeBasePos, float alpha);

        // Skeletal rig inspection & 3D model compatibility accessors
        std::shared_ptr<Skeleton> getSkeleton() const { return _skeleton; }
        const std::vector<Mat4>& getGlobalTransforms() const { return _globalTransforms; }
        const std::vector<Mat4>& getSkinMatrices() const { return _skinMatrices; }
        Mat4 getWeaponSocketMatrix(const Mat4& rootTransform = Mat4::identity()) const;

    private:
        void initGuardViewmodelRig();
        void updateRigPose(const Vec3& rightElbow, const Vec3& rightWrist, const Vec3& rightRot,
                           const Vec3& leftElbow, const Vec3& leftWrist, const Vec3& leftRot,
                           WeaponID weaponId);

        // Draw a single anatomical Guard forearm sleeve from elbow joint to wrist joint
        void drawForearmSleeve(const Vec3& elbowStart, const Vec3& wristEnd,
                               const Vec3& wristRot, bool isLeft, Texture* texture);

        // Draw a Guard tactical combat glove (palm, knuckle armor plate, 5 multi-joint fingers)
        void drawTacticalGlove(const Vec3& wristPos, const Vec3& wristRot,
                               bool isLeft, WeaponID weaponId);

        std::shared_ptr<Skeleton> _skeleton;
        std::vector<Mat4> _localTransforms;
        std::vector<Mat4> _globalTransforms;
        std::vector<Mat4> _skinMatrices;
        std::unique_ptr<Texture> _guardSleeveTex;
        bool _triedLoadingTex = false;

        // Recent history of pipe tip positions for motion ribbon
        std::vector<Vec3> _trailHistory;
        float _trailTimer = 0.0f;
    };

} // namespace Lab
