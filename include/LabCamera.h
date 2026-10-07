#pragma once
#include "LabComponent.h"
#include "LabTransform.h"
#include "LabEntity.h"
#include "LabMath.h"
#include <algorithm>
#include <cmath>

namespace Lab {

    enum class CameraProjection {
        Perspective,
        Orthographic
    };

    enum class ClearFlags {
        Skybox,
        SolidColor,
        DepthOnly,
        Nothing
    };

    /// @brief Universal Engine Camera component and viewport rendering definition
    class LAB_CORE_API Camera : public Component {
    public:
        // --- 1. Podstawowe właściwości kamery (C++ & Lua) ---
        float fieldOfView{60.0f};          // Kąt widzenia (FOV) w stopniach (tylko Perspective)
        float nearClipPlane{0.3f};         // Minimalna odległość renderowania
        float farClipPlane{1000.0f};       // Maksymalna odległość renderowania
        bool orthographic{false};          // Perspektywa vs ortograficzna
        float orthographicSize{5.0f};      // Rozmiar dla trybu ortograficznego
        Color backgroundColor{0.0f, 0.0f, 0.0f, 1.0f}; // Kolor tła
        ClearFlags clearFlags{ClearFlags::SolidColor};  // Tryb czyszczenia tła

        // --- 2. Główne właściwości (panel edytora & zaawansowane) ---
        uint32_t cullingMask{0xFFFFFFFF};  // Maska warstw (Layers) do renderowania
        int depth{0};                      // Priorytet renderowania (wyższy = renderowany na wierzchu)
        Rect rect{0.0f, 0.0f, 1.0f, 1.0f}; // Viewport na ekranie [x, y, w, h] w zakresie 0..1
        uint32_t targetTexture{0};         // OpenGL FBO / Texture ID (0 = domyślny bufor ekranu)

        // Konstruktory
        Camera() {
            _fov = fieldOfView;
            _near = nearClipPlane;
            _far = farClipPlane;
            updateVectors();
        }

        Camera(float fov, float aspect, float nearPlane, float farPlane)
            : fieldOfView(fov), nearClipPlane(nearPlane), farClipPlane(farPlane),
              _fov(fov), _aspect(aspect), _near(nearPlane), _far(farPlane),
              _position(0.0f, 1.8f, 0.0f), _yaw(-90.0f), _pitch(0.0f) {
            orthographic = false;
            updateVectors();
        }

        Camera(float orthoSize, float aspect, float nearPlane, float farPlane, CameraProjection proj)
            : nearClipPlane(nearPlane), farClipPlane(farPlane),
              orthographicSize(orthoSize),
              _fov(60.0f), _aspect(aspect), _near(nearPlane), _far(farPlane),
              _position(0.0f, 0.0f, 10.0f), _yaw(-90.0f), _pitch(0.0f),
              _orthoSize(orthoSize) {
            orthographic = (proj == CameraProjection::Orthographic);
            updateVectors();
        }

        virtual ~Camera() = default;

        void setViewport(float w, float h) {
            if (h > 0.0f) setAspect(w / h);
        }

        // Synchronizacja właściwości
        void syncProperties() {
            _fov = fieldOfView;
            _near = nearClipPlane;
            _far = farClipPlane;
            _orthoSize = orthographicSize;
        }

        // --- Konwersje i rzutowania ---

        // Widok [0..1] -> Promień w świecie 3D
        Ray ViewportPointToRay(const Vec3& viewportPos) const {
            Vec3 pos = getEffectivePosition();
            float ndcX = (2.0f * viewportPos.x) - 1.0f;
            float ndcY = 1.0f - (2.0f * viewportPos.y);

            if (orthographic) {
                float halfH = orthographicSize * 0.5f;
                float halfW = halfH * _aspect;
                Vec3 origin = pos + getEffectiveRight() * (ndcX * halfW) + getEffectiveUp() * (ndcY * halfH);
                return Ray(origin, getEffectiveFront());
            }

            float tanFov = std::tan((fieldOfView * 0.5f) * (3.14159265f / 180.0f));
            Vec3 rayView(ndcX * tanFov * _aspect, ndcY * tanFov, -1.0f);
            Vec3 rayDir = (getEffectiveRight() * rayView.x + getEffectiveUp() * rayView.y - getEffectiveFront() * rayView.z).normalized();
            return Ray(pos, rayDir);
        }

