#include "LabCable.h"
#include <cmath>
#include <numbers>

namespace Lab {

    Vec3 CableGenerator::sampleCurve(const Vec3& start, const Vec3& end, float slack, float t) {
        // Linear interpolation between start and end
        Vec3 p = start + (end - start) * t;

        // Catenary parabolic sag approximation under gravity: peak sag at t = 0.5 is exactly 'slack' meters
        float sag = 4.0f * slack * t * (1.0f - t);
        p.y -= sag;
        return p;
    }

    CableMesh CableGenerator::generate(const Vec3& start, const Vec3& end,
                                       float slack,
                                       float radius,
                                       int segments,
                                       int radialSegments,
                                       float textureTileLength) {
        CableMesh mesh;
        if (segments < 2) segments = 2;
        if (radialSegments < 3) radialSegments = 4; // 4-sided tube is standard for Source cables

        // Total approximate length of the cable
        float totalLength = 0.0f;
        std::vector<Vec3> curvePoints(static_cast<size_t>(segments + 1));
        std::vector<float> arcLengths(static_cast<size_t>(segments + 1), 0.0f);

        for (int i = 0; i <= segments; ++i) {
            float t = static_cast<float>(i) / static_cast<float>(segments);
            curvePoints[static_cast<size_t>(i)] = sampleCurve(start, end, slack, t);
            if (i > 0) {
                float dist = (curvePoints[static_cast<size_t>(i)] - curvePoints[static_cast<size_t>(i - 1)]).length();
                totalLength += dist;
                arcLengths[static_cast<size_t>(i)] = totalLength;
            }
        }

        if (totalLength <= 0.0001f) totalLength = 1.0f;
        if (textureTileLength <= 0.0001f) textureTileLength = 1.0f;

        // Generate rings along the curve
        for (int i = 0; i <= segments; ++i) {
            Vec3 center = curvePoints[static_cast<size_t>(i)];

            // Tangent calculation
            Vec3 tangent;
            if (i == 0) {
                tangent = curvePoints[1] - curvePoints[0];
            } else if (i == segments) {
                tangent = curvePoints[static_cast<size_t>(segments)] - curvePoints[static_cast<size_t>(segments - 1)];
            } else {
                tangent = curvePoints[static_cast<size_t>(i + 1)] - curvePoints[static_cast<size_t>(i - 1)];
            }
            if (tangent.lengthSquared() > 0.00001f) {
                tangent = tangent.normalized();
            } else {
                tangent = Vec3(0, 0, 1);
            }

            // Normal & Binormal frame
            Vec3 up = (std::abs(tangent.y) > 0.99f) ? Vec3(1, 0, 0) : Vec3(0, 1, 0);
            Vec3 right = tangent.cross(up).normalized();
            Vec3 binormal = right.cross(tangent).normalized();

            float v = arcLengths[static_cast<size_t>(i)] / textureTileLength;

            for (int r = 0; r <= radialSegments; ++r) {
                float theta = (static_cast<float>(r) / static_cast<float>(radialSegments)) * 2.0f * 3.1415926535f;
                float cosT = std::cos(theta);
                float sinT = std::sin(theta);

                Vec3 ringNormal = right * cosT + binormal * sinT;
                Vec3 vertexPos = center + ringNormal * radius;
                float u = static_cast<float>(r) / static_cast<float>(radialSegments);

                CableVertex vtx;
                vtx.position = vertexPos;
                vtx.normal = ringNormal;
                vtx.texCoords = Vec2(u, v);
                mesh.vertices.push_back(vtx);
            }
        }

        // Generate indices for triangle strips between rings
        int ringStride = radialSegments + 1;
        for (int i = 0; i < segments; ++i) {
            int ringA = i * ringStride;
            int ringB = (i + 1) * ringStride;

            for (int r = 0; r < radialSegments; ++r) {
                uint32_t a0 = static_cast<uint32_t>(ringA + r);
                uint32_t a1 = static_cast<uint32_t>(ringA + r + 1);
                uint32_t b0 = static_cast<uint32_t>(ringB + r);
                uint32_t b1 = static_cast<uint32_t>(ringB + r + 1);

                // Two triangles per quad
                mesh.indices.push_back(a0);
                mesh.indices.push_back(b0);
                mesh.indices.push_back(a1);

                mesh.indices.push_back(a1);
                mesh.indices.push_back(b0);
                mesh.indices.push_back(b1);
            }
        }

        return mesh;
    }

} // namespace Lab
