#pragma once

#include "LabMath.h"
#include <vector>
#include <cstdint>

namespace Lab {

    struct CableVertex {
        Vec3 position;
        Vec3 normal;
        Vec2 texCoords;
    };

    struct CableMesh {
        std::vector<CableVertex> vertices;
        std::vector<uint32_t> indices;
    };

    class CableGenerator {
    public:
        // Generates procedural catenary tube geometry between two points
        static CableMesh generate(const Vec3& start, const Vec3& end,
                                  float slack = 0.35f,
                                  float radius = 0.02f,
                                  int segments = 16,
                                  int radialSegments = 4,
                                  float textureTileLength = 1.0f);

        // Samples curve point at t in [0, 1]
        static Vec3 sampleCurve(const Vec3& start, const Vec3& end, float slack, float t);
    };

} // namespace Lab
