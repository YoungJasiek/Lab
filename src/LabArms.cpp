#include "LabArms.h"
#include "LabRenderer.h"
#include "LabWeapon.h"
#include <cmath>
#include <algorithm>

namespace Lab {

    ViewModelArms::ViewModelArms() {
        _trailHistory.reserve(32);
        initGuardViewmodelRig();
    }

    static Vec3 rotateOffset(const Vec3& offset, const Vec3& rot) {
        float rx = rot.x * 3.14159265f / 180.0f;
        float ry = rot.y * 3.14159265f / 180.0f;
        float rz = rot.z * 3.14159265f / 180.0f;

        // Roll (Z)
        float x1 = offset.x * std::cos(rz) - offset.y * std::sin(rz);
        float y1 = offset.x * std::sin(rz) + offset.y * std::cos(rz);
        float z1 = offset.z;

        // Pitch (X)
        float y2 = y1 * std::cos(rx) - z1 * std::sin(rx);
        float z2 = y1 * std::sin(rx) + z1 * std::cos(rx);
        float x2 = x1;

        // Yaw (Y)
        float x3 = x2 * std::cos(ry) + z2 * std::sin(ry);
        float z3 = -x2 * std::sin(ry) + z2 * std::cos(ry);
        float y3 = y2;

        return Vec3(x3, y3, z3);
    }

    static Vec3 cylinderRotFromDir(const Vec3& dir) {
        Vec3 n = dir.normalized();
        float rx = std::acos(std::clamp(n.y, -1.0f, 1.0f)) * 180.0f / 3.14159265f;
        float ry = std::atan2(n.x, n.z) * 180.0f / 3.14159265f;
        return Vec3(rx, ry, 0.0f);
    }

    static Vec3 cylinderRotFromEulerZ(const Vec3& eulerDeg) {
        Vec3 fwd = rotateOffset(Vec3(0.0f, 0.0f, -1.0f), eulerDeg);
        return cylinderRotFromDir(fwd);
    }

    void ViewModelArms::initGuardViewmodelRig() {
        _skeleton = std::make_shared<Skeleton>();

        // Left Arm Hierarchy (0..10 + 23) - compatible with Mixamo / ValveBiped / glTF 3D arm rigs
        int bClavL   = _skeleton->addBone("Clavicle_L", -1, { -0.42f, -0.46f, -0.10f });
        int bUpperL  = _skeleton->addBone("UpperArm_L", bClavL, { 0.04f, 0.04f, -0.12f });
        int bForeL   = _skeleton->addBone("Forearm_L", bUpperL, { 0.14f, 0.16f, -0.18f });
        int bHandL   = _skeleton->addBone("Hand_L", bForeL, { 0.16f, 0.16f, -0.12f });
        int bThumb1L = _skeleton->addBone("Thumb1_L", bHandL, { -0.022f, 0.028f, -0.032f });
        _skeleton->addBone("Thumb2_L", bThumb1L, { -0.012f, 0.012f, -0.022f });
        int bIdx1L   = _skeleton->addBone("Index1_L", bHandL, { 0.015f, 0.014f, -0.038f });
        _skeleton->addBone("Index2_L", bIdx1L, { 0.008f, 0.006f, -0.024f });
        _skeleton->addBone("Middle1_L", bHandL, { 0.015f, 0.000f, -0.042f });
        _skeleton->addBone("Ring1_L", bHandL, { 0.014f, -0.014f, -0.038f });
        _skeleton->addBone("Pinky1_L", bHandL, { 0.012f, -0.028f, -0.034f });

        // Right Arm Hierarchy (11..22)
        int bClavR   = _skeleton->addBone("Clavicle_R", -1, { 0.42f, -0.46f, -0.10f });
        int bUpperR  = _skeleton->addBone("UpperArm_R", bClavR, { -0.04f, 0.04f, -0.12f });
        int bForeR   = _skeleton->addBone("Forearm_R", bUpperR, { -0.06f, 0.08f, -0.14f });
        int bHandR   = _skeleton->addBone("Hand_R", bForeR, { -0.075f, 0.06f, -0.14f });
        _skeleton->addBone("Socket_Weapon", bHandR, { 0.015f, 0.065f, 0.04f });
        int bThumb1R = _skeleton->addBone("Thumb1_R", bHandR, { -0.038f, 0.012f, -0.028f });
        _skeleton->addBone("Thumb2_R", bThumb1R, { -0.014f, 0.010f, -0.020f });
        int bIdx1R   = _skeleton->addBone("Index1_R", bHandR, { 0.022f, -0.005f, -0.055f });
        _skeleton->addBone("Index2_R", bIdx1R, { 0.004f, -0.002f, -0.026f });
        _skeleton->addBone("Middle1_R", bHandR, { 0.012f, -0.024f, -0.052f });
        _skeleton->addBone("Ring1_R", bHandR, { 0.012f, -0.043f, -0.050f });
        _skeleton->addBone("Pinky1_R", bHandR, { 0.010f, -0.062f, -0.046f });
        _skeleton->addBone("WristComm_L", bForeL, { 0.12f, 0.14f, -0.10f });

        _skeleton->computeBindPose();
        _localTransforms.resize(_skeleton->getBoneCount(), Mat4::identity());
        for (size_t i = 0; i < _skeleton->getBoneCount(); ++i) {
            if (const Bone* b = _skeleton->getBone(static_cast<int>(i))) {
                _localTransforms[i] = b->localBindMatrix;
            }
        }
        _skeleton->evaluate(_localTransforms, _globalTransforms, _skinMatrices);
    }

