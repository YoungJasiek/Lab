#include "Lab.h"
#include "LabFont.h"
#include <glad/gl.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <vector>
#include <unordered_map>
#include <algorithm>
#include <cmath>

using namespace Lab;

class LabPlayerApp : public Engine {
public:
    LabPlayerApp(const std::string& title, int width, int height, const std::string& scenePath)
        : Engine(title, width, height),
          _gameTitle(title),
          _scenePath(scenePath),
          _camera(75.0f, (float)width / (float)height, 0.01f, 2000.0f),
          _playerPos(0.0f, 1.8f, 0.0f),
          _playerVel(0.0f, 0.0f, 0.0f),
          _isGrounded(false),
          _isPaused(false) {
    }

    void onInit() override {
        LabLog::info("Lab Universal Player initializing...");
        Renderer::init();

        // Discover and load available textures
        discoverTextures();
        discoverModels();

        // Load scene map
        loadScene();

        // Capture cursor for first-person gameplay
        setCursorCaptured(true);
        _isPaused = false;
        LabLog::info("Lab Player ready. Running scene: " + _scenePath);
    }

    void discoverTextures() {
        std::vector<std::string> searchDirs = { "assets/textures", "../assets/textures", "textures" };
        for (const auto& dir : searchDirs) {
            if (std::filesystem::exists(dir)) {
                for (const auto& entry : std::filesystem::directory_iterator(dir)) {
                    std::string ext = entry.path().extension().string();
                    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return (char)::tolower(c); });
                    if (ext == ".bmp" || ext == ".png" || ext == ".jpg" || ext == ".tga") {
                        std::string fname = entry.path().filename().string();
                        if (!_textures.contains(fname)) {
                            _textures[fname] = std::make_unique<Texture>(fname);
                        }
                    }
                }
                if (!_textures.empty()) break;
            }
        }
    }

    void discoverModels() {
        std::vector<std::string> searchDirs = { "assets/models", "../assets/models", "models" };
        for (const auto& dir : searchDirs) {
            if (std::filesystem::exists(dir)) {
                for (const auto& entry : std::filesystem::directory_iterator(dir)) {
                    std::string ext = entry.path().extension().string();
                    if (ext == ".stl" || ext == ".STL") {
                        std::string fname = entry.path().filename().string();
                        if (!_meshes.contains(fname)) {
                            Mesh* m = Mesh::loadSTL(fname);
                            if (m) _meshes[fname] = std::unique_ptr<Mesh>(m);
                        }
                    }
                }
                if (!_meshes.empty()) break;
            }
        }
    }

    void loadScene() {
        // If scene file does not exist, check fallback paths
        std::string finalPath = _scenePath;
        if (!std::filesystem::exists(finalPath)) {
            if (std::filesystem::exists("assets/maps/" + finalPath)) {
                finalPath = "assets/maps/" + finalPath;
            } else if (std::filesystem::exists("assets/maps/cryo_complex.labmap")) {
                finalPath = "assets/maps/cryo_complex.labmap";
            } else if (std::filesystem::exists("assets/maps/default_box.labmap")) {
                finalPath = "assets/maps/default_box.labmap";
            }
        }

        if (std::filesystem::exists(finalPath)) {
            _map = LabMap::loadFromFile(finalPath);
            LabLog::info("Loaded map from: " + finalPath);
        }

        if (!_map) {
            LabLog::warn("Map file not found (" + finalPath + "), creating procedural default arena.");
            _map = std::make_unique<LabMap>();
            _map->metadata.name = _gameTitle;
            _map->metadata.ambientColor = Vec3(0.25f, 0.28f, 0.35f);
            _map->metadata.sunDir = Vec3(-0.4f, -0.8f, -0.4f);
            _map->metadata.sunColor = Vec3(1.0f, 0.95f, 0.9f);

            // Ground floor
            MapBrush floor;
            floor.position = Vec3(0, -0.5f, 0);
            floor.size = Vec3(40, 1, 40);
            floor.color = Vec3(0.8f, 0.8f, 0.8f);
            floor.texturePath = "floor_lab.bmp";
            _map->brushes.push_back(floor);

            // Wall 1
            MapBrush wall1;
            wall1.position = Vec3(0, 2.5f, -20.0f);
            wall1.size = Vec3(40, 5, 1);
            wall1.color = Vec3(0.9f, 0.9f, 0.9f);
            wall1.texturePath = "concrete_wall.bmp";
            _map->brushes.push_back(wall1);

            // Wall 2
            MapBrush wall2;
            wall2.position = Vec3(0, 2.5f, 20.0f);
            wall2.size = Vec3(40, 5, 1);
            wall2.color = Vec3(0.9f, 0.9f, 0.9f);
            wall2.texturePath = "concrete_wall.bmp";
            _map->brushes.push_back(wall2);

            // Light
            MapLight lgt;
            lgt.position = Vec3(0, 6.0f, 0);
            lgt.color = Vec3(1.0f, 0.9f, 0.7f);
            lgt.intensity = 3.5f;
            lgt.radius = 25.0f;
            _map->lights.push_back(lgt);
        }

        // Set spawn point
        if (!_map->spawnPoints.empty()) {
            _playerPos = _map->spawnPoints[0].position;
            _camera.setYaw(_map->spawnPoints[0].yaw);
        } else {
            _playerPos = _map->spawn.position;
            _camera.setYaw(_map->spawn.yaw);
        }
        if (_playerPos.y < 1.0f) _playerPos.y = 1.8f;
        _camera.setPosition(_playerPos);
    }

    void onUpdate(const Time& time) override {
        float dt = time.delta;
        if (dt <= 0.0f || dt > 0.1f) dt = 0.016f;

        // Toggle pause / cursor lock with ESC
        if (Input::isKeyJustPressed(GLFW_KEY_ESCAPE)) {
            _isPaused = !_isPaused;
            setCursorCaptured(!_isPaused);
        }

        if (_isPaused) {
            if (Input::isKeyJustPressed(GLFW_KEY_Q)) {
                stop();
            }
            return;
        }

        // Mouse look
        Vec2 mouseDelta = Input::getMouseDelta();
        if (isCursorCaptured()) {
            _camera.update(mouseDelta);
        }

        // Movement input
        Vec3 forward = _camera.getForward();
        forward.y = 0.0f;
        float fLen = std::sqrt(forward.x * forward.x + forward.z * forward.z);
        if (fLen > 0.0001f) forward = forward * (1.0f / fLen);

        Vec3 right = _camera.getRight();
        right.y = 0.0f;
        float rLen = std::sqrt(right.x * right.x + right.z * right.z);
        if (rLen > 0.0001f) right = right * (1.0f / rLen);

        Vec3 moveDir(0, 0, 0);
        if (Input::isKeyPressed('W') || Input::isKeyPressed('w') || Input::isKeyPressed(GLFW_KEY_UP)) moveDir = moveDir + forward;
        if (Input::isKeyPressed('S') || Input::isKeyPressed('s') || Input::isKeyPressed(GLFW_KEY_DOWN)) moveDir = moveDir - forward;
        if (Input::isKeyPressed('D') || Input::isKeyPressed('d') || Input::isKeyPressed(GLFW_KEY_RIGHT)) moveDir = moveDir + right;
        if (Input::isKeyPressed('A') || Input::isKeyPressed('a') || Input::isKeyPressed(GLFW_KEY_LEFT)) moveDir = moveDir - right;

        float mLen = std::sqrt(moveDir.x * moveDir.x + moveDir.z * moveDir.z);
        if (mLen > 0.0001f) moveDir = moveDir * (1.0f / mLen);

        float speed = Input::isKeyPressed(GLFW_KEY_LEFT_SHIFT) ? 12.0f : 6.0f;

        // Gravity & Vertical Physics
        if (!_isGrounded) {
            _playerVel.y -= 25.0f * dt;
        } else {
            if (Input::isKeyPressed(GLFW_KEY_SPACE)) {
                _playerVel.y = 8.5f;
                _isGrounded = false;
            } else {
                _playerVel.y = 0.0f;
            }
        }

        // Horizontal velocity
        _playerVel.x = moveDir.x * speed;
        _playerVel.z = moveDir.z * speed;

        // Apply movement with brush collision resolution
        Vec3 newPos = _playerPos + _playerVel * dt;
        resolveCollisions(newPos);
        _playerPos = newPos;

        // Keep camera bound to player eyes
        _camera.setPosition(_playerPos + Vec3(0, 0.2f, 0));

        // Update dynamic doors
        if (_map) {
            for (auto& door : _map->doors) {
                float dist = std::sqrt(
                    (door.position.x - _playerPos.x) * (door.position.x - _playerPos.x) +
                    (door.position.z - _playerPos.z) * (door.position.z - _playerPos.z)
                );
                bool shouldOpen = (dist <= door.triggerRadius && !door.isLocked);
                float targetProg = shouldOpen ? 1.0f : 0.0f;
                if (door.currentProgress < targetProg) {
                    door.currentProgress = std::min(targetProg, door.currentProgress + door.openSpeed * dt);
                } else if (door.currentProgress > targetProg) {
                    door.currentProgress = std::max(targetProg, door.currentProgress - door.openSpeed * dt);
                }
            }
        }
    }

    void resolveCollisions(Vec3& pos) {
        float playerRadius = 0.5f;
        float playerHeight = 1.8f;
        _isGrounded = false;

        // Check against map brushes
        if (!_map) return;
        for (const auto& b : _map->brushes) {
            Vec3 halfSize = b.size * 0.5f;
            Vec3 bMin = b.position - halfSize;
            Vec3 bMax = b.position + halfSize;

            // Player AABB
            Vec3 pMin = pos - Vec3(playerRadius, 0.0f, playerRadius);
            Vec3 pMax = pos + Vec3(playerRadius, playerHeight, playerRadius);

            // Overlap test
            if (pMax.x > bMin.x && pMin.x < bMax.x &&
                pMax.y > bMin.y && pMin.y < bMax.y &&
                pMax.z > bMin.z && pMin.z < bMax.z) {

                // Calculate penetration depths
                float dLeft   = pMax.x - bMin.x;
                float dRight  = bMax.x - pMin.x;
                float dBottom = pMax.y - bMin.y;
                float dTop    = bMax.y - pMin.y;
                float dFront  = pMax.z - bMin.z;
                float dBack   = bMax.z - pMin.z;

                // Ground landing
                if (dTop > 0.0f && dTop < 0.6f && _playerVel.y <= 0.0f) {
                    pos.y = bMax.y;
                    _isGrounded = true;
                    _playerVel.y = 0.0f;
                    continue;
                }

                // Minimum penetration axis pushout
                float minX = (dLeft < dRight) ? -dLeft : dRight;
                float minZ = (dFront < dBack) ? -dFront : dBack;

                if (std::abs(minX) < std::abs(minZ)) {
                    pos.x += minX;
                } else {
                    pos.z += minZ;
                }
            }
        }

        // Safety floor boundary
        if (pos.y < -50.0f) {
            pos = Vec3(0, 2.0f, 0);
            _playerVel = Vec3(0, 0, 0);
        }
    }

    void onRender() override {
        // Map atmospheric lighting configuration
        if (_map) {
            glClearColor(0.06f, 0.08f, 0.12f, 1.0f);
            Renderer::setSunLight(_map->metadata.sunDir, _map->metadata.sunColor, _map->metadata.ambientColor);
        } else {
            glClearColor(0.05f, 0.07f, 0.10f, 1.0f);
            Renderer::setSunLight(Vec3(-0.4f, -0.8f, -0.4f), Vec3(1.0f, 0.95f, 0.9f), Vec3(0.25f, 0.28f, 0.35f));
        }

        // 1. Begin 3D Frame
        Renderer::beginFrame(_camera);

        // Upload map dynamic lights
        if (_map) {
            std::vector<PointLight> dynamicLights;
            dynamicLights.reserve(_map->lights.size());
            for (const auto& lgt : _map->lights) {
                dynamicLights.push_back(PointLight{
                    .position = lgt.position,
                    .color = lgt.color,
                    .intensity = lgt.intensity,
                    .radius = lgt.radius
                });
            }
            Renderer::setPointLights(dynamicLights);
        }

        // Render Brushes
        if (_map) {
            for (const auto& b : _map->brushes) {
                Vec3 halfSize = b.size * 0.5f;
                Vec3 bMin = b.position - halfSize;
                Vec3 bMax = b.position + halfSize;
                if (!_camera.isInFrustum(bMin, bMax)) continue;

                Texture* tex = b.texturePath.empty() ? nullptr : getTexture(b.texturePath);
                if (b.type == "poly" && !b.customVertices.empty()) {
                    if (!b.runtimeMesh) {
                        b.runtimeMesh = std::make_shared<Mesh>(b.customVertices, b.customIndices);
                    }
                    Renderer::drawMesh(*b.runtimeMesh, Vec3(0, 0, 0), Vec3(0, 0, 0), Vec3(1, 1, 1), b.color, tex);
                } else {
                    Renderer::drawCube(b.position, b.size, b.color, tex, true, b.uvScale, b.uvMode);
                }
            }

            // Render Props
            for (const auto& p : _map->props) {
                Vec3 halfScale = p.scale * 0.5f;
                if (!_camera.isInFrustum(p.position - halfScale, p.position + halfScale)) continue;

                Mesh* m = getMesh(p.modelPath);
                Texture* tex = p.texturePath.empty() ? nullptr : getTexture(p.texturePath);
                if (m) {
                    Renderer::drawMesh(*m, p.position, p.rotation, p.scale, p.color, tex);
                } else {
                    Renderer::drawCube(p.position, p.scale, p.color, tex);
                }
            }

            // Render Doors (animated offset)
            for (const auto& d : _map->doors) {
                Vec3 renderPos = d.position + d.openOffset * d.currentProgress;
                Renderer::drawCube(renderPos, d.size, d.color);
            }

            // Render weapon spawners / pickups
            for (const auto& wsp : _map->weaponSpawners) {
                Vec3 pPos = wsp.position + Vec3(0, 0.4f, 0);
                Renderer::drawCube(pPos, Vec3(0.6f, 0.2f, 0.3f), Vec3(0.9f, 0.8f, 0.2f));
            }
        }

        // 2. 2D HUD & Overlay
        int fbW = 0, fbH = 0;
        getFramebufferSize(fbW, fbH);
        float w = (fbW > 0) ? (float)fbW : (float)getWidth();
        float h = (fbH > 0) ? (float)fbH : (float)getHeight();

        Renderer::beginUI((int)w, (int)h);

        // Center Crosshair
        float cx = w * 0.5f;
        float cy = h * 0.5f;
        Renderer::drawRect(cx - 6.0f, cy - 1.0f, 12.0f, 2.0f, Vec3(1.0f, 1.0f, 1.0f));
        Renderer::drawRect(cx - 1.0f, cy - 6.0f, 2.0f, 12.0f, Vec3(1.0f, 1.0f, 1.0f));

        // Game watermark in top-left
        LabFont::drawText(16.0f, 16.0f, _gameTitle, 1.8f, Vec3(1.0f, 0.95f, 0.8f), LabFontType::System);
        if (_map) {
            LabFont::drawText(16.0f, 38.0f, "Sector: " + _map->metadata.name, 1.3f, Vec3(0.6f, 0.7f, 0.85f), LabFontType::System);
        }

        // Controls hint in bottom-left
        LabFont::drawText(16.0f, h - 28.0f, "WASD: Move | SPACE: Jump | SHIFT: Sprint | ESC: Pause Menu", 1.2f, Vec3(0.7f, 0.7f, 0.7f), LabFontType::System);

        // Pause Menu Overlay
        if (_isPaused) {
            Renderer::drawRect(0, 0, w, h, Vec3(0.04f, 0.05f, 0.07f));

            float bw = 460.0f;
            float bh = 220.0f;
            float bx = (w - bw) * 0.5f;
            float by = (h - bh) * 0.5f;

            Renderer::drawRect(bx, by, bw, bh, Vec3(0.14f, 0.16f, 0.20f));
            Renderer::drawRect(bx + 2, by + 2, bw - 4, 30.0f, Vec3(0.20f, 0.24f, 0.32f));
            LabFont::drawText(bx + 18.0f, by + 8.0f, "GAME PAUSED", 1.8f, Vec3(1.0f, 0.85f, 0.2f), LabFontType::System);

            LabFont::drawText(bx + 30.0f, by + 65.0f, "Press [ ESC ] to Resume Exploration", 1.5f, Vec3(0.9f, 0.9f, 0.9f), LabFontType::System);
            LabFont::drawText(bx + 30.0f, by + 105.0f, "Press [ Q ] to Exit Game", 1.5f, Vec3(0.85f, 0.4f, 0.4f), LabFontType::System);

            LabFont::drawText(bx + 30.0f, by + 160.0f, "Powered by Lab Engine - Universal Standalone Runtime", 1.15f, Vec3(0.5f, 0.55f, 0.65f), LabFontType::System);
        }

        Renderer::endUI();
    }

    Texture* getTexture(const std::string& path) {
        if (path.empty()) return nullptr;
        std::string fname = std::filesystem::path(path).filename().string();
        if (_textures.contains(fname)) return _textures[fname].get();
        if (std::filesystem::exists("assets/textures/" + fname)) {
            _textures[fname] = std::make_unique<Texture>("assets/textures/" + fname);
            return _textures[fname].get();
        }
        return nullptr;
    }

    Mesh* getMesh(const std::string& path) {
        if (path.empty()) return nullptr;
        std::string fname = std::filesystem::path(path).filename().string();
        if (_meshes.contains(fname)) return _meshes[fname].get();
        if (std::filesystem::exists("assets/models/" + fname)) {
            Mesh* m = Mesh::loadSTL("assets/models/" + fname);
            if (m) {
                _meshes[fname] = std::unique_ptr<Mesh>(m);
                return _meshes[fname].get();
            }
        }
        return nullptr;
    }

