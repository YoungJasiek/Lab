#include "LabCSG.h"
#include <cmath>
#include <algorithm>
#include <vector>

namespace Lab {

    Vec2 CSGTool::computePlanarUV(const Vec3& pos, const Vec3& normal, const Vec2& uvScale) {
        float ax = std::abs(normal.x);
        float ay = std::abs(normal.y);
        float az = std::abs(normal.z);

        if (ay >= ax && ay >= az) {
            // Horizontal surface (Floor / Ceiling)
            return Vec2(pos.x * uvScale.x, pos.z * uvScale.y);
        } else if (ax >= ay && ax >= az) {
            // X-facing wall
            return Vec2(pos.z * uvScale.x, pos.y * uvScale.y);
        } else {
            // Z-facing wall
            return Vec2(pos.x * uvScale.x, pos.y * uvScale.y);
        }
    }

    void CSGTool::makePlaneFrom2DLine(const Vec2& p1, const Vec2& p2, Vec3& outPoint, Vec3& outNormal) {
        Vec2 mid = (p1 + p2) * 0.5f;
        Vec2 dir = p2 - p1;
        float len = dir.length();
        if (len < 1e-5f) {
            outPoint = Vec3(p1.x, 0.0f, p1.y);
            outNormal = Vec3(0.0f, 0.0f, 1.0f);
            return;
        }

        // Perpendicular vector in 2D (normal pointing right of line direction)
        Vec2 n(-dir.y / len, dir.x / len);
        outPoint = Vec3(mid.x, 0.0f, mid.y);
        outNormal = Vec3(n.x, 0.0f, n.y);
    }

    struct PolyFace {
        std::vector<Vec3> points;
        Vec3 normal{ 0, 1, 0 };
    };

    static void triangulatePolygon(const PolyFace& poly, const Vec2& uvScale, SlicedGeometry& outGeom) {
        if (poly.points.size() < 3) return;

        unsigned int baseIndex = (unsigned int)outGeom.vertices.size();
        for (const auto& pt : poly.points) {
            Vec2 uv = CSGTool::computePlanarUV(pt, poly.normal, uvScale);
            outGeom.vertices.push_back(Vertex(pt, poly.normal, uv, Vec3(1.0f, 1.0f, 1.0f)));
        }

        for (size_t i = 1; i + 1 < poly.points.size(); ++i) {
            outGeom.indices.push_back(baseIndex);
            outGeom.indices.push_back(baseIndex + (unsigned int)i);
            outGeom.indices.push_back(baseIndex + (unsigned int)(i + 1));
            outGeom.triangleCount++;
        }
    }

    static void computeBoundsAndCentroid(SlicedGeometry& geom) {
        if (geom.vertices.empty()) return;

        geom.aabbMin = geom.vertices[0].position;
        geom.aabbMax = geom.vertices[0].position;
        Vec3 sum(0, 0, 0);

        for (const auto& v : geom.vertices) {
            geom.aabbMin.x = std::min(geom.aabbMin.x, v.position.x);
            geom.aabbMin.y = std::min(geom.aabbMin.y, v.position.y);
            geom.aabbMin.z = std::min(geom.aabbMin.z, v.position.z);

            geom.aabbMax.x = std::max(geom.aabbMax.x, v.position.x);
            geom.aabbMax.y = std::max(geom.aabbMax.y, v.position.y);
            geom.aabbMax.z = std::max(geom.aabbMax.z, v.position.z);

            sum += v.position;
        }

        geom.centroid = sum * (1.0f / (float)geom.vertices.size());
    }

    static void clipPolygonAgainstPlane(const PolyFace& inPoly, const Vec3& planePoint, const Vec3& planeNormal,
                                        PolyFace& outFront, PolyFace& outBack,
                                        std::vector<Vec3>& outIntersections) {
        outFront.points.clear();
        outBack.points.clear();
        outFront.normal = inPoly.normal;
        outBack.normal = inPoly.normal;

        const float eps = 1e-4f;
        size_t n = inPoly.points.size();
        if (n < 3) return;

        std::vector<float> dists(n);
        for (size_t i = 0; i < n; ++i) {
            dists[i] = Vec3::dot(inPoly.points[i] - planePoint, planeNormal);
        }

        for (size_t i = 0; i < n; ++i) {
            size_t next = (i + 1) % n;
            const Vec3& pA = inPoly.points[i];
            const Vec3& pB = inPoly.points[next];
            float dA = dists[i];
            float dB = dists[next];

            // Add pA to front if dA >= -eps
            if (dA >= -eps) {
                outFront.points.push_back(pA);
            }
            // Add pA to back if dA <= eps
            if (dA <= eps) {
                outBack.points.push_back(pA);
            }

            // Check edge intersection
            if ((dA > eps && dB < -eps) || (dA < -eps && dB > eps)) {
                float t = dA / (dA - dB);
                Vec3 pInt = pA + (pB - pA) * t;
                outFront.points.push_back(pInt);
                outBack.points.push_back(pInt);
                outIntersections.push_back(pInt);
            }
        }
    }

