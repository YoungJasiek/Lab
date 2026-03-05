#include "Lab.h"
#include <GL/freeglut.h>
#include <iostream>
#include <algorithm>

using namespace Lab;

class FrozenLife : public Engine {
public:
    FrozenLife() : Engine("Frozen-Life: Lab FPS", 1280, 720), _camera(75.0f, 16.0f / 9.0f, 0.01f, 1000.0f) {
        _skybox = nullptr;
        _baseplateTexture = nullptr;
        
        // Player stats
        _velocity = { 0, 0, 0 };
        _isGrounded = false;
        _isJumping = false;
        _bobTime = 0.0f;
        _muzzleFlashTime = 0.0f;
    }

    void onInit() override {
        std::cout << "Frozen-Life System Init..." << std::endl;
        Renderer::init();

        // Create icy baseplate texture
        unsigned char iceData[32 * 32 * 3];
        for (int y = 0; y < 32; y++) {
            for (int x = 0; x < 32; x++) {
                int noise = (rand() % 40) - 20;
                int idx = (y * 32 + x) * 3;
                iceData[idx] = 200 + noise;     // Light blue/white
                iceData[idx + 1] = 220 + noise;
                iceData[idx + 2] = 255 + noise;
            }
        }
        _baseplateTexture = new Texture(iceData, 32, 32, 3);

        // Player height and position
        _camera.setPosition({ 0.0f, 1.8f, 0.0f });

        // Fog for atmosphere
        glEnable(GL_FOG);
        GLfloat fogColor[4] = { 0.8f, 0.9f, 1.0f, 1.0f };
        glFogfv(GL_FOG_COLOR, fogColor);
        glFogf(GL_FOG_DENSITY, 0.03f);
        glFogi(GL_FOG_MODE, GL_EXP2);

        std::cout << "Controls: WASD + SPACE to Move/Jump, LMB to Shoot, ESC to Exit" << std::endl;
    }

    void handleMovement(const Time& time) {
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
            _bobTime += time.delta * (speed * 2.0f);
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
        _velocity.y -= 12.0f * time.delta;

        // Apply movement
        Vec3 pos = _camera.getPosition();
        pos += _velocity * time.delta;

        // Simple ground collision
        if (pos.y < 1.8f) {
            pos.y = 1.8f;
            _velocity.y = 0.0f;
            _isGrounded = true;
        }

        _camera.setPosition(pos);
        _camera.update(Input::mouseDelta);
    }

    void handleCombat(const Time& time) {
        if (Input::isMouseButtonPressed(0) && _muzzleFlashTime <= 0.0f) {
            _muzzleFlashTime = 0.1f;
            // Add recoil effect?
        }

        if (_muzzleFlashTime > 0.0f) {
            _muzzleFlashTime -= time.delta;
        }
    }

    void onUpdate(const Time& time) override {
        handleMovement(time);
        handleCombat(time);
    }

    void drawWeapon() {
        // ViewModel space
        glPushMatrix();
        glLoadIdentity();
        
        // Sway & Bobbing
        float swayX = Input::mouseDelta.x * -0.001f;
        float swayY = Input::mouseDelta.y * 0.001f;
        float bobX = std::cos(_bobTime * 0.5f) * 0.02f;
        float bobY = std::abs(std::sin(_bobTime)) * 0.02f;

        glTranslatef(0.4f + swayX + bobX, -0.4f + swayY - bobY, -0.6f);
        glRotatef(-5.0f, 0, 1, 0); // Slight angle

        // Gun barrel
        Renderer::drawCube({ 0, 0, 0 }, { 0.1f, 0.15f, 0.5f }, { 0.15f, 0.15f, 0.18f });
        // Gun handle
        Renderer::drawCube({ 0, -0.1f, 0.1f }, { 0.08f, 0.25f, 0.1f }, { 0.1f, 0.1f, 0.1f });

        // Muzzle Flash
        if (_muzzleFlashTime > 0.0f) {
            Renderer::drawCube({ 0, 0, -0.3f }, { 0.2f, 0.2f, 0.2f }, { 1.0f, 0.8f, 0.2f });
        }

        glPopMatrix();
    }

    void drawUI() {
        int w = 1280, h = 720; // Default or get from engine
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
        Renderer::drawBaseplate(250.0f, _baseplateTexture);

        // Draw some "frozen" crystals/structures
        for(int i = 0; i < 10; ++i) {
            float angle = i * (3.14f * 2.0f / 10.0f);
            float dist = 20.0f + (i % 3) * 5.0f;
            Renderer::drawCube({std::cos(angle)*dist, 5.0f, std::sin(angle)*dist}, {2, 10, 2}, {0.7f, 0.85f, 1.0f});
        }

        // Weapons and UI
        drawWeapon();
        drawUI();

        Renderer::endFrame();
    }

    void onShutdown() override {
        delete _baseplateTexture;
        delete _skybox;
    }

private:
    Camera _camera;
    Skybox* _skybox;
    Texture* _baseplateTexture;

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
