#pragma once
#include <memory>
#include <string>
#include "LabMath.h"

namespace Lab {

    class Shader;

    enum class TonemapperType {
        ACESFilmic = 0,
        Reinhard = 1
    };

    class PostProcessPipeline {
    public:
        PostProcessPipeline();
        ~PostProcessPipeline();

        // Non-copyable, movable (RAII)
        PostProcessPipeline(const PostProcessPipeline&) = delete;
        PostProcessPipeline& operator=(const PostProcessPipeline&) = delete;
        PostProcessPipeline(PostProcessPipeline&& other) noexcept;
        PostProcessPipeline& operator=(PostProcessPipeline&& other) noexcept;

        bool init(int width, int height);
        void shutdown();
        void resize(int width, int height);

        // Begins rendering 3D scene into HDR framebuffer (MRT: Scene Color + Bright Pass)
        void beginScene();
        // Ends rendering into HDR framebuffer
        void endScene();

        // Performs bloom downsample/blur passes and composites to screen with tonemapping & cryo frost
        void render(float exposure = 1.0f, float frostIntensity = 0.0f, float timeSeconds = 0.0f, TonemapperType tonemapper = TonemapperType::ACESFilmic);

        // Accessors for diagnostics & tests
        unsigned int getHDRTexture() const { return _hdrColorTexture; }
        unsigned int getBrightTexture() const { return _brightColorTexture; }
        unsigned int getBloomTexture() const { return _pingPongColor[0]; }
        unsigned int getHDRFbo() const { return _hdrFbo; }
        bool isInitialized() const { return _initialized; }
        int getWidth() const { return _width; }
        int getHeight() const { return _height; }

        // Math utilities for tests
        static Vec3 acesFilmicTonemap(const Vec3& color);
        static Vec3 reinhardTonemap(const Vec3& color);
        static float calculateFrostVignette(float health, float maxHealth, bool isCryoSector);

    private:
        int _width = 1280;
        int _height = 720;
        bool _initialized = false;

        // Primary HDR Framebuffer (GL_RGBA16F, MRT)
        unsigned int _hdrFbo = 0;
        unsigned int _hdrColorTexture = 0;      // Attachment 0: Full unclamped scene
        unsigned int _brightColorTexture = 0;   // Attachment 1: Thresholded bright highlights (> 1.0)
        unsigned int _depthStencilRbo = 0;

        // Downsampled Bloom Ping-Pong Framebuffers (W/2, H/2)
        int _bloomWidth = 640;
        int _bloomHeight = 360;
        unsigned int _pingPongFbo[2] = { 0, 0 };
        unsigned int _pingPongColor[2] = { 0, 0 };

        // Fullscreen Quad Geometry
        unsigned int _quadVao = 0;
        unsigned int _quadVbo = 0;

        // Shaders
        std::unique_ptr<Shader> _blurShader;
        std::unique_ptr<Shader> _compositeShader;

        void setupQuad();
        void initFramebuffers();
        void initShaders();
        void destroyFramebuffers();
    };

} // namespace Lab
