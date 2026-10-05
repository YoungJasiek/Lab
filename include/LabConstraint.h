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

        void setMass(float m);
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
        static void solveDistance(ConstraintBody& a, ConstraintBody& b, const Constraint& c);
        static void solveBallSocket(ConstraintBody& a, ConstraintBody& b, const Constraint& c);
        static void solveSlider(ConstraintBody& a, ConstraintBody& b, const Constraint& c);
    };

} // namespace Lab
