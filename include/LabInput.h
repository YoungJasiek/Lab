#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <memory>
#include "LabMath.h"
#include "LabExport.h"

namespace Lab {

    enum class InputDevice {
        Keyboard = 0,
        Mouse = 1,
        Gamepad = 2
    };

    enum class GamepadButton {
        A = 0,
        B = 1,
        X = 2,
        Y = 3,
        LeftBumper = 4,
        RightBumper = 5,
        Back = 6,
        Start = 7,
        Guide = 8,
        LeftThumb = 9,
        RightThumb = 10,
        DPadUp = 11,
        DPadRight = 12,
        DPadDown = 13,
        DPadLeft = 14,
        Count = 15
    };

    enum class GamepadAxis {
        LeftX = 0,
        LeftY = 1,
        RightX = 2,
        RightY = 3,
        LeftTrigger = 4,
        RightTrigger = 5,
        Count = 6
    };

    struct LAB_CORE_API ActionBinding {
        InputDevice device = InputDevice::Keyboard;
        int code = 0;              // GLFW key or mouse button or GamepadButton
        int modifierFlags = 0;     // 1: Shift, 2: Ctrl, 4: Alt
        float scale = 1.0f;        // Multiplier (+1 or -1 for axes)
        float deadzone = 0.15f;    // Threshold for analog sticks
        bool invert = false;
        std::string customLabel = "";
    };

    struct LAB_CORE_API AxisBinding {
        InputDevice device = InputDevice::Keyboard;
        int positiveKey = 0;       // Key for +1.0
        int negativeKey = 0;       // Key for -1.0
        int gamepadAxis = 0;       // GamepadAxis index
        int mouseAxis = 0;         // 0: MouseX, 1: MouseY, 2: Wheel
        float scale = 1.0f;
        float deadzone = 0.15f;
        bool invert = false;
    };

    struct LAB_CORE_API InputAction {
        std::string name;
        std::string description;
        std::vector<ActionBinding> bindings;
        bool isPressed = false;
        bool isJustPressed = false;
        bool isJustReleased = false;
        float value = 0.0f;
    };

    struct LAB_CORE_API InputAxis {
        std::string name;
        std::string description;
        std::vector<AxisBinding> bindings;
        float value = 0.0f;
    };

    struct LAB_CORE_API GamepadStateInfo {
        bool connected = false;
        std::string name = "Disconnected";
        float axes[6] = { 0.0f };
        bool buttons[15] = { false };
    };

    class LAB_CORE_API InputActionManager {
    public:
        static InputActionManager& instance();

        // --- Action Registration ---
        void registerAction(const std::string& name, const std::string& description = "");
        void registerAxis(const std::string& name, const std::string& description = "");
        bool hasAction(const std::string& name) const;
        bool hasAxis(const std::string& name) const;

        // --- Binding Setup ---
        void bindKeyToAction(const std::string& actionName, int key, int mods = 0);
        void bindMouseButtonToAction(const std::string& actionName, int button);
        void bindGamepadButtonToAction(const std::string& actionName, GamepadButton btn);
        void bindKeysToAxis(const std::string& axisName, int positiveKey, int negativeKey, float scale = 1.0f);
        void bindGamepadAxisToAxis(const std::string& axisName, GamepadAxis axis, float scale = 1.0f, float deadzone = 0.15f, bool invert = false);
        void bindMouseToAxis(const std::string& axisName, int mouseAxis, float sensitivity = 1.0f);
        void clearBindings(const std::string& name);

        // --- State Queries ---
        bool isActionPressed(const std::string& name) const;
        bool isActionJustPressed(const std::string& name) const;
        bool isActionJustReleased(const std::string& name) const;
        float getAxis(const std::string& name) const;

        // --- Polling & Update ---
        void update(float dt);

        // --- Event Callbacks ---
        void onAction(const std::string& actionName, std::function<void(bool pressed)> callback);

        // --- Serialization ---
        bool saveToFile(const std::string& filePath) const;
        bool loadFromFile(const std::string& filePath);

        // --- Gamepad Inspection ---
        GamepadStateInfo getGamepadState(int jid = 0) const;

        // --- Registry Access ---
        const std::unordered_map<std::string, InputAction>& getActions() const { return _actions; }
        const std::unordered_map<std::string, InputAxis>& getAxes() const { return _axes; }

        // --- String Helpers for Lua & Editor UI ---
        static std::string keyToString(int key);
        static int stringToKey(const std::string& str);
        static std::string gamepadButtonToString(GamepadButton btn);
        static GamepadButton stringToGamepadButton(const std::string& str);
        static std::string gamepadAxisToString(GamepadAxis axis);
        static GamepadAxis stringToGamepadAxis(const std::string& str);
        static std::string mouseButtonToString(int button);
        static int stringToMouseButton(const std::string& str);

        // Setup standard default game actions
        void setupDefaultBindings();

    private:
        InputActionManager();
        ~InputActionManager() = default;

        std::unordered_map<std::string, InputAction> _actions;
        std::unordered_map<std::string, InputAxis> _axes;
        std::unordered_map<std::string, std::vector<std::function<void(bool)>>> _callbacks;
    };

} // namespace Lab
