#include "LabInput.h"
#include "LabCore.h"
#include <GLFW/glfw3.h>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cmath>

namespace Lab {

    InputActionManager& InputActionManager::instance() {
        static InputActionManager s_instance;
        return s_instance;
    }

    InputActionManager::InputActionManager() {
        setupDefaultBindings();
    }

    void InputActionManager::setupDefaultBindings() {
        registerAction("Jump", "Jump / Mantle");
        bindKeyToAction("Jump", GLFW_KEY_SPACE);
        bindGamepadButtonToAction("Jump", GamepadButton::A);

        registerAction("Fire", "Primary Attack / Shoot");
        bindMouseButtonToAction("Fire", GLFW_MOUSE_BUTTON_LEFT);
        bindGamepadButtonToAction("Fire", GamepadButton::RightBumper);

        registerAction("AltFire", "Secondary Attack / Aim");
        bindMouseButtonToAction("AltFire", GLFW_MOUSE_BUTTON_RIGHT);
        bindGamepadButtonToAction("AltFire", GamepadButton::LeftBumper);

        registerAction("Interact", "Use / Interact / Pickup");
        bindKeyToAction("Interact", GLFW_KEY_E);
        bindGamepadButtonToAction("Interact", GamepadButton::X);

        registerAction("Refill", "Refill Weapon");
        bindKeyToAction("Refill", GLFW_KEY_R);
        bindGamepadButtonToAction("Refill", GamepadButton::B);

        registerAction("Sprint", "Sprint / Speed Boost");
        bindKeyToAction("Sprint", GLFW_KEY_LEFT_SHIFT);
        bindGamepadButtonToAction("Sprint", GamepadButton::LeftThumb);

        registerAction("Crouch", "Crouch / Slide");
        bindKeyToAction("Crouch", GLFW_KEY_LEFT_CONTROL);
        bindGamepadButtonToAction("Crouch", GamepadButton::RightThumb);

        registerAction("Pause", "Pause Menu / Cancel");
        bindKeyToAction("Pause", GLFW_KEY_ESCAPE);
        bindGamepadButtonToAction("Pause", GamepadButton::Start);

        // Analog Axes
        registerAxis("MoveForward", "Move Forward / Backward");
        bindKeysToAxis("MoveForward", GLFW_KEY_W, GLFW_KEY_S, 1.0f);
        bindGamepadAxisToAxis("MoveForward", GamepadAxis::LeftY, -1.0f, 0.15f, true); // Stick Y is inverted

        registerAxis("MoveRight", "Move Strafe Right / Left");
        bindKeysToAxis("MoveRight", GLFW_KEY_D, GLFW_KEY_A, 1.0f);
        bindGamepadAxisToAxis("MoveRight", GamepadAxis::LeftX, 1.0f, 0.15f, false);

        registerAxis("LookPitch", "Look Vertical Pitch");
        bindMouseToAxis("LookPitch", 1, 1.0f);
        bindGamepadAxisToAxis("LookPitch", GamepadAxis::RightY, 1.0f, 0.15f, true);

        registerAxis("LookYaw", "Look Horizontal Yaw");
        bindMouseToAxis("LookYaw", 0, 1.0f);
        bindGamepadAxisToAxis("LookYaw", GamepadAxis::RightX, 1.0f, 0.15f, false);
    }

    void InputActionManager::registerAction(const std::string& name, const std::string& description) {
        if (_actions.contains(name)) {
            if (!description.empty()) _actions[name].description = description;
            return;
        }
        InputAction act;
        act.name = name;
        act.description = description;
        _actions[name] = act;
    }

    void InputActionManager::registerAxis(const std::string& name, const std::string& description) {
        if (_axes.contains(name)) {
            if (!description.empty()) _axes[name].description = description;
            return;
        }
        InputAxis axis;
        axis.name = name;
        axis.description = description;
        _axes[name] = axis;
    }

