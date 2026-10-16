#include "LabDevConsole.h"
#include "LabRenderer.h"
#include "LabFont.h"
#include <GLFW/glfw3.h>
#include <sstream>
#include <algorithm>
#include <iomanip>
#include <cctype>

namespace Lab {

    LabDevConsole::LabDevConsole() {
        registerKnownCommands();

        print("Lab Engine Developer Console initialized.", Vec3(0.85f, 0.95f, 0.85f));
        print("Type 'help' for command list. Press [~] or [ESC] to close.", Vec3(0.7f, 0.75f, 0.7f));
    }

    void LabDevConsole::open() {
        isOpen = true;
        _cursorBlink = 0.0f;
        isDragging = false;
        updateAutocomplete();
    }

    void LabDevConsole::close() {
        isOpen = false;
        _currentInput.clear();
        _cursorPos = 0;
        _historyIndex = -1;
        isDragging = false;
        _activeSuggestions.clear();
        _selectedSuggestion = -1;
    }

    void LabDevConsole::toggle() {
        if (isOpen) close();
        else open();
    }

    void LabDevConsole::print(const std::string& line, const Vec3& color) {
        _entries.push_back({ line, color });
        while (_entries.size() > 250) {
            _entries.pop_front();
        }
        _scrollOffset = 0;
    }

    void LabDevConsole::registerKnownCommands() {
        _knownCommands = {
            "sp_cheats", "sp_god", "sp_noclip", "sp_bhop", "sp_speed", "sp_jump",
            "sp_heal", "sp_armor", "sp_ammo", "sp_get", "sp_give_all", "sp_respawn", "sp_suicide",
            "log_momentum", "log_fps", "log_ping", "log_pos", "log_colliders", "log_triggers",
            "log_ai", "log_drawcalls", "log_memory", "log_all",
            "wrd_map", "wrd_restart", "wrd_skybox", "wrd_model", "wrd_timescale", "wrd_wireframe", "wrd_fog", "wrd_postprocess",
            "phys_gravity", "phys_friction", "phys_air_accel", "phys_bhop_window", "phys_max_bhop_speed",
            "cam_fov", "cam_thirdperson", "cam_bob", "cam_shake", "cam_freecam",
            "r_bloom", "r_fxaa", "r_vsync", "r_shadows", "r_lightmaps", "r_normalmaps", "r_cubemaps", "r_reload_shaders",
            "ai_disable", "ai_freeze", "ai_sight", "ai_paths", "ai_aggro", "ai_ignore_player", "ai_spawn",
            "snd_volume", "snd_music", "snd_sfx", "snd_voice", "snd_play", "snd_stop_all", "snd_lipsync_test", "snd_restart",
            "ui_hide", "ui_crosshair", "ui_scale", "ui_reload",
            "dcl_clear", "dcl_limit", "dcl_spawn", "dcl_distance",
            "mp_a_bots", "mp_b_bots", "mp_bot_kick_all", "mp_bot_freeze", "mp_restart",
            "net_status", "net_fake_lag", "net_fake_loss", "net_connect", "net_disconnect",
            "sys_clear", "sys_help", "sys_bind", "sys_quit"
        };
    }

    void LabDevConsole::updateAutocomplete() {
        _activeSuggestions.clear();
        _selectedSuggestion = -1;
        if (_currentInput.empty()) return;

        std::string query = _currentInput;
        std::transform(query.begin(), query.end(), query.begin(), [](unsigned char c) { return (char)std::tolower(c); });

        // Only search first token
        size_t spacePos = query.find(' ');
        std::string token = (spacePos != std::string::npos) ? query.substr(0, spacePos) : query;

        for (const auto& cmd : _knownCommands) {
            if (cmd.find(token) == 0) { // prefix match
                _activeSuggestions.push_back(cmd);
                if (_activeSuggestions.size() >= 5) break;
            }
        }
        if (!_activeSuggestions.empty()) {
            _selectedSuggestion = 0;
        }
    }

    void LabDevConsole::execute(const std::string& rawLine) {
        size_t start = rawLine.find_first_not_of(" \t\r\n");
        if (start == std::string::npos) return;
        size_t end = rawLine.find_last_not_of(" \t\r\n");
        std::string line = rawLine.substr(start, end - start + 1);
        if (line.empty()) return;

        // Add to history
        _commandHistory.push_back(line);
        _historyIndex = -1;

        // Print executed command with ']' prompt
        print("] " + line, Vec3(0.95f, 0.95f, 0.95f));

        // Tokenize command and arguments
        std::istringstream iss(line);
        std::string cmd;
        iss >> cmd;

        std::string args;
        std::getline(iss, args);
        size_t argStart = args.find_first_not_of(" \t");
        if (argStart != std::string::npos) {
            args = args.substr(argStart);
        } else {
            args.clear();
        }

        // Check for prefix underscore
        size_t underscore = cmd.find('_');
        if (underscore != std::string::npos) {
            std::string prefix = cmd.substr(0, underscore);
            std::string name = cmd.substr(underscore + 1);
            dispatchCommand(prefix, name, args);
        } else {
            // Standalone or Valve compatibility alias
            if (cmd == "god") dispatchCommand("sp", "god", args);
            else if (cmd == "noclip") dispatchCommand("sp", "noclip", args);
            else if (cmd == "bhop") dispatchCommand("sp", "bhop", args);
            else if (cmd == "clear") dispatchCommand("sys", "clear", args);
            else if (cmd == "help") dispatchCommand("sys", "help", args);
            else if (cmd == "quit" || cmd == "exit") dispatchCommand("sys", "quit", args);
            else dispatchCommand("sys", cmd, args);
        }

        updateAutocomplete();
    }

