#include "LabCore.h"
#include <glad/gl.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <fstream>
#include <chrono>
#include <ctime>
#include <cstring>

namespace Lab {

    static std::ofstream& getLogStream() {
        static std::ofstream s_logFile("lab_engine.log", std::ios::out | std::ios::app);
        return s_logFile;
    }

    static void logOutput(const std::string& level, const std::string& msg) {
        auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
        char timeBuf[32];
        struct tm tmBuf;
        localtime_s(&tmBuf, &now);
        strftime(timeBuf, sizeof(timeBuf), "%H:%M:%S", &tmBuf);

        std::string line = "[" + std::string(timeBuf) + "] [" + level + "] " + msg;
        std::cout << line << std::endl;
        auto& logFile = getLogStream();
        if (logFile.is_open()) {
            logFile << line << std::endl;
            logFile.flush();
        }
    }

    void LabLog::info(const std::string& msg) { logOutput("INFO", msg); }
    void LabLog::warn(const std::string& msg) { logOutput("WARN", msg); }
    void LabLog::error(const std::string& msg) { logOutput("ERROR", msg); }
    void LabLog::debug(const std::string& msg) { logOutput("DEBUG", msg); }

    Engine* Engine::_instance = nullptr;
    bool Input::keys[512] = { false };
    bool Input::keysJustPressed[512] = { false };
    bool Input::keysJustReleased[512] = { false };
    bool Input::mouseButtons[8] = { false };
    bool Input::mouseButtonsJustPressed[8] = { false };
    bool Input::mouseButtonsJustReleased[8] = { false };
    Vec2 Input::mousePos = { 0, 0 };
    Vec2 Input::mouseDelta = { 0, 0 };
    float Input::scrollDelta = 0.0f;

    bool Input::isKeyPressed(int key) {
        if (key >= 'a' && key <= 'z') key = key - 'a' + 'A';
        return key >= 0 && key < 512 && (keys[key] || keysJustPressed[key]);
    }

    bool Input::isKeyJustPressed(int key) {
        if (key >= 'a' && key <= 'z') key = key - 'a' + 'A';
        return key >= 0 && key < 512 && keysJustPressed[key];
    }

    bool Input::isKeyJustReleased(int key) {
        if (key >= 'a' && key <= 'z') key = key - 'a' + 'A';
        return key >= 0 && key < 512 && keysJustReleased[key];
    }

    bool Input::isMouseButtonPressed(int button) {
        return button >= 0 && button < 8 && (mouseButtons[button] || mouseButtonsJustPressed[button]);
    }

    bool Input::isMouseButtonJustPressed(int button) {
        return button >= 0 && button < 8 && mouseButtonsJustPressed[button];
    }

    bool Input::isMouseButtonJustReleased(int button) {
        return button >= 0 && button < 8 && mouseButtonsJustReleased[button];
    }

    Engine::Engine(const std::string& title, int width, int height)
        : _window(nullptr), _standardCursor(nullptr), _cursorCaptured(false),
          _title(title), _width(width), _height(height), _running(false),
          _lastFrameTime(0.0), _physicsAccumulator(0.0f), _firstMouse(true), _lastMousePos({0,0}) {
        if (_instance) {
            std::cerr << "Engine instance already exists!" << std::endl;
            return;
        }
        _instance = this;
        _time = { 0, 0, 1.0f / 64.0f };
    }

    Engine::~Engine() {
        if (_standardCursor) {
            glfwDestroyCursor(_standardCursor);
            _standardCursor = nullptr;
        }
    }

