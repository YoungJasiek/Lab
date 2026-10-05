#include "LabCloth.h"

#include <cmath>

namespace Lab {

    namespace {
        constexpr float kMinDistance = 1e-6f;
        constexpr float kStructuralStiffness = 1.0f;
        constexpr float kShearStiffness = 0.8f;
        constexpr float kWindDragCoefficient = 2.0f;
    }

    void ClothMesh::initGrid(std::size_t width, std::size_t height, float spacing, const Vec3& origin) {
        gridWidth = width;
        gridHeight = height;

        particles.assign(width * height, ClothParticle{});
        springs.clear();
        if (width == 0 || height == 0) return;

        // Upper bound: 4 springs per particle (right, down, 2x shear)
        springs.reserve(width * height * 4);

        const float shearLength = std::sqrt(2.0f) * spacing;

        for (std::size_t y = 0; y < height; ++y) {
            for (std::size_t x = 0; x < width; ++x) {
                const std::size_t idx = y * width + x;
                const Vec3 pos = origin + Vec3(static_cast<float>(x) * spacing, 0.0f, static_cast<float>(y) * spacing);

                ClothParticle& p = particles[idx];
                p.position = pos;
                p.prevPosition = pos;
                p.acceleration = Vec3(0, 0, 0);
                p.unpin(0.1f);

                // Structural right
                if (x + 1 < width) {
                    springs.push_back({ idx, idx + 1, spacing, kStructuralStiffness });
                }
                // Structural down
                if (y + 1 < height) {
                    springs.push_back({ idx, idx + width, spacing, kStructuralStiffness });
                }
                // Shear diagonal (down-right)
                if (x + 1 < width && y + 1 < height) {
                    springs.push_back({ idx, idx + width + 1, shearLength, kShearStiffness });
                }
                // Shear diagonal (down-left)
                if (x > 0 && y + 1 < height) {
                    springs.push_back({ idx, idx + width - 1, shearLength, kShearStiffness });
                }
            }
        }
    }

    void ClothMesh::applyForce(const Vec3& force) {
        for (auto& p : particles) {
            if (!p.isPinned && p.invMass > 0.0f) {
                p.acceleration += force * p.invMass;
            }
        }
    }

    void ClothMesh::applyWind(const Vec3& windVelocity) {
        // Aerodynamic drag proportional to relative wind velocity
        for (auto& p : particles) {
            if (p.isPinned) continue;
            const Vec3 particleVel = p.position - p.prevPosition;
            const Vec3 relVel = windVelocity - particleVel;
            p.acceleration += relVel * kWindDragCoefficient;
        }
    }

    void ClothMesh::integrate(float dt, const Vec3& gravity, float damping) {
        // Position Verlet integration
        const float dt2 = dt * dt;
        for (auto& p : particles) {
            if (p.isPinned) continue;

            const Vec3 current = p.position;
            const Vec3 velocity = (p.position - p.prevPosition) * damping;
            p.position = p.position + velocity + (gravity + p.acceleration) * dt2;
            p.prevPosition = current;
            p.acceleration = Vec3(0, 0, 0);
        }
    }

    void ClothMesh::solveSprings() {
        // Single Gauss-Seidel PBD relaxation pass over all distance constraints
        for (const auto& s : springs) {
            ClothParticle& pA = particles[s.pA];
            ClothParticle& pB = particles[s.pB];

            const float totalInvMass = pA.invMass + pB.invMass;
            if (totalInvMass <= 0.0f) continue;

            const Vec3 delta = pB.position - pA.position;
            const float dist = delta.length();
            if (dist < kMinDistance) continue;

            const float diff = (dist - s.restLength) / dist;
            const Vec3 correction = delta * (diff * s.stiffness);

            if (!pA.isPinned) pA.position += correction * (pA.invMass / totalInvMass);
            if (!pB.isPinned) pB.position -= correction * (pB.invMass / totalInvMass);
        }
    }

    void ClothMesh::step(float dt, const Vec3& gravity, int solverIterations, float damping) {
        integrate(dt, gravity, damping);
        for (int iter = 0; iter < solverIterations; ++iter) {
            solveSprings();
        }
    }

    void ClothMesh::collideSphere(const Vec3& sphereCenter, float sphereRadius) {
        for (auto& p : particles) {
            if (p.isPinned) continue;
            const Vec3 diff = p.position - sphereCenter;
            const float dist = diff.length();
            if (dist < sphereRadius && dist > kMinDistance) {
                p.position = sphereCenter + (diff / dist) * sphereRadius;
            }
        }
    }

    void ClothMesh::stepWithSphere(float dt, const Vec3& gravity, const Vec3& sphereCenter, float sphereRadius,
                                   int solverIterations, float damping) {
        integrate(dt, gravity, damping);
        // Interleave constraint relaxation with collision projection for stable contact
        for (int iter = 0; iter < solverIterations; ++iter) {
            solveSprings();
            collideSphere(sphereCenter, sphereRadius);
        }
    }

} // namespace Lab
