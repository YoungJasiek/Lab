#include "LabDevConsole.h"
#include "LabRenderer.h"
#include "LabFont.h"
#include <sstream>
#include <algorithm>
#include <iomanip>
#include <cctype>

namespace Lab {

    LabDevConsole::LabDevConsole() {
        _entries.push_back({ "=== LAB ENGINE DEVELOPER CONSOLE (v0.4.0) ===", Vec3(0.3f, 0.85f, 1.0f) });
        _entries.push_back({ "Type 'sys_help' for command list. Press [~] or [ESC] to close.", Vec3(0.7f, 0.75f, 0.8f) });
        registerDefaultCommands();
    }

    void LabDevConsole::open() {
        isOpen = true;
        _cursorBlink = 0.0f;
    }

    void LabDevConsole::close() {
        isOpen = false;
        _currentInput.clear();
        _cursorPos = 0;
        _historyIndex = -1;
    }

    void LabDevConsole::toggle() {
        if (isOpen) close();
        else open();
    }

    void LabDevConsole::print(const std::string& line, const Vec3& color) {
        _entries.push_back({ line, color });
        while (_entries.size() > 100) {
            _entries.pop_front();
        }
        _scrollOffset = 0;
    }

    void LabDevConsole::registerDefaultCommands() {
        // Will be matched in dispatchCommand
    }