    void ViewModelArms::updateRigPose(const Vec3& rightElbow, const Vec3& rightWrist, const Vec3& rightRot,
                                      const Vec3& leftElbow, const Vec3& leftWrist, const Vec3& leftRot,
                                      WeaponID /*weaponId*/) {
        if (!_skeleton) return;
        size_t count = _skeleton->getBoneCount();
        _localTransforms.resize(count, Mat4::identity());
        for (size_t i = 0; i < count; ++i) {
            if (const Bone* b = _skeleton->getBone(static_cast<int>(i))) {
                _localTransforms[i] = b->localBindMatrix;
            }
        }

        auto degToRad = [](float d) { return d * 3.14159265f / 180.0f; };

        int idxUpperL = _skeleton->findBoneIndex("UpperArm_L");
        int idxForeL  = _skeleton->findBoneIndex("Forearm_L");
        int idxHandL  = _skeleton->findBoneIndex("Hand_L");
        int idxUpperR = _skeleton->findBoneIndex("UpperArm_R");
        int idxForeR  = _skeleton->findBoneIndex("Forearm_R");
        int idxHandR  = _skeleton->findBoneIndex("Hand_R");

        Vec3 lDir = (leftWrist - leftElbow);
        Vec3 rDir = (rightWrist - rightElbow);

        if (idxUpperL >= 0) _localTransforms[idxUpperL] = makeTransform(leftElbow, Quat::identity(), Vec3(1, 1, 1));
        if (idxForeL >= 0)  _localTransforms[idxForeL]  = makeTransform(lDir * 0.5f, Quat::identity(), Vec3(1, 1, 1));
        if (idxHandL >= 0) {
            Quat qL = Quat::fromEuler(degToRad(leftRot.x), degToRad(leftRot.y), degToRad(leftRot.z));
            _localTransforms[idxHandL] = makeTransform(lDir * 0.5f, qL, Vec3(1, 1, 1));
        }

        if (idxUpperR >= 0) _localTransforms[idxUpperR] = makeTransform(rightElbow, Quat::identity(), Vec3(1, 1, 1));
        if (idxForeR >= 0)  _localTransforms[idxForeR]  = makeTransform(rDir * 0.5f, Quat::identity(), Vec3(1, 1, 1));
        if (idxHandR >= 0) {
            Quat qR = Quat::fromEuler(degToRad(rightRot.x), degToRad(rightRot.y), degToRad(rightRot.z));
            _localTransforms[idxHandR] = makeTransform(rDir * 0.5f, qR, Vec3(1, 1, 1));
        }

        _skeleton->evaluate(_localTransforms, _globalTransforms, _skinMatrices);
    }

