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

        size_t totalFloats = static_cast<size_t>(_width) * static_cast<size_t>(_height) * 3;
        _hdrData.assign(data, data + totalFloats);
        stbi_image_free(data);

        glCreateTextures(GL_TEXTURE_2D, 1, &_textureId);
        glTextureStorage2D(_textureId, 1, GL_RGB16F, _width, _height);
        glTextureSubImage2D(_textureId, 0, 0, 0, _width, _height, GL_RGB, GL_FLOAT, _hdrData.data());

        glTextureParameteri(_textureId, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTextureParameteri(_textureId, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTextureParameteri(_textureId, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTextureParameteri(_textureId, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

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

        size_t totalPixels = static_cast<size_t>(_width) * static_cast<size_t>(_height);
        _hdrData.resize(totalPixels * 3);

        Vec3 normSun = sunDir.normalized();

        for (int y = 0; y < _height; ++y) {
            float v = static_cast<float>(y) / static_cast<float>(_height);
            float phi = (v - 0.5f) * 3.14159265f; // -pi/2 to pi/2

            for (int x = 0; x < _width; ++x) {
                float u = static_cast<float>(x) / static_cast<float>(_width);
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

                size_t idx = (static_cast<size_t>(y) * static_cast<size_t>(_width) + static_cast<size_t>(x)) * 3;
                _hdrData[idx + 0] = pixelColor.x;
                _hdrData[idx + 1] = pixelColor.y;
                _hdrData[idx + 2] = pixelColor.z;
            }
        }

        glCreateTextures(GL_TEXTURE_2D, 1, &_textureId);
        glTextureStorage2D(_textureId, 1, GL_RGB16F, _width, _height);
        glTextureSubImage2D(_textureId, 0, 0, 0, _width, _height, GL_RGB, GL_FLOAT, _hdrData.data());

        glTextureParameteri(_textureId, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTextureParameteri(_textureId, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTextureParameteri(_textureId, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTextureParameteri(_textureId, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        _loaded = true;
        return true;
    }

    void HDRISkybox::bind(unsigned int slot) const {
        if (_textureId != 0) {
            glBindTextureUnit(slot, _textureId);
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
    // 3. BLINN-PHONG MATERIALS & SOURCE-STYLE SHADING (No PBR per AGENTS.md)
    // =========================================================================
    float BlinnPhongShading::specularTerm(const Vec3& N, const Vec3& H, float shininess) {
        float s = std::max(shininess, 1.0f);
        float NdotH = std::max(N.dot(H), 0.0f);
        constexpr float invEightPi = 1.0f / (8.0f * 3.14159265f);
        return ((s + 8.0f) * invEightPi) * std::pow(NdotH, s);
    }

    Vec3 BlinnPhongShading::fresnelSchlick(float cosTheta, const Vec3& F0) {
        float p = std::pow(std::clamp(1.0f - cosTheta, 0.0f, 1.0f), 5.0f);
        return F0 + (Vec3(1.0f, 1.0f, 1.0f) - F0) * p;
    }

    Vec3 BlinnPhongShading::evaluateDirectLight(const BlinnPhongMaterial& mat,
                                                const Vec3& N,
                                                const Vec3& V,
                                                const Vec3& L,
                                                const Vec3& lightRadiance) {
        float NdotL = std::max(N.dot(L), 0.0f);
        if (NdotL <= 0.0f) {
            return mat.emissive;
        }

        Vec3 H = (V + L).normalized();
        Vec3 albedo3 = Vec3(mat.albedoColor.x, mat.albedoColor.y, mat.albedoColor.z);

        // Derive effective shininess & specular strength (supporting both direct Blinn-Phong and legacy fields)
        float effectiveShininess = (mat.shininess != 32.0f)
            ? std::max(2.0f, mat.shininess)
            : std::max(2.0f, (1.0f - std::clamp(mat.roughness, 0.05f, 0.95f)) * 128.0f);
        float effectiveSpecStrength = (mat.specularStrength != 0.5f)
            ? mat.specularStrength
            : (0.25f + 0.75f * std::clamp(mat.metallic, 0.0f, 1.0f));

        float spec = specularTerm(N, H, effectiveShininess) * effectiveSpecStrength;
        Vec3 specColor = Vec3(1.0f, 1.0f, 1.0f) * (1.0f - mat.metallic) + albedo3 * mat.metallic;

        Vec3 diffuse = albedo3 * (1.0f / 3.14159265f);
        Vec3 specular = specColor * spec;

        return (diffuse + specular) * lightRadiance * NdotL + mat.emissive;
    }

    Vec3 BlinnPhongShading::evaluateEnvironment(const BlinnPhongMaterial& mat,
                                                const Vec3& N,
                                                const Vec3& V,
                                                const Vec3& ambientIrradiance,
                                                const Vec3& envCubemapColor) {
        float NdotV = std::max(N.dot(V), 0.0f);
        Vec3 albedo3 = Vec3(mat.albedoColor.x, mat.albedoColor.y, mat.albedoColor.z);
        Vec3 baseF0 = Vec3(0.04f, 0.04f, 0.04f);
        Vec3 F0 = baseF0 * (1.0f - mat.metallic) + albedo3 * mat.metallic;

        Vec3 rimFresnel = fresnelSchlick(NdotV, F0);
        Vec3 diffuse = ambientIrradiance * albedo3;
        Vec3 envReflection = envCubemapColor * (rimFresnel.x * 0.6f + 0.15f);

        return (diffuse + envReflection) * mat.ao + mat.emissive;
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
        glCreateVertexArrays(1, &_vao);
        glCreateBuffers(1, &_vbo);
        glCreateBuffers(1, &_ebo);
        glCreateBuffers(1, &_instanceVbo);

        glNamedBufferData(_vbo, vertices.size() * sizeof(Vertex), vertices.data(), GL_STATIC_DRAW);
        glNamedBufferData(_ebo, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);

        glVertexArrayVertexBuffer(_vao, 0, _vbo, 0, sizeof(Vertex));
        glVertexArrayElementBuffer(_vao, _ebo);

        // Per-vertex attributes (binding 0)
        glEnableVertexArrayAttrib(_vao, 0);
        glVertexArrayAttribFormat(_vao, 0, 3, GL_FLOAT, GL_FALSE, (GLuint)offsetof(Vertex, position));
        glVertexArrayAttribBinding(_vao, 0, 0);

        glEnableVertexArrayAttrib(_vao, 1);
        glVertexArrayAttribFormat(_vao, 1, 3, GL_FLOAT, GL_FALSE, (GLuint)offsetof(Vertex, normal));
        glVertexArrayAttribBinding(_vao, 1, 0);

        glEnableVertexArrayAttrib(_vao, 2);
        glVertexArrayAttribFormat(_vao, 2, 2, GL_FLOAT, GL_FALSE, (GLuint)offsetof(Vertex, texCoords));
        glVertexArrayAttribBinding(_vao, 2, 0);

        glEnableVertexArrayAttrib(_vao, 3);
        glVertexArrayAttribFormat(_vao, 3, 3, GL_FLOAT, GL_FALSE, (GLuint)offsetof(Vertex, color));
        glVertexArrayAttribBinding(_vao, 3, 0);

        // Per-instance attributes (binding 1: Mat4 = 4 x Vec4 attributes 4..7, UV Atlas = attrib 8, Tint = attrib 9)
        glVertexArrayVertexBuffer(_vao, 1, _instanceVbo, 0, sizeof(InstanceData));
        glVertexArrayBindingDivisor(_vao, 1, 1);

        for (int i = 0; i < 4; ++i) {
            GLuint attr = static_cast<GLuint>(4 + i);
            glEnableVertexArrayAttrib(_vao, attr);
            glVertexArrayAttribFormat(_vao, attr, 4, GL_FLOAT, GL_FALSE, static_cast<GLuint>(sizeof(Vec4) * i));
            glVertexArrayAttribBinding(_vao, attr, 1);
        }

        glEnableVertexArrayAttrib(_vao, 8);
        glVertexArrayAttribFormat(_vao, 8, 4, GL_FLOAT, GL_FALSE, (GLuint)offsetof(InstanceData, uvAtlasRect));
        glVertexArrayAttribBinding(_vao, 8, 1);

        glEnableVertexArrayAttrib(_vao, 9);
        glVertexArrayAttribFormat(_vao, 9, 4, GL_FLOAT, GL_FALSE, (GLuint)offsetof(InstanceData, colorTint));
        glVertexArrayAttribBinding(_vao, 9, 1);
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
        glNamedBufferData(_instanceVbo, _instances.size() * sizeof(InstanceData), _instances.data(), GL_DYNAMIC_DRAW);
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
            glCreateTextures(GL_TEXTURE_2D, 1, &_textureId);
            glTextureStorage2D(_textureId, 1, GL_RGB8, _width, _height);
            glTextureParameteri(_textureId, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTextureParameteri(_textureId, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glTextureParameteri(_textureId, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTextureParameteri(_textureId, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        }
        glTextureSubImage2D(_textureId, 0, 0, 0, _width, _height, GL_RGB, GL_UNSIGNED_BYTE, byteData.data());
    }

    void BakedLightmap::bind(unsigned int slot) const {
        if (_textureId != 0) {
            glBindTextureUnit(slot, _textureId);
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

        glCreateFramebuffers(1, &_fbo);
        glCreateTextures(GL_TEXTURE_2D, 1, &_depthTexture);
        glTextureStorage2D(_depthTexture, 1, GL_DEPTH_COMPONENT24, _atlasSize, _atlasSize);

        glTextureParameteri(_depthTexture, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTextureParameteri(_depthTexture, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTextureParameteri(_depthTexture, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTextureParameteri(_depthTexture, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        glNamedFramebufferTexture(_fbo, GL_DEPTH_ATTACHMENT, _depthTexture, 0);
        glNamedFramebufferDrawBuffer(_fbo, GL_NONE);
        glNamedFramebufferReadBuffer(_fbo, GL_NONE);

        GLenum status = glCheckNamedFramebufferStatus(_fbo, GL_FRAMEBUFFER);
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
            glBindTextureUnit(unit, _depthTexture);
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
        _faceResolution = std::max(1, faceResolution);

        glCreateFramebuffers(1, &_fbo);
        glCreateRenderbuffers(1, &_depthRbo);

        glCreateTextures(GL_TEXTURE_CUBE_MAP, 1, &_cubemapTexture);
        glTextureStorage2D(_cubemapTexture, 1, GL_RGB16F, _faceResolution, _faceResolution);

        glTextureParameteri(_cubemapTexture, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTextureParameteri(_cubemapTexture, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTextureParameteri(_cubemapTexture, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTextureParameteri(_cubemapTexture, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTextureParameteri(_cubemapTexture, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

        glNamedRenderbufferStorage(_depthRbo, GL_DEPTH_COMPONENT24, _faceResolution, _faceResolution);
        glNamedFramebufferRenderbuffer(_fbo, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, _depthRbo);

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
        glNamedFramebufferTextureLayer(_fbo, GL_COLOR_ATTACHMENT0, _cubemapTexture, 0, faceIdx);
        glBindFramebuffer(GL_FRAMEBUFFER, _fbo);
        glViewport(0, 0, _faceResolution, _faceResolution);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    }

    void CubemapProbe::endFace() {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    void CubemapProbe::bind(unsigned int slot) const {
        if (_cubemapTexture != 0) {
            glBindTextureUnit(slot, _cubemapTexture);
        }
    }

    void CubemapProbe::bakeProceduralEnvironment(const Vec3& ambientColor, const Vec3& sunDir, const Vec3& sunColor) {
        if (!_initialized) return;

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

            glTextureSubImage3D(_cubemapTexture, 0, 0, 0, f,
                                _faceResolution, _faceResolution, 1, GL_RGB, GL_FLOAT, faceData.data());
        }
    }

} // namespace Lab