    void LabDevConsole::dispatchCommand(const std::string& prefix, const std::string& name, const std::string& args) {
        std::string p = prefix;
        std::string n = name;
        std::transform(p.begin(), p.end(), p.begin(), [](unsigned char c) { return (char)std::tolower(c); });
        std::transform(n.begin(), n.end(), n.begin(), [](unsigned char c) { return (char)std::tolower(c); });

        auto parseBool = [](const std::string& a) -> int {
            if (a.empty()) return -1;
            if (a == "1" || a == "true" || a == "on") return 1;
            if (a == "0" || a == "false" || a == "off") return 0;
            return -1;
        };

        // =====================================================================
        // SP_ (SUPER / CHEATS & PLAYER)
        // =====================================================================
        if (p == "sp" || p == "sv") {
            if (n == "cheats") {
                int b = parseBool(args);
                if (b >= 0) sp_cheats = (b == 1);
                print(std::string("sp_cheats set to ") + (sp_cheats ? "1" : "0"),
                      sp_cheats ? Vec3(0.3f, 0.95f, 0.4f) : Vec3(1.0f, 0.4f, 0.4f));
                return;
            }

            // Commands below require sp_cheats 1 warning
            if (!sp_cheats) {
                print("Can't use cheat cvar " + p + "_" + n + " in multiplayer, unless the server has sp_cheats set to 1.",
                      Vec3(1.0f, 0.35f, 0.35f));
                return;
            }

            if (n == "god") {
                int b = parseBool(args);
                sp_god = (b >= 0) ? (b == 1) : !sp_god;
                print(std::string("godmode ") + (sp_god ? "ON" : "OFF"), Vec3(0.9f, 0.9f, 0.3f));
            } else if (n == "noclip") {
                int b = parseBool(args);
                sp_noclip = (b >= 0) ? (b == 1) : !sp_noclip;
                print(std::string("noclip ") + (sp_noclip ? "ON" : "OFF"), Vec3(0.9f, 0.9f, 0.3f));
            } else if (n == "bhop") {
                int b = parseBool(args);
                sp_bhop = (b >= 0) ? (b == 1) : !sp_bhop;
                print(std::string("bhop ") + (sp_bhop ? "ENABLED" : "DISABLED"), Vec3(0.9f, 0.9f, 0.3f));
            } else if (n == "speed") {
                try {
                    float s = std::stof(args);
                    sp_speed = std::clamp(s, 0.1f, 10.0f);
                    print("sp_speed = " + std::to_string(sp_speed), Vec3(0.9f, 0.9f, 0.9f));
                } catch (...) {
                    print("sp_speed currently: " + std::to_string(sp_speed), Vec3(0.7f, 0.7f, 0.7f));
                }
            } else if (n == "jump") {
                try {
                    float j = std::stof(args);
                    sp_jump = std::clamp(j, 1.0f, 30.0f);
                    print("sp_jump = " + std::to_string(sp_jump), Vec3(0.9f, 0.9f, 0.9f));
                } catch (...) {
                    print("sp_jump currently: " + std::to_string(sp_jump), Vec3(0.7f, 0.7f, 0.7f));
                }
            } else if (n == "heal") {
                int val = 100;
                try { if (!args.empty()) val = std::stoi(args); } catch (...) {}
                if (onHeal) onHeal(val);
                print("Giving player " + std::to_string(val) + " health", Vec3(0.3f, 0.95f, 0.4f));
            } else if (n == "armor") {
                int val = 100;
                try { if (!args.empty()) val = std::stoi(args); } catch (...) {}
                if (onArmor) onArmor(val);
                print("Giving player " + std::to_string(val) + " suit armor", Vec3(0.3f, 0.85f, 1.0f));
            } else if (n == "ammo") {
                int val = 250;
                try { if (!args.empty()) val = std::stoi(args); } catch (...) {}
                if (onAmmo) onAmmo(val);
                print("Ammo replenished (" + std::to_string(val) + " rounds)", Vec3(1.0f, 0.9f, 0.2f));
            } else if (n == "get" || n == "give") {
                try {
                    int wId = std::stoi(args);
                    if (onGetWeapon) onGetWeapon(wId);
                    print("Acquired weapon ID #" + std::to_string(wId), Vec3(0.3f, 0.9f, 1.0f));
                } catch (...) {
                    print("Usage: sp_get <weapon_id> (101..109)", Vec3(1.0f, 0.4f, 0.4f));
                }
            } else if (n == "give_all" || n == "impulse101") {
                if (onGiveAll) onGiveAll();
                print("All weapons and maximum ammunition unlocked!", Vec3(0.3f, 0.95f, 0.4f));
            } else if (n == "respawn") {
                if (onRespawn) onRespawn();
                print("Respawning player...", Vec3(0.85f, 0.85f, 0.85f));
            } else if (n == "suicide" || n == "kill") {
                if (onSuicide) onSuicide();
                print("Player killed.", Vec3(1.0f, 0.35f, 0.35f));
            } else {
                print("Unknown command '" + p + "_" + n + "'", Vec3(1.0f, 0.35f, 0.35f));
            }
            return;
        }

        // =====================================================================
        // LOG_ (HUD 2D TELEMETRY OVERLAYS)
        // =====================================================================
        if (p == "log") {
            auto toggleFlag = [&](bool& flag, const std::string& label) {
                int b = parseBool(args);
                flag = (b >= 0) ? (b == 1) : !flag;
                print(label + " = " + (flag ? "1" : "0"), Vec3(0.85f, 0.85f, 0.85f));
            };

            if (n == "momentum") toggleFlag(log_momentum, "log_momentum");
            else if (n == "fps") toggleFlag(log_fps, "log_fps");
            else if (n == "ping") toggleFlag(log_ping, "log_ping");
            else if (n == "pos") toggleFlag(log_pos, "log_pos");
            else if (n == "colliders") toggleFlag(log_colliders, "log_colliders");
            else if (n == "triggers") toggleFlag(log_triggers, "log_triggers");
            else if (n == "ai") toggleFlag(log_ai, "log_ai");
            else if (n == "all") {
                int b = parseBool(args);
                bool state = (b >= 0) ? (b == 1) : !log_all;
                log_all = state;
                log_momentum = state;
                log_fps = state;
                log_pos = state;
                log_ping = state;
                print(std::string("log_all = ") + (state ? "1" : "0"), Vec3(0.85f, 0.85f, 0.85f));
            } else {
                print("Unknown overlay cvar 'log_" + n + "'", Vec3(1.0f, 0.35f, 0.35f));
            }
            return;
        }

        // =====================================================================
        // WRD_ (WORLD, MAP, ENVIRONMENT)
        // =====================================================================
        if (p == "wrd") {
            if (n == "map") {
                if (onLoadMap && !args.empty()) {
                    onLoadMap(args);
                    print("Loading scene " + args, Vec3(0.85f, 0.85f, 0.85f));
                } else {
                    print("Usage: wrd_map <scene_name>", Vec3(1.0f, 0.4f, 0.4f));
                }
            } else if (n == "restart") {
                if (onRestartMap) onRestartMap();
                print("Reloading current map...", Vec3(0.85f, 0.85f, 0.85f));
            } else if (n == "timescale") {
                try {
                    float ts = std::stof(args);
                    wrd_timescale = std::clamp(ts, 0.05f, 10.0f);
                    print("wrd_timescale = " + std::to_string(wrd_timescale), Vec3(0.85f, 0.85f, 0.85f));
                } catch (...) {
                    print("wrd_timescale currently: " + std::to_string(wrd_timescale), Vec3(0.7f, 0.7f, 0.7f));
                }
            } else if (n == "wireframe") {
                int b = parseBool(args);
                wrd_wireframe = (b >= 0) ? (b == 1) : !wrd_wireframe;
                print(std::string("wrd_wireframe = ") + (wrd_wireframe ? "1" : "0"), Vec3(0.85f, 0.85f, 0.85f));
            } else if (n == "fog") {
                int b = parseBool(args);
                wrd_fog = (b >= 0) ? (b == 1) : !wrd_fog;
                print(std::string("wrd_fog = ") + (wrd_fog ? "1" : "0"), Vec3(0.85f, 0.85f, 0.85f));
            } else {
                print("Unknown world command 'wrd_" + n + "'", Vec3(1.0f, 0.35f, 0.35f));
            }
            return;
        }

        // =====================================================================
        // PHYS_ (PHYSICS & BALANCED BHOP TUNING)
        // =====================================================================
        if (p == "phys") {
            if (n == "friction") {
                try {
                    phys_friction = std::stof(args);
                    print("phys_friction = " + std::to_string(phys_friction), Vec3(0.85f, 0.85f, 0.85f));
                } catch (...) {}
            } else if (n == "air_accel") {
                try {
                    phys_air_accel = std::stof(args);
                    print("phys_air_accel = " + std::to_string(phys_air_accel), Vec3(0.85f, 0.85f, 0.85f));
                } catch (...) {}
            } else if (n == "bhop_window") {
                try {
                    phys_bhop_window = std::stof(args) / 1000.0f; // ms to s
                    print("phys_bhop_window = " + std::to_string((int)(phys_bhop_window * 1000.0f)) + " ms", Vec3(0.85f, 0.85f, 0.85f));
                } catch (...) {}
            } else if (n == "max_bhop_speed") {
                try {
                    phys_max_bhop_speed = std::stof(args);
                    print("phys_max_bhop_speed = " + std::to_string(phys_max_bhop_speed), Vec3(0.85f, 0.85f, 0.85f));
                } catch (...) {
                    print("phys_max_bhop_speed currently: " + std::to_string(phys_max_bhop_speed), Vec3(0.7f, 0.7f, 0.7f));
                }
            } else if (n == "gravity") {
                try {
                    phys_gravity = std::stof(args);
                    print("phys_gravity = " + std::to_string(phys_gravity), Vec3(0.85f, 0.85f, 0.85f));
                } catch (...) {}
            } else {
                print("Unknown physics command 'phys_" + n + "'", Vec3(1.0f, 0.35f, 0.35f));
            }
            return;
        }

        // =====================================================================
        // CAM_ (CAMERA & VIEW)
        // =====================================================================
        if (p == "cam") {
            if (n == "fov") {
                try {
                    float f = std::stof(args);
                    cam_fov = std::clamp(f, 45.0f, 140.0f);
                    print("cam_fov = " + std::to_string((int)cam_fov), Vec3(0.85f, 0.85f, 0.85f));
                } catch (...) {}
            } else if (n == "thirdperson") {
                int b = parseBool(args);
                cam_thirdperson = (b >= 0) ? (b == 1) : !cam_thirdperson;
                print(std::string("cam_thirdperson = ") + (cam_thirdperson ? "1" : "0"), Vec3(0.85f, 0.85f, 0.85f));
            } else {
                print("Unknown camera command 'cam_" + n + "'", Vec3(1.0f, 0.35f, 0.35f));
            }
            return;
        }

        // =====================================================================
        // R_ (RENDERER)
        // =====================================================================
        if (p == "r") {
            if (n == "bloom") {
                int b = parseBool(args);
                r_bloom = (b >= 0) ? (b == 1) : !r_bloom;
                print(std::string("r_bloom = ") + (r_bloom ? "1" : "0"), Vec3(0.85f, 0.85f, 0.85f));
            } else if (n == "fxaa") {
                int b = parseBool(args);
                r_fxaa = (b >= 0) ? (b == 1) : !r_fxaa;
                print(std::string("r_fxaa = ") + (r_fxaa ? "1" : "0"), Vec3(0.85f, 0.85f, 0.85f));
            } else if (n == "reload_shaders") {
                if (onReloadShaders) onReloadShaders();
                print("Reloading all shaders...", Vec3(0.3f, 0.95f, 0.4f));
            } else {
                print("Unknown renderer command 'r_" + n + "'", Vec3(1.0f, 0.35f, 0.35f));
            }
            return;
        }

        // =====================================================================
        // MP_ & AI_ (MULTIPLAYER & BOTS)
        // =====================================================================
        if (p == "mp" || p == "ai") {
            if (n == "a_bots" || n == "b_bots") {
                try {
                    int c = std::stoi(args);
                    if (n == "a_bots" && onSetBotsA) onSetBotsA(c);
                    if (n == "b_bots" && onSetBotsB) onSetBotsB(c);
                    print("Set bot count to " + std::to_string(c), Vec3(0.85f, 0.85f, 0.85f));
                } catch (...) {}
            } else if (n == "bot_kick_all" || n == "kick_all") {
                if (onKickBots) onKickBots();
                print("Kicked all bots.", Vec3(1.0f, 0.4f, 0.4f));
            } else if (n == "freeze" || n == "bot_freeze") {
                int b = parseBool(args);
                bool freeze = (b >= 0) ? (b == 1) : true;
                if (onFreezeBots) onFreezeBots(freeze);
                print(std::string("bot_freeze = ") + (freeze ? "1" : "0"), Vec3(0.85f, 0.85f, 0.85f));
            } else {
                print("Unknown AI/MP command", Vec3(1.0f, 0.35f, 0.35f));
            }
            return;
        }

        // =====================================================================
        // SYS_ (SYSTEM & CONSOLE)
        // =====================================================================
        if (p == "sys" || p.empty()) {
            if (n == "clear") {
                _entries.clear();
                return;
            } else if (n == "quit" || n == "exit") {
                if (onQuit) onQuit();
                return;
            } else if (n == "help") {
                print("=== LAB ENGINE DEVELOPER COMMAND LIST ===", Vec3(0.9f, 0.85f, 0.3f));
                print("  sp_cheats 0/1          - Unlock developer cheats", Vec3(0.85f, 0.85f, 0.85f));
                print("  sp_god, sp_noclip      - Invincibility and ghost fly", Vec3(0.85f, 0.85f, 0.85f));
                print("  sp_bhop 0/1            - Toggle bunny hop physics", Vec3(0.85f, 0.85f, 0.85f));
                print("  sp_give_all, sp_get    - Weapon acquisition", Vec3(0.85f, 0.85f, 0.85f));
                print("  log_momentum, log_fps  - 2D HUD telemetry diagnostics", Vec3(0.85f, 0.85f, 0.85f));
                print("  phys_max_bhop_speed    - Bhop speed ceiling cap", Vec3(0.85f, 0.85f, 0.85f));
                print("  phys_air_accel, phys_friction - Air strafe & ground friction tuning", Vec3(0.85f, 0.85f, 0.85f));
                print("  wrd_map, wrd_restart   - Level loading", Vec3(0.85f, 0.85f, 0.85f));
                return;
            }
        }

        print("Unknown command \"" + prefix + "_" + name + "\"", Vec3(1.0f, 0.35f, 0.35f));
    }

