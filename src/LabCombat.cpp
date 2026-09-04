#include "LabCombat.h"
#include <cmath>
#include <algorithm>

namespace Lab {

    bool Raycast::rayIntersectAABB(const Vec3& rayOrigin, const Vec3& rayDir,
                                   const Vec3& boxMin, const Vec3& boxMax,
                                   float& tOut, Vec3* outNormal) {
        float tmin = -1e9f;
        float tmax =  1e9f;
        Vec3 hitNormal = { 0.0f, 1.0f, 0.0f };

        // X axis slab
        if (std::abs(rayDir.x) < 1e-6f) {
            if (rayOrigin.x < boxMin.x || rayOrigin.x > boxMax.x) return false;
        } else {
            float invD = 1.0f / rayDir.x;
            float t1 = (boxMin.x - rayOrigin.x) * invD;
            float t2 = (boxMax.x - rayOrigin.x) * invD;
            Vec3 norm1 = { -1.0f, 0.0f, 0.0f };
            Vec3 norm2 = {  1.0f, 0.0f, 0.0f };
            if (invD < 0.0f) {
                std::swap(t1, t2);
                std::swap(norm1, norm2);
            }
            if (t1 > tmin) {
                tmin = t1;
                hitNormal = norm1;
            }
            tmax = std::min(tmax, t2);
            if (tmin > tmax) return false;
        }

        // Y axis slab
        if (std::abs(rayDir.y) < 1e-6f) {
            if (rayOrigin.y < boxMin.y || rayOrigin.y > boxMax.y) return false;
        } else {
            float invD = 1.0f / rayDir.y;
            float t1 = (boxMin.y - rayOrigin.y) * invD;
            float t2 = (boxMax.y - rayOrigin.y) * invD;
            Vec3 norm1 = { 0.0f, -1.0f, 0.0f };
            Vec3 norm2 = { 0.0f,  1.0f, 0.0f };
            if (invD < 0.0f) {
                std::swap(t1, t2);
                std::swap(norm1, norm2);
            }
            if (t1 > tmin) {
                tmin = t1;
                hitNormal = norm1;
            }
            tmax = std::min(tmax, t2);
            if (tmin > tmax) return false;
        }

        // Z axis slab
        if (std::abs(rayDir.z) < 1e-6f) {
            if (rayOrigin.z < boxMin.z || rayOrigin.z > boxMax.z) return false;
        } else {
            float invD = 1.0f / rayDir.z;
            float t1 = (boxMin.z - rayOrigin.z) * invD;
            float t2 = (boxMax.z - rayOrigin.z) * invD;
            Vec3 norm1 = { 0.0f, 0.0f, -1.0f };
            Vec3 norm2 = { 0.0f, 0.0f,  1.0f };
            if (invD < 0.0f) {
                std::swap(t1, t2);
                std::swap(norm1, norm2);
            }
            if (t1 > tmin) {
                tmin = t1;
                hitNormal = norm1;
            }
            tmax = std::min(tmax, t2);
            if (tmin > tmax) return false;
        }

        if (tmax < 0.0f) return false; // Box is behind the ray origin

        tOut = (tmin < 0.0f) ? 0.0f : tmin;
        if (outNormal) *outNormal = hitNormal;
        return true;
    }

} // namespace Lab
