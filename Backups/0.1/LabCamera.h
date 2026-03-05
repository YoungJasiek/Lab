#pragma once
#include "LabMath.h"

namespace Lab {

    class Camera {
    public:
        Camera(float fov, float aspect, float near, float far)
            : _fov(fov), _aspect(aspect), _near(near), _far(far),
              _position(0.0f, 1.8f, 0.0f), _yaw(-90.0f), _pitch(0.0f) {
            updateVectors();
        }

        void update(const Vec2& mouseDelta) {
            float sensitivity = 0.15f;
            _yaw += mouseDelta.x * sensitivity;
            _pitch -= mouseDelta.y * sensitivity;

            if (_pitch > 89.0f) _pitch = 89.0f;
            if (_pitch < -89.0f) _pitch = -89.0f;

            updateVectors();
        }

        void move(const Vec3& direction, float speed) {
            _position += direction * speed;
        }

        void setPosition(const Vec3& position) {
            _position = position;
        }

        Mat4 getViewMatrix() const {
            return Mat4::lookAt(_position, _position + _front, _up);
        }

        Mat4 getProjectionMatrix() const {
            return Mat4::perspective(_fov * (3.141592f / 180.0f), _aspect, _near, _far);
        }

        Vec3 getPosition() const { return _position; }
        Vec3 getFront() const { return _front; }
        Vec3 getRight() const { return _right; }

    private:
        void updateVectors() {
            float yawRad = _yaw * (3.141592f / 180.0f);
            float pitchRad = _pitch * (3.141592f / 180.0f);

            _front.x = std::cos(yawRad) * std::cos(pitchRad);
            _front.y = std::sin(pitchRad);
            _front.z = std::sin(yawRad) * std::cos(pitchRad);
            _front = _front.normalized();

            _right = Vec3::cross(_front, Vec3(0, 1, 0)).normalized();
            _up = Vec3::cross(_right, _front).normalized();
        }

        Vec3 _position;
        Vec3 _front;
        Vec3 _up;
        Vec3 _right;

        float _yaw, _pitch;
        float _fov, _aspect, _near, _far;
    };
}