    bool InputActionManager::hasAction(const std::string& name) const {
        return _actions.contains(name);
    }

    bool InputActionManager::hasAxis(const std::string& name) const {
        return _axes.contains(name);
    }

    void InputActionManager::bindKeyToAction(const std::string& actionName, int key, int mods) {
        registerAction(actionName);
        ActionBinding b;
        b.device = InputDevice::Keyboard;
        b.code = key;
        b.modifierFlags = mods;
        _actions[actionName].bindings.push_back(b);
    }

    void InputActionManager::bindMouseButtonToAction(const std::string& actionName, int button) {
        registerAction(actionName);
        ActionBinding b;
        b.device = InputDevice::Mouse;
        b.code = button;
        _actions[actionName].bindings.push_back(b);
    }

    void InputActionManager::bindGamepadButtonToAction(const std::string& actionName, GamepadButton btn) {
        registerAction(actionName);
        ActionBinding b;
        b.device = InputDevice::Gamepad;
        b.code = static_cast<int>(btn);
        _actions[actionName].bindings.push_back(b);
    }

    void InputActionManager::bindKeysToAxis(const std::string& axisName, int positiveKey, int negativeKey, float scale) {
        registerAxis(axisName);
        AxisBinding b;
        b.device = InputDevice::Keyboard;
        b.positiveKey = positiveKey;
        b.negativeKey = negativeKey;
        b.scale = scale;
        _axes[axisName].bindings.push_back(b);
    }

    void InputActionManager::bindGamepadAxisToAxis(const std::string& axisName, GamepadAxis axis, float scale, float deadzone, bool invert) {
        registerAxis(axisName);
        AxisBinding b;
        b.device = InputDevice::Gamepad;
        b.gamepadAxis = static_cast<int>(axis);
        b.scale = scale;
        b.deadzone = deadzone;
        b.invert = invert;
        _axes[axisName].bindings.push_back(b);
    }

    void InputActionManager::bindMouseToAxis(const std::string& axisName, int mouseAxis, float sensitivity) {
        registerAxis(axisName);
        AxisBinding b;
        b.device = InputDevice::Mouse;
        b.mouseAxis = mouseAxis;
        b.scale = sensitivity;
        _axes[axisName].bindings.push_back(b);
    }

    void InputActionManager::clearBindings(const std::string& name) {
        if (_actions.contains(name)) _actions[name].bindings.clear();
        if (_axes.contains(name)) _axes[name].bindings.clear();
    }

    bool InputActionManager::isActionPressed(const std::string& name) const {
        auto it = _actions.find(name);
        return (it != _actions.end()) ? it->second.isPressed : false;
    }

    bool InputActionManager::isActionJustPressed(const std::string& name) const {
        auto it = _actions.find(name);
        return (it != _actions.end()) ? it->second.isJustPressed : false;
    }

    bool InputActionManager::isActionJustReleased(const std::string& name) const {
        auto it = _actions.find(name);
        return (it != _actions.end()) ? it->second.isJustReleased : false;
    }

    float InputActionManager::getAxis(const std::string& name) const {
        auto it = _axes.find(name);
        return (it != _axes.end()) ? it->second.value : 0.0f;
    }

    GamepadStateInfo InputActionManager::getGamepadState(int jid) const {
        GamepadStateInfo info;
        if (glfwJoystickPresent(jid) && glfwJoystickIsGamepad(jid)) {
            info.connected = true;
            const char* name = glfwGetGamepadName(jid);
            info.name = name ? name : "Generic Gamepad";

            GLFWgamepadstate state;
            if (glfwGetGamepadState(jid, &state)) {
                for (int i = 0; i < 6; ++i) info.axes[i] = state.axes[i];
                for (int i = 0; i < 15; ++i) info.buttons[i] = (state.buttons[i] == GLFW_PRESS);
            }
        }
        return info;
    }

