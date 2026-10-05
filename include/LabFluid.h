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
        float getWaterHeightAt(float x, float z, float time = 0.0f) const;
        Vec3 getWaterNormalAt(float x, float z, float time = 0.0f) const;
    };

    struct FloatingBody {
        Vec3 position{0, 0, 0};
        Vec3 velocity{0, 0, 0};
        float mass{50.0f};      // 50 kg
        float volume{0.1f};     // 0.1 m^3 -> density = 500 kg/m^3 (floats in water!)
        float height{0.5f};     // Full height of box

        void step(float dt, const WaterVolume& water, float time, const Vec3& gravity = Vec3(0, -9.81f, 0));
    };

} // namespace Lab
