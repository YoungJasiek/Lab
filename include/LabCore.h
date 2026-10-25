#pragma once
#include <vector>
#include <string>
#include <memory>
#include <functional>
#include "LabMath.h"
#include "LabExport.h"

struct GLFWwindow;
struct GLFWcursor;

namespace Lab {

    struct Time {
        float delta;
        float total;
        float fixedDelta;
    };

    class LAB_CORE_API LabLog {
    public:
        using LogListener = std::function<void(const std::string& level, const std::string& msg)>;
        static void setListener(LogListener listener);
        static void info(const std::string& msg);
        static void warn(const std::string& msg);
        static void error(const std::string& msg);
        static void debug(const std::string& msg);
    };

    struct LAB_CORE_API Input {
        static bool keys[512];
        static bool keysJustPressed[512];
        static bool keysJustReleased[512];
        static bool mouseButtons[8];
        static bool mouseButtonsJustPressed[8];
        static bool mouseButtonsJustReleased[8];
        static Vec2 mousePos;
        static Vec2 mouseDelta;
        static float scrollDelta;

        static bool isKeyPressed(int key);
        static bool isKeyJustPressed(int key);
        static bool isKeyJustReleased(int key);
        static bool isMouseButtonPressed(int button);
        static bool isMouseButtonJustPressed(int button);
        static bool isMouseButtonJustReleased(int button);

        static Vec2 getMousePos();
        static float getMouseX();
        static float getMouseY();
        static Vec2 getMouseDelta();

        static Vec3 mousePosition;
        static float GetAxis(const std::string& axisName);
    };

    class LAB_CORE_API Engine {
    public:
        Engine(const std::string& title, int width, int height);
        virtual ~Engine();

        void run();
        void stop();

        static int runApplication(std::unique_ptr<class ILabApplication> app, const std::string& title, int width, int height);

        virtual void onInit() {}
        virtual void onUpdate(const Time& /*time*/) {}
        virtual void onFixedUpdate(float /*fixedDelta*/) {}
        virtual void onRender() {}
        virtual void onShutdown() {}
        virtual void onDrop(int count, const char** paths);

        static Engine* get();
        GLFWwindow* getWindow() const;
        int getWidth() const;
        int getHeight() const;

        void getWindowSize(int& width, int& height) const;
        void setWindowSize(int width, int height);
        void getFramebufferSize(int& width, int& height) const;
        void getCursorPos(double& xpos, double& ypos) const;

        void setCursorCaptured(bool captured);
        bool isCursorCaptured() const;

        void setFullscreen(bool enable);
        bool isFullscreen() const;

    private:
        static Engine* _instance;
        GLFWwindow* _window;
        GLFWcursor* _standardCursor;
        bool _cursorCaptured;
        bool _fullscreen;
        int _windowedX;
        int _windowedY;
        int _windowedWidth;
        int _windowedHeight;

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
        double _savedCursorX{0.0};
        double _savedCursorY{0.0};
    };
}
