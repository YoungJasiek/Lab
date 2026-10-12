#pragma once

#include "LabMath.h"
#include "LabMap.h"
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <unordered_map>

namespace Lab {

    class Shader;
    class Texture;

    // =========================================================================
    // 1. HDRI SKYBOX (.hdr) & IBL AMBIENT
    // =========================================================================
    class HDRISkybox {
    public:
        HDRISkybox();
        ~HDRISkybox();

        HDRISkybox(const HDRISkybox&) = delete;
        HDRISkybox& operator=(const HDRISkybox&) = delete;
        HDRISkybox(HDRISkybox&& other) noexcept;
        HDRISkybox& operator=(HDRISkybox&& other) noexcept;

        /// @brief Load a Radiance .hdr image from disk
        bool loadFromFile(const std::string& path);

        /// @brief Generate a procedural analytical physical sky gradient with simulated sun disk
        bool createProceduralSky(const Vec3& sunDir = { -0.4f, -0.8f, -0.4f },
                                 const Vec3& zenithColor = { 0.25f, 0.50f, 0.85f },
                                 const Vec3& horizonColor = { 0.70f, 0.80f, 0.90f },
                                 const Vec3& groundColor = { 0.15f, 0.12f, 0.10f },
                                 int width = 512, int height = 256);

        void shutdown();

        /// @brief Binds floating-point HDR texture to given texture slot
        void bind(unsigned int slot = 0) const;

        /// @brief Sample equirectangular coordinates (u in 0..1, v in 0..1)
        Vec3 sampleEquirectangular(float u, float v) const;

        /// @brief Sample 3D direction vector
        Vec3 sampleDirection(const Vec3& dir) const;

        /// @brief Computes diffuse IBL ambient radiance for a given surface normal
        Vec3 computeIBLAmbient(const Vec3& normal) const;

        /// @brief Computes average ambient irradiance across entire hemisphere
        Vec3 computeAverageIrradiance() const;

        void setExposure(float exp) { _exposure = exp; }
        float getExposure() const { return _exposure; }

        unsigned int getTextureId() const { return _textureId; }
        int getWidth() const { return _width; }
        int getHeight() const { return _height; }
        bool isLoaded() const { return _loaded; }
        const std::string& getPath() const { return _path; }

    private:
        unsigned int _textureId = 0;
        int _width = 0;
        int _height = 0;
        float _exposure = 1.0f;
        bool _loaded = false;
        std::string _path;
        std::vector<float> _hdrData; // RGB float components
    };

    // =========================================================================
    // 2. ATMOSPHERIC HEIGHT & DISTANCE FOG
    // =========================================================================
    struct FogSettings {
        bool enabled = true;
        Vec3 color = { 0.5f, 0.6f, 0.7f };
        float distanceStart = 10.0f;
        float distanceEnd = 100.0f;
        float baseHeight = 0.0f;
        float heightDensity = 0.04f;
        float heightFalloff = 0.15f;
    };

    class AtmosphericFog {
    public:
        /// @brief Linear distance fog factor (0.0 = clear, 1.0 = fully fogged)
        static float computeDistanceFog(float distance, float startDist, float endDist);

        /// @brief Exponential height fog factor based on camera and fragment elevation
        static float computeHeightFog(float cameraY, float fragmentY, float baseHeight, float density, float falloff);

        /// @brief Blended combination of height and distance fog
        static float computeCombinedFog(const Vec3& cameraPos, const Vec3& fragmentPos, const FogSettings& settings);

        /// @brief Applies atmospheric fog to an input fragment color
        static Vec3 applyFog(const Vec3& fragmentColor, const Vec3& cameraPos, const Vec3& fragmentPos, const FogSettings& settings);
    };

    // =========================================================================
    // 3. BLINN-PHONG MATERIALS & SOURCE-STYLE SHADING (No PBR per AGENTS.md)
    // =========================================================================
    struct BlinnPhongMaterial {
        std::string name = "DefaultBlinnPhong";
        Vec4 albedoColor = { 1.0f, 1.0f, 1.0f, 1.0f };
        float specularStrength = 0.5f;
        float shininess = 32.0f;
        // Legacy aliases mapped to specular/shininess for compatibility
        float metallic = 0.0f;
        float roughness = 0.5f;
        float ao = 1.0f;
        Vec3 emissive = { 0.0f, 0.0f, 0.0f };

        std::string albedoPath;
        std::string normalPath;
        std::string specularPath;
        std::string aoPath;
        std::string emissivePath;

