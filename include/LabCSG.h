#pragma once
#include <vector>
#include <string>
#include <memory>
#include "LabMath.h"
#include "LabRenderer.h"

namespace Lab {

    enum class ClipMode {
        KeepFront = 0, // Keep positive half-space (N . (X - P) >= 0)
        KeepBack  = 1, // Keep negative half-space (N . (X - P) <= 0)
        KeepBoth  = 2  // Split into two separate convex brushes
    };

    struct SlicedGeometry {
        std::vector<Vertex> vertices;
        std::vector<unsigned int> indices;
        Vec3 centroid{ 0.0f, 0.0f, 0.0f };
        Vec3 aabbMin{ 0.0f, 0.0f, 0.0f };
        Vec3 aabbMax{ 0.0f, 0.0f, 0.0f };
        int triangleCount = 0;

        bool isValid() const {
            return !vertices.empty() && !indices.empty() && (indices.size() % 3 == 0);
        }
    };

    struct ClipResult {
        bool didIntersect = false;
        SlicedGeometry frontPiece;
        SlicedGeometry backPiece;
    };

    class CSGTool {
    public:
        // Slices an axis-aligned 3D box (from boxMin to boxMax) with an arbitrary plane (planePoint, planeNormal).
        // Computes water-tight convex polyhedral geometry for front and back pieces with normals and UV coordinates.
        static ClipResult sliceBox(const Vec3& boxMin, const Vec3& boxMax,
                                   const Vec3& planePoint, const Vec3& planeNormal,
                                   const Vec2& uvScale = { 0.25f, 0.25f });

        // Slices arbitrary convex geometry (vertices and indices) with a plane.
        static ClipResult sliceConvexMesh(const std::vector<Vertex>& inVertices,
                                          const std::vector<unsigned int>& inIndices,
                                          const Vec3& planePoint, const Vec3& planeNormal,
                                          const Vec2& uvScale = { 0.25f, 0.25f });

        // Helper to construct a vertical slicing plane from 2 points in 2D Top-Down view (X, Z)
        static void makePlaneFrom2DLine(const Vec2& p1, const Vec2& p2, Vec3& outPoint, Vec3& outNormal);

        // Helper to compute planar UV coordinates for a vertex on a given face normal
        static Vec2 computePlanarUV(const Vec3& pos, const Vec3& normal, const Vec2& uvScale);
    };

} // namespace Lab
