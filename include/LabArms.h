#pragma once
#include <vector>
#include <array>
#include <string>
#include <memory>
#include "LabMath.h"
#include "LabAnim.h"

namespace Lab {

    class Texture;
    enum class WeaponID : int;

    // Tactical first-person arms and hands rendering system
    // Provides anatomical dual-arm kinematics, glove knuckles, cyber-gauntlet status LED,
    // dynamic support hand placement, animated magazine during reload, and melee motion trail.
    class ViewModelArms {
    public:
        ViewModelArms();
        ~ViewModelArms() = default;

        // Render both arms and hands relative to active weapon transform
        void render(const Vec3& weaponBasePos, const Vec3& weaponRot,
                    WeaponID weaponId, const WeaponAnimator& animator,
                    Texture* sleeveTexture = nullptr);

        // Render melee motion ribbon during pipe slash
        void renderMeleeTrail(const Vec3& pipeTipPos, const Vec3& pipeBasePos, float alpha);

    private:
        // Draw a single anatomical forearm sleeve from screen edge to wrist
        void drawForearmSleeve(const Vec3& elbowStart, const Vec3& wristEnd,
                               const Vec3& wristRot, bool isLeft, Texture* texture);

        // Draw a tactical combat glove (palm, knuckle armor plate, 5 articulated fingers)
        void drawTacticalGlove(const Vec3& wristPos, const Vec3& wristRot,
                               bool isLeft, WeaponID weaponId, const WeaponAnimator& animator);

        // Draw fresh magazine held in left hand during reload cycle
        void drawReloadMagazine(const Vec3& magPos, const Vec3& magRot, WeaponID weaponId);

        // Recent history of pipe tip positions for motion ribbon
        std::vector<Vec3> _trailHistory;
        float _trailTimer = 0.0f;
    };

} // namespace Lab
