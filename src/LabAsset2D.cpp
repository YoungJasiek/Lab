#include "LabAsset2D.h"
#include "LabSpriteBatcher.h"
#include "LabCore.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>
#include <charconv>

namespace Lab {

    // ==========================================================
    // SpriteSheet2D Implementation
    // ==========================================================

    SpriteSheet2D::SpriteSheet2D(const std::string& texturePath, int texW, int texH)
        : _texturePath(texturePath), _textureWidth(texW), _textureHeight(texH) {}

    void SpriteSheet2D::setTextureSize(int width, int height) {
        _textureWidth = width;
        _textureHeight = height;
    }

    void SpriteSheet2D::sliceGrid(int cols, int rows, int cellW, int cellH, int spacing, int margin) {
        if (cols <= 0 || rows <= 0) return;

        if (cellW <= 0 && _textureWidth > 0) {
            cellW = (_textureWidth - 2 * margin - (cols - 1) * spacing) / cols;
        }
        if (cellH <= 0 && _textureHeight > 0) {
            cellH = (_textureHeight - 2 * margin - (rows - 1) * spacing) / rows;
        }

        if (cellW <= 0) cellW = 32;
        if (cellH <= 0) cellH = 32;

        float texW = (_textureWidth > 0) ? static_cast<float>(_textureWidth) : static_cast<float>(cols * cellW);
        float texH = (_textureHeight > 0) ? static_cast<float>(_textureHeight) : static_cast<float>(rows * cellH);

        clearFrames();

        for (int r = 0; r < rows; ++r) {
            for (int c = 0; c < cols; ++c) {
                float pxX = static_cast<float>(margin + c * (cellW + spacing));
                float pxY = static_cast<float>(margin + r * (cellH + spacing));
                float pxW = static_cast<float>(cellW);
                float pxH = static_cast<float>(cellH);

                SpriteFrame frame;
                frame.name = "frame_" + std::to_string(r * cols + c);
                frame.size = { pxW, pxH };
                frame.pivot = { 0.5f, 0.5f };
                frame.duration = 0.1f;
                frame.uvMin = { pxX / texW, pxY / texH };
                frame.uvMax = { (pxX + pxW) / texW, (pxY + pxH) / texH };

                _nameLookup[frame.name] = _frames.size();
                _frames.push_back(frame);
            }
        }
    }

    void SpriteSheet2D::addFrame(const std::string& name, float pxX, float pxY, float pxW, float pxH, const Vec2& pivot) {
        float texW = (_textureWidth > 0) ? static_cast<float>(_textureWidth) : (pxX + pxW);
        float texH = (_textureHeight > 0) ? static_cast<float>(_textureHeight) : (pxY + pxH);

        SpriteFrame frame;
        frame.name = name;
        frame.size = { pxW, pxH };
        frame.pivot = pivot;
        frame.duration = 0.1f;
        frame.uvMin = { pxX / texW, pxY / texH };
        frame.uvMax = { (pxX + pxW) / texW, (pxY + pxH) / texH };

        _nameLookup[frame.name] = _frames.size();
        _frames.push_back(frame);
    }

    const SpriteFrame* SpriteSheet2D::getFrame(size_t index) const {
        if (index < _frames.size()) {
            return &_frames[index];
        }
        return nullptr;
    }

    const SpriteFrame* SpriteSheet2D::getFrame(const std::string& name) const {
        auto it = _nameLookup.find(name);
        if (it != _nameLookup.end()) {
            return &_frames[it->second];
        }
        return nullptr;
    }

    void SpriteSheet2D::clearFrames() {
        _frames.clear();
        _nameLookup.clear();
    }

    bool SpriteSheet2D::saveToFile(const std::string& filePath) const {
        std::ofstream out(filePath);
        if (!out.is_open()) return false;

        out << "{\n";
        out << "  \"texturePath\": \"" << _texturePath << "\",\n";
        out << "  \"width\": " << _textureWidth << ",\n";
        out << "  \"height\": " << _textureHeight << ",\n";
        out << "  \"frames\": [\n";

        for (size_t i = 0; i < _frames.size(); ++i) {
            const auto& f = _frames[i];
            out << "    {\n";
            out << "      \"name\": \"" << f.name << "\",\n";
            out << "      \"uvMin\": [" << f.uvMin.x << ", " << f.uvMin.y << "],\n";
            out << "      \"uvMax\": [" << f.uvMax.x << ", " << f.uvMax.y << "],\n";
            out << "      \"size\": [" << f.size.x << ", " << f.size.y << "],\n";
            out << "      \"pivot\": [" << f.pivot.x << ", " << f.pivot.y << "],\n";
            out << "      \"duration\": " << f.duration << "\n";
            out << "    }" << (i + 1 < _frames.size() ? "," : "") << "\n";
        }

        out << "  ]\n";
        out << "}\n";
        return true;
    }