        unsigned int albedoTex = 0;
        unsigned int normalTex = 0;
        unsigned int specularTex = 0;
        unsigned int aoTex = 0;
        unsigned int emissiveTex = 0;
    };

    class BlinnPhongShading {
    public:
        /// @brief Normalized Blinn-Phong specular highlight term: ((s + 8) / 8pi) * (N.H)^s
        static float specularTerm(const Vec3& N, const Vec3& H, float shininess);

        /// @brief Schlick's Fresnel rim reflection factor for cubemap environment mapping
        static Vec3 fresnelSchlick(float cosTheta, const Vec3& F0);

        /// @brief Evaluates Blinn-Phong direct lighting (Lambertian diffuse + Blinn-Phong specular)
        static Vec3 evaluateDirectLight(const BlinnPhongMaterial& mat,
                                        const Vec3& N,
                                        const Vec3& V,
                                        const Vec3& L,
                                        const Vec3& lightRadiance);

        /// @brief Evaluates ambient + cubemap environment reflection (Source Engine style)
        static Vec3 evaluateEnvironment(const BlinnPhongMaterial& mat,
                                        const Vec3& N,
                                        const Vec3& V,
                                        const Vec3& ambientIrradiance,
                                        const Vec3& envCubemapColor);

        // Compatibility wrappers converting roughness -> shininess for legacy callers
        static float distributionGGX(const Vec3& N, const Vec3& H, float roughness) {
            float shininess = std::max(2.0f, (1.0f - std::clamp(roughness, 0.05f, 0.95f)) * 128.0f);
            return specularTerm(N, H, shininess);
        }
        static float geometrySmith(const Vec3& N, const Vec3& V, const Vec3& L, float) {
            return (N.dot(V) > 0.0f && N.dot(L) > 0.0f) ? 1.0f : 0.0f;
        }
        static Vec3 evaluateIBL(const BlinnPhongMaterial& mat, const Vec3& N, const Vec3& V,
                                const Vec3& irradiance, const Vec3& envColor) {
            return evaluateEnvironment(mat, N, V, irradiance, envColor);
        }
    };

    using PBRMaterial = BlinnPhongMaterial;
    using PBRShading = BlinnPhongShading;

    // =========================================================================
    // 4. INSTANCING & BATCHING WITH TEXTURE ATLAS
    // =========================================================================
    struct InstanceData {
        Mat4 transform = Mat4::identity();
        Vec4 uvAtlasRect = { 0.0f, 0.0f, 1.0f, 1.0f }; // uMin, vMin, uMax, vMax
        Vec4 colorTint = { 1.0f, 1.0f, 1.0f, 1.0f };
    };

    class TextureAtlas {
    public:
        TextureAtlas() = default;

        void addTile(const std::string& name, const Vec4& uvRect);
        Vec4 getUVRect(const std::string& name) const;
        bool hasTile(const std::string& name) const;

    private:
        std::unordered_map<std::string, Vec4> _tiles;
    };

    class InstancedMesh {
    public:
        InstancedMesh(const std::vector<Vertex>& vertices, const std::vector<unsigned int>& indices);
        ~InstancedMesh();

        InstancedMesh(const InstancedMesh&) = delete;
        InstancedMesh& operator=(const InstancedMesh&) = delete;
        InstancedMesh(InstancedMesh&& other) noexcept;
        InstancedMesh& operator=(InstancedMesh&& other) noexcept;

        void clearInstances();
        void addInstance(const Mat4& transform, const Vec4& uvRect = { 0, 0, 1, 1 }, const Vec4& tint = { 1, 1, 1, 1 });
        void updateInstanceBuffer();
        void draw() const;

        size_t getInstanceCount() const { return _instances.size(); }
        int getIndexCount() const { return _indexCount; }

    private:
        unsigned int _vao = 0;
        unsigned int _vbo = 0;
        unsigned int _ebo = 0;
        unsigned int _instanceVbo = 0;
        int _indexCount = 0;
        std::vector<InstanceData> _instances;
    };

    // =========================================================================
    // 5. BAKED LIGHTMAPS
    // =========================================================================
    class BakedLightmap {
    public:
        BakedLightmap(int width = 256, int height = 256);
        ~BakedLightmap();

        BakedLightmap(const BakedLightmap&) = delete;
        BakedLightmap& operator=(const BakedLightmap&) = delete;
        BakedLightmap(BakedLightmap&& other) noexcept;
        BakedLightmap& operator=(BakedLightmap&& other) noexcept;

        void setTexel(int x, int y, const Vec3& color);
        Vec3 getTexel(int x, int y) const;
        Vec3 sample(float u, float v) const;

