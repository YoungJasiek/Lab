#pragma once
#include <vector>
#include <cmath>
#include <algorithm>
#include "LabMath.h"

namespace Lab {

    enum class ConstraintType {
        Distance,
        BallSocket,
        Hinge,
        Slider,
        Fixed
    };

    struct ConstraintBody {
        Vec3 position{0, 0, 0};
        Vec3 velocity{0, 0, 0};
        float mass{1.0f};
        float invMass{1.0f};
        bool isStatic{false};

        void setMass(float m) {
            mass = m;
            invMass = (isStatic || m <= 0.0001f) ? 0.0f : (1.0f / m);
        }
    };

    struct Constraint {
        ConstraintType type{ConstraintType::Distance};
        size_t bodyA{0};
        size_t bodyB{0};
        Vec3 localAnchorA{0, 0, 0};
        Vec3 localAnchorB{0, 0, 0};

        // Parameters
        float targetDistance{1.0f};
        Vec3 axis{0, 1, 0}; // for Hinge and Slider
        float minLimit{-1.0f};
        float maxLimit{1.0f};
    };

    class ConstraintSolver {
    public:
        static void solveDistance(ConstraintBody& a, ConstraintBody& b, const Constraint& c) {
            Vec3 posA = a.position + c.localAnchorA;
            Vec3 posB = b.position + c.localAnchorB;

            Vec3 delta = posB - posA;
            float currentDist = delta.length();
            if (currentDist < 1e-5f) return;

            float error = currentDist - c.targetDistance;
            Vec3 norm = delta / currentDist;

            float totalInvMass = a.invMass + b.invMass;
            if (totalInvMass <= 0.0f) return;

            Vec3 impulse = norm * (error / totalInvMass);

            if (!a.isStatic) a.position += impulse * a.invMass;
            if (!b.isStatic) b.position -= impulse * b.invMass;
        }

        static void solveBallSocket(ConstraintBody& a, ConstraintBody& b, const Constraint& c) {
            Vec3 posA = a.position + c.localAnchorA;
            Vec3 posB = b.position + c.localAnchorB;

            Vec3 error = posB - posA;
            float totalInvMass = a.invMass + b.invMass;
            if (totalInvMass <= 0.0f) return;

            Vec3 impulse = error * (1.0f / totalInvMass);
            if (!a.isStatic) a.position += impulse * a.invMass;
            if (!b.isStatic) b.position -= impulse * b.invMass;
        }

        static void solveSlider(ConstraintBody& a, ConstraintBody& b, const Constraint& c) {
            // Constrain relative motion to axis only, clamp within limits
            Vec3 delta = (b.position + c.localAnchorB) - (a.position + c.localAnchorA);
            float proj = Vec3::dot(delta, c.axis);
            Vec3 onAxis = c.axis * proj;
            Vec3 offAxis = delta - onAxis;

            float wA = a.isStatic ? 0.0f : a.invMass;
            float wB = b.isStatic ? 0.0f : b.invMass;
            float totalInvMass = wA + wB;
            if (totalInvMass <= 0.0f) return;

            // Remove off-axis deviation
            if (!a.isStatic) a.position += offAxis * (wA / totalInvMass);
            if (!b.isStatic) b.position -= offAxis * (wB / totalInvMass);

            // Enforce linear limits [minLimit .. maxLimit]
            float clampedProj = std::clamp(proj, c.minLimit, c.maxLimit);
            if (std::abs(proj - clampedProj) > 1e-4f) {
                Vec3 limitCorrection = c.axis * (proj - clampedProj);
                if (!a.isStatic) a.position += limitCorrection * (wA / totalInvMass);
                if (!b.isStatic) b.position -= limitCorrection * (wB / totalInvMass);
            }
        }
    };

} // namespace Lab
