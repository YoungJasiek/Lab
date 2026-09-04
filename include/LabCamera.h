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
        Vec3 getForward() const { return _front; }
        Vec3 getRight() const { return _right; }
        Vec3 getUp() const { return _up; }
        float getFov() const { return _fov; }
        float getAspect() const { return _aspect; }

        struct FrustumPlane {
            Vec3 normal;
            float d = 0.0f;
        };

        void updateFrustum() const {
            Mat4 vp = getProjectionMatrix() * getViewMatrix();

            // Left plane
            _planes[0].normal.x = vp.m[3] + vp.m[0];
            _planes[0].normal.y = vp.m[7] + vp.m[4];
            _planes[0].normal.z = vp.m[11] + vp.m[8];
            _planes[0].d = vp.m[15] + vp.m[12];

            // Right plane
            _planes[1].normal.x = vp.m[3] - vp.m[0];
            _planes[1].normal.y = vp.m[7] - vp.m[4];
            _planes[1].normal.z = vp.m[11] - vp.m[8];
            _planes[1].d = vp.m[15] - vp.m[12];

            // Bottom plane
            _planes[2].normal.x = vp.m[3] + vp.m[1];
            _planes[2].normal.y = vp.m[7] + vp.m[5];
            _planes[2].normal.z = vp.m[11] + vp.m[9];
            _planes[2].d = vp.m[15] + vp.m[13];

            // Top plane
            _planes[3].normal.x = vp.m[3] - vp.m[1];
            _planes[3].normal.y = vp.m[7] - vp.m[5];
            _planes[3].normal.z = vp.m[11] - vp.m[9];
            _planes[3].d = vp.m[15] - vp.m[13];

            // Near plane
            _planes[4].normal.x = vp.m[3] + vp.m[2];
            _planes[4].normal.y = vp.m[7] + vp.m[6];
            _planes[4].normal.z = vp.m[11] + vp.m[10];
            _planes[4].d = vp.m[15] + vp.m[14];

            // Far plane
            _planes[5].normal.x = vp.m[3] - vp.m[2];
            _planes[5].normal.y = vp.m[7] - vp.m[6];
            _planes[5].normal.z = vp.m[11] - vp.m[10];
            _planes[5].d = vp.m[15] - vp.m[14];

            for (int i = 0; i < 6; ++i) {
                float len = _planes[i].normal.length();
                if (len > 0.00001f) {
                    _planes[i].normal = _planes[i].normal / len;
                    _planes[i].d /= len;
                }
            }
        }

        // Fast AABB vs Frustum Culling: "To czego oko nie widzi tego maszyna renderowac nie musi"
        bool isInFrustum(const Vec3& min, const Vec3& max) const {
            for (int i = 0; i < 6; ++i) {
                Vec3 p(
                    _planes[i].normal.x > 0 ? max.x : min.x,
                    _planes[i].normal.y > 0 ? max.y : min.y,
                    _planes[i].normal.z > 0 ? max.z : min.z
                );
                if (Vec3::dot(_planes[i].normal, p) + _planes[i].d < 0.0f) {
                    return false; // Completely outside view frustum!
                }
            }
            return true;
        }

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

            updateFrustum();
        }

        Vec3 _position;
        Vec3 _front;
        Vec3 _up;
        Vec3 _right;

        float _yaw, _pitch;
        float _fov, _aspect, _near, _far;
        mutable FrustumPlane _planes[6];
    };
}
