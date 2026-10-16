#pragma once

#include <string>
#include <vector>
#include <deque>
#include <functional>
#include <unordered_map>
#include "LabMath.h"

namespace Lab {

    struct DevConsoleEntry {
        std::string text;
        Vec3 color{ 0.85f, 0.85f, 0.85f };
    };

    struct DevTelemetryData {
        float currentSpeed = 0.0f;
        float horizontalSpeed = 0.0f;
        Vec3 velocity{ 0.0f, 0.0f, 0.0f };
        Vec3 playerPos{ 0.0f, 0.0f, 0.0f };
        float pitch = 0.0f;
        float yaw = 0.0f;
        float fps = 0.0f;
        float frametimeMs = 0.0f;
        int pingMs = 0;
        int activeBots = 0;
        bool isGrounded = false;
        bool isBhopActive = false;
    };

    class LabDevConsole {
    public:
        LabDevConsole();
        ~LabDevConsole() = default;

        // Visibility & State
        bool isOpen = false;
        void open();
        void close();
        void toggle();

        // Execution & Logging
        void print(const std::string& line, const Vec3& color = Vec3(0.85f, 0.85f, 0.85f));
        void execute(const std::string& commandLine);

        // Input Processing
        bool onKey(int key, int action, bool shift = false, bool ctrl = false);
        void onChar(unsigned int codepoint);
        bool onMouseButton(int button, int action, float mx, float my);
        void onMouseMove(float mx, float my);
        void onMouseScroll(float yoffset);
        void update(float dt);

        // Rendering
        void render(int screenW, int screenH);
        void renderOverlay(int screenW, int screenH, const DevTelemetryData& telemetry);

        // =====================================================================
        // Floating Window Dimensions & Dragging
        // =====================================================================
        float windowX = 180.0f;
        float windowY = 60.0f;
        float windowW = 920.0f;
        float windowH = 580.0f;
        bool isDragging = false;
        float dragOffsetX = 0.0f;
        float dragOffsetY = 0.0f;
        int lastScreenW = 1280;
        int lastScreenH = 720;

        // =====================================================================
        // Console Variables (CVars) & Toggles from dev_console.md
        // =====================================================================

        // sp_ (SUPER / Cheats & Player)
        bool sp_cheats = false;
        bool sp_god = false;
        bool sp_noclip = false;
        bool sp_bhop = true;
        float sp_speed = 1.0f;
        float sp_jump = 5.2f;

        // log_ (HUD 2D Telemetry Overlays)
        bool log_momentum = false;
        bool log_fps = false;
        bool log_ping = false;
        bool log_pos = false;
        bool log_colliders = false;
        bool log_triggers = false;
        bool log_ai = false;
        bool log_drawcalls = false;
        bool log_memory = false;
        bool log_all = false;

        // wrd_ (World, Map & Environment)
        float wrd_timescale = 1.0f;
        bool wrd_wireframe = false;
        bool wrd_fog = true;
        float wrd_fog_density = 0.02f;
        bool wrd_postprocess = true;

        // phys_ (Physics & Balanced Bhop tuning)
        float phys_gravity = 12.0f;
        float phys_friction = 5.5f;
        float phys_air_accel = 12.0f;        // Default air acceleration for strafe momentum
        float phys_bhop_window = 0.08f;      // 80ms landing timing window
        float phys_max_bhop_speed = 11.5f;   // Maximum horizontal speed ceiling to prevent runaway momentum

        // cam_ (Camera)
        float cam_fov = 75.0f;
        bool cam_thirdperson = false;
        bool cam_bob = true;

        // r_ (Renderer)
        bool r_bloom = true;
        bool r_fxaa = true;
        bool r_shadows = true;
        bool r_lightmaps = true;
        bool r_normalmaps = true;
        bool r_cubemaps = true;

        // Callbacks to game systems
        std::function<void(int amount)> onHeal;
        std::function<void(int amount)> onArmor;
        std::function<void(int amount)> onAmmo;
        std::function<void(int weaponId)> onGetWeapon;
        std::function<void()> onGiveAll;
        std::function<void()> onRespawn;
        std::function<void()> onSuicide;
        std::function<void(const std::string& mapName)> onLoadMap;
        std::function<void()> onRestartMap;
        std::function<void()> onReloadShaders;
        std::function<void(int count)> onSetBotsA;
        std::function<void(int count)> onSetBotsB;
        std::function<void()> onKickBots;
        std::function<void(bool freeze)> onFreezeBots;
        std::function<void()> onQuit;

    private:
        void registerKnownCommands();
        void updateAutocomplete();
        void dispatchCommand(const std::string& prefix, const std::string& name, const std::string& args);

        std::string _currentInput;
        int _cursorPos = 0;
        float _cursorBlink = 0.0f;
        int _historyIndex = -1;
        int _scrollOffset = 0;

        std::deque<DevConsoleEntry> _entries;
        std::vector<std::string> _commandHistory;
        std::vector<std::string> _knownCommands;
        std::vector<std::string> _activeSuggestions;
        int _selectedSuggestion = -1;
        int _lastCharHandledKey = -1;
    };

} // namespace Lab
