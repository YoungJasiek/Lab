#include "Lab.h"
#include "LabFont.h"
#include <glad/gl.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <vector>
#include <memory>
#include <string>
#include <sstream>
#include <iomanip>
#include <filesystem>
#include <unordered_map>

using namespace Lab;

class LabEngineShowcase : public Engine {
public:
    LabEngineShowcase()
        : Engine("Lab Engine SDK v0.1.0 - Modular 3D Showcase [C++20 & OpenGL 4.5+ DSA]", 1600, 900),
          _camera(70.0f, 16.0f / 9.0f, 0.01f, 1000.0f),
          _sunDir(-0.4f, -0.8f, -0.45f),
          _sunColor(1.0f, 0.95f, 0.9f),
          _ambientColor(0.2f, 0.22f, 0.28f) {
        _camera.setPosition(Vec3(0.0f, 4.0f, 12.0f));
    }

    ~LabEngineShowcase() override = default;

    void onInit() override {
        LabLog::info("===============================================================");
        LabLog::info("  LAB GAME ENGINE SDK - MODULAR SHOWCASE INITIALIZATION        ");
        LabLog::info("===============================================================");

        // 1. Initialize Renderer (OpenGL 4.5+ Direct State Access)
        Renderer::init();
        Renderer::setSunLight(_sunDir.normalized(), _sunColor, _ambientColor);
        Renderer::setFog(true, Vec3(0.06f, 0.08f, 0.12f), 20.0f, 120.0f);

        // 2. Initialize Audio Engine (miniaudio spatial audio + procedural synthesizer)
        AudioEngine::init(true);

        // 3. Initialize Physics Subsystem
        _physicsWorld.init();

        // 4. Initialize Particle Subsystem
        _particleSystem.init();

        // 5. Initialize Lua 5.4 Scripting Subsystem
        if (_scriptEngine.init("assets/scripts/game_mechanics.lua")) {
            LabLog::info("[LabScript] Lua 5.4.6 runtime initialized successfully.");
            _scriptEngine.executeString("print('[Lua] Lab Engine SDK Lua Runtime Active!')");
        }

        // Preload core textures
        loadTextures();

        // Spawn initial physics demonstration props
        spawnInitialPhysicsScene();

        LabLog::info("[LabEngine] Initialization complete. Entering main loop.");
    }

    void onUpdate(const Time& time) override {
        _fpsTimer += time.delta;
        _frameCount++;
        if (_fpsTimer >= 1.0f) {
            _currentFps = (float)_frameCount / _fpsTimer;
            _frameCount = 0;
            _fpsTimer = 0.0f;
        }

        handleInput(time.delta);

        // Update Audio listener position and orientation
        AudioEngine::setListener(_camera.getPosition(), _camera.getFront(), _camera.getUp());

        // Update Physics simulation
        std::vector<CollisionBox> obstacles;
        // Ground barrier
        obstacles.push_back({ Vec3(-50.0f, -1.0f, -50.0f), Vec3(50.0f, 0.0f, 50.0f) });
        _physicsWorld.update(time.delta, obstacles);

        // Check explosive barrel detonation events
        for (const auto& expl : _physicsWorld.pendingExplosions) {
            _particleSystem.spawnExplosion(expl.position, expl.radius, Vec3(1.0f, 0.45f, 0.1f));
            AudioEngine::playSound3D(SoundID::RPGExplosion, expl.position, 1.0f, 1.0f, 1.5f, 40.0f);
        }
        _physicsWorld.pendingExplosions.clear();

        // Update Particle system
        _particleSystem.update(time.delta);

        // Update Flashlight if enabled
        if (_flashlightOn) {
            Vec3 flPos = _camera.getPosition() + _camera.getRight() * 0.2f - _camera.getUp() * 0.15f;
            Renderer::setFlashlight(flPos, _camera.getFront(), Vec3(0.95f, 0.98f, 1.0f), 0.94f, 0.88f, 35.0f, 1.25f);
        } else {
            Renderer::disableFlashlight();
        }
    }