    bool SpriteSheet2D::loadFromFile(const std::string& filePath) {
        std::ifstream in(filePath);
        if (!in.is_open()) return false;

        clearFrames();
        std::string line;
        SpriteFrame currentFrame;
        bool inFrame = false;

        auto parseJsonVec2 = [](const std::string& str, Vec2& outVec) -> bool {
            size_t openBracket = str.find('[');
            if (openBracket == std::string::npos) return false;
            const char* p = str.data() + openBracket + 1;
            const char* end = str.data() + str.size();
            while (p < end && (*p == ' ' || *p == '\t')) ++p;
            float x = 0.0f;
            auto [p1, ec1] = std::from_chars(p, end, x);
            if (ec1 != std::errc{}) return false;
            p = p1;
            while (p < end && (*p == ' ' || *p == '\t' || *p == ',')) ++p;
            float y = 0.0f;
            auto [p2, ec2] = std::from_chars(p, end, y);
            if (ec2 != std::errc{}) return false;
            outVec = { x, y };
            return true;
        };

        while (std::getline(in, line)) {
            if (line.find("\"texturePath\":") != std::string::npos) {
                size_t q1 = line.find('\"', line.find(':'));
                size_t q2 = line.find('\"', q1 + 1);
                if (q1 != std::string::npos && q2 != std::string::npos) {
                    _texturePath = line.substr(q1 + 1, q2 - q1 - 1);
                }
            } else if (line.find("\"width\":") != std::string::npos) {
                _textureWidth = std::atoi(line.substr(line.find(':') + 1).c_str());
            } else if (line.find("\"height\":") != std::string::npos) {
                _textureHeight = std::atoi(line.substr(line.find(':') + 1).c_str());
            } else if (line.find("    {") != std::string::npos) {
                inFrame = true;
                currentFrame = SpriteFrame();
            } else if (line.find("    }") != std::string::npos && inFrame) {
                _nameLookup[currentFrame.name] = _frames.size();
                _frames.push_back(currentFrame);
                inFrame = false;
            } else if (inFrame) {
                if (line.find("\"name\":") != std::string::npos) {
                    size_t q1 = line.find('\"', line.find(':'));
                    size_t q2 = line.find('\"', q1 + 1);
                    if (q1 != std::string::npos && q2 != std::string::npos) {
                        currentFrame.name = line.substr(q1 + 1, q2 - q1 - 1);
                    }
                } else if (line.find("\"uvMin\":") != std::string::npos) {
                    Vec2 uv{ 0.0f, 0.0f };
                    parseJsonVec2(line, uv);
                    currentFrame.uvMin = uv;
                } else if (line.find("\"uvMax\":") != std::string::npos) {
                    Vec2 uv{ 1.0f, 1.0f };
                    parseJsonVec2(line, uv);
                    currentFrame.uvMax = uv;
                } else if (line.find("\"size\":") != std::string::npos) {
                    Vec2 sz{ 32.0f, 32.0f };
                    parseJsonVec2(line, sz);
                    currentFrame.size = sz;
                } else if (line.find("\"pivot\":") != std::string::npos) {
                    Vec2 pv{ 0.5f, 0.5f };
                    parseJsonVec2(line, pv);
                    currentFrame.pivot = pv;
                } else if (line.find("\"duration\":") != std::string::npos) {
                    currentFrame.duration = static_cast<float>(std::atof(line.substr(line.find(':') + 1).c_str()));
                }
            }
        }
        return true;
    }

    // ==========================================================
    // AnimationPlayer2D Implementation
    // ==========================================================

    void AnimationPlayer2D::addAnimation(const Animation2D& anim) {
        _animations[anim.name] = anim;
    }

    void AnimationPlayer2D::play(const std::string& animName, bool restart) {
        if (_currentAnim == animName && _isPlaying && !restart) return;

        auto it = _animations.find(animName);
        if (it != _animations.end()) {
            _currentAnim = animName;
            _frameStep = 0;
            _timer = 0.0f;
            _isPlaying = true;
            _isFinished = false;
        }
    }

    void AnimationPlayer2D::stop() {
        _isPlaying = false;
    }