    void LabDevConsole::execute(const std::string& rawLine) {
        // Trim leading and trailing whitespace
        size_t start = rawLine.find_first_not_of(" \t\r\n");
        if (start == std::string::npos) return;
        size_t end = rawLine.find_last_not_of(" \t\r\n");
        std::string line = rawLine.substr(start, end - start + 1);
        if (line.empty()) return;

        // Add to history
        _commandHistory.push_back(line);
        _historyIndex = -1;
        print("> " + line, Vec3(1.0f, 0.85f, 0.3f));

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
            // Standalone commands
            dispatchCommand("sys", cmd, args);
        }
    }

    void LabDevConsole::dispatchCommand(const std::string& prefix, const std::string& name, const std::string& args) {
        // Convert to lowercase
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
        if (p == "sp") {
            if (n == "cheats") {
                int b = parseBool(args);
                if (b >= 0) sp_cheats = (b == 1);
                print(std::string("sp_cheats = ") + (sp_cheats ? "1 (ENABLED)" : "0 (DISABLED)"),
                      sp_cheats ? Vec3(0.2f, 1.0f, 0.4f) : Vec3(1.0f, 0.3f, 0.3f));
                return;
            }

            // Commands below require sp_cheats 1
            if (!sp_cheats) {
                print("[SP] 'sp_cheats' is 0! Type 'sp_cheats 1' to unlock cheats and developer controls.", Vec3(1.0f, 0.4f, 0.3f));
                return;
            }

            if (n == "god") {
                int b = parseBool(args);
                if (b >= 0) sp_god = (b == 1);
                else sp_god = !sp_god;
                print(std::string("sp_god = ") + (sp_god ? "1 (INVINCIBLE)" : "0 (MORTAL)"), Vec3(0.3f, 0.9f, 1.0f));
            } else if (n == "noclip") {
                int b = parseBool(args);
                if (b >= 0) sp_noclip = (b == 1);
                else sp_noclip = !sp_noclip;
                print(std::string("sp_noclip = ") + (sp_noclip ? "1 (GHOST / FLY)" : "0 (COLLISION)"), Vec3(0.3f, 0.9f, 1.0f));
            } else if (n == "bhop") {
                int b = parseBool(args);
                if (b >= 0) sp_bhop = (b == 1);
                else sp_bhop = !sp_bhop;
                print(std::string("sp_bhop = ") + (sp_bhop ? "1 (MOMENTUM PRESERVED)" : "0 (VANILLA CLAMP)"), Vec3(0.3f, 0.9f, 1.0f));
            } else if (n == "speed") {
                try {
                    float s = std::stof(args);
                    sp_speed = std::clamp(s, 0.1f, 10.0f);
                    print("sp_speed = " + std::to_string(sp_speed) + "x", Vec3(0.3f, 0.9f, 1.0f));
                } catch (...) {
                    print("sp_speed currently: " + std::to_string(sp_speed) + "x (Usage: sp_speed <multiplier>)", Vec3(0.7f, 0.7f, 0.7f));
                }
            } else if (n == "jump") {
                try {
                    float j = std::stof(args);
                    sp_jump = std::clamp(j, 1.0f, 30.0f);
                    print("sp_jump = " + std::to_string(sp_jump), Vec3(0.3f, 0.9f, 1.0f));
                } catch (...) {
                    print("sp_jump currently: " + std::to_string(sp_jump), Vec3(0.7f, 0.7f, 0.7f));
                }
            } else if (n == "heal") {
                int val = 100;
                try { if (!args.empty()) val = std::stoi(args); } catch (...) {}
                if (onHeal) onHeal(val);
                print("Healed player by " + std::to_string(val) + " HP", Vec3(0.2f, 1.0f, 0.4f));
            } else if (n == "armor") {
                int val = 100;
                try { if (!args.empty()) val = std::stoi(args); } catch (...) {}
                if (onArmor) onArmor(val);
                print("Gave player " + std::to_string(val) + " armor points", Vec3(0.2f, 0.8f, 1.0f));
            } else if (n == "ammo") {
                int val = 250;
                try { if (!args.empty()) val = std::stoi(args); } catch (...) {}
                if (onAmmo) onAmmo(val);
                print("Refilled ammo (" + std::to_string(val) + " rounds)", Vec3(1.0f, 0.9f, 0.2f));
            } else if (n == "get") {
                try {
                    int wId = std::stoi(args);
                    if (onGetWeapon) onGetWeapon(wId);
                    print("Spawned weapon ID #" + std::to_string(wId), Vec3(0.3f, 0.9f, 1.0f));
                } catch (...) {
                    print("Usage: sp_get <weapon_id> (e.g. 101, 102, 103, 104)", Vec3(1.0f, 0.5f, 0.3f));
                }
            } else if (n == "give_all") {
                if (onGiveAll) onGiveAll();
                print("Arsenal unlocked: all weapons and maximum ammo acquired!", Vec3(0.2f, 1.0f, 0.5f));
            } else if (n == "respawn") {
                if (onRespawn) onRespawn();
                print("Respawned player at origin spawn point", Vec3(0.3f, 0.9f, 1.0f));
            } else if (n == "suicide") {
                if (onSuicide) onSuicide();
                print("Player killed via console command", Vec3(1.0f, 0.2f, 0.2f));
            } else {
                print("Unknown super command 'sp_" + n + "'. Type 'sys_help sp' for list.", Vec3(1.0f, 0.4f, 0.3f));
            }
            return;
        }

        // =====================================================================
        // LOG_ (HUD 2D TELEMETRY OVERLAYS)
        // =====================================================================
        if (p == "log") {
            auto toggleFlag = [&](bool& flag, const std::string& label) {
                int b = parseBool(args);
                if (b >= 0) flag = (b == 1);
                else flag = !flag;
                print(label + " = " + (flag ? "1 (VISIBLE)" : "0 (HIDDEN)"), flag ? Vec3(0.2f, 1.0f, 0.4f) : Vec3(0.7f, 0.7f, 0.7f));
            };

            if (n == "momentum") toggleFlag(log_momentum, "log_momentum");
            else if (n == "fps") toggleFlag(log_fps, "log_fps");
            else if (n == "ping") toggleFlag(log_ping, "log_ping");
            else if (n == "pos") toggleFlag(log_pos, "log_pos");
            else if (n == "colliders") toggleFlag(log_colliders, "log_colliders");
            else if (n == "triggers") toggleFlag(log_triggers, "log_triggers");
            else if (n == "ai") toggleFlag(log_ai, "log_ai");
            else if (n == "drawcalls") toggleFlag(log_drawcalls, "log_drawcalls");
            else if (n == "memory") toggleFlag(log_memory, "log_memory");
            else if (n == "all") {
                int b = parseBool(args);
                bool state = (b >= 0) ? (b == 1) : !log_all;
                log_all = state;
                log_momentum = state;
                log_fps = state;
                log_pos = state;
                log_ping = state;
                print(std::string("All telemetry overlays ") + (state ? "ENABLED" : "DISABLED"), state ? Vec3(0.2f, 1.0f, 0.4f) : Vec3(0.7f, 0.7f, 0.7f));
            } else {
                print("Unknown log overlay 'log_" + n + "'. Type 'sys_help log' for list.", Vec3(1.0f, 0.4f, 0.3f));
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
                    print("Loading map: " + args, Vec3(0.3f, 0.85f, 1.0f));
                } else {
                    print("Usage: wrd_map <scene_name>", Vec3(1.0f, 0.5f, 0.3f));
                }
            } else if (n == "restart") {
                if (onRestartMap) onRestartMap();
                print("Restarting current map session...", Vec3(0.3f, 0.85f, 1.0f));
            } else if (n == "timescale") {
                try {
                    float ts = std::stof(args);
                    wrd_timescale = std::clamp(ts, 0.05f, 10.0f);
                    print("wrd_timescale = " + std::to_string(wrd_timescale) + "x", Vec3(0.3f, 0.9f, 1.0f));
                } catch (...) {
                    print("wrd_timescale currently: " + std::to_string(wrd_timescale) + "x", Vec3(0.7f, 0.7f, 0.7f));
                }
            } else if (n == "wireframe") {
                int b = parseBool(args);
                wrd_wireframe = (b >= 0) ? (b == 1) : !wrd_wireframe;
                print(std::string("wrd_wireframe = ") + (wrd_wireframe ? "1 (ON)" : "0 (OFF)"), Vec3(0.3f, 0.9f, 1.0f));
            } else if (n == "fog") {
                int b = parseBool(args);
                wrd_fog = (b >= 0) ? (b == 1) : !wrd_fog;
                print(std::string("wrd_fog = ") + (wrd_fog ? "1 (ON)" : "0 (OFF)"), Vec3(0.3f, 0.9f, 1.0f));
            } else if (n == "postprocess") {
                int b = parseBool(args);
                wrd_postprocess = (b >= 0) ? (b == 1) : !wrd_postprocess;
                print(std::string("wrd_postprocess = ") + (wrd_postprocess ? "1 (ON)" : "0 (OFF)"), Vec3(0.3f, 0.9f, 1.0f));
            } else {
                print("Unknown world command 'wrd_" + n + "'", Vec3(1.0f, 0.4f, 0.3f));
            }
            return;
        }

        // =====================================================================
        // PHYS_ (PHYSICS & BHOP TUNING)
        // =====================================================================
        if (p == "phys") {
            if (n == "friction") {
                try {
                    phys_friction = std::stof(args);
                    print("phys_friction = " + std::to_string(phys_friction), Vec3(0.3f, 0.9f, 1.0f));
                } catch (...) {
                    print("phys_friction currently: " + std::to_string(phys_friction), Vec3(0.7f, 0.7f, 0.7f));
                }
            } else if (n == "air_accel") {
                try {
                    phys_air_accel = std::stof(args);
                    print("phys_air_accel = " + std::to_string(phys_air_accel), Vec3(0.3f, 0.9f, 1.0f));
                } catch (...) {
                    print("phys_air_accel currently: " + std::to_string(phys_air_accel), Vec3(0.7f, 0.7f, 0.7f));
                }
            } else if (n == "bhop_window") {
                try {
                    phys_bhop_window = std::stof(args) / 1000.0f; // in ms
                    print("phys_bhop_window = " + std::to_string((int)(phys_bhop_window * 1000.0f)) + " ms", Vec3(0.3f, 0.9f, 1.0f));
                } catch (...) {
                    print("phys_bhop_window currently: " + std::to_string((int)(phys_bhop_window * 1000.0f)) + " ms", Vec3(0.7f, 0.7f, 0.7f));
                }
            } else if (n == "gravity") {
                try {
                    phys_gravity = std::stof(args);
                    print("phys_gravity = " + std::to_string(phys_gravity), Vec3(0.3f, 0.9f, 1.0f));
                } catch (...) {
                    print("phys_gravity currently: " + std::to_string(phys_gravity), Vec3(0.7f, 0.7f, 0.7f));
                }
            } else {
                print("Unknown physics command 'phys_" + n + "'", Vec3(1.0f, 0.4f, 0.3f));
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
                    print("cam_fov = " + std::to_string((int)cam_fov) + " deg", Vec3(0.3f, 0.9f, 1.0f));
                } catch (...) {
                    print("cam_fov currently: " + std::to_string((int)cam_fov), Vec3(0.7f, 0.7f, 0.7f));
                }
            } else if (n == "thirdperson") {
                int b = parseBool(args);
                cam_thirdperson = (b >= 0) ? (b == 1) : !cam_thirdperson;
                print(std::string("cam_thirdperson = ") + (cam_thirdperson ? "1 (TPP)" : "0 (FPP)"), Vec3(0.3f, 0.9f, 1.0f));
            } else if (n == "bob") {
                int b = parseBool(args);
                cam_bob = (b >= 0) ? (b == 1) : !cam_bob;
                print(std::string("cam_bob = ") + (cam_bob ? "1 (ENABLED)" : "0 (DISABLED)"), Vec3(0.3f, 0.9f, 1.0f));
            } else {
                print("Unknown camera command 'cam_" + n + "'", Vec3(1.0f, 0.4f, 0.3f));
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
                print(std::string("r_bloom = ") + (r_bloom ? "1" : "0"), Vec3(0.3f, 0.9f, 1.0f));
            } else if (n == "fxaa") {
                int b = parseBool(args);
                r_fxaa = (b >= 0) ? (b == 1) : !r_fxaa;
                print(std::string("r_fxaa = ") + (r_fxaa ? "1" : "0"), Vec3(0.3f, 0.9f, 1.0f));
            } else if (n == "reload_shaders") {
                if (onReloadShaders) onReloadShaders();
                print("Hot-reloading OpenGL GLSL shaders...", Vec3(0.2f, 1.0f, 0.4f));
            } else {
                print("Unknown renderer command 'r_" + n + "'", Vec3(1.0f, 0.4f, 0.3f));
            }
            return;
        }

        // =====================================================================
        // MP_ & AI_ (MULTIPLAYER & BOTS)
        // =====================================================================
        if (p == "mp" || p == "ai") {
            if (n == "a_bots") {
                try {
                    int c = std::stoi(args);
                    if (onSetBotsA) onSetBotsA(c);
                    print("Set Team A bot count to " + std::to_string(c), Vec3(0.3f, 0.9f, 1.0f));
                } catch (...) {}
            } else if (n == "b_bots") {
                try {
                    int c = std::stoi(args);
                    if (onSetBotsB) onSetBotsB(c);
                    print("Set Team B bot count to " + std::to_string(c), Vec3(0.3f, 0.9f, 1.0f));
                } catch (...) {}
            } else if (n == "bot_kick_all" || n == "kick_all") {
                if (onKickBots) onKickBots();
                print("Removed all active bots from the world", Vec3(1.0f, 0.4f, 0.4f));
            } else if (n == "freeze" || n == "bot_freeze") {
                int b = parseBool(args);
                bool freeze = (b >= 0) ? (b == 1) : true;
                if (onFreezeBots) onFreezeBots(freeze);
                print(std::string("Bot AI logic ") + (freeze ? "FROZEN" : "UNFROZEN"), Vec3(0.3f, 0.9f, 1.0f));
            } else {
                print("Unknown AI/MP command", Vec3(1.0f, 0.4f, 0.3f));
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
                print("--- LAB ENGINE COMMAND PREFIXES ---", Vec3(0.3f, 0.85f, 1.0f));
                print("  sp_   : Cheats & Player controls (sp_cheats, sp_god, sp_noclip, sp_bhop, sp_speed, sp_get)", Vec3(0.9f, 0.9f, 0.9f));
                print("  log_  : 2D HUD Telemetry overlays (log_momentum, log_fps, log_pos, log_ping, log_all)", Vec3(0.9f, 0.9f, 0.9f));
                print("  wrd_  : World & Map controls (wrd_map, wrd_restart, wrd_timescale, wrd_wireframe, wrd_fog)", Vec3(0.9f, 0.9f, 0.9f));
                print("  phys_ : Physics & Bhop (phys_friction, phys_air_accel, phys_bhop_window, phys_gravity)", Vec3(0.9f, 0.9f, 0.9f));
                print("  cam_  : Camera FOV & view (cam_fov, cam_thirdperson, cam_bob)", Vec3(0.9f, 0.9f, 0.9f));
                print("  r_    : Graphics & Shaders (r_bloom, r_fxaa, r_reload_shaders)", Vec3(0.9f, 0.9f, 0.9f));
                print("  mp_   : Bot & arena controls (mp_a_bots, mp_b_bots, mp_bot_kick_all, mp_bot_freeze)", Vec3(0.9f, 0.9f, 0.9f));
                print("  sys_  : System commands (sys_clear, sys_help, sys_quit)", Vec3(0.9f, 0.9f, 0.9f));
                return;
            }
        }

        print("Command not recognized: '" + prefix + "_" + name + "'. Type 'sys_help' for list.", Vec3(1.0f, 0.35f, 0.35f));
    }

    bool LabDevConsole::onKey(int key, int action) {
        if (!isOpen) return false;
        if (action != 1 && action != 2) return false; // Press or Repeat

        // Tilde (`~`, key 96 / GLFW_KEY_GRAVE_ACCENT) or Escape (256) closes console
        if (key == 96 || key == 256) {
            close();
            return true;
        }

        // Enter: execute command
        if (key == 257) {
            if (!_currentInput.empty()) {
                execute(_currentInput);
                _currentInput.clear();
                _cursorPos = 0;
            }
            return true;
        }

        // Backspace
        if (key == 259) {
            if (!_currentInput.empty() && _cursorPos > 0) {
                _currentInput.erase(_cursorPos - 1, 1);
                _cursorPos--;
            }
            return true;
        }

        // Delete
        if (key == 261) {
            if (_cursorPos < (int)_currentInput.size()) {
                _currentInput.erase(_cursorPos, 1);
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
            if (_cursorPos < (int)_currentInput.size()) _cursorPos++;
            return true;
        }

        // Up Arrow: History backward
        if (key == 265) {
            if (!_commandHistory.empty()) {
                if (_historyIndex == -1) {
                    _historyIndex = (int)_commandHistory.size() - 1;
                } else if (_historyIndex > 0) {
                    _historyIndex--;
                }
                _currentInput = _commandHistory[_historyIndex];
                _cursorPos = (int)_currentInput.size();
            }
            return true;
        }

        // Down Arrow: History forward
        if (key == 264) {
            if (!_commandHistory.empty() && _historyIndex != -1) {
                if (_historyIndex < (int)_commandHistory.size() - 1) {
                    _historyIndex++;
                    _currentInput = _commandHistory[_historyIndex];
                } else {
                    _historyIndex = -1;
                    _currentInput.clear();
                }
                _cursorPos = (int)_currentInput.size();
            }
            return true;
        }

        // PageUp / PageDown: scroll
        if (key == 266) { // PageUp
            _scrollOffset = std::min((int)_entries.size() - 5, _scrollOffset + 5);
            return true;
        }
        if (key == 267) { // PageDown
            _scrollOffset = std::max(0, _scrollOffset - 5);
            return true;
        }

        return true;
    }

    void LabDevConsole::onChar(unsigned int codepoint) {
        if (!isOpen) return;
        // Skip tilde key character if it opens console
        if (codepoint == '`' || codepoint == '~') return;

        if (codepoint >= 32 && codepoint < 127) {
            _currentInput.insert(_currentInput.begin() + _cursorPos, static_cast<char>(codepoint));
            _cursorPos++;
        }
    }

    void LabDevConsole::update(float dt) {
        _cursorBlink += dt * 4.0f;
    }

    void LabDevConsole::render(int screenW, int screenH) {
        if (!isOpen) return;

        Renderer::beginUI(screenW, screenH);

        float conW = (float)screenW;
        float conH = (float)screenH * 0.45f;
        float headerH = 26.0f;
        float inputH = 28.0f;
        float lineH = 18.0f;

        // Dark glass background
        Renderer::drawRect(0.0f, 0.0f, conW, conH, Vec3(0.05f, 0.07f, 0.10f));
        // Header bar
        Renderer::drawRect(0.0f, 0.0f, conW, headerH, Vec3(0.09f, 0.13f, 0.18f));
        Renderer::drawRect(0.0f, headerH - 2.0f, conW, 2.0f, Vec3(0.2f, 0.7f, 0.95f));

        LabFont::drawText(16.0f, 6.0f, "[LAB ENGINE DEVELOPER CONSOLE] - Press [~] or [ESC] to exit",
                          1.25f, Vec3(0.4f, 0.85f, 1.0f), LabFontType::GeoSans);

        // Render logs
        float logAreaH = conH - headerH - inputH;
        int maxVisible = (int)(logAreaH / lineH);
        int total = (int)_entries.size();

        int endIdx = total - _scrollOffset;
        int startIdx = std::max(0, endIdx - maxVisible);

        float curY = headerH + 6.0f;
        for (int i = startIdx; i < endIdx && i < total; ++i) {
            const auto& entry = _entries[i];
            LabFont::drawText(18.0f, curY, entry.text, 1.15f, entry.color, LabFontType::GeoSans);
            curY += lineH;
        }

        // Input bottom bar
        float inputY = conH - inputH;
        Renderer::drawRect(0.0f, inputY, conW, inputH, Vec3(0.07f, 0.10f, 0.14f));
        Renderer::drawRect(0.0f, inputY, conW, 1.0f, Vec3(0.2f, 0.35f, 0.5f));

        // Prompt
        LabFont::drawText(14.0f, inputY + 5.0f, "> ", 1.35f, Vec3(1.0f, 0.8f, 0.2f), LabFontType::GeoSans);

        // Input text
        std::string displayText = _currentInput;
        bool showCursor = (std::fmod(_cursorBlink, 2.0f) < 1.0f);
        if (showCursor) {
            displayText.insert(_cursorPos, "_");
        }
        LabFont::drawText(34.0f, inputY + 5.0f, displayText, 1.3f, Vec3(1.0f, 1.0f, 1.0f), LabFontType::GeoSans);

        // Bottom border
        Renderer::drawRect(0.0f, conH - 2.0f, conW, 2.0f, Vec3(0.25f, 0.75f, 0.95f));

        Renderer::endUI();
    }

    void LabDevConsole::renderOverlay(int screenW, int screenH, const DevTelemetryData& t) {
        bool showAny = log_momentum || log_fps || log_ping || log_pos || log_all;
        if (!showAny) return;

        Renderer::beginUI(screenW, screenH);

        float startX = 20.0f;
        float curY = 20.0f;
        float boxW = 340.0f;

        // 1. Momentum & Speed Overlay
        if (log_momentum || log_all) {
            Renderer::drawRect(startX, curY, boxW, 46.0f, Vec3(0.06f, 0.08f, 0.12f));
            Renderer::drawRect(startX, curY, 3.0f, 46.0f, Vec3(0.2f, 0.85f, 0.4f));

            std::ostringstream ss;
            ss << std::fixed << std::setprecision(1);
            ss << "SPEED: " << t.horizontalSpeed << " u/s (Total: " << t.currentSpeed << ")";
            LabFont::drawText(startX + 10.0f, curY + 6.0f, ss.str(), 1.25f, Vec3(0.3f, 1.0f, 0.5f), LabFontType::GeoSans);

            std::string bhopStatus = t.isBhopActive ? "BHOPPING: GAINING MOMENTUM" : (t.isGrounded ? "ON GROUND: FRICTION ACTIVE" : "IN AIR: AIR STRAFE ACTIVE");
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
