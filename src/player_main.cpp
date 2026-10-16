#include "Lab.h"
#include "LabFont.h"
#include "LabGpuPreference.h"
#include "LabHUDEditor.h"
#include "LabScript.h"
#include "LabProject.h"
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

static void saveFrameToBMP(const char* filename, int width, int height) {
    std::vector<unsigned char> pixels(width * height * 4);
    glReadBuffer(GL_BACK);
    glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());

    int rowSize = ((width * 24 + 31) / 32) * 4;
    int imageSize = rowSize * height;
    int fileSize = 54 + imageSize;

    unsigned char header[54] = {
        'B', 'M',
        (unsigned char)(fileSize), (unsigned char)(fileSize >> 8), (unsigned char)(fileSize >> 16), (unsigned char)(fileSize >> 24),
        0, 0, 0, 0,
        54, 0, 0, 0,
        40, 0, 0, 0,
        (unsigned char)(width), (unsigned char)(width >> 8), (unsigned char)(width >> 16), (unsigned char)(width >> 24),
        (unsigned char)(height), (unsigned char)(height >> 8), (unsigned char)(height >> 16), (unsigned char)(height >> 24),
        1, 0, 24, 0,
        0, 0, 0, 0,
        (unsigned char)(imageSize), (unsigned char)(imageSize >> 8), (unsigned char)(imageSize >> 16), (unsigned char)(imageSize >> 24),
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
    };

    std::ofstream out(filename, std::ios::binary);
    if (!out.is_open()) return;
    out.write((char*)header, 54);

    std::vector<unsigned char> row(rowSize, 0);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            int srcIdx = (y * width + x) * 4;
            row[x * 3 + 0] = pixels[srcIdx + 2]; // B
            row[x * 3 + 1] = pixels[srcIdx + 1]; // G
            row[x * 3 + 2] = pixels[srcIdx + 0]; // R
        }
        out.write((char*)row.data(), rowSize);
    }
    out.close();
}

static std::string normalizeToLocalAsset(const std::string& inputPath, const std::string& fallbackSubfolder = "") {
    if (inputPath.empty()) return "";
    if (std::filesystem::exists(inputPath)) return inputPath;

    // 1. If path contains "assets/" or "assets\", strip foreign machine-specific absolute prefixes
    size_t aPos = inputPath.find("assets/");
    if (aPos == std::string::npos) aPos = inputPath.find("assets\\");
    if (aPos != std::string::npos) {
        std::string relFromAssets = inputPath.substr(aPos);
        std::replace(relFromAssets.begin(), relFromAssets.end(), '\\', '/');
        if (std::filesystem::exists(relFromAssets)) {
            return relFromAssets;
        }
    }

    // 2. Try VFS resolution
    std::string vfsResolved = ProjectManager::get().resolveAssetPath(inputPath).string();
    if (!vfsResolved.empty() && std::filesystem::exists(vfsResolved)) {
        return vfsResolved;
    }

    // 3. Try filename in specified fallback subfolder
    std::string fname = std::filesystem::path(inputPath).filename().string();
    if (!fallbackSubfolder.empty()) {
        std::string candidate = fallbackSubfolder + "/" + fname;
        if (std::filesystem::exists(candidate)) return candidate;
    }

    // 4. Try direct filename in standard directories
    if (std::filesystem::exists("assets/maps/" + fname)) return "assets/maps/" + fname;
    if (std::filesystem::exists("assets/scripts/" + fname)) return "assets/scripts/" + fname;
    if (std::filesystem::exists("assets/hud_projects/" + fname)) return "assets/hud_projects/" + fname;

    return inputPath;
}

class LabPlayerApp : public Engine {
public:
    LabPlayerApp(const std::string& title, int width, int height, const std::string& scenePath,
                 const std::string& hudPath = "", const std::string& rulesPath = "",
                 const std::string& screenshotPath = "", const std::string& dimension = "3D",
                 bool retroFX = false, bool retroScanlines = true, bool retroVignette = true,
                 bool retroAberration = true, int retroPixelScale = 2, int retroColorTheme = 0,
                 bool showSplash = true)
        : Engine(title, width, height),
          _gameTitle(title),
          _dimension(dimension),
          _scenePath(scenePath),
          _hudPath(hudPath),
          _rulesPath(rulesPath),
          _screenshotPath(screenshotPath),
          _retroFX(retroFX),
          _retroScanlines(retroScanlines),
          _retroVignette(retroVignette),
          _retroAberration(retroAberration),
          _retroPixelScale(retroPixelScale),
          _retroColorTheme(retroColorTheme),
          _showSplash(showSplash),
          _splashActive(showSplash),
          _splashTimer(0.0f),
          _splashDuration(2.5f),
          _camera(75.0f, (float)width / (float)height, 0.01f, 2000.0f),
          _playerPos(0.0f, 1.8f, 0.0f),
          _playerVel(0.0f, 0.0f, 0.0f),
          _isGrounded(false),
          _isPaused(false),
          _cardsCollected(0),
          _totalCards(3),
          _missionTimer(300.0f),
          _playerHealth(100.0f),
          _currentWeaponName("PISTOLET"),
          _weaponClip(12),
          _weaponReserve(60),
          _missionStatus("RUNNING"),
          _missionMessage("") {
    }

    void onInit() override {
        LabLog::info("Lab Universal Player initializing...");
        Renderer::init();

        // Discover and load available textures
        discoverTextures();
        discoverModels();

        // Load scene map
        loadScene();

        // Initialize Mission Rules Script (Lua) if configured
        initMissionRules();

        // Load custom HUD Project (.labhud) if configured
        initCustomHUD();

        // Camera setup based on dimension (2D pixel/canvas vs 3D perspective)
        if (_dimension == "2D" || _splashActive) {
            int w = 1280, h = 720;
            getFramebufferSize(w, h);
            float aspect = (h > 0) ? ((float)w / (float)h) : (16.0f / 9.0f);
            if (_dimension == "2D") InitializeCamera2D(_camera, 10.0f, aspect);
            setCursorCaptured(false);
        } else {
            // Capture cursor for first-person gameplay
            setCursorCaptured(true);
        }
        _isPaused = false;
        LabLog::info("Lab Player ready (" + _dimension + "). Running scene: " + _scenePath);
    }