    void AnimationPlayer2D::update(float dt) {
        if (!_isPlaying || _currentAnim.empty()) return;

        auto it = _animations.find(_currentAnim);
        if (it == _animations.end() || it->second.frameIndices.empty()) return;

        const auto& anim = it->second;
        float frameTime = (anim.fps > 0.0f) ? (1.0f / anim.fps) : 0.1f;

        _timer += dt;
        while (_timer >= frameTime && _isPlaying) {
            _timer -= frameTime;
            _frameStep++;

            if (_frameStep >= anim.frameIndices.size()) {
                if (anim.loop) {
                    _frameStep = 0;
                } else {
                    _frameStep = anim.frameIndices.size() - 1;
                    _isPlaying = false;
                    _isFinished = true;
                    break;
                }
            }
        }
    }

    const SpriteFrame* AnimationPlayer2D::getCurrentFrame(const SpriteSheet2D& sheet) const {
        auto it = _animations.find(_currentAnim);
        if (it == _animations.end() || it->second.frameIndices.empty()) return nullptr;

        const auto& anim = it->second;
        if (_frameStep < anim.frameIndices.size()) {
            size_t frameIdx = anim.frameIndices[_frameStep];
            return sheet.getFrame(frameIdx);
        }
        return nullptr;
    }

    size_t AnimationPlayer2D::getCurrentFrameIndex() const {
        auto it = _animations.find(_currentAnim);
        if (it != _animations.end() && !it->second.frameIndices.empty() && _frameStep < it->second.frameIndices.size()) {
            return it->second.frameIndices[_frameStep];
        }
        return 0;
    }

    float AnimationPlayer2D::getProgress() const {
        auto it = _animations.find(_currentAnim);
        if (it == _animations.end() || it->second.frameIndices.empty()) return 0.0f;
        return static_cast<float>(_frameStep) / static_cast<float>(it->second.frameIndices.size());
    }

    // ==========================================================
    // Tilemap2D Implementation
    // ==========================================================

    Tilemap2D::Tilemap2D()
        : _width(16), _height(16), _tileWidth(32), _tileHeight(32) {
        addLayer("Ground");
    }

    Tilemap2D::Tilemap2D(int width, int height, int tileWidth, int tileHeight)
        : _width(width), _height(height), _tileWidth(tileWidth), _tileHeight(tileHeight) {
        addLayer("Ground");
    }

    void Tilemap2D::setDimensions(int w, int h) {
        _width = w;
        _height = h;
        for (auto& layer : _layers) {
            layer.width = w;
            layer.height = h;
            layer.tiles.resize(w * h, -1);
        }
    }

    size_t Tilemap2D::addLayer(const std::string& name) {
        TilemapLayer2D layer;
        layer.name = name;
        layer.width = _width;
        layer.height = _height;
        layer.tiles.resize(_width * _height, -1);
        layer.zOrder = static_cast<int>(_layers.size());
        _layers.push_back(layer);
        return _layers.size() - 1;
    }

    void Tilemap2D::removeLayer(size_t index) {
        if (index < _layers.size()) {
            _layers.erase(_layers.begin() + index);
        }
    }

    TilemapLayer2D* Tilemap2D::getLayer(size_t index) {
        if (index < _layers.size()) {
            return &_layers[index];
        }
        return nullptr;
    }

    const TilemapLayer2D* Tilemap2D::getLayer(size_t index) const {
        if (index < _layers.size()) {
            return &_layers[index];
        }
        return nullptr;
    }

    void Tilemap2D::setTile(size_t layerIdx, int x, int y, int tileId) {
        if (layerIdx < _layers.size() && x >= 0 && x < _width && y >= 0 && y < _height) {
            _layers[layerIdx].tiles[y * _width + x] = tileId;
        }
    }

    int Tilemap2D::getTile(size_t layerIdx, int x, int y) const {
        if (layerIdx < _layers.size() && x >= 0 && x < _width && y >= 0 && y < _height) {
            return _layers[layerIdx].tiles[y * _width + x];
        }
        return -1;
    }

    void Tilemap2D::render(SpriteBatcher& batcher, const Texture* texture, const Vec2& worldOffset) const {
        for (const auto& layer : _layers) {
            if (!layer.visible || layer.tiles.empty()) continue;

            Vec4 tint = { 1.0f, 1.0f, 1.0f, layer.opacity };
            for (int y = 0; y < layer.height; ++y) {
                for (int x = 0; x < layer.width; ++x) {
                    int tileId = layer.tiles[y * layer.width + x];
                    if (tileId < 0) continue;

                    Vec2 pos = {
                        worldOffset.x + static_cast<float>(x * _tileWidth),
                        worldOffset.y + static_cast<float>(y * _tileHeight)
                    };
                    Vec2 size = {
                        static_cast<float>(_tileWidth),
                        static_cast<float>(_tileHeight)
                    };

                    batcher.draw(texture, pos, size, tint, 0.0f, {0.0f, 0.0f}, layer.zOrder);
                }
            }
        }
    }