    void onRender() override {
        // 1. Begin 3D Scene
        Renderer::beginFrame(_camera);

        // Draw Ground Grid / Baseplate
        Texture* floorTex = getTexture("assets/textures/floor_tiles.bmp");
        if (!floorTex) floorTex = getTexture("floor_tiles.bmp");
        Renderer::drawBaseplate(80.0f, floorTex);

        // Draw Architectural Pillars and Reference Geometry
        Texture* wallTex = getTexture("assets/textures/concrete_wall.bmp");
        if (!wallTex) wallTex = getTexture("concrete_wall.bmp");
        Texture* hazardTex = getTexture("assets/textures/hazard_stripes.bmp");
        if (!hazardTex) hazardTex = getTexture("hazard_stripes.bmp");

        // Reference Pillars
        for (float x = -20.0f; x <= 20.0f; x += 10.0f) {
            for (float z = -20.0f; z <= 20.0f; z += 20.0f) {
                if (std::abs(x) < 1.0f && std::abs(z) < 1.0f) continue;
                Renderer::drawCube(Vec3(x, 2.5f, z), Vec3(1.2f, 5.0f, 1.2f), Vec3(0.9f, 0.9f, 0.9f), wallTex, true, Vec2(0.5f, 2.0f), 1);
                Renderer::drawCube(Vec3(x, 5.2f, z), Vec3(1.4f, 0.4f, 1.4f), Vec3(1.0f, 0.9f, 0.4f), hazardTex, true, Vec2(1.0f, 0.2f), 1);
            }
        }

        // Render Physics Rigid Bodies (Crates, Barrels, Debris)
        _physicsWorld.render();

        // Render Active Dynamic Particles
        _particleSystem.render(_camera);

        // End 3D Scene
        Renderer::endFrame();

        // 2. Render 2D UI & Engine Telemetry HUD
        renderHUD();
    }

    void onShutdown() override {
        _particleSystem.shutdown();
        AudioEngine::shutdown();
        Renderer::shutdown();
        LabLog::info("[LabEngine] Engine shutdown clean.");
    }

private:
    Camera _camera;
    Vec3 _sunDir;
    Vec3 _sunColor;
    Vec3 _ambientColor;

    PhysicsWorld _physicsWorld;
    ParticleSystem _particleSystem;
    ScriptEngine _scriptEngine;

    std::unordered_map<std::string, std::unique_ptr<Texture>> _textures;
    bool _flashlightOn = false;
    bool _cursorLocked = true;
    float _moveSpeed = 8.0f;

    // Telemetry
    float _currentFps = 60.0f;
    float _fpsTimer = 0.0f;
    int _frameCount = 0;
    std::string _lastLuaResult = "Ready (Press [T] to execute)";

    void loadTextures() {
        std::vector<std::string> paths = {
            "assets/textures/floor_tiles.bmp",
            "assets/textures/concrete_wall.bmp",
            "assets/textures/hazard_stripes.bmp",
            "assets/textures/metal_hull.bmp",
            "assets/textures/snow_frost.bmp",
            "floor_tiles.bmp",
            "concrete_wall.bmp",
            "hazard_stripes.bmp"
        };
        for (const auto& p : paths) {
            getTexture(p);
        }
    }

    Texture* getTexture(const std::string& path) {
        auto it = _textures.find(path);
        if (it != _textures.end()) return it->second.get();
        if (std::filesystem::exists(path)) {
            auto tex = std::make_unique<Texture>(path);
            if (tex && tex->getId() != 0) {
                Texture* ptr = tex.get();
                _textures[path] = std::move(tex);
                return ptr;
            }
        }
        return nullptr;
    }

