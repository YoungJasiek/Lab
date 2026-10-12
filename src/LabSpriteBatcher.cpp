#include "LabSpriteBatcher.h"
#include "LabRenderer.h"
#include <glad/gl.h>
#include <algorithm>
#include <cmath>

namespace Lab {

    static const char* spriteVertexShaderSrc = R"(
        #version 450 core
        layout (location = 0) in vec3 aPos;
        layout (location = 1) in vec2 aTexCoord;
        layout (location = 2) in vec4 aColor;
        layout (location = 3) in float aTexSlot;

        out vec2 TexCoord;
        out vec4 Color;
        flat out int TexSlot;

        uniform mat4 uViewProj;

        void main() {
            TexCoord = aTexCoord;
            Color = aColor;
            TexSlot = int(aTexSlot);
            gl_Position = uViewProj * vec4(aPos, 1.0);
        }
    )";

    static const char* spriteFragmentShaderSrc = R"(
        #version 450 core
        in vec2 TexCoord;
        in vec4 Color;
        flat in int TexSlot;

        out vec4 FragColor;

        uniform sampler2D uTexture;
        uniform int uHasTexture;

        void main() {
            if (uHasTexture == 1) {
                FragColor = texture(uTexture, TexCoord) * Color;
            } else {
                FragColor = Color;
            }
        }
    )";

    SpriteBatcher::SpriteBatcher() {
        _queue.reserve(MAX_SPRITES_PER_BATCH);
        _vertices.reserve(MAX_VERTICES);
    }

    SpriteBatcher::~SpriteBatcher() {
        shutdown();
    }

    void SpriteBatcher::init() {
        if (_initialized) return;

        // Only create GL buffers if GLAD is initialized and glGenVertexArrays exists
        if (glad_glGenVertexArrays != nullptr) {
            glGenVertexArrays(1, &_vao);
            glBindVertexArray(_vao);

            glGenBuffers(1, &_vbo);
            glBindBuffer(GL_ARRAY_BUFFER, _vbo);
            glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(MAX_VERTICES * sizeof(SpriteVertex)), nullptr, GL_DYNAMIC_DRAW);

            // Setup interleaved vertex attributes
            glEnableVertexAttribArray(0);
            glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(SpriteVertex), reinterpret_cast<const void*>(offsetof(SpriteVertex, position)));

            glEnableVertexAttribArray(1);
            glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(SpriteVertex), reinterpret_cast<const void*>(offsetof(SpriteVertex, texCoords)));

            glEnableVertexAttribArray(2);
            glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(SpriteVertex), reinterpret_cast<const void*>(offsetof(SpriteVertex, color)));

            glEnableVertexAttribArray(3);
            glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, sizeof(SpriteVertex), reinterpret_cast<const void*>(offsetof(SpriteVertex, textureSlot)));

            // Pre-generate element indices: 6 indices per quad (0, 1, 2, 2, 3, 0)
            std::vector<unsigned int> indices(MAX_INDICES);
            unsigned int offset = 0;
            for (size_t i = 0; i < MAX_INDICES; i += 6) {
                indices[i + 0] = offset + 0;
                indices[i + 1] = offset + 1;
                indices[i + 2] = offset + 2;
                indices[i + 3] = offset + 2;
                indices[i + 4] = offset + 3;
                indices[i + 5] = offset + 0;
                offset += 4;
            }

            glGenBuffers(1, &_ebo);
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, _ebo);
            glBufferData(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLsizeiptr>(indices.size() * sizeof(unsigned int)), indices.data(), GL_STATIC_DRAW);

            glBindVertexArray(0);

            _shader = std::make_unique<Shader>(spriteVertexShaderSrc, spriteFragmentShaderSrc);
        }

        _initialized = true;
    }

    void SpriteBatcher::shutdown() {
        if (!_initialized) return;

        if (glad_glDeleteVertexArrays != nullptr && _vao != 0) {
            glDeleteVertexArrays(1, &_vao);
            glDeleteBuffers(1, &_vbo);
            glDeleteBuffers(1, &_ebo);
            _vao = _vbo = _ebo = 0;
        }

        _shader.reset();

        _queue.clear();
        _vertices.clear();
        _initialized = false;
    }

    void SpriteBatcher::begin(const Mat4& viewProjection) {
        _viewProjection = viewProjection;
        _inBegin = true;
        _queue.clear();
        _vertices.clear();
        _nextSequence = 0;
    }

    void SpriteBatcher::draw(const Texture* texture, const Vec2& position, const Vec2& size,
                             const Vec4& color, float rotationRad, const Vec2& origin, int layer) {
        draw(texture, position, size, Vec2(0.0f, 0.0f), Vec2(1.0f, 1.0f), color, rotationRad, origin, layer);
    }

    void SpriteBatcher::draw(const Texture* texture, const Vec2& position, const Vec2& size,
                             const Vec2& uvMin, const Vec2& uvMax,
                             const Vec4& color, float rotationRad, const Vec2& origin, int layer) {
        SpriteItem item;
        item.texture = texture;
        item.position = position;
        item.size = size;
        item.uvMin = uvMin;
        item.uvMax = uvMax;
        item.color = color;
        item.rotation = rotationRad;
        item.origin = origin;
        item.layer = layer;
        item.sequenceId = _nextSequence++;
        _queue.push_back(item);

        if (_queue.size() >= MAX_SPRITES_PER_BATCH) {
            flush();
        }
    }

    void SpriteBatcher::drawRect(const Vec2& position, const Vec2& size,
                                 const Vec4& color, float rotationRad, const Vec2& origin, int layer) {
        draw(nullptr, position, size, Vec2(0.0f, 0.0f), Vec2(1.0f, 1.0f), color, rotationRad, origin, layer);
    }

    void SpriteBatcher::flush() {
        if (_queue.empty()) return;

        // Stable sort queue by layer first, then insertion sequence
        std::stable_sort(_queue.begin(), _queue.end(), [](const SpriteItem& a, const SpriteItem& b) {
            if (a.layer != b.layer) return a.layer < b.layer;
            return a.sequenceId < b.sequenceId;
        });

        const Texture* currentTexture = nullptr;
        bool hasTextureBatch = false;

        for (size_t i = 0; i < _queue.size(); ++i) {
            const auto& item = _queue[i];

            // If texture changed and we already have buffered vertices, dispatch current batch
            if (i > 0 && item.texture != currentTexture) {
                renderBatch();
            }

            currentTexture = item.texture;
            hasTextureBatch = (currentTexture != nullptr);

            float cosA = std::cos(item.rotation);
            float sinA = std::sin(item.rotation);

            // Local coordinates relative to origin
            float left = -item.origin.x * item.size.x;
            float right = (1.0f - item.origin.x) * item.size.x;
            float bottom = -item.origin.y * item.size.y;
            float top = (1.0f - item.origin.y) * item.size.y;

            auto transformPoint = [&](float lx, float ly) -> Vec3 {
                float rx = lx * cosA - ly * sinA;
                float ry = lx * sinA + ly * cosA;
                return Vec3(item.position.x + rx, item.position.y + ry, static_cast<float>(item.layer) * 0.001f);
            };

            float slot = hasTextureBatch ? 0.0f : -1.0f;

            // Quad vertex 0: bottom-left
            _vertices.push_back({ transformPoint(left, bottom), Vec2(item.uvMin.x, item.uvMin.y), item.color, slot });
            // Quad vertex 1: bottom-right
            _vertices.push_back({ transformPoint(right, bottom), Vec2(item.uvMax.x, item.uvMin.y), item.color, slot });
            // Quad vertex 2: top-right
            _vertices.push_back({ transformPoint(right, top), Vec2(item.uvMax.x, item.uvMax.y), item.color, slot });
            // Quad vertex 3: top-left
            _vertices.push_back({ transformPoint(left, top), Vec2(item.uvMin.x, item.uvMax.y), item.color, slot });

            _stats.spriteCount++;
        }

        renderBatch();
        _queue.clear();
    }

    void SpriteBatcher::renderBatch() {
        if (_vertices.empty()) return;

        size_t quadCount = _vertices.size() / 4;
        size_t indexCount = quadCount * 6;

        _stats.drawCallCount++;
        _stats.vertexCount += _vertices.size();
        _stats.indexCount += indexCount;

        // Render to OpenGL if GPU pipeline is ready
        if (glad_glDrawElements != nullptr && _vao != 0 && _shader != nullptr) {
            _shader->use();
            _shader->setMat4("uViewProj", _viewProjection);

            bool hasTexture = (_vertices[0].textureSlot >= 0.0f);
            _shader->setInt("uHasTexture", hasTexture ? 1 : 0);

            // Upload vertex data to GPU
            glBindVertexArray(_vao);
            glBindBuffer(GL_ARRAY_BUFFER, _vbo);
            glBufferSubData(GL_ARRAY_BUFFER, 0, static_cast<GLsizeiptr>(_vertices.size() * sizeof(SpriteVertex)), _vertices.data());

            glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(indexCount), GL_UNSIGNED_INT, nullptr);
            glBindVertexArray(0);
        }

        _vertices.clear();
    }

    void SpriteBatcher::end() {
        if (_inBegin) {
            flush();
            _inBegin = false;
        }
    }

} // namespace Lab