    bool Tilemap2D::saveToFile(const std::string& filePath) const {
        std::ofstream out(filePath);
        if (!out.is_open()) return false;

        out << "{\n";
        out << "  \"name\": \"" << "Tilemap" << "\",\n";
        out << "  \"width\": " << _width << ",\n";
        out << "  \"height\": " << _height << ",\n";
        out << "  \"tileWidth\": " << _tileWidth << ",\n";
        out << "  \"tileHeight\": " << _tileHeight << ",\n";
        out << "  \"spriteSheetPath\": \"" << _spriteSheetPath << "\",\n";
        out << "  \"layers\": [\n";

        for (size_t l = 0; l < _layers.size(); ++l) {
            const auto& lay = _layers[l];
            out << "    {\n";
            out << "      \"name\": \"" << lay.name << "\",\n";
            out << "      \"opacity\": " << lay.opacity << ",\n";
            out << "      \"visible\": " << (lay.visible ? "true" : "false") << ",\n";
            out << "      \"zOrder\": " << lay.zOrder << ",\n";
            out << "      \"tiles\": [";
            for (size_t t = 0; t < lay.tiles.size(); ++t) {
                out << lay.tiles[t] << (t + 1 < lay.tiles.size() ? "," : "");
            }
            out << "]\n";
            out << "    }" << (l + 1 < _layers.size() ? "," : "") << "\n";
        }

        out << "  ]\n";
        out << "}\n";
        return true;
    }

    bool Tilemap2D::loadFromFile(const std::string& filePath) {
        std::ifstream in(filePath);
        if (!in.is_open()) return false;

        _layers.clear();
        std::string line;
        TilemapLayer2D currentLayer;
        bool inLayer = false;

        while (std::getline(in, line)) {
            if (line.find("\"width\":") != std::string::npos && !inLayer) {
                _width = std::atoi(line.substr(line.find(':') + 1).c_str());
            } else if (line.find("\"height\":") != std::string::npos && !inLayer) {
                _height = std::atoi(line.substr(line.find(':') + 1).c_str());
            } else if (line.find("\"tileWidth\":") != std::string::npos) {
                _tileWidth = std::atoi(line.substr(line.find(':') + 1).c_str());
            } else if (line.find("\"tileHeight\":") != std::string::npos) {
                _tileHeight = std::atoi(line.substr(line.find(':') + 1).c_str());
            } else if (line.find("\"spriteSheetPath\":") != std::string::npos) {
                size_t q1 = line.find('\"', line.find(':'));
                size_t q2 = line.find('\"', q1 + 1);
                if (q1 != std::string::npos && q2 != std::string::npos) {
                    _spriteSheetPath = line.substr(q1 + 1, q2 - q1 - 1);
                }
            } else if (line.find("    {") != std::string::npos) {
                inLayer = true;
                currentLayer = TilemapLayer2D();
                currentLayer.width = _width;
                currentLayer.height = _height;
            } else if (line.find("    }") != std::string::npos && inLayer) {
                _layers.push_back(currentLayer);
                inLayer = false;
            } else if (inLayer) {
                if (line.find("\"name\":") != std::string::npos) {
                    size_t q1 = line.find('\"', line.find(':'));
                    size_t q2 = line.find('\"', q1 + 1);
                    if (q1 != std::string::npos && q2 != std::string::npos) {
                        currentLayer.name = line.substr(q1 + 1, q2 - q1 - 1);
                    }
                } else if (line.find("\"opacity\":") != std::string::npos) {
                    currentLayer.opacity = static_cast<float>(std::atof(line.substr(line.find(':') + 1).c_str()));
                } else if (line.find("\"visible\":") != std::string::npos) {
                    currentLayer.visible = (line.find("true") != std::string::npos);
                } else if (line.find("\"zOrder\":") != std::string::npos) {
                    currentLayer.zOrder = std::atoi(line.substr(line.find(':') + 1).c_str());
                } else if (line.find("\"tiles\":") != std::string::npos) {
                    size_t b1 = line.find('[');
                    size_t b2 = line.find(']');
                    if (b1 != std::string::npos && b2 != std::string::npos) {
                        std::stringstream ss(line.substr(b1 + 1, b2 - b1 - 1));
                        std::string tVal;
                        while (std::getline(ss, tVal, ',')) {
                            if (!tVal.empty()) {
                                currentLayer.tiles.push_back(std::atoi(tVal.c_str()));
                            }
                        }
                    }
                }
            }
        }

        if (_layers.empty()) {
            addLayer("Ground");
        }
        return true;
    }

} // namespace Lab
