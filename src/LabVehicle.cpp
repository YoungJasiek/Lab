#include "LabVehicle.h"

namespace Lab {

    RaycastVehicle::RaycastVehicle(float mass, const Vec3& pos)
        : _mass(mass), _position(pos) {}

    int RaycastVehicle::addWheel(const Vec3& localPos, float radius, float restLength,
                                 float stiffness, float damping,
                                 bool isSteering, bool isDrive) {
        Wheel w;
        w.localPosition = localPos;
        w.radius = radius;
        w.restLength = restLength;
        w.springStiffness = stiffness;
        w.springDamping = damping;
        w.isSteering = isSteering;
        w.isDrive = isDrive;

        int idx = static_cast<int>(_wheels.size());
        _wheels.push_back(w);
        return idx;
    }

    void RaycastVehicle::setThrottle(float throttle) {
        _throttle = std::clamp(throttle, -1.0f, 1.0f);
    }

    void RaycastVehicle::setSteering(float steer) {
        _steering = std::clamp(steer, -1.0f, 1.0f);
    }

    void RaycastVehicle::setBrake(float brake) {
        _brake = std::clamp(brake, 0.0f, 1.0f);
    }

    void RaycastVehicle::step(float dt, const Vec3& gravity, const RaycastCallback& raycastFn) {
        if (dt <= 0.0f) return;

        // 1. Gravity & damping
        _velocity += gravity * dt;
        _velocity *= 0.998f;

        // Forward vector based on yaw rotation
        float yawRad = _yaw;
        Vec3 forward(std::sin(yawRad), 0.0f, std::cos(yawRad));
        Vec3 right(std::cos(yawRad), 0.0f, -std::sin(yawRad));

        // 2. Wheel suspension & contact forces
        Vec3 totalSuspensionForce(0, 0, 0);
        int groundedCount = 0;

        for (auto& w : _wheels) {
            // Compute world position of wheel mount
            Vec3 mountPos = _position + right * w.localPosition.x + Vec3(0, w.localPosition.y, 0) + forward * w.localPosition.z;
            Vec3 rayDir(0.0f, -1.0f, 0.0f);
            float maxTrace = w.restLength + w.radius;

            Vec3 hitPoint, hitNorm;
            if (raycastFn && raycastFn(mountPos, rayDir, maxTrace, hitPoint, hitNorm)) {
                w.isGrounded = true;
                groundedCount++;

                float dist = (mountPos - hitPoint).length();
                float compression = (w.restLength + w.radius) - dist;
                w.currentCompression = std::max(0.0f, compression);

                // Spring Hooke force + Damper
                float springF = w.currentCompression * w.springStiffness;
                float damperF = -_velocity.y * w.springDamping;
                w.suspensionForce = std::max(0.0f, springF + damperF);

                totalSuspensionForce += hitNorm * w.suspensionForce;
                w.worldPosition = hitPoint + hitNorm * w.radius;
            } else {
                w.isGrounded = false;
                w.currentCompression = 0.0f;
                w.suspensionForce = 0.0f;
                w.worldPosition = mountPos + rayDir * w.restLength;
            }
        }

        // Apply suspension forces
        _velocity += (totalSuspensionForce * (1.0f / _mass)) * dt;

        // 3. Engine drive force & lateral grip
        if (groundedCount > 0) {
            float driveForce = _throttle * 4000.0f;
            _velocity += (forward * driveForce * (1.0f / _mass)) * dt;

            // Lateral friction (kill sideways slip)
            float lateralSpeed = Vec3::dot(_velocity, right);
            _velocity -= right * (lateralSpeed * 0.9f);

            // Brake
            if (_brake > 0.0f) {
                _velocity *= (1.0f - _brake * 0.5f * dt);
            }

            // Steering yaw rate proportional to forward speed
            float forwardSpeed = Vec3::dot(_velocity, forward);
            _yaw += _steering * forwardSpeed * 0.1f * dt;
        }

        // 4. Integrate chassis position
        _position += _velocity * dt;
    }

} // namespace Lab