    Mat4 ViewModelArms::getWeaponSocketMatrix(const Mat4& rootTransform) const {
        if (_skeleton) {
            return _skeleton->getSocketTransform("Socket_Weapon", rootTransform, _globalTransforms);
        }
        return rootTransform;
    }

    void ViewModelArms::drawForearmSleeve(const Vec3& elbowStart, const Vec3& wristEnd,
                                         const Vec3& wristRot, bool isLeft, Texture* texture) {
        (void)wristRot;
        Vec3 dir = wristEnd - elbowStart;
        float armLength = dir.length();
        if (armLength < 0.001f) return;

        Vec3 armForward = dir.normalized();
        Vec3 upperSleevePos = elbowStart + armForward * (armLength * 0.28f);
        Vec3 midArm = elbowStart + armForward * (armLength * 0.52f);
        Vec3 cuffFoldPos = elbowStart + armForward * (armLength * 0.72f);
        Vec3 gauntletPos = elbowStart + armForward * (armLength * 0.86f);

        float pitch = std::asin(std::clamp(armForward.y, -1.0f, 1.0f)) * 180.0f / 3.14159265f;
        float yaw = std::atan2(-armForward.x, -armForward.z) * 180.0f / 3.14159265f;
        Vec3 sleeveRot = Vec3(pitch, yaw, isLeft ? 12.0f : -12.0f);
        Vec3 cylRot = cylinderRotFromDir(armForward);

        // Site-14 Security Guard Uniform Palette (matching guard.glb navy-slate fatigues & Kevlar bracers)
        Vec3 guardNavyFabric = { 0.19f, 0.26f, 0.36f };
        Vec3 guardCuffWeave  = { 0.22f, 0.30f, 0.42f };
        Vec3 kevlarBracer    = { 0.13f, 0.15f, 0.18f };
        Vec3 steelTrim       = { 0.48f, 0.52f, 0.58f };
        Vec3 wristCuffLeather = { 0.10f, 0.11f, 0.13f };

        // 1. Guard Navy Tactical Uniform Sleeve (Upper & Mid Forearm)
        Renderer::drawCylinder(upperSleevePos, cylRot, { 0.102f, armLength * 0.52f, 0.096f }, guardNavyFabric, texture, true);
        Renderer::drawCylinder(midArm, cylRot, { 0.094f, armLength * 0.58f, 0.088f }, guardNavyFabric, texture, true);

        // 2. Rolled Guard Uniform Cuff Fold Ring
        Renderer::drawCylinder(cuffFoldPos, cylRot, { 0.099f, armLength * 0.12f, 0.093f }, guardCuffWeave, texture, true);

        // 3. Guard Dorsal Kevlar Forearm Armor Bracer + Metallic Trim Rings
        Vec3 plateLocal = Vec3(isLeft ? -0.012f : 0.012f, 0.018f, 0.0f);
        Vec3 platePos = midArm + rotateOffset(plateLocal, sleeveRot);
        Renderer::drawCylinder(platePos, cylRot, { 0.086f, armLength * 0.48f, 0.064f }, kevlarBracer, texture, true);
        Renderer::drawCylinder(platePos + armForward * (armLength * 0.18f), cylRot, { 0.089f, armLength * 0.04f, 0.067f }, steelTrim, nullptr, true);
        Renderer::drawCylinder(platePos - armForward * (armLength * 0.18f), cylRot, { 0.089f, armLength * 0.04f, 0.067f }, steelTrim, nullptr, true);

        // 4. Tactical Guard Wrist Cuff & Cylindrical Security Chronometer / Comm Dial (zero cubes!)
        Renderer::drawCylinder(gauntletPos, cylRot, { 0.092f, armLength * 0.22f, 0.086f }, wristCuffLeather, nullptr, true);
        if (isLeft) {
            Vec3 watchLocal = Vec3(-0.026f, 0.042f, 0.0f);
            Vec3 watchPos = gauntletPos + rotateOffset(watchLocal, sleeveRot);
            Renderer::drawCylinder(watchPos, sleeveRot, { 0.034f, 0.014f, 0.034f }, steelTrim, nullptr, true);
            Renderer::drawCylinder(watchPos + rotateOffset(Vec3(0.0f, 0.005f, 0.0f), sleeveRot), sleeveRot, { 0.026f, 0.008f, 0.026f }, Vec3(0.18f, 0.72f, 0.95f), nullptr, false);
        }
    }