    void InputActionManager::update(float dt) {
        (void)dt;
        GamepadStateInfo pad = getGamepadState(0);

        // 1. Update Digital Actions
        for (auto& [name, act] : _actions) {
            bool currentlyPressed = false;
            float maxVal = 0.0f;

            for (const auto& b : act.bindings) {
                if (b.device == InputDevice::Keyboard) {
                    if (b.code >= 0 && b.code < 512) {
                        if (Input::isKeyPressed(b.code)) {
                            currentlyPressed = true;
                            maxVal = 1.0f;
                            break;
                        }
                    }
                } else if (b.device == InputDevice::Mouse) {
                    if (b.code >= 0 && b.code < 8) {
                        if (Input::isMouseButtonPressed(b.code)) {
                            currentlyPressed = true;
                            maxVal = 1.0f;
                            break;
                        }
                    }
                } else if (b.device == InputDevice::Gamepad && pad.connected) {
                    if (b.code >= 0 && b.code < 15) {
                        if (pad.buttons[b.code]) {
                            currentlyPressed = true;
                            maxVal = 1.0f;
                            break;
                        }
                    }
                }
            }

            act.isJustPressed = (!act.isPressed && currentlyPressed);
            act.isJustReleased = (act.isPressed && !currentlyPressed);
            act.isPressed = currentlyPressed;
            act.value = maxVal;

            if (act.isJustPressed) {
                if (_callbacks.contains(name)) {
                    for (auto& cb : _callbacks[name]) cb(true);
                }
            } else if (act.isJustReleased) {
                if (_callbacks.contains(name)) {
                    for (auto& cb : _callbacks[name]) cb(false);
                }
            }
        }

        // 2. Update Analog Axes
        for (auto& [name, axis] : _axes) {
            float total = 0.0f;

            for (const auto& b : axis.bindings) {
                if (b.device == InputDevice::Keyboard) {
                    float val = 0.0f;
                    if (b.positiveKey > 0 && Input::isKeyPressed(b.positiveKey)) val += 1.0f;
                    if (b.negativeKey > 0 && Input::isKeyPressed(b.negativeKey)) val -= 1.0f;
                    total += val * b.scale;
                } else if (b.device == InputDevice::Gamepad && pad.connected) {
                    if (b.gamepadAxis >= 0 && b.gamepadAxis < 6) {
                        float raw = pad.axes[b.gamepadAxis];
                        if (b.invert) raw = -raw;
                        if (std::abs(raw) < b.deadzone) raw = 0.0f;
                        else {
                            float sign = (raw > 0.0f) ? 1.0f : -1.0f;
                            raw = sign * ((std::abs(raw) - b.deadzone) / (1.0f - b.deadzone));
                        }
                        total += raw * b.scale;
                    }
                } else if (b.device == InputDevice::Mouse) {
                    Vec2 delta = Input::getMouseDelta();
                    if (b.mouseAxis == 0) total += delta.x * b.scale;
                    else if (b.mouseAxis == 1) total += delta.y * b.scale;
                    else if (b.mouseAxis == 2) total += Input::scrollDelta * b.scale;
                }
            }

            // Clamp between -1.0 and 1.0
            axis.value = std::clamp(total, -1.0f, 1.0f);
        }
    }

    void InputActionManager::onAction(const std::string& actionName, std::function<void(bool)> callback) {
        registerAction(actionName);
        _callbacks[actionName].push_back(callback);
    }

