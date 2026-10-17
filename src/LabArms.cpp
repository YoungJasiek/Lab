#include "LabArms.h"
#include "LabRenderer.h"
#include "LabWeapon.h"
#include <cmath>
#include <algorithm>

namespace Lab {

    ViewModelArms::ViewModelArms() {
        _trailHistory.reserve(32);
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

    void ViewModelArms::drawForearmSleeve(const Vec3& elbowStart, const Vec3& wristEnd,
                                         const Vec3& wristRot, bool isLeft, Texture* texture) {
        (void)wristRot;
        Vec3 dir = wristEnd - elbowStart;
        float armLength = dir.length();
        if (armLength < 0.001f) return;

        Vec3 armForward = dir.normalized();
        Vec3 midArm = elbowStart + armForward * (armLength * 0.42f);
        Vec3 gauntletPos = elbowStart + armForward * (armLength * 0.82f);

        float pitch = std::asin(std::clamp(armForward.y, -1.0f, 1.0f)) * 180.0f / 3.14159265f;
        float yaw = std::atan2(-armForward.x, -armForward.z) * 180.0f / 3.14159265f;
        Vec3 sleeveRot = Vec3(pitch, yaw, isLeft ? 12.0f : -12.0f);
        Vec3 cylRot = cylinderRotFromDir(armForward);

        // 1. Base Cylindrical Forearm Sleeve (HEV / Cryo-suit fabric)
        Vec3 suitColor = { 0.16f, 0.19f, 0.23f };
        Vec3 sleeveCylSize = { 0.094f, armLength * 0.84f, 0.088f };
        Renderer::drawCylinder(midArm, cylRot, sleeveCylSize, suitColor, texture, true);

        // 2. Forearm Armor Reinforcement Plate (curved composite shell)
        Vec3 plateColor = { 0.24f, 0.27f, 0.32f };
        Vec3 plateLocal = Vec3(isLeft ? -0.012f : 0.012f, 0.018f, 0.0f);
        Vec3 platePos = midArm + rotateOffset(plateLocal, sleeveRot);
        Vec3 plateCylSize = { 0.086f, armLength * 0.54f, 0.064f };
        Renderer::drawCylinder(platePos, cylRot, plateCylSize, plateColor, texture, true);

        // 3. Tactical Cylindrical Wrist Gauntlet Cuff
        Vec3 gauntletColor = { 0.11f, 0.13f, 0.16f };
        Vec3 gauntletCylSize = { 0.098f, armLength * 0.28f, 0.092f };
        Renderer::drawCylinder(gauntletPos, cylRot, gauntletCylSize, gauntletColor, texture, true);

        // 4. Gauntlet Amber HEV Status Strip
        Vec3 ledLocal = Vec3(isLeft ? -0.030f : 0.030f, 0.044f, 0.0f);
        Vec3 ledPos = gauntletPos + rotateOffset(ledLocal, sleeveRot);
        Vec3 ledSize = { 0.028f, 0.010f, armLength * 0.16f };
        Vec3 ledColor = { 1.0f, 0.72f, 0.16f };
        Renderer::drawCube(ledPos, sleeveRot, ledSize, ledColor, nullptr, false);
    }

    void ViewModelArms::drawTacticalGlove(const Vec3& wristPos, const Vec3& wristRot,
                                         bool isLeft, WeaponID weaponId) {
        Vec3 gloveLeather = { 0.15f, 0.16f, 0.19f };
        Vec3 carbonPlate  = { 0.09f, 0.10f, 0.12f };
        Vec3 fingerJoint  = { 0.13f, 0.14f, 0.16f };

        // 1. Rounded Palm & Back of Hand
        Vec3 palmLocal = isLeft ? Vec3(0.01f, 0.015f, -0.038f) : Vec3(-0.01f, 0.015f, -0.038f);
        Vec3 palmPos = wristPos + rotateOffset(palmLocal, wristRot);
        Renderer::drawCylinder(palmPos, cylinderRotFromEulerZ(wristRot), { 0.074f, 0.064f, 0.048f }, gloveLeather, nullptr, true);

        // 2. Reinforced Carbon-Fiber Knuckle Guard
        Vec3 knuckleLocal = isLeft ? Vec3(0.005f, 0.030f, -0.048f) : Vec3(-0.005f, 0.030f, -0.048f);
        Vec3 knucklePos = wristPos + rotateOffset(knuckleLocal, wristRot);
        Renderer::drawCylinder(knucklePos, cylinderRotFromEulerZ(wristRot + Vec3(0.0f, 90.0f, 0.0f)), { 0.036f, 0.068f, 0.022f }, carbonPlate, nullptr, true);

        // 3. Cylindrical Articulated Fingers & Thumb
        if (!isLeft) {
            Vec3 thumbLocal = Vec3(-0.038f, 0.012f, -0.028f);
            Vec3 thumbPos = wristPos + rotateOffset(thumbLocal, wristRot);
            Renderer::drawCylinder(thumbPos, cylinderRotFromEulerZ(wristRot + Vec3(0, -25.0f, 30.0f)), { 0.023f, 0.044f, 0.022f }, gloveLeather, nullptr, true);

            Vec3 indexLocal = Vec3(0.022f, -0.005f, -0.075f);
            Vec3 indexPos = wristPos + rotateOffset(indexLocal, wristRot);
            Renderer::drawCylinder(indexPos, cylinderRotFromEulerZ(wristRot + Vec3(18.0f, -8.0f, 0.0f)), { 0.020f, 0.046f, 0.019f }, fingerJoint, nullptr, true);

            for (int f = 0; f < 3; ++f) {
                float yOff = -0.024f - f * 0.019f;
                Vec3 gripLocal = Vec3(0.012f, yOff, -0.052f);
                Vec3 gripPos = wristPos + rotateOffset(gripLocal, wristRot);
                Renderer::drawCylinder(gripPos, cylinderRotFromEulerZ(wristRot + Vec3(22.0f, 15.0f, -10.0f)), { 0.021f, 0.042f, 0.019f }, fingerJoint, nullptr, true);
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
        // Shoulder roots (where arms emerge from offscreen bottom corners)
        Vec3 rightElbowStart = { 0.38f, -0.42f, -0.22f };
        Vec3 leftElbowStart  = { -0.38f, -0.42f, -0.22f };

        // 1. Right Arm / Trigger Hand Socket:
        // Positioned firmly at the weapon's pistol grip / pipe handle
        Vec3 defaultRightPos = Vec3(-0.015f, -0.065f, -0.04f);
        Vec3 defaultRightRot = Vec3(8.0f, 0.0f, -4.0f);
        Vec3 rightWristPos = weaponBasePos + (rightSocketOffset ? *rightSocketOffset : defaultRightPos);
        Vec3 rightWristRot = weaponRot + (rightSocketRot ? *rightSocketRot : defaultRightRot);

        drawForearmSleeve(rightElbowStart, rightWristPos, rightWristRot, false, sleeveTexture);
        drawTacticalGlove(rightWristPos, rightWristRot, false, weaponId);

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
            // Two-handed baseball bat stance for Pipe
            leftWristPos = weaponBasePos + Vec3(-0.01f, -0.125f, -0.02f);
            leftWristRot = weaponRot + Vec3(12.0f, -5.0f, 6.0f);
        } else if (weaponId == WeaponID::Pistol) {
            // Two-handed Pistol Support Grip (cup and saucer under right fist)
            leftWristPos = weaponBasePos + Vec3(-0.055f, -0.105f, -0.035f);
            leftWristRot = weaponRot + Vec3(14.0f, -12.0f, 15.0f);
        } else {
            // Two-handed Rifle/Shotgun/RPG Foregrip Support
            float fwdReach = (weaponId == WeaponID::Shotgun || weaponId == WeaponID::Minigun) ? -0.26f : -0.34f;
            leftWristPos = weaponBasePos + Vec3(-0.065f, -0.060f, fwdReach);
            leftWristRot = weaponRot + Vec3(22.0f, 12.0f, -22.0f);
        }

        drawForearmSleeve(leftElbowStart, leftWristPos, leftWristRot, true, sleeveTexture);
        drawTacticalGlove(leftWristPos, leftWristRot, true, weaponId);
    }

} // namespace Lab
