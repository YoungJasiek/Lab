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

    class LabLog {
    public:
        static void info(const std::string& msg);
        static void warn(const std::string& msg);
        static void error(const std::string& msg);
        static void debug(const std::string& msg);
    };

    struct Input {
        static bool keys[512];
        static bool mouseButtons[8];
        static Vec2 mousePos;
        static Vec2 mouseDelta;
        static float scrollDelta;

        static bool isKeyPressed(int key) {
            if (key >= 'a' && key <= 'z') key = key - 'a' + 'A';
            return key >= 0 && key < 512 && keys[key];
        }
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
        virtual void onFixedUpdate(float /*fixedDelta*/) {}
        virtual void onRender() {}
        virtual void onShutdown() {}

        static Engine* get() { return _instance; }
        GLFWwindow* getWindow() const { return _window; }
        int getWidth() const { return _width; }
        int getHeight() const { return _height; }

    private:
        static Engine* _instance;
        GLFWwindow* _window;

        static void _keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
        static void _mouseButtonCallback(GLFWwindow* window, int button, int action, int mods);
        static void _cursorPosCallback(GLFWwindow* window, double xpos, double ypos);
        static void _scrollCallback(GLFWwindow* window, double xoffset, double yoffset);
        static void _framebufferSizeCallback(GLFWwindow* window, int width, int height);

        std::string _title;
        int _width, _height;
        bool _running;
        Time _time;
        double _lastFrameTime;
        float _physicsAccumulator;
        bool _firstMouse;
        Vec2 _lastMousePos;
    };
}