    bool InputActionManager::saveToFile(const std::string& filePath) const {
        std::ofstream out(filePath);
        if (!out.is_open()) return false;

        out << "{\n  \"actions\": {\n";
        size_t aIdx = 0;
        for (const auto& [name, act] : _actions) {
            out << "    \"" << name << "\": {\n";
            out << "      \"description\": \"" << act.description << "\",\n";
            out << "      \"bindings\": [\n";
            for (size_t b = 0; b < act.bindings.size(); ++b) {
                const auto& bind = act.bindings[b];
                out << "        { \"device\": " << static_cast<int>(bind.device)
                    << ", \"code\": " << bind.code
                    << ", \"label\": \"" << (bind.device == InputDevice::Keyboard ? keyToString(bind.code) :
                                            (bind.device == InputDevice::Mouse ? mouseButtonToString(bind.code) :
                                             gamepadButtonToString(static_cast<GamepadButton>(bind.code))))
                    << "\" }" << (b + 1 < act.bindings.size() ? "," : "") << "\n";
            }
            out << "      ]\n    }" << (++aIdx < _actions.size() ? "," : "") << "\n";
        }

        out << "  },\n  \"axes\": {\n";
        size_t axIdx = 0;
        for (const auto& [name, axis] : _axes) {
            out << "    \"" << name << "\": {\n";
            out << "      \"description\": \"" << axis.description << "\",\n";
            out << "      \"bindings\": [\n";
            for (size_t b = 0; b < axis.bindings.size(); ++b) {
                const auto& bind = axis.bindings[b];
                out << "        { \"device\": " << static_cast<int>(bind.device)
                    << ", \"posKey\": " << bind.positiveKey
                    << ", \"negKey\": " << bind.negativeKey
                    << ", \"gamepadAxis\": " << bind.gamepadAxis
                    << ", \"scale\": " << bind.scale
                    << ", \"deadzone\": " << bind.deadzone
                    << ", \"invert\": " << (bind.invert ? "true" : "false")
                    << " }" << (b + 1 < axis.bindings.size() ? "," : "") << "\n";
            }
            out << "      ]\n    }" << (++axIdx < _axes.size() ? "," : "") << "\n";
        }
        out << "  }\n}\n";
        return true;
    }

    bool InputActionManager::loadFromFile(const std::string& filePath) {
        std::ifstream in(filePath);
        if (!in.is_open()) return false;
        // Simple JSON-like key reader
        std::string line;
        std::string currentSection;
        std::string currentItem;

        while (std::getline(in, line)) {
            size_t posAct = line.find("\"actions\":");
            if (posAct != std::string::npos) { currentSection = "actions"; continue; }
            size_t posAxes = line.find("\"axes\":");
            if (posAxes != std::string::npos) { currentSection = "axes"; continue; }

            // Detect item name "Jump": {
            if (line.find("\": {") != std::string::npos) {
                size_t q1 = line.find('\"');
                size_t q2 = line.find('\"', q1 + 1);
                if (q1 != std::string::npos && q2 != std::string::npos) {
                    currentItem = line.substr(q1 + 1, q2 - q1 - 1);
                    if (currentSection == "actions") registerAction(currentItem);
                    else if (currentSection == "axes") registerAxis(currentItem);
                }
            }

            // Detect action binding
            if (currentSection == "actions" && !currentItem.empty() && line.find("\"device\":") != std::string::npos) {
                size_t devPos = line.find("\"device\":");
                size_t codePos = line.find("\"code\":");
                if (devPos != std::string::npos && codePos != std::string::npos) {
                    int dev = std::atoi(line.substr(devPos + 9).c_str());
                    int code = std::atoi(line.substr(codePos + 7).c_str());
                    if (dev == 0) bindKeyToAction(currentItem, code);
                    else if (dev == 1) bindMouseButtonToAction(currentItem, code);
                    else if (dev == 2) bindGamepadButtonToAction(currentItem, static_cast<GamepadButton>(code));
                }
            }
        }
        return true;
    }

