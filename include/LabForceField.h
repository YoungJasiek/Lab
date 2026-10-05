#pragma once

#include "LabMath.h"
#include <vector>
#include <string>
#include <cmath>
#include <algorithm>

namespace Lab {

    enum class ForceFieldType {
        RadialExplosion,  // Shockwave blast decaying with distance squared
        DirectionalWind,  // Linear directional air current with turbulence
        PointGravity,     // Spherical planetary attractor (Newtonian Gm/r^2)
        FrictionVolume    // Surface modifier (e.g. ice, mud) changing friction/damping
    };

    struct ForceFieldTrigger {
        std::string name{"ForceField"};
        ForceFieldType type{ForceFieldType::RadialExplosion};
        Vec3 position{0, 0, 0};
        float radius{10.0f};

        // Radial explosion parameters
        float blastMagnitude{1000.0f}; // Peak impulse / force
        float shockwaveSpeed{50.0f};    // Expanding blast front (m/s)
        float elapsedTime{0.0f};

        // Directional wind parameters
        Vec3 direction{0, 0, 1};
        float windSpeed{15.0f};
        float turbulence{0.0f}; // Random variation

        // Planetary / point gravity parameters
        float gravityStrength{9.81f}; // Acceleration at unit distance or GM

        // Friction / viscosity parameters
        float linearDampingMultiplier{1.0f};

        // Evaluate acceleration / force at target point
        Vec3 evaluateForceAt(const Vec3& targetPos, float targetMass = 1.0f) const;
        bool containsPoint(const Vec3& pt) const;
        void update(float dt);
    };

    class ForceFieldManager {
    public:
        std::vector<ForceFieldTrigger> fields;

        void addField(const ForceFieldTrigger& field);
        void clear();
        void update(float dt);
        Vec3 sampleTotalForceAt(const Vec3& targetPos, float targetMass = 1.0f) const;
        float sampleDampingMultiplierAt(const Vec3& targetPos) const;
    };

} // namespace Lab