    bool LabDevConsole::onMouseButton(int button, int action, float mx, float my) {
        if (!isOpen) return false;

        float titleH = 26.0f;
        float inputAreaH = 34.0f;
        float textX = windowX + 8.0f;
        float inputY = windowY + windowH - inputAreaH + 2.0f;
        float submitW = 76.0f;
        float inputW = windowW - submitW - 24.0f;
        float submitX = textX + inputW + 8.0f;

        float popW = 340.0f;
        float popItemH = 19.0f;
        float popH = (float)_activeSuggestions.size() * popItemH + 6.0f;
        float popY = inputY + 27.0f;

        // Check if click is inside the console window
        bool insideWindow = (mx >= windowX && mx <= windowX + windowW &&
                             my >= windowY && my <= windowY + windowH);

        // Check suggestion popup click (if rendered below input)
        bool insidePopup = (!_activeSuggestions.empty() &&
                            mx >= windowX + 8.0f && mx <= windowX + 8.0f + popW &&
                            my >= popY && my <= popY + popH);

        if (!insideWindow && !insidePopup) {
            if (action == 1) { // Click outside
                isDragging = false;
            }
            return false;
        }

        if (button == 0) { // Left Mouse Button
            if (action == 1) { // Pressed
                // 1. Close button [X]
                float closeBtnX = windowX + windowW - 22.0f;
                float closeBtnY = windowY + 5.0f;
                if (mx >= closeBtnX && mx <= closeBtnX + 16.0f && my >= closeBtnY && my <= closeBtnY + 16.0f) {
                    close();
                    return true;
                }

                // 2. Submit Button
                if (mx >= submitX && mx <= submitX + submitW && my >= inputY && my <= inputY + 24.0f) {
                    if (!_currentInput.empty()) {
                        execute(_currentInput);
                        _currentInput.clear();
                        _cursorPos = 0;
                    }
                    return true;
                }

                // 2b. Input Box Click (Focus & place cursor at end)
                if (mx >= textX && mx <= textX + inputW && my >= inputY && my <= inputY + 24.0f) {
                    _cursorPos = static_cast<int>(_currentInput.size());
                    return true;
                }

                // 3. Autocomplete popup click
                if (insidePopup) {
                    int clickedIdx = static_cast<int>((my - (popY + 4.0f)) / popItemH);
                    if (clickedIdx >= 0 && clickedIdx < static_cast<int>(_activeSuggestions.size())) {
                        _currentInput = _activeSuggestions[clickedIdx] + " ";
                        _cursorPos = static_cast<int>(_currentInput.size());
                        _selectedSuggestion = -1;
                        updateAutocomplete();
                        return true;
                    }
                }

                // 4. Title bar dragging
                if (my >= windowY && my <= windowY + titleH) {
                    isDragging = true;
                    dragOffsetX = mx - windowX;
                    dragOffsetY = my - windowY;
                    return true;
                }

                // 5. Scrollbar click
                float textW = windowW - 16.0f;
                float textH = windowH - titleH - inputAreaH - 14.0f;
                float textY = windowY + titleH + 6.0f;
                float sbW = 14.0f;
                float sbX = textX + textW - sbW - 2.0f;
                float sbY = textY + 2.0f;
                float sbH = textH - 4.0f;
                if (mx >= sbX && mx <= sbX + sbW && my >= sbY && my <= sbY + sbH) {
                    float clickRatio = (my - sbY) / sbH;
                    int total = static_cast<int>(_entries.size());
                    float lineH = 17.0f;
                    int maxLines = static_cast<int>(textH / lineH) - 1;
                    int maxScroll = std::max(0, total - maxLines);
                    _scrollOffset = static_cast<int>((1.0f - clickRatio) * static_cast<float>(maxScroll));
                    _scrollOffset = std::clamp(_scrollOffset, 0, maxScroll);
                    return true;
                }
            } else if (action == 0) { // Released
                isDragging = false;
            }
        }

        return true;
    }