    void Engine::run() {
        if (!glfwInit()) {
            std::cerr << "Failed to initialize GLFW" << std::endl;
            return;
        }

        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

        _window = glfwCreateWindow(_width, _height, _title.c_str(), nullptr, nullptr);
        if (!_window) {
            std::cerr << "Failed to create GLFW window" << std::endl;
            glfwTerminate();
            return;
        }

        glfwMakeContextCurrent(_window);
        glfwSetWindowUserPointer(_window, this);

        // Load glad
        if (!gladLoadGL(glfwGetProcAddress)) {
            std::cerr << "Failed to initialize GLAD" << std::endl;
            return;
        }

        // Enable VSync (caps framerate to monitor refresh rate, prevents 100% GPU utilization)
        glfwSwapInterval(1);

        // Callbacks
        glfwSetKeyCallback(_window, _keyCallback);
        glfwSetMouseButtonCallback(_window, _mouseButtonCallback);
        glfwSetCursorPosCallback(_window, _cursorPosCallback);
        glfwSetScrollCallback(_window, _scrollCallback);
        glfwSetFramebufferSizeCallback(_window, _framebufferSizeCallback);

        // Standard Arrow Cursor (visible and active by default)
        _standardCursor = glfwCreateStandardCursor(GLFW_ARROW_CURSOR);
        if (_standardCursor) {
            glfwSetCursor(_window, _standardCursor);
        }
        glfwSetInputMode(_window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);

        glEnable(GL_DEPTH_TEST);
        glEnable(GL_CULL_FACE);

        onInit();

        _running = true;
        _lastFrameTime = glfwGetTime();
        const float fixedDelta = 1.0f / 64.0f; // 64-tick physics rate
        
        while (!glfwWindowShouldClose(_window) && _running) {
            // Clear transient per-frame edge states before polling
            std::memset(Input::keysJustPressed, 0, sizeof(Input::keysJustPressed));
            std::memset(Input::keysJustReleased, 0, sizeof(Input::keysJustReleased));
            std::memset(Input::mouseButtonsJustPressed, 0, sizeof(Input::mouseButtonsJustPressed));
            std::memset(Input::mouseButtonsJustReleased, 0, sizeof(Input::mouseButtonsJustReleased));
            Input::mouseDelta = { 0, 0 };
            Input::scrollDelta = 0.0f;

            glfwPollEvents();

            // Synchronize cursor position directly from GLFW if valid; preserve callback coordinates otherwise
            double curX = 0, curY = 0;
            glfwGetCursorPos(_window, &curX, &curY);
            if (curX != 0.0 || curY != 0.0 || (Input::mousePos.x == 0.0f && Input::mousePos.y == 0.0f)) {
                Input::mousePos.x = (float)curX;
                Input::mousePos.y = (float)curY;
            }

            // Maintain continuous hold/release state for pressed buttons
            for (int b = 0; b < 8; ++b) {
                int state = glfwGetMouseButton(_window, b);
                if (state == GLFW_PRESS) {
                    Input::mouseButtons[b] = true;
                } else if (state == GLFW_RELEASE) {
                    Input::mouseButtons[b] = false;
                }
            }

            double currentTime = glfwGetTime();
            float frameTime = (float)(currentTime - _lastFrameTime);
            if (frameTime > 0.25f) frameTime = 0.25f; // Clamp to avoid spiral of death
            _lastFrameTime = currentTime;

            _time.delta = frameTime;
            _time.total += frameTime;
            _physicsAccumulator += frameTime;

            // Fixed timestep physics update
            while (_physicsAccumulator >= fixedDelta) {
                onFixedUpdate(fixedDelta);
                _physicsAccumulator -= fixedDelta;
            }

            // Variable framerate update
            onUpdate(_time);
            
            // Render
            onRender();

            glfwSwapBuffers(_window);
        }

        onShutdown();
        if (_standardCursor) {
            glfwDestroyCursor(_standardCursor);
            _standardCursor = nullptr;
        }
        glfwTerminate();
    }

