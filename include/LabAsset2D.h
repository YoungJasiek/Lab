#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include "LabMath.h"
#include "LabExport.h"

namespace Lab {

    class Texture;
    class SpriteBatcher;

    struct SpriteFrame {
        std::string name;
        Vec2 uvMin{0.0f, 0.0f};
        Vec2 uvMax{1.0f, 1.0f};
        Vec2 size{32.0f, 32.0f};      // Size in pixels
        Vec2 pivot{0.5f, 0.5f};     // Normalised pivot (0.5, 0.5 is center)
        float duration{0.1f};       // Frame duration for animations in seconds
    };

    class SpriteSheet2D {
    public:
        SpriteSheet2D() = default;
        explicit SpriteSheet2D(const std::string& texturePath, int texW = 0, int texH = 0);

        void setTexturePath(const std::string& path) { _texturePath = path; }
        const std::string& getTexturePath() const { return _texturePath; }

        void setTextureSize(int width, int height);
        int getTextureWidth() const { return _textureWidth; }
        int getTextureHeight() const { return _textureHeight; }

        // Grid slicer: creates frames based on rows and columns or cell dimensions
        void sliceGrid(int cols, int rows, int cellW = 0, int cellH = 0, int spacing = 0, int margin = 0);

        // Custom manual frame definition (in pixels)
        void addFrame(const std::string& name, float pxX, float pxY, float pxW, float pxH, const Vec2& pivot = {0.5f, 0.5f});

        size_t getFrameCount() const { return _frames.size(); }
        const SpriteFrame* getFrame(size_t index) const;
        const SpriteFrame* getFrame(const std::string& name) const;
        const std::vector<SpriteFrame>& getAllFrames() const { return _frames; }
        void clearFrames();

        // JSON serialization
        bool saveToFile(const std::string& filePath) const;
        bool loadFromFile(const std::string& filePath);

    private:
        std::string _texturePath;
        int _textureWidth{0};
        int _textureHeight{0};
        std::vector<SpriteFrame> _frames;
        std::unordered_map<std::string, size_t> _nameLookup;
    };

    struct Animation2D {
        std::string name;
        std::vector<size_t> frameIndices;
        float fps{12.0f};
        bool loop{true};
    };

    class AnimationPlayer2D {
    public:
        AnimationPlayer2D() = default;

        void addAnimation(const Animation2D& anim);
        void play(const std::string& animName, bool restart = false);
        void stop();
        void update(float dt);

        const SpriteFrame* getCurrentFrame(const SpriteSheet2D& sheet) const;
        size_t getCurrentFrameIndex() const;
        std::string getCurrentAnimationName() const { return _currentAnim; }
        bool isPlaying() const { return _isPlaying; }
        bool isFinished() const { return _isFinished; }
        float getProgress() const; // 0.0 to 1.0

        const std::unordered_map<std::string, Animation2D>& getAnimations() const { return _animations; }

    private:
        std::unordered_map<std::string, Animation2D> _animations;
        std::string _currentAnim;
        float _timer{0.0f};
        size_t _frameStep{0};
        bool _isPlaying{false};
        bool _isFinished{false};
    };

    struct TilemapLayer2D {
        std::string name{"Default"};
        int width{0};
        int height{0};
        std::vector<int> tiles; // Index into SpriteSheet2D, or -1 for empty
        float opacity{1.0f};
        bool visible{true};
        int zOrder{0};
    };

    class Tilemap2D {
    public:
        Tilemap2D();
        Tilemap2D(int width, int height, int tileWidth = 32, int tileHeight = 32);

        void setDimensions(int w, int h);
        int getWidth() const { return _width; }
        int getHeight() const { return _height; }

        void setTileSize(int tw, int th) { _tileWidth = tw; _tileHeight = th; }
        int getTileWidth() const { return _tileWidth; }
        int getTileHeight() const { return _tileHeight; }

        void setSpriteSheetPath(const std::string& path) { _spriteSheetPath = path; }
        const std::string& getSpriteSheetPath() const { return _spriteSheetPath; }

        size_t addLayer(const std::string& name);
        void removeLayer(size_t index);
        size_t getLayerCount() const { return _layers.size(); }
        TilemapLayer2D* getLayer(size_t index);
        const TilemapLayer2D* getLayer(size_t index) const;

        void setTile(size_t layerIdx, int x, int y, int tileId);
        int getTile(size_t layerIdx, int x, int y) const;

        void render(SpriteBatcher& batcher, const Texture* texture, const Vec2& worldOffset = {0.0f, 0.0f}) const;

        bool saveToFile(const std::string& filePath) const;
        bool loadFromFile(const std::string& filePath);

    private:
        int _width{16};
        int _height{16};
        int _tileWidth{32};
        int _tileHeight{32};
        std::string _spriteSheetPath;
        std::vector<TilemapLayer2D> _layers;
    };

} // namespace Lab
