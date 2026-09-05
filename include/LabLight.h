#pragma once
#include "LabMath.h"

namespace Lab {

    struct Flashlight {
        bool enabled = false;
        Vec3 position = { 0.0f, 0.0f, 0.0f };
        Vec3 direction = { 0.0f, 0.0f, -1.0f };
        Vec3 color = { 1.0f, 0.98f, 0.92f };
        float intensity = 2.2f;
        float innerCone = 0.9781f; // cos(12 degrees)
        float outerCone = 0.9510f; // cos(18 degrees)
        float range = 42.0f;

        void toggle() { enabled = !enabled; }

        void update(const Vec3& eyePos, const Vec3& forward, const Vec3& right, const Vec3& up) {
            // Tactical chest/shoulder harness mounting (CS:GO / Half-Life 2 style)
            position = eyePos + right * 0.14f - up * 0.10f;
            // Point slightly converged toward crosshair at 18m distance
            Vec3 target = eyePos + forward * 18.0f;
            direction = (target - position).normalized();
        }

        Mat4 getLightSpaceMatrix() const {
            // Perspective matrix matching outer cone angle (36 deg total FOV + safety margin)
            float spotFov = 42.0f * (3.14159265f / 180.0f);
            Mat4 proj = Mat4::perspective(spotFov, 1.0f, 0.1f, range);
            Mat4 view = Mat4::lookAt(position, position + direction, Vec3(0, 1, 0));
            return proj * view;
        }
    };

    class ShadowMap {
    public:
        ShadowMap();
        ~ShadowMap();

        ShadowMap(const ShadowMap&) = delete;
        ShadowMap& operator=(const ShadowMap&) = delete;
        ShadowMap(ShadowMap&& other) noexcept;
        ShadowMap& operator=(ShadowMap&& other) noexcept;

        bool init(int width = 2048, int height = 2048);
        void shutdown();

        void beginShadowPass(const Mat4& lightSpaceMatrix);
        void endShadowPass(int screenWidth, int screenHeight);

        void bind(unsigned int textureUnit = 1) const;

        unsigned int getFbo() const { return _fbo; }
        unsigned int getDepthTexture() const { return _depthTexture; }
        const Mat4& getLightSpaceMatrix() const { return _lightSpaceMatrix; }
        int getWidth() const { return _width; }
        int getHeight() const { return _height; }
        bool isInitialized() const { return _initialized; }

        // Helper to compute directional sun light space matrix
        static Mat4 computeSunLightSpaceMatrix(const Vec3& sunDirection, const Vec3& centerPos, float orthoSize = 45.0f);

    private:
        unsigned int _fbo = 0;
        unsigned int _depthTexture = 0;
        int _width = 2048;
        int _height = 2048;
        Mat4 _lightSpaceMatrix;
        bool _initialized = false;
    };

} // namespace Lab
