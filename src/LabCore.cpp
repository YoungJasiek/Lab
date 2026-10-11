#include "LabCore.h"
#include "LabInput.h"
#include "LabApplication.h"
#include "LabPackage.h"
#include <glad/gl.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <fstream>
#include <chrono>
#include <ctime>
#include <cstring>
#include <filesystem>
#include <cstdlib>

namespace Lab {

    static void glfwErrorCallback(int error, const char* description) {
        std::string desc = description ? description : "unknown";
        std::cerr << "[GLFW Error " << error << "]: " << desc << std::endl;
        LabLog::error("[GLFW Error " + std::to_string(error) + "]: " + desc);
    }

    // Drag & drop: delegacja do wirtualnej metody Engine::onDrop
    static void labDropCallback(GLFWwindow* window, int count, const char** paths) {
        Engine* eng = static_cast<Engine*>(glfwGetWindowUserPointer(window));
        if (eng) {
            eng->onDrop(count, paths);
        }
    }

    void Engine::onDrop(int count, const char** paths) {
        for (int i = 0; i < count; ++i) {
            std::string src = paths[i] ? paths[i] : "";
            if (src.size() >= 4 && src.substr(src.size() - 4) == ".lua") {
                std::string id = PackageManager::instance().importDropinFile(src);
                if (!id.empty()) LabLog::info("Drag&drop plugin installed: " + id);
                else LabLog::error("Drag&drop plugin failed: " + src);
            } else {
                LabLog::warn("Drop ignored: " + src);
            }
        }
    }

    static std::ofstream& getLogStream() {
        static std::ofstream s_logFile("lab_engine.log", std::ios::out | std::ios::app);
        return s_logFile;
    }