    void LabDevConsole::onMouseMove(float mx, float my) {
        if (!isOpen) return;
        if (isDragging) {
            windowX = mx - dragOffsetX;
            windowY = my - dragOffsetY;
            windowX = std::clamp(windowX, 0.0f, 1920.0f - windowW);
            windowY = std::clamp(windowY, 0.0f, 1080.0f - windowH);
        }
    }

    void LabDevConsole::onMouseScroll(float yoffset) {
        if (!isOpen) return;
        int maxScroll = std::max(0, (int)_entries.size() - 20);
        _scrollOffset = std::clamp(_scrollOffset + (int)(yoffset * 3.0f), 0, maxScroll);
    }

    bool LabDevConsole::onKey(int key, int action, bool shift, bool ctrl) {
        if (!isOpen) return false;
        if (action != 1 && action != 2) return false; // Press or Repeat

        // Tilde (`~`, key 96) or Escape (256) closes console
        if (key == 96 || key == 256) {
            close();
            return true;
        }

        // Clipboard: Ctrl+V (Paste)
        if (ctrl && (key == 86 || key == 'v' || key == 'V')) { // GLFW_KEY_V = 86
            GLFWwindow* win = glfwGetCurrentContext();
            if (win) {
                const char* clip = glfwGetClipboardString(win);
                if (clip) {
                    for (const char* p = clip; *p; ++p) {
                        if (*p >= 32 && *p < 127) {
                            _currentInput.insert(_currentInput.begin() + _cursorPos, *p);
                            _cursorPos++;
                        }
                    }
                    _selectedSuggestion = -1;
                    updateAutocomplete();
                }
            }
            return true;
        }

        // Clipboard: Ctrl+C (Copy)
        if (ctrl && (key == 67 || key == 'c' || key == 'C')) { // GLFW_KEY_C = 67
            if (!_currentInput.empty()) {
                GLFWwindow* win = glfwGetCurrentContext();
                if (win) {
                    glfwSetClipboardString(win, _currentInput.c_str());
                }
            }
            return true;
        }

        // Tab: Autocomplete selected suggestion
        if (key == 258) { // GLFW_KEY_TAB
            if (!_activeSuggestions.empty()) {
                int idx = (_selectedSuggestion >= 0 && _selectedSuggestion < static_cast<int>(_activeSuggestions.size()))
                          ? _selectedSuggestion : 0;
                _currentInput = _activeSuggestions[idx] + " ";
                _cursorPos = static_cast<int>(_currentInput.size());
                _selectedSuggestion = -1;
                updateAutocomplete();
                return true;
            }
        }

        // Enter: execute command
        if (key == 257 || key == 335) { // GLFW_KEY_ENTER or Keypad Enter
            if (!_currentInput.empty()) {
                execute(_currentInput);
                _currentInput.clear();
                _cursorPos = 0;
                _selectedSuggestion = -1;
            }
            return true;
        }

        // Backspace
        if (key == 259) {
            if (!_currentInput.empty() && _cursorPos > 0) {
                _currentInput.erase(_cursorPos - 1, 1);
                _cursorPos--;
                _selectedSuggestion = -1;
                updateAutocomplete();
            }
            return true;
        }

        // Delete
        if (key == 261) {
            if (_cursorPos < static_cast<int>(_currentInput.size())) {
                _currentInput.erase(_cursorPos, 1);
                _selectedSuggestion = -1;
                updateAutocomplete();
            }
            return true;
        }

        // Left Arrow
        if (key == 263) {
            if (_cursorPos > 0) _cursorPos--;
            return true;
        }

        // Right Arrow
        if (key == 262) {
            if (_cursorPos < static_cast<int>(_currentInput.size())) _cursorPos++;
            return true;
        }

        // Home
        if (key == 268) {
            _cursorPos = 0;
            return true;
        }

        // End
        if (key == 269) {
            _cursorPos = static_cast<int>(_currentInput.size());
            return true;
        }

        // Up Arrow: Suggestions or Command History backward
        if (key == 265) {
            if (!_activeSuggestions.empty() && _selectedSuggestion > 0) {
                _selectedSuggestion--;
            } else if (!_activeSuggestions.empty() && _selectedSuggestion == 0) {
                _selectedSuggestion = -1;
            } else if (!_commandHistory.empty()) {
                if (_historyIndex == -1) {
                    _historyIndex = static_cast<int>(_commandHistory.size()) - 1;
                } else if (_historyIndex > 0) {
                    _historyIndex--;
                }
                _currentInput = _commandHistory[_historyIndex];
                _cursorPos = static_cast<int>(_currentInput.size());
                _selectedSuggestion = -1;
                updateAutocomplete();
            }
            return true;
        }

        // Down Arrow: Suggestions or Command History forward
        if (key == 264) {
            if (!_activeSuggestions.empty() && _selectedSuggestion < static_cast<int>(_activeSuggestions.size()) - 1) {
                _selectedSuggestion++;
            } else if (!_commandHistory.empty() && _historyIndex != -1) {
                if (_historyIndex < static_cast<int>(_commandHistory.size()) - 1) {
                    _historyIndex++;
                    _currentInput = _commandHistory[_historyIndex];
                } else {
                    _historyIndex = -1;
                    _currentInput.clear();
                }
                _cursorPos = static_cast<int>(_currentInput.size());
                _selectedSuggestion = -1;
                updateAutocomplete();
            }
            return true;
        }

        // PageUp / PageDown: scroll output
        if (key == 266) { // PageUp
            int maxScroll = std::max(0, static_cast<int>(_entries.size()) - 15);
            _scrollOffset = std::min(maxScroll, _scrollOffset + 5);
            return true;
        }
        if (key == 267) { // PageDown
            _scrollOffset = std::max(0, _scrollOffset - 5);
            return true;
        }

        // Alphanumeric and Printable Characters
        char c = '\0';
        if (key >= 65 && key <= 90) { // A-Z
            c = shift ? static_cast<char>(key) : static_cast<char>(key + 32);
        } else if (key >= 48 && key <= 57) { // 0-9
            if (shift) {
                const char shiftNums[] = ")!@#$%^&*(";
                c = shiftNums[key - 48];
            } else {
                c = static_cast<char>(key);
            }
        } else if (key >= 320 && key <= 329) { // Keypad 0-9
            c = static_cast<char>('0' + (key - 320));
        } else if (key == 32) { // Space
            c = ' ';
        } else if (key == 45) { // Minus / Underscore
            c = shift ? '_' : '-';
        } else if (key == 333) { // Keypad Minus
            c = '-';
        } else if (key == 61) { // Equal / Plus
            c = shift ? '+' : '=';
        } else if (key == 334) { // Keypad Plus
            c = '+';
        } else if (key == 46 || key == 330) { // Period / Keypad Period
            c = shift ? '>' : '.';
        } else if (key == 47 || key == 331) { // Slash / Keypad Slash
            c = shift ? '?' : '/';
        } else if (key == 44) { // Comma
            c = shift ? '<' : ',';
        } else if (key == 59) { // Semicolon
            c = shift ? ':' : ';';
        } else if (key == 39) { // Apostrophe
            c = shift ? '"' : '\'';
        } else if (key == 91) { // Left bracket
            c = shift ? '{' : '[';
        } else if (key == 93) { // Right bracket
            c = shift ? '}' : ']';
        } else if (key == 92) { // Backslash
            c = shift ? '|' : '\\';
        }

        if (c != '\0') {
            _currentInput.insert(_currentInput.begin() + _cursorPos, c);
            _cursorPos++;
            _lastCharHandledKey = static_cast<int>(c);
            _selectedSuggestion = -1;
            updateAutocomplete();
            return true;
        }

        return false;
    }

