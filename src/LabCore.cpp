#include "LabCore.h"
#include <GL/freeglut.h>
#include <iostream>

namespace Lab {

    Engine* Engine::_instance = nullptr;
    bool Input::keys[256] = { false };
    bool Input::specialKeys[256] = { false };
    bool Input::mouseButtons[3] = { false };
    Vec2 Input::mousePos = { 0, 0 };
    Vec2 Input::mouseDelta = { 0, 0 };

    Engine::Engine(const std::string& title, int width, int height)
        : _title(title), _width(width), _height(height), _running(false), _lastFrameTime(0) {
        if (_instance) {
            std::cerr << "Engine instance already exists!" << std::endl;
            return;
        }
        _instance = this;
        _time = { 0, 0, 1.0f / 60.0f };
    }

    void Engine::run() {
        int argc = 0;
        char** argv = nullptr;
        glutInit(&argc, argv);
        glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
        glutInitWindowSize(_width, _height);
        glutCreateWindow(_title.c_str());

        // Callbacks
        glutDisplayFunc(_displayFunc);
        glutIdleFunc(_idleFunc);
        glutKeyboardFunc(_keyboardFunc);
        glutKeyboardUpFunc(_keyboardUpFunc);
        glutSpecialFunc(_specialFunc);
        glutSpecialUpFunc(_specialUpFunc);
        glutMouseFunc(_mouseFunc);
        glutPassiveMotionFunc(_passiveMotionFunc);
        glutReshapeFunc(_reshapeFunc);

        glEnable(GL_DEPTH_TEST);
        glEnable(GL_CULL_FACE);

        Input::mousePos = { (float)_width / 2.0f, (float)_height / 2.0f };
        glutWarpPointer((int)Input::mousePos.x, (int)Input::mousePos.y);

        onInit();

        _running = true;
        _lastFrameTime = glutGet(GLUT_ELAPSED_TIME);
        
        glutMainLoop();
    }

    void Engine::stop() {
        _running = false;
        onShutdown();
        exit(0);
    }

    void Engine::_displayFunc() {
        if (_instance) _instance->onRender();
        glutSwapBuffers();
    }

    void Engine::_idleFunc() {
        if (!_instance) return;

        int currentTime = glutGet(GLUT_ELAPSED_TIME);
        _instance->_time.delta = (currentTime - _instance->_lastFrameTime) / 1000.0f;
        _instance->_time.total += _instance->_time.delta;
        _instance->_lastFrameTime = currentTime;

        _instance->onUpdate(_instance->_time);
        
        // Reset mouse delta after each frame update
        Input::mouseDelta = { 0, 0 };

        glutPostRedisplay();
    }

    void Engine::_keyboardFunc(unsigned char key, int x, int y) {
        Input::keys[key] = true;
        if (key == 27) _instance->stop(); // ESC to exit
    }

    void Engine::_keyboardUpFunc(unsigned char key, int x, int y) {
        Input::keys[key] = false;
    }

    void Engine::_specialFunc(int key, int x, int y) {
        if (key >= 0 && key < 256) Input::specialKeys[key] = true;
    }

    void Engine::_specialUpFunc(int key, int x, int y) {
        if (key >= 0 && key < 256) Input::specialKeys[key] = false;
    }

    void Engine::_mouseFunc(int button, int state, int x, int y) {
        if (button >= 0 && button < 3) {
            Input::mouseButtons[button] = (state == GLUT_DOWN);
        }
    }

    void Engine::_passiveMotionFunc(int x, int y) {
        Input::mouseDelta.x += (float)x - Input::mousePos.x;
        Input::mouseDelta.y += (float)y - Input::mousePos.y;
        Input::mousePos = { (float)x, (float)y };

        // Center mouse for FPS camera
        int centerX = glutGet(GLUT_WINDOW_WIDTH) / 2;
        int centerY = glutGet(GLUT_WINDOW_HEIGHT) / 2;
        if (x != centerX || y != centerY) {
            glutWarpPointer(centerX, centerY);
            Input::mousePos = { (float)centerX, (float)centerY };
        }
    }

    void Engine::_reshapeFunc(int w, int h) {
        if (_instance) {
            _instance->_width = w;
            _instance->_height = h;
            glViewport(0, 0, w, h);
        }
    }
}
