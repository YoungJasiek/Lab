#include "Lab.h"
#include "LabHUDEditor.h"
#include <glad/gl.h>
#include <GLFW/glfw3.h>
#include <iostream>

using namespace Lab;

class LabHUDEditorApp : public Engine {
public:
    LabHUDEditorApp()
        : Engine("Lab 2D/HUD Editor - [Lab Engine 2026]", 1600, 900) {
    }

    ~LabHUDEditorApp() override {
        // Auto-save on exit if project is dirty
    }

    void onInit() override {
        LabLog::info("Initializing Lab 2D/HUD Editor...");
        Renderer::init();
        _editor.init();
        _editor.setWindow(getWindow());
        setCursorCaptured(false);
    }

    void onFixedUpdate(float fixedDelta) override {
        (void)fixedDelta;
    }

    void onUpdate(const Time& time) override {
        int winW = 1600, winH = 900;
        getWindowSize(winW, winH);
        int fbW = 1600, fbH = 900;
        getFramebufferSize(fbW, fbH);

        double mx = 0, my = 0;
        getCursorPos(mx, my);
        if (mx == 0.0 && my == 0.0) {
            mx = (double)Input::getMouseX();
            my = (double)Input::getMouseY();
        }

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
                _editor.handleKeyDown(k, ctrl, shift);
            }
            _lastKeys[k] = Input::keys[k];
        }

        _editor.update(time.delta, scaledMx, scaledMy, lmb, rmb, Input::scrollDelta);
        Input::scrollDelta = 0.0f;

        if (_editor.requestExit()) {
            stop();
        }
    }

    void onRender() override {
        int fbW = 0, fbH = 0;
        getFramebufferSize(fbW, fbH);
        int w = (fbW > 0) ? fbW : getWidth();
        int h = (fbH > 0) ? fbH : getHeight();

        glViewport(0, 0, w, h);
        glClearColor(0.10f, 0.11f, 0.13f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        _editor.render(w, h);
    }

private:
    LabHUDEditor2D _editor;
    bool _lastKeys[512] = { false };
};

int main() {
    try {
        LabHUDEditorApp app;
        app.run();
    } catch (const std::exception& ex) {
        std::cerr << "Fatal HUD Editor Exception: " << ex.what() << "\n";
        return 1;
    }
    return 0;
}
