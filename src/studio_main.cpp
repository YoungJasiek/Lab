#include "Lab.h"
#include "LabStudio.h"
#include <glad/gl.h>
#include <GLFW/glfw3.h>
#include <iostream>

using namespace Lab;

class LabStudioApp : public Engine {
public:
    LabStudioApp()
        : Engine("Lab Character & Weapon Studio - [Lab Engine 2026]", 1600, 900) {
    }

    ~LabStudioApp() override {
        _studio.saveConfig();
    }

    void onInit() override {
        LabLog::info("Initializing Standalone Lab Character Studio...");
        Renderer::init();
        _studio.init();
        _studio.setWindow(getWindow());
        glfwSetInputMode(getWindow(), GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    }

    void onFixedUpdate(float fixedDelta) override {
        (void)fixedDelta;
    }

    void onUpdate(const Time& time) override {
        int winW = 0, winH = 0;
        glfwGetWindowSize(getWindow(), &winW, &winH);
        int fbW = 0, fbH = 0;
        glfwGetFramebufferSize(getWindow(), &fbW, &fbH);

        double mx = 0, my = 0;
        glfwGetCursorPos(getWindow(), &mx, &my);

        float scaleX = (winW > 0 && fbW > 0) ? (static_cast<float>(fbW) / static_cast<float>(winW)) : 1.0f;
        float scaleY = (winH > 0 && fbH > 0) ? (static_cast<float>(fbH) / static_cast<float>(winH)) : 1.0f;

        float scaledMx = static_cast<float>(mx) * scaleX;
        float scaledMy = static_cast<float>(my) * scaleY;

        bool lmb = Input::isMouseButtonPressed(0);
        bool rmb = Input::isMouseButtonPressed(1);

        // Keyboard shortcuts
        bool ctrl = Input::isKeyPressed(GLFW_KEY_LEFT_CONTROL) || Input::isKeyPressed(GLFW_KEY_RIGHT_CONTROL);
        bool shift = Input::isKeyPressed(GLFW_KEY_LEFT_SHIFT) || Input::isKeyPressed(GLFW_KEY_RIGHT_SHIFT);

        for (int k = 0; k < 512; ++k) {
            if (Input::keys[k] && !_lastKeys[k]) {
                _studio.handleKeyDown(k, ctrl, shift);
            }
            _lastKeys[k] = Input::keys[k];
        }

        _scaledMx = scaledMx;
        _scaledMy = scaledMy;
        _studio.update(time.delta, scaledMx, scaledMy, lmb, rmb, Input::scrollDelta);
        Input::scrollDelta = 0.0f;

        if (_studio.requestExit()) {
            _studio.saveConfig();
            stop();
        }
    }

    void onRender() override {
        int fbW = 0, fbH = 0;
        glfwGetFramebufferSize(getWindow(), &fbW, &fbH);
        int w = (fbW > 0) ? fbW : getWidth();
        int h = (fbH > 0) ? fbH : getHeight();

        // Clear background to dark slate
        glViewport(0, 0, w, h);
        glClearColor(0.12f, 0.13f, 0.15f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        _studio.render(w, h);
    }

private:
    CharacterStudio _studio;
    float _scaledMx = 0.0f;
    float _scaledMy = 0.0f;
    bool _lastKeys[512] = { false };
};

int main() {
    try {
        LabStudioApp app;
        app.run();
    } catch (const std::exception& ex) {
        std::cerr << "Fatal Studio Exception: " << ex.what() << "\n";
        return 1;
    }
    return 0;
}
