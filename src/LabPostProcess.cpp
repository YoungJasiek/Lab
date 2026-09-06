#include "LabPostProcess.h"
#include "LabRenderer.h"
#include <glad/gl.h>
#include <iostream>
#include <cmath>
#include <algorithm>

namespace Lab {

    // --- Shader Sources ---

    static const char* s_quadVertexShader = R"(
        #version 450 core
        layout (location = 0) in vec2 aPos;
        layout (location = 1) in vec2 aTexCoords;

        out vec2 vTexCoords;

        void main() {
            vTexCoords = aTexCoords;
            gl_Position = vec4(aPos.x, aPos.y, 0.0, 1.0);
        }
    )";

    // Extracts bright fragments exceeding threshold (1.0) and downsamples
    static const char* s_extractBrightFragmentShader = R"(
        #version 450 core
        out vec4 FragColor;
        in vec2 vTexCoords;

        uniform sampler2D uHDRScene;
        uniform float uThreshold;

        void main() {
            vec3 color = texture(uHDRScene, vTexCoords).rgb;
            // Calculate perceived brightness (luminance)
            float brightness = dot(color, vec3(0.2126, 0.7152, 0.0722));
            if (brightness > uThreshold) {
                // Soft knee thresholding
                FragColor = vec4(color, 1.0);
            } else {
                FragColor = vec4(0.0, 0.0, 0.0, 1.0);
            }
        }
    )";

    // Separable 5-tap Gaussian Blur
    static const char* s_blurFragmentShader = R"(
        #version 450 core
        out vec4 FragColor;
        in vec2 vTexCoords;

        uniform sampler2D uImage;
        uniform bool uHorizontal;

        const float weight[5] = float[] (0.227027, 0.1945946, 0.1216216, 0.054054, 0.016216);

        void main() {
            vec2 tex_offset = 1.0 / textureSize(uImage, 0);
            vec3 result = texture(uImage, vTexCoords).rgb * weight[0];
            if (uHorizontal) {
                for (int i = 1; i < 5; ++i) {
                    result += texture(uImage, vTexCoords + vec2(tex_offset.x * i, 0.0)).rgb * weight[i];
                    result += texture(uImage, vTexCoords - vec2(tex_offset.x * i, 0.0)).rgb * weight[i];
                }
            } else {
                for (int i = 1; i < 5; ++i) {
                    result += texture(uImage, vTexCoords + vec2(0.0, tex_offset.y * i)).rgb * weight[i];
                    result += texture(uImage, vTexCoords - vec2(0.0, tex_offset.y * i)).rgb * weight[i];
                }
            }
            FragColor = vec4(result, 1.0);
        }
    )";

    // Composite Shader: Scene + Bloom + Cryo Frost Vignette + ACES Tonemapping + Gamma
    static const char* s_compositeFragmentShader = R"(
        #version 450 core
        out vec4 FragColor;
        in vec2 vTexCoords;

        uniform sampler2D uScene;
        uniform sampler2D uBloom;
        uniform float uBloomIntensity;
        uniform float uExposure;
        uniform float uFrostIntensity;
        uniform float uTime;
        uniform int uTonemapper; // 0 = ACES, 1 = Reinhard

        // Procedural fractal ice crystallization
        float hash(vec2 p) {
            return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453123);
        }

        float frostNoise(vec2 p) {
            vec2 i = floor(p);
            vec2 f = fract(p);
            f = f * f * (3.0 - 2.0 * f);
            return mix(mix(hash(i + vec2(0.0, 0.0)), hash(i + vec2(1.0, 0.0)), f.x),
                       mix(hash(i + vec2(0.0, 1.0)), hash(i + vec2(1.0, 1.0)), f.x), f.y);
        }

        float fbmFrost(vec2 p) {
            float v = 0.0;
            v += 0.5000 * frostNoise(p); p *= 2.02;
            v += 0.2500 * frostNoise(p); p *= 2.03;
            v += 0.1250 * frostNoise(p);
            return v;
        }

        vec3 acesFilmic(vec3 x) {
            const float a = 2.51;
            const float b = 0.03;
            const float c = 2.43;
            const float d = 0.59;
            const float e = 0.14;
            return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
        }

        vec3 reinhard(vec3 x) {
            return x / (x + vec3(1.0));
        }

        void main() {
            vec3 hdrColor = texture(uScene, vTexCoords).rgb;
            vec3 bloomColor = texture(uBloom, vTexCoords).rgb;

            // Additive Screen-Space Bloom
            hdrColor += bloomColor * uBloomIntensity;

            // Cryo Frost HUD Vignette
            if (uFrostIntensity > 0.001) {
                vec2 uv = vTexCoords;
                vec2 centered = abs(uv - 0.5) * 2.0;
                float dist = max(centered.x, centered.y); // Rectangular visor border
                
                // Ice crystal pattern creeping from edges
                float n = fbmFrost(uv * vec2(14.0, 8.0) + vec2(sin(uTime * 0.1) * 0.2, cos(uTime * 0.08) * 0.2));
                float threshold = 1.05 - uFrostIntensity * 0.55;
                float frostMask = smoothstep(threshold, 1.02, dist + n * 0.18);
                
                // Cyan-tinted frosted ice crystals
                vec3 frostTint = vec3(0.72, 0.90, 1.0) * (0.85 + n * 0.45);
                hdrColor = mix(hdrColor, frostTint * 1.8, frostMask * clamp(uFrostIntensity * 1.25, 0.0, 1.0));
            }

            // Exposure tone-mapping
            vec3 color = hdrColor * uExposure;
            vec3 mapped;
            if (uTonemapper == 0) {
                mapped = acesFilmic(color);
            } else {
                mapped = reinhard(color);
            }

            // Gamma 2.2 correction
            mapped = pow(mapped, vec3(1.0 / 2.2));

            FragColor = vec4(mapped, 1.0);
        }
    )";

    // --- C++ Implementation ---

    PostProcessPipeline::PostProcessPipeline()
        : _width(1280), _height(720), _initialized(false) {
    }

    PostProcessPipeline::~PostProcessPipeline() {
        shutdown();
    }

    PostProcessPipeline::PostProcessPipeline(PostProcessPipeline&& other) noexcept
        : _width(other._width), _height(other._height), _initialized(other._initialized),
          _hdrFbo(other._hdrFbo), _hdrColorTexture(other._hdrColorTexture),
          _brightColorTexture(other._brightColorTexture), _depthStencilRbo(other._depthStencilRbo),
          _bloomWidth(other._bloomWidth), _bloomHeight(other._bloomHeight),
          _quadVao(other._quadVao), _quadVbo(other._quadVbo),
          _blurShader(std::move(other._blurShader)),
          _compositeShader(std::move(other._compositeShader)) {
        
        _pingPongFbo[0] = other._pingPongFbo[0];
        _pingPongFbo[1] = other._pingPongFbo[1];
        _pingPongColor[0] = other._pingPongColor[0];
        _pingPongColor[1] = other._pingPongColor[1];

        other._hdrFbo = 0;
        other._hdrColorTexture = 0;
        other._brightColorTexture = 0;
        other._depthStencilRbo = 0;
        other._pingPongFbo[0] = other._pingPongFbo[1] = 0;
        other._pingPongColor[0] = other._pingPongColor[1] = 0;
        other._quadVao = other._quadVbo = 0;
        other._initialized = false;
    }

    PostProcessPipeline& PostProcessPipeline::operator=(PostProcessPipeline&& other) noexcept {
        if (this != &other) {
            shutdown();

            _width = other._width;
            _height = other._height;
            _initialized = other._initialized;
            _hdrFbo = other._hdrFbo;
            _hdrColorTexture = other._hdrColorTexture;
            _brightColorTexture = other._brightColorTexture;
            _depthStencilRbo = other._depthStencilRbo;
            _bloomWidth = other._bloomWidth;
            _bloomHeight = other._bloomHeight;
            _pingPongFbo[0] = other._pingPongFbo[0];
            _pingPongFbo[1] = other._pingPongFbo[1];
            _pingPongColor[0] = other._pingPongColor[0];
            _pingPongColor[1] = other._pingPongColor[1];
            _quadVao = other._quadVao;
            _quadVbo = other._quadVbo;
            _blurShader = std::move(other._blurShader);
            _compositeShader = std::move(other._compositeShader);

            other._hdrFbo = 0;
            other._hdrColorTexture = 0;
            other._brightColorTexture = 0;
            other._depthStencilRbo = 0;
            other._pingPongFbo[0] = other._pingPongFbo[1] = 0;
            other._pingPongColor[0] = other._pingPongColor[1] = 0;
            other._quadVao = other._quadVbo = 0;
            other._initialized = false;
        }
        return *this;
    }

    bool PostProcessPipeline::init(int width, int height) {
        shutdown();
        _width = std::max(1, width);
        _height = std::max(1, height);
        _bloomWidth = std::max(1, _width / 2);
        _bloomHeight = std::max(1, _height / 2);

        setupQuad();
        initShaders();
        initFramebuffers();

        _initialized = true;
        return true;
    }

    void PostProcessPipeline::shutdown() {
        destroyFramebuffers();

        if (_quadVao) {
            glDeleteVertexArrays(1, &_quadVao);
            _quadVao = 0;
        }
        if (_quadVbo) {
            glDeleteBuffers(1, &_quadVbo);
            _quadVbo = 0;
        }

        _blurShader.reset();
        _compositeShader.reset();
        _initialized = false;
    }

    void PostProcessPipeline::resize(int width, int height) {
        if (width <= 0 || height <= 0) return;
        if (width == _width && height == _height) return;

        _width = width;
        _height = height;
        _bloomWidth = std::max(1, _width / 2);
        _bloomHeight = std::max(1, _height / 2);

        destroyFramebuffers();
        initFramebuffers();
    }

    void PostProcessPipeline::setupQuad() {
        if (_quadVao != 0) return;

        float quadVertices[] = {
            // positions   // texCoords
            -1.0f,  1.0f,  0.0f, 1.0f,
            -1.0f, -1.0f,  0.0f, 0.0f,
             1.0f, -1.0f,  1.0f, 0.0f,

            -1.0f,  1.0f,  0.0f, 1.0f,
             1.0f, -1.0f,  1.0f, 0.0f,
             1.0f,  1.0f,  1.0f, 1.0f
        };

        glCreateVertexArrays(1, &_quadVao);
        glCreateBuffers(1, &_quadVbo);

        glNamedBufferData(_quadVbo, sizeof(quadVertices), quadVertices, GL_STATIC_DRAW);

        glEnableVertexArrayAttrib(_quadVao, 0);
        glVertexArrayAttribFormat(_quadVao, 0, 2, GL_FLOAT, GL_FALSE, 0);
        glVertexArrayAttribBinding(_quadVao, 0, 0);

        glEnableVertexArrayAttrib(_quadVao, 1);
        glVertexArrayAttribFormat(_quadVao, 1, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float));
        glVertexArrayAttribBinding(_quadVao, 1, 0);

        glVertexArrayVertexBuffer(_quadVao, 0, _quadVbo, 0, 4 * sizeof(float));
    }

    void PostProcessPipeline::initShaders() {
        _blurShader = std::make_unique<Shader>(s_quadVertexShader, s_blurFragmentShader);
        _compositeShader = std::make_unique<Shader>(s_quadVertexShader, s_compositeFragmentShader);
    }

    void PostProcessPipeline::initFramebuffers() {
        // 1. Primary HDR Framebuffer (GL_RGBA16F)
        glCreateFramebuffers(1, &_hdrFbo);

        glCreateTextures(GL_TEXTURE_2D, 1, &_hdrColorTexture);
        glTextureStorage2D(_hdrColorTexture, 1, GL_RGBA16F, _width, _height);
        glTextureParameteri(_hdrColorTexture, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTextureParameteri(_hdrColorTexture, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTextureParameteri(_hdrColorTexture, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTextureParameteri(_hdrColorTexture, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glNamedFramebufferTexture(_hdrFbo, GL_COLOR_ATTACHMENT0, _hdrColorTexture, 0);

        // Depth/Stencil Renderbuffer
        glCreateRenderbuffers(1, &_depthStencilRbo);
        glNamedRenderbufferStorage(_depthStencilRbo, GL_DEPTH24_STENCIL8, _width, _height);
        glNamedFramebufferRenderbuffer(_hdrFbo, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, _depthStencilRbo);

        GLenum drawBufs[1] = { GL_COLOR_ATTACHMENT0 };
        glNamedFramebufferDrawBuffers(_hdrFbo, 1, drawBufs);

        GLenum status = glCheckNamedFramebufferStatus(_hdrFbo, GL_FRAMEBUFFER);
        if (status != GL_FRAMEBUFFER_COMPLETE) {
            std::cerr << "[PostProcess] Error: HDR FBO incomplete (0x" << std::hex << status << std::dec << ")\n";
        }

        // 2. Downsampled Bloom Ping-Pong Framebuffers (GL_RGBA16F at Half Resolution)
        for (int i = 0; i < 2; ++i) {
            glCreateFramebuffers(1, &_pingPongFbo[i]);

            glCreateTextures(GL_TEXTURE_2D, 1, &_pingPongColor[i]);
            glTextureStorage2D(_pingPongColor[i], 1, GL_RGBA16F, _bloomWidth, _bloomHeight);
            glTextureParameteri(_pingPongColor[i], GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTextureParameteri(_pingPongColor[i], GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTextureParameteri(_pingPongColor[i], GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTextureParameteri(_pingPongColor[i], GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

            glNamedFramebufferTexture(_pingPongFbo[i], GL_COLOR_ATTACHMENT0, _pingPongColor[i], 0);

            GLenum pStatus = glCheckNamedFramebufferStatus(_pingPongFbo[i], GL_FRAMEBUFFER);
            if (pStatus != GL_FRAMEBUFFER_COMPLETE) {
                std::cerr << "[PostProcess] Error: Bloom PingPong FBO [" << i << "] incomplete\n";
            }
        }
    }

    void PostProcessPipeline::destroyFramebuffers() {
        if (_hdrFbo) {
            glDeleteFramebuffers(1, &_hdrFbo);
            _hdrFbo = 0;
        }
        if (_hdrColorTexture) {
            glDeleteTextures(1, &_hdrColorTexture);
            _hdrColorTexture = 0;
        }
        if (_brightColorTexture) {
            glDeleteTextures(1, &_brightColorTexture);
            _brightColorTexture = 0;
        }
        if (_depthStencilRbo) {
            glDeleteRenderbuffers(1, &_depthStencilRbo);
            _depthStencilRbo = 0;
        }

        for (int i = 0; i < 2; ++i) {
            if (_pingPongFbo[i]) {
                glDeleteFramebuffers(1, &_pingPongFbo[i]);
                _pingPongFbo[i] = 0;
            }
            if (_pingPongColor[i]) {
                glDeleteTextures(1, &_pingPongColor[i]);
                _pingPongColor[i] = 0;
            }
        }
    }

    void PostProcessPipeline::beginScene() {
        if (!_initialized) return;
        glBindFramebuffer(GL_FRAMEBUFFER, _hdrFbo);
        glViewport(0, 0, _width, _height);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
    }

    void PostProcessPipeline::endScene() {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    void PostProcessPipeline::render(float exposure, float frostIntensity, float timeSeconds, TonemapperType tonemapper) {
        if (!_initialized) return;

        // 1. Initial downsample and bright extraction pass into PingPong[0]
        glBindFramebuffer(GL_FRAMEBUFFER, _pingPongFbo[0]);
        glViewport(0, 0, _bloomWidth, _bloomHeight);
        glClear(GL_COLOR_BUFFER_BIT);

        // We can use the blur shader or an initial blit with bright extraction
        static std::unique_ptr<Shader> s_extractShader;
        if (!s_extractShader) {
            s_extractShader = std::make_unique<Shader>(s_quadVertexShader, s_extractBrightFragmentShader);
        }

        s_extractShader->use();
        s_extractShader->setInt("uHDRScene", 0);
        s_extractShader->setFloat("uThreshold", 0.95f);
        glBindTextureUnit(0, _hdrColorTexture);

        glBindVertexArray(_quadVao);
        glDrawArrays(GL_TRIANGLES, 0, 6);

        // 2. Separable Multi-Pass Gaussian Blur (5 iterations = 10 passes)
        _blurShader->use();
        _blurShader->setInt("uImage", 0);

        bool horizontal = true;
        bool firstIteration = true;
        int blurAmount = 6;

        for (int i = 0; i < blurAmount; ++i) {
            glBindFramebuffer(GL_FRAMEBUFFER, _pingPongFbo[horizontal ? 1 : 0]);
            _blurShader->setInt("uHorizontal", horizontal ? 1 : 0);

            unsigned int inputTex = firstIteration ? _pingPongColor[0] : _pingPongColor[horizontal ? 0 : 1];
            glBindTextureUnit(0, inputTex);

            glDrawArrays(GL_TRIANGLES, 0, 6);

            horizontal = !horizontal;
            if (firstIteration) firstIteration = false;
        }

        // 3. Composite Pass back to default backbuffer (screen)
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, _width, _height);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        _compositeShader->use();
        _compositeShader->setInt("uScene", 0);
        _compositeShader->setInt("uBloom", 1);
        _compositeShader->setFloat("uBloomIntensity", 0.45f);
        _compositeShader->setFloat("uExposure", exposure);
        _compositeShader->setFloat("uFrostIntensity", frostIntensity);
        _compositeShader->setFloat("uTime", timeSeconds);
        _compositeShader->setInt("uTonemapper", (int)tonemapper);

        glBindTextureUnit(0, _hdrColorTexture);
        glBindTextureUnit(1, _pingPongColor[0]); // blurred bloom result

        glBindVertexArray(_quadVao);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        glBindVertexArray(0);
    }

    Vec3 PostProcessPipeline::acesFilmicTonemap(const Vec3& color) {
        auto aces = [](float x) -> float {
            const float a = 2.51f;
            const float b = 0.03f;
            const float c = 2.43f;
            const float d = 0.59f;
            const float e = 0.14f;
            float val = (x * (a * x + b)) / (x * (c * x + d) + e);
            return std::clamp(val, 0.0f, 1.0f);
        };
        return Vec3(aces(color.x), aces(color.y), aces(color.z));
    }

    Vec3 PostProcessPipeline::reinhardTonemap(const Vec3& color) {
        return Vec3(
            color.x / (color.x + 1.0f),
            color.y / (color.y + 1.0f),
            color.z / (color.z + 1.0f)
        );
    }

    float PostProcessPipeline::calculateFrostVignette(float health, float maxHealth, bool isCryoSector) {
        float ratio = (maxHealth > 0.0f) ? std::clamp(health / maxHealth, 0.0f, 1.0f) : 0.0f;
        float healthFrost = std::max(0.0f, (1.0f - ratio) * 0.90f);
        float sectorChill = isCryoSector ? 0.22f : 0.0f;
        return std::clamp(healthFrost + sectorChill, 0.0f, 1.0f);
    }

} // namespace Lab