private:
    std::string _gameTitle;
    std::string _scenePath;
    Camera _camera;
    std::unique_ptr<LabMap> _map;
    Vec3 _playerPos;
    Vec3 _playerVel;
    bool _isGrounded;
    bool _isPaused;

    std::unordered_map<std::string, std::unique_ptr<Texture>> _textures;
    std::unordered_map<std::string, std::unique_ptr<Mesh>> _meshes;
};

// Simple config parser
struct GameLaunchConfig {
    std::string title = "Lab Game";
    std::string scene = "assets/maps/cryo_complex.labmap";
    int width = 1280;
    int height = 720;
    bool fullscreen = false;
};

static GameLaunchConfig loadGameConfig() {
    GameLaunchConfig cfg;

    // 1. Try game.config (key=value)
    std::ifstream cfgFile("game.config");
    if (cfgFile.is_open()) {
        std::string line;
        while (std::getline(cfgFile, line)) {
            size_t eq = line.find('=');
            if (eq != std::string::npos) {
                std::string k = line.substr(0, eq);
                std::string v = line.substr(eq + 1);
                // Trim
                k.erase(k.find_last_not_of(" \t\r\n") + 1);
                v.erase(0, v.find_first_not_of(" \t\r\n"));
                v.erase(v.find_last_not_of(" \t\r\n") + 1);
                if (k == "title") cfg.title = v;
                else if (k == "scene") cfg.scene = v;
                else if (k == "width") cfg.width = std::atoi(v.c_str());
                else if (k == "height") cfg.height = std::atoi(v.c_str());
                else if (k == "fullscreen") cfg.fullscreen = (v == "1" || v == "true");
            }
        }
        cfgFile.close();
        return cfg;
    }

    // 2. Try game.manifest (json)
    std::ifstream mfFile("game.manifest");
    if (!mfFile.is_open()) mfFile.open("build.manifest");
    if (mfFile.is_open()) {
        std::string line;
        while (std::getline(mfFile, line)) {
            size_t posTitle = line.find("\"title\":");
            if (posTitle != std::string::npos) {
                size_t q1 = line.find('\"', posTitle + 8);
                size_t q2 = line.find('\"', q1 + 1);
                if (q1 != std::string::npos && q2 != std::string::npos) {
                    cfg.title = line.substr(q1 + 1, q2 - q1 - 1);
                }
            }
            size_t posScene = line.find("\"entryScene\":");
            if (posScene != std::string::npos) {
                size_t q1 = line.find('\"', posScene + 13);
                size_t q2 = line.find('\"', q1 + 1);
                if (q1 != std::string::npos && q2 != std::string::npos) {
                    cfg.scene = line.substr(q1 + 1, q2 - q1 - 1);
                }
            }
        }
        mfFile.close();
    }

    return cfg;
}

int main(int argc, char* argv[]) {
    LabLog::info("Starting Lab Universal Player (LabPlayer Standalone)...");

    GameLaunchConfig config = loadGameConfig();

    // Command-line overrides
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--scene" && i + 1 < argc) {
            config.scene = argv[++i];
        } else if (arg == "--title" && i + 1 < argc) {
            config.title = argv[++i];
        } else if (arg == "--width" && i + 1 < argc) {
            config.width = std::atoi(argv[++i]);
        } else if (arg == "--height" && i + 1 < argc) {
            config.height = std::atoi(argv[++i]);
        }
    }

    LabPlayerApp app(config.title, config.width, config.height, config.scene);
    app.run();

    return 0;
}
