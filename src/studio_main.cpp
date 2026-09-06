#include "Lab.h"
#include "LabStudio.h"
#include <glad/gl.h>
#include <GLFW/glfw3.h>
#include <iostream>

using namespace Lab;

class LabStudioApp : public Engine {
public:
    LabStudioApp()
        : Engine("Valve Hammer Character & Weapon Studio - [Lab Engine 2026]", 1600, 900) {
    }

    void onInit() override {
        LabLog::info("Initializing Standalone Valve Hammer Character Studio...");
        Renderer::init();
        _studio.init();
        glfwSetInputMode(getWindow(), GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    }

    void onFixedUpdate(float fixedDelta) override {
        (void)fixedDelta;
    }

    void onUpdate(const Time& time) override {
        double mx = 0, my = 0;
        glfwGetCursorPos(getWindow(), &mx, &my);
        bool lmb = (glfwGetMouseButton(getWindow(), GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS);
        bool rmb = (glfwGetMouseButton(getWindow(), GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS);

        _studio.update(time.delta, static_cast<float>(mx), static_cast<float>(my), lmb, rmb, Input::scrollDelta);
        Input::scrollDelta = 0.0f;
    }

    void onRender() override {
        int fbW = 0, fbH = 0;
        glfwGetFramebufferSize(getWindow(), &fbW, &fbH);
        int w = (fbW > 0) ? fbW : getWidth();
        int h = (fbH > 0) ? fbH : getHeight();

        // Clear background to Valve Hammer dark slate
        glViewport(0, 0, w, h);
        glClearColor(0.12f, 0.13f, 0.15f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        _studio.render(w, h);
    }

private:
    CharacterStudio _studio;
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