    void ViewModelArms::drawTacticalGlove(const Vec3& wristPos, const Vec3& wristRot,
                                         bool isLeft, WeaponID weaponId) {
        Vec3 gloveLeather = { 0.14f, 0.15f, 0.18f };
        Vec3 carbonPlate  = { 0.08f, 0.09f, 0.11f };
        Vec3 fingerJoint  = { 0.12f, 0.13f, 0.15f };
        Vec3 fingerTipCol = { 0.10f, 0.11f, 0.13f };

        // 1. Rounded Palm & Back of Hand
        Vec3 palmLocal = isLeft ? Vec3(0.01f, 0.015f, -0.038f) : Vec3(-0.01f, 0.015f, -0.038f);
        Vec3 palmPos = wristPos + rotateOffset(palmLocal, wristRot);
        Renderer::drawCylinder(palmPos, cylinderRotFromEulerZ(wristRot), { 0.074f, 0.064f, 0.048f }, gloveLeather, nullptr, true);

        // 2. Reinforced Carbon-Fiber Knuckle Guard
        Vec3 knuckleLocal = isLeft ? Vec3(0.005f, 0.030f, -0.048f) : Vec3(-0.005f, 0.030f, -0.048f);
        Vec3 knucklePos = wristPos + rotateOffset(knuckleLocal, wristRot);
        Renderer::drawCylinder(knucklePos, cylinderRotFromEulerZ(wristRot + Vec3(0.0f, 90.0f, 0.0f)), { 0.036f, 0.068f, 0.022f }, carbonPlate, nullptr, true);

        // 3. Multi-joint Articulated Fingers (Proximal + Distal Phalanges)
        if (!isLeft) {
            // Right HandThumb (2 segments)
            Vec3 thumb1Local = Vec3(-0.038f, 0.012f, -0.028f);
            Vec3 thumb1Pos = wristPos + rotateOffset(thumb1Local, wristRot);
            Renderer::drawCylinder(thumb1Pos, cylinderRotFromEulerZ(wristRot + Vec3(0, -25.0f, 30.0f)), { 0.023f, 0.032f, 0.022f }, gloveLeather, nullptr, true);
            Vec3 thumb2Local = Vec3(-0.048f, 0.020f, -0.048f);
            Vec3 thumb2Pos = wristPos + rotateOffset(thumb2Local, wristRot);
            Renderer::drawCylinder(thumb2Pos, cylinderRotFromEulerZ(wristRot + Vec3(8.0f, -38.0f, 25.0f)), { 0.020f, 0.026f, 0.019f }, fingerTipCol, nullptr, true);

            // Right Trigger Index Finger (Proximal + Distal wrapped onto trigger)
            Vec3 index1Local = Vec3(0.022f, -0.005f, -0.062f);
            Vec3 index1Pos = wristPos + rotateOffset(index1Local, wristRot);
            Renderer::drawCylinder(index1Pos, cylinderRotFromEulerZ(wristRot + Vec3(14.0f, -8.0f, 0.0f)), { 0.020f, 0.034f, 0.019f }, fingerJoint, nullptr, true);
            Vec3 index2Local = Vec3(0.018f, -0.010f, -0.088f);
            Vec3 index2Pos = wristPos + rotateOffset(index2Local, wristRot);
            Renderer::drawCylinder(index2Pos, cylinderRotFromEulerZ(wristRot + Vec3(28.0f, -24.0f, 0.0f)), { 0.018f, 0.026f, 0.017f }, fingerTipCol, nullptr, true);

            // Right Middle, Ring, Pinky Grip Fingers
            for (int f = 0; f < 3; ++f) {
                float yOff = -0.024f - f * 0.019f;
                Vec3 grip1Local = Vec3(0.012f, yOff, -0.048f);
                Vec3 grip1Pos = wristPos + rotateOffset(grip1Local, wristRot);
                Renderer::drawCylinder(grip1Pos, cylinderRotFromEulerZ(wristRot + Vec3(22.0f, 15.0f, -10.0f)), { 0.021f, 0.034f, 0.019f }, fingerJoint, nullptr, true);

                Vec3 grip2Local = Vec3(-0.004f, yOff - 0.004f, -0.066f);
                Vec3 grip2Pos = wristPos + rotateOffset(grip2Local, wristRot);
                Renderer::drawCylinder(grip2Pos, cylinderRotFromEulerZ(wristRot + Vec3(35.0f, 32.0f, -14.0f)), { 0.018f, 0.026f, 0.017f }, fingerTipCol, nullptr, true);
            }
        } else {
            if (weaponId == WeaponID::Pistol) {
                Vec3 cupLocal = Vec3(0.00f, -0.025f, -0.035f);
                Vec3 cupPos = wristPos + rotateOffset(cupLocal, wristRot);
                Renderer::drawCylinder(cupPos, cylinderRotFromEulerZ(wristRot + Vec3(-12.0f, 15.0f, 18.0f)), { 0.064f, 0.056f, 0.028f }, fingerJoint, nullptr, true);
            } else if (weaponId == WeaponID::Pipe) {
                for (int f = 0; f < 4; ++f) {
                    float yOff = 0.018f - f * 0.017f;
                    Vec3 gripLocal = Vec3(-0.015f, yOff, -0.042f);
                    Vec3 gripPos = wristPos + rotateOffset(gripLocal, wristRot);
                    Renderer::drawCylinder(gripPos, cylinderRotFromEulerZ(wristRot + Vec3(18.0f, 25.0f, -15.0f)), { 0.020f, 0.040f, 0.019f }, fingerJoint, nullptr, true);
                }
            } else {
                for (int f = 0; f < 4; ++f) {
                    float zOff = -0.025f - f * 0.017f;
                    Vec3 gripLocal = Vec3(0.015f, 0.012f, zOff);
                    Vec3 gripPos = wristPos + rotateOffset(gripLocal, wristRot);
                    Renderer::drawCylinder(gripPos, cylinderRotFromEulerZ(wristRot + Vec3(25.0f, 0.0f, -20.0f)), { 0.020f, 0.040f, 0.019f }, fingerJoint, nullptr, true);
                }
                Vec3 thumbLocal = Vec3(-0.022f, 0.028f, -0.042f);
                Vec3 thumbPos = wristPos + rotateOffset(thumbLocal, wristRot);
                Renderer::drawCylinder(thumbPos, cylinderRotFromEulerZ(wristRot + Vec3(0.0f, 30.0f, -15.0f)), { 0.022f, 0.040f, 0.020f }, gloveLeather, nullptr, true);
            }
        }
    }

