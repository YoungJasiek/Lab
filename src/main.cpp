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

        // Load glTF 2.0 animation from Blender
        SkeletalAnimation::loadGLTFAnimation("assets/animations/bot_walk.gltf", _botAnim);
        _patrolBot.position = Vec3(0.0f, 0.0f, -6.0f);

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

        // Free-cam Noclip flying in Hammer Editor mode
        if (_hammerEditor.active) {
            Vec3 camPos = _camera.getPosition();
            float flySpeed = isSprinting ? 24.0f : 12.0f;
            if (Input::isKeyPressed('W') || Input::isKeyPressed('w')) camPos += _camera.getFront() * flySpeed * fixedDelta;
            if (Input::isKeyPressed('S') || Input::isKeyPressed('s')) camPos -= _camera.getFront() * flySpeed * fixedDelta;
            if (Input::isKeyPressed('A') || Input::isKeyPressed('a')) camPos -= _camera.getRight() * flySpeed * fixedDelta;
            if (Input::isKeyPressed('D') || Input::isKeyPressed('d')) camPos += _camera.getRight() * flySpeed * fixedDelta;
            if (Input::isKeyPressed(32)) camPos.y += flySpeed * fixedDelta; // Space = Up
            if (Input::isKeyPressed(341)) camPos.y -= flySpeed * fixedDelta; // Left Ctrl = Down
            _camera.setPosition(camPos);
            return;
        }

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

        // Combat cooldown & Procedural Recoil
        if (Input::isMouseButtonPressed(0) && _muzzleFlashTime <= 0.0f && !_hammerEditor.active) {
            if (_hud.ammoClip > 0) {
                _hud.ammoClip--;
                _muzzleFlashTime = 0.08f;
                _weaponAnimator.onFire();
            }
        }
        if (_muzzleFlashTime > 0.0f) {
            _muzzleFlashTime -= time.delta;
        }

        // Weapon Reload (R key when not in Hammer Editor)
        if (Input::isKeyPressed('R') || Input::isKeyPressed('r')) {
            if (!_hammerEditor.active && _hud.ammoClip < 18 && _hud.ammoReserve > 0) {
                int needed = 18 - _hud.ammoClip;
                int transfer = std::min(needed, _hud.ammoReserve);
                _hud.ammoClip += transfer;
                _hud.ammoReserve -= transfer;
                _weaponAnimator.recoilSpring.addImpulse(Vec3(0.0f, -0.05f, 0.05f));
            }
        }

        // Procedural Weapon Sway and Bob update
        float horizontalSpeed = std::sqrt(_velocity.x * _velocity.x + _velocity.z * _velocity.z);
        _weaponAnimator.update(time.delta, Input::mouseDelta, horizontalSpeed);

        // Update Animated Patrol Bot
        _patrolBot.update(time.delta);

        // F2 Hammer Editor Toggle
        if (Input::isKeyPressed(291)) { // GLFW_KEY_F2
            if (!_f2PressedLast) {
                _hammerEditor.toggle(_camera);
                LabLog::info("Lab Hammer Editor: " + std::string(_hammerEditor.active ? "OPENED" : "CLOSED"));
                _f2PressedLast = true;
            }
        } else {
            _f2PressedLast = false;
        }

        // Hammer Editor Controls when active
        if (_hammerEditor.active && _currentMap) {
            _hammerEditor.update(time.delta, _camera, *_currentMap);

            // E key: Place object (Brush or Door)
            if (Input::isKeyPressed('E') || Input::isKeyPressed('e')) {
                if (!_ePressedLast) {
                    if (_hammerEditor.currentTool == EditorTool::CreateBrush) {
                        _hammerEditor.placeBrush(*_currentMap);
                    } else if (_hammerEditor.currentTool == EditorTool::CreateDoor) {
                        _hammerEditor.placeDoor(*_currentMap);
                    }
                    _ePressedLast = true;
                }
            } else {
                _ePressedLast = false;
            }

            // R key: Toggle Tool (Brush <-> Door)
            if (Input::isKeyPressed('R') || Input::isKeyPressed('r')) {
                if (!_rPressedLast) {
                    _hammerEditor.currentTool = (_hammerEditor.currentTool == EditorTool::CreateBrush) ? EditorTool::CreateDoor : EditorTool::CreateBrush;
                    _rPressedLast = true;
                }
            } else {
                _rPressedLast = false;
            }

            // K key: Quick Save Map to assets/maps/hammer_export.labmap
            if (Input::isKeyPressed('K') || Input::isKeyPressed('k')) {
                if (!_kPressedLast) {
                    _hammerEditor.saveMap(*_currentMap, "assets/maps/hammer_export.labmap");
                    _kPressedLast = true;
                }
            } else {
                _kPressedLast = false;
            }

            // Backspace / Delete: Delete last placed brush
            if (Input::isKeyPressed(259)) { // GLFW_KEY_BACKSPACE
                if (!_backspacePressedLast) {
                    _hammerEditor.deleteLast(*_currentMap);
                    _backspacePressedLast = true;
                }
            } else {
                _backspacePressedLast = false;
            }
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

        Vec3 gunDefaultPos = { 0.38f, -0.36f, -0.6f };
        Vec3 gunDefaultRot = { 0.0f, -4.0f, 0.0f };

        Vec3 gunBasePos = _weaponAnimator.calculatePositionOffset(gunDefaultPos);
        Vec3 gunRot = _weaponAnimator.calculateRotationOffset(gunDefaultRot);

        // Gun barrel / receiver
        Renderer::drawCube(gunBasePos, gunRot, { 0.09f, 0.14f, 0.52f }, { 0.16f, 0.18f, 0.22f });
        // Gun top rail (Source HL2 style cyan strip)
        Renderer::drawCube(gunBasePos + Vec3(0.0f, 0.075f, -0.05f), gunRot, { 0.04f, 0.03f, 0.38f }, { 0.2f, 0.7f, 0.9f });
        // Gun handle / grip
        Renderer::drawCube(gunBasePos + Vec3(0.0f, -0.11f, 0.12f), gunRot + Vec3(12.0f, 0.0f, 0.0f), { 0.07f, 0.22f, 0.1f }, { 0.1f, 0.1f, 0.12f });

        // Dynamic Muzzle Flash
        if (_muzzleFlashTime > 0.0f) {
            Renderer::drawCube(gunBasePos + Vec3(0.0f, 0.02f, -0.32f), gunRot, { 0.18f, 0.18f, 0.18f }, { 1.0f, 0.85f, 0.2f }, nullptr, false);
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

        // Half-Life 2 Inspired Amber & Cyan HUD (when not in editor mode)
        if (!_hammerEditor.active) {
            _hud.render(w, h);
        } else {
            // Lab Hammer Editor UI toolbar & status overlay
            _hammerEditor.drawUI(w, h);
        }

        // Debug mode overlay (F3)
        if (_debugMode) {
            Renderer::beginUI(w, h);
            Renderer::drawRect(10.0f, 10.0f, 220.0f, 25.0f, { 0.1f, 0.1f, 0.15f });
            Renderer::drawRect(12.0f, 12.0f, 216.0f, 21.0f, { 0.2f, 0.8f, 0.2f });

            float speedMag = std::sqrt(_velocity.x * _velocity.x + _velocity.z * _velocity.z);
            Renderer::drawRect(10.0f, 40.0f, speedMag * 20.0f, 8.0f, { 0.2f, 0.6f, 1.0f });
            Renderer::drawRect(10.0f, 52.0f, 15.0f, 15.0f, _isGrounded ? Vec3(0.1f, 1.0f, 0.2f) : Vec3(1.0f, 0.2f, 0.1f));
            Renderer::endUI();
        }
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

        // Render Animated Patrol Bot (Half-Life 2 / Combine Synth style)
        {
            float legSwing = std::sin(_patrolBot.walkCycle) * 0.25f;
            float bodyBob = std::abs(std::sin(_patrolBot.walkCycle * 2.0f)) * 0.05f;
            Vec3 botPos = _patrolBot.position + Vec3(0, bodyBob, 0);

            // Torso (Dark industrial steel)
            Renderer::drawCube(botPos + Vec3(0, 1.2f, 0), _patrolBot.rotation, Vec3(0.5f, 0.7f, 0.35f), Vec3(0.18f, 0.22f, 0.26f));
            // Head / Visor (Cyan optics)
            Renderer::drawCube(botPos + Vec3(0, 1.7f, 0), _patrolBot.rotation, Vec3(0.3f, 0.25f, 0.3f), Vec3(0.12f, 0.14f, 0.18f));
            Renderer::drawCube(botPos + Vec3(0, 1.7f, 0.16f), _patrolBot.rotation, Vec3(0.24f, 0.08f, 0.04f), Vec3(0.2f, 0.8f, 1.0f), nullptr, false);
            // Left Leg (Animated swing)
            Renderer::drawCube(botPos + Vec3(-0.16f, 0.5f, legSwing), _patrolBot.rotation, Vec3(0.12f, 0.8f, 0.15f), Vec3(0.15f, 0.15f, 0.18f));
            // Right Leg (Opposite swing)
            Renderer::drawCube(botPos + Vec3(0.16f, 0.5f, -legSwing), _patrolBot.rotation, Vec3(0.12f, 0.8f, 0.15f), Vec3(0.15f, 0.15f, 0.18f));
        }

        // Lab Hammer Editor 3D Ghost/Grid Overlay
        _hammerEditor.draw3DOverlay();

        // Viewmodel (only drawn when not in Hammer Editor mode)
        if (!_hammerEditor.active) {
            drawWeapon();
        }

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

    // Animation & Combat state
    WeaponAnimator _weaponAnimator;
    AnimatedBot _patrolBot;
    SkeletalAnimation _botAnim;
    float _muzzleFlashTime;

    // Hammer Editor & HUD
    LabHammerEditor _hammerEditor;
    LabHUD _hud;
    bool _f2PressedLast = false;
    bool _ePressedLast = false;
    bool _rPressedLast = false;
    bool _kPressedLast = false;
    bool _backspacePressedLast = false;

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
