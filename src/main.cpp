#include "Lab.h"
#include <glad/gl.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <algorithm>
#include <memory>
#include <vector>
#include <filesystem>
#include <unordered_map>

using namespace Lab;

class FrozenLife : public Engine {
public:
    FrozenLife()
        : Engine("Frozen-Life: Lab FPS", 1280, 720),
          _camera(75.0f, 16.0f / 9.0f, 0.01f, 1000.0f),
          _velocity{ 0, 0, 0 },
          _isGrounded(false),
          _isJumping(false),
          _bobTime(0.0f),
          _muzzleFlashTime(0.0f) {
    }

    void onInit() override {
        LabLog::info("Frozen-Life Init: Modular .LABMAP and GUI System...");
        Renderer::init();

        // Discover available maps in assets/maps/
        scanMapFiles();

        // Load Default Test Texture
        _textures["Test.bmp"] = std::make_unique<Texture>("Test.bmp");

        // Start in Map Selection Menu
        _inMenu = true;
        glfwSetInputMode(getWindow(), GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    }

    void scanMapFiles() {
        _availableMaps.clear();
        std::vector<std::string> searchDirs = { "assets/maps", "../assets/maps", "../../assets/maps" };
        for (const auto& dir : searchDirs) {
            if (std::filesystem::exists(dir)) {
                for (const auto& entry : std::filesystem::directory_iterator(dir)) {
                    if (entry.path().extension() == ".labmap") {
                        _availableMaps.push_back(entry.path().string());
                    }
                }
                if (!_availableMaps.empty()) break;
            }
        }
        if (_availableMaps.empty()) {
            _availableMaps.push_back("facility_alpha.labmap");
            _availableMaps.push_back("cryo_outpost.labmap");
        }
    }

    void loadSelectedMap(const std::string& mapPath) {
        LabLog::info("Loading Map: " + mapPath);
        _currentMap = LabMap::loadFromFile(mapPath);

        if (_currentMap) {
            // Apply map atmospheric parameters
            Renderer::setSunLight(
                _currentMap->metadata.sunDir,
                _currentMap->metadata.sunColor,
                _currentMap->metadata.ambientColor
            );

            // Set player spawn
            _camera.setPosition(_currentMap->spawn.position);

            // Preload props and meshes
            for (const auto& prop : _currentMap->props) {
                if (!_meshes.contains(prop.modelPath)) {
                    Mesh* m = Mesh::loadSTL(prop.modelPath);
                    if (m) _meshes[prop.modelPath] = std::unique_ptr<Mesh>(m);
                }
                if (!prop.texturePath.empty() && !_textures.contains(prop.texturePath)) {
                    _textures[prop.texturePath] = std::make_unique<Texture>(prop.texturePath);
                }
            }

            // Preload brush textures
            for (const auto& brush : _currentMap->brushes) {
                if (!brush.texturePath.empty() && !_textures.contains(brush.texturePath)) {
                    _textures[brush.texturePath] = std::make_unique<Texture>(brush.texturePath);
                }
            }
        }

        // Switch to gameplay mode and lock cursor
        _inMenu = false;
        glfwSetInputMode(getWindow(), GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    }

    void onFixedUpdate(float fixedDelta) override {
        if (_inMenu) return;

        // Physics tick rate (64 ticks per second)
        bool isSprinting = Input::isKeyPressed(340); // Left Shift
        float speed = isSprinting ? 8.5f : 4.5f;
        Vec3 inputDir = { 0, 0, 0 };

        Vec3 front = _camera.getFront();
        front.y = 0;
        front = front.normalized();

        Vec3 right = _camera.getRight();
        right.y = 0;
        right = right.normalized();

        if (Input::isKeyPressed('W') || Input::isKeyPressed('w')) inputDir += front;
        if (Input::isKeyPressed('S') || Input::isKeyPressed('s')) inputDir -= front;
        if (Input::isKeyPressed('A') || Input::isKeyPressed('a')) inputDir -= right;
        if (Input::isKeyPressed('D') || Input::isKeyPressed('d')) inputDir += right;

        if (inputDir.lengthSq() > 0) {
            inputDir = inputDir.normalized();
            _velocity.x = inputDir.x * speed;
            _velocity.z = inputDir.z * speed;
            _bobTime += fixedDelta * (speed * 2.0f);
        } else {
            _velocity.x *= 0.85f;
            _velocity.z *= 0.85f;
        }

        // Jump physics (Spacebar = 32)
        if (Input::isKeyPressed(32) && _isGrounded) {
            _velocity.y = 5.0f;
            _isGrounded = false;
        }

        // Gravity
        _velocity.y -= 12.0f * fixedDelta;

        // Apply movement
        Vec3 pos = _camera.getPosition();
        pos += _velocity * fixedDelta;

        // Simple ground collision
        if (pos.y < 1.8f) {
            pos.y = 1.8f;
            _velocity.y = 0.0f;
            _isGrounded = true;
        }
        _camera.setPosition(pos);

        // Procedural Door animations and distance triggers
        if (_currentMap) {
            for (auto& door : _currentMap->doors) {
                float distSq = (pos - door.position).lengthSq();
                float triggerRadiusSq = door.triggerRadius * door.triggerRadius;
                door.isOpen = (distSq < triggerRadiusSq);

                if (door.isOpen && door.currentProgress < 1.0f) {
                    door.currentProgress = std::min(1.0f, door.currentProgress + fixedDelta * door.openSpeed);
                } else if (!door.isOpen && door.currentProgress > 0.0f) {
                    door.currentProgress = std::max(0.0f, door.currentProgress - fixedDelta * door.openSpeed);
                }
            }
        }
    }

    void onUpdate(const Time& time) override {
        if (_inMenu) {
            // Check for key navigation in map menu
            if (Input::isKeyPressed(GLFW_KEY_UP)) {
                if (!_upPressedLast) {
                    if (_selectedMapIndex > 0) _selectedMapIndex--;
                    _upPressedLast = true;
                }
            } else {
                _upPressedLast = false;
            }

            if (Input::isKeyPressed(GLFW_KEY_DOWN)) {
                if (!_downPressedLast) {
                    if (_selectedMapIndex + 1 < (int)_availableMaps.size()) _selectedMapIndex++;
                    _downPressedLast = true;
                }
            } else {
                _downPressedLast = false;
            }

            if (Input::isKeyPressed(GLFW_KEY_ENTER)) {
                if (!_availableMaps.empty() && _selectedMapIndex < (int)_availableMaps.size()) {
                    loadSelectedMap(_availableMaps[_selectedMapIndex]);
                }
            }
            return;
        }

        // Gameplay camera orientation update
        _camera.update(Input::mouseDelta);

        // Return to map menu with M key
        if (Input::isKeyPressed(GLFW_KEY_M)) {
            _inMenu = true;
            glfwSetInputMode(getWindow(), GLFW_CURSOR, GLFW_CURSOR_NORMAL);
            return;
        }

        // Combat cooldown
        if (Input::isMouseButtonPressed(0) && _muzzleFlashTime <= 0.0f) {
            _muzzleFlashTime = 0.1f;
        }
        if (_muzzleFlashTime > 0.0f) {
            _muzzleFlashTime -= time.delta;
        }

        // F3 Debug Mode Toggle
        if (Input::isKeyPressed(292)) { // GLFW_KEY_F3
            if (!_f3PressedLast) {
                _debugMode = !_debugMode;
                LabLog::info("Debug mode: " + std::string(_debugMode ? "ON" : "OFF"));
                _f3PressedLast = true;
            }
        } else {
            _f3PressedLast = false;
        }

        // F1 Wireframe Toggle
        if (Input::isKeyPressed(290)) { // GLFW_KEY_F1
            if (!_f1PressedLast) {
                _wireframeMode = !_wireframeMode;
                glPolygonMode(GL_FRONT_AND_BACK, _wireframeMode ? GL_LINE : GL_FILL);
                LabLog::info("Wireframe mode: " + std::string(_wireframeMode ? "ON" : "OFF"));
                _f1PressedLast = true;
            }
        } else {
            _f1PressedLast = false;
        }
    }

    void drawWeapon() {
        Renderer::beginViewModel();

        float swayX = Input::mouseDelta.x * -0.001f;
        float swayY = Input::mouseDelta.y * 0.001f;
        float bobX = std::cos(_bobTime * 0.5f) * 0.02f;
        float bobY = std::abs(std::sin(_bobTime)) * 0.02f;

        Vec3 gunBasePos = { 0.4f + swayX + bobX, -0.4f + swayY - bobY, -0.6f };
        Vec3 gunRot = { 0.0f, -5.0f, 0.0f };

        // Gun barrel
        Renderer::drawCube(gunBasePos, gunRot, { 0.1f, 0.15f, 0.5f }, { 0.15f, 0.15f, 0.18f });
        // Gun handle
        Renderer::drawCube(gunBasePos + Vec3(0, -0.1f, 0.1f), gunRot, { 0.08f, 0.25f, 0.1f }, { 0.1f, 0.1f, 0.1f });

        // Muzzle Flash
        if (_muzzleFlashTime > 0.0f) {
            Renderer::drawCube(gunBasePos + Vec3(0, 0, -0.3f), gunRot, { 0.2f, 0.2f, 0.2f }, { 1.0f, 0.8f, 0.2f }, nullptr, false);
        }

        Renderer::endViewModel(_camera);
    }

    void drawMapMenu() {
        int w = 1280, h = 720;
        Renderer::beginUI(w, h);

        // Dark background overlay (Half-Life 2 style backdrop)
        Renderer::drawRect(0, 0, (float)w, (float)h, { 0.06f, 0.08f, 0.11f });

        // Menu Banner Frame
        Renderer::drawRect(100.0f, 60.0f, 1080.0f, 600.0f, { 0.1f, 0.13f, 0.18f });
        Renderer::drawRect(102.0f, 62.0f, 1076.0f, 40.0f, { 0.15f, 0.22f, 0.32f });

        // Header indicator (Cyan strip)
        Renderer::drawRect(102.0f, 100.0f, 1076.0f, 4.0f, { 0.2f, 0.75f, 0.95f });

        // List available maps
        float startY = 140.0f;
        for (int i = 0; i < (int)_availableMaps.size(); ++i) {
            bool isSelected = (i == _selectedMapIndex);
            float itemY = startY + i * 55.0f;

            // Highlight bar
            Vec3 barColor = isSelected ? Vec3(0.2f, 0.55f, 0.85f) : Vec3(0.12f, 0.16f, 0.22f);
            Renderer::drawRect(140.0f, itemY, 800.0f, 45.0f, barColor);

            // Selection indicator marker
            if (isSelected) {
                Renderer::drawRect(140.0f, itemY, 8.0f, 45.0f, { 0.3f, 0.9f, 1.0f });
            }

            // Mini visual representation box
            Renderer::drawRect(160.0f, itemY + 10.0f, 25.0f, 25.0f, isSelected ? Vec3(0.9f, 0.95f, 1.0f) : Vec3(0.4f, 0.45f, 0.5f));
        }

        // Launch button preview
        Renderer::drawRect(140.0f, 560.0f, 280.0f, 50.0f, { 0.18f, 0.65f, 0.45f });
        Renderer::drawRect(142.0f, 562.0f, 276.0f, 46.0f, { 0.25f, 0.85f, 0.55f });

        Renderer::endUI();
    }

    void drawUI() {
        int w = 1280, h = 720;
        Renderer::beginUI(w, h);

        // Simple crosshair
        float size = 4.0f;
        float centerX = w / 2.0f;
        float centerY = h / 2.0f;
        Renderer::drawRect(centerX - size, centerY - 1.0f, size * 2, 2.0f, { 1, 1, 1 });
        Renderer::drawRect(centerX - 1.0f, centerY - size, 2.0f, size * 2, { 1, 1, 1 });

        // Debug mode overlay (F3)
        if (_debugMode) {
            Renderer::drawRect(10.0f, 10.0f, 220.0f, 25.0f, { 0.1f, 0.1f, 0.15f });
            Renderer::drawRect(12.0f, 12.0f, 216.0f, 21.0f, { 0.2f, 0.8f, 0.2f });

            float speedMag = std::sqrt(_velocity.x * _velocity.x + _velocity.z * _velocity.z);
            Renderer::drawRect(10.0f, 40.0f, speedMag * 20.0f, 8.0f, { 0.2f, 0.6f, 1.0f });
            Renderer::drawRect(10.0f, 52.0f, 15.0f, 15.0f, _isGrounded ? Vec3(0.1f, 1.0f, 0.2f) : Vec3(1.0f, 0.2f, 0.1f));
        }

        Renderer::endUI();
    }

    void onRender() override {
        if (_inMenu) {
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            drawMapMenu();
            return;
        }

        Renderer::beginFrame(_camera);

        if (_currentMap) {
            // Render map brushes
            for (const auto& b : _currentMap->brushes) {
                Texture* tex = b.texturePath.empty() ? nullptr : _textures[b.texturePath].get();
                Renderer::drawCube(b.position, b.size, b.color, tex);
            }

            // Render map props (STL models)
            for (const auto& p : _currentMap->props) {
                if (_meshes.contains(p.modelPath)) {
                    Texture* tex = p.texturePath.empty() ? nullptr : _textures[p.texturePath].get();
                    Renderer::drawMesh(*_meshes[p.modelPath], p.position, p.rotation, p.scale, p.color, tex);
                }
            }

            // Render procedural animated doors
            for (const auto& d : _currentMap->doors) {
                Vec3 animatedPos = d.position + d.openOffset * d.currentProgress;
                Renderer::drawCube(animatedPos, d.size, d.color);
            }
        }

        // Viewmodel and HUD
        drawWeapon();
        drawUI();

        Renderer::endFrame();
    }

    void onShutdown() override {
        Renderer::shutdown();
    }

private:
    Camera _camera;
    std::unique_ptr<LabMap> _currentMap;
    std::unordered_map<std::string, std::unique_ptr<Texture>> _textures;
    std::unordered_map<std::string, std::unique_ptr<Mesh>> _meshes;

    // Map selection menu state
    bool _inMenu = true;
    std::vector<std::string> _availableMaps;
    int _selectedMapIndex = 0;
    bool _upPressedLast = false;
    bool _downPressedLast = false;

    // Movement state
    Vec3 _velocity;
    bool _isGrounded;
    bool _isJumping;
    float _bobTime;

    // Combat state
    float _muzzleFlashTime;

    // Debug mode
    bool _debugMode = false;
    bool _wireframeMode = false;
    bool _f3PressedLast = false;
    bool _f1PressedLast = false;
};

int main() {
    FrozenLife game;
    game.run();
    return 0;
}
