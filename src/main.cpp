#include "Lab.h"
#include "LabFont.h"
#include "LabDialogs.h"
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

        // Preload Core Textures
        std::vector<std::string> coreTextures = {
            "concrete_wall.bmp", "floor_tiles.bmp", "cryo_ice.bmp",
            "hazard_stripes.bmp", "metal_hull.bmp", "snow_frost.bmp",
            "floor_lab.bmp", "wall_concrete.bmp", "brick_wall.bmp"
        };
        for (const auto& texName : coreTextures) {
            getTexture(texName);
        }

        // Load glTF 2.0 animation from Blender
        SkeletalAnimation::loadGLTFAnimation("assets/animations/bot_walk.gltf", _botAnim);
        _patrolBot.position = Vec3(0.0f, 0.0f, -6.0f);

        // Start in Map Selection Menu so the player can choose a mission
        _inMenu = true;
        glfwSetInputMode(getWindow(), GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    }

    Texture* getTexture(const std::string& path) {
        if (path.empty()) return nullptr;
        auto it = _textures.find(path);
        if (it != _textures.end()) return it->second.get();
        auto tex = std::make_unique<Texture>(path);
        if (tex && tex->getId() != 0) {
            Texture* ptr = tex.get();
            _textures[path] = std::move(tex);
            return ptr;
        }
        return nullptr;
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
            _availableMaps.push_back("assets/maps/facility_alpha.labmap");
            _availableMaps.push_back("assets/maps/cryo_outpost.labmap");
        }
    }

    void loadSelectedMap(const std::string& mapPath) {
        LabLog::info("Loading Map: " + mapPath);
        auto loaded = LabMap::loadFromFile(mapPath);

        if (loaded) {
            _currentMap = std::move(loaded);

            // Apply map atmospheric parameters
            Renderer::setSunLight(
                _currentMap->metadata.sunDir,
                _currentMap->metadata.sunColor,
                _currentMap->metadata.ambientColor
            );

            // Set player spawn
            _camera.setPosition(_currentMap->spawn.position);
            _velocity = { 0, 0, 0 };

            // Preload props and meshes
            for (const auto& prop : _currentMap->props) {
                if (!_meshes.contains(prop.modelPath)) {
                    Mesh* m = Mesh::loadSTL(prop.modelPath);
                    if (m) _meshes[prop.modelPath] = std::unique_ptr<Mesh>(m);
                }
                if (!prop.texturePath.empty()) {
                    getTexture(prop.texturePath);
                }
            }

            // Preload brush textures
            for (const auto& brush : _currentMap->brushes) {
                if (!brush.texturePath.empty()) {
                    getTexture(brush.texturePath);
                }
            }

            // Ensure map is registered in list and selected
            bool found = false;
            for (int i = 0; i < (int)_availableMaps.size(); ++i) {
                if (_availableMaps[i] == mapPath) {
                    _selectedMapIndex = i;
                    found = true;
                    break;
                }
            }
            if (!found) {
                _availableMaps.push_back(mapPath);
                _selectedMapIndex = (int)_availableMaps.size() - 1;
            }

            // Switch to gameplay mode and lock cursor
            _inMenu = false;
            glfwSetInputMode(getWindow(), GLFW_CURSOR, GLFW_CURSOR_DISABLED);
            LabLog::info("Map successfully loaded and entered: " + mapPath);
        } else {
            LabLog::error("Failed to load map: " + mapPath);
        }
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
            float scaleX = 1280.0f / (float)std::max(1, getWidth());
            float scaleY = 720.0f / (float)std::max(1, getHeight());
            float mx = Input::mousePos.x * scaleX;
            float my = Input::mousePos.y * scaleY;

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

            // ESC key: Resume mission if a map is already loaded
            if (Input::isKeyPressed(GLFW_KEY_ESCAPE)) {
                if (!_escPressedLast) {
                    if (_currentMap) {
                        _inMenu = false;
                        glfwSetInputMode(getWindow(), GLFW_CURSOR, GLFW_CURSOR_DISABLED);
                    }
                    _escPressedLast = true;
                }
            } else {
                _escPressedLast = false;
            }

            // O key: Open native Windows File Dialog
            if (Input::isKeyPressed('O') || Input::isKeyPressed('o')) {
                if (!_oPressedLast) {
                    std::string picked = LabDialogs::openFileDialog(getWindow(), "Lab Map Files (*.labmap)\0*.labmap\0All Files (*.*)\0*.*\0", "assets\\maps");
                    if (!picked.empty()) {
                        loadSelectedMap(picked);
                    }
                    _oPressedLast = true;
                }
            } else {
                _oPressedLast = false;
            }

            // Mouse click on Launch, Open from disk, Resume, or list items
            if (Input::isMouseButtonPressed(0)) {
                if (!_menuLmbLast) {
                    // Click on Launch Map button (x: 140..390, y: 560..608)
                    if (mx >= 140.0f && mx <= 390.0f && my >= 560.0f && my <= 608.0f) {
                        if (!_availableMaps.empty() && _selectedMapIndex < (int)_availableMaps.size()) {
                            loadSelectedMap(_availableMaps[_selectedMapIndex]);
                        }
                    }
                    // Click on Open From Disk button (x: 410..690, y: 560..608)
                    else if (mx >= 410.0f && mx <= 690.0f && my >= 560.0f && my <= 608.0f) {
                        std::string picked = LabDialogs::openFileDialog(getWindow(), "Lab Map Files (*.labmap)\0*.labmap\0All Files (*.*)\0*.*\0", "assets\\maps");
                        if (!picked.empty()) {
                            loadSelectedMap(picked);
                        }
                    }
                    // Click on Resume Mission button (x: 710..970, y: 560..608)
                    else if (_currentMap && mx >= 710.0f && mx <= 970.0f && my >= 560.0f && my <= 608.0f) {
                        _inMenu = false;
                        glfwSetInputMode(getWindow(), GLFW_CURSOR, GLFW_CURSOR_DISABLED);
                    }
                    // Click on map list items
                    else if (mx >= 140.0f && mx <= 940.0f && my >= 130.0f) {
                        int clickedIdx = (int)((my - 130.0f) / 55.0f);
                        if (clickedIdx >= 0 && clickedIdx < (int)_availableMaps.size()) {
                            if (_selectedMapIndex == clickedIdx) {
                                loadSelectedMap(_availableMaps[clickedIdx]);
                            } else {
                                _selectedMapIndex = clickedIdx;
                            }
                        }
                    }
                    _menuLmbLast = true;
                }
            } else {
                _menuLmbLast = false;
            }
            return;
        }

        // Gameplay camera orientation update
        _camera.update(Input::mouseDelta);

        // Return to map menu with M or ESC key
        if (Input::isKeyPressed(GLFW_KEY_M) || Input::isKeyPressed(GLFW_KEY_ESCAPE)) {
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

        // Feed real dynamic player values to HUD
        if (_currentMap) _hud.mapName = _currentMap->metadata.name;

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

        // Header title
        LabFont::drawText(120.0f, 72.0f, "FROZEN-LIFE : MAP SELECTION & MISSION SELECT", 2.2f, Vec3(0.9f, 0.95f, 1.0f), LabFontType::GeoSans);

        // List available maps with titles
        float startY = 130.0f;
        for (int i = 0; i < (int)_availableMaps.size(); ++i) {
            bool isSelected = (i == _selectedMapIndex);
            float itemY = startY + i * 55.0f;

            // Highlight bar
            Vec3 barColor = isSelected ? Vec3(0.18f, 0.45f, 0.75f) : Vec3(0.12f, 0.16f, 0.22f);
            Renderer::drawRect(140.0f, itemY, 800.0f, 45.0f, barColor);

            // Selection indicator marker
            if (isSelected) {
                Renderer::drawRect(140.0f, itemY, 6.0f, 45.0f, Vec3(0.98f, 0.78f, 0.08f));
            }

            // Map filename / path label
            std::string mapDisplay = _availableMaps[i];
            LabFont::drawText(160.0f, itemY + 14.0f, mapDisplay, 2.0f, isSelected ? Vec3(1, 1, 1) : Vec3(0.7f, 0.75f, 0.8f), LabFontType::GeoSans);
        }

        // Action Button 1: Launch Map [ENTER]
        Renderer::drawRect(140.0f, 560.0f, 250.0f, 48.0f, Vec3(0.18f, 0.65f, 0.45f));
        Renderer::drawRect(142.0f, 562.0f, 246.0f, 44.0f, Vec3(0.22f, 0.75f, 0.52f));
        LabFont::drawText(160.0f, 576.0f, "LAUNCH MAP [ENTER]", 1.7f, Vec3(1, 1, 1), LabFontType::GeoSans);

        // Action Button 2: Browse File... [O key / Click] (Native Windows Open Dialog)
        Renderer::drawRect(410.0f, 560.0f, 280.0f, 48.0f, Vec3(0.22f, 0.45f, 0.75f));
        Renderer::drawRect(412.0f, 562.0f, 276.0f, 44.0f, Vec3(0.28f, 0.55f, 0.88f));
        LabFont::drawText(425.0f, 576.0f, "OPEN FROM DISK... [O]", 1.7f, Vec3(1, 1, 1), LabFontType::GeoSans);

        // Action Button 3: Resume Mission [ESC] (only visible when a map is loaded)
        if (_currentMap) {
            Renderer::drawRect(710.0f, 560.0f, 260.0f, 48.0f, Vec3(0.75f, 0.45f, 0.15f));
            Renderer::drawRect(712.0f, 562.0f, 256.0f, 44.0f, Vec3(0.88f, 0.55f, 0.20f));
            LabFont::drawText(725.0f, 576.0f, "RESUME MISSION [ESC]", 1.7f, Vec3(1, 1, 1), LabFontType::GeoSans);
        }

        // Instructions Footer
        LabFont::drawText(140.0f, 622.0f, "USE ARROWS / MOUSE TO SELECT | ENTER: LAUNCH | O: OPEN FILE | ESC / M: MENU", 1.4f, Vec3(0.55f, 0.65f, 0.75f), LabFontType::GeoSans);

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
            // Render map brushes with Frustum Culling & Source Tri-Planar UV scaling
            for (const auto& b : _currentMap->brushes) {
                Vec3 halfSize = b.size * 0.5f;
                Vec3 bMin = b.position - halfSize;
                Vec3 bMax = b.position + halfSize;
                if (!_camera.isInFrustum(bMin, bMax)) continue;

                Texture* tex = b.texturePath.empty() ? nullptr : getTexture(b.texturePath);
                Renderer::drawCube(b.position, b.size, b.color, tex, true, b.uvScale, b.uvMode);
            }

            // Render map props (STL models) with Frustum Culling
            for (const auto& p : _currentMap->props) {
                if (_meshes.contains(p.modelPath)) {
                    Vec3 halfScale = p.scale * 0.5f;
                    Vec3 pMin = p.position - halfScale;
                    Vec3 pMax = p.position + halfScale;
                    if (!_camera.isInFrustum(pMin, pMax)) continue;

                    Texture* tex = p.texturePath.empty() ? nullptr : getTexture(p.texturePath);
                    Renderer::drawMesh(*_meshes[p.modelPath], p.position, p.rotation, p.scale, p.color, tex);
                }
            }

            // Render procedural animated doors with Frustum Culling
            for (const auto& d : _currentMap->doors) {
                Vec3 animatedPos = d.position + d.openOffset * d.currentProgress;
                Vec3 halfSize = d.size * 0.5f;
                Vec3 dMin = animatedPos - halfSize;
                Vec3 dMax = animatedPos + halfSize;
                if (!_camera.isInFrustum(dMin, dMax)) continue;

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
    bool _escPressedLast = false;

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
    bool _oPressedLast = false;
    bool _menuLmbLast = false;

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