    void Engine::setCursorCaptured(bool captured) {
        _cursorCaptured = captured;
        if (!_window) return;
        _firstMouse = true;
        if (captured) {
            glfwSetInputMode(_window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        } else {
            glfwSetInputMode(_window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
            if (_standardCursor) {
                glfwSetCursor(_window, _standardCursor);
            }
        }
    }

    void Engine::stop() {
        _running = false;
        if (_window) glfwSetWindowShouldClose(_window, true);
    }

    void Engine::_keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
        (void)window; (void)scancode; (void)mods;
        if (key >= 0 && key < 512) {
            if (action == GLFW_PRESS) {
                Input::keys[key] = true;
                Input::keysJustPressed[key] = true;
            } else if (action == GLFW_RELEASE) {
                Input::keys[key] = false;
                Input::keysJustReleased[key] = true;
            }
        }
    }

    void Engine::_mouseButtonCallback(GLFWwindow* window, int button, int action, int mods) {
        (void)window; (void)mods;
        if (button >= 0 && button < 8) {
            if (action == GLFW_PRESS) {
                Input::mouseButtons[button] = true;
                Input::mouseButtonsJustPressed[button] = true;
                LabLog::info("[Engine] Mouse Button PRESS: " + std::to_string(button) + " at (" + std::to_string(Input::mousePos.x) + ", " + std::to_string(Input::mousePos.y) + ")");
            } else if (action == GLFW_RELEASE) {
                Input::mouseButtons[button] = false;
                Input::mouseButtonsJustReleased[button] = true;
                LabLog::info("[Engine] Mouse Button RELEASE: " + std::to_string(button) + " at (" + std::to_string(Input::mousePos.x) + ", " + std::to_string(Input::mousePos.y) + ")");
            }
        }
    }

    void Engine::_cursorPosCallback(GLFWwindow* window, double xpos, double ypos) {
        (void)window;
        if (_instance) {
            if (_instance->_firstMouse) {
                _instance->_lastMousePos.x = (float)xpos;
                _instance->_lastMousePos.y = (float)ypos;
                _instance->_firstMouse = false;
            }

            Input::mouseDelta.x += (float)xpos - _instance->_lastMousePos.x;
            Input::mouseDelta.y += (float)ypos - _instance->_lastMousePos.y;
            _instance->_lastMousePos.x = (float)xpos;
            _instance->_lastMousePos.y = (float)ypos;

            Input::mousePos.x = (float)xpos;
            Input::mousePos.y = (float)ypos;
        }
    }

    void Engine::_scrollCallback(GLFWwindow* window, double xoffset, double yoffset) {
        (void)window; (void)xoffset;
        Input::scrollDelta += (float)yoffset;
    }

    void Engine::_framebufferSizeCallback(GLFWwindow* window, int width, int height) {
        (void)window;
        if (_instance) {
            _instance->_width = width;
            _instance->_height = height;
            glViewport(0, 0, width, height);
        }
    }

    Engine* Engine::get() {
        return _instance;
    }

    GLFWwindow* Engine::getWindow() const {
        return _window;
    }

    int Engine::getWidth() const {
        return _width;
    }

    int Engine::getHeight() const {
        return _height;
    }

    void Engine::getWindowSize(int& width, int& height) const {
        if (_window) {
            glfwGetWindowSize(_window, &width, &height);
        } else {
            width = _width;
            height = _height;
        }
    }

    void Engine::getFramebufferSize(int& width, int& height) const {
        if (_window) {
            glfwGetFramebufferSize(_window, &width, &height);
        } else {
            width = _width;
            height = _height;
        }
    }

    void Engine::getCursorPos(double& xpos, double& ypos) const {
        if (_window) {
            glfwGetCursorPos(_window, &xpos, &ypos);
            if (xpos == 0.0 && ypos == 0.0 && (Input::mousePos.x != 0.0f || Input::mousePos.y != 0.0f)) {
                xpos = (double)Input::mousePos.x;
                ypos = (double)Input::mousePos.y;
            }
        } else {
            xpos = (double)Input::mousePos.x;
            ypos = (double)Input::mousePos.y;
        }
    }

    bool Engine::isCursorCaptured() const {
        return _cursorCaptured;
    }

    Vec2 Input::getMousePos() {
        return mousePos;
    }

    float Input::getMouseX() {
        return mousePos.x;
    }

    float Input::getMouseY() {
        return mousePos.y;
    }

    Vec2 Input::getMouseDelta() {
        return mouseDelta;
    }
}