        Ray ScreenPointToRay(const Vec2& screenPos, float screenW, float screenH) const {
            if (screenW <= 0.0f) screenW = 1920.0f;
            if (screenH <= 0.0f) screenH = 1080.0f;
            Vec3 vp(screenPos.x / screenW, screenPos.y / screenH, 0.0f);
            return ViewportPointToRay(vp);
        }

        // Ekran -> Świat
        Vec3 ScreenToWorldPoint(const Vec3& screenPos, float screenW = 0.0f, float screenH = 0.0f) const {
            if (screenW <= 0.0f) screenW = 1920.0f;
            if (screenH <= 0.0f) screenH = 1080.0f;
            float ndcX = (2.0f * screenPos.x) / screenW - 1.0f;
            float ndcY = 1.0f - (2.0f * screenPos.y) / screenH;

            Vec3 pos = getEffectivePosition();
            if (orthographic) {
                float halfH = orthographicSize * 0.5f;
                float halfW = halfH * _aspect;
                return pos + getEffectiveRight() * (ndcX * halfW) + getEffectiveUp() * (ndcY * halfH);
            }

            float tanFov = std::tan((fieldOfView * 0.5f) * (3.14159265f / 180.0f));
            Vec3 rayView(ndcX * tanFov * _aspect, ndcY * tanFov, 1.0f);
            Vec3 rayDir = (getEffectiveRight() * rayView.x + getEffectiveUp() * rayView.y + getEffectiveFront() * rayView.z).normalized();
            float dist = screenPos.z > 0.0f ? screenPos.z : 10.0f;
            return pos + rayDir * dist;
        }

        // Świat -> Ekran
        Vec3 WorldToScreenPoint(const Vec3& worldPos, float screenW = 0.0f, float screenH = 0.0f) const {
            if (screenW <= 0.0f) screenW = 1920.0f;
            if (screenH <= 0.0f) screenH = 1080.0f;

            Mat4 vp = getProjectionMatrix() * getViewMatrix();
            // Vector4 multiplication
            float x = vp.m[0] * worldPos.x + vp.m[4] * worldPos.y + vp.m[8] * worldPos.z + vp.m[12];
            float y = vp.m[1] * worldPos.x + vp.m[5] * worldPos.y + vp.m[9] * worldPos.z + vp.m[13];
            float z = vp.m[2] * worldPos.x + vp.m[6] * worldPos.y + vp.m[10] * worldPos.z + vp.m[14];
            float w = vp.m[3] * worldPos.x + vp.m[7] * worldPos.y + vp.m[11] * worldPos.z + vp.m[15];

            if (std::abs(w) > 1e-6f) {
                x /= w;
                y /= w;
                z /= w;
            }

            float screenX = (x + 1.0f) * 0.5f * screenW;
            float screenY = (1.0f - y) * 0.5f * screenH;
            return Vec3(screenX, screenY, z);
        }

        // --- Macierze Widoku i Projekcji ---
        Mat4 getViewMatrix() const {
            if (entity && entity->transform) {
                Vec3 pos = entity->transform->getWorldPosition();
                Vec3 fwd = entity->transform->forward();
                Vec3 up = entity->transform->up();
                return Mat4::lookAt(pos, pos + fwd, up);
            }
            return Mat4::lookAt(_position, _position + _front, _up);
        }

        Mat4 getProjectionMatrix(float customAspect = 0.0f) const {
            float aspect = (customAspect > 0.0f) ? customAspect : _aspect;
            if (orthographic) {
                float halfH = orthographicSize * 0.5f;
                float halfW = halfH * aspect;
                return Mat4::ortho(-halfW, halfW, -halfH, halfH, nearClipPlane, farClipPlane);
            }
            return Mat4::perspective(fieldOfView * (3.14159265f / 180.0f), aspect, nearClipPlane, farClipPlane);
        }

