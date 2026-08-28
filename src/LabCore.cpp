#include "LabCore.h"
#include <glad/gl.h>
#include <GLFW/glfw3.h>
#include <iostream>

namespace Lab {

    Engine* Engine::_instance = nullptr;
    bool Input::keys[512] = { false };
    bool Input::mouseButtons[8] = { false };
    Vec2 Input::mousePos = { 0, 0 };
    Vec2 Input::mouseDelta = { 0, 0 };

    Engine::Engine(const std::string& title, int width, int height)
        : _title(title), _width(width), _height(height), _running(false), _lastFrameTime(0.0), _firstMouse(true), _lastMousePos({0,0}) {
        if (_instance) {
            std::cerr << "Engine instance already exists!" << std::endl;
            return;
        }
        _instance = this;
        _time = { 0, 0, 1.0f / 60.0f };
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

        // Callbacks
        glfwSetKeyCallback(_window, _keyCallback);
        glfwSetMouseButtonCallback(_window, _mouseButtonCallback);
        glfwSetCursorPosCallback(_window, _cursorPosCallback);
        glfwSetFramebufferSizeCallback(_window, _framebufferSizeCallback);

        // Capture mouse
        glfwSetInputMode(_window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

        glEnable(GL_DEPTH_TEST);
        glEnable(GL_CULL_FACE);

        onInit();

        _running = true;
        _lastFrameTime = glfwGetTime();
        
        while (!glfwWindowShouldClose(_window) && _running) {
            double currentTime = glfwGetTime();
            _time.delta = (float)(currentTime - _lastFrameTime);
            _time.total += _time.delta;
            _lastFrameTime = currentTime;

            onUpdate(_time);
            
            // Render
            onRender();

            glfwSwapBuffers(_window);
            
            // Reset delta before poll events
            Input::mouseDelta = { 0, 0 };
            glfwPollEvents();
        }

        onShutdown();
        glfwTerminate();
    }

    void Engine::stop() {
        _running = false;
        if (_window) glfwSetWindowShouldClose(_window, true);
    }

    void Engine::_keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
        (void)window; (void)scancode; (void)mods;
        if (key >= 0 && key < 512) {
            if (action == GLFW_PRESS) Input::keys[key] = true;
            else if (action == GLFW_RELEASE) Input::keys[key] = false;
        }
        
        if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
            if (_instance) _instance->stop();
        }
    }

    void Engine::_mouseButtonCallback(GLFWwindow* window, int button, int action, int mods) {
        (void)window; (void)mods;
        if (button >= 0 && button < 8) {
            if (action == GLFW_PRESS) Input::mouseButtons[button] = true;
            else if (action == GLFW_RELEASE) Input::mouseButtons[button] = false;
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

            Input::mouseDelta.x = (float)xpos - _instance->_lastMousePos.x;
            Input::mouseDelta.y = (float)ypos - _instance->_lastMousePos.y;
            _instance->_lastMousePos.x = (float)xpos;
            _instance->_lastMousePos.y = (float)ypos;

            Input::mousePos.x = (float)xpos;
            Input::mousePos.y = (float)ypos;
        }
    }

    void Engine::_framebufferSizeCallback(GLFWwindow* window, int width, int height) {
        (void)window;
        if (_instance) {
            _instance->_width = width;
            _instance->_height = height;
            glViewport(0, 0, width, height);
        }
    }
}
