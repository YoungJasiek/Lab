#include "LabLight.h"
#include <glad/gl.h>
#include <iostream>
#include <utility>

namespace Lab {

    ShadowMap::ShadowMap()
        : _fbo(0), _depthTexture(0), _width(2048), _height(2048), _initialized(false) {
    }

    ShadowMap::~ShadowMap() {
        shutdown();
    }

    ShadowMap::ShadowMap(ShadowMap&& other) noexcept
        : _fbo(other._fbo), _depthTexture(other._depthTexture),
          _width(other._width), _height(other._height),
          _lightSpaceMatrix(other._lightSpaceMatrix),
          _initialized(other._initialized) {
        other._fbo = 0;
        other._depthTexture = 0;
        other._initialized = false;
    }

    ShadowMap& ShadowMap::operator=(ShadowMap&& other) noexcept {
        if (this != &other) {
            shutdown();
            _fbo = other._fbo;
            _depthTexture = other._depthTexture;
            _width = other._width;
            _height = other._height;
            _lightSpaceMatrix = other._lightSpaceMatrix;
            _initialized = other._initialized;

            other._fbo = 0;
            other._depthTexture = 0;
            other._initialized = false;
        }
        return *this;
    }

    bool ShadowMap::init(int width, int height) {
        shutdown();
        _width = width;
        _height = height;

        // 1. Create Depth Texture (OpenGL 4.5+ Direct State Access)
        glCreateTextures(GL_TEXTURE_2D, 1, &_depthTexture);
        glTextureStorage2D(_depthTexture, 1, GL_DEPTH_COMPONENT24, _width, _height);

        glTextureParameteri(_depthTexture, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTextureParameteri(_depthTexture, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTextureParameteri(_depthTexture, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
        glTextureParameteri(_depthTexture, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);

        // Clamp to white border: fragments outside shadow frustum are not shadowed
        float borderColor[] = { 1.0f, 1.0f, 1.0f, 1.0f };
        glTextureParameterfv(_depthTexture, GL_TEXTURE_BORDER_COLOR, borderColor);

        // 2. Create Framebuffer Object (DSA)
        glCreateFramebuffers(1, &_fbo);
        glNamedFramebufferTexture(_fbo, GL_DEPTH_ATTACHMENT, _depthTexture, 0);
        glNamedFramebufferDrawBuffer(_fbo, GL_NONE);
        glNamedFramebufferReadBuffer(_fbo, GL_NONE);

        GLenum status = glCheckNamedFramebufferStatus(_fbo, GL_FRAMEBUFFER);
        if (status != GL_FRAMEBUFFER_COMPLETE) {
            std::cerr << "[ShadowMap] Error: Framebuffer incomplete! Status: 0x" << std::hex << status << std::dec << "\n";
            shutdown();
            return false;
        }

        _initialized = true;
        return true;
    }

    void ShadowMap::shutdown() {
        if (_fbo) {
            glDeleteFramebuffers(1, &_fbo);
            _fbo = 0;
        }
        if (_depthTexture) {
            glDeleteTextures(1, &_depthTexture);
            _depthTexture = 0;
        }
        _initialized = false;
    }

    void ShadowMap::beginShadowPass(const Mat4& lightSpaceMatrix) {
        if (!_initialized) return;

        _lightSpaceMatrix = lightSpaceMatrix;
        glViewport(0, 0, _width, _height);
        glBindFramebuffer(GL_FRAMEBUFFER, _fbo);
        glClear(GL_DEPTH_BUFFER_BIT);

        // Front-face culling for shadow casters completely eliminates shadow acne
        glCullFace(GL_FRONT);
    }

    void ShadowMap::endShadowPass(int screenWidth, int screenHeight) {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, screenWidth, screenHeight);
        glCullFace(GL_BACK);
    }

    void ShadowMap::bind(unsigned int textureUnit) const {
        if (_depthTexture) {
            glBindTextureUnit(textureUnit, _depthTexture);
        }
    }

    Mat4 ShadowMap::computeSunLightSpaceMatrix(const Vec3& sunDirection, const Vec3& centerPos, float orthoSize) {
        Vec3 sunNorm = sunDirection.normalized();
        Vec3 lightPos = centerPos - sunNorm * 40.0f;
        Mat4 lightView = Mat4::lookAt(lightPos, centerPos, Vec3(0.0f, 1.0f, 0.0f));
        Mat4 lightProj = Mat4::ortho(-orthoSize, orthoSize, -orthoSize, orthoSize, 1.0f, 90.0f);
        return lightProj * lightView;
    }

} // namespace Lab