        // --- Metody Pomocnicze dla kompatybilności wstecznej ---
        void setOrthographic(float orthoSize, float nearPlane = -100.0f, float farPlane = 100.0f) {
            orthographic = true;
            orthographicSize = orthoSize;
            nearClipPlane = nearPlane;
            farClipPlane = farPlane;
            _orthoSize = orthoSize;
            _near = nearPlane;
            _far = farPlane;
            updateVectors();
        }

        void setPerspective(float fov, float aspect, float nearPlane = 0.1f, float farPlane = 1000.0f) {
            orthographic = false;
            fieldOfView = fov;
            _aspect = aspect;
            nearClipPlane = nearPlane;
            farClipPlane = farPlane;
            _fov = fov;
            _near = nearPlane;
            _far = farPlane;
            updateVectors();
        }

        void setProjectionMode(CameraProjection proj) {
            orthographic = (proj == CameraProjection::Orthographic);
            updateVectors();
        }

        CameraProjection getProjectionMode() const {
            return orthographic ? CameraProjection::Orthographic : CameraProjection::Perspective;
        }

        bool isOrthographic() const { return orthographic; }
        float getOrthoSize() const { return orthographicSize; }
        void setOrthoSize(float size) {
            orthographicSize = size;
            _orthoSize = size;
            updateVectors();
        }

        void update(const Vec2& mouseDelta) {
            float sensitivity = 0.15f;
            _yaw += mouseDelta.x * sensitivity;
            // Zawijanie yaw w zakresie [0, 360) dla zachowania stabilności numerycznej
            _yaw = std::fmod(_yaw, 360.0f);
            if (_yaw < 0.0f) _yaw += 360.0f;

            _pitch -= mouseDelta.y * sensitivity;
            if (_pitch > 89.0f) _pitch = 89.0f;
            if (_pitch < -89.0f) _pitch = -89.0f;

            updateVectors();
        }

        void move(const Vec3& direction, float speed) {
            if (entity && entity->transform) {
                entity->transform->position += direction * speed;
            } else {
                _position += direction * speed;
            }
        }

        void setPosition(const Vec3& position) {
            if (entity && entity->transform) {
                entity->transform->position = position;
            }
            _position = position;
        }

        void setYaw(float yaw) {
            _yaw = std::fmod(yaw, 360.0f);
            if (_yaw < 0.0f) _yaw += 360.0f;
            updateVectors();
        }

        void setPitch(float pitch) {
            _pitch = std::clamp(pitch, -89.0f, 89.0f);
            updateVectors();
        }

        void lookAt(const Vec3& target) {
            Vec3 pos = getEffectivePosition();
            Vec3 dir = (target - pos).normalized();
            _pitch = std::asin(std::clamp(dir.y, -1.0f, 1.0f)) * (180.0f / 3.14159265f);
            _yaw = std::atan2(dir.z, dir.x) * (180.0f / 3.14159265f);
            if (entity && entity->transform) {
                entity->transform->LookAt(target);
            }
            updateVectors();
        }

        Vec3 getPosition() const { return getEffectivePosition(); }
        Vec3 getFront() const { return getEffectiveFront(); }
        Vec3 getForward() const { return getEffectiveFront(); }
        Vec3 getRight() const { return getEffectiveRight(); }
        Vec3 getUp() const { return getEffectiveUp(); }
        float getFov() const { return fieldOfView; }
        float getAspect() const { return _aspect; }
        float getYaw() const { return _yaw; }
        float getPitch() const { return _pitch; }

        void setAspect(float aspect) {
            _aspect = aspect;
            updateVectors();
        }

        Vec3 screenToWorldRay(float screenX, float screenY, float screenW, float screenH) const {
            return ScreenPointToRay(Vec2(screenX, screenY), screenW, screenH).direction;
        }

        Vec3 screenToWorldPoint2D(float screenX, float screenY, float screenW, float screenH) const {
            return ScreenToWorldPoint(Vec3(screenX, screenY, 0.0f), screenW, screenH);
        }

        struct FrustumPlane {
            Vec3 normal;
            float d{0.0f};
        };

