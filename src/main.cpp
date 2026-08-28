#include "Lab.h"
#include <glad/gl.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <algorithm>
#include <memory>
#include <vector>

using namespace Lab;

struct CrystalEntity {
    Vec3 position;
    Vec3 size;
    Vec3 color;
};

// Main Game Class
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
        std::cout << "Frozen-Life System Init (Modern C++20 / Data-Oriented)..." << std::endl;
        Renderer::init();

        // Load Test.bmp texture
        _testTexture = std::make_unique<Texture>("Test.bmp");

        // Load an STL model if it exists
        Mesh* rawMesh = Mesh::loadSTL("Model.stl");
        if (rawMesh) {
            _stlModel.reset(rawMesh);
            std::cout << "Loaded Model.stl successfully!" << std::endl;
        }

        // Create icy baseplate texture
        unsigned char iceData[32 * 32 * 3];
        for (int y = 0; y < 32; y++) {
            for (int x = 0; x < 32; x++) {
                int noise = (rand() % 40) - 20;
                int idx = (y * 32 + x) * 3;
                iceData[idx] = static_cast<unsigned char>(200 + noise);
                iceData[idx + 1] = static_cast<unsigned char>(220 + noise);
                iceData[idx + 2] = static_cast<unsigned char>(std::min(255, 255 + noise));
            }
        }
        _baseplateTexture = std::make_unique<Texture>(iceData, 32, 32, 3);

        // Player height and position
        _camera.setPosition({ 0.0f, 1.8f, 0.0f });

        // Set atmospheric lighting (Cold Frost / Half-Life 2 style)
        Renderer::setSunLight(
            { -0.4f, -0.8f, -0.4f },     // Sun Direction
            { 0.9f, 0.95f, 1.0f },      // Cool White Sun
            { 0.2f, 0.25f, 0.35f }      // Frost Blue Ambient
        );

        // Pre-populate crystals (Data-Oriented approach)
        _crystals.reserve(10);
        for (int i = 0; i < 10; ++i) {
            float angle = i * (3.14159f * 2.0f / 10.0f);
            float dist = 20.0f + (i % 3) * 5.0f;
            _crystals.push_back({
                { std::cos(angle) * dist, 5.0f, std::sin(angle) * dist },
                { 2.0f, 10.0f, 2.0f },
                { 0.7f, 0.85f, 1.0f }
            });
        }

        std::cout << "Controls: WASD + SPACE to Move/Jump, LMB to Shoot, ESC to Exit" << std::endl;
    }

    void onFixedUpdate(float fixedDelta) override {
        // Physics tick rate (64 ticks per second)
        float speed = (Input::isKeyPressed(' ') && !_isJumping) ? 8.0f : 4.5f;
        Vec3 inputDir = { 0, 0, 0 };

        Vec3 front = _camera.getFront();
        front.y = 0;
        front = front.normalized();

        Vec3 right = _camera.getRight();
        right.y = 0;
        right = right.normalized();

        if (Input::isKeyPressed('w')) inputDir += front;
        if (Input::isKeyPressed('s')) inputDir -= front;
        if (Input::isKeyPressed('a')) inputDir -= right;
        if (Input::isKeyPressed('d')) inputDir += right;

        if (inputDir.lengthSq() > 0) {
            inputDir = inputDir.normalized();
            _velocity.x = inputDir.x * speed;
            _velocity.z = inputDir.z * speed;
            _bobTime += fixedDelta * (speed * 2.0f);
        } else {
            // Friction/Deceleration
            _velocity.x *= 0.85f;
            _velocity.z *= 0.85f;
        }

        // Jump physics
        if (Input::isKeyPressed(' ') && _isGrounded) {
            _velocity.y = 4.5f;
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
    }

    void onUpdate(const Time& time) override {
        // Camera orientation update
        _camera.update(Input::mouseDelta);

        // Combat cooldown
        if (Input::isMouseButtonPressed(0) && _muzzleFlashTime <= 0.0f) {
            _muzzleFlashTime = 0.1f;
        }

        if (_muzzleFlashTime > 0.0f) {
            _muzzleFlashTime -= time.delta;
        }
    }

    void drawWeapon() {
        Renderer::beginViewModel();
        
        // Sway & Bobbing
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

    void drawUI() {
        int w = 1280, h = 720;
        Renderer::beginUI(w, h);

        // Simple crosshair
        float size = 4.0f;
        float centerX = w / 2.0f;
        float centerY = h / 2.0f;
        Renderer::drawRect(centerX - size, centerY - 1.0f, size * 2, 2.0f, { 1, 1, 1 });
        Renderer::drawRect(centerX - 1.0f, centerY - size, 2.0f, size * 2, { 1, 1, 1 });

        Renderer::endUI();
    }

    void onRender() override {
        Renderer::beginFrame(_camera);

        // Draw Environment
        Renderer::drawBaseplate(250.0f, _baseplateTexture.get());

        // Render the STL Model directly in front
        if (_stlModel) {
            // Draw a small red cube at the base as a marker
            Renderer::drawCube({ 0, 0.5f, -5 }, { 0.2f, 1.0f, 0.2f }, { 1.0f, 0, 0 });
            // Position: X=0, Y=1.0 (above floor), Z=-5
            Renderer::drawMesh(*_stlModel, { 0, 1.0f, -5 }, { 0, 0, 0 }, { 1, 1, 1 });
        }

        // Draw the Test Quad (further away)
        if (_testTexture) {
            Renderer::drawCube({ 0, 4.0f, -15.0f }, { 0, 0, 0 }, { 4.0f, 4.0f, 0.1f }, { 1.0f, 1.0f, 1.0f }, _testTexture.get());
        }

        // Draw "frozen" crystal structures
        for (const auto& crystal : _crystals) {
            Renderer::drawCube(crystal.position, crystal.size, crystal.color);
        }

        // Weapons and UI
        drawWeapon();
        drawUI();

        Renderer::endFrame();
    }

    void onShutdown() override {
        Renderer::shutdown();
    }

private:
    Camera _camera;
    std::unique_ptr<Skybox> _skybox;
    std::unique_ptr<Texture> _baseplateTexture;
    std::unique_ptr<Texture> _testTexture;
    std::unique_ptr<Mesh> _stlModel;
    std::vector<CrystalEntity> _crystals;

    // Movement state
    Vec3 _velocity;
    bool _isGrounded;
    bool _isJumping;
    float _bobTime;

    // Combat state
    float _muzzleFlashTime;
};

int main() {
    FrozenLife game;
    game.run();
    return 0;
}
