#include "LabGraphics.h"
#include <glad/gl.h>
#include "stb_image.h"
#include <iostream>
#include <cmath>
#include <algorithm>
#include <fstream>
#include <sstream>

namespace Lab {

    // =========================================================================
    // 1. HDRI SKYBOX (.hdr) & IBL AMBIENT
    // =========================================================================
    HDRISkybox::HDRISkybox() {}

    HDRISkybox::~HDRISkybox() {
        shutdown();
    }

    HDRISkybox::HDRISkybox(HDRISkybox&& other) noexcept
        : _textureId(other._textureId), _width(other._width), _height(other._height),
          _exposure(other._exposure), _loaded(other._loaded), _path(std::move(other._path)),
          _hdrData(std::move(other._hdrData)) {
        other._textureId = 0;
        other._loaded = false;
    }

    HDRISkybox& HDRISkybox::operator=(HDRISkybox&& other) noexcept {
        if (this != &other) {
            shutdown();
            _textureId = other._textureId;
            _width = other._width;
            _height = other._height;
            _exposure = other._exposure;
            _loaded = other._loaded;
            _path = std::move(other._path);
            _hdrData = std::move(other._hdrData);

            other._textureId = 0;
            other._loaded = false;
        }
        return *this;
    }

    void HDRISkybox::shutdown() {
        if (_textureId != 0) {
            glDeleteTextures(1, &_textureId);
            _textureId = 0;
        }
        _hdrData.clear();
        _loaded = false;
        _width = 0;
        _height = 0;
    }

    bool HDRISkybox::loadFromFile(const std::string& path) {
        shutdown();
        _path = path;

        int comp = 0;
        stbi_set_flip_vertically_on_load(1);
        float* data = stbi_loadf(path.c_str(), &_width, &_height, &comp, 3);
        if (!data) {
            // If file does not exist, fallback to procedural analytical sky
            return createProceduralSky();
        }

        size_t totalFloats = (size_t)_width * (size_t)_height * 3;
        _hdrData.assign(data, data + totalFloats);
        stbi_image_free(data);

        glGenTextures(1, &_textureId);
        glBindTexture(GL_TEXTURE_2D, _textureId);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, _width, _height, 0, GL_RGB, GL_FLOAT, _hdrData.data());

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glBindTexture(GL_TEXTURE_2D, 0);