    static void logOutput(const std::string& level, const std::string& msg) {
        auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
        char timeBuf[32];
        struct tm tmBuf;
#if defined(_WIN32)
        localtime_s(&tmBuf, &now);
#else
        localtime_r(&now, &tmBuf);
#endif
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
    Vec3 Input::mousePosition = { 0.0f, 0.0f, 0.0f };

    float Input::GetAxis(const std::string& axisName) {
        if (axisName == "MouseX" || axisName == "mouseX") {
            return mouseDelta.x;
        }
        if (axisName == "MouseY" || axisName == "mouseY") {
            return mouseDelta.y;
        }
        return InputActionManager::instance().getAxis(axisName);
    }

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
          _fullscreen(false), _windowedX(100), _windowedY(100), _windowedWidth(width), _windowedHeight(height),
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
#if !defined(_WIN32)
        // On Linux / Mesa / WSL2 environments, enable modern OpenGL 4.6 & GLSL 460 if supported by Mesa driver
        // (Mesa drivers like d3d12 often default to 4.2 unless explicitly requested/overridden).
        setenv("MESA_GL_VERSION_OVERRIDE", "4.6", 0);
        setenv("MESA_GLSL_VERSION_OVERRIDE", "460", 0);

        const char* disp = std::getenv("DISPLAY");
        const char* waylandDisp = std::getenv("WAYLAND_DISPLAY");
        if ((!disp || std::strlen(disp) == 0) && (!waylandDisp || std::strlen(waylandDisp) == 0)) {
            std::cerr << "\n[LabCore Warning] Neither DISPLAY nor WAYLAND_DISPLAY environment variable is set.\n"
                      << "If running in WSL2, ensure WSLg or an X-Server (VcXsrv/X410) is running, or execute:\n"
                      << "  export DISPLAY=:0\n" << std::endl;
            LabLog::warn("[LabCore] Missing DISPLAY and WAYLAND_DISPLAY environment variables.");
        }
#endif

        glfwSetErrorCallback(glfwErrorCallback);

        if (!glfwInit()) {
            std::cerr << "Failed to initialize GLFW" << std::endl;
            return;
        }

        GLFWmonitor* monitor = nullptr;
        int winW = _width;
        int winH = _height;
        if (_fullscreen) {
            monitor = glfwGetPrimaryMonitor();
            if (monitor) {
                const GLFWvidmode* mode = glfwGetVideoMode(monitor);
                if (mode) {
                    winW = mode->width;
                    winH = mode->height;
                }
            }
        }

        // Try highest profile (4.6) down to 3.3 for graceful driver, VM and Mesa compatibility
        static const struct { int major; int minor; } glVersions[] = {
            { 4, 6 },
            { 4, 5 },
            { 4, 4 },
            { 4, 3 },
            { 4, 2 },
            { 4, 1 },
            { 4, 0 },
            { 3, 3 }
        };

        for (const auto& ver : glVersions) {
            glfwDefaultWindowHints();
            glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, ver.major);
            glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, ver.minor);
            glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#if defined(__APPLE__)
            glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif
            _window = glfwCreateWindow(winW, winH, _title.c_str(), monitor, nullptr);
            if (_window) {
                if (ver.major != 4 || ver.minor != 6) {
                    std::cout << "[LabCore] Initialized OpenGL " << ver.major << "." << ver.minor << " Core Profile (Driver Fallback)." << std::endl;
                    LabLog::info("[LabCore] Initialized OpenGL " + std::to_string(ver.major) + "." + std::to_string(ver.minor) + " Core Profile (Driver Fallback).");
                }
                break;
            }
        }

        if (!_window) {
            std::cerr << "Failed to create GLFW window with Core Profile (tried OpenGL 4.6 down to 3.3)" << std::endl;
            glfwTerminate();
            return;
        }

        if (!_fullscreen) {
            glfwGetWindowPos(_window, &_windowedX, &_windowedY);
            glfwGetWindowSize(_window, &_windowedWidth, &_windowedHeight);
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
        glfwSetDropCallback(_window, labDropCallback);

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

    class GenericAppEngineHost : public Engine {
    public:
        GenericAppEngineHost(std::unique_ptr<ILabApplication> app, const std::string& title, int width, int height)
            : Engine(title, width, height), m_app(std::move(app)) {}

        void onInit() override {
            if (m_app) m_app->onInit();
        }

        void onUpdate(const Time& time) override {
            if (m_app) m_app->onUpdate(time.delta);
        }

        void onRender() override {
            if (m_app) m_app->onRender();
        }

        void onShutdown() override {
            if (m_app) m_app->onShutdown();
        }

    private:
        std::unique_ptr<ILabApplication> m_app;
    };

    int Engine::runApplication(std::unique_ptr<ILabApplication> app, const std::string& title, int width, int height) {
        if (!app) return -1;
        GenericAppEngineHost host(std::move(app), title, width, height);
        host.run();
        return 0;
    }

    void Engine::setCursorCaptured(bool captured) {
        if (_cursorCaptured == captured) return;
        _cursorCaptured = captured;
        if (!_window) return;
        _firstMouse = true;
        if (captured) {
            glfwGetCursorPos(_window, &_savedCursorX, &_savedCursorY);
            glfwSetInputMode(_window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
#if !defined(__linux__)
            if (glfwRawMouseMotionSupported()) {
                glfwSetInputMode(_window, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
            }
#endif
        } else {
#if !defined(__linux__)
            if (glfwRawMouseMotionSupported()) {
                glfwSetInputMode(_window, GLFW_RAW_MOUSE_MOTION, GLFW_FALSE);
            }
#endif
            glfwSetInputMode(_window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
            if (_savedCursorX > 0.0 || _savedCursorY > 0.0) {
                glfwSetCursorPos(_window, _savedCursorX, _savedCursorY);
                Input::mousePos.x = (float)_savedCursorX;
                Input::mousePos.y = (float)_savedCursorY;
            }
            if (_standardCursor) {
                glfwSetCursor(_window, _standardCursor);
            }
        }
        Input::mouseDelta = { 0, 0 };
    }

    void Engine::setFullscreen(bool enable) {
        if (_fullscreen == enable) return;
        _fullscreen = enable;
        if (!_window) return;

        if (_fullscreen) {
            glfwGetWindowPos(_window, &_windowedX, &_windowedY);
            glfwGetWindowSize(_window, &_windowedWidth, &_windowedHeight);
            GLFWmonitor* monitor = glfwGetPrimaryMonitor();
            if (monitor) {
                const GLFWvidmode* mode = glfwGetVideoMode(monitor);
                if (mode) {
                    glfwSetWindowMonitor(_window, monitor, 0, 0, mode->width, mode->height, mode->refreshRate);
                    _width = mode->width;
                    _height = mode->height;
                    glViewport(0, 0, _width, _height);
                }
            }
        } else {
            glfwSetWindowMonitor(_window, nullptr, _windowedX, _windowedY, _windowedWidth, _windowedHeight, 0);
            _width = _windowedWidth;
            _height = _windowedHeight;
            glViewport(0, 0, _width, _height);
        }
        LabLog::info("[Engine] Fullscreen set to: " + std::string(_fullscreen ? "ON" : "OFF") + " (" + std::to_string(_width) + "x" + std::to_string(_height) + ")");
    }

    bool Engine::isFullscreen() const {
        return _fullscreen;
    }

    void Engine::stop() {
        _running = false;
        if (_window) glfwSetWindowShouldClose(_window, true);
    }

    void Engine::_keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
        (void)window; (void)scancode;
        if (key >= 0 && key < 512) {
            if (action == GLFW_PRESS) {
                if (key == GLFW_KEY_F11 || (key == GLFW_KEY_ENTER && (mods & GLFW_MOD_ALT))) {
                    if (_instance) {
                        _instance->setFullscreen(!_instance->isFullscreen());
                    }
                }
                Input::keys[key] = true;
                Input::keysJustPressed[key] = true;
            } else if (action == GLFW_REPEAT) {
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
                Input::mouseDelta = { 0.0f, 0.0f };
                Input::mousePos.x = (float)xpos;
                Input::mousePos.y = (float)ypos;
                Input::mousePosition.x = (float)xpos;
                Input::mousePosition.y = (float)ypos;
                Input::mousePosition.z = 0.0f;
                return;
            }

            float dx = (float)xpos - _instance->_lastMousePos.x;
            float dy = (float)ypos - _instance->_lastMousePos.y;

            // Clamp delta spike (e.g. cursor warp/grab jumps) to prevent camera 180-deg flips
            dx = std::clamp(dx, -100.0f, 100.0f);
            dy = std::clamp(dy, -100.0f, 100.0f);

            Input::mouseDelta.x += dx;
            Input::mouseDelta.y += dy;
            _instance->_lastMousePos.x = (float)xpos;
            _instance->_lastMousePos.y = (float)ypos;

            Input::mousePos.x = (float)xpos;
            Input::mousePos.y = (float)ypos;
            Input::mousePosition.x = (float)xpos;
            Input::mousePosition.y = (float)ypos;
            Input::mousePosition.z = 0.0f;
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

    void Engine::setWindowSize(int width, int height) {
        if (width <= 0 || height <= 0) return;
        _width = width;
        _height = height;
        if (_window) {
            glfwSetWindowSize(_window, width, height);
            glViewport(0, 0, width, height);
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
