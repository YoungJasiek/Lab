#pragma once
#include <vector>
#include <string>
#include <memory>
#include "LabMath.h"

namespace Lab {

    struct Time {
        float delta;
        float total;
        float fixedDelta;
    };

    struct Input {
        static bool keys[256];
        static bool mouseButtons[3];
        static Vec2 mousePos;
        static Vec2 mouseDelta;

        static bool isKeyPressed(unsigned char key) { return keys[key]; }
        static bool isMouseButtonPressed(int button) { return mouseButtons[button]; }
    };

    class Engine {
    public:
        Engine(const std::string& title, int width, int height);
        virtual ~Engine() = default;

        void run();
        void stop();

        virtual void onInit() {}
        virtual void onUpdate(const Time& time) {}
        virtual void onRender() {}
        virtual void onShutdown() {}

        static Engine* get() { return _instance; }

    private:
        static void _displayFunc();
        static void _idleFunc();
        static void _keyboardFunc(unsigned char key, int x, int y);
        static void _keyboardUpFunc(unsigned char key, int x, int y);
        static void _passiveMotionFunc(int x, int y);
        static void _reshapeFunc(int w, int h);

        static Engine* _instance;

        std::string _title;
        int _width, _height;
        bool _running;
        Time _time;
        int _lastFrameTime;
    };
}
