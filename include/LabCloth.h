#pragma once

#include "LabMath.h"
#include <vector>
#include <cmath>
#include <algorithm>

namespace Lab {

    struct ClothParticle {
        Vec3 position{0, 0, 0};
        Vec3 prevPosition{0, 0, 0};
        Vec3 acceleration{0, 0, 0};
        float mass{0.1f};
        float invMass{10.0f};
        bool isPinned{false};

        void pin() {
            isPinned = true;
            invMass = 0.0f;
        }

        void unpin(float m = 0.1f) {
            isPinned = false;
            mass = m;
            invMass = (m > 0.0001f) ? (1.0f / m) : 0.0f;
        }
    };

    struct ClothSpring {
        size_t pA{0};
        size_t pB{0};
        float restLength{1.0f};
        float stiffness{1.0f}; // 0..1
    };

    class ClothMesh {
    public:
        size_t gridWidth{0};
        size_t gridHeight{0};
        std::vector<ClothParticle> particles;
        std::vector<ClothSpring> springs;

        ClothMesh() = default;

        // Initialize a rectangular cloth grid
        void initGrid(size_t width, size_t height, float spacing, const Vec3& origin = Vec3(0, 0, 0)) {
            gridWidth = width;
            gridHeight = height;
            particles.resize(width * height);
            springs.clear();

            for (size_t y = 0; y < height; ++y) {
                for (size_t x = 0; x < width; ++x) {
                    size_t idx = y * width + x;
                    Vec3 pos = origin + Vec3((float)x * spacing, 0.0f, (float)y * spacing);
                    particles[idx].position = pos;
                    particles[idx].prevPosition = pos;
                    particles[idx].acceleration = Vec3(0, 0, 0);
                    particles[idx].unpin(0.1f);
                }
            }

            // Create structural springs (horizontal and vertical)
            for (size_t y = 0; y < height; ++y) {
                for (size_t x = 0; x < width; ++x) {
                    size_t idx = y * width + x;

                    // Structural right
                    if (x + 1 < width) {
                        size_t nextX = y * width + (x + 1);
                        springs.push_back({ idx, nextX, spacing, 1.0f });
                    }
                    // Structural down
                    if (y + 1 < height) {
                        size_t nextY = (y + 1) * width + x;
                        springs.push_back({ idx, nextY, spacing, 1.0f });
                    }
                    // Shear diagonal 1
                    if (x + 1 < width && y + 1 < height) {
                        size_t diag1 = (y + 1) * width + (x + 1);
                        springs.push_back({ idx, diag1, std::sqrt(2.0f) * spacing, 0.8f });
                    }
                    // Shear diagonal 2
                    if (x > 0 && y + 1 < height) {
                        size_t diag2 = (y + 1) * width + (x - 1);
                        springs.push_back({ idx, diag2, std::sqrt(2.0f) * spacing, 0.8f });
                    }
                }
            }
        }

        void applyForce(const Vec3& force) {
            for (auto& p : particles) {
                if (!p.isPinned && p.invMass > 0.0f) {
                    p.acceleration += force * p.invMass;
                }
            }
        }

        void applyWind(const Vec3& windVelocity) {
            // Apply aerodynamic drag to particles based on wind
            for (auto& p : particles) {
                if (!p.isPinned) {
                    Vec3 particleVel = (p.position - p.prevPosition);
                    Vec3 relVel = windVelocity - particleVel;
                    p.acceleration += relVel * 2.0f;
                }
            }
        }

        void step(float dt, const Vec3& gravity, int solverIterations = 4, float damping = 0.99f) {
            // 1. Verlet numerical integration
            for (auto& p : particles) {
                if (p.isPinned) continue;

                Vec3 temp = p.position;
                Vec3 velocity = (p.position - p.prevPosition) * damping;
                p.position = p.position + velocity + (gravity + p.acceleration) * (dt * dt);
                p.prevPosition = temp;
                p.acceleration = Vec3(0, 0, 0);
            }

            // 2. Relaxation constraints (Gauss-Seidel PBD iterations)
            for (int iter = 0; iter < solverIterations; ++iter) {
                for (const auto& s : springs) {
                    ClothParticle& pA = particles[s.pA];
                    ClothParticle& pB = particles[s.pB];

                    Vec3 delta = pB.position - pA.position;
                    float dist = delta.length();
                    if (dist < 1e-6f) continue;

                    float diff = (dist - s.restLength) / dist;
                    float totalInvMass = pA.invMass + pB.invMass;
                    if (totalInvMass <= 0.0f) continue;

                    Vec3 correction = delta * (diff * s.stiffness);

                    if (!pA.isPinned) {
                        pA.position += correction * (pA.invMass / totalInvMass);
                    }
                    if (!pB.isPinned) {
                        pB.position -= correction * (pB.invMass / totalInvMass);
                    }
                }
            }
        }

        // Sphere collider check (bone/body collision for cape / clothing)
        void collideSphere(const Vec3& sphereCenter, float sphereRadius) {
            for (auto& p : particles) {
                if (p.isPinned) continue;
                Vec3 diff = p.position - sphereCenter;
                float dist = diff.length();
                if (dist < sphereRadius && dist > 1e-6f) {
                    Vec3 normal = diff / dist;
                    p.position = sphereCenter + normal * sphereRadius;
                }
            }
        }

        void stepWithSphere(float dt, const Vec3& gravity, const Vec3& sphereCenter, float sphereRadius, int solverIterations = 4, float damping = 0.99f) {
            // 1. Verlet numerical integration
            for (auto& p : particles) {
                if (p.isPinned) continue;

                Vec3 temp = p.position;
                Vec3 velocity = (p.position - p.prevPosition) * damping;
                p.position = p.position + velocity + (gravity + p.acceleration) * (dt * dt);
                p.prevPosition = temp;
                p.acceleration = Vec3(0, 0, 0);
            }

            // 2. Relaxation constraints + continuous collision projection
            for (int iter = 0; iter < solverIterations; ++iter) {
                for (const auto& s : springs) {
                    ClothParticle& pA = particles[s.pA];
                    ClothParticle& pB = particles[s.pB];

                    Vec3 delta = pB.position - pA.position;
                    float dist = delta.length();
                    if (dist < 1e-6f) continue;

                    float diff = (dist - s.restLength) / dist;
                    float totalInvMass = pA.invMass + pB.invMass;
                    if (totalInvMass <= 0.0f) continue;

                    Vec3 correction = delta * (diff * s.stiffness);

                    if (!pA.isPinned) {
                        pA.position += correction * (pA.invMass / totalInvMass);
                    }
                    if (!pB.isPinned) {
                        pB.position -= correction * (pB.invMass / totalInvMass);
                    }
                }
                collideSphere(sphereCenter, sphereRadius);
            }
        }
    };

} // namespace Lab
