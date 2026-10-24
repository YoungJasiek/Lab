#pragma once

#include <string>
#include <vector>
#include <deque>
#include <functional>
#include <unordered_map>
#include <cstdint>
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
        uint32_t drawCalls = 0;
        uint32_t vertexCount = 0;
        size_t ramWorkingSetMB = 0;
        size_t vramTexturesMB = 0;
        std::string closestBotInfo = "";
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

        // Clipboard & Utilities
        void copyLogToClipboard();
        void copyLineToClipboard(int lineIndex);
        const std::deque<DevConsoleEntry>& getEntries() const { return _entries; }

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
        std::string getCvarValue(const std::string& cmd) const;

        // =====================================================================
        // Floating Window Dimensions, Dragging & Resizing (Source Engine VGUI2)
        // =====================================================================
        float windowX = 320.0f;
        float windowY = 120.0f;
        float windowW = 640.0f;
        float windowH = 440.0f;
        bool isDragging = false;
        float dragOffsetX = 0.0f;
        float dragOffsetY = 0.0f;
        bool isResizing = false;
        float resizeStartW = 0.0f;
        float resizeStartH = 0.0f;
        float resizeStartMX = 0.0f;
        float resizeStartMY = 0.0f;
        int lastScreenW = 1280;
        int lastScreenH = 720;

        // =====================================================================
        // Console Variables (CVars) & Toggles from dev_console.md
        // =====================================================================

        // sp_ / sv_ (SUPER / Cheats & Player)
        bool sp_cheats = false;
        bool sv_cheats = false;
        bool isPublicMultiplayerServer = false;
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
        bool wrd_fog = false;
        float wrd_fog_density = 0.02f;
        bool wrd_postprocess = true;
        std::string wrd_skybox = "#0d121a";
        Vec3 wrd_skybox_color{ 0.05f, 0.07f, 0.10f };
        bool wrd_skybox_is_color = true;
        Vec3 wrd_light_ambient{ 0.2f, 0.2f, 0.25f };
        float wrd_gravity = -12.0f;

        // phys_ (Physics & Balanced Bhop tuning)
        float phys_gravity = 12.0f;
        float phys_friction = 5.5f;
        float phys_air_accel = 12.0f;        // Default air acceleration for strafe momentum
        float phys_bhop_window = 0.08f;      // 80ms landing timing window
        float phys_max_bhop_speed = 11.5f;   // Maximum horizontal speed ceiling to prevent runaway momentum
        float phys_step_height = 0.5f;
        float phys_ragdoll_force = 1.0f;
        bool phys_debug = false;

        // cam_ (Camera)
        float cam_fov = 75.0f;
        float cam_viewmodel_fov = 54.0f;
        Vec3 cam_viewmodel_offset{ 0.0f, 0.0f, 0.0f };
        bool cam_thirdperson = false;
        bool cam_bob = true;
        bool cam_freecam = false;

        // r_ (Renderer)
        bool r_bloom = true;
        bool r_fxaa = true;
        bool r_vsync = true;
        bool r_shadows = true;
        int r_shadow_res = 1024;
        bool r_lightmaps = true;
        bool r_normalmaps = true;
        bool r_cubemaps = true;
        bool r_postprocess = true;

        // ai_ (AI)
        bool ai_disable = false;
        bool ai_freeze = false;
        bool ai_sight = false;
        bool ai_paths = false;
        bool ai_aggro = false;
        bool ai_ignore_player = false;

        // snd_ (Audio)
        float snd_volume = 1.0f;
        float snd_music = 0.7f;
        float snd_sfx = 1.0f;
        float snd_voice = 1.0f;

        // ui_ (UI)
        bool ui_hide = false;
        bool ui_crosshair = true;
        std::string ui_crosshair_color = "#00ff00";
        Vec3 ui_crosshair_color_rgb{ 0.0f, 1.0f, 0.0f };
        float ui_scale = 1.0f;

        // dcl_ (Decals)
        int dcl_limit = 256;
        float dcl_distance = 50.0f;

        // mp_ (Multiplayer)
        bool mp_friendly_fire = false;

        // net_ (Network)
        int net_fake_lag = 0;
        float net_fake_loss = 0.0f;

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
        std::function<void(const std::string& skybox)> onSetSkybox;
        std::function<void(const std::string& modelPath)> onSpawnModel;
        std::function<void(float force)> onCamShake;
        std::function<void(bool vsync)> onSetVsync;
        std::function<void()> onReloadShaders;
        std::function<void(int count)> onSetBotsA;
        std::function<void(int count)> onSetBotsB;
        std::function<void()> onKickBots;
        std::function<void(bool freeze)> onFreezeBots;
        std::function<void(const std::string& type)> onSpawnAi;
        std::function<void(float vol)> onSetMasterVolume;
        std::function<void(const std::string& path)> onPlaySound;
        std::function<void()> onStopAllSounds;
        std::function<void(const std::string& file)> onLipsyncTest;
        std::function<void()> onRestartAudio;
        std::function<void(const std::string& menuName)> onOpenMenu;
        std::function<void()> onReloadUi;
        std::function<void()> onClearDecals;
        std::function<void(const std::string& type)> onSpawnDecal;
        std::function<void()> onRestartRound;
        std::function<void(const std::string& addr)> onNetConnect;
        std::function<void()> onNetDisconnect;
        std::function<void()> onQuit;

        // Key bindings
        const std::unordered_map<std::string, std::string>& getKeyBinds() const { return _keyBinds; }
        bool executeBind(const std::string& keyName);

    private:
        void registerKnownCommands();
        void updateAutocomplete();
        void dispatchCommand(const std::string& prefix, const std::string& name, const std::string& args);
        void handleHelpCommand(const std::string& filter);

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
        bool _charHandledThisFrame = false;
        std::unordered_map<std::string, std::string> _keyBinds;
    };

} // namespace Lab