    std::string InputActionManager::keyToString(int key) {
        switch (key) {
            case GLFW_KEY_SPACE: return "Space";
            case GLFW_KEY_ENTER: return "Enter";
            case GLFW_KEY_ESCAPE: return "Escape";
            case GLFW_KEY_TAB: return "Tab";
            case GLFW_KEY_BACKSPACE: return "Backspace";
            case GLFW_KEY_LEFT_SHIFT: return "Left Shift";
            case GLFW_KEY_RIGHT_SHIFT: return "Right Shift";
            case GLFW_KEY_LEFT_CONTROL: return "Left Ctrl";
            case GLFW_KEY_RIGHT_CONTROL: return "Right Ctrl";
            case GLFW_KEY_LEFT_ALT: return "Left Alt";
            case GLFW_KEY_RIGHT_ALT: return "Right Alt";
            case GLFW_KEY_UP: return "Arrow Up";
            case GLFW_KEY_DOWN: return "Arrow Down";
            case GLFW_KEY_LEFT: return "Arrow Left";
            case GLFW_KEY_RIGHT: return "Arrow Right";
            case GLFW_KEY_W: return "W";
            case GLFW_KEY_A: return "A";
            case GLFW_KEY_S: return "S";
            case GLFW_KEY_D: return "D";
            case GLFW_KEY_E: return "E";
            case GLFW_KEY_R: return "R";
            case GLFW_KEY_F: return "F";
            case GLFW_KEY_Q: return "Q";
            case GLFW_KEY_C: return "C";
            case GLFW_KEY_X: return "X";
            case GLFW_KEY_Z: return "Z";
            case GLFW_KEY_1: return "1";
            case GLFW_KEY_2: return "2";
            case GLFW_KEY_3: return "3";
            case GLFW_KEY_4: return "4";
            case GLFW_KEY_5: return "5";
            case GLFW_KEY_6: return "6";
            case GLFW_KEY_7: return "7";
            case GLFW_KEY_8: return "8";
            case GLFW_KEY_9: return "9";
            case GLFW_KEY_0: return "0";
            default:
                if (key >= 32 && key < 127) return std::string(1, static_cast<char>(key));
                return "Key #" + std::to_string(key);
        }
    }