        void updateFrustum() const {
            Mat4 vp = getProjectionMatrix() * getViewMatrix();

            _planes[0].normal.x = vp.m[3] + vp.m[0];
            _planes[0].normal.y = vp.m[7] + vp.m[4];
            _planes[0].normal.z = vp.m[11] + vp.m[8];
            _planes[0].d        = vp.m[15] + vp.m[12];

            _planes[1].normal.x = vp.m[3] - vp.m[0];
            _planes[1].normal.y = vp.m[7] - vp.m[4];
            _planes[1].normal.z = vp.m[11] - vp.m[8];
            _planes[1].d        = vp.m[15] - vp.m[12];

            _planes[2].normal.x = vp.m[3] + vp.m[1];
            _planes[2].normal.y = vp.m[7] + vp.m[5];
            _planes[2].normal.z = vp.m[11] + vp.m[9];
            _planes[2].d        = vp.m[15] + vp.m[13];

            _planes[3].normal.x = vp.m[3] - vp.m[1];
            _planes[3].normal.y = vp.m[7] - vp.m[5];
            _planes[3].normal.z = vp.m[11] - vp.m[9];
            _planes[3].d        = vp.m[15] - vp.m[13];

            _planes[4].normal.x = vp.m[3] + vp.m[2];
            _planes[4].normal.y = vp.m[7] + vp.m[6];
            _planes[4].normal.z = vp.m[11] + vp.m[10];
            _planes[4].d        = vp.m[15] + vp.m[14];

            _planes[5].normal.x = vp.m[3] - vp.m[2];
            _planes[5].normal.y = vp.m[7] - vp.m[6];
            _planes[5].normal.z = vp.m[11] - vp.m[10];
            _planes[5].d        = vp.m[15] - vp.m[14];

            for (int i = 0; i < 6; ++i) {
                float len = _planes[i].normal.length();
                if (len > 0.00001f) {
                    _planes[i].normal = _planes[i].normal / len;
                    _planes[i].d /= len;
                }
            }
        }

        bool isInFrustum(const Vec3& min, const Vec3& max, float margin = 0.75f) const {
            for (int i = 0; i < 6; ++i) {
                Vec3 p(
                    _planes[i].normal.x > 0 ? (max.x + margin) : (min.x - margin),
                    _planes[i].normal.y > 0 ? (max.y + margin) : (min.y - margin),
                    _planes[i].normal.z > 0 ? (max.z + margin) : (min.z - margin)
                );
                if (Vec3::dot(_planes[i].normal, p) + _planes[i].d < 0.0f) {
                    return false;
                }
            }
            return true;
        }

    private:
        Vec3 getEffectivePosition() const {
            if (entity && entity->transform) {
                return entity->transform->getWorldPosition();
            }
            return _position;
        }

        Vec3 getEffectiveFront() const {
            if (entity && entity->transform) {
                return entity->transform->forward();
            }
            return _front;
        }

        Vec3 getEffectiveRight() const {
            if (entity && entity->transform) {
                return entity->transform->right();
            }
            return _right;
        }

        Vec3 getEffectiveUp() const {
            if (entity && entity->transform) {
                return entity->transform->up();
            }
            return _up;
        }

        void updateVectors() {
            float yawRad = _yaw * (3.141592f / 180.0f);
            float pitchRad = _pitch * (3.141592f / 180.0f);

            _front.x = std::cos(yawRad) * std::cos(pitchRad);
            _front.y = std::sin(pitchRad);
            _front.z = std::sin(yawRad) * std::cos(pitchRad);
            _front = _front.normalized();

            _right = Vec3::cross(_front, Vec3(0, 1, 0)).normalized();
            _up = Vec3::cross(_right, _front).normalized();

            if (entity && entity->transform) {
                entity->transform->rotation = Quat::fromEuler(_pitch * (3.14159265f / 180.0f), _yaw * (3.14159265f / 180.0f), 0.0f);
            }

            updateFrustum();
        }

        Vec3 _position{0.0f, 1.8f, 0.0f};
        Vec3 _front{0.0f, 0.0f, -1.0f};
        Vec3 _up{0.0f, 1.0f, 0.0f};
        Vec3 _right{1.0f, 0.0f, 0.0f};

        float _yaw{-90.0f};
        float _pitch{0.0f};
        float _fov{60.0f};
        float _aspect{16.0f / 9.0f};
        float _near{0.3f};
        float _far{1000.0f};
        float _orthoSize{5.0f};

        mutable FrustumPlane _planes[6];
    };

} // namespace Lab