    void LabDevConsole::onChar(unsigned int codepoint) {
        if (!isOpen) return;
        if (codepoint == '`' || codepoint == '~') return;

        // Prevent double typing if onKey already processed this character
        if (_lastCharHandledKey == static_cast<int>(codepoint)) {
            _lastCharHandledKey = -1;
            return;
        }
        _lastCharHandledKey = -1;

        if (codepoint >= 32 && codepoint < 127) {
            _currentInput.insert(_currentInput.begin() + _cursorPos, static_cast<char>(codepoint));
            _cursorPos++;
            _selectedSuggestion = -1;
            updateAutocomplete();
        }
    }

    void LabDevConsole::update(float dt) {
        _cursorBlink += dt * 4.0f;
    }

    void LabDevConsole::render(int screenW, int screenH) {
        if (!isOpen) return;

        Renderer::beginUI(screenW, screenH);

        // Clamp window within screen viewport
        windowX = std::clamp(windowX, 10.0f, (float)screenW - windowW - 10.0f);
        windowY = std::clamp(windowY, 10.0f, (float)screenH - windowH - 10.0f);

        float titleH = 26.0f;
        float inputAreaH = 34.0f;

        // ==================== CONSOLE THEME COLOR PALETTE ====================
        Vec3 frameBg(0.28f, 0.33f, 0.27f);      // #475445 (Muted olive-green frame)
        Vec3 titleBg(0.24f, 0.28f, 0.23f);      // #3d473b (Darker olive title bar)
        Vec3 borderLight(0.38f, 0.44f, 0.36f);  // 3D Bevel highlight
        Vec3 borderDark(0.15f, 0.18f, 0.14f);   // 3D Bevel shadow
        Vec3 textAreaBg(0.14f, 0.16f, 0.13f);   // #242921 (Dark text area)
        Vec3 inputBg(0.18f, 0.21f, 0.17f);      // #2e362b (Input box)

        // 1. Window Outer Frame & 3D Bevel
        Renderer::drawRect(windowX, windowY, windowW, windowH, frameBg);
        Renderer::drawRect(windowX, windowY, windowW, 1.0f, borderLight);
        Renderer::drawRect(windowX, windowY, 1.0f, windowH, borderLight);
        Renderer::drawRect(windowX, windowY + windowH - 1.0f, windowW, 1.0f, borderDark);
        Renderer::drawRect(windowX + windowW - 1.0f, windowY, 1.0f, windowH, borderDark);

        // 2. Title Bar
        Renderer::drawRect(windowX + 2.0f, windowY + 2.0f, windowW - 4.0f, titleH, titleBg);
        LabFont::drawText(windowX + 10.0f, windowY + 6.0f, "Console", 1.25f, Vec3(0.92f, 0.95f, 0.92f), LabFontType::GeoSans);

        // Close Button [X] on top right
        float closeBtnX = windowX + windowW - 22.0f;
        float closeBtnY = windowY + 5.0f;
        Renderer::drawRect(closeBtnX, closeBtnY, 16.0f, 16.0f, Vec3(0.32f, 0.37f, 0.30f));
        Renderer::drawRect(closeBtnX, closeBtnY, 16.0f, 1.0f, borderLight);
        Renderer::drawRect(closeBtnX, closeBtnY + 15.0f, 16.0f, 1.0f, borderDark);
        LabFont::drawText(closeBtnX + 4.0f, closeBtnY + 2.0f, "x", 1.1f, Vec3(0.85f, 0.9f, 0.85f), LabFontType::GeoSans);

        // 3. Inner Log Text Area (Sunken)
        float textX = windowX + 8.0f;
        float textY = windowY + titleH + 6.0f;
        float textW = windowW - 16.0f;
        float textH = windowH - titleH - inputAreaH - 14.0f;

        Renderer::drawRect(textX, textY, textW, textH, textAreaBg);
        // Sunken borders
        Renderer::drawRect(textX, textY, textW, 1.0f, borderDark);
        Renderer::drawRect(textX, textY, 1.0f, textH, borderDark);
        Renderer::drawRect(textX, textY + textH - 1.0f, textW, 1.0f, borderLight);
        Renderer::drawRect(textX + textW - 1.0f, textY, 1.0f, textH, borderLight);

        // Render visible text lines with clipping
        float lineH = 17.0f;
        int maxLines = (int)(textH / lineH) - 1;
        int total = (int)_entries.size();
        int endIdx = total - _scrollOffset;
        int startIdx = std::max(0, endIdx - maxLines);

        float curY = textY + 6.0f;
        for (int i = startIdx; i < endIdx && i < total; ++i) {
            const auto& entry = _entries[i];
            LabFont::drawText(textX + 8.0f, curY, entry.text, 1.08f, entry.color, LabFontType::GeoSans);
            curY += lineH;
        }

        // Vertical Scrollbar on the right of text area
        float sbW = 14.0f;
        float sbX = textX + textW - sbW - 2.0f;
        float sbY = textY + 2.0f;
        float sbH = textH - 4.0f;
        Renderer::drawRect(sbX, sbY, sbW, sbH, Vec3(0.11f, 0.13f, 0.10f));

        if (total > maxLines && total > 0) {
            float thumbRatio = std::clamp((float)maxLines / (float)total, 0.15f, 1.0f);
            float thumbH = sbH * thumbRatio;
            int maxScroll = total - maxLines;
            float scrollProgress = (maxScroll > 0) ? (float)(maxScroll - _scrollOffset) / (float)maxScroll : 1.0f;
            float thumbY = sbY + (sbH - thumbH) * scrollProgress;

            Renderer::drawRect(sbX + 1.0f, thumbY, sbW - 2.0f, thumbH, Vec3(0.35f, 0.40f, 0.33f));
            Renderer::drawRect(sbX + 1.0f, thumbY, sbW - 2.0f, 1.0f, borderLight);
            Renderer::drawRect(sbX + 1.0f, thumbY + thumbH - 1.0f, sbW - 2.0f, 1.0f, borderDark);
        }

        // 4. Command Input Field (Sunken)
        float inputY = windowY + windowH - inputAreaH + 2.0f;
        float submitW = 76.0f;
        float inputW = windowW - submitW - 24.0f;

        Renderer::drawRect(textX, inputY, inputW, 24.0f, inputBg);
        Renderer::drawRect(textX, inputY, inputW, 1.0f, borderDark);
        Renderer::drawRect(textX, inputY, 1.0f, 24.0f, borderDark);
        Renderer::drawRect(textX, inputY + 23.0f, inputW, 1.0f, borderLight);
        Renderer::drawRect(textX + inputW - 1.0f, inputY, 1.0f, 24.0f, borderLight);

        // Text & blinking cursor in input
        std::string dispInput = _currentInput;
        bool blink = (std::fmod(_cursorBlink, 2.0f) < 1.0f);
        if (blink) {
            dispInput.insert(_cursorPos, "_");
        }
        LabFont::drawText(textX + 6.0f, inputY + 4.0f, dispInput, 1.15f, Vec3(0.95f, 0.98f, 0.95f), LabFontType::GeoSans);

        // 5. Submit Button (Raised Bevel)
        float submitX = textX + inputW + 8.0f;
        Renderer::drawRect(submitX, inputY, submitW, 24.0f, Vec3(0.32f, 0.38f, 0.30f));
        Renderer::drawRect(submitX, inputY, submitW, 1.0f, borderLight);
        Renderer::drawRect(submitX, inputY, 1.0f, 24.0f, borderLight);
        Renderer::drawRect(submitX, inputY + 23.0f, submitW, 1.0f, borderDark);
        Renderer::drawRect(submitX + submitW - 1.0f, inputY, 1.0f, 24.0f, borderDark);
        LabFont::drawText(submitX + 16.0f, inputY + 4.0f, "Submit", 1.15f, Vec3(0.92f, 0.95f, 0.92f), LabFontType::GeoSans);

        // 6. Autocomplete Suggestion Popup (Rendered right below input like in screenshot!)
        if (!_activeSuggestions.empty()) {
            float popW = 340.0f;
            float popItemH = 19.0f;
            float popH = (float)_activeSuggestions.size() * popItemH + 6.0f;
            float popY = inputY + 27.0f;

            Renderer::drawRect(textX, popY, popW, popH, Vec3(0.14f, 0.17f, 0.13f));
            Renderer::drawRect(textX, popY, popW, 1.0f, borderLight);
            Renderer::drawRect(textX, popY, 1.0f, popH, borderLight);
            Renderer::drawRect(textX, popY + popH - 1.0f, popW, 1.0f, borderDark);
            Renderer::drawRect(textX + popW - 1.0f, popY, 1.0f, popH, borderDark);

            float curSugY = popY + 4.0f;
            for (size_t s = 0; s < _activeSuggestions.size(); ++s) {
                bool isSel = ((int)s == _selectedSuggestion);
                if (isSel) {
                    Renderer::drawRect(textX + 2.0f, curSugY - 1.0f, popW - 4.0f, popItemH, Vec3(0.24f, 0.32f, 0.22f));
                }
                LabFont::drawText(textX + 8.0f, curSugY + 2.0f, _activeSuggestions[s], 1.1f,
                                  isSel ? Vec3(1.0f, 0.95f, 0.3f) : Vec3(0.85f, 0.9f, 0.85f), LabFontType::GeoSans);
                curSugY += popItemH;
            }
        }

        Renderer::endUI();
    }

