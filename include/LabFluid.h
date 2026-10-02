#pragma once
#include <vector>
#include <cmath>
#include <algorithm>
#include "LabMath.h"

namespace Lab {

    struct WaterVolume {
        float surfaceY{0.0f};
        float density{1000.0f}; // kg/m^3 (water density)
        float linearDrag{0.92f};
        float angularDrag{0.85f};

        // Gerstner / Sine Wave height calculation
        float getWaterHeightAt(float x, float z, float time = 0.0f) const {
            float wave1 = std::sin(x * 0.5f + time * 2.0f) * 0.2f;
            float wave2 = std::cos(z * 0.3f + time * 1.5f) * 0.15f;
            return surfaceY + wave1 + wave2;
        }

        Vec3 getWaterNormalAt(float x, float z, float time = 0.0f) const {
            float eps = 0.1f;
            float hL = getWaterHeightAt(x - eps, z, time);
            float hR = getWaterHeightAt(x + eps, z, time);
            float hD = getWaterHeightAt(x, z - eps, time);
            float hU = getWaterHeightAt(x, z + eps, time);
            Vec3 n(-(hR - hL) / (2.0f * eps), 1.0f, -(hU - hD) / (2.0f * eps));
            return n.normalized();
        }
    };

    struct FloatingBody {
        Vec3 position{0, 0, 0};
        Vec3 velocity{0, 0, 0};
        float mass{50.0f};      // 50 kg
        float volume{0.1f};     // 0.1 m^3 -> density = 500 kg/m^3 (floats in water!)
        float height{0.5f};     // Full height of box

        void step(float dt, const WaterVolume& water, float time, const Vec3& gravity = Vec3(0, -9.81f, 0)) {
            if (dt <= 0.0f) return;

            // 1. Gravity
            velocity += gravity * dt;

            // 2. Archimedes Buoyancy Force
            float waterY = water.getWaterHeightAt(position.x, position.z, time);
            float bottomY = position.y - height * 0.5f;
            float topY = position.y + height * 0.5f;

            if (bottomY < waterY) {
                // Submerged fraction [0..1]
                float submergedDepth = std::clamp(waterY - bottomY, 0.0f, height);
                float submergedFraction = submergedDepth / height;
                float submergedVolume = volume * submergedFraction;

                // F_buoyancy = rho * V_submerged * g (upwards)
                float buoyancyMagnitude = water.density * submergedVolume * std::abs(gravity.y);
                Vec3 buoyancyForce(0.0f, buoyancyMagnitude, 0.0f);

                velocity += (buoyancyForce * (1.0f / mass)) * dt;

                // Fluid drag damping
                velocity.x *= water.linearDrag;
                velocity.y *= water.linearDrag;
                velocity.z *= water.linearDrag;
            }

            // 3. Integrate position
            position += velocity * dt;
        }
    };

} // namespace Lab