    ClipResult CSGTool::sliceBox(const Vec3& boxMin, const Vec3& boxMax,
                                 const Vec3& planePoint, const Vec3& planeNormIn,
                                 const Vec2& uvScale) {
        ClipResult result;
        Vec3 planeNormal = planeNormIn.normalized();

        // 1. Define the 6 initial quadrilateral faces of the cuboid
        float x0 = boxMin.x, x1 = boxMax.x;
        float y0 = boxMin.y, y1 = boxMax.y;
        float z0 = boxMin.z, z1 = boxMax.z;

        std::vector<PolyFace> boxFaces = {
            // Top (+Y)
            { { Vec3(x0, y1, z0), Vec3(x0, y1, z1), Vec3(x1, y1, z1), Vec3(x1, y1, z0) }, Vec3(0, 1, 0) },
            // Bottom (-Y)
            { { Vec3(x0, y0, z1), Vec3(x0, y0, z0), Vec3(x1, y0, z0), Vec3(x1, y0, z1) }, Vec3(0, -1, 0) },
            // Right (+X)
            { { Vec3(x1, y0, z0), Vec3(x1, y1, z0), Vec3(x1, y1, z1), Vec3(x1, y0, z1) }, Vec3(1, 0, 0) },
            // Left (-X)
            { { Vec3(x0, y0, z1), Vec3(x0, y1, z1), Vec3(x0, y1, z0), Vec3(x0, y0, z0) }, Vec3(-1, 0, 0) },
            // Front (+Z)
            { { Vec3(x1, y0, z1), Vec3(x1, y1, z1), Vec3(x0, y1, z1), Vec3(x0, y0, z1) }, Vec3(0, 0, 1) },
            // Back (-Z)
            { { Vec3(x0, y0, z0), Vec3(x0, y1, z0), Vec3(x1, y1, z0), Vec3(x1, y0, z0) }, Vec3(0, 0, -1) }
        };

        // 2. Test if plane intersects the box (check if vertices exist on both sides)
        int frontCount = 0;
        int backCount = 0;
        for (float x : { x0, x1 }) {
            for (float y : { y0, y1 }) {
                for (float z : { z0, z1 }) {
                    float d = Vec3::dot(Vec3(x, y, z) - planePoint, planeNormal);
                    if (d > 1e-4f) frontCount++;
                    else if (d < -1e-4f) backCount++;
                }
            }
        }

        if (frontCount == 0 || backCount == 0) {
            // Box is entirely on one side of plane
            result.didIntersect = false;
            return result;
        }

        result.didIntersect = true;
        std::vector<PolyFace> frontFaces;
        std::vector<PolyFace> backFaces;
        std::vector<Vec3> cutIntersections;

        // 3. Clip each face against the plane
        for (const auto& face : boxFaces) {
            PolyFace fFront, fBack;
            clipPolygonAgainstPlane(face, planePoint, planeNormal, fFront, fBack, cutIntersections);
            if (fFront.points.size() >= 3) frontFaces.push_back(fFront);
            if (fBack.points.size() >= 3) backFaces.push_back(fBack);
        }

        // 4. Construct cap faces on the cut plane to make water-tight closed solid meshes
        // Filter unique intersection points
        std::vector<Vec3> uniqueCapPts;
        for (const auto& pt : cutIntersections) {
            bool dup = false;
            for (const auto& u : uniqueCapPts) {
                if ((pt - u).lengthSq() < 1e-7f) {
                    dup = true;
                    break;
                }
            }
            if (!dup) uniqueCapPts.push_back(pt);
        }

        if (uniqueCapPts.size() >= 3) {
            // Calculate center of cut face
            Vec3 capCenter(0, 0, 0);
            for (const auto& pt : uniqueCapPts) capCenter += pt;
            capCenter = capCenter * (1.0f / (float)uniqueCapPts.size());

            // Build tangent frame in cut plane
            Vec3 uAxis = (std::abs(planeNormal.y) < 0.9f)
                       ? Vec3::cross(planeNormal, Vec3(0, 1, 0)).normalized()
                       : Vec3::cross(planeNormal, Vec3(1, 0, 0)).normalized();
            Vec3 vAxis = Vec3::cross(planeNormal, uAxis).normalized();

            // Sort points by polar angle around cut plane normal
            std::sort(uniqueCapPts.begin(), uniqueCapPts.end(), [&](const Vec3& a, const Vec3& b) {
                Vec3 da = a - capCenter;
                Vec3 db = b - capCenter;
                float angleA = std::atan2(Vec3::dot(da, vAxis), Vec3::dot(da, uAxis));
                float angleB = std::atan2(Vec3::dot(db, vAxis), Vec3::dot(db, uAxis));
                return angleA < angleB;
            });

            // For front piece, the outward cap normal is -planeNormal
            PolyFace capFront;
            capFront.normal = -planeNormal;
            capFront.points = uniqueCapPts;
            std::reverse(capFront.points.begin(), capFront.points.end()); // Invert winding for -planeNormal
            frontFaces.push_back(capFront);

            // For back piece, the outward cap normal is +planeNormal
            PolyFace capBack;
            capBack.normal = planeNormal;
            capBack.points = uniqueCapPts;
            backFaces.push_back(capBack);
        }

        // 5. Triangulate faces into final mesh buffers
        for (const auto& f : frontFaces) {
            triangulatePolygon(f, uvScale, result.frontPiece);
        }
        for (const auto& b : backFaces) {
            triangulatePolygon(b, uvScale, result.backPiece);
        }

        // 6. Compute bounds & centroids
        computeBoundsAndCentroid(result.frontPiece);
        computeBoundsAndCentroid(result.backPiece);

        return result;
    }