        void upload();
        void bind(unsigned int slot = 2) const;

        bool saveToFile(const std::string& path) const;
        bool loadFromFile(const std::string& path);

        unsigned int getTextureId() const { return _textureId; }
        int getWidth() const { return _width; }
        int getHeight() const { return _height; }
        const std::vector<Vec3>& getData() const { return _pixels; }

    private:
        int _width = 256;
        int _height = 256;
        unsigned int _textureId = 0;
        std::vector<Vec3> _pixels;
    };

    class LightmapBaker {
    public:
        /// @brief Bakes static scene direct diffuse & ambient lighting into a BakedLightmap
        static std::unique_ptr<BakedLightmap> bake(const LabMap& map,
                                                   const Vec3& sunDir = { -0.4f, -0.8f, -0.4f },
                                                   const Vec3& sunColor = { 1.0f, 0.98f, 0.90f },
                                                   const Vec3& ambientColor = { 0.2f, 0.25f, 0.35f },
                                                   int resolution = 256);
    };

    // =========================================================================
    // 6. SHADOW ATLAS
    // =========================================================================
    class ShadowAtlas {
    public:
        ShadowAtlas();
        ~ShadowAtlas();

        ShadowAtlas(const ShadowAtlas&) = delete;
        ShadowAtlas& operator=(const ShadowAtlas&) = delete;
        ShadowAtlas(ShadowAtlas&& other) noexcept;
        ShadowAtlas& operator=(ShadowAtlas&& other) noexcept;

        /// @brief Initialize unified depth atlas framebuffer (e.g. 2048x2048 with 2x2 or 4x4 tiles)
        bool init(int atlasSize = 2048, int tilesPerRow = 2);
        void shutdown();

        int getTileCount() const { return _tilesPerRow * _tilesPerRow; }
        int getTilesPerRow() const { return _tilesPerRow; }
        int getTileResolution() const { return _atlasSize / _tilesPerRow; }

        Vec4 getTileViewport(int tileIdx) const;
        void setTileLightMatrix(int tileIdx, const Mat4& matrix);
        Mat4 getTileLightMatrix(int tileIdx) const;

        void beginTile(int tileIdx);
        void endTile();

        void bind(unsigned int unit = 1) const;

        unsigned int getDepthTexture() const { return _depthTexture; }
        unsigned int getFbo() const { return _fbo; }
        bool isInitialized() const { return _initialized; }

    private:
        int _atlasSize = 2048;
        int _tilesPerRow = 2;
        unsigned int _fbo = 0;
        unsigned int _depthTexture = 0;
        bool _initialized = false;
        std::vector<Mat4> _tileMatrices;
    };

    // =========================================================================
    // 7. CUBEMAP PROBE (Local Reflection & Environment Probe)
    // =========================================================================
    class CubemapProbe {
    public:
        CubemapProbe();
        ~CubemapProbe();

        CubemapProbe(const CubemapProbe&) = delete;
        CubemapProbe& operator=(const CubemapProbe&) = delete;
        CubemapProbe(CubemapProbe&& other) noexcept;
        CubemapProbe& operator=(CubemapProbe&& other) noexcept;

        bool init(int faceResolution = 256);
        void shutdown();

        void setPosition(const Vec3& pos) { _position = pos; }
        const Vec3& getPosition() const { return _position; }

        void setRadius(float r) { _radius = r; }
        float getRadius() const { return _radius; }

        /// @brief Returns view matrix for cubemap face (0: +X, 1: -X, 2: +Y, 3: -Y, 4: +Z, 5: -Z)
        Mat4 getFaceViewMatrix(int faceIdx) const;
        Mat4 getProjectionMatrix() const;

        void beginFace(int faceIdx);
        void endFace();

        void bind(unsigned int slot = 3) const;

        /// @brief Procedural ambient bake into probe cubemap faces
        void bakeProceduralEnvironment(const Vec3& ambientColor, const Vec3& sunDir, const Vec3& sunColor);

        unsigned int getCubemapTexture() const { return _cubemapTexture; }
        int getResolution() const { return _faceResolution; }
        bool isInitialized() const { return _initialized; }

    private:
        Vec3 _position = { 0.0f, 1.5f, 0.0f };
        float _radius = 20.0f;
        int _faceResolution = 256;
        unsigned int _fbo = 0;
        unsigned int _cubemapTexture = 0;
        unsigned int _depthRbo = 0;
        bool _initialized = false;
    };

} // namespace Lab
