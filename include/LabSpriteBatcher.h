#pragma once
#include <vector>
#include <cstdint>
#include <memory>
#include "LabMath.h"

namespace Lab {

    class Texture;
    class Shader;

    struct SpriteVertex {
        Vec3 position;    // x, y, z (z corresponds to sorting layer depth)
        Vec2 texCoords;   // u, v
        Vec4 color;       // r, g, b, a
        float textureSlot;// slot index or -1 for untextured
    };

    struct SpriteItem {
        const Texture* texture{nullptr};
        Vec2 position{0.0f, 0.0f};
        Vec2 size{1.0f, 1.0f};
        Vec2 uvMin{0.0f, 0.0f};
        Vec2 uvMax{1.0f, 1.0f};
        Vec4 color{1.0f, 1.0f, 1.0f, 1.0f};
        float rotation{0.0f};
        Vec2 origin{0.5f, 0.5f};
        int layer{0};
        uint32_t sequenceId{0};
    };

    struct SpriteBatchStats {
        size_t spriteCount{0};
        size_t drawCallCount{0};
        size_t vertexCount{0};
        size_t indexCount{0};
    };

    class SpriteBatcher {
    public:
        static constexpr size_t MAX_SPRITES_PER_BATCH = 10000;
        static constexpr size_t MAX_VERTICES = MAX_SPRITES_PER_BATCH * 4;
        static constexpr size_t MAX_INDICES = MAX_SPRITES_PER_BATCH * 6;
        static constexpr size_t MAX_TEXTURE_SLOTS = 16;

        SpriteBatcher();
        ~SpriteBatcher();

        SpriteBatcher(const SpriteBatcher&) = delete;
        SpriteBatcher& operator=(const SpriteBatcher&) = delete;
        SpriteBatcher(SpriteBatcher&&) noexcept = default;
        SpriteBatcher& operator=(SpriteBatcher&&) noexcept = default;

        void init();
        void shutdown();

        // Begin sprite batch with a view-projection matrix (typically ortho)
        void begin(const Mat4& viewProjection);

        // Draw textured quad
        void draw(const Texture* texture, const Vec2& position, const Vec2& size,
                  const Vec4& color = { 1.0f, 1.0f, 1.0f, 1.0f },
                  float rotationRad = 0.0f, const Vec2& origin = { 0.5f, 0.5f },
                  int layer = 0);

        // Draw sub-rectangle (sprite atlas / tilemap UVs)
        void draw(const Texture* texture, const Vec2& position, const Vec2& size,
                  const Vec2& uvMin, const Vec2& uvMax,
                  const Vec4& color = { 1.0f, 1.0f, 1.0f, 1.0f },
                  float rotationRad = 0.0f, const Vec2& origin = { 0.5f, 0.5f },
                  int layer = 0);

        // Draw solid colored quad without texture
        void drawRect(const Vec2& position, const Vec2& size,
                      const Vec4& color = { 1.0f, 1.0f, 1.0f, 1.0f },
                      float rotationRad = 0.0f, const Vec2& origin = { 0.5f, 0.5f },
                      int layer = 0);

        // Flushes current queue of sprites to GPU
        void flush();

        // End current batch and flush remaining sprites
        void end();

        // Statistics and inspection
        const SpriteBatchStats& getStats() const { return _stats; }
        void resetStats() { _stats = SpriteBatchStats{}; }

        const std::vector<SpriteVertex>& getBatchedVertices() const { return _vertices; }
        bool isInitialized() const { return _initialized; }

    private:
        void renderBatch();

        bool _initialized{false};
        bool _inBegin{false};
        Mat4 _viewProjection{Mat4::identity()};

        uint32_t _nextSequence{0};
        std::vector<SpriteItem> _queue;
        std::vector<SpriteVertex> _vertices;

        unsigned int _vao{0};
        unsigned int _vbo{0};
        unsigned int _ebo{0};
        std::unique_ptr<Shader> _shader;

        SpriteBatchStats _stats;
    };

} // namespace Lab
