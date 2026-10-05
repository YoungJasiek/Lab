#pragma once
#include <vector>
#include <cmath>
#include <algorithm>
#include <limits>
#include "LabMath.h"

namespace Lab {

    struct ConvexHull3D {
        std::vector<Vec3> vertices;
        std::vector<Vec3> faceNormals;

        // Support point in a given direction (Extreme point for SAT / GJK)
        Vec3 getSupportPoint(const Vec3& direction) const;

        // Project hull onto axis -> [min, max]
        void projectToAxis(const Vec3& axis, float& outMin, float& outMax) const;
    };

    struct TriangleMeshCollider3D {
        struct Triangle {
            Vec3 a, b, c;
            Vec3 normal;
        };
        std::vector<Triangle> triangles;

        void addTriangle(const Vec3& p1, const Vec3& p2, const Vec3& p3);
    };

    struct CCDHit {
        bool hit{false};
        float timeOfImpact{1.0f}; // [0..1] fraction of step
        Vec3 point{0, 0, 0};
        Vec3 normal{0, 0, 0};
    };

    class CCDPhysics {
    public:
        // Separating Axis Theorem (SAT) test for two Convex Hulls in 3D
        static bool testConvexHullOverlap(const ConvexHull3D& a, const Mat4& transA,
                                          const ConvexHull3D& b, const Mat4& transB);

        // Continuous Collision Detection (CCD) Ray/Sphere Sweep against thin plane or AABB
        // Prevents high-speed tunneling through obstacles
        static bool sweepSphereVsAABB(const Vec3& start, const Vec3& end, float radius,
                                      const Vec3& boxMin, const Vec3& boxMax, CCDHit& outHit);
    };

} // namespace Lab
