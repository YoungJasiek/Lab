#include "LabConstraint.h"

namespace Lab {

    void ConstraintBody::setMass(float m) {
        mass = m;
        invMass = (isStatic || m <= 0.0001f) ? 0.0f : (1.0f / m);
    }

    void ConstraintSolver::solveDistance(ConstraintBody& a, ConstraintBody& b, const Constraint& c) {
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

    void ConstraintSolver::solveBallSocket(ConstraintBody& a, ConstraintBody& b, const Constraint& c) {
        Vec3 posA = a.position + c.localAnchorA;
        Vec3 posB = b.position + c.localAnchorB;

        Vec3 error = posB - posA;
        float totalInvMass = a.invMass + b.invMass;
        if (totalInvMass <= 0.0f) return;

        Vec3 impulse = error * (1.0f / totalInvMass);
        if (!a.isStatic) a.position += impulse * a.invMass;
        if (!b.isStatic) b.position -= impulse * b.invMass;
    }

    void ConstraintSolver::solveSlider(ConstraintBody& a, ConstraintBody& b, const Constraint& c) {
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

} // namespace Lab
