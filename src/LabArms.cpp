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

    void ViewModelArms::drawForearmSleeve(const Vec3& elbowStart, const Vec3& wristEnd,
                                         const Vec3& wristRot, bool isLeft, Texture* texture) {
        (void)wristRot;
        Vec3 dir = wristEnd - elbowStart;
        float armLength = dir.length();
        if (armLength < 0.001f) return;

        Vec3 armForward = dir.normalized();
        // Calculate intermediate bone positions along the arm
        Vec3 midArm = elbowStart + armForward * (armLength * 0.42f);
        Vec3 gauntletPos = elbowStart + armForward * (armLength * 0.82f);

        // Arm orientation angles: aligned with -Z forward vector
        float pitch = std::asin(std::clamp(armForward.y, -1.0f, 1.0f)) * 180.0f / 3.14159265f;
        float yaw = std::atan2(-armForward.x, -armForward.z) * 180.0f / 3.14159265f;
        Vec3 sleeveRot = Vec3(pitch, yaw, isLeft ? 12.0f : -12.0f);

        // 1. Base Forearm Sleeve (Tactical cryo-suit fabric: dark slate)
        Vec3 suitColor = { 0.16f, 0.18f, 0.22f };
        Vec3 sleeveSize = { 0.095f, 0.088f, armLength * 0.84f };
        Renderer::drawCube(midArm, sleeveRot, sleeveSize, suitColor, texture, true);

        // 2. Forearm Armor Reinforcement Plate (Kevlar/Composite plate)
        Vec3 plateColor = { 0.22f, 0.25f, 0.30f };
        Vec3 plateLocal = Vec3(isLeft ? -0.015f : 0.015f, 0.030f, 0.0f);
        Vec3 platePos = midArm + rotateOffset(plateLocal, sleeveRot);
        Vec3 plateSize = { 0.082f, 0.026f, armLength * 0.55f };
        Renderer::drawCube(platePos, sleeveRot, plateSize, plateColor, nullptr, true);

        // 3. Tactical Cyber-Gauntlet / Wrist Comm Unit
        Vec3 gauntletColor = { 0.10f, 0.12f, 0.15f };
        Vec3 gauntletSize = { 0.098f, 0.090f, armLength * 0.28f };
        Renderer::drawCube(gauntletPos, sleeveRot, gauntletSize, gauntletColor, nullptr, true);

        // 4. Gauntlet Telemetry LED Diode Strip (Cyan Frozen-Life HUD link)
        Vec3 ledLocal = Vec3(isLeft ? -0.032f : 0.032f, 0.048f, 0.0f);
        Vec3 ledPos = gauntletPos + rotateOffset(ledLocal, sleeveRot);
        Vec3 ledSize = { 0.040f, 0.012f, armLength * 0.18f };
        Vec3 ledColor = { 0.20f, 0.85f, 1.0f };
        Renderer::drawCube(ledPos, sleeveRot, ledSize, ledColor, nullptr, false);
    }

    void ViewModelArms::drawTacticalGlove(const Vec3& wristPos, const Vec3& wristRot,
                                         bool isLeft, WeaponID weaponId, const WeaponAnimator& animator) {
        // Glove base palette: dark textured military leather & carbon composite
        Vec3 gloveLeather = { 0.15f, 0.16f, 0.19f };
        Vec3 carbonPlate  = { 0.08f, 0.08f, 0.10f };
        Vec3 fingerJoint  = { 0.12f, 0.13f, 0.15f };

        // 1. Palm & Back of Hand
        Vec3 palmLocal = isLeft ? Vec3(0.01f, 0.015f, -0.038f) : Vec3(-0.01f, 0.015f, -0.038f);
        Vec3 palmPos = wristPos + rotateOffset(palmLocal, wristRot);
        Vec3 palmSize = { 0.074f, 0.046f, 0.062f };
        Renderer::drawCube(palmPos, wristRot, palmSize, gloveLeather, nullptr, true);

        // 2. Reinforced Carbon-Fiber Knuckle Guard
        Vec3 knuckleLocal = isLeft ? Vec3(0.005f, 0.032f, -0.048f) : Vec3(-0.005f, 0.032f, -0.048f);
        Vec3 knucklePos = wristPos + rotateOffset(knuckleLocal, wristRot);
        Vec3 knuckleSize = { 0.068f, 0.018f, 0.035f };
        Renderer::drawCube(knucklePos, wristRot, knuckleSize, carbonPlate, nullptr, true);

        // 3. Articulated Fingers & Thumb
        if (!isLeft) {
            // RIGHT HAND (Trigger Hand):
            // Thumb curled onto the left side of the handle
            Vec3 thumbLocal = Vec3(-0.038f, 0.012f, -0.028f);
            Vec3 thumbPos = wristPos + rotateOffset(thumbLocal, wristRot);
            Renderer::drawCube(thumbPos, wristRot + Vec3(0, -25.0f, 30.0f), { 0.024f, 0.022f, 0.042f }, gloveLeather, nullptr, true);

            // Index Finger (Trigger Finger): extended forward slightly curled onto trigger
            Vec3 indexLocal = Vec3(0.022f, -0.005f, -0.075f);
            Vec3 indexPos = wristPos + rotateOffset(indexLocal, wristRot);
            Renderer::drawCube(indexPos, wristRot + Vec3(18.0f, -8.0f, 0.0f), { 0.020f, 0.019f, 0.045f }, fingerJoint, nullptr, true);

            // Grip Fingers (Middle, Ring, Pinky): curled around grip
            for (int f = 0; f < 3; ++f) {
                float yOff = -0.024f - f * 0.019f;
                Vec3 gripLocal = Vec3(0.012f, yOff, -0.052f);
                Vec3 gripPos = wristPos + rotateOffset(gripLocal, wristRot);
                Renderer::drawCube(gripPos, wristRot + Vec3(22.0f, 15.0f, -10.0f), { 0.021f, 0.018f, 0.040f }, fingerJoint, nullptr, true);
            }
        } else {
            // LEFT HAND (Support Hand):
            if (animator.isReloading()) {
                // Fingers holding / guiding the magazine during reload
                for (int f = 0; f < 4; ++f) {
                    float yOff = 0.015f - f * 0.016f;
                    Vec3 gripLocal = Vec3(0.018f, yOff, -0.045f);
                    Vec3 gripPos = wristPos + rotateOffset(gripLocal, wristRot);
                    Renderer::drawCube(gripPos, wristRot + Vec3(10.0f, -15.0f, 0.0f), { 0.019f, 0.016f, 0.036f }, fingerJoint, nullptr, true);
                }
                // Thumb bracing magazine back
                Vec3 thumbLocal = Vec3(-0.022f, 0.020f, -0.030f);
                Vec3 thumbPos = wristPos + rotateOffset(thumbLocal, wristRot);
                Renderer::drawCube(thumbPos, wristRot + Vec3(0, 20.0f, -25.0f), { 0.022f, 0.019f, 0.038f }, gloveLeather, nullptr, true);
            } else if (weaponId == WeaponID::Pistol) {
                // Two-handed Pistol Stance: Support hand cups underneath right fist
                Vec3 cupLocal = Vec3(0.00f, -0.025f, -0.035f);
                Vec3 cupPos = wristPos + rotateOffset(cupLocal, wristRot);
                Renderer::drawCube(cupPos, wristRot + Vec3(-12.0f, 15.0f, 18.0f), { 0.065f, 0.025f, 0.055f }, fingerJoint, nullptr, true);
            } else if (weaponId == WeaponID::Pipe) {
                // Two-handed baseball bat grip: left hand wrapped around pipe shaft directly below right hand
                for (int f = 0; f < 4; ++f) {
                    float yOff = 0.018f - f * 0.017f;
                    Vec3 gripLocal = Vec3(-0.015f, yOff, -0.042f);
                    Vec3 gripPos = wristPos + rotateOffset(gripLocal, wristRot);
                    Renderer::drawCube(gripPos, wristRot + Vec3(18.0f, 25.0f, -15.0f), { 0.020f, 0.018f, 0.038f }, fingerJoint, nullptr, true);
                }
            } else {
                // Standard 2-handed Rifle/Shotgun/RPG foregrip hold
                for (int f = 0; f < 4; ++f) {
                    float zOff = -0.025f - f * 0.017f;
                    Vec3 gripLocal = Vec3(0.015f, 0.012f, zOff);
                    Vec3 gripPos = wristPos + rotateOffset(gripLocal, wristRot);
                    Renderer::drawCube(gripPos, wristRot + Vec3(25.0f, 0.0f, -20.0f), { 0.020f, 0.019f, 0.038f }, fingerJoint, nullptr, true);
                }
                // Thumb wrapping over the top/side of handguard
                Vec3 thumbLocal = Vec3(-0.022f, 0.028f, -0.042f);
                Vec3 thumbPos = wristPos + rotateOffset(thumbLocal, wristRot);
                Renderer::drawCube(thumbPos, wristRot + Vec3(0.0f, 30.0f, -15.0f), { 0.022f, 0.019f, 0.038f }, gloveLeather, nullptr, true);
            }
        }
    }

    void ViewModelArms::drawReloadMagazine(const Vec3& magPos, const Vec3& magRot, WeaponID weaponId) {
        (void)weaponId;
        // High-detail tactical magazine box
        Vec3 magBodyColor = { 0.08f, 0.09f, 0.11f };
        Vec3 magPlateColor = { 0.16f, 0.18f, 0.22f };
        Vec3 brassBulletColor = { 0.85f, 0.72f, 0.24f };

        // Magazine housing body
        Renderer::drawCube(magPos, magRot, { 0.038f, 0.13f, 0.065f }, magBodyColor, nullptr, true);
        // Baseplate
        Renderer::drawCube(magPos - Vec3(0.0f, 0.065f, 0.0f), magRot, { 0.044f, 0.015f, 0.072f }, magPlateColor, nullptr, true);
        // Visible brass bullet cartridges at the feed lips
        Renderer::drawCube(magPos + Vec3(0.0f, 0.068f, -0.008f), magRot, { 0.022f, 0.014f, 0.042f }, brassBulletColor, nullptr, true);
    }

    void ViewModelArms::renderMeleeTrail(const Vec3& pipeTipPos, const Vec3& pipeBasePos, float alpha) {
        if (alpha <= 0.01f) return;

        // Render layered kinetic energy blade arc along the pipe sweep
        Vec3 trailColor = { 0.65f * alpha, 0.85f * alpha, 1.0f * alpha };
        Vec3 trailCenter = (pipeTipPos + pipeBasePos) * 0.5f;
        Vec3 trailDir = pipeTipPos - pipeBasePos;
        float trailLen = trailDir.length();
        if (trailLen < 0.01f) return;

        Vec3 trailRot = { 0.0f, 0.0f, 0.0f };
        Renderer::drawCube(trailCenter, trailRot, { 0.035f * alpha, trailLen, 0.06f }, trailColor, nullptr, false);
    }

    void ViewModelArms::render(const Vec3& weaponBasePos, const Vec3& weaponRot,
                              WeaponID weaponId, const WeaponAnimator& animator,
                              Texture* sleeveTexture) {
        // Shoulder roots (where arms emerge from offscreen bottom corners)
        Vec3 rightElbowStart = { 0.38f, -0.42f, -0.22f };
        Vec3 leftElbowStart  = { -0.38f, -0.42f, -0.22f };

        // 1. Right Arm / Trigger Hand Socket:
        // Positioned firmly at the weapon's pistol grip / pipe handle
        Vec3 rightWristPos = weaponBasePos + Vec3(-0.015f, -0.065f, -0.04f);
        Vec3 rightWristRot = weaponRot + Vec3(8.0f, 0.0f, -4.0f);

        drawForearmSleeve(rightElbowStart, rightWristPos, rightWristRot, false, sleeveTexture);
        drawTacticalGlove(rightWristPos, rightWristRot, false, weaponId, animator);

        // 2. Left Arm / Support Hand Socket:
        Vec3 leftWristPos;
        Vec3 leftWristRot;

        if (animator.isReloading()) {
            // During reload: left hand detaches and follows magazine retrieval and insertion path
            Vec3 reloadOff = animator.getLeftHandReloadOffset();
            Vec3 reloadRot = animator.getLeftHandReloadRotation();

            leftWristPos = weaponBasePos + Vec3(-0.12f, -0.10f, -0.22f) + reloadOff;
            leftWristRot = weaponRot + Vec3(18.0f, -15.0f, 25.0f) + reloadRot;

            drawForearmSleeve(leftElbowStart, leftWristPos, leftWristRot, true, sleeveTexture);
            drawTacticalGlove(leftWristPos, leftWristRot, true, weaponId, animator);

            // Draw fresh magazine held in left hand
            if (animator.isReloadMagazineVisible()) {
                Vec3 magPos = leftWristPos + Vec3(0.02f, 0.04f, 0.0f);
                drawReloadMagazine(magPos, leftWristRot, weaponId);
            }
        } else if (animator.isInspecting()) {
            // During inspect: left hand drops slightly to showcase weapon model
            float p = animator.getStateProgress();
            float dropY = (p < 0.5f) ? (p * 2.0f * -0.12f) : ((1.0f - p) * 2.0f * -0.12f);

            leftWristPos = weaponBasePos + Vec3(-0.18f, -0.14f + dropY, -0.22f);
            leftWristRot = weaponRot + Vec3(15.0f, 20.0f, -15.0f);

            drawForearmSleeve(leftElbowStart, leftWristPos, leftWristRot, true, sleeveTexture);
            drawTacticalGlove(leftWristPos, leftWristRot, true, weaponId, animator);
        } else if (weaponId == WeaponID::Pipe) {
            // Two-handed baseball bat stance for Pipe
            leftWristPos = weaponBasePos + Vec3(-0.01f, -0.125f, -0.02f);
            leftWristRot = weaponRot + Vec3(12.0f, -5.0f, 6.0f);

            drawForearmSleeve(leftElbowStart, leftWristPos, leftWristRot, true, sleeveTexture);
            drawTacticalGlove(leftWristPos, leftWristRot, true, weaponId, animator);

            // If melee swing is active, render the kinetic motion trail along the pipe tip!
            if (animator.isMeleeSwinging()) {
                float p = animator.getStateProgress();
                if (p >= 0.22f && p <= 0.72f) {
                    float trailAlpha = std::sin((p - 0.22f) / 0.50f * 3.14159265f);
                    Vec3 pipeTipPos = weaponBasePos + Vec3(0.08f, 0.35f, -0.22f);
                    Vec3 pipeBasePos = weaponBasePos + Vec3(0.0f, 0.02f, 0.0f);
                    renderMeleeTrail(pipeTipPos, pipeBasePos, trailAlpha);
                }
            }
        } else if (weaponId == WeaponID::Pistol) {
            // Two-handed Pistol Support Grip (cup and saucer under right fist)
            leftWristPos = weaponBasePos + Vec3(-0.055f, -0.105f, -0.035f);
            leftWristRot = weaponRot + Vec3(14.0f, -12.0f, 15.0f);

            drawForearmSleeve(leftElbowStart, leftWristPos, leftWristRot, true, sleeveTexture);
            drawTacticalGlove(leftWristPos, leftWristRot, true, weaponId, animator);
        } else {
            // Two-handed Rifle/Shotgun/RPG Foregrip Support
            float fwdReach = (weaponId == WeaponID::Shotgun || weaponId == WeaponID::Minigun) ? -0.26f : -0.34f;
            leftWristPos = weaponBasePos + Vec3(-0.065f, -0.060f, fwdReach);
            leftWristRot = weaponRot + Vec3(22.0f, 12.0f, -22.0f);

            drawForearmSleeve(leftElbowStart, leftWristPos, leftWristRot, true, sleeveTexture);
            drawTacticalGlove(leftWristPos, leftWristRot, true, weaponId, animator);
        }
    }

} // namespace Lab
