#pragma once
#include <vector>
#include <cmath>
#include <functional>
#include <algorithm>
#include "LabMath.h"

namespace Lab {

    struct Wheel {
        Vec3 localPosition;
        float radius{0.35f};
        float restLength{0.5f};
        float springStiffness{25000.0f};
        float springDamping{3000.0f};
        bool isSteering{false};
        bool isDrive{false};

        // Dynamic state
        float currentCompression{0.0f};
        float suspensionForce{0.0f};
        Vec3 worldPosition{0, 0, 0};
        bool isGrounded{false};
    };

    class RaycastVehicle {
    public:
        using RaycastCallback = std::function<bool(const Vec3& start, const Vec3& dir, float maxDist, Vec3& outHit, Vec3& outNorm)>;

        RaycastVehicle(float mass = 1200.0f, const Vec3& pos = Vec3(0, 1.0f, 0));

        int addWheel(const Vec3& localPos, float radius = 0.35f, float restLength = 0.5f,
                     float stiffness = 25000.0f, float damping = 3000.0f,
                     bool isSteering = false, bool isDrive = false);

        void setThrottle(float throttle);
        void setSteering(float steer);
        void setBrake(float brake);

        const Vec3& getPosition() const { return _position; }
        const Vec3& getVelocity() const { return _velocity; }
        const std::vector<Wheel>& getWheels() const { return _wheels; }

        void step(float dt, const Vec3& gravity, const RaycastCallback& raycastFn);

    private:
        float _mass{1200.0f};
        Vec3 _position{0, 1.0f, 0};
        Vec3 _velocity{0, 0, 0};
        float _yaw{0.0f};

        float _throttle{0.0f};
        float _steering{0.0f};
        float _brake{0.0f};

        std::vector<Wheel> _wheels;
    };

} // namespace Lab