    void ViewModelArms::renderMeleeTrail(const Vec3& pipeTipPos, const Vec3& pipeBasePos, float alpha) {
        if (alpha <= 0.01f) return;
        Vec3 trailColor = { 0.65f * alpha, 0.85f * alpha, 1.0f * alpha };
        Renderer::drawAdditiveTracer(pipeBasePos, pipeTipPos, trailColor, 0.035f * alpha);
    }

    void ViewModelArms::render(const Vec3& weaponBasePos, const Vec3& weaponRot,
                              WeaponID weaponId,
                              Texture* sleeveTexture,
                              const Vec3* rightSocketOffset,
                              const Vec3* rightSocketRot,
                              const Vec3* leftSocketOffset,
                              const Vec3* leftSocketRot,
                              const Vec3* supportHandOffset,
                              const Vec3* supportHandRot) {
        if (!sleeveTexture && !_triedLoadingTex) {
            _triedLoadingTex = true;
            auto candidate = std::make_unique<Texture>("assets/textures/guard.bmp");
            if (candidate && candidate->getId() != 0) {
                _guardSleeveTex = std::move(candidate);
            }
        }
        Texture* activeSleeveTex = sleeveTexture ? sleeveTexture : _guardSleeveTex.get();

        // Shoulder/Elbow roots (where Guard arms emerge from lower screen corners)
        Vec3 rightElbowStart = { 0.38f, -0.42f, -0.22f };
        Vec3 leftElbowStart  = { -0.38f, -0.42f, -0.22f };

        // 1. Right Arm / Trigger Hand Socket:
        Vec3 defaultRightPos = Vec3(-0.015f, -0.065f, -0.04f);
        Vec3 defaultRightRot = Vec3(8.0f, 0.0f, -4.0f);
        Vec3 rightWristPos = weaponBasePos + (rightSocketOffset ? *rightSocketOffset : defaultRightPos);
        Vec3 rightWristRot = weaponRot + (rightSocketRot ? *rightSocketRot : defaultRightRot);

        // 2. Left Arm / Support Hand Socket:
        Vec3 leftWristPos;
        Vec3 leftWristRot;

        if (supportHandOffset) {
            leftWristPos = weaponBasePos + *supportHandOffset;
            leftWristRot = weaponRot + (supportHandRot ? *supportHandRot : Vec3(0.0f, 0.0f, 0.0f));
        } else if (leftSocketOffset) {
            leftWristPos = weaponBasePos + *leftSocketOffset;
            leftWristRot = weaponRot + (leftSocketRot ? *leftSocketRot : Vec3(20.0f, 10.0f, -18.0f));
        } else if (weaponId == WeaponID::Pipe) {
            leftWristPos = weaponBasePos + Vec3(-0.01f, -0.125f, -0.02f);
            leftWristRot = weaponRot + Vec3(12.0f, -5.0f, 6.0f);
        } else if (weaponId == WeaponID::Pistol) {
            leftWristPos = weaponBasePos + Vec3(-0.055f, -0.105f, -0.035f);
            leftWristRot = weaponRot + Vec3(14.0f, -12.0f, 15.0f);
        } else {
            float fwdReach = (weaponId == WeaponID::Shotgun || weaponId == WeaponID::Minigun) ? -0.26f : -0.34f;
            leftWristPos = weaponBasePos + Vec3(-0.065f, -0.060f, fwdReach);
            leftWristRot = weaponRot + Vec3(22.0f, 12.0f, -22.0f);
        }

        // Update 24-bone Guard Viewmodel skeletal rig
        updateRigPose(rightElbowStart, rightWristPos, rightWristRot,
                      leftElbowStart, leftWristPos, leftWristRot,
                      weaponId);

        drawForearmSleeve(rightElbowStart, rightWristPos, rightWristRot, false, activeSleeveTex);
        drawTacticalGlove(rightWristPos, rightWristRot, false, weaponId);

        drawForearmSleeve(leftElbowStart, leftWristPos, leftWristRot, true, activeSleeveTex);
        drawTacticalGlove(leftWristPos, leftWristRot, true, weaponId);
    }

} // namespace Lab