    int InputActionManager::stringToKey(const std::string& str) {
        std::string s = str;
        std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return (char)::tolower(c); });
        if (s == "space") return GLFW_KEY_SPACE;
        if (s == "enter" || s == "return") return GLFW_KEY_ENTER;
        if (s == "escape" || s == "esc") return GLFW_KEY_ESCAPE;
        if (s == "tab") return GLFW_KEY_TAB;
        if (s == "shift" || s == "leftshift" || s == "lshift") return GLFW_KEY_LEFT_SHIFT;
        if (s == "rshift" || s == "rightshift") return GLFW_KEY_RIGHT_SHIFT;
        if (s == "ctrl" || s == "leftctrl" || s == "lctrl") return GLFW_KEY_LEFT_CONTROL;
        if (s == "rctrl" || s == "rightctrl") return GLFW_KEY_RIGHT_CONTROL;
        if (s == "alt") return GLFW_KEY_LEFT_ALT;
        if (s == "up") return GLFW_KEY_UP;
        if (s == "down") return GLFW_KEY_DOWN;
        if (s == "left") return GLFW_KEY_LEFT;
        if (s == "right") return GLFW_KEY_RIGHT;
        if (s.size() == 1) {
            char c = (char)::toupper(s[0]);
            return static_cast<int>(c);
        }
        return 0;
    }

    std::string InputActionManager::gamepadButtonToString(GamepadButton btn) {
        switch (btn) {
            case GamepadButton::A: return "Button A (Cross)";
            case GamepadButton::B: return "Button B (Circle)";
            case GamepadButton::X: return "Button X (Square)";
            case GamepadButton::Y: return "Button Y (Triangle)";
            case GamepadButton::LeftBumper: return "LB (L1)";
            case GamepadButton::RightBumper: return "RB (R1)";
            case GamepadButton::Back: return "Back (Select/Share)";
            case GamepadButton::Start: return "Start (Options)";
            case GamepadButton::Guide: return "Guide (Home)";
            case GamepadButton::LeftThumb: return "Left Stick Click (L3)";
            case GamepadButton::RightThumb: return "Right Stick Click (R3)";
            case GamepadButton::DPadUp: return "D-Pad Up";
            case GamepadButton::DPadRight: return "D-Pad Right";
            case GamepadButton::DPadDown: return "D-Pad Down";
            case GamepadButton::DPadLeft: return "D-Pad Left";
            default: return "Gamepad Button";
        }
    }

    GamepadButton InputActionManager::stringToGamepadButton(const std::string& str) {
        std::string s = str;
        std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return (char)::tolower(c); });
        if (s == "a" || s == "cross" || s == "buttona") return GamepadButton::A;
        if (s == "b" || s == "circle" || s == "buttonb") return GamepadButton::B;
        if (s == "x" || s == "square" || s == "buttonx") return GamepadButton::X;
        if (s == "y" || s == "triangle" || s == "buttony") return GamepadButton::Y;
        if (s == "lb" || s == "l1" || s == "leftbumper") return GamepadButton::LeftBumper;
        if (s == "rb" || s == "r1" || s == "rightbumper") return GamepadButton::RightBumper;
        if (s == "back" || s == "select") return GamepadButton::Back;
        if (s == "start" || s == "options") return GamepadButton::Start;
        if (s == "dpadup" || s == "up") return GamepadButton::DPadUp;
        if (s == "dpaddown" || s == "down") return GamepadButton::DPadDown;
        if (s == "dpadleft" || s == "left") return GamepadButton::DPadLeft;
        if (s == "dpadright" || s == "right") return GamepadButton::DPadRight;
        return GamepadButton::A;
    }

    std::string InputActionManager::gamepadAxisToString(GamepadAxis axis) {
        switch (axis) {
            case GamepadAxis::LeftX: return "Left Stick X";
            case GamepadAxis::LeftY: return "Left Stick Y";
            case GamepadAxis::RightX: return "Right Stick X";
            case GamepadAxis::RightY: return "Right Stick Y";
            case GamepadAxis::LeftTrigger: return "Left Trigger (LT / L2)";
            case GamepadAxis::RightTrigger: return "Right Trigger (RT / R2)";
            default: return "Gamepad Axis";
        }
    }

    GamepadAxis InputActionManager::stringToGamepadAxis(const std::string& str) {
        std::string s = str;
        std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return (char)::tolower(c); });
        if (s == "leftx" || s == "lx") return GamepadAxis::LeftX;
        if (s == "lefty" || s == "ly") return GamepadAxis::LeftY;
        if (s == "rightx" || s == "rx") return GamepadAxis::RightX;
        if (s == "righty" || s == "ry") return GamepadAxis::RightY;
        if (s == "lt" || s == "l2" || s == "lefttrigger") return GamepadAxis::LeftTrigger;
        if (s == "rt" || s == "r2" || s == "righttrigger") return GamepadAxis::RightTrigger;
        return GamepadAxis::LeftX;
    }

    std::string InputActionManager::mouseButtonToString(int button) {
        switch (button) {
            case GLFW_MOUSE_BUTTON_LEFT: return "Left Click";
            case GLFW_MOUSE_BUTTON_RIGHT: return "Right Click";
            case GLFW_MOUSE_BUTTON_MIDDLE: return "Middle Click";
            default: return "Mouse Button #" + std::to_string(button);
        }
    }

    int InputActionManager::stringToMouseButton(const std::string& str) {
        std::string s = str;
        std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return (char)::tolower(c); });
        if (s == "left" || s == "lmb") return GLFW_MOUSE_BUTTON_LEFT;
        if (s == "right" || s == "rmb") return GLFW_MOUSE_BUTTON_RIGHT;
        if (s == "middle" || s == "mmb") return GLFW_MOUSE_BUTTON_MIDDLE;
        return 0;
    }

} // namespace Lab