    void discoverTextures() {
        auto& pm = ProjectManager::get();
        std::vector<std::string> searchDirs = {
            (pm.getActiveProjectAssetsPath() / "textures").string(),
            (pm.getEngineAssetsPath() / "textures").string(),
            "assets/textures", "../assets/textures", "textures"
        };
        for (const auto& dir : searchDirs) {
            if (std::filesystem::exists(dir)) {
                for (const auto& entry : std::filesystem::directory_iterator(dir)) {
                    std::string ext = entry.path().extension().string();
                    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return (char)::tolower(c); });
                    if (ext == ".bmp" || ext == ".png" || ext == ".jpg" || ext == ".tga") {
                        std::string fname = entry.path().filename().string();
                        if (!_textures.contains(fname)) {
                            _textures[fname] = std::make_unique<Texture>(entry.path().string());
                        }
                    }
                }
            }
        }
    }

    void discoverModels() {
        auto& pm = ProjectManager::get();
        std::vector<std::string> searchDirs = {
            (pm.getActiveProjectAssetsPath() / "models").string(),
            (pm.getEngineAssetsPath() / "models").string(),
            "assets/models", "../assets/models", "models"
        };
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
            }
        }
    }

    void initMissionRules() {
        std::string resolvedRules = normalizeToLocalAsset(_rulesPath, "assets/scripts");
        if ((resolvedRules.empty() || !std::filesystem::exists(resolvedRules)) && std::filesystem::exists("assets/scripts/game_mechanics.lua")) {
            resolvedRules = "assets/scripts/game_mechanics.lua";
        }

        if (!resolvedRules.empty() && std::filesystem::exists(resolvedRules)) {
            if (_scriptEngine.init(resolvedRules)) {
                _scriptEngine.setActiveMap(_map.get());
                _totalCards = _scriptEngine.getMissionCardCount();
                _missionTimer = _scriptEngine.getMissionTimeLimit();
                _playerHealth = _scriptEngine.getPlayerMaxHealth();
                _missionMessage = _scriptEngine.getMissionStartText();
                LabLog::info("Loaded mission rules script: " + resolvedRules + " (Cards: " +
                             std::to_string(_totalCards) + ", Timer: " + std::to_string((int)_missionTimer) + "s)");
            } else {
                LabLog::warn("Failed to load rules script: " + resolvedRules);
                if (_map) _scriptEngine.setActiveMap(_map.get());
            }
        } else {
            // Even without a specific rules file, always connect map entities to Lua script engine!
            if (_map) {
                _scriptEngine.setActiveMap(_map.get());
            }
        }
    }

    void initCustomHUD() {
        std::string resolvedHud = normalizeToLocalAsset(_hudPath, "assets/hud_projects");
        if ((resolvedHud.empty() || !std::filesystem::exists(resolvedHud)) && std::filesystem::exists("assets/hud_projects/default.labhud")) {
            resolvedHud = "assets/hud_projects/default.labhud";
        }

        if (!resolvedHud.empty() && std::filesystem::exists(resolvedHud)) {
            _hudProject = HUDProject::loadFromFile(resolvedHud);
            if (_hudProject) {
                LabLog::info("Loaded custom HUD project: " + resolvedHud + " (" + _hudProject->name + ")");
            } else {
                LabLog::warn("Failed to parse HUD project: " + resolvedHud);
            }
        }
    }

    void loadScene() {
        std::string finalPath = normalizeToLocalAsset(_scenePath, "assets/maps");
        if (!std::filesystem::exists(finalPath)) {
            if (std::filesystem::exists("assets/maps/default_box.labmap")) {
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

        // Set spawn point with standardized yaw convention
        // Map spawn convention: yaw 180 = facing -Z (North/corridor)
        // Camera spherical coordinates: yaw -90 = facing -Z
        // Formula: camYaw = spawnYaw - 270.0f
        float spawnYaw = 180.0f;
        if (!_map->spawnPoints.empty()) {
            _playerPos = _map->spawnPoints[0].position;
            spawnYaw = _map->spawnPoints[0].yaw;
        } else {
            _playerPos = _map->spawn.position;
            spawnYaw = _map->spawn.yaw;
        }
        if (_playerPos.y < 1.0f) _playerPos.y = 1.8f;

        float camYaw = spawnYaw - 270.0f;
        while (camYaw > 180.0f) camYaw -= 360.0f;
        while (camYaw <= -180.0f) camYaw += 360.0f;
        _camera.setYaw(camYaw);
        _camera.setPosition(_playerPos + Vec3(0, 0.2f, 0));

        // Active Camera resolution from scene (.labmap)
        _activeCamera = nullptr;
        if (!_map->cameras.empty()) {
            for (auto& c : _map->cameras) {
                if (c.active) {
                    _activeCamera = &c;
                    break;
                }
            }
            if (!_activeCamera) {
                _activeCamera = &_map->cameras[0];
            }
        }
        if (_activeCamera) {
            int w = 1280, h = 720;
            getFramebufferSize(w, h);
            float aspect = (h > 0) ? ((float)w / (float)h) : (16.0f / 9.0f);
            _camera.setPerspective(_activeCamera->fov, aspect, 0.1f, 1000.0f);
            ScriptEngine::setActiveCameraName(_activeCamera->name);
        }

        // Register dynamic camera switching callback from Lua
        ScriptEngine::registerCameraSwitchCallback([this](const std::string& camName) {
            if (!_map) return;
            for (auto& c : _map->cameras) {
                if (c.name == camName) {
                    _activeCamera = &c;
                    int w = 1280, h = 720;
                    getFramebufferSize(w, h);
                    float aspect = (h > 0) ? ((float)w / (float)h) : (16.0f / 9.0f);
                    _camera.setPerspective(_activeCamera->fov, aspect, 0.1f, 1000.0f);
                    break;
                }
            }
        });

        // Initialize weapon name from Weapons Lua table
        _currentWeaponName = _scriptEngine.getWeaponNameById(1);

        // Initialize weapon pad pickup tracking
        _weaponPadCollected.assign(_map->weaponSpawners.size(), false);
    }

    void onUpdate(const Time& time) override {
        float dt = time.delta;
        if (dt <= 0.0f || dt > 0.1f) dt = 0.016f;

        // Splash Screen Handling (~2.5s duration with fade-in / fade-out; skippable on key or mouse click)
        if (_splashActive) {
            _splashTimer += dt;
            bool skip = Input::isKeyJustPressed(GLFW_KEY_SPACE) || Input::isKeyJustPressed(GLFW_KEY_ENTER) ||
                        Input::isKeyJustPressed(GLFW_KEY_KP_ENTER) || Input::isKeyJustPressed(GLFW_KEY_ESCAPE) ||
                        Input::isMouseButtonJustPressed(GLFW_MOUSE_BUTTON_LEFT) || Input::isMouseButtonJustPressed(GLFW_MOUSE_BUTTON_RIGHT);
            if (skip || _splashTimer >= _splashDuration) {
                _splashActive = false;
                if (_dimension != "2D") {
                    setCursorCaptured(true);
                }
            }
            return;
        }

        // Toggle pause / cursor lock with ESC
        if (Input::isKeyJustPressed(GLFW_KEY_ESCAPE)) {
            _isPaused = !_isPaused;
            setCursorCaptured(!_isPaused);
        }

        // Animate retro scanlines & toggle key F8
        if (_retroFX) {
            _retroScanlinePhase += dt * 3.5f;
            if (_retroScanlinePhase > 6.283185f) _retroScanlinePhase -= 6.283185f;
        }
        if (Input::isKeyJustPressed(GLFW_KEY_F8)) {
            _retroFX = !_retroFX;
            LabLog::info(std::string("Retro CRT Post-Process FX toggled: ") + (_retroFX ? "ENABLED" : "DISABLED"));
        }

        // Mouse click to capture cursor when window loses focus / uncaptured
        if (!isCursorCaptured() && !_isPaused) {
            if (Input::isMouseButtonJustPressed(GLFW_MOUSE_BUTTON_LEFT)) {
                setCursorCaptured(true);
            }
        }

        if (_isPaused) {
            if (Input::isKeyJustPressed(GLFW_KEY_Q)) {
                stop();
            }
            return;
        }

        if (_dimension == "2D") {
            // Native 2D pan with WASD or arrow keys
            float speed2D = 10.0f;
            Vec3 camP = _camera.getPosition();
            if (Input::isKeyPressed('W') || Input::isKeyPressed('w') || Input::isKeyPressed(GLFW_KEY_UP)) camP.y += speed2D * dt;
            if (Input::isKeyPressed('S') || Input::isKeyPressed('s') || Input::isKeyPressed(GLFW_KEY_DOWN)) camP.y -= speed2D * dt;
            if (Input::isKeyPressed('D') || Input::isKeyPressed('d') || Input::isKeyPressed(GLFW_KEY_RIGHT)) camP.x += speed2D * dt;
            if (Input::isKeyPressed('A') || Input::isKeyPressed('a') || Input::isKeyPressed(GLFW_KEY_LEFT)) camP.x -= speed2D * dt;
            _camera.setPosition(camP);
            _camera.setPitch(0.0f);
            _camera.setYaw(-90.0f);
        } else {
            // 3D Mouse look
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

            // Camera position binding: Scene Camera (TPP / static) or FPS fallback
            if (_activeCamera && _activeCamera->follow_target == "player") {
                // TPP over-the-shoulder camera
                Vec3 fwd = _camera.getForward();
                Vec3 rightVec = _camera.getRight();
                Vec3 up(0, 1, 0);
                Vec3 eyePos = _playerPos + Vec3(0, 1.4f, 0);
                Vec3 targetCamPos = eyePos + rightVec * _activeCamera->tpp_offset.x
                                           + up * _activeCamera->tpp_offset.y
                                           + fwd * _activeCamera->tpp_offset.z;
                if (_map) {
                    for (const auto& b : _map->brushes) {
                        Vec3 bMin = b.position - b.size * 0.5f;
                        Vec3 bMax = b.position + b.size * 0.5f;
                        if (targetCamPos.x >= bMin.x - _activeCamera->collision_radius && targetCamPos.x <= bMax.x + _activeCamera->collision_radius &&
                            targetCamPos.y >= bMin.y - _activeCamera->collision_radius && targetCamPos.y <= bMax.y + _activeCamera->collision_radius &&
                            targetCamPos.z >= bMin.z - _activeCamera->collision_radius && targetCamPos.z <= bMax.z + _activeCamera->collision_radius) {
                            targetCamPos = eyePos + (targetCamPos - eyePos) * 0.5f;
                        }
                    }
                }
                _camera.setPosition(targetCamPos);
            } else if (_activeCamera && _activeCamera->follow_target.empty()) {
                // Static / anchor / scene camera
                _camera.setPosition(_activeCamera->position);
                _camera.setPitch(_activeCamera->rotation.x);
                _camera.setYaw(_activeCamera->rotation.y);
            } else if (_map && _map->spawnPoints.empty()) {
                // Clean scene without player spawn: keep scene camera
                if (_activeCamera) {
                    _camera.setPosition(_activeCamera->position);
                    _camera.setPitch(_activeCamera->rotation.x);
                    _camera.setYaw(_activeCamera->rotation.y);
                }
            } else {
                // Standard FPS camera bound to player eyes
                _camera.setPosition(_playerPos + Vec3(0, 0.2f, 0));
            }
        }

        // Mission timer countdown
        if (_missionTimer > 0.0f) {
            _missionTimer = std::max(0.0f, _missionTimer - dt);
        }

        // Lua Script Tick (calls OnUpdate(dt))
        _scriptEngine.update(dt);

        // Update dynamic doors & dispatch triggers
        if (_map) {
            for (size_t i = 0; i < _map->doors.size(); ++i) {
                auto& door = _map->doors[i];
                float dist = std::sqrt(
                    (door.position.x - _playerPos.x) * (door.position.x - _playerPos.x) +
                    (door.position.z - _playerPos.z) * (door.position.z - _playerPos.z)
                );
                bool shouldOpen = (dist <= door.triggerRadius && !door.isLocked);
                if (shouldOpen && !door.isOpen) {
                    door.isOpen = true;
                    _scriptEngine.callOnTriggerEnter((int)i, 0);
                } else if (!shouldOpen && door.isOpen) {
                    door.isOpen = false;
                }

                float targetProg = shouldOpen ? 1.0f : 0.0f;
                if (door.currentProgress < targetProg) {
                    door.currentProgress = std::min(targetProg, door.currentProgress + door.openSpeed * dt);
                } else if (door.currentProgress > targetProg) {
                    door.currentProgress = std::max(targetProg, door.currentProgress - door.openSpeed * dt);
                }
            }

            // Weapon Spawner proximity: pickup collection & mission card progression
            for (size_t i = 0; i < _map->weaponSpawners.size(); ++i) {
                if (_weaponPadCollected[i]) continue;
                const auto& wsp = _map->weaponSpawners[i];
                float dist = std::sqrt(
                    (wsp.position.x - _playerPos.x) * (wsp.position.x - _playerPos.x) +
                    (wsp.position.z - _playerPos.z) * (wsp.position.z - _playerPos.z)
                );
                if (dist < 1.8f) {
                    _weaponPadCollected[i] = true;
                    _cardsCollected = std::min(_totalCards, _cardsCollected + 1);
                    _currentWeaponName = _scriptEngine.getWeaponNameById(wsp.weaponId);
                    _scriptEngine.callOnTriggerEnter((int)i, 1);
                }
            }
        }

        // Mission Progress dispatch
        auto [progStatus, progMsg] = _scriptEngine.callMissionProgress(_cardsCollected, _missionTimer);
        if (!progStatus.empty()) _missionStatus = progStatus;
        if (!progMsg.empty()) _missionMessage = progMsg;
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
                [[maybe_unused]] float dBottom = pMax.y - bMin.y;
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
        if (_splashActive) {
            renderSplashScreen();
            return;
        }

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
                    Renderer::drawMesh(*b.runtimeMesh, Vec3(0, 0, 0), b.rotation, Vec3(1, 1, 1), b.color, tex);
                } else {
                    if (b.rotation.x != 0.0f || b.rotation.y != 0.0f || b.rotation.z != 0.0f) {
                        Renderer::drawCube(b.position, b.rotation, b.size, b.color, tex, true, b.uvScale, b.uvMode);
                    } else {
                        Renderer::drawCube(b.position, b.size, b.color, tex, true, b.uvScale, b.uvMode);
                    }
                }
            }

            // In TPP mode: render third-person player avatar model / avatar silhouette
            if (_activeCamera && _activeCamera->follow_target == "player") {
                float pYaw = _camera.getYaw() + 90.0f;
                Renderer::drawCube(_playerPos + Vec3(0, -0.1f, 0), Vec3(0.0f, -pYaw, 0.0f), Vec3(0.7f, 1.6f, 0.5f), Vec3(0.2f, 0.45f, 0.8f), nullptr, true);
                Renderer::drawCube(_playerPos + Vec3(0, 0.95f, 0), Vec3(0.0f, -pYaw, 0.0f), Vec3(0.42f, 0.45f, 0.42f), Vec3(0.85f, 0.75f, 0.65f), nullptr, true);
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
            for (size_t i = 0; i < _map->weaponSpawners.size(); ++i) {
                const auto& wsp = _map->weaponSpawners[i];
                Vec3 pPos = wsp.position + Vec3(0, 0.4f, 0);
                Vec3 col = _weaponPadCollected[i] ? Vec3(0.3f, 0.3f, 0.3f) : Vec3(0.9f, 0.8f, 0.2f);
                Renderer::drawCube(pPos, Vec3(0.6f, 0.2f, 0.3f), col);
            }

            // Render 2D Sprites (World Billboards in 3D)
            for (const auto& sp : _map->sprites) {
                Texture* tex = sp.texturePath.empty() ? nullptr : getTexture(sp.texturePath);
                if (!sp.texturePath.empty() && !tex) {
                    LabLog::warn("Missing texture for sprite_2d: " + sp.texturePath);
                }
                Vec3 halfScale(sp.size.x * 0.5f, sp.size.y * 0.5f, 0.05f);
                if (!_camera.isInFrustum(sp.position - halfScale, sp.position + halfScale)) continue;

                if (sp.billboard) {
                    float dx = _camera.getPosition().x - sp.position.x;
                    float dz = _camera.getPosition().z - sp.position.z;
                    float rotY = std::atan2(dx, dz) * (180.0f / 3.14159265f);
                    Renderer::drawCube(sp.position, Vec3(0.0f, rotY, 0.0f), Vec3(sp.size.x, sp.size.y, 0.02f),
                                       Vec3(sp.color.x, sp.color.y, sp.color.z), tex, false);
                } else {
                    Renderer::drawCube(sp.position, Vec3(0.0f, sp.rotation, 0.0f), Vec3(sp.size.x, sp.size.y, 0.02f),
                                       Vec3(sp.color.x, sp.color.y, sp.color.z), tex, false);
                }
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

        // Render Custom HUD if loaded from .labhud
        if (_hudProject) {
            renderCustomHUD(w, h);
        }

        // Window Focus Hint: displayed when mouse is not captured
        if (!isCursorCaptured() && !_isPaused) {
            float bw = 380.0f;
            float bh = 42.0f;
            float bx = (w - bw) * 0.5f;
            float by = cy + 45.0f;

            Renderer::drawRect(bx, by, bw, bh, Vec3(0.08f, 0.10f, 0.14f));
            Renderer::drawRect(bx + 2, by + 2, bw - 4, bh - 4, Vec3(0.18f, 0.22f, 0.30f));
            LabFont::drawText(bx + 22.0f, by + 12.0f, "Kliknij, aby zlapac mysz", 1.4f, Vec3(1.0f, 0.90f, 0.25f), LabFontType::System);
        }

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

        // Retro CRT & Post-Process FX (Scanlines, Vignette, CRT Badge)
        if (_retroFX) {
            if (_retroScanlines) {
                float stepY = (float)_retroPixelScale * 2.0f;
                Vec3 scanCol = (_retroColorTheme == 1) ? Vec3(0.18f, 0.12f, 0.02f) :
                               (_retroColorTheme == 2) ? Vec3(0.02f, 0.18f, 0.05f) :
                               (_retroColorTheme == 3) ? Vec3(0.08f, 0.03f, 0.12f) :
                               Vec3(0.03f, 0.03f, 0.05f);
                for (float sy = 0; sy < h; sy += stepY) {
                    Renderer::drawRect(0, sy, w, 1.0f, scanCol);
                }
            }
            if (_retroVignette) {
                Vec3 vigCol = Vec3(0.02f, 0.03f, 0.05f);
                float vigSize = 16.0f;
                Renderer::drawRect(0, 0, w, vigSize, vigCol);
                Renderer::drawRect(0, h - vigSize, w, vigSize, vigCol);
                Renderer::drawRect(0, 0, vigSize, h, vigCol);
                Renderer::drawRect(w - vigSize, 0, vigSize, h, vigCol);
            }
            // CRT FX ACTIVE Badge
            Renderer::drawRect(16.0f, 16.0f, 130.0f, 22.0f, Vec3(0.10f, 0.18f, 0.12f));
            Renderer::drawRect(16.0f, 16.0f, 130.0f, 2.0f, Vec3(0.3f, 0.8f, 0.4f));
            LabFont::drawText(22.0f, 20.0f, "CRT FX ACTIVE", 1.25f, Vec3(0.4f, 1.0f, 0.5f), LabFontType::System);
        }

        Renderer::endUI();

        if (!_screenshotPath.empty()) {
            static int frameCount = 0;
            if (++frameCount >= 2) {
                saveFrameToBMP(_screenshotPath.c_str(), (int)w, (int)h);
                LabLog::info("Saved screenshot to: " + _screenshotPath);
                stop();
            }
        }
    }

    void renderCustomHUD(float screenW, float screenH) {
        if (!_hudProject) return;

        float projW = _hudProject->resolutionW > 0.0f ? _hudProject->resolutionW : 1920.0f;
        float projH = _hudProject->resolutionH > 0.0f ? _hudProject->resolutionH : 1080.0f;
        float scaleX = screenW / projW;
        float scaleY = screenH / projH;
        float fontScale = std::min(scaleX, scaleY);

        for (const auto& root : _hudProject->rootElements) {
            renderHUDElement(root, {0.0f, 0.0f}, scaleX, scaleY, fontScale, screenW, screenH, true);
        }
    }

    void renderHUDElement(const HUDElement& elem, const Vec2& parentPos,
                          float scaleX, float scaleY, float fontScale,
                          float screenW, float screenH, bool isRoot) {
        if (!elem.visible) return;

        Vec2 absPos;
        float elemW = elem.w * scaleX;
        float elemH = elem.h * scaleY;

        if (isRoot) {
            Vec2 anchorOffset = {0.0f, 0.0f};
            switch (elem.anchor) {
                case HUDAnchor::TopLeft:      anchorOffset = {0.0f, 0.0f}; break;
                case HUDAnchor::TopCenter:    anchorOffset = {screenW * 0.5f, 0.0f}; break;
                case HUDAnchor::TopRight:     anchorOffset = {screenW, 0.0f}; break;
                case HUDAnchor::CenterLeft:   anchorOffset = {0.0f, screenH * 0.5f}; break;
                case HUDAnchor::Center:       anchorOffset = {screenW * 0.5f, screenH * 0.5f}; break;
                case HUDAnchor::CenterRight:  anchorOffset = {screenW, screenH * 0.5f}; break;
                case HUDAnchor::BottomLeft:   anchorOffset = {0.0f, screenH}; break;
                case HUDAnchor::BottomCenter: anchorOffset = {screenW * 0.5f, screenH}; break;
                case HUDAnchor::BottomRight:  anchorOffset = {screenW, screenH}; break;
            }
            absPos = { anchorOffset.x + elem.x * scaleX, anchorOffset.y + elem.y * scaleY };
        } else {
            absPos = { parentPos.x + elem.x * scaleX, parentPos.y + elem.y * scaleY };
        }

        // Render Background & Borders
        if (elem.type == HUDElementType::Card || elem.type == HUDElementType::Panel || elem.type == HUDElementType::Rect) {
            Renderer::drawRect(absPos.x, absPos.y, elemW, elemH, elem.color);
            if (elem.borderWidth > 0.0f) {
                float bw = std::max(1.0f, elem.borderWidth * fontScale);
                Renderer::drawRect(absPos.x, absPos.y, elemW, bw, elem.borderColor);
                Renderer::drawRect(absPos.x, absPos.y + elemH - bw, elemW, bw, elem.borderColor);
                Renderer::drawRect(absPos.x, absPos.y, bw, elemH, elem.borderColor);
                Renderer::drawRect(absPos.x + elemW - bw, absPos.y, bw, elemH, elem.borderColor);
            }
        }

        // Resolve text & binding
        std::string textToDraw = elem.text;
        if (!elem.binding.empty()) {
            textToDraw = resolveHUDValue(elem.binding);
        } else if (ScriptEngine::getHUDText(elem.id) != "") {
            textToDraw = ScriptEngine::getHUDText(elem.id);
        }

        if (!textToDraw.empty() && (elem.type == HUDElementType::Label || elem.type == HUDElementType::Button || elem.type == HUDElementType::Card)) {
            float fSize = elem.fontSize * fontScale;
            LabFont::drawText(absPos.x, absPos.y, textToDraw, fSize, elem.color, (LabFontType)elem.fontType);
        }

        // Render nested children
        for (const auto& child : elem.children) {
            renderHUDElement(child, absPos, scaleX, scaleY, fontScale, screenW, screenH, false);
        }
    }

    std::string resolveHUDValue(const std::string& binding) {
        if (binding.empty()) return "";
        return ScriptEngine::getHUDText(binding);
    }

    void renderSplashScreen() {
        int fbW = 0, fbH = 0;
        getFramebufferSize(fbW, fbH);
        float w = (fbW > 0) ? (float)fbW : (float)getWidth();
        float h = (fbH > 0) ? (float)fbH : (float)getHeight();
        if (w <= 0.0f) w = 1280.0f;
        if (h <= 0.0f) h = 720.0f;

        glClearColor(0.025f, 0.035f, 0.048f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        Renderer::beginUI((int)w, (int)h);

        // Calculate smooth fade envelope (0.0s..0.4s fade-in, 2.1s..2.5s fade-out)
        float alpha = 1.0f;
        if (_splashTimer < 0.4f) {
            alpha = _splashTimer / 0.4f;
        } else if (_splashTimer > 2.1f) {
            alpha = std::clamp((_splashDuration - _splashTimer) / 0.4f, 0.0f, 1.0f);
        }
        alpha = std::clamp(alpha, 0.0f, 1.0f);

        // Dark cinematic background with subtle gradient vignette
        Vec3 bgDark = Vec3(0.025f, 0.035f, 0.048f) * alpha;
        Renderer::drawRect(0.0f, 0.0f, w, h, bgDark);

        float centerX = w * 0.5f;
        float centerY = h * 0.44f;

        // "p o w e r e d   b y" subtitle in authentic Source Engine styling
        Vec3 subCol = Vec3(0.60f, 0.65f, 0.72f) * alpha;
        std::string poweredBy = "p o w e r e d   b y";
        float subW = LabFont::getTextWidth(poweredBy, 1.45f, LabFontType::GeoSans);
        LabFont::drawText(centerX - subW * 0.5f, centerY - 95.0f, poweredBy, 1.45f, subCol, LabFontType::GeoSans);

        // Official LaB logo (Try LabLogoText.png, LabLogoText_White.png, LabLogo.png)
        Texture* logoTex = getTexture("LabLogoText.png");
        if (!logoTex) logoTex = getTexture("LabLogoText_White.png");
        if (!logoTex) logoTex = getTexture("LabLogo.png");

        if (logoTex) {
            float logoW = 340.0f;
            float logoH = 175.0f;
            Vec3 tint = Vec3(1.0f, 1.0f, 1.0f) * alpha;
            Renderer::drawTextureRect(centerX - logoW * 0.5f, centerY - 65.0f, logoW, logoH, *logoTex, tint);
        } else {
            // High-fidelity stylized vector emblem fallback
            float emblemW = 280.0f;
            float emblemH = 68.0f;
            float eX = centerX - emblemW * 0.5f;
            float eY = centerY - 34.0f;
            Renderer::drawRect(eX, eY, emblemW, emblemH, Vec3(0.08f, 0.11f, 0.16f) * alpha);
            Renderer::drawRect(eX, eY + emblemH - 3.0f, emblemW, 3.0f, Vec3(0.18f, 0.78f, 0.95f) * alpha);
            std::string logoTitle = "L a B   E N G I N E";
            float tw = LabFont::getTextWidth(logoTitle, 2.2f, LabFontType::GeoSans);
            LabFont::drawText(centerX - tw * 0.5f, eY + 20.0f, logoTitle, 2.2f, Vec3(0.95f, 0.98f, 1.0f) * alpha, LabFontType::GeoSans);
        }

        // Technology partner logos underneath: OpenGL, CMake, miniaudio, Lua 5.4
        float badgesY = centerY + 130.0f;
        struct TechPartner {
            std::string texName;
            std::string name;
            float logoW;
            float logoH;
            Vec3 accentCol;
        };
        std::vector<TechPartner> partners = {
            { "opengl_logo.png", "OpenGL", 108.0f, 45.0f, Vec3(0.33f, 0.55f, 0.72f) },
            { "cmake_logo.png", "CMake", 44.0f, 44.0f, Vec3(0.85f, 0.32f, 0.28f) },
            { "miniaudio_logo.png", "miniaudio", 120.0f, 18.0f, Vec3(0.95f, 0.78f, 0.25f) },
            { "lua_logo.png", "Lua 5.4", 42.0f, 42.0f, Vec3(0.25f, 0.65f, 0.95f) }
        };

        float cardW = 160.0f;
        float cardH = 72.0f;
        float gap = 20.0f;
        float totalBadgesW = partners.size() * cardW + (partners.size() - 1) * gap;
        float startBX = centerX - totalBadgesW * 0.5f;

        for (size_t i = 0; i < partners.size(); ++i) {
            float bx = startBX + i * (cardW + gap);
            Renderer::drawRect(bx, badgesY, cardW, cardH, Vec3(0.06f, 0.08f, 0.11f) * alpha);
            Renderer::drawRect(bx, badgesY + cardH - 2.0f, cardW, 2.0f, partners[i].accentCol * alpha);

            Texture* pTex = getTexture(partners[i].texName);
            if (pTex) {
                float imgX = bx + (cardW - partners[i].logoW) * 0.5f;
                float imgY = badgesY + 6.0f + (cardH - 22.0f - partners[i].logoH) * 0.5f;
                Renderer::drawTextureRect(imgX, imgY, partners[i].logoW, partners[i].logoH, *pTex, Vec3(1.0f, 1.0f, 1.0f) * alpha);
                float nameW = LabFont::getTextWidth(partners[i].name, 1.15f, LabFontType::System);
                LabFont::drawText(bx + (cardW - nameW) * 0.5f, badgesY + cardH - 18.0f, partners[i].name, 1.15f, Vec3(0.65f, 0.70f, 0.78f) * alpha, LabFontType::System);
            } else {
                float nameW = LabFont::getTextWidth(partners[i].name, 1.35f, LabFontType::System);
                LabFont::drawText(bx + (cardW - nameW) * 0.5f, badgesY + (cardH - 16.0f) * 0.5f, partners[i].name, 1.35f, partners[i].accentCol * alpha, LabFontType::System);
            }
        }

        // Skip prompt hint (subtle at bottom)
        std::string skipHint = "Press [ SPACE ], [ ENTER ] or [ ESC ] to skip";
        float hintW = LabFont::getTextWidth(skipHint, 1.15f, LabFontType::System);
        LabFont::drawText(centerX - hintW * 0.5f, h - 38.0f, skipHint, 1.15f, Vec3(0.35f, 0.40f, 0.48f) * alpha, LabFontType::System);

        Renderer::endUI();
    }

    Texture* getTexture(const std::string& path) {
        if (path.empty()) return nullptr;
        std::string fname = std::filesystem::path(path).filename().string();
        if (_textures.contains(fname)) return _textures[fname].get();

        auto& pm = ProjectManager::get();
        std::vector<std::filesystem::path> searchCandidates = {
            path,
            fname,
            std::filesystem::path("assets/textures") / fname,
            std::filesystem::path("assets/textures/logos") / fname,
            std::filesystem::path("logos") / fname,
            std::filesystem::path("../assets/textures") / fname,
            std::filesystem::path("../assets/textures/logos") / fname,
            std::filesystem::path("../logos") / fname,
            std::filesystem::path("../../assets/textures") / fname,
            std::filesystem::path("../../logos") / fname,
            pm.getActiveProjectAssetsPath() / "textures" / fname,
            pm.getActiveProjectAssetsPath() / "textures/logos" / fname,
            pm.getEngineAssetsPath() / "textures" / fname,
            pm.getEngineAssetsPath() / "textures/logos" / fname,
            pm.getEngineRoot() / "logos" / fname,
            pm.getEngineRoot() / "assets/textures" / fname,
            pm.getEngineRoot() / "assets/textures/logos" / fname
        };

        for (const auto& cand : searchCandidates) {
            std::error_code ec;
            if (std::filesystem::exists(cand, ec) && !std::filesystem::is_directory(cand, ec)) {
                try {
                    _textures[fname] = std::make_unique<Texture>(cand.string());
                    return _textures[fname].get();
                } catch (...) {}
            }
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
    std::string _dimension;
    std::string _scenePath;
    std::string _hudPath;
    std::string _rulesPath;
    std::string _screenshotPath;

    // Retro CRT & Post-Process FX
    bool _retroFX = false;
    bool _retroScanlines = true;
    bool _retroVignette = true;
    bool _retroAberration = true;
    int _retroPixelScale = 2;
    int _retroColorTheme = 0;
    float _retroScanlinePhase = 0.0f;

    // Startup Splash Screen
    bool _showSplash = true;
    bool _splashActive = true;
    float _splashTimer = 0.0f;
    float _splashDuration = 2.5f;

    Camera _camera;
    MapCamera* _activeCamera = nullptr;
    std::unique_ptr<LabMap> _map;
    std::unique_ptr<HUDProject> _hudProject;
    ScriptEngine _scriptEngine;

    Vec3 _playerPos;
    Vec3 _playerVel;
    bool _isGrounded;
    bool _isPaused;

    // Mission & Player State
    int _cardsCollected;
    int _totalCards;
    float _missionTimer;
    float _playerHealth;
    std::string _currentWeaponName;
    int _weaponClip;
    int _weaponReserve;
    std::string _missionStatus;
    std::string _missionMessage;
    std::vector<bool> _weaponPadCollected;

    std::unordered_map<std::string, std::unique_ptr<Texture>> _textures;
    std::unordered_map<std::string, std::unique_ptr<Mesh>> _meshes;
};

// Launch configuration
struct GameLaunchConfig {
    std::string title = "Lab Game";
    std::string scene = "assets/maps/default_box.labmap";
    std::string hud = "";
    std::string rules = "";
    std::string screenshot = "";
    int width = 1280;
    int height = 720;
    bool fullscreen = false;
    std::string dimension = "3D";
    bool retroFX = false;
    bool retroScanlines = true;
    bool retroVignette = true;
    bool retroAberration = true;
    int retroPixelScale = 2;
    int retroColorTheme = 0;
    bool showSplash = true;
};

static void parseConfigStream(std::istream& stream, GameLaunchConfig& cfg) {
    std::string line;
    while (std::getline(stream, line)) {
        size_t eq = line.find('=');
        if (eq != std::string::npos) {
            std::string k = line.substr(0, eq);
            std::string v = line.substr(eq + 1);
            k.erase(k.find_last_not_of(" \t\r\n") + 1);
            v.erase(0, v.find_first_not_of(" \t\r\n"));
            v.erase(v.find_last_not_of(" \t\r\n") + 1);
            if (k == "title") cfg.title = v;
            else if (k == "scene") cfg.scene = v;
            else if (k == "hud") cfg.hud = v;
            else if (k == "rules") cfg.rules = v;
            else if (k == "dimension") cfg.dimension = v;
            else if (k == "width") cfg.width = std::atoi(v.c_str());
            else if (k == "height") cfg.height = std::atoi(v.c_str());
            else if (k == "fullscreen") cfg.fullscreen = (v == "1" || v == "true");
            else if (k == "retro" || k == "retro_fx") cfg.retroFX = (v == "1" || v == "true");
            else if (k == "retro_scanlines") cfg.retroScanlines = (v == "1" || v == "true");
            else if (k == "retro_vignette") cfg.retroVignette = (v == "1" || v == "true");
            else if (k == "retro_aberration") cfg.retroAberration = (v == "1" || v == "true");
            else if (k == "retro_pixel_scale") cfg.retroPixelScale = std::clamp(std::atoi(v.c_str()), 1, 4);
            else if (k == "retro_theme") cfg.retroColorTheme = std::clamp(std::atoi(v.c_str()), 0, 3);
        }
    }
}

static GameLaunchConfig loadGameConfig(const std::string& specificConfig = "") {
    GameLaunchConfig cfg;

    if (!specificConfig.empty() && std::filesystem::exists(specificConfig)) {
        std::ifstream file(specificConfig);
        if (file.is_open()) {
            parseConfigStream(file, cfg);
            file.close();
            return cfg;
        }
    }

    // 0. Try project.labproject (JSON)
    std::ifstream projFile("project.labproject");
    if (projFile.is_open()) {
        std::string line;
        while (std::getline(projFile, line)) {
            size_t posDim = line.find("\"dimension\":");
            if (posDim != std::string::npos) {
                size_t q1 = line.find('\"', posDim + 12);
                size_t q2 = line.find('\"', q1 + 1);
                if (q1 != std::string::npos && q2 != std::string::npos) {
                    cfg.dimension = line.substr(q1 + 1, q2 - q1 - 1);
                }
            }
            size_t posW = line.find("\"defaultWidth\":");
            if (posW != std::string::npos) {
                cfg.width = std::atoi(line.c_str() + posW + 15);
            }
            size_t posH = line.find("\"defaultHeight\":");
            if (posH != std::string::npos) {
                cfg.height = std::atoi(line.c_str() + posH + 16);
            }
            size_t posFS = line.find("\"defaultFullscreen\":");
            if (posFS != std::string::npos) {
                cfg.fullscreen = (line.find("true", posFS) != std::string::npos);
            }
        }
        projFile.close();
    }

    // 1. Try game.config in current working directory
    std::ifstream cfgFile("game.config");
    if (cfgFile.is_open()) {
        parseConfigStream(cfgFile, cfg);
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

    std::string customConfigFile = "";
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--config" && i + 1 < argc) {
            customConfigFile = argv[++i];
        }
    }

    GameLaunchConfig config = loadGameConfig(customConfigFile);

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
        } else if (arg == "--dimension" && i + 1 < argc) {
            config.dimension = argv[++i];
        } else if (arg == "--fullscreen") {
            config.fullscreen = true;
        } else if (arg == "--hud" && i + 1 < argc) {
            config.hud = argv[++i];
        } else if (arg == "--rules" && i + 1 < argc) {
            config.rules = argv[++i];
        } else if (arg == "--screenshot" && i + 1 < argc) {
            config.screenshot = argv[++i];
        } else if (arg == "--retro") {
            config.retroFX = true;
        } else if (arg == "--nosplash" || arg == "-nosplash") {
            config.showSplash = false;
        }
    }

    // Auto-detect game config, HUD and rules if not specified directly
    if (config.hud.empty() || config.rules.empty()) {
        std::string sceneStem = std::filesystem::path(config.scene).stem().string();
        std::vector<std::string> candidateConfigs = {
            "games/labrunner_escape/game.config",
            "games/" + sceneStem + "/game.config"
        };
        if (std::filesystem::exists("games")) {
            for (const auto& entry : std::filesystem::recursive_directory_iterator("games")) {
                if (entry.path().filename() == "game.config") {
                    candidateConfigs.push_back(entry.path().string());
                }
            }
        }

        for (const auto& cpath : candidateConfigs) {
            if (!std::filesystem::exists(cpath)) continue;
            std::ifstream f(cpath);
            if (!f.is_open()) continue;
            GameLaunchConfig candidate;
            parseConfigStream(f, candidate);
            f.close();

            std::string candSceneStem = std::filesystem::path(candidate.scene).stem().string();
            if (candidate.scene == config.scene || candSceneStem == sceneStem ||
                (sceneStem.find("labrunner") != std::string::npos && candSceneStem.find("labrunner") != std::string::npos)) {
                if (config.hud.empty() && !candidate.hud.empty()) config.hud = candidate.hud;
                if (config.rules.empty() && !candidate.rules.empty()) config.rules = candidate.rules;
                if (config.title == "Lab Game" && !candidate.title.empty()) config.title = candidate.title;
                break;
            }
        }

        // Conventional asset paths
        if (config.hud.empty()) {
            std::string h1 = "assets/hud_projects/" + sceneStem + ".labhud";
            std::string h2 = "assets/hud_projects/labrunner_escape.labhud";
            if (std::filesystem::exists(h1)) config.hud = h1;
            else if (sceneStem.find("labrunner") != std::string::npos && std::filesystem::exists(h2)) config.hud = h2;
        }

        if (config.rules.empty()) {
            std::string r1 = "assets/scripts/" + sceneStem + "_rules.lua";
            std::string r2 = "assets/scripts/qa_labrunner_rules.lua";
            if (std::filesystem::exists(r1)) config.rules = r1;
            else if (sceneStem.find("labrunner") != std::string::npos && std::filesystem::exists(r2)) config.rules = r2;
        }
    }

    LabLog::info("Game: '" + config.title + "' | Dimension: '" + config.dimension + "' | Scene: '" + config.scene + "' | HUD: '" + config.hud + "' | Rules: '" + config.rules + "' | RetroFX: " + (config.retroFX ? "ON" : "OFF"));

    LabPlayerApp app(config.title, config.width, config.height, config.scene, config.hud, config.rules,
                     config.screenshot, config.dimension, config.retroFX, config.retroScanlines,
                     config.retroVignette, config.retroAberration, config.retroPixelScale, config.retroColorTheme,
                     config.showSplash);
    if (config.fullscreen) {
        app.setFullscreen(true);
    }
    app.run();

    return 0;
}