    void spawnInitialPhysicsScene() {
        // Spawn pyramid of wooden crates
        for (int y = 0; y < 3; ++y) {
            for (int x = -1; x <= 1 - y; ++x) {
                float px = (float)x * 1.1f + (float)y * 0.55f;
                float py = 0.5f + (float)y * 1.05f;
                _physicsWorld.spawnCrate(Vec3(px, py, -2.0f));
            }
        }

        // Spawn red explosive fuel barrels
        _physicsWorld.spawnExplosiveBarrel(Vec3(-3.5f, 0.6f, -1.5f));
        _physicsWorld.spawnExplosiveBarrel(Vec3(3.5f, 0.6f, -1.5f));
    }

    void handleInput(float dt) {
        // Toggle Mouse Cursor
        static bool escPrev = false;
        bool escNow = Input::isKeyPressed(GLFW_KEY_ESCAPE);
        if (escNow && !escPrev) {
            _cursorLocked = !_cursorLocked;
            glfwSetInputMode(getWindow(), GLFW_CURSOR, _cursorLocked ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
        }
        escPrev = escNow;

        // Camera Mouse Look
        if (_cursorLocked || Input::isMouseButtonPressed(GLFW_MOUSE_BUTTON_RIGHT)) {
            Vec2 mDelta = Input::mouseDelta;
            _camera.update(mDelta);
        }

        // Camera Movement
        float speed = _moveSpeed;
        if (Input::isKeyPressed(GLFW_KEY_LEFT_SHIFT)) speed *= 2.5f;

        Vec3 moveDir(0.0f, 0.0f, 0.0f);
        if (Input::isKeyPressed('W')) moveDir = moveDir + _camera.getFront();
        if (Input::isKeyPressed('S')) moveDir = moveDir - _camera.getFront();
        if (Input::isKeyPressed('A')) moveDir = moveDir - _camera.getRight();
        if (Input::isKeyPressed('D')) moveDir = moveDir + _camera.getRight();
        if (Input::isKeyPressed(GLFW_KEY_SPACE)) moveDir = moveDir + Vec3(0.0f, 1.0f, 0.0f);
        if (Input::isKeyPressed(GLFW_KEY_LEFT_CONTROL)) moveDir = moveDir - Vec3(0.0f, 1.0f, 0.0f);

        if (moveDir.lengthSq() > 0.001f) {
            _camera.setPosition(_camera.getPosition() + moveDir.normalized() * (speed * dt));
        }

        // Spawn Crate in front of camera [G]
        static bool gPrev = false;
        bool gNow = Input::isKeyPressed('G');
        if (gNow && !gPrev) {
            Vec3 spawnPos = _camera.getPosition() + _camera.getFront() * 3.0f;
            auto* b = _physicsWorld.spawnCrate(spawnPos);
            if (b) {
                b->applyCentralImpulse(_camera.getFront() * 120.0f);
                b->applyTorqueImpulse(Vec3(15.0f, 25.0f, 5.0f));
            }
            AudioEngine::playSound(SoundID::PickupAmmo);
        }
        gPrev = gNow;

        // Spawn Explosive Barrel [H]
        static bool hPrev = false;
        bool hNow = Input::isKeyPressed('H');
        if (hNow && !hPrev) {
            Vec3 spawnPos = _camera.getPosition() + _camera.getFront() * 3.0f;
            auto* b = _physicsWorld.spawnExplosiveBarrel(spawnPos);
            if (b) {
                b->applyCentralImpulse(_camera.getFront() * 150.0f);
            }
            AudioEngine::playSound(SoundID::WeaponSpawn);
        }
        hPrev = hNow;

        // Toggle Flashlight [F]
        static bool fPrev = false;
        bool fNow = Input::isKeyPressed('F');
        if (fNow && !fPrev) {
            _flashlightOn = !_flashlightOn;
            AudioEngine::playSound(SoundID::FlashlightToggle);
        }
        fPrev = fNow;

        // Play Procedural Audio Chime [B]
        static bool bPrev = false;
        bool bNow = Input::isKeyPressed('B');
        if (bNow && !bPrev) {
            AudioEngine::playSound3D(SoundID::TerminalBeep, _camera.getPosition() + _camera.getFront() * 2.0f);
        }
        bPrev = bNow;

        // Spawn Particle Spark Spray [P]
        static bool pPrev = false;
        bool pNow = Input::isKeyPressed('P');
        if (pNow && !pPrev) {
            Vec3 pPos = _camera.getPosition() + _camera.getFront() * 4.0f;
            _particleSystem.spawnImpact(pPos, Vec3(0, 1, 0), SurfaceType::Metal);
            AudioEngine::playSound(SoundID::PipeHit);
        }
        pPrev = pNow;

        // Live Lua 5.4 Execution [T]
        static bool tPrev = false;
        bool tNow = Input::isKeyPressed('T');
        if (tNow && !tPrev) {
            _scriptEngine.executeString("print('[Lua 5.4 Live]: Tick evaluated successfully!')");
            _lastLuaResult = "Executed Lua 5.4 snippet (check console)";
            AudioEngine::playSound(SoundID::AccessGranted);
        }
        tPrev = tNow;

        // Reset Scene [R]
        static bool rPrev = false;
        bool rNow = Input::isKeyPressed('R');
        if (rNow && !rPrev) {
            _physicsWorld.clear();
            spawnInitialPhysicsScene();
            AudioEngine::playSound(SoundID::Reload);
        }
        rPrev = rNow;
    }

    void renderHUD() {
        int w = getWidth();
        int h = getHeight();
        Renderer::beginUI(w, h);

        // Top Header Banner
        Renderer::drawRect(10.0f, 10.0f, (float)w - 20.0f, 54.0f, Vec3(0.04f, 0.06f, 0.09f));
        Renderer::drawRect(10.0f, 62.0f, (float)w - 20.0f, 2.0f, Vec3(0.0f, 0.75f, 0.95f));

        LabFont::drawText(24.0f, 18.0f, "LAB ENGINE SDK v0.1.0  |  MODULAR C++20 3D GAME ENGINE", 1.1f, Vec3(1.0f, 1.0f, 1.0f), LabFontType::System);
        LabFont::drawText(24.0f, 38.0f, "OpenGL 4.5+ Direct State Access (DSA) - Fixed-Timestep 64Hz Physics - Lua 5.4 Runtime", 0.8f, Vec3(0.0f, 0.85f, 1.0f), LabFontType::System);

        // Subsystems Badge Bar
        float badgeY = 72.0f;
        std::vector<std::string> modules = {
            "LabCore", "LabRender", "LabAudio", "LabPhysics", "LabAnimation",
            "LabWorld", "LabNetwork", "LabAI", "LabScript", "LabStudioCore"
        };
        float badgeX = 14.0f;
        for (const auto& mod : modules) {
            float bw = (float)mod.length() * 8.5f + 14.0f;
            Renderer::drawRect(badgeX, badgeY, bw, 22.0f, Vec3(0.1f, 0.2f, 0.3f));
            LabFont::drawText(badgeX + 6.0f, badgeY + 4.0f, mod, 0.75f, Vec3(0.4f, 0.9f, 1.0f), LabFontType::System);
            badgeX += bw + 8.0f;
        }

        // Left Telemetry Card
        float cardY = 106.0f;
        Renderer::drawRect(14.0f, cardY, 320.0f, 210.0f, Vec3(0.05f, 0.07f, 0.10f));
        Renderer::drawRect(14.0f, cardY, 320.0f, 2.0f, Vec3(0.0f, 0.65f, 0.85f));

        std::ostringstream ssFps, ssRigid, ssPart, ssPos;
        ssFps << "Framerate: " << std::fixed << std::setprecision(1) << _currentFps << " FPS (" << (1000.0f / std::max(1.0f, _currentFps)) << " ms)";
        ssRigid << "Active Rigid Bodies: " << _physicsWorld.bodies.size();
        ssPart << "Active Dynamic Particles: " << _particleSystem.getActiveCount();
        ssPos << "Camera Pos: (" << std::setprecision(1) << _camera.getPosition().x << ", " << _camera.getPosition().y << ", " << _camera.getPosition().z << ")";

        LabFont::drawText(24.0f, cardY + 10.0f, "ENGINE TELEMETRY", 0.9f, Vec3(1.0f, 0.85f, 0.2f), LabFontType::System);
        LabFont::drawText(24.0f, cardY + 34.0f, ssFps.str(), 0.8f, Vec3(0.85f, 0.85f, 0.85f), LabFontType::System);
        LabFont::drawText(24.0f, cardY + 54.0f, ssRigid.str(), 0.8f, Vec3(0.85f, 0.85f, 0.85f), LabFontType::System);
        LabFont::drawText(24.0f, cardY + 74.0f, ssPart.str(), 0.8f, Vec3(0.85f, 0.85f, 0.85f), LabFontType::System);
        LabFont::drawText(24.0f, cardY + 94.0f, ssPos.str(), 0.8f, Vec3(0.85f, 0.85f, 0.85f), LabFontType::System);
        LabFont::drawText(24.0f, cardY + 114.0f, "Lua State: " + _lastLuaResult, 0.75f, Vec3(0.3f, 1.0f, 0.5f), LabFontType::System);
        LabFont::drawText(24.0f, cardY + 134.0f, "Renderer: DSA Persistent Buffers", 0.75f, Vec3(0.7f, 0.7f, 0.7f), LabFontType::System);
        LabFont::drawText(24.0f, cardY + 154.0f, "Sound Engine: miniaudio v0.11.25", 0.75f, Vec3(0.7f, 0.7f, 0.7f), LabFontType::System);
        LabFont::drawText(24.0f, cardY + 174.0f, "Flashlight: " + std::string(_flashlightOn ? "ENABLED" : "DISABLED"), 0.75f, _flashlightOn ? Vec3(1.0f, 1.0f, 0.3f) : Vec3(0.5f, 0.5f, 0.5f), LabFontType::System);

        // Bottom Controls Cheat-Sheet Card
        float ctrlY = (float)h - 90.0f;
        Renderer::drawRect(14.0f, ctrlY, (float)w - 28.0f, 76.0f, Vec3(0.04f, 0.06f, 0.09f));
        Renderer::drawRect(14.0f, ctrlY, (float)w - 28.0f, 2.0f, Vec3(0.0f, 0.65f, 0.85f));

        LabFont::drawText(24.0f, ctrlY + 8.0f, "CONTROLS & INTERACTIVE ENGINE TESTING", 0.85f, Vec3(1.0f, 0.85f, 0.2f), LabFontType::System);
        LabFont::drawText(24.0f, ctrlY + 28.0f, "[WASD / Space / Ctrl] Freecam Flight    [Shift] Boost Speed    [ESC] Toggle Mouse Cursor    [F] Toggle Spotlight", 0.8f, Vec3(0.85f, 0.85f, 0.85f), LabFontType::System);
        LabFont::drawText(24.0f, ctrlY + 48.0f, "[G] Spawn Physics Crate    [H] Spawn Explosive Barrel    [P] Spawn Spark Emitter    [B] 3D Audio Chime    [T] Run Lua 5.4    [R] Reset Scene", 0.8f, Vec3(0.3f, 0.85f, 1.0f), LabFontType::System);

        // Crosshair
        float cx = (float)w * 0.5f;
        float cy = (float)h * 0.5f;
        Renderer::drawRect(cx - 5.0f, cy - 1.0f, 10.0f, 2.0f, Vec3(0.0f, 1.0f, 0.85f));
        Renderer::drawRect(cx - 1.0f, cy - 5.0f, 2.0f, 10.0f, Vec3(0.0f, 1.0f, 0.85f));

        Renderer::endUI();
    }
};

int main(int /*argc*/, char* /*argv*/[]) {
    try {
        LabEngineShowcase showcase;
        showcase.run();
    } catch (const std::exception& e) {
        std::cerr << "[Fatal Error] " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