        _loaded = true;
        return true;
    }

    bool HDRISkybox::createProceduralSky(const Vec3& sunDir,
                                         const Vec3& zenithColor,
                                         const Vec3& horizonColor,
                                         const Vec3& groundColor,
                                         int width, int height) {
        shutdown();
        _width = width;
        _height = height;
        _path = "procedural://sky.hdr";

        size_t totalPixels = (size_t)_width * (size_t)_height;
        _hdrData.resize(totalPixels * 3);

        Vec3 normSun = sunDir.normalized();

        for (int y = 0; y < _height; ++y) {
            float v = (float)y / (float)_height;
            float phi = (v - 0.5f) * 3.14159265f; // -pi/2 to pi/2

            for (int x = 0; x < _width; ++x) {
                float u = (float)x / (float)_width;
                float theta = (u - 0.5f) * 2.0f * 3.14159265f; // -pi to pi

                Vec3 dir = {
                    std::cos(phi) * std::sin(theta),
                    std::sin(phi),
                    std::cos(phi) * std::cos(theta)
                };
                dir = dir.normalized();

                Vec3 pixelColor;
                if (dir.y >= 0.0f) {
                    float t = std::pow(std::clamp(dir.y, 0.0f, 1.0f), 0.7f);
                    pixelColor = horizonColor * (1.0f - t) + zenithColor * t;
                } else {
                    float t = std::clamp(-dir.y, 0.0f, 1.0f);
                    pixelColor = horizonColor * (1.0f - t) + groundColor * t;
                }

                // Sun disc highlight
                float sunDot = std::max(0.0f, dir.dot(-normSun));
                if (sunDot > 0.995f) {
                    float sunIntensity = std::pow((sunDot - 0.995f) / 0.005f, 2.0f) * 15.0f;
                    pixelColor = pixelColor + Vec3(1.0f, 0.95f, 0.85f) * sunIntensity;
                }

                size_t idx = ((size_t)y * (size_t)_width + (size_t)x) * 3;
                _hdrData[idx + 0] = pixelColor.x;
                _hdrData[idx + 1] = pixelColor.y;
                _hdrData[idx + 2] = pixelColor.z;
            }
        }

        glGenTextures(1, &_textureId);
        glBindTexture(GL_TEXTURE_2D, _textureId);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, _width, _height, 0, GL_RGB, GL_FLOAT, _hdrData.data());

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glBindTexture(GL_TEXTURE_2D, 0);

        _loaded = true;
        return true;
    }

    void HDRISkybox::bind(unsigned int slot) const {
        if (_textureId != 0) {
            glActiveTexture(GL_TEXTURE0 + slot);
            glBindTexture(GL_TEXTURE_2D, _textureId);
        }
    }

    Vec3 HDRISkybox::sampleEquirectangular(float u, float v) const {
        if (_hdrData.empty() || _width <= 0 || _height <= 0) return { 0.2f, 0.25f, 0.35f };

        u = u - std::floor(u);
        v = std::clamp(v, 0.0f, 1.0f);

        float fx = u * (_width - 1);
        float fy = v * (_height - 1);
        int x0 = (int)std::floor(fx);
        int y0 = (int)std::floor(fy);
        int x1 = std::min(x0 + 1, _width - 1);
        int y1 = std::min(y0 + 1, _height - 1);

        float tx = fx - x0;
        float ty = fy - y0;

        auto getPixel = [this](int x, int y) -> Vec3 {
            size_t idx = ((size_t)y * (size_t)_width + (size_t)x) * 3;
            return { _hdrData[idx], _hdrData[idx + 1], _hdrData[idx + 2] };
        };

        Vec3 c00 = getPixel(x0, y0);
        Vec3 c10 = getPixel(x1, y0);
        Vec3 c01 = getPixel(x0, y1);
        Vec3 c11 = getPixel(x1, y1);

        Vec3 c0 = c00 * (1.0f - tx) + c10 * tx;
        Vec3 c1 = c01 * (1.0f - tx) + c11 * tx;
        Vec3 res = c0 * (1.0f - ty) + c1 * ty;

        // Apply exposure
        return res * _exposure;
    }

    Vec3 HDRISkybox::sampleDirection(const Vec3& dir) const {
        Vec3 d = dir.normalized();
        float theta = std::atan2(d.x, d.z);
        float phi = std::asin(std::clamp(d.y, -1.0f, 1.0f));

        float u = 0.5f + theta / (2.0f * 3.14159265f);
        float v = 0.5f + phi / 3.14159265f;
        return sampleEquirectangular(u, v);
    }

    Vec3 HDRISkybox::computeIBLAmbient(const Vec3& normal) const {
        if (!_loaded) return { 0.2f, 0.25f, 0.35f };

        // Sample hemisphere around normal
        Vec3 norm = normal.normalized();
        Vec3 up = std::abs(norm.y) < 0.99f ? Vec3(0, 1, 0) : Vec3(1, 0, 0);
        Vec3 right = norm.cross(up).normalized();
        up = right.cross(norm).normalized();

        Vec3 irradiance = { 0, 0, 0 };
        int samples = 16;
        for (int i = 0; i < samples; ++i) {
            float theta = ((float)i / (float)samples) * 1.5707963f; // 0 to pi/2
            float phi = ((float)i * 1.6180339f) * 2.0f * 3.14159265f;

            Vec3 tangentSample = {
                std::sin(theta) * std::cos(phi),
                std::cos(theta),
                std::sin(theta) * std::sin(phi)
            };
            Vec3 sampleVec = right * tangentSample.x + norm * tangentSample.y + up * tangentSample.z;
            irradiance = irradiance + sampleDirection(sampleVec) * std::cos(theta) * std::sin(theta);
        }
        return irradiance * (3.14159265f / (float)samples);
    }

    Vec3 HDRISkybox::computeAverageIrradiance() const {
        if (!_loaded || _hdrData.empty()) return { 0.25f, 0.30f, 0.40f };

        Vec3 sum = { 0, 0, 0 };
        size_t step = std::max((size_t)1, _hdrData.size() / (3 * 256));
        size_t count = 0;

        for (size_t i = 0; i < _hdrData.size(); i += step * 3) {
            sum = sum + Vec3(_hdrData[i], _hdrData[i + 1], _hdrData[i + 2]);
            count++;
        }
        if (count == 0) return { 0.25f, 0.30f, 0.40f };
        return (sum * (1.0f / (float)count)) * _exposure;
    }

    // =========================================================================
    // 2. ATMOSPHERIC HEIGHT & DISTANCE FOG
    // =========================================================================
    float AtmosphericFog::computeDistanceFog(float distance, float startDist, float endDist) {
        if (distance <= startDist) return 0.0f;
        if (distance >= endDist) return 1.0f;
        return (distance - startDist) / (endDist - startDist);
    }

    float AtmosphericFog::computeHeightFog(float cameraY, float fragmentY, float baseHeight, float density, float falloff) {
        float hDiff = fragmentY - cameraY;
        if (std::abs(hDiff) < 0.001f) {
            return std::clamp(density * std::exp(-falloff * std::max(0.0f, cameraY - baseHeight)), 0.0f, 1.0f);
        }
        float a = density * std::exp(-falloff * (cameraY - baseHeight));
        float b = -falloff * hDiff;
        float opticalDepth = (a / b) * (1.0f - std::exp(b));
        return std::clamp(1.0f - std::exp(-std::abs(opticalDepth)), 0.0f, 1.0f);
    }

    float AtmosphericFog::computeCombinedFog(const Vec3& cameraPos, const Vec3& fragmentPos, const FogSettings& settings) {
        if (!settings.enabled) return 0.0f;

        float dist = (fragmentPos - cameraPos).length();
        float distFactor = computeDistanceFog(dist, settings.distanceStart, settings.distanceEnd);
        float heightFactor = computeHeightFog(cameraPos.y, fragmentPos.y, settings.baseHeight, settings.heightDensity, settings.heightFalloff);

        // Volumetric blending: both distance extinction and ground fog accumulation
        float combined = 1.0f - (1.0f - distFactor) * (1.0f - heightFactor);
        return std::clamp(combined, 0.0f, 1.0f);
    }

    Vec3 AtmosphericFog::applyFog(const Vec3& fragmentColor, const Vec3& cameraPos, const Vec3& fragmentPos, const FogSettings& settings) {
        float f = computeCombinedFog(cameraPos, fragmentPos, settings);
        return fragmentColor * (1.0f - f) + settings.color * f;
    }

    // =========================================================================
    // 3. PBR MATERIALS & COOK-TORRANCE BRDF (glTF 2.0 Standard)
    // =========================================================================
    float PBRShading::distributionGGX(const Vec3& N, const Vec3& H, float roughness) {
        float a = roughness * roughness;
        float a2 = a * a;
        float NdotH = std::max(N.dot(H), 0.0f);
        float NdotH2 = NdotH * NdotH;

        float nom = a2;
        float denom = (NdotH2 * (a2 - 1.0f) + 1.0f);
        denom = 3.14159265f * denom * denom;
        return denom > 0.0001f ? (nom / denom) : 0.0f;
    }

    float PBRShading::geometrySchlickGGX(float NdotV, float roughness) {
        float r = (roughness + 1.0f);
        float k = (r * r) / 8.0f;
        float nom = NdotV;
        float denom = NdotV * (1.0f - k) + k;
        return denom > 0.0001f ? (nom / denom) : 0.0f;
    }

    float PBRShading::geometrySmith(const Vec3& N, const Vec3& V, const Vec3& L, float roughness) {
        float NdotV = std::max(N.dot(V), 0.0f);
        float NdotL = std::max(N.dot(L), 0.0f);
        float ggx2 = geometrySchlickGGX(NdotV, roughness);
        float ggx1 = geometrySchlickGGX(NdotL, roughness);
        return ggx1 * ggx2;
    }

    Vec3 PBRShading::fresnelSchlick(float cosTheta, const Vec3& F0) {
        float p = std::pow(std::clamp(1.0f - cosTheta, 0.0f, 1.0f), 5.0f);
        return F0 + (Vec3(1.0f, 1.0f, 1.0f) - F0) * p;
    }

    Vec3 PBRShading::evaluateDirectLight(const PBRMaterial& mat,
                                         const Vec3& N,
                                         const Vec3& V,
                                         const Vec3& L,
                                         const Vec3& lightRadiance) {
        Vec3 H = (V + L).normalized();
        float NdotL = std::max(N.dot(L), 0.0f);
        float NdotV = std::max(N.dot(V), 0.0f);

        if (NdotL <= 0.0f || NdotV <= 0.0f) {
            return mat.emissive;
        }

        // Base reflectivity F0 (0.04 for non-metals, albedo for metals)
        Vec3 baseF0 = Vec3(0.04f, 0.04f, 0.04f);
        Vec3 albedo3 = Vec3(mat.albedoColor.x, mat.albedoColor.y, mat.albedoColor.z);
        Vec3 F0 = baseF0 * (1.0f - mat.metallic) + albedo3 * mat.metallic;

        // Cook-Torrance Specular BRDF terms
        float D = distributionGGX(N, H, mat.roughness);
        float G = geometrySmith(N, V, L, mat.roughness);
        Vec3 F = fresnelSchlick(std::max(H.dot(V), 0.0f), F0);

        Vec3 numerator = F * (D * G);
        float denominator = 4.0f * NdotV * NdotL + 0.0001f;
        Vec3 specular = numerator * (1.0f / denominator);

        // Energy conservation
        Vec3 kS = F;
        Vec3 kD = (Vec3(1.0f, 1.0f, 1.0f) - kS) * (1.0f - mat.metallic);

        // Lambertian diffuse
        Vec3 diffuse = albedo3 * (1.0f / 3.14159265f);

        return (diffuse * kD + specular) * lightRadiance * NdotL + mat.emissive;
    }

    Vec3 PBRShading::evaluateIBL(const PBRMaterial& mat,
                                 const Vec3& N,
                                 const Vec3& V,
                                 const Vec3& irradiance,
                                 const Vec3& prefilteredSpecular) {
        float NdotV = std::max(N.dot(V), 0.0f);
        Vec3 albedo3 = Vec3(mat.albedoColor.x, mat.albedoColor.y, mat.albedoColor.z);
        Vec3 baseF0 = Vec3(0.04f, 0.04f, 0.04f);
        Vec3 F0 = baseF0 * (1.0f - mat.metallic) + albedo3 * mat.metallic;

        Vec3 F = fresnelSchlick(NdotV, F0);
        Vec3 kS = F;
        Vec3 kD = (Vec3(1.0f, 1.0f, 1.0f) - kS) * (1.0f - mat.metallic);

        Vec3 diffuse = irradiance * albedo3;
        Vec3 specular = prefilteredSpecular * (F.x * 0.8f + 0.2f);

        return (diffuse * kD + specular) * mat.ao + mat.emissive;
    }

    // =========================================================================
    // 4. INSTANCING & BATCHING WITH TEXTURE ATLAS
    // =========================================================================
    void TextureAtlas::addTile(const std::string& name, const Vec4& uvRect) {
        _tiles[name] = uvRect;
    }

    Vec4 TextureAtlas::getUVRect(const std::string& name) const {
        auto it = _tiles.find(name);
        if (it != _tiles.end()) return it->second;
        return { 0.0f, 0.0f, 1.0f, 1.0f };
    }

    bool TextureAtlas::hasTile(const std::string& name) const {
        return _tiles.contains(name);
    }

    InstancedMesh::InstancedMesh(const std::vector<Vertex>& vertices, const std::vector<unsigned int>& indices)
        : _indexCount((int)indices.size()) {
        glGenVertexArrays(1, &_vao);
        glGenBuffers(1, &_vbo);
        glGenBuffers(1, &_ebo);
        glGenBuffers(1, &_instanceVbo);

        glBindVertexArray(_vao);

        glBindBuffer(GL_ARRAY_BUFFER, _vbo);
        glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex), vertices.data(), GL_STATIC_DRAW);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, _ebo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);

        // Vertex layout
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, position));
        glEnableVertexAttribArray(0);

        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, normal));
        glEnableVertexAttribArray(1);

        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, texCoords));
        glEnableVertexAttribArray(2);

        glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, color));
        glEnableVertexAttribArray(3);

        // Instance buffer layout (Mat4 = 4 x Vec4 attributes 4..7, UV Atlas = attrib 8, Tint = attrib 9)
        glBindBuffer(GL_ARRAY_BUFFER, _instanceVbo);

        for (int i = 0; i < 4; ++i) {
            glEnableVertexAttribArray(4 + i);
            glVertexAttribPointer(4 + i, 4, GL_FLOAT, GL_FALSE, sizeof(InstanceData), (void*)(sizeof(Vec4) * i));
            glVertexAttribDivisor(4 + i, 1);
        }

        glEnableVertexAttribArray(8);
        glVertexAttribPointer(8, 4, GL_FLOAT, GL_FALSE, sizeof(InstanceData), (void*)offsetof(InstanceData, uvAtlasRect));
        glVertexAttribDivisor(8, 1);

        glEnableVertexAttribArray(9);
        glVertexAttribPointer(9, 4, GL_FLOAT, GL_FALSE, sizeof(InstanceData), (void*)offsetof(InstanceData, colorTint));
        glVertexAttribDivisor(9, 1);

        glBindVertexArray(0);
    }

    InstancedMesh::~InstancedMesh() {
        if (_vao) glDeleteVertexArrays(1, &_vao);
        if (_vbo) glDeleteBuffers(1, &_vbo);
        if (_ebo) glDeleteBuffers(1, &_ebo);
        if (_instanceVbo) glDeleteBuffers(1, &_instanceVbo);
    }

    InstancedMesh::InstancedMesh(InstancedMesh&& other) noexcept
        : _vao(other._vao), _vbo(other._vbo), _ebo(other._ebo), _instanceVbo(other._instanceVbo),
          _indexCount(other._indexCount), _instances(std::move(other._instances)) {
        other._vao = 0;
        other._vbo = 0;
        other._ebo = 0;
        other._instanceVbo = 0;
        other._indexCount = 0;
    }

    InstancedMesh& InstancedMesh::operator=(InstancedMesh&& other) noexcept {
        if (this != &other) {
            if (_vao) glDeleteVertexArrays(1, &_vao);
            if (_vbo) glDeleteBuffers(1, &_vbo);
            if (_ebo) glDeleteBuffers(1, &_ebo);
            if (_instanceVbo) glDeleteBuffers(1, &_instanceVbo);

            _vao = other._vao;
            _vbo = other._vbo;
            _ebo = other._ebo;
            _instanceVbo = other._instanceVbo;
            _indexCount = other._indexCount;
            _instances = std::move(other._instances);

            other._vao = 0;
            other._vbo = 0;
            other._ebo = 0;
            other._instanceVbo = 0;
            other._indexCount = 0;
        }
        return *this;
    }

    void InstancedMesh::clearInstances() {
        _instances.clear();
    }

    void InstancedMesh::addInstance(const Mat4& transform, const Vec4& uvRect, const Vec4& tint) {
        _instances.push_back({ transform, uvRect, tint });
    }

    void InstancedMesh::updateInstanceBuffer() {
        if (_instanceVbo == 0 || _instances.empty()) return;
        glBindBuffer(GL_ARRAY_BUFFER, _instanceVbo);
        glBufferData(GL_ARRAY_BUFFER, _instances.size() * sizeof(InstanceData), _instances.data(), GL_DYNAMIC_DRAW);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
    }

    void InstancedMesh::draw() const {
        if (_vao == 0 || _instances.empty() || _indexCount == 0) return;
        glBindVertexArray(_vao);
        glDrawElementsInstanced(GL_TRIANGLES, _indexCount, GL_UNSIGNED_INT, 0, (GLsizei)_instances.size());
        glBindVertexArray(0);
    }

    // =========================================================================
    // 5. BAKED LIGHTMAPS
    // =========================================================================
    BakedLightmap::BakedLightmap(int width, int height)
        : _width(width), _height(height), _pixels(width * height, Vec3(0.2f, 0.25f, 0.35f)) {}

    BakedLightmap::~BakedLightmap() {
        if (_textureId != 0) {
            glDeleteTextures(1, &_textureId);
            _textureId = 0;
        }
    }

    BakedLightmap::BakedLightmap(BakedLightmap&& other) noexcept
        : _width(other._width), _height(other._height), _textureId(other._textureId),
          _pixels(std::move(other._pixels)) {
        other._textureId = 0;
    }

    BakedLightmap& BakedLightmap::operator=(BakedLightmap&& other) noexcept {
        if (this != &other) {
            if (_textureId != 0) glDeleteTextures(1, &_textureId);
            _width = other._width;
            _height = other._height;
            _textureId = other._textureId;
            _pixels = std::move(other._pixels);
            other._textureId = 0;
        }
        return *this;
    }

    void BakedLightmap::setTexel(int x, int y, const Vec3& color) {
        if (x >= 0 && x < _width && y >= 0 && y < _height) {
            _pixels[(size_t)y * (size_t)_width + (size_t)x] = color;
        }
    }

    Vec3 BakedLightmap::getTexel(int x, int y) const {
        if (x >= 0 && x < _width && y >= 0 && y < _height) {
            return _pixels[(size_t)y * (size_t)_width + (size_t)x];
        }
        return { 0, 0, 0 };
    }

    Vec3 BakedLightmap::sample(float u, float v) const {
        if (_pixels.empty() || _width <= 0 || _height <= 0) return { 0.2f, 0.25f, 0.35f };
        u = std::clamp(u, 0.0f, 1.0f);
        v = std::clamp(v, 0.0f, 1.0f);

        float fx = u * (_width - 1);
        float fy = v * (_height - 1);
        int x0 = (int)std::floor(fx);
        int y0 = (int)std::floor(fy);
        int x1 = std::min(x0 + 1, _width - 1);
        int y1 = std::min(y0 + 1, _height - 1);

        float tx = fx - x0;
        float ty = fy - y0;

        Vec3 c00 = getTexel(x0, y0);
        Vec3 c10 = getTexel(x1, y0);
        Vec3 c01 = getTexel(x0, y1);
        Vec3 c11 = getTexel(x1, y1);

        Vec3 c0 = c00 * (1.0f - tx) + c10 * tx;
        Vec3 c1 = c01 * (1.0f - tx) + c11 * tx;
        return c0 * (1.0f - ty) + c1 * ty;
    }

    void BakedLightmap::upload() {
        if (_pixels.empty()) return;

        std::vector<unsigned char> byteData(_width * _height * 3);
        for (size_t i = 0; i < _pixels.size(); ++i) {
            byteData[i * 3 + 0] = (unsigned char)std::clamp(_pixels[i].x * 255.0f, 0.0f, 255.0f);
            byteData[i * 3 + 1] = (unsigned char)std::clamp(_pixels[i].y * 255.0f, 0.0f, 255.0f);
            byteData[i * 3 + 2] = (unsigned char)std::clamp(_pixels[i].z * 255.0f, 0.0f, 255.0f);
        }

        if (_textureId == 0) {
            glGenTextures(1, &_textureId);
        }
        glBindTexture(GL_TEXTURE_2D, _textureId);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, _width, _height, 0, GL_RGB, GL_UNSIGNED_BYTE, byteData.data());

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glBindTexture(GL_TEXTURE_2D, 0);
    }

    void BakedLightmap::bind(unsigned int slot) const {
        if (_textureId != 0) {
            glActiveTexture(GL_TEXTURE0 + slot);
            glBindTexture(GL_TEXTURE_2D, _textureId);
        }
    }

    bool BakedLightmap::saveToFile(const std::string& path) const {
        std::ofstream out(path, std::ios::binary);
        if (!out.is_open()) return false;

        out.write((const char*)&_width, sizeof(int));
        out.write((const char*)&_height, sizeof(int));
        for (const auto& p : _pixels) {
            out.write((const char*)&p.x, sizeof(float));
            out.write((const char*)&p.y, sizeof(float));
            out.write((const char*)&p.z, sizeof(float));
        }
        return true;
    }

    bool BakedLightmap::loadFromFile(const std::string& path) {
        std::ifstream in(path, std::ios::binary);
        if (!in.is_open()) return false;

        in.read((char*)&_width, sizeof(int));
        in.read((char*)&_height, sizeof(int));
        if (_width <= 0 || _height <= 0 || _width > 4096 || _height > 4096) return false;

        _pixels.resize(_width * _height);
        for (auto& p : _pixels) {
            in.read((char*)&p.x, sizeof(float));
            in.read((char*)&p.y, sizeof(float));
            in.read((char*)&p.z, sizeof(float));
        }
        upload();
        return true;
    }

    std::unique_ptr<BakedLightmap> LightmapBaker::bake(const LabMap& map,
                                                       const Vec3& sunDir,
                                                       const Vec3& sunColor,
                                                       const Vec3& ambientColor,
                                                       int resolution) {
        auto lightmap = std::make_unique<BakedLightmap>(resolution, resolution);
        Vec3 normSun = sunDir.normalized();

        // Sample static scene geometry & compute direct sun diffuse + ambient bounce
        for (int y = 0; y < resolution; ++y) {
            float v = (float)y / (float)resolution;
            for (int x = 0; x < resolution; ++x) {
                float u = (float)x / (float)resolution;

                // Simple hemispherical ambient base + sun directional contribution
                Vec3 worldPos = { (u - 0.5f) * 40.0f, 0.0f, (v - 0.5f) * 40.0f };
                Vec3 normal = { 0.0f, 1.0f, 0.0f };

                float NdotL = std::max(0.0f, normal.dot(-normSun));
                Vec3 directSun = sunColor * NdotL;

                // Check light influences from map point lights
                Vec3 pointLightAccum = { 0, 0, 0 };
                for (const auto& l : map.lights) {
                    Vec3 toL = l.position - worldPos;
                    float dist = toL.length();
                    if (dist < l.radius && dist > 0.001f) {
                        float atten = std::max(0.0f, 1.0f - dist / l.radius);
                        float dot = std::max(0.0f, normal.dot(toL.normalized()));
                        pointLightAccum = pointLightAccum + l.color * (dot * atten * l.intensity);
                    }
                }

                Vec3 finalColor = ambientColor + directSun * 0.8f + pointLightAccum;
                lightmap->setTexel(x, y, finalColor);
            }
        }

        lightmap->upload();
        return lightmap;
    }

    // =========================================================================
    // 6. SHADOW ATLAS
    // =========================================================================
    ShadowAtlas::ShadowAtlas() {}

    ShadowAtlas::~ShadowAtlas() {
        shutdown();
    }

    ShadowAtlas::ShadowAtlas(ShadowAtlas&& other) noexcept
        : _atlasSize(other._atlasSize), _tilesPerRow(other._tilesPerRow),
          _fbo(other._fbo), _depthTexture(other._depthTexture),
          _initialized(other._initialized), _tileMatrices(std::move(other._tileMatrices)) {
        other._fbo = 0;
        other._depthTexture = 0;
        other._initialized = false;
    }

    ShadowAtlas& ShadowAtlas::operator=(ShadowAtlas&& other) noexcept {
        if (this != &other) {
            shutdown();
            _atlasSize = other._atlasSize;
            _tilesPerRow = other._tilesPerRow;
            _fbo = other._fbo;
            _depthTexture = other._depthTexture;
            _initialized = other._initialized;
            _tileMatrices = std::move(other._tileMatrices);

            other._fbo = 0;
            other._depthTexture = 0;
            other._initialized = false;
        }
        return *this;
    }

    bool ShadowAtlas::init(int atlasSize, int tilesPerRow) {
        shutdown();
        _atlasSize = atlasSize;
        _tilesPerRow = std::max(1, tilesPerRow);
        int totalTiles = _tilesPerRow * _tilesPerRow;
        _tileMatrices.assign(totalTiles, Mat4::identity());

        glGenFramebuffers(1, &_fbo);
        glBindFramebuffer(GL_FRAMEBUFFER, _fbo);

        glGenTextures(1, &_depthTexture);
        glBindTexture(GL_TEXTURE_2D, _depthTexture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, _atlasSize, _atlasSize, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, _depthTexture, 0);
        glDrawBuffer(GL_NONE);
        glReadBuffer(GL_NONE);

        GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        _initialized = (status == GL_FRAMEBUFFER_COMPLETE);
        return _initialized;
    }

    void ShadowAtlas::shutdown() {
        if (_fbo != 0) {
            glDeleteFramebuffers(1, &_fbo);
            _fbo = 0;
        }
        if (_depthTexture != 0) {
            glDeleteTextures(1, &_depthTexture);
            _depthTexture = 0;
        }
        _initialized = false;
        _tileMatrices.clear();
    }

    Vec4 ShadowAtlas::getTileViewport(int tileIdx) const {
        if (_tilesPerRow <= 0) return { 0, 0, (float)_atlasSize, (float)_atlasSize };
        int tileRes = getTileResolution();
        int tileX = tileIdx % _tilesPerRow;
        int tileY = tileIdx / _tilesPerRow;
        return { (float)(tileX * tileRes), (float)(tileY * tileRes), (float)tileRes, (float)tileRes };
    }

    void ShadowAtlas::setTileLightMatrix(int tileIdx, const Mat4& matrix) {
        if (tileIdx >= 0 && tileIdx < (int)_tileMatrices.size()) {
            _tileMatrices[tileIdx] = matrix;
        }
    }

    Mat4 ShadowAtlas::getTileLightMatrix(int tileIdx) const {
        if (tileIdx >= 0 && tileIdx < (int)_tileMatrices.size()) {
            return _tileMatrices[tileIdx];
        }
        return Mat4::identity();
    }

    void ShadowAtlas::beginTile(int tileIdx) {
        if (!_initialized) return;
        glBindFramebuffer(GL_FRAMEBUFFER, _fbo);

        Vec4 vp = getTileViewport(tileIdx);
        glViewport((int)vp.x, (int)vp.y, (int)vp.z, (int)vp.w);
        glEnable(GL_SCISSOR_TEST);
        glScissor((int)vp.x, (int)vp.y, (int)vp.z, (int)vp.w);
        glClear(GL_DEPTH_BUFFER_BIT);
        glDisable(GL_SCISSOR_TEST);
    }

    void ShadowAtlas::endTile() {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    void ShadowAtlas::bind(unsigned int unit) const {
        if (_depthTexture != 0) {
            glActiveTexture(GL_TEXTURE0 + unit);
            glBindTexture(GL_TEXTURE_2D, _depthTexture);
        }
    }

    // =========================================================================
    // 7. CUBEMAP PROBE (Local Reflection & Environment Probe)
    // =========================================================================
    CubemapProbe::CubemapProbe() {}

    CubemapProbe::~CubemapProbe() {
        shutdown();
    }

    CubemapProbe::CubemapProbe(CubemapProbe&& other) noexcept
        : _position(other._position), _radius(other._radius), _faceResolution(other._faceResolution),
          _fbo(other._fbo), _cubemapTexture(other._cubemapTexture), _depthRbo(other._depthRbo),
          _initialized(other._initialized) {
        other._fbo = 0;
        other._cubemapTexture = 0;
        other._depthRbo = 0;
        other._initialized = false;
    }

    CubemapProbe& CubemapProbe::operator=(CubemapProbe&& other) noexcept {
        if (this != &other) {
            shutdown();
            _position = other._position;
            _radius = other._radius;
            _faceResolution = other._faceResolution;
            _fbo = other._fbo;
            _cubemapTexture = other._cubemapTexture;
            _depthRbo = other._depthRbo;
            _initialized = other._initialized;

            other._fbo = 0;
            other._cubemapTexture = 0;
            other._depthRbo = 0;
            other._initialized = false;
        }
        return *this;
    }

    bool CubemapProbe::init(int faceResolution) {
        shutdown();
        _faceResolution = faceResolution;

        glGenFramebuffers(1, &_fbo);
        glGenRenderbuffers(1, &_depthRbo);

        glGenTextures(1, &_cubemapTexture);
        glBindTexture(GL_TEXTURE_CUBE_MAP, _cubemapTexture);

        for (int i = 0; i < 6; ++i) {
            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB16F,
                         _faceResolution, _faceResolution, 0, GL_RGB, GL_FLOAT, nullptr);
        }

        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

        glBindFramebuffer(GL_FRAMEBUFFER, _fbo);
        glBindRenderbuffer(GL_RENDERBUFFER, _depthRbo);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, _faceResolution, _faceResolution);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, _depthRbo);

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glBindTexture(GL_TEXTURE_CUBE_MAP, 0);

        _initialized = true;
        return true;
    }

    void CubemapProbe::shutdown() {
        if (_fbo != 0) {
            glDeleteFramebuffers(1, &_fbo);
            _fbo = 0;
        }
        if (_depthRbo != 0) {
            glDeleteRenderbuffers(1, &_depthRbo);
            _depthRbo = 0;
        }
        if (_cubemapTexture != 0) {
            glDeleteTextures(1, &_cubemapTexture);
            _cubemapTexture = 0;
        }
        _initialized = false;
    }

    Mat4 CubemapProbe::getFaceViewMatrix(int faceIdx) const {
        // 0: +X, 1: -X, 2: +Y, 3: -Y, 4: +Z, 5: -Z
        switch (faceIdx) {
            case 0: return Mat4::lookAt(_position, _position + Vec3( 1,  0,  0), Vec3(0, -1,  0));
            case 1: return Mat4::lookAt(_position, _position + Vec3(-1,  0,  0), Vec3(0, -1,  0));
            case 2: return Mat4::lookAt(_position, _position + Vec3( 0,  1,  0), Vec3(0,  0,  1));
            case 3: return Mat4::lookAt(_position, _position + Vec3( 0, -1,  0), Vec3(0,  0, -1));
            case 4: return Mat4::lookAt(_position, _position + Vec3( 0,  0,  1), Vec3(0, -1,  0));
            case 5: return Mat4::lookAt(_position, _position + Vec3( 0,  0, -1), Vec3(0, -1,  0));
            default: return Mat4::identity();
        }
    }

    Mat4 CubemapProbe::getProjectionMatrix() const {
        // 90 degree FOV, aspect ratio 1.0, near 0.1, far = radius
        return Mat4::perspective(1.5707963f, 1.0f, 0.1f, _radius);
    }

    void CubemapProbe::beginFace(int faceIdx) {
        if (!_initialized) return;
        glBindFramebuffer(GL_FRAMEBUFFER, _fbo);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X + faceIdx, _cubemapTexture, 0);
        glViewport(0, 0, _faceResolution, _faceResolution);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    }

    void CubemapProbe::endFace() {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    void CubemapProbe::bind(unsigned int slot) const {
        if (_cubemapTexture != 0) {
            glActiveTexture(GL_TEXTURE0 + slot);
            glBindTexture(GL_TEXTURE_CUBE_MAP, _cubemapTexture);
        }
    }

    void CubemapProbe::bakeProceduralEnvironment(const Vec3& ambientColor, const Vec3& sunDir, const Vec3& sunColor) {
        if (!_initialized) return;

        glBindTexture(GL_TEXTURE_CUBE_MAP, _cubemapTexture);
        std::vector<float> faceData(_faceResolution * _faceResolution * 3);

        for (int f = 0; f < 6; ++f) {
            for (int y = 0; y < _faceResolution; ++y) {
                for (int x = 0; x < _faceResolution; ++x) {
                    float u = (2.0f * (x + 0.5f) / _faceResolution) - 1.0f;
                    float v = (2.0f * (y + 0.5f) / _faceResolution) - 1.0f;

                    Vec3 dir;
                    switch (f) {
                        case 0: dir = Vec3( 1, -v, -u); break;
                        case 1: dir = Vec3(-1, -v,  u); break;
                        case 2: dir = Vec3( u,  1,  v); break;
                        case 3: dir = Vec3( u, -1, -v); break;
                        case 4: dir = Vec3( u, -v,  1); break;
                        case 5: dir = Vec3(-u, -v, -1); break;
                    }
                    dir = dir.normalized();

                    Vec3 col = ambientColor;
                    float sunDot = std::max(0.0f, dir.dot(-sunDir.normalized()));
                    if (sunDot > 0.95f) {
                        col = col + sunColor * (std::pow(sunDot, 16.0f) * 4.0f);
                    }

                    size_t idx = ((size_t)y * _faceResolution + x) * 3;
                    faceData[idx + 0] = col.x;
                    faceData[idx + 1] = col.y;
                    faceData[idx + 2] = col.z;
                }
            }

            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + f, 0, GL_RGB16F,
                         _faceResolution, _faceResolution, 0, GL_RGB, GL_FLOAT, faceData.data());
        }
        glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
    }

} // namespace Lab
