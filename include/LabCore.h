#pragma once
#include <vector>
#include <string>
#include <memory>
#include "LabMath.h"

struct GLFWwindow;

namespace Lab {

    struct Time {
        float delta;
        float total;
        float fixedDelta;
    };

    struct Input {
        static bool keys[512];
        static bool mouseButtons[8];
        static Vec2 mousePos;
        static Vec2 mouseDelta;

        static bool isKeyPressed(int key) { return key >= 0 && key < 512 && keys[key]; }
        static bool isMouseButtonPressed(int button) { return button >= 0 && button < 8 && mouseButtons[button]; }
    };

    class Engine {
    public:
        Engine(const std::string& title, int width, int height);
        virtual ~Engine() = default;

        void run();
        void stop();

        virtual void onInit() {}
        virtual void onUpdate(const Time& /*time*/) {}
        virtual void onRender() {}
        virtual void onShutdown() {}

        static Engine* get() { return _instance; }

    private:
        static Engine* _instance;
        GLFWwindow* _window;

        static void _keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
        static void _mouseButtonCallback(GLFWwindow* window, int button, int action, int mods);
        static void _cursorPosCallback(GLFWwindow* window, double xpos, double ypos);
        static void _framebufferSizeCallback(GLFWwindow* window, int width, int height);

        std::string _title;
        int _width, _height;
        bool _running;
        Time _time;
        double _lastFrameTime;
        bool _firstMouse;
        Vec2 _lastMousePos;
    };
}