    ClipResult CSGTool::sliceConvexMesh(const std::vector<Vertex>& inVertices,
                                        const std::vector<unsigned int>& inIndices,
                                        const Vec3& planePoint, const Vec3& planeNormIn,
                                        const Vec2& uvScale) {
        ClipResult result;
        if (inVertices.empty() || inIndices.empty()) return result;

        Vec3 planeNormal = planeNormIn.normalized();
        // Extract polygonal faces from indexed triangles
        std::vector<PolyFace> meshFaces;
        for (size_t i = 0; i + 2 < inIndices.size(); i += 3) {
            PolyFace f;
            f.points = {
                inVertices[inIndices[i + 0]].position,
                inVertices[inIndices[i + 1]].position,
                inVertices[inIndices[i + 2]].position
            };
            Vec3 edge1 = f.points[1] - f.points[0];
            Vec3 edge2 = f.points[2] - f.points[0];
            f.normal = Vec3::cross(edge1, edge2).normalized();
            meshFaces.push_back(f);
        }

        std::vector<PolyFace> frontFaces;
        std::vector<PolyFace> backFaces;
        std::vector<Vec3> cutIntersections;

        for (const auto& face : meshFaces) {
            PolyFace fFront, fBack;
            clipPolygonAgainstPlane(face, planePoint, planeNormal, fFront, fBack, cutIntersections);
            if (fFront.points.size() >= 3) frontFaces.push_back(fFront);
            if (fBack.points.size() >= 3) backFaces.push_back(fBack);
        }

        if (frontFaces.empty() || backFaces.empty()) {
            result.didIntersect = false;
            return result;
        }

        result.didIntersect = true;

        // Cap polygons
        std::vector<Vec3> uniqueCapPts;
        for (const auto& pt : cutIntersections) {
            bool dup = false;
            for (const auto& u : uniqueCapPts) {
                if ((pt - u).lengthSq() < 1e-7f) {
                    dup = true;
                    break;
                }
            }
            if (!dup) uniqueCapPts.push_back(pt);
        }

        if (uniqueCapPts.size() >= 3) {
            Vec3 capCenter(0, 0, 0);
            for (const auto& pt : uniqueCapPts) capCenter += pt;
            capCenter = capCenter * (1.0f / (float)uniqueCapPts.size());

            Vec3 uAxis = (std::abs(planeNormal.y) < 0.9f)
                       ? Vec3::cross(planeNormal, Vec3(0, 1, 0)).normalized()
                       : Vec3::cross(planeNormal, Vec3(1, 0, 0)).normalized();
            Vec3 vAxis = Vec3::cross(planeNormal, uAxis).normalized();

            std::sort(uniqueCapPts.begin(), uniqueCapPts.end(), [&](const Vec3& a, const Vec3& b) {
                Vec3 da = a - capCenter;
                Vec3 db = b - capCenter;
                float angleA = std::atan2(Vec3::dot(da, vAxis), Vec3::dot(da, uAxis));
                float angleB = std::atan2(Vec3::dot(db, vAxis), Vec3::dot(db, uAxis));
                return angleA < angleB;
            });

            PolyFace capFront;
            capFront.normal = -planeNormal;
            capFront.points = uniqueCapPts;
            std::reverse(capFront.points.begin(), capFront.points.end());
            frontFaces.push_back(capFront);

            PolyFace capBack;
            capBack.normal = planeNormal;
            capBack.points = uniqueCapPts;
            backFaces.push_back(capBack);
        }

        for (const auto& f : frontFaces) triangulatePolygon(f, uvScale, result.frontPiece);
        for (const auto& b : backFaces) triangulatePolygon(b, uvScale, result.backPiece);

        computeBoundsAndCentroid(result.frontPiece);
        computeBoundsAndCentroid(result.backPiece);

        return result;
    }

} // namespace Lab