    void LabDevConsole::renderOverlay(int screenW, int screenH, const DevTelemetryData& t) {
        bool showAny = log_momentum || log_fps || log_ping || log_pos || log_all;
        if (!showAny) return;

        Renderer::beginUI(screenW, screenH);

        float startX = 20.0f;
        float curY = 20.0f;
        float boxW = 340.0f;

        // 1. Momentum & Speed Overlay (cl_showpos style)
        if (log_momentum || log_all) {
            Renderer::drawRect(startX, curY, boxW, 46.0f, Vec3(0.06f, 0.08f, 0.12f));
            Renderer::drawRect(startX, curY, 3.0f, 46.0f, Vec3(0.2f, 0.85f, 0.4f));

            std::ostringstream ss;
            ss << std::fixed << std::setprecision(1);
            ss << "SPEED: " << t.horizontalSpeed << " u/s (Cap: " << phys_max_bhop_speed << ")";
            LabFont::drawText(startX + 10.0f, curY + 6.0f, ss.str(), 1.25f, Vec3(0.3f, 1.0f, 0.5f), LabFontType::GeoSans);

            std::string bhopStatus = t.isBhopActive ? "BHOPPING: ACCELERATING" : (t.isGrounded ? "GROUND: FRICTION ACTIVE" : "AIR STRAFE ACTIVE");
            Vec3 bhopCol = t.isBhopActive ? Vec3(1.0f, 0.85f, 0.2f) : (t.isGrounded ? Vec3(0.7f, 0.7f, 0.7f) : Vec3(0.3f, 0.85f, 1.0f));
            LabFont::drawText(startX + 10.0f, curY + 24.0f, bhopStatus, 1.1f, bhopCol, LabFontType::GeoSans);

            curY += 52.0f;
        }

        // 2. Position & Angles Overlay
        if (log_pos || log_all) {
            Renderer::drawRect(startX, curY, boxW, 46.0f, Vec3(0.06f, 0.08f, 0.12f));
            Renderer::drawRect(startX, curY, 3.0f, 46.0f, Vec3(0.3f, 0.7f, 1.0f));

            std::ostringstream ss;
            ss << std::fixed << std::setprecision(2);
            ss << "POS: (" << t.playerPos.x << ", " << t.playerPos.y << ", " << t.playerPos.z << ")";
            LabFont::drawText(startX + 10.0f, curY + 6.0f, ss.str(), 1.15f, Vec3(0.85f, 0.9f, 0.95f), LabFontType::GeoSans);

            std::ostringstream ss2;
            ss2 << std::fixed << std::setprecision(1);
            ss2 << "ANG: PITCH " << t.pitch << " deg | YAW " << t.yaw << " deg";
            LabFont::drawText(startX + 10.0f, curY + 24.0f, ss2.str(), 1.1f, Vec3(0.6f, 0.8f, 1.0f), LabFontType::GeoSans);

            curY += 52.0f;
        }

        // 3. FPS & Performance Overlay
        if (log_fps || log_all) {
            Renderer::drawRect(startX, curY, 210.0f, 26.0f, Vec3(0.06f, 0.08f, 0.12f));
            Renderer::drawRect(startX, curY, 3.0f, 26.0f, Vec3(1.0f, 0.85f, 0.2f));

            std::ostringstream ss;
            ss << std::fixed << std::setprecision(1);
            ss << (int)t.fps << " FPS (" << t.frametimeMs << " ms)";
            LabFont::drawText(startX + 10.0f, curY + 5.0f, ss.str(), 1.25f, Vec3(1.0f, 0.85f, 0.2f), LabFontType::GeoSans);

            curY += 32.0f;
        }

        // 4. Ping Overlay
        if (log_ping || log_all) {
            Renderer::drawRect(startX, curY, 140.0f, 26.0f, Vec3(0.06f, 0.08f, 0.12f));
            Renderer::drawRect(startX, curY, 3.0f, 26.0f, Vec3(0.2f, 0.8f, 1.0f));

            std::string pingStr = "PING: " + std::to_string(t.pingMs) + " ms";
            LabFont::drawText(startX + 10.0f, curY + 5.0f, pingStr, 1.25f, Vec3(0.3f, 0.85f, 1.0f), LabFontType::GeoSans);

            curY += 32.0f;
        }

        Renderer::endUI();
    }

} // namespace Lab
