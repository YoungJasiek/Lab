#pragma once

#include "LabMath.h"
#include <cstddef>
#include <vector>

namespace Lab {

    struct ClothParticle {
        Vec3 position{0, 0, 0};
        Vec3 prevPosition{0, 0, 0};
        Vec3 acceleration{0, 0, 0};
        float mass{0.1f};
        float invMass{10.0f};
        bool isPinned{false};

        void pin() noexcept {
            isPinned = true;
            invMass = 0.0f;
        }

        void unpin(float m = 0.1f) noexcept {
            isPinned = false;
            mass = m;
            invMass = (m > 0.0001f) ? (1.0f / m) : 0.0f;
        }
    };

    struct ClothSpring {
        std::size_t pA{0};
        std::size_t pB{0};
        float restLength{1.0f};
        float stiffness{1.0f}; // 0..1
    };

    // Verlet / PBD cloth simulation. Implementation lives in LabCloth.cpp (LabPhysics.dll).
    class ClothMesh {
    public:
        std::size_t gridWidth{0};
        std::size_t gridHeight{0};
        std::vector<ClothParticle> particles;
        std::vector<ClothSpring> springs;

        ClothMesh() = default;

        // Initialize a rectangular cloth grid (structural + shear springs)
        void initGrid(std::size_t width, std::size_t height, float spacing, const Vec3& origin = Vec3(0, 0, 0));

        void applyForce(const Vec3& force);
        void applyWind(const Vec3& windVelocity);

        void step(float dt, const Vec3& gravity, int solverIterations = 4, float damping = 0.99f);

        // Sphere collider check (bone/body collision for cape / clothing)
        void collideSphere(const Vec3& sphereCenter, float sphereRadius);

        void stepWithSphere(float dt, const Vec3& gravity, const Vec3& sphereCenter, float sphereRadius,
                            int solverIterations = 4, float damping = 0.99f);

    private:
        void integrate(float dt, const Vec3& gravity, float damping);
        void solveSprings();
    };

} // namespace Lab
