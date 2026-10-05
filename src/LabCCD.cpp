#include "LabCCD.h"

namespace Lab {

    Vec3 ConvexHull3D::getSupportPoint(const Vec3& direction) const {
        if (vertices.empty()) return Vec3(0, 0, 0);
        float maxDot = -std::numeric_limits<float>::infinity();
        Vec3 best = vertices[0];
        for (const auto& v : vertices) {
            float d = Vec3::dot(v, direction);
            if (d > maxDot) {
                maxDot = d;
                best = v;
            }
        }
        return best;
    }

    void ConvexHull3D::projectToAxis(const Vec3& axis, float& outMin, float& outMax) const {
        if (vertices.empty()) { outMin = outMax = 0; return; }
        outMin = std::numeric_limits<float>::infinity();
        outMax = -std::numeric_limits<float>::infinity();
        for (const auto& v : vertices) {
            float d = Vec3::dot(v, axis);
            if (d < outMin) outMin = d;
            if (d > outMax) outMax = d;
        }
    }

    void TriangleMeshCollider3D::addTriangle(const Vec3& p1, const Vec3& p2, const Vec3& p3) {
        Vec3 norm = Vec3::cross(p2 - p1, p3 - p1).normalized();
        triangles.push_back({ p1, p2, p3, norm });
    }

    bool CCDPhysics::testConvexHullOverlap(const ConvexHull3D& a, const Mat4& transA,
                                          const ConvexHull3D& b, const Mat4& transB) {
        std::vector<Vec3> axes;

        // Transform normals of hull A
        for (const auto& n : a.faceNormals) {
            axes.push_back(Mat4::transformVector(transA, n).normalized());
        }
        // Transform normals of hull B
        for (const auto& n : b.faceNormals) {
            axes.push_back(Mat4::transformVector(transB, n).normalized());
        }

        // Test projection overlap on all axes
        for (const auto& axis : axes) {
            if (axis.lengthSquared() < 1e-6f) continue;

            float minA = 0, maxA = 0;
            float minB = 0, maxB = 0;

            // Project A
            float a0 = std::numeric_limits<float>::infinity();
            float a1 = -std::numeric_limits<float>::infinity();
            for (const auto& v : a.vertices) {
                Vec3 worldV = Mat4::transformPoint(transA, v);
                float d = Vec3::dot(worldV, axis);
                if (d < a0) a0 = d;
                if (d > a1) a1 = d;
            }
            minA = a0; maxA = a1;

            // Project B
            float b0 = std::numeric_limits<float>::infinity();
            float b1 = -std::numeric_limits<float>::infinity();
            for (const auto& v : b.vertices) {
                Vec3 worldV = Mat4::transformPoint(transB, v);
                float d = Vec3::dot(worldV, axis);
                if (d < b0) b0 = d;
                if (d > b1) b1 = d;
            }
            minB = b0; maxB = b1;

            // Separating axis found!
            if (maxA < minB || maxB < minA) {
                return false;
            }
        }
        return true;
    }

    bool CCDPhysics::sweepSphereVsAABB(const Vec3& start, const Vec3& end, float radius,
                                       const Vec3& boxMin, const Vec3& boxMax, CCDHit& outHit) {
        Vec3 delta = end - start;
        float len = delta.length();
        if (len < 1e-6f) return false;

        Vec3 dir = delta / len;

        // Expand box by sphere radius
        Vec3 expMin = boxMin - Vec3(radius, radius, radius);
        Vec3 expMax = boxMax + Vec3(radius, radius, radius);

        // Ray vs expanded AABB slab test
        float tMin = 0.0f;
        float tMax = len;
        Vec3 hitNorm(0, 0, 0);

        for (int i = 0; i < 3; ++i) {
            float ood = (i == 0) ? (1.0f / dir.x) : ((i == 1) ? (1.0f / dir.y) : (1.0f / dir.z));
            float originCoord = (i == 0) ? start.x : ((i == 1) ? start.y : start.z);
            float minCoord = (i == 0) ? expMin.x : ((i == 1) ? expMin.y : expMin.z);
            float maxCoord = (i == 0) ? expMax.x : ((i == 1) ? expMax.y : expMax.z);

            if (std::abs((i == 0) ? dir.x : ((i == 1) ? dir.y : dir.z)) < 1e-6f) {
                if (originCoord < minCoord || originCoord > maxCoord) return false;
            } else {
                float t1 = (minCoord - originCoord) * ood;
                float t2 = (maxCoord - originCoord) * ood;
                Vec3 n1(i == 0 ? -1.0f : 0.0f, i == 1 ? -1.0f : 0.0f, i == 2 ? -1.0f : 0.0f);
                Vec3 n2(i == 0 ? 1.0f : 0.0f, i == 1 ? 1.0f : 0.0f, i == 2 ? 1.0f : 0.0f);

                if (t1 > t2) { std::swap(t1, t2); std::swap(n1, n2); }
                if (t1 > tMin) { tMin = t1; hitNorm = n1; }
                if (t2 < tMax) tMax = t2;
                if (tMin > tMax) return false;
            }
        }

        if (tMin >= 0.0f && tMin <= len) {
            outHit.hit = true;
            outHit.timeOfImpact = tMin / len;
            outHit.point = start + dir * tMin;
            outHit.normal = hitNorm;
            return true;
        }

        return false;
    }

} // namespace Lab
