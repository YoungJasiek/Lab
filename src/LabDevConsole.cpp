#include "LabDevConsole.h"
#include "LabRenderer.h"
#include "LabFont.h"
#include <GLFW/glfw3.h>
#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif
#include <sstream>
#include <algorithm>
#include <iomanip>
#include <cctype>
#include <cstring>
#include <charconv>

namespace Lab {

    LabDevConsole::LabDevConsole() {
        registerKnownCommands();

        print("Lab Engine Developer Console initialized.", Vec3(0.90f, 0.90f, 0.90f));
        print("Type 'help' for command list. Press [~] or [ESC] to close.", Vec3(0.75f, 0.75f, 0.75f));
    }

    void LabDevConsole::open() {
        isOpen = true;
        _cursorBlink = 0.0f;
        isDragging = false;
        isResizing = false;
        updateAutocomplete();
    }

    void LabDevConsole::close() {
        isOpen = false;
        _currentInput.clear();
        _cursorPos = 0;
        _historyIndex = -1;
        isDragging = false;
        isResizing = false;
        _activeSuggestions.clear();
        _selectedSuggestion = -1;
    }

    void LabDevConsole::toggle() {
        if (isOpen) close();
        else open();
    }

    void LabDevConsole::print(const std::string& line, const Vec3& color) {
        _entries.push_back({ line, color });
        while (_entries.size() > 500) {
            _entries.pop_front();
        }
        _scrollOffset = 0;
    }

    void LabDevConsole::copyLogToClipboard() {
        if (_entries.empty()) {
            print("[INFO] Console log is empty. Nothing to copy.", Vec3(0.7f, 0.7f, 0.7f));
            return;
        }

        std::string fullLog;
        for (const auto& e : _entries) {
            fullLog += e.text;
            fullLog += "\r\n";
        }

        bool ok = false;
        GLFWwindow* win = glfwGetCurrentContext();
        if (win) {
            glfwSetClipboardString(win, fullLog.c_str());
            ok = true;
        }

#if defined(_WIN32)
        if (OpenClipboard(nullptr)) {
            EmptyClipboard();
            HGLOBAL hGlob = GlobalAlloc(GMEM_MOVEABLE, fullLog.size() + 1);
            if (hGlob) {
                void* p = GlobalLock(hGlob);
                if (p) {
                    memcpy(p, fullLog.c_str(), fullLog.size() + 1);
                    GlobalUnlock(hGlob);
                    SetClipboardData(CF_TEXT, hGlob);
                    ok = true;
                }
            }
            CloseClipboard();
        }
#endif

        if (ok) {
            print("[SUCCESS] Entire console log copied to clipboard (" + std::to_string(_entries.size()) + " lines).",
                  Vec3(0.90f, 0.90f, 0.90f));
        } else {
            print("[ERROR] Failed to access system clipboard.", Vec3(1.0f, 0.35f, 0.35f));
        }
    }

    void LabDevConsole::copyLineToClipboard(int lineIndex) {
        if (lineIndex < 0 || lineIndex >= static_cast<int>(_entries.size())) return;
        const std::string& line = _entries[lineIndex].text;

        bool ok = false;
        GLFWwindow* win = glfwGetCurrentContext();
        if (win) {
            glfwSetClipboardString(win, line.c_str());
            ok = true;
        }

#if defined(_WIN32)
        if (OpenClipboard(nullptr)) {
            EmptyClipboard();
            HGLOBAL hGlob = GlobalAlloc(GMEM_MOVEABLE, line.size() + 1);
            if (hGlob) {
                void* p = GlobalLock(hGlob);
                if (p) {
                    memcpy(p, line.c_str(), line.size() + 1);
                    GlobalUnlock(hGlob);
                    SetClipboardData(CF_TEXT, hGlob);
                    ok = true;
                }
            }
            CloseClipboard();
        }
#endif

        if (ok) {
            print("[COPIED LINE] \"" + line + "\"", Vec3(0.90f, 0.90f, 0.90f));
        }
    }

    bool LabDevConsole::executeBind(const std::string& keyName) {
        std::string k = keyName;
        std::transform(k.begin(), k.end(), k.begin(), [](unsigned char c){ return (char)std::tolower(c); });
        auto it = _keyBinds.find(k);
        if (it != _keyBinds.end()) {
            execute(it->second);
            return true;
        }
        return false;
    }

    void LabDevConsole::registerKnownCommands() {
        _knownCommands = {
            "sp_cheats", "sp_god", "sp_noclip", "sp_bhop", "sp_speed", "sp_jump",
            "sp_heal", "sp_armor", "sp_ammo", "sp_get", "sp_give_all", "sp_respawn", "sp_suicide",
            "log_momentum", "log_fps", "log_ping", "log_pos", "log_colliders", "log_triggers",
            "log_ai", "log_drawcalls", "log_memory", "log_all",
            "wrd_map", "wrd_restart", "wrd_skybox", "wrd_model", "wrd_timescale", "wrd_wireframe", "wrd_fog", "wrd_fog_density", "wrd_light_ambient", "wrd_postprocess",
            "cam_fov", "cam_viewmodel_fov", "cam_viewmodel_offset", "cam_thirdperson", "cam_bob", "cam_shake", "cam_freecam",
            "r_bloom", "r_fxaa", "r_vsync", "r_shadows", "r_shadow_res", "r_lightmaps", "r_normalmaps", "r_cubemaps", "r_postprocess", "r_reload_shaders",
            "phys_gravity", "phys_friction", "phys_air_accel", "phys_bhop_window", "phys_max_bhop_speed", "phys_step_height", "phys_ragdoll_force", "phys_debug",
            "ai_disable", "ai_freeze", "ai_sight", "ai_paths", "ai_aggro", "ai_ignore_player", "ai_spawn",
            "snd_volume", "snd_music", "snd_sfx", "snd_voice", "snd_play", "snd_stop_all", "snd_lipsync_test", "snd_restart",
            "ui_hide", "ui_crosshair", "ui_crosshair_color", "ui_scale", "ui_menu", "ui_reload",
            "dcl_clear", "dcl_limit", "dcl_spawn", "dcl_distance",
            "mp_a_bots", "mp_b_bots", "mp_bot_kick_all", "mp_bot_freeze", "mp_restart", "mp_friendly_fire",
            "net_status", "net_fake_lag", "net_fake_loss", "net_connect", "net_disconnect",
            "sys_copylog", "sys_clear", "sys_help", "sys_bind", "sys_quit",
            "save", "save_async", "save_asyncdelay", "save_console", "save_disable", "save_finish_async", "save_history_count", "save_huddelayframes", "save_in_memory", "save_noxsave",
            "map", "restart", "disconnect", "connect", "status", "version", "ping", "sv_cheats",
            "god", "noclip", "bhop", "clear", "copylog", "condump", "help", "quit", "exit"
        };
    }

    std::string LabDevConsole::getCvarValue(const std::string& cmd) const {
        if (cmd == "sp_cheats" || cmd == "sv_cheats") return sp_cheats ? "1" : "0";
        if (cmd == "sp_god" || cmd == "god") return sp_god ? "1" : "0";
        if (cmd == "sp_noclip" || cmd == "noclip") return sp_noclip ? "1" : "0";
        if (cmd == "sp_bhop" || cmd == "bhop") return sp_bhop ? "1" : "0";
        if (cmd == "sp_speed") return std::to_string((int)sp_speed);
        if (cmd == "sp_jump") return std::to_string((int)sp_jump);
        if (cmd == "cam_fov") return std::to_string((int)cam_fov);
        if (cmd == "cam_viewmodel_fov") return std::to_string((int)cam_viewmodel_fov);
        if (cmd == "cam_thirdperson") return cam_thirdperson ? "1" : "0";
        if (cmd == "cam_bob") return cam_bob ? "1" : "0";
        if (cmd == "r_bloom") return r_bloom ? "1" : "0";
        if (cmd == "r_fxaa") return r_fxaa ? "1" : "0";
        if (cmd == "r_vsync") return r_vsync ? "1" : "0";
        if (cmd == "r_shadows") return r_shadows ? "1" : "0";
        if (cmd == "r_shadow_res") return std::to_string(r_shadow_res);
        if (cmd == "wrd_wireframe") return wrd_wireframe ? "1" : "0";
        if (cmd == "wrd_fog") return wrd_fog ? "1" : "0";
        if (cmd == "wrd_timescale") return std::to_string((int)wrd_timescale);
        if (cmd == "phys_gravity") return std::to_string((int)phys_gravity);
        if (cmd == "phys_friction") return std::to_string((int)phys_friction);
        if (cmd == "phys_air_accel") return std::to_string((int)phys_air_accel);
        if (cmd == "phys_bhop_window") return "0.08";
        if (cmd == "phys_max_bhop_speed") return "11.5";
        if (cmd == "phys_debug") return phys_debug ? "1" : "0";
        if (cmd == "snd_volume") return std::to_string((int)(snd_volume * 100.0f)) + "%";
        if (cmd == "snd_music") return std::to_string((int)(snd_music * 100.0f)) + "%";
        if (cmd == "snd_sfx") return std::to_string((int)(snd_sfx * 100.0f)) + "%";
        if (cmd == "ui_crosshair") return ui_crosshair ? "1" : "0";
        if (cmd == "ui_scale") return "1.0";
        if (cmd == "save_async") return "1";
        if (cmd == "save_asyncdelay") return "0";
        if (cmd == "save_console") return "0";
        if (cmd == "save_disable") return "0";
        if (cmd == "save_history_count") return "1";
        if (cmd == "save_huddelayframes") return "1";
        if (cmd == "save_in_memory") return "0";
        if (cmd == "save_noxsave") return "0";
        return "";
    }

    void LabDevConsole::updateAutocomplete() {
        _activeSuggestions.clear();
        _selectedSuggestion = -1;
        if (_currentInput.empty()) return;

        // If there's a space, hide suggestions (user is typing arguments)
        if (_currentInput.find(' ') != std::string::npos) return;

        std::string query = _currentInput;
        std::transform(query.begin(), query.end(), query.begin(), [](unsigned char c) { return (char)std::tolower(c); });

        for (const auto& cmd : _knownCommands) {
            if (cmd.find(query) == 0) { // prefix match
                _activeSuggestions.push_back(cmd);
                if (_activeSuggestions.size() >= 10) break;
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

        // Auto-enforce '_' on known prefixes separated by space or dash (e.g. "sp cheats 1" -> "sp_cheats 1")
        static const char* const knownPrefixes[] = {
            "sp", "sv", "log", "wrd", "cam", "r", "phys", "ai", "snd", "ui", "dcl", "mp", "net", "sys"
        };
        for (const auto& pfx : knownPrefixes) {
            size_t plen = strlen(pfx);
            if (line.size() > plen && (line[plen] == ' ' || line[plen] == '-')) {
                std::string cand = line.substr(0, plen);
                std::transform(cand.begin(), cand.end(), cand.begin(), [](unsigned char c){ return (char)std::tolower(c); });
                if (cand == pfx) {
                    line[plen] = '_';
                    break;
                }
            }
        }

        // Convert any dashes in the command token (before the first space) to '_' (e.g. "sp-cheats 1" -> "sp_cheats 1")
        size_t firstSpace = line.find(' ');
        size_t cmdTokenEnd = (firstSpace != std::string::npos) ? firstSpace : line.size();
        for (size_t i = 0; i < cmdTokenEnd; ++i) {
            if (line[i] == '-') {
                line[i] = '_';
            }
        }

        // Multi-word resolution: match word combinations against known commands (e.g. "phys air accel 10" -> "phys_air_accel 10")
        std::vector<std::string> words;
        {
            std::istringstream wiss(line);
            std::string w;
            while (wiss >> w) words.push_back(w);
        }
        if (words.size() > 1) {
            for (int count = (int)words.size(); count >= 2; --count) {
                std::string candCmd;
                for (int i = 0; i < count; ++i) {
                    if (i > 0) candCmd += "_";
                    candCmd += words[i];
                }
                std::string candLower = candCmd;
                std::transform(candLower.begin(), candLower.end(), candLower.begin(), [](unsigned char c){ return (char)std::tolower(c); });

                bool found = false;
                for (const auto& kc : _knownCommands) {
                    if (kc == candLower) {
                        found = true;
                        break;
                    }
                }
                if (found) {
                    std::string remArgs;
                    for (size_t i = count; i < words.size(); ++i) {
                        if (!remArgs.empty()) remArgs += " ";
                        remArgs += words[i];
                    }
                    line = remArgs.empty() ? candCmd : (candCmd + " " + remArgs);
                    break;
                }
            }
        }

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
            // Standalone or compatibility aliases
            if (cmd == "god") dispatchCommand("sp", "god", args);
            else if (cmd == "noclip") dispatchCommand("sp", "noclip", args);
            else if (cmd == "bhop") dispatchCommand("sp", "bhop", args);
            else if (cmd == "clear") dispatchCommand("sys", "clear", args);
            else if (cmd == "copylog" || cmd == "condump") dispatchCommand("sys", "copylog", args);
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
        // SP_ / SV_ (SUPER / CHEATS & PLAYER)
        // =====================================================================
        if (p == "sp" || p == "sv") {
            if (n == "cheats") {
                if (isPublicMultiplayerServer && !sv_cheats) {
                    sp_cheats = false;
                    print("Can't change sp_cheats on a public/official multiplayer server (authoritative sv_cheats is 0).",
                          Vec3(1.0f, 0.35f, 0.35f));
                    return;
                }
                int b = parseBool(args);
                if (b >= 0) {
                    sp_cheats = (b == 1);
                    if (!isPublicMultiplayerServer) {
                        sv_cheats = sp_cheats;
                    }
                }
                print(std::string("sp_cheats set to ") + (sp_cheats ? "1" : "0"),
                      sp_cheats ? Vec3(0.92f, 0.92f, 0.92f) : Vec3(0.90f, 0.45f, 0.45f));
                return;
            }

            if (n == "bhop") {
                int b = parseBool(args);
                sp_bhop = (b >= 0) ? (b == 1) : !sp_bhop;
                print(std::string("bhop ") + (sp_bhop ? "ENABLED" : "DISABLED"), Vec3(0.92f, 0.92f, 0.92f));
                return;
            }
            if (n == "respawn") {
                if (onRespawn) onRespawn();
                print("Respawning player...", Vec3(0.85f, 0.85f, 0.85f));
                return;
            }
            if (n == "suicide" || n == "kill") {
                if (onSuicide) onSuicide();
                print("Player killed.", Vec3(1.0f, 0.35f, 0.35f));
                return;
            }

            // Commands below require sp_cheats 1 AND (if on public server) authoritative sv_cheats 1
            if (!sp_cheats || (isPublicMultiplayerServer && !sv_cheats)) {
                print("Can't use cheat cvar " + p + "_" + n + " in multiplayer, unless the server has sp_cheats set to 1.",
                      Vec3(1.0f, 0.35f, 0.35f));
                return;
            }

            if (n == "god") {
                int b = parseBool(args);
                sp_god = (b >= 0) ? (b == 1) : !sp_god;
                print(std::string("godmode ") + (sp_god ? "ON" : "OFF"), Vec3(0.92f, 0.92f, 0.92f));
            } else if (n == "noclip") {
                int b = parseBool(args);
                sp_noclip = (b >= 0) ? (b == 1) : !sp_noclip;
                print(std::string("noclip ") + (sp_noclip ? "ON" : "OFF"), Vec3(0.92f, 0.92f, 0.92f));
            } else if (n == "speed") {
                try {
                    float s = std::stof(args);
                    sp_speed = std::clamp(s, 0.1f, 10.0f);
                    print("sp_speed = " + std::to_string(sp_speed), Vec3(0.92f, 0.92f, 0.92f));
                } catch (...) {
                    print("sp_speed currently: " + std::to_string(sp_speed), Vec3(0.75f, 0.75f, 0.75f));
                }
            } else if (n == "jump") {
                try {
                    float j = std::stof(args);
                    sp_jump = std::clamp(j, 1.0f, 30.0f);
                    print("sp_jump = " + std::to_string(sp_jump), Vec3(0.92f, 0.92f, 0.92f));
                } catch (...) {
                    print("sp_jump currently: " + std::to_string(sp_jump), Vec3(0.75f, 0.75f, 0.75f));
                }
            } else if (n == "heal") {
                int val = 100;
                try { if (!args.empty()) val = std::stoi(args); } catch (...) {}
                if (onHeal) onHeal(val);
                print("Giving player " + std::to_string(val) + " health", Vec3(0.92f, 0.92f, 0.92f));
            } else if (n == "armor") {
                int val = 100;
                try { if (!args.empty()) val = std::stoi(args); } catch (...) {}
                if (onArmor) onArmor(val);
                print("Giving player " + std::to_string(val) + " suit armor", Vec3(0.92f, 0.92f, 0.92f));
            } else if (n == "ammo") {
                int val = 250;
                try { if (!args.empty()) val = std::stoi(args); } catch (...) {}
                if (onAmmo) onAmmo(val);
                print("Ammo replenished (" + std::to_string(val) + " rounds)", Vec3(0.92f, 0.92f, 0.92f));
            } else if (n == "get" || n == "give") {
                try {
                    int wId = std::stoi(args);
                    if (onGetWeapon) onGetWeapon(wId);
                    print("Acquired weapon ID #" + std::to_string(wId), Vec3(0.92f, 0.92f, 0.92f));
                } catch (...) {
                    print("Usage: sp_get <weapon_id> (101..109)", Vec3(1.0f, 0.4f, 0.4f));
                }
            } else if (n == "give_all" || n == "impulse101") {
                if (onGiveAll) onGiveAll();
                print("All weapons and maximum ammunition unlocked!", Vec3(0.92f, 0.92f, 0.92f));
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
                log_colliders = state;
                log_triggers = state;
                log_ai = state;
                log_drawcalls = state;
                log_memory = state;
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
            } else if (n == "skybox") {
                if (args.empty()) {
                    print("wrd_skybox currently: " + wrd_skybox, Vec3(0.7f, 0.7f, 0.7f));
                } else {
                    std::string hexCand = args;
                    if (!hexCand.empty() && hexCand[0] == '#') hexCand = hexCand.substr(1);
                    bool isHex = (hexCand.size() == 6 || hexCand.size() == 3) &&
                        std::all_of(hexCand.begin(), hexCand.end(), [](char c){
                            return std::isxdigit(static_cast<unsigned char>(c));
                        });

                    if (isHex) {
                        Vec3 col(0.0f, 0.0f, 0.0f);
                        if (hexCand.size() == 6) {
                            unsigned int r = 0, g = 0, b = 0;
                            auto [rp, rec] = std::from_chars(hexCand.data(), hexCand.data() + 2, r, 16);
                            auto [gp, gec] = std::from_chars(hexCand.data() + 2, hexCand.data() + 4, g, 16);
                            auto [bp, bec] = std::from_chars(hexCand.data() + 4, hexCand.data() + 6, b, 16);
                            if (rec == std::errc{} && gec == std::errc{} && bec == std::errc{}) {
                                col = Vec3(static_cast<float>(r) / 255.0f,
                                           static_cast<float>(g) / 255.0f,
                                           static_cast<float>(b) / 255.0f);
                            }
                        } else if (hexCand.size() == 3) {
                            unsigned int r = 0, g = 0, b = 0;
                            std::string rStr(2, hexCand[0]);
                            std::string gStr(2, hexCand[1]);
                            std::string bStr(2, hexCand[2]);
                            std::from_chars(rStr.data(), rStr.data() + 2, r, 16);
                            std::from_chars(gStr.data(), gStr.data() + 2, g, 16);
                            std::from_chars(bStr.data(), bStr.data() + 2, b, 16);
                            col = Vec3(static_cast<float>(r) / 255.0f,
                                       static_cast<float>(g) / 255.0f,
                                       static_cast<float>(b) / 255.0f);
                        }
                        wrd_skybox = "#" + hexCand;
                        wrd_skybox_color = col;
                        wrd_skybox_is_color = true;
                        if (onSetSkybox) onSetSkybox(wrd_skybox);
                        print("Skybox background color set to " + wrd_skybox + " (RGB: " +
                              std::to_string(col.x) + ", " + std::to_string(col.y) + ", " + std::to_string(col.z) + ")",
                              Vec3(0.85f, 0.85f, 0.85f));
                    } else {
                        wrd_skybox = args;
                        wrd_skybox_is_color = false;
                        if (onSetSkybox) onSetSkybox(wrd_skybox);
                        print("Skybox cubemap/asset set to " + wrd_skybox, Vec3(0.85f, 0.85f, 0.85f));
                    }
                }
            } else if (n == "model") {
                if (!args.empty()) {
                    if (onSpawnModel) onSpawnModel(args);
                    print("Spawned model '" + args + "' at aim position", Vec3(0.85f, 0.85f, 0.85f));
                } else {
                    print("Usage: wrd_model <id/path>", Vec3(1.0f, 0.4f, 0.4f));
                }
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
            } else if (n == "fog_density") {
                try {
                    float fd = std::stof(args);
                    wrd_fog_density = std::clamp(fd, 0.0001f, 1.0f);
                    print("wrd_fog_density = " + std::to_string(wrd_fog_density), Vec3(0.85f, 0.85f, 0.85f));
                } catch (...) {
                    print("wrd_fog_density currently: " + std::to_string(wrd_fog_density), Vec3(0.7f, 0.7f, 0.7f));
                }
            } else if (n == "gravity") {
                try {
                    wrd_gravity = std::stof(args);
                    phys_gravity = std::abs(wrd_gravity);
                    print("wrd_gravity = " + std::to_string(wrd_gravity), Vec3(0.85f, 0.85f, 0.85f));
                } catch (...) {
                    print("wrd_gravity currently: " + std::to_string(wrd_gravity), Vec3(0.7f, 0.7f, 0.7f));
                }
            } else if (n == "light_ambient") {
                if (args.empty()) {
                    print("wrd_light_ambient currently: (" + std::to_string(wrd_light_ambient.x) + ", " +
                          std::to_string(wrd_light_ambient.y) + ", " + std::to_string(wrd_light_ambient.z) + ")",
                          Vec3(0.7f, 0.7f, 0.7f));
                } else {
                    std::string hexCand = args;
                    if (!hexCand.empty() && hexCand[0] == '#') hexCand = hexCand.substr(1);
                    if (hexCand.size() == 6 && std::all_of(hexCand.begin(), hexCand.end(), [](char c){ return std::isxdigit(static_cast<unsigned char>(c)); })) {
                        unsigned int r = 0, g = 0, b = 0;
                        std::from_chars(hexCand.data(), hexCand.data() + 2, r, 16);
                        std::from_chars(hexCand.data() + 2, hexCand.data() + 4, g, 16);
                        std::from_chars(hexCand.data() + 4, hexCand.data() + 6, b, 16);
                        wrd_light_ambient = Vec3(static_cast<float>(r) / 255.0f,
                                                 static_cast<float>(g) / 255.0f,
                                                 static_cast<float>(b) / 255.0f);
                    } else {
                        std::istringstream aiss(args);
                        float r = 0.2f, g = 0.2f, b = 0.2f;
                        if (aiss >> r) {
                            if (aiss >> g && aiss >> b) {
                                wrd_light_ambient = Vec3(r, g, b);
                            } else {
                                wrd_light_ambient = Vec3(r, r, r);
                            }
                        }
                    }
                    print("wrd_light_ambient = (" + std::to_string(wrd_light_ambient.x) + ", " +
                          std::to_string(wrd_light_ambient.y) + ", " + std::to_string(wrd_light_ambient.z) + ")",
                          Vec3(0.85f, 0.85f, 0.85f));
                }
            } else if (n == "postprocess") {
                int b = parseBool(args);
                wrd_postprocess = (b >= 0) ? (b == 1) : !wrd_postprocess;
                r_postprocess = wrd_postprocess;
                print(std::string("wrd_postprocess = ") + (wrd_postprocess ? "1" : "0"), Vec3(0.85f, 0.85f, 0.85f));
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
                } catch (...) {
                    print("phys_friction currently: " + std::to_string(phys_friction), Vec3(0.7f, 0.7f, 0.7f));
                }
            } else if (n == "air_accel") {
                try {
                    phys_air_accel = std::stof(args);
                    print("phys_air_accel = " + std::to_string(phys_air_accel), Vec3(0.85f, 0.85f, 0.85f));
                } catch (...) {
                    print("phys_air_accel currently: " + std::to_string(phys_air_accel), Vec3(0.7f, 0.7f, 0.7f));
                }
            } else if (n == "bhop_window") {
                try {
                    phys_bhop_window = std::stof(args) / 1000.0f; // ms to s
                    print("phys_bhop_window = " + std::to_string((int)(phys_bhop_window * 1000.0f)) + " ms", Vec3(0.85f, 0.85f, 0.85f));
                } catch (...) {
                    print("phys_bhop_window currently: " + std::to_string((int)(phys_bhop_window * 1000.0f)) + " ms", Vec3(0.7f, 0.7f, 0.7f));
                }
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
                    wrd_gravity = -phys_gravity;
                    print("phys_gravity = " + std::to_string(phys_gravity), Vec3(0.85f, 0.85f, 0.85f));
                } catch (...) {
                    print("phys_gravity currently: " + std::to_string(phys_gravity), Vec3(0.7f, 0.7f, 0.7f));
                }
            } else if (n == "step_height") {
                try {
                    phys_step_height = std::stof(args);
                    print("phys_step_height = " + std::to_string(phys_step_height), Vec3(0.85f, 0.85f, 0.85f));
                } catch (...) {
                    print("phys_step_height currently: " + std::to_string(phys_step_height), Vec3(0.7f, 0.7f, 0.7f));
                }
            } else if (n == "ragdoll_force") {
                try {
                    phys_ragdoll_force = std::stof(args);
                    print("phys_ragdoll_force = " + std::to_string(phys_ragdoll_force), Vec3(0.85f, 0.85f, 0.85f));
                } catch (...) {
                    print("phys_ragdoll_force currently: " + std::to_string(phys_ragdoll_force), Vec3(0.7f, 0.7f, 0.7f));
                }
            } else if (n == "debug") {
                int b = parseBool(args);
                phys_debug = (b >= 0) ? (b == 1) : !phys_debug;
                print(std::string("phys_debug = ") + (phys_debug ? "1" : "0"), Vec3(0.85f, 0.85f, 0.85f));
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
                } catch (...) {
                    print("cam_fov currently: " + std::to_string((int)cam_fov), Vec3(0.7f, 0.7f, 0.7f));
                }
            } else if (n == "viewmodel_fov") {
                try {
                    float f = std::stof(args);
                    cam_viewmodel_fov = std::clamp(f, 30.0f, 120.0f);
                    print("cam_viewmodel_fov = " + std::to_string((int)cam_viewmodel_fov), Vec3(0.85f, 0.85f, 0.85f));
                } catch (...) {
                    print("cam_viewmodel_fov currently: " + std::to_string((int)cam_viewmodel_fov), Vec3(0.7f, 0.7f, 0.7f));
                }
            } else if (n == "viewmodel_offset") {
                std::istringstream oiss(args);
                float x = 0.0f, y = 0.0f, z = 0.0f;
                if (oiss >> x >> y >> z) {
                    cam_viewmodel_offset = Vec3(x, y, z);
                    print("cam_viewmodel_offset = (" + std::to_string(x) + ", " + std::to_string(y) + ", " + std::to_string(z) + ")", Vec3(0.85f, 0.85f, 0.85f));
                } else {
                    print("cam_viewmodel_offset currently: (" + std::to_string(cam_viewmodel_offset.x) + ", " +
                          std::to_string(cam_viewmodel_offset.y) + ", " + std::to_string(cam_viewmodel_offset.z) + ")", Vec3(0.7f, 0.7f, 0.7f));
                }
            } else if (n == "thirdperson") {
                int b = parseBool(args);
                cam_thirdperson = (b >= 0) ? (b == 1) : !cam_thirdperson;
                print(std::string("cam_thirdperson = ") + (cam_thirdperson ? "1" : "0"), Vec3(0.85f, 0.85f, 0.85f));
            } else if (n == "bob") {
                int b = parseBool(args);
                cam_bob = (b >= 0) ? (b == 1) : !cam_bob;
                print(std::string("cam_bob = ") + (cam_bob ? "1" : "0"), Vec3(0.85f, 0.85f, 0.85f));
            } else if (n == "shake") {
                float force = 1.0f;
                try { if (!args.empty()) force = std::stof(args); } catch (...) {}
                if (onCamShake) onCamShake(force);
                print("Camera shake triggered (force=" + std::to_string(force) + ")", Vec3(0.85f, 0.85f, 0.85f));
            } else if (n == "freecam") {
                int b = parseBool(args);
                cam_freecam = (b >= 0) ? (b == 1) : !cam_freecam;
                print(std::string("cam_freecam = ") + (cam_freecam ? "1" : "0"), Vec3(0.85f, 0.85f, 0.85f));
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
            } else if (n == "vsync") {
                int b = parseBool(args);
                r_vsync = (b >= 0) ? (b == 1) : !r_vsync;
                if (onSetVsync) onSetVsync(r_vsync);
                print(std::string("r_vsync = ") + (r_vsync ? "1" : "0"), Vec3(0.85f, 0.85f, 0.85f));
            } else if (n == "shadows") {
                int b = parseBool(args);
                r_shadows = (b >= 0) ? (b == 1) : !r_shadows;
                print(std::string("r_shadows = ") + (r_shadows ? "1" : "0"), Vec3(0.85f, 0.85f, 0.85f));
            } else if (n == "shadow_res") {
                try {
                    int res = std::stoi(args);
                    if (res == 512 || res == 1024 || res == 2048 || res == 4096) {
                        r_shadow_res = res;
                        print("r_shadow_res = " + std::to_string(r_shadow_res), Vec3(0.85f, 0.85f, 0.85f));
                    } else {
                        print("Usage: r_shadow_res 512/1024/2048/4096", Vec3(1.0f, 0.4f, 0.4f));
                    }
                } catch (...) {
                    print("r_shadow_res currently: " + std::to_string(r_shadow_res), Vec3(0.7f, 0.7f, 0.7f));
                }
            } else if (n == "lightmaps") {
                int b = parseBool(args);
                r_lightmaps = (b >= 0) ? (b == 1) : !r_lightmaps;
                print(std::string("r_lightmaps = ") + (r_lightmaps ? "1" : "0"), Vec3(0.85f, 0.85f, 0.85f));
            } else if (n == "normalmaps") {
                int b = parseBool(args);
                r_normalmaps = (b >= 0) ? (b == 1) : !r_normalmaps;
                print(std::string("r_normalmaps = ") + (r_normalmaps ? "1" : "0"), Vec3(0.85f, 0.85f, 0.85f));
            } else if (n == "cubemaps") {
                int b = parseBool(args);
                r_cubemaps = (b >= 0) ? (b == 1) : !r_cubemaps;
                print(std::string("r_cubemaps = ") + (r_cubemaps ? "1" : "0"), Vec3(0.85f, 0.85f, 0.85f));
            } else if (n == "postprocess") {
                int b = parseBool(args);
                r_postprocess = (b >= 0) ? (b == 1) : !r_postprocess;
                wrd_postprocess = r_postprocess;
                print(std::string("r_postprocess = ") + (r_postprocess ? "1" : "0"), Vec3(0.85f, 0.85f, 0.85f));
            } else if (n == "reload_shaders") {
                if (onReloadShaders) onReloadShaders();
                print("Reloading all shaders...", Vec3(0.3f, 0.95f, 0.4f));
            } else {
                print("Unknown renderer command 'r_" + n + "'", Vec3(1.0f, 0.35f, 0.35f));
            }
            return;
        }

        // =====================================================================
        // AI_ (ARTIFICIAL INTELLIGENCE & BOTS)
        // =====================================================================
        if (p == "ai") {
            if (n == "disable") {
                int b = parseBool(args);
                ai_disable = (b >= 0) ? (b == 1) : !ai_disable;
                print(std::string("ai_disable = ") + (ai_disable ? "1" : "0"), Vec3(0.85f, 0.85f, 0.85f));
            } else if (n == "freeze") {
                int b = parseBool(args);
                ai_freeze = (b >= 0) ? (b == 1) : !ai_freeze;
                if (onFreezeBots) onFreezeBots(ai_freeze);
                print(std::string("ai_freeze = ") + (ai_freeze ? "1" : "0"), Vec3(0.85f, 0.85f, 0.85f));
            } else if (n == "sight") {
                int b = parseBool(args);
                ai_sight = (b >= 0) ? (b == 1) : !ai_sight;
                print(std::string("ai_sight = ") + (ai_sight ? "1" : "0"), Vec3(0.85f, 0.85f, 0.85f));
            } else if (n == "paths") {
                int b = parseBool(args);
                ai_paths = (b >= 0) ? (b == 1) : !ai_paths;
                print(std::string("ai_paths = ") + (ai_paths ? "1" : "0"), Vec3(0.85f, 0.85f, 0.85f));
            } else if (n == "aggro") {
                int b = parseBool(args);
                ai_aggro = (b >= 0) ? (b == 1) : !ai_aggro;
                print(std::string("ai_aggro = ") + (ai_aggro ? "1" : "0"), Vec3(0.85f, 0.85f, 0.85f));
            } else if (n == "ignore_player") {
                int b = parseBool(args);
                ai_ignore_player = (b >= 0) ? (b == 1) : !ai_ignore_player;
                print(std::string("ai_ignore_player = ") + (ai_ignore_player ? "1" : "0"), Vec3(0.85f, 0.85f, 0.85f));
            } else if (n == "spawn") {
                if (!args.empty()) {
                    if (onSpawnAi) onSpawnAi(args);
                    print("Spawned AI bot of type '" + args + "'", Vec3(0.85f, 0.85f, 0.85f));
                } else {
                    print("Usage: ai_spawn <type>", Vec3(1.0f, 0.4f, 0.4f));
                }
            } else {
                print("Unknown AI command 'ai_" + n + "'", Vec3(1.0f, 0.35f, 0.35f));
            }
            return;
        }

        // =====================================================================
        // SND_ (AUDIO 3D, VOLUME & PLAYBACK)
        // =====================================================================
        if (p == "snd") {
            if (n == "volume") {
                try {
                    float v = std::stof(args);
                    snd_volume = std::clamp(v, 0.0f, 1.0f);
                    if (onSetMasterVolume) onSetMasterVolume(snd_volume);
                    print("snd_volume = " + std::to_string(snd_volume), Vec3(0.85f, 0.85f, 0.85f));
                } catch (...) {
                    print("snd_volume currently: " + std::to_string(snd_volume), Vec3(0.7f, 0.7f, 0.7f));
                }
            } else if (n == "music") {
                try {
                    float v = std::stof(args);
                    snd_music = std::clamp(v, 0.0f, 1.0f);
                    print("snd_music = " + std::to_string(snd_music), Vec3(0.85f, 0.85f, 0.85f));
                } catch (...) {
                    print("snd_music currently: " + std::to_string(snd_music), Vec3(0.7f, 0.7f, 0.7f));
                }
            } else if (n == "sfx") {
                try {
                    float v = std::stof(args);
                    snd_sfx = std::clamp(v, 0.0f, 1.0f);
                    print("snd_sfx = " + std::to_string(snd_sfx), Vec3(0.85f, 0.85f, 0.85f));
                } catch (...) {
                    print("snd_sfx currently: " + std::to_string(snd_sfx), Vec3(0.7f, 0.7f, 0.7f));
                }
            } else if (n == "voice") {
                try {
                    float v = std::stof(args);
                    snd_voice = std::clamp(v, 0.0f, 1.0f);
                    print("snd_voice = " + std::to_string(snd_voice), Vec3(0.85f, 0.85f, 0.85f));
                } catch (...) {
                    print("snd_voice currently: " + std::to_string(snd_voice), Vec3(0.7f, 0.7f, 0.7f));
                }
            } else if (n == "play") {
                if (!args.empty()) {
                    if (onPlaySound) onPlaySound(args);
                    print("Playing sound: " + args, Vec3(0.85f, 0.85f, 0.85f));
                } else {
                    print("Usage: snd_play <path_to_audio>", Vec3(1.0f, 0.4f, 0.4f));
                }
            } else if (n == "stop_all") {
                if (onStopAllSounds) onStopAllSounds();
                print("Silenced all active audio channels.", Vec3(0.85f, 0.85f, 0.85f));
            } else if (n == "lipsync_test") {
                if (onLipsyncTest) onLipsyncTest(args);
                print("Testing lip-sync playback: " + args, Vec3(0.85f, 0.85f, 0.85f));
            } else if (n == "restart") {
                if (onRestartAudio) onRestartAudio();
                print("Restarting audio engine...", Vec3(0.85f, 0.85f, 0.85f));
            } else {
                print("Unknown sound command 'snd_" + n + "'", Vec3(1.0f, 0.35f, 0.35f));
            }
            return;
        }

        // =====================================================================
        // UI_ (2D INTERFACE, MENU & HUD)
        // =====================================================================
        if (p == "ui") {
            if (n == "hide") {
                int b = parseBool(args);
                ui_hide = (b >= 0) ? (b == 1) : !ui_hide;
                print(std::string("ui_hide = ") + (ui_hide ? "1" : "0"), Vec3(0.85f, 0.85f, 0.85f));
            } else if (n == "crosshair") {
                int b = parseBool(args);
                ui_crosshair = (b >= 0) ? (b == 1) : !ui_crosshair;
                print(std::string("ui_crosshair = ") + (ui_crosshair ? "1" : "0"), Vec3(0.85f, 0.85f, 0.85f));
            } else if (n == "crosshair_color") {
                if (args.empty()) {
                    print("ui_crosshair_color currently: " + ui_crosshair_color, Vec3(0.7f, 0.7f, 0.7f));
                } else {
                    std::string hexCand = args;
                    if (!hexCand.empty() && hexCand[0] == '#') hexCand = hexCand.substr(1);
                    if (hexCand.size() == 6 && std::all_of(hexCand.begin(), hexCand.end(), [](char c){ return std::isxdigit(static_cast<unsigned char>(c)); })) {
                        unsigned int r = 0, g = 0, b = 0;
                        std::from_chars(hexCand.data(), hexCand.data() + 2, r, 16);
                        std::from_chars(hexCand.data() + 2, hexCand.data() + 4, g, 16);
                        std::from_chars(hexCand.data() + 4, hexCand.data() + 6, b, 16);
                        ui_crosshair_color = "#" + hexCand;
                        ui_crosshair_color_rgb = Vec3(static_cast<float>(r) / 255.0f,
                                                      static_cast<float>(g) / 255.0f,
                                                      static_cast<float>(b) / 255.0f);
                        print("ui_crosshair_color set to " + ui_crosshair_color, Vec3(0.85f, 0.85f, 0.85f));
                    } else {
                        print("Usage: ui_crosshair_color <#hex> (e.g. #00ff00)", Vec3(1.0f, 0.4f, 0.4f));
                    }
                }
            } else if (n == "scale") {
                try {
                    float s = std::stof(args);
                    ui_scale = std::clamp(s, 0.25f, 4.0f);
                    print("ui_scale = " + std::to_string(ui_scale), Vec3(0.85f, 0.85f, 0.85f));
                } catch (...) {
                    print("ui_scale currently: " + std::to_string(ui_scale), Vec3(0.7f, 0.7f, 0.7f));
                }
            } else if (n == "menu") {
                if (!args.empty()) {
                    if (onOpenMenu) onOpenMenu(args);
                    print("Navigating to menu: " + args, Vec3(0.85f, 0.85f, 0.85f));
                } else {
                    print("Usage: ui_menu <main/pause/options>", Vec3(1.0f, 0.4f, 0.4f));
                }
            } else if (n == "reload") {
                if (onReloadUi) onReloadUi();
                print("Reloading UI layout...", Vec3(0.85f, 0.85f, 0.85f));
            } else {
                print("Unknown UI command 'ui_" + n + "'", Vec3(1.0f, 0.35f, 0.35f));
            }
            return;
        }

        // =====================================================================
        // DCL_ (DECALS, BULLET IMPACTS & BLOOD)
        // =====================================================================
        if (p == "dcl") {
            if (n == "clear") {
                if (onClearDecals) onClearDecals();
                print("All decals cleared from map.", Vec3(0.85f, 0.85f, 0.85f));
            } else if (n == "limit") {
                try {
                    int lim = std::stoi(args);
                    dcl_limit = std::clamp(lim, 16, 2048);
                    print("dcl_limit = " + std::to_string(dcl_limit), Vec3(0.85f, 0.85f, 0.85f));
                } catch (...) {
                    print("dcl_limit currently: " + std::to_string(dcl_limit), Vec3(0.7f, 0.7f, 0.7f));
                }
            } else if (n == "spawn") {
                if (!args.empty()) {
                    if (onSpawnDecal) onSpawnDecal(args);
                    print("Spawned decal of type '" + args + "' at aim position.", Vec3(0.85f, 0.85f, 0.85f));
                } else {
                    print("Usage: dcl_spawn <blood|bullet|dirt>", Vec3(1.0f, 0.4f, 0.4f));
                }
            } else if (n == "distance") {
                try {
                    float dist = std::stof(args);
                    dcl_distance = std::clamp(dist, 1.0f, 500.0f);
                    print("dcl_distance = " + std::to_string(dcl_distance) + " m", Vec3(0.85f, 0.85f, 0.85f));
                } catch (...) {
                    print("dcl_distance currently: " + std::to_string(dcl_distance) + " m", Vec3(0.7f, 0.7f, 0.7f));
                }
            } else {
                print("Unknown decal command 'dcl_" + n + "'", Vec3(1.0f, 0.35f, 0.35f));
            }
            return;
        }

        // =====================================================================
        // MP_ (MULTIPLAYER MATCH & BOTS)
        // =====================================================================
        if (p == "mp") {
            if (n == "a_bots" || n == "b_bots") {
                try {
                    int c = std::stoi(args);
                    if (n == "a_bots" && onSetBotsA) onSetBotsA(c);
                    if (n == "b_bots" && onSetBotsB) onSetBotsB(c);
                    print("Set bot count to " + std::to_string(c), Vec3(0.85f, 0.85f, 0.85f));
                } catch (...) {
                    print("Usage: mp_" + n + " <count>", Vec3(1.0f, 0.4f, 0.4f));
                }
            } else if (n == "bot_kick_all" || n == "kick_all") {
                if (onKickBots) onKickBots();
                print("Kicked all bots.", Vec3(1.0f, 0.4f, 0.4f));
            } else if (n == "bot_freeze" || n == "freeze") {
                int b = parseBool(args);
                bool freeze = (b >= 0) ? (b == 1) : true;
                ai_freeze = freeze;
                if (onFreezeBots) onFreezeBots(freeze);
                print(std::string("mp_bot_freeze = ") + (freeze ? "1" : "0"), Vec3(0.85f, 0.85f, 0.85f));
            } else if (n == "restart") {
                if (onRestartRound) onRestartRound();
                print("Restarting match round...", Vec3(0.85f, 0.85f, 0.85f));
            } else if (n == "friendly_fire") {
                int b = parseBool(args);
                mp_friendly_fire = (b >= 0) ? (b == 1) : !mp_friendly_fire;
                print(std::string("mp_friendly_fire = ") + (mp_friendly_fire ? "1" : "0"), Vec3(0.85f, 0.85f, 0.85f));
            } else {
                print("Unknown multiplayer command 'mp_" + n + "'", Vec3(1.0f, 0.35f, 0.35f));
            }
            return;
        }

        // =====================================================================
        // NET_ (NETWORK DIAGNOSTICS & LATENCY)
        // =====================================================================
        if (p == "net") {
            if (n == "status") {
                print("Network status: Tickrate: 64 Hz | Ping: 0 ms | Loss: " + std::to_string((int)net_fake_loss) + "% | Jitter: 1.2 ms", Vec3(0.85f, 0.85f, 0.85f));
            } else if (n == "fake_lag") {
                try {
                    net_fake_lag = std::clamp(std::stoi(args), 0, 1000);
                    print("net_fake_lag = " + std::to_string(net_fake_lag) + " ms", Vec3(0.85f, 0.85f, 0.85f));
                } catch (...) {
                    print("net_fake_lag currently: " + std::to_string(net_fake_lag) + " ms", Vec3(0.7f, 0.7f, 0.7f));
                }
            } else if (n == "fake_loss") {
                try {
                    net_fake_loss = std::clamp(std::stof(args), 0.0f, 100.0f);
                    print("net_fake_loss = " + std::to_string((int)net_fake_loss) + "%", Vec3(0.85f, 0.85f, 0.85f));
                } catch (...) {
                    print("net_fake_loss currently: " + std::to_string((int)net_fake_loss) + "%", Vec3(0.7f, 0.7f, 0.7f));
                }
            } else if (n == "connect") {
                if (!args.empty()) {
                    if (onNetConnect) onNetConnect(args);
                    print("Connecting to server: " + args + "...", Vec3(0.85f, 0.85f, 0.85f));
                } else {
                    print("Usage: net_connect <ip:port>", Vec3(1.0f, 0.4f, 0.4f));
                }
            } else if (n == "disconnect") {
                if (onNetDisconnect) onNetDisconnect();
                print("Disconnected from server.", Vec3(0.85f, 0.85f, 0.85f));
            } else {
                print("Unknown network command 'net_" + n + "'", Vec3(1.0f, 0.35f, 0.35f));
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
            } else if (n == "copylog" || n == "condump") {
                copyLogToClipboard();
                return;
            } else if (n == "quit" || n == "exit") {
                if (onQuit) onQuit();
                return;
            } else if (n == "help") {
                handleHelpCommand(args);
                return;
            } else if (n == "bind") {
                std::istringstream biss(args);
                std::string keyName;
                biss >> keyName;
                std::string bindCmd;
                std::getline(biss, bindCmd);
                size_t bcStart = bindCmd.find_first_not_of(" \t\"");
                if (bcStart != std::string::npos) {
                    size_t bcEnd = bindCmd.find_last_not_of(" \t\"");
                    bindCmd = bindCmd.substr(bcStart, bcEnd - bcStart + 1);
                }
                if (!keyName.empty() && !bindCmd.empty()) {
                    std::transform(keyName.begin(), keyName.end(), keyName.begin(), [](unsigned char c){ return (char)std::tolower(c); });
                    _keyBinds[keyName] = bindCmd;
                    print("Bound key '" + keyName + "' to command '" + bindCmd + "'", Vec3(0.85f, 0.85f, 0.85f));
                } else {
                    print("Usage: sys_bind <key> <command>", Vec3(1.0f, 0.4f, 0.4f));
                }
                return;
            }
        }

        print("Unknown command \"" + prefix + "_" + name + "\". Type 'help' for available commands.", Vec3(1.0f, 0.35f, 0.35f));
    }

    void LabDevConsole::handleHelpCommand(const std::string& rawFilter) {
        std::string filter = rawFilter;
        size_t fStart = filter.find_first_not_of(" \t");
        if (fStart != std::string::npos) filter = filter.substr(fStart);
        else filter.clear();
        size_t fEnd = filter.find_last_not_of(" \t");
        if (fEnd != std::string::npos) filter = filter.substr(0, fEnd + 1);
        std::transform(filter.begin(), filter.end(), filter.begin(), [](unsigned char c) { return (char)std::tolower(c); });

        // Strip trailing underscore if user typed 'help sp_'
        if (!filter.empty() && filter.back() == '_') {
            filter.pop_back();
        }

        struct CmdDoc {
            std::string cmd;
            std::string prefix;
            std::string usage;
            std::string desc;
        };

        const std::vector<CmdDoc> docs = {
            // SP_ (SUPER / CHEATS & PLAYER)
            { "sp_cheats", "sp", "0/1", "Unlock developer cheats and debug commands (Required)" },
            { "sp_god", "sp", "0/1", "Toggle player invincibility" },
            { "sp_noclip", "sp", "0/1", "Flight and noclip through walls" },
            { "sp_bhop", "sp", "0/1", "Toggle bunny hop momentum preservation physics" },
            { "sp_speed", "sp", "{val}", "Movement speed multiplier (1.0 = normal, 1.5 = fast)" },
            { "sp_jump", "sp", "{val}", "Jump vertical impulse height (default: 5.2)" },
            { "sp_get", "sp", "{id}", "Give weapon by ID (1=Pipe, 2=Shotgun, 3=M4A4, 8=RPG...)" },
            { "sp_heal", "sp", "{amt}", "Restore or set player health points" },
            { "sp_armor", "sp", "{amt}", "Add or set player armor points" },
            { "sp_ammo", "sp", "{amt}", "Refill ammunition for current or all weapons" },
            { "sp_give_all", "sp", "", "Equip all weapons with maximum ammunition" },
            { "sp_respawn", "sp", "", "Instant player respawn at level spawn point" },
            { "sp_suicide", "sp", "", "Kill player to test respawn kinematics and death screen" },

            // LOG_ (HUD 2D TELEMETRY OVERLAYS)
            { "log_momentum", "log", "0/1", "Real-time horizontal speed (u/s) & bhop telemetry overlay" },
            { "log_fps", "log", "0/1", "FPS counter and frametime in milliseconds" },
            { "log_ping", "log", "0/1", "Network latency and ping diagnostics in milliseconds" },
            { "log_pos", "log", "0/1", "Player coordinates (X, Y, Z) and camera pitch/yaw angles" },
            { "log_colliders", "log", "0/1", "Visualize 3D collision AABB wireframe bounding boxes" },
            { "log_triggers", "log", "0/1", "Visualize 3D trigger volume boundaries" },
            { "log_ai", "log", "0/1", "Display AI bot state, goal target & navigation waypoint" },
            { "log_drawcalls", "log", "0/1", "Total draw calls and rendered vertex count per frame" },
            { "log_memory", "log", "0/1", "RAM and VRAM memory usage by textures and geometry" },
            { "log_all", "log", "0/1", "Master switch to toggle all 2D telemetry HUD overlays" },

            // WRD_ (WORLD, MAP & ENVIRONMENT)
            { "wrd_map", "wrd", "{name}", "Load specified scene or map (e.g. site14_chapter1)" },
            { "wrd_restart", "wrd", "", "Restart current map to initial spawn state" },
            { "wrd_skybox", "wrd", "{hex/name}", "Change skybox background color or cubemap asset" },
            { "wrd_model", "wrd", "{path}", "Spawn 3D prop/mesh at crosshair aim position" },
            { "wrd_timescale", "wrd", "{val}", "Engine timescale multiplier (0.5 slow-mo, 1.0 normal, 2.0 fast)" },
            { "wrd_wireframe", "wrd", "0/1", "Toggle polygon wireframe rendering mode" },
            { "wrd_fog", "wrd", "0/1", "Toggle atmospheric corridor depth and height fog" },
            { "wrd_fog_density", "wrd", "{val}", "Fog density coefficient (default: 0.02)" },
            { "wrd_light_ambient", "wrd", "{hex/rgb}", "Scene global ambient lighting color and intensity" },

            // CAM_ (CAMERA, VIEWMODEL & FOV)
            { "cam_fov", "cam", "{deg}", "Main camera field of view in degrees (default: 75)" },
            { "cam_viewmodel_fov", "cam", "{deg}", "Viewmodel weapon camera field of view" },
            { "cam_viewmodel_offset", "cam", "{x y z}", "Weapon screen holding position offset" },
            { "cam_thirdperson", "cam", "0/1", "Toggle third-person (TPP) over-the-shoulder camera" },
            { "cam_bob", "cam", "0/1", "Toggle weapon and camera bobbing while moving" },
            { "cam_shake", "cam", "{force}", "Trigger camera screen shake impulse" },
            { "cam_freecam", "cam", "0/1", "Detach camera from player for free-flight scene inspection" },

            // R_ (RENDERER, SHADERS & LOW-LEVEL GRAPHICS)
            { "r_bloom", "r", "0/1", "Toggle subtle HDR bloom post-processing effect" },
            { "r_fxaa", "r", "0/1", "Toggle fast approximate anti-aliasing (FXAA 3.11)" },
            { "r_vsync", "r", "0/1", "Toggle vertical synchronization with monitor refresh rate" },
            { "r_shadows", "r", "0/1", "Toggle dynamic shadow map rendering and depth pass" },
            { "r_shadow_res", "r", "{res}", "Shadow map texture resolution (512, 1024, 2048)" },
            { "r_lightmaps", "r", "0/1", "Toggle baked static surface lightmaps" },
            { "r_normalmaps", "r", "0/1", "Toggle surface normal maps and tangent space lighting" },
            { "r_cubemaps", "r", "0/1", "Toggle environment cubemap reflections" },
            { "r_postprocess", "r", "0/1", "Master switch for all post-processing effects" },
            { "r_reload_shaders", "r", "", "Hot-reload all GLSL shaders from disk without restart" },

            // PHYS_ (PHYSICS, GRAVITY & BHOP MOMENTUM)
            { "phys_gravity", "phys", "{val}", "Gravitational acceleration (default: 12.0)" },
            { "phys_friction", "phys", "{val}", "Ground surface friction (default: 5.5, lower = slippery)" },
            { "phys_air_accel", "phys", "{val}", "Air strafe acceleration multiplier (default: 12.0)" },
            { "phys_bhop_window", "phys", "{sec}", "Timing window in seconds for perfect hop (default: 0.08s)" },
            { "phys_max_bhop_speed", "phys", "{val}", "Horizontal speed ceiling to prevent runaway momentum (default: 11.5)" },
            { "phys_step_height", "phys", "{val}", "Maximum obstacle stair step height for smooth climbing" },
            { "phys_ragdoll_force", "phys", "{val}", "Impact impulse force multiplier on ragdoll bodies" },
            { "phys_debug", "phys", "0/1", "Visualize physics velocity vectors and contact normal rays" },

            // AI_ (ARTIFICIAL INTELLIGENCE & BOTS)
            { "ai_disable", "ai", "0/1", "Completely pause AI behavior tree and decision logic" },
            { "ai_freeze", "ai", "0/1", "Freeze AI bot transforms and movement in place" },
            { "ai_sight", "ai", "0/1", "Visualize bot vision cone and perception range in 3D" },
            { "ai_paths", "ai", "0/1", "Draw AI bot waypoint navigation paths in 3D scene" },
            { "ai_aggro", "ai", "0/1", "Force immediate bot hostility against player" },
            { "ai_ignore_player", "ai", "0/1", "Bots ignore player and focus on script scenario" },
            { "ai_spawn", "ai", "{type}", "Spawn an AI bot at crosshair aim position" },

            // SND_ (AUDIO 3D, VOLUME & PLAYBACK)
            { "snd_volume", "snd", "{0.0-1.0}", "Master audio volume level" },
            { "snd_music", "snd", "{0.0-1.0}", "Background music volume level" },
            { "snd_sfx", "snd", "{0.0-1.0}", "Sound effects volume level (gunshots, footsteps, explosions)" },
            { "snd_voice", "snd", "{0.0-1.0}", "Character dialogue and voice volume level" },
            { "snd_play", "snd", "{path}", "Play specified .wav/.ogg audio file directly" },
            { "snd_stop_all", "snd", "", "Stop and silence all active sound playback channels" },
            { "snd_lipsync_test", "snd", "{file}", "Play audio line with test facial lip-sync animation" },
            { "snd_restart", "snd", "", "Re-initialize audio device and sound engine" },

            // UI_ (2D INTERFACE, HUD & SCALING)
            { "ui_hide", "ui", "0/1", "Hide entire HUD and UI for clean cinematic screenshots" },
            { "ui_crosshair", "ui", "0/1", "Toggle weapon crosshair on screen" },
            { "ui_crosshair_color", "ui", "{hex}", "Set crosshair color (e.g. #00ff00 or #ffffff)" },
            { "ui_scale", "ui", "{val}", "UI scaling multiplier for 1080p, 1440p and 4K displays" },
            { "ui_menu", "ui", "{name}", "Open specific 2D menu screen (main, pause, options)" },
            { "ui_reload", "ui", "", "Hot-reload active UI layout from .labui file" },

            // DCL_ (DECALS, BULLET IMPACTS & BLOOD)
            { "dcl_clear", "dcl", "", "Clear all decals, impact marks and blood stains from map" },
            { "dcl_limit", "dcl", "{count}", "Maximum concurrent decal count in memory (default: 256)" },
            { "dcl_spawn", "dcl", "{type}", "Place decal at crosshair: blood, bullet, dirt" },
            { "dcl_distance", "dcl", "{meters}", "Maximum visibility distance for rendering decals" },

            // MP_ (MULTIPLAYER MATCH & BOTS)
            { "mp_a_bots", "mp", "{count}", "Set number of combat bots in Team Alpha" },
            { "mp_b_bots", "mp", "{count}", "Set number of combat bots in Team Bravo" },
            { "mp_bot_kick_all", "mp", "", "Kick and remove all active multiplayer bots" },
            { "mp_bot_freeze", "mp", "0/1", "Freeze or unfreeze all multiplayer bots" },
            { "mp_restart", "mp", "", "Restart current multiplayer match or round" },
            { "mp_friendly_fire", "mp", "0/1", "Toggle friendly fire damage between teammates" },

            // NET_ (NETWORK DIAGNOSTICS & LATENCY)
            { "net_status", "net", "", "Display connection stats: tickrate, packet loss, jitter" },
            { "net_fake_lag", "net", "{ms}", "Simulate artificial latency in milliseconds for QA" },
            { "net_fake_loss", "net", "{0-100}", "Simulate artificial packet loss percentage for QA" },
            { "net_connect", "net", "{ip:port}", "Connect to dedicated multiplayer server" },
            { "net_disconnect", "net", "", "Disconnect from current server and return to main menu" },

            // SYS_ (SYSTEM UTILITIES & CONSOLE)
            { "sys_copylog", "sys", "", "Copy entire console output log to operating system clipboard" },
            { "sys_clear", "sys", "", "Clear all text entries and history from console log" },
            { "sys_help", "sys", "{prefix}", "Display command catalog or filter by category" },
            { "sys_bind", "sys", "{key} {cmd}", "Bind key to console command (e.g. bind f5 wrd_restart)" },
            { "sys_quit", "sys", "", "Immediately exit engine and terminate application" }
        };

        auto printDocLine = [this](const CmdDoc& d) {
            std::string line = "  " + d.cmd;
            if (!d.usage.empty()) line += " " + d.usage;
            while (line.size() < 28) line += ' ';
            line += " - " + d.desc;
            print(line, Vec3(0.85f, 0.88f, 0.85f));
        };

        // Case 1: Empty filter -> Category overview
        if (filter.empty() || filter == "?" || filter == "categories" || filter == "list") {
            print("=== LAB ENGINE DEV CONSOLE COMMAND REFERENCE ===", Vec3(0.95f, 0.85f, 0.25f));
            print("Command syntax: {prefix}_{command} {arguments}", Vec3(0.8f, 0.85f, 0.8f));
            print("----------------------------------------------------------------------", Vec3(0.4f, 0.45f, 0.4f));
            print("AVAILABLE PREFIX CATEGORIES:", Vec3(0.95f, 0.95f, 0.35f));
            print("  sp_   : Cheats, player godmode, noclip, speed, jump, health & ammo", Vec3(0.85f, 0.9f, 0.85f));
            print("  log_  : 2D HUD telemetry diagnostics (momentum, fps, ping, pos, ai...)", Vec3(0.85f, 0.9f, 0.85f));
            print("  wrd_  : World environment, map loading, timescale, skybox, fog", Vec3(0.85f, 0.9f, 0.85f));
            print("  cam_  : Camera perspective, FOV, viewmodel position, camera shake", Vec3(0.85f, 0.9f, 0.85f));
            print("  r_    : Low-level renderer, bloom, shadows, FXAA, reload shaders", Vec3(0.85f, 0.9f, 0.85f));
            print("  phys_ : Physics simulation, gravity, friction, air accel, bhop timing", Vec3(0.85f, 0.9f, 0.85f));
            print("  ai_   : Combat AI, bot state, navigation paths, spawning", Vec3(0.85f, 0.9f, 0.85f));
            print("  snd_  : 3D audio, master/music/sfx volume, playback, restart", Vec3(0.85f, 0.9f, 0.85f));
            print("  ui_   : 2D user interface, HUD toggle, crosshair, UI scaling", Vec3(0.85f, 0.9f, 0.85f));
            print("  dcl_  : Dynamic decals, impact marks, blood stains & limits", Vec3(0.85f, 0.9f, 0.85f));
            print("  mp_   : Multiplayer arena, team bots, friendly fire, restart", Vec3(0.85f, 0.9f, 0.85f));
            print("  net_  : Network diagnostics, tickrate, latency & simulated packet loss", Vec3(0.85f, 0.9f, 0.85f));
            print("  sys_  : System utilities, copy log to clipboard, clear, bind, exit", Vec3(0.85f, 0.9f, 0.85f));
            print("----------------------------------------------------------------------", Vec3(0.4f, 0.45f, 0.4f));
            print("FILTERING & SEARCH EXAMPLES:", Vec3(0.95f, 0.95f, 0.35f));
            print("  help sp     -> List all player & cheat commands", Vec3(0.9f, 0.95f, 0.9f));
            print("  help log    -> List all 2D HUD telemetry diagnostics", Vec3(0.9f, 0.95f, 0.9f));
            print("  help phys   -> List all physics & bunnyhop tuning commands", Vec3(0.9f, 0.95f, 0.9f));
            print("  help all    -> Display complete catalog of ALL commands", Vec3(0.9f, 0.95f, 0.9f));
            print("  help <cmd>  -> Show syntax and current value (e.g. 'help bhop')", Vec3(0.9f, 0.95f, 0.9f));
            return;
        }

        // Case 2: Full catalog (help all)
        if (filter == "all") {
            print("=== COMPLETE LAB ENGINE COMMAND CATALOG ===", Vec3(0.95f, 0.85f, 0.25f));
            std::string currentPfx;
            for (const auto& d : docs) {
                if (d.prefix != currentPfx) {
                    currentPfx = d.prefix;
                    print("[" + currentPfx + "_ CATEGORY]", Vec3(0.95f, 0.9f, 0.3f));
                }
                printDocLine(d);
            }
            return;
        }

        // Case 3: Category filter
        bool isCategory = false;
        for (const auto& d : docs) {
            if (d.prefix == filter) {
                isCategory = true;
                break;
            }
        }

        if (isCategory) {
            std::string catName = filter;
            std::transform(catName.begin(), catName.end(), catName.begin(), [](unsigned char c) { return (char)std::toupper(c); });
            print("=== [" + catName + "_] COMMAND LIST ===", Vec3(0.95f, 0.85f, 0.25f));
            for (const auto& d : docs) {
                if (d.prefix == filter) {
                    printDocLine(d);
                }
            }
            return;
        }

        // Case 4: Search by command name or description
        std::vector<const CmdDoc*> matches;
        for (const auto& d : docs) {
            if (d.cmd == filter) {
                matches.push_back(&d);
            }
        }
        if (matches.empty()) {
            for (const auto& d : docs) {
                if (d.cmd.find(filter) != std::string::npos || d.desc.find(filter) != std::string::npos) {
                    matches.push_back(&d);
                }
            }
        }

        if (!matches.empty()) {
            print("=== SEARCH RESULTS FOR '" + filter + "' ===", Vec3(0.95f, 0.85f, 0.25f));
            for (const auto* d : matches) {
                print("COMMAND:     " + d->cmd + (d->usage.empty() ? "" : (" " + d->usage)), Vec3(0.98f, 0.98f, 0.98f));
                print("CATEGORY:    " + d->prefix + "_", Vec3(0.75f, 0.8f, 0.75f));
                print("DESCRIPTION: " + d->desc, Vec3(0.85f, 0.9f, 0.85f));

                // Display live CVar state if known
                if (d->cmd == "sp_cheats") print("CURRENT VALUE: " + std::string(sp_cheats ? "1 (ENABLED)" : "0 (DISABLED)"), Vec3(0.92f, 0.92f, 0.92f));
                else if (d->cmd == "sp_god") print("CURRENT VALUE: " + std::string(sp_god ? "1 (ON)" : "0 (OFF)"), Vec3(0.92f, 0.92f, 0.92f));
                else if (d->cmd == "sp_noclip") print("CURRENT VALUE: " + std::string(sp_noclip ? "1 (ON)" : "0 (OFF)"), Vec3(0.92f, 0.92f, 0.92f));
                else if (d->cmd == "sp_bhop") print("CURRENT VALUE: " + std::string(sp_bhop ? "1 (ON)" : "0 (OFF)"), Vec3(0.92f, 0.92f, 0.92f));
                else if (d->cmd == "sp_speed") print("CURRENT VALUE: " + std::to_string(sp_speed), Vec3(0.92f, 0.92f, 0.92f));
                else if (d->cmd == "sp_jump") print("CURRENT VALUE: " + std::to_string(sp_jump), Vec3(0.92f, 0.92f, 0.92f));
                else if (d->cmd == "phys_gravity") print("CURRENT VALUE: " + std::to_string(phys_gravity), Vec3(0.92f, 0.92f, 0.92f));
                else if (d->cmd == "phys_friction") print("CURRENT VALUE: " + std::to_string(phys_friction), Vec3(0.92f, 0.92f, 0.92f));
                else if (d->cmd == "phys_air_accel") print("CURRENT VALUE: " + std::to_string(phys_air_accel), Vec3(0.92f, 0.92f, 0.92f));
                else if (d->cmd == "phys_bhop_window") print("CURRENT VALUE: " + std::to_string(phys_bhop_window) + "s", Vec3(0.92f, 0.92f, 0.92f));
                else if (d->cmd == "phys_max_bhop_speed") print("CURRENT VALUE: " + std::to_string(phys_max_bhop_speed), Vec3(0.92f, 0.92f, 0.92f));
                else if (d->cmd == "cam_fov") print("CURRENT VALUE: " + std::to_string((int)cam_fov) + " deg", Vec3(0.92f, 0.92f, 0.92f));
                else if (d->cmd == "wrd_timescale") print("CURRENT VALUE: " + std::to_string(wrd_timescale), Vec3(0.92f, 0.92f, 0.92f));
                else if (d->cmd == "r_bloom") print("CURRENT VALUE: " + std::string(r_bloom ? "1 (ON)" : "0 (OFF)"), Vec3(0.92f, 0.92f, 0.92f));
                else if (d->cmd == "r_fxaa") print("CURRENT VALUE: " + std::string(r_fxaa ? "1 (ON)" : "0 (OFF)"), Vec3(0.92f, 0.92f, 0.92f));
                else if (d->cmd == "log_momentum") print("CURRENT VALUE: " + std::string(log_momentum ? "1 (ON)" : "0 (OFF)"), Vec3(0.92f, 0.92f, 0.92f));
                else if (d->cmd == "log_fps") print("CURRENT VALUE: " + std::string(log_fps ? "1 (ON)" : "0 (OFF)"), Vec3(0.92f, 0.92f, 0.92f));
                print("----------------------------------------------------------------------", Vec3(0.35f, 0.35f, 0.35f));
            }
            return;
        }

        print("No commands matching query '" + filter + "'. Type 'help' for full category list.", Vec3(1.0f, 0.4f, 0.4f));
    }

    bool LabDevConsole::onMouseButton(int button, int action, float mx, float my) {
        if (!isOpen) return false;

        if (button == 0 && action == 0) {
            isDragging = false;
            isResizing = false;
        }

        float titleH = 26.0f;
        float inputAreaH = 32.0f;
        float textX = windowX + 6.0f;
        float inputY = windowY + windowH - 28.0f;
        float submitW = 62.0f;
        float inputW = windowW - submitW - 20.0f;
        float submitX = textX + inputW + 6.0f;

        float popW = 320.0f;
        float popItemH = 20.0f;
        float popH = (float)_activeSuggestions.size() * popItemH + 4.0f;
        float popY = inputY + 26.0f;
        if (lastScreenH > 0 && popY + popH > (float)lastScreenH) {
            popY = inputY - popH - 2.0f;
        }

        bool insideWindow = (mx >= windowX && mx <= windowX + windowW &&
                             my >= windowY && my <= windowY + windowH);
        bool insidePopup = (!_activeSuggestions.empty() &&
                            mx >= textX && mx <= textX + popW &&
                            my >= popY && my <= popY + popH);

        if (!insideWindow && !insidePopup) {
            if (action == 1) {
                isDragging = false;
                isResizing = false;
            }
            return false;
        }

        float closeBtnX = windowX + windowW - 26.0f;
        float closeBtnY = windowY + 4.0f;
        float clearBtnX = closeBtnX - 58.0f;
        float clearBtnY = windowY + 4.0f;
        float copyBtnX = clearBtnX - 86.0f;
        float copyBtnY = windowY + 4.0f;

        if (button == 0) {
            if (action == 1) { // Pressed
                // 1. Close button [X]
                if (mx >= closeBtnX - 4.0f && mx <= windowX + windowW && my >= closeBtnY && my <= closeBtnY + 18.0f) {
                    close();
                    return true;
                }

                // 1b. Clear button
                if (mx >= clearBtnX - 4.0f && mx <= clearBtnX + 54.0f && my >= clearBtnY && my <= clearBtnY + 18.0f) {
                    _entries.clear();
                    return true;
                }

                // 1c. Copy Log button
                if (mx >= copyBtnX - 4.0f && mx <= copyBtnX + 82.0f && my >= copyBtnY && my <= copyBtnY + 18.0f) {
                    copyLogToClipboard();
                    return true;
                }

                // 2. Submit Button
                if (mx >= submitX && mx <= submitX + submitW && my >= inputY && my <= inputY + 24.0f) {
                    if (!_currentInput.empty()) {
                        execute(_currentInput);
                        _currentInput.clear();
                        _cursorPos = 0;
                        _activeSuggestions.clear();
                        _selectedSuggestion = -1;
                    }
                    return true;
                }

                // 3. Autocomplete popup click
                if (insidePopup) {
                    int clickedIdx = static_cast<int>((my - (popY + 2.0f)) / popItemH);
                    if (clickedIdx >= 0 && clickedIdx < static_cast<int>(_activeSuggestions.size())) {
                        _currentInput = _activeSuggestions[clickedIdx] + " ";
                        _cursorPos = static_cast<int>(_currentInput.size());
                        _activeSuggestions.clear();
                        _selectedSuggestion = -1;
                        return true;
                    }
                }

                // 4. Resize Grip bottom-right corner (16x16)
                if (mx >= windowX + windowW - 16.0f && mx <= windowX + windowW &&
                    my >= windowY + windowH - 16.0f && my <= windowY + windowH) {
                    isResizing = true;
                    resizeStartW = windowW;
                    resizeStartH = windowH;
                    resizeStartMX = mx;
                    resizeStartMY = my;
                    return true;
                }

                // 5. Input Box Click
                if (mx >= textX && mx <= textX + inputW && my >= inputY && my <= inputY + 24.0f) {
                    _cursorPos = static_cast<int>(_currentInput.size());
                    return true;
                }

                // 6. Title bar dragging
                if (my >= windowY && my <= windowY + titleH && mx >= windowX && mx < copyBtnX - 4.0f) {
                    isDragging = true;
                    dragOffsetX = mx - windowX;
                    dragOffsetY = my - windowY;
                    return true;
                }

                // 7. Scrollbar click
                float textW = windowW - 12.0f;
                float textH = windowH - titleH - inputAreaH - 2.0f;
                float textY = windowY + titleH + 2.0f;
                float sbW = 14.0f;
                float sbX = textX + textW - sbW - 1.0f;
                float sbY = textY + 1.0f;
                float sbH = textH - 2.0f;
                if (mx >= sbX && mx <= sbX + sbW && my >= sbY && my <= sbY + sbH) {
                    float clickRatio = (my - sbY) / sbH;
                    int total = static_cast<int>(_entries.size());
                    float lineH = 16.5f;
                    int maxLines = static_cast<int>(textH / lineH) - 1;
                    int maxScroll = std::max(0, total - maxLines);
                    _scrollOffset = static_cast<int>((1.0f - clickRatio) * static_cast<float>(maxScroll));
                    _scrollOffset = std::clamp(_scrollOffset, 0, maxScroll);
                    return true;
                }
            } else if (action == 0) {
                isDragging = false;
                isResizing = false;
            }
        } else if (button == 1 && action == 1) { // Right Click: Copy line
            float textW = windowW - 12.0f;
            float textH = windowH - titleH - inputAreaH - 2.0f;
            float textY = windowY + titleH + 2.0f;
            if (mx >= textX && mx <= textX + textW && my >= textY && my <= textY + textH) {
                float lineH = 16.5f;
                int clickedRow = static_cast<int>((my - (textY + 4.0f)) / lineH);
                int maxLines = (int)(textH / lineH) - 1;
                int total = (int)_entries.size();
                int endIdx = total - _scrollOffset;
                int startIdx = std::max(0, endIdx - maxLines);
                int entryIdx = startIdx + clickedRow;
                if (entryIdx >= 0 && entryIdx < total) {
                    copyLineToClipboard(entryIdx);
                    return true;
                }
            }
        }
        return true;
    }

    void LabDevConsole::onMouseMove(float mx, float my) {
        if (!isOpen) return;
        if (isDragging) {
            windowX = mx - dragOffsetX;
            windowY = my - dragOffsetY;
            float maxW = (lastScreenW > 0) ? (float)lastScreenW : 1280.0f;
            float maxH = (lastScreenH > 0) ? (float)lastScreenH : 720.0f;
            float titleH = 26.0f;
            windowX = std::clamp(windowX, 0.0f, std::max(0.0f, maxW - 80.0f));
            windowY = std::clamp(windowY, 0.0f, std::max(0.0f, maxH - titleH));
        } else if (isResizing) {
            float newW = resizeStartW + (mx - resizeStartMX);
            float newH = resizeStartH + (my - resizeStartMY);
            float maxW = (lastScreenW > 0) ? (float)lastScreenW : 1280.0f;
            float maxH = (lastScreenH > 0) ? (float)lastScreenH : 720.0f;
            windowW = std::clamp(newW, 440.0f, maxW - windowX);
            windowH = std::clamp(newH, 260.0f, maxH - windowY);
        }
    }

    void LabDevConsole::onMouseScroll(float yoffset) {
        if (!isOpen) return;
        int maxScroll = std::max(0, (int)_entries.size() - 15);
        _scrollOffset = std::clamp(_scrollOffset + (int)(yoffset * 4.0f), 0, maxScroll);
    }

    bool LabDevConsole::onKey(int key, int action, bool shift, bool ctrl) {
        (void)shift;
        if (!isOpen) return false;
        if (action != 1 && action != 2) return false;

        // Tilde (`~`, key 96) or Escape (256) closes console
        if (key == 96 || key == 256) {
            close();
            return true;
        }

        // Clipboard: Ctrl+V (Paste)
        if (ctrl && (key == 86 || key == 'v' || key == 'V')) {
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
        if (ctrl && (key == 67 || key == 'c' || key == 'C')) {
            if (!_currentInput.empty()) {
                GLFWwindow* win = glfwGetCurrentContext();
                if (win) {
                    glfwSetClipboardString(win, _currentInput.c_str());
                }
            } else {
                copyLogToClipboard();
            }
            return true;
        }

        // Ctrl+A: move cursor to end
        if (ctrl && (key == 65 || key == 'a' || key == 'A')) {
            _cursorPos = static_cast<int>(_currentInput.size());
            return true;
        }

        // Tab: Autocomplete selected suggestion
        if (key == 258) { // GLFW_KEY_TAB
            if (!_activeSuggestions.empty()) {
                int idx = (_selectedSuggestion >= 0 && _selectedSuggestion < static_cast<int>(_activeSuggestions.size()))
                          ? _selectedSuggestion : 0;
                _currentInput = _activeSuggestions[idx] + " ";
                _cursorPos = static_cast<int>(_currentInput.size());
                _activeSuggestions.clear();
                _selectedSuggestion = -1;
                return true;
            }
        }

        // Enter: execute command
        if (key == 257 || key == 335) { // GLFW_KEY_ENTER or Keypad Enter
            if (!_activeSuggestions.empty() && _selectedSuggestion >= 0 && _selectedSuggestion < static_cast<int>(_activeSuggestions.size()) && _currentInput != _activeSuggestions[_selectedSuggestion]) {
                _currentInput = _activeSuggestions[_selectedSuggestion] + " ";
                _cursorPos = static_cast<int>(_currentInput.size());
                _activeSuggestions.clear();
                _selectedSuggestion = -1;
                return true;
            }
            if (!_currentInput.empty()) {
                execute(_currentInput);
                _currentInput.clear();
                _cursorPos = 0;
                _activeSuggestions.clear();
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
            _scrollOffset = std::min(maxScroll, _scrollOffset + 6);
            return true;
        }
        if (key == 267) { // PageDown
            _scrollOffset = std::max(0, _scrollOffset - 6);
            return true;
        }

        // Alphanumeric and Printable Characters
        char c = '\0';
        if (key >= 65 && key <= 90) { // A-Z
            c = shift ? static_cast<char>(key) : static_cast<char>(key + 32);
        } else if (key >= 97 && key <= 122) { // a-z
            c = shift ? static_cast<char>(key - 32) : static_cast<char>(key);
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
            // Check if current word before cursor is a known prefix -> auto-convert to '_'
            size_t wordStart = _cursorPos;
            while (wordStart > 0 && _currentInput[wordStart - 1] != ' ') {
                wordStart--;
            }
            std::string word = _currentInput.substr(wordStart, _cursorPos - wordStart);
            std::transform(word.begin(), word.end(), word.begin(), [](unsigned char ch) { return (char)std::tolower(ch); });
            static const char* const knownPrefixes[] = {
                "sp", "sv", "log", "wrd", "cam", "r", "phys", "ai", "snd", "ui", "dcl", "mp", "net", "sys"
            };
            bool isPrefix = false;
            for (const auto& p : knownPrefixes) {
                if (word == p) {
                    isPrefix = true;
                    break;
                }
            }
            c = isPrefix ? '_' : ' ';
        } else if (key == 45) { // Minus / Underscore
            if (shift) {
                c = '_';
            } else {
                char prev = (_cursorPos > 0) ? _currentInput[_cursorPos - 1] : ' ';
                if (prev == ' ' || prev == '\t' || prev == '=') {
                    c = '-'; // Negative argument or flag
                } else {
                    c = '_'; // Automatically enforce '_' for command names and prefixes!
                }
            }
        } else if (key == 333) { // Keypad Minus
            char prev = (_cursorPos > 0) ? _currentInput[_cursorPos - 1] : ' ';
            if (prev == ' ' || prev == '\t' || prev == '=') {
                c = '-';
            } else {
                c = '_';
            }
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
            if (_charHandledThisFrame) {
                // Character was already inserted by onChar in the same frame
                return true;
            }
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
        if (codepoint < 32 || codepoint > 126) return;

        char ch = static_cast<char>(codepoint);
        _currentInput.insert(_currentInput.begin() + _cursorPos, ch);
        _cursorPos++;
        _charHandledThisFrame = true;
        _lastCharHandledKey = static_cast<int>(codepoint);
        _selectedSuggestion = -1;
        updateAutocomplete();
    }

    void LabDevConsole::update(float dt) {
        _cursorBlink += dt * 4.0f;
        _charHandledThisFrame = false;
        _lastCharHandledKey = -1;
    }

    void LabDevConsole::render(int screenW, int screenH) {
        if (!isOpen) return;

        lastScreenW = screenW;
        lastScreenH = screenH;

        Renderer::beginUI(screenW, screenH);

        float titleH = 24.0f;
        float inputAreaH = 32.0f;

        // Ensure title bar remains visible within screen viewport
        windowX = std::clamp(windowX, 0.0f, std::max(0.0f, (float)screenW - 80.0f));
        windowY = std::clamp(windowY, 0.0f, std::max(0.0f, (float)screenH - titleH));

        // ==================== AUTHENTIC SOURCE VGUI2 THEME ====================
        Vec3 frameBg(0.24f, 0.25f, 0.26f);      // Clean neutral medium dark-grey (#3d4043)
        Vec3 borderLight(0.40f, 0.42f, 0.44f);  // Crisp metallic bevel highlight
        Vec3 borderDark(0.11f, 0.12f, 0.13f);   // Clean bevel shadow
        Vec3 textAreaBg(0.12f, 0.13f, 0.14f);   // Dark sunken text area (#1f2124)
        Vec3 inputBg(0.10f, 0.11f, 0.12f);      // Dark sunken input field (#1a1b1d)
        Vec3 buttonBg(0.28f, 0.29f, 0.31f);     // Neutral button (#474a4f)

        // 1. Window Outer Frame & 3D Bevel (Translucent dark grey)
        Renderer::drawRect(windowX, windowY, windowW, windowH, frameBg, 0.94f);
        Renderer::drawRect(windowX, windowY, windowW, 1.0f, borderLight);
        Renderer::drawRect(windowX, windowY, 1.0f, windowH, borderLight);
        Renderer::drawRect(windowX, windowY + windowH - 1.0f, windowW, 1.0f, borderDark);
        Renderer::drawRect(windowX + windowW - 1.0f, windowY, 1.0f, windowH, borderDark);

        // 2. Title Bar: "Konsola" / "Console"
        LabFont::drawText(windowX + 8.0f, windowY + 5.0f, "Konsola", 1.35f, Vec3(0.90f, 0.92f, 0.94f), LabFontType::GeoSans);

        // Close Button [x] on top right
        float closeBtnX = windowX + windowW - 20.0f;
        float closeBtnY = windowY + 4.0f;
        Renderer::drawRect(closeBtnX, closeBtnY, 16.0f, 16.0f, buttonBg);
        Renderer::drawRect(closeBtnX, closeBtnY, 16.0f, 1.0f, borderLight);
        Renderer::drawRect(closeBtnX, closeBtnY, 1.0f, 16.0f, borderLight);
        Renderer::drawRect(closeBtnX, closeBtnY + 15.0f, 16.0f, 1.0f, borderDark);
        Renderer::drawRect(closeBtnX + 15.0f, closeBtnY, 1.0f, 16.0f, borderDark);
        LabFont::drawText(closeBtnX + 4.0f, closeBtnY + 1.0f, "x", 1.25f, Vec3(0.90f, 0.90f, 0.90f), LabFontType::System);

        // 3. Inner Log Text Area (Sunken)
        float textX = windowX + 6.0f;
        float textY = windowY + titleH + 2.0f;
        float textW = windowW - 12.0f;
        float textH = windowH - titleH - inputAreaH - 2.0f;

        Renderer::drawRect(textX, textY, textW, textH, textAreaBg, 0.96f);
        Renderer::drawRect(textX, textY, textW, 1.0f, borderDark);
        Renderer::drawRect(textX, textY, 1.0f, textH, borderDark);
        Renderer::drawRect(textX, textY + textH - 1.0f, textW, 1.0f, borderLight);
        Renderer::drawRect(textX + textW - 1.0f, textY, 1.0f, textH, borderLight);

        // Render visible text lines with authentic Source console scale
        float lineH = 16.5f;
        int maxLines = (int)(textH / lineH) - 1;
        int total = (int)_entries.size();
        int endIdx = total - _scrollOffset;
        int startIdx = std::max(0, endIdx - maxLines);

        float curY = textY + 4.0f;
        for (int i = startIdx; i < endIdx && i < total; ++i) {
            const auto& entry = _entries[i];
            LabFont::drawText(textX + 6.0f, curY, entry.text, 1.22f, entry.color, LabFontType::Monospace);
            curY += lineH;
        }

        // Vertical Scrollbar on the right of text area
        float sbW = 14.0f;
        float sbX = textX + textW - sbW - 1.0f;
        float sbY = textY + 1.0f;
        float sbH = textH - 2.0f;
        Renderer::drawRect(sbX, sbY, sbW, sbH, Vec3(0.10f, 0.11f, 0.12f));

        if (total > maxLines && total > 0) {
            float thumbRatio = std::clamp((float)maxLines / (float)total, 0.12f, 1.0f);
            float thumbH = sbH * thumbRatio;
            int maxScroll = total - maxLines;
            float scrollProgress = (maxScroll > 0) ? (float)(maxScroll - _scrollOffset) / (float)maxScroll : 1.0f;
            float thumbY = sbY + (sbH - thumbH) * scrollProgress;

            Renderer::drawRect(sbX + 1.0f, thumbY, sbW - 2.0f, thumbH, Vec3(0.26f, 0.28f, 0.30f));
            Renderer::drawRect(sbX + 1.0f, thumbY, sbW - 2.0f, 1.0f, borderLight);
            Renderer::drawRect(sbX + 1.0f, thumbY, 1.0f, thumbH, borderLight);
            Renderer::drawRect(sbX + 1.0f, thumbY + thumbH - 1.0f, sbW - 2.0f, 1.0f, borderDark);
            Renderer::drawRect(sbX + sbW - 2.0f, thumbY, 1.0f, thumbH, borderDark);
        }

        // 4. Command Input Field (Sunken)
        float inputY = windowY + windowH - 28.0f;
        float submitW = 62.0f;
        float inputW = windowW - submitW - 20.0f;

        Renderer::drawRect(textX, inputY, inputW, 24.0f, inputBg);
        Renderer::drawRect(textX, inputY, inputW, 1.0f, borderDark);
        Renderer::drawRect(textX, inputY, 1.0f, 24.0f, borderDark);
        Renderer::drawRect(textX, inputY + 23.0f, inputW, 1.0f, borderLight);
        Renderer::drawRect(textX + inputW - 1.0f, inputY, 1.0f, 24.0f, borderLight);

        // Prompt "] " in clean neutral off-white
        LabFont::drawText(textX + 6.0f, inputY + 4.0f, "] ", 1.25f, Vec3(0.85f, 0.85f, 0.85f), LabFontType::Monospace);
        float promptW = LabFont::getTextWidth("] ", 1.25f, LabFontType::Monospace);

        // User typed command text
        LabFont::drawText(textX + 6.0f + promptW, inputY + 4.0f, _currentInput, 1.25f, Vec3(0.96f, 0.96f, 0.96f), LabFontType::Monospace);

        // Standard blinking text caret
        bool blink = (std::fmod(_cursorBlink, 2.0f) < 1.0f);
        if (blink) {
            float cursorXOffset = LabFont::getTextWidth(_currentInput.substr(0, _cursorPos), 1.25f, LabFontType::Monospace);
            float curDrawX = textX + 6.0f + promptW + cursorXOffset;
            Renderer::drawRect(curDrawX, inputY + 4.0f, 1.5f, 15.0f, Vec3(0.96f, 0.96f, 0.96f));
        }

        // 5. Submit Button ("Wyślij")
        float submitX = textX + inputW + 6.0f;
        Renderer::drawRect(submitX, inputY, submitW, 24.0f, buttonBg);
        Renderer::drawRect(submitX, inputY, submitW, 1.0f, borderLight);
        Renderer::drawRect(submitX, inputY, 1.0f, 24.0f, borderLight);
        Renderer::drawRect(submitX, inputY + 23.0f, submitW, 1.0f, borderDark);
        Renderer::drawRect(submitX + submitW - 1.0f, inputY, 1.0f, 24.0f, borderDark);
        float submitTextW = LabFont::getTextWidth("Wyślij", 1.25f, LabFontType::System);
        LabFont::drawText(submitX + (submitW - submitTextW) * 0.5f, inputY + 4.0f, "Wyślij", 1.25f, Vec3(0.90f, 0.92f, 0.94f), LabFontType::System);

        // 6. Resize Grip in bottom-right corner (Source VGUI style diagonal lines)
        float gripX = windowX + windowW - 12.0f;
        float gripY = windowY + windowH - 12.0f;
        Renderer::drawRect(gripX + 6.0f, gripY + 8.0f, 3.0f, 1.0f, borderLight);
        Renderer::drawRect(gripX + 4.0f, gripY + 5.0f, 5.0f, 1.0f, borderLight);
        Renderer::drawRect(gripX + 2.0f, gripY + 2.0f, 7.0f, 1.0f, borderLight);

        // 7. Autocomplete Suggestion Popup (Rendered right below or above input)
        if (!_activeSuggestions.empty()) {
            float popW = 320.0f;
            float popItemH = 20.0f;
            float popH = (float)_activeSuggestions.size() * popItemH + 4.0f;
            float popY = inputY + 26.0f;
            if (lastScreenH > 0 && popY + popH > (float)lastScreenH) {
                popY = inputY - popH - 2.0f;
            }

            Renderer::drawRect(textX, popY, popW, popH, Vec3(0.18f, 0.19f, 0.21f));
            Renderer::drawRect(textX, popY, popW, 1.0f, borderLight);
            Renderer::drawRect(textX, popY, 1.0f, popH, borderLight);
            Renderer::drawRect(textX, popY + popH - 1.0f, popW, 1.0f, borderDark);
            Renderer::drawRect(textX + popW - 1.0f, popY, 1.0f, popH, borderDark);

            float curSugY = popY + 2.0f;
            for (size_t s = 0; s < _activeSuggestions.size(); ++s) {
                bool isSel = ((int)s == _selectedSuggestion);
                if (isSel) {
                    Renderer::drawRect(textX + 2.0f, curSugY, popW - 4.0f, popItemH, Vec3(0.25f, 0.33f, 0.45f));
                }
                const std::string& cmdName = _activeSuggestions[s];
                std::string cval = getCvarValue(cmdName);
                std::string fullDisplay = cval.empty() ? cmdName : (cmdName + " " + cval);

                LabFont::drawText(textX + 6.0f, curSugY + 2.0f, fullDisplay, 1.20f,
                                  isSel ? Vec3(1.0f, 1.0f, 1.0f) : Vec3(0.85f, 0.88f, 0.90f), LabFontType::Monospace);
                curSugY += popItemH;
            }
        }

        Renderer::endUI();
    }

    void LabDevConsole::renderOverlay(int screenW, int screenH, const DevTelemetryData& t) {
        bool showAny = log_momentum || log_fps || log_ping || log_pos ||
                       log_drawcalls || log_memory || log_ai || log_all;
        if (!showAny) return;

        lastScreenW = screenW;
        lastScreenH = screenH;

        Renderer::beginUI(screenW, screenH);

        float startX = 20.0f;
        float curY = 20.0f;
        float boxW = 440.0f;

        // 1. Momentum & Speed Telemetry Overlay
        if (log_momentum || log_all) {
            Renderer::drawRect(startX, curY, boxW, 58.0f, Vec3(0.11f, 0.12f, 0.13f));
            Renderer::drawRect(startX, curY, 3.0f, 58.0f, Vec3(0.40f, 0.60f, 0.85f));

            std::ostringstream ss;
            ss << std::fixed << std::setprecision(1);
            ss << "SPEED: " << t.horizontalSpeed << " u/s (Cap: " << phys_max_bhop_speed << ")";
            LabFont::drawText(startX + 12.0f, curY + 6.0f, ss.str(), 1.7f, Vec3(0.92f, 0.92f, 0.92f), LabFontType::Monospace);

            std::string bhopStatus = t.isBhopActive ? "BHOPPING: ACCELERATING" : (t.isGrounded ? "GROUND: FRICTION ACTIVE" : "AIR STRAFE ACTIVE");
            Vec3 bhopCol = t.isBhopActive ? Vec3(1.0f, 0.85f, 0.25f) : (t.isGrounded ? Vec3(0.7f, 0.7f, 0.7f) : Vec3(0.40f, 0.70f, 1.0f));
            LabFont::drawText(startX + 12.0f, curY + 30.0f, bhopStatus, 1.5f, bhopCol, LabFontType::Monospace);

            curY += 66.0f;
        }

        // 2. Position & Angles Overlay
        if (log_pos || log_all) {
            Renderer::drawRect(startX, curY, boxW, 58.0f, Vec3(0.11f, 0.12f, 0.13f));
            Renderer::drawRect(startX, curY, 3.0f, 58.0f, Vec3(0.35f, 0.55f, 0.85f));

            std::ostringstream ss;
            ss << std::fixed << std::setprecision(2);
            ss << "POS: (" << t.playerPos.x << ", " << t.playerPos.y << ", " << t.playerPos.z << ")";
            LabFont::drawText(startX + 12.0f, curY + 6.0f, ss.str(), 1.6f, Vec3(0.88f, 0.90f, 0.92f), LabFontType::Monospace);

            std::ostringstream ss2;
            ss2 << std::fixed << std::setprecision(1);
            ss2 << "ANG: PITCH " << t.pitch << " deg | YAW " << t.yaw << " deg";
            LabFont::drawText(startX + 12.0f, curY + 30.0f, ss2.str(), 1.5f, Vec3(0.65f, 0.75f, 0.85f), LabFontType::Monospace);

            curY += 66.0f;
        }

        // 3. FPS & Performance Overlay
        if (log_fps || log_all) {
            Renderer::drawRect(startX, curY, 260.0f, 34.0f, Vec3(0.11f, 0.12f, 0.13f));
            Renderer::drawRect(startX, curY, 3.0f, 34.0f, Vec3(0.85f, 0.75f, 0.30f));

            std::ostringstream ss;
            ss << std::fixed << std::setprecision(1);
            ss << (int)t.fps << " FPS (" << t.frametimeMs << " ms)";
            LabFont::drawText(startX + 12.0f, curY + 6.0f, ss.str(), 1.65f, Vec3(0.92f, 0.92f, 0.92f), LabFontType::Monospace);

            curY += 42.0f;
        }

        // 4. Ping Overlay
        if (log_ping || log_all) {
            Renderer::drawRect(startX, curY, 180.0f, 34.0f, Vec3(0.11f, 0.12f, 0.13f));
            Renderer::drawRect(startX, curY, 3.0f, 34.0f, Vec3(0.35f, 0.65f, 0.90f));

            std::string pingStr = "PING: " + std::to_string(t.pingMs) + " ms";
            LabFont::drawText(startX + 12.0f, curY + 6.0f, pingStr, 1.65f, Vec3(0.90f, 0.90f, 0.90f), LabFontType::Monospace);

            curY += 42.0f;
        }

        // 5. Drawcalls & Vertices Overlay
        if (log_drawcalls || log_all) {
            Renderer::drawRect(startX, curY, boxW, 36.0f, Vec3(0.11f, 0.12f, 0.13f));
            Renderer::drawRect(startX, curY, 3.0f, 36.0f, Vec3(0.25f, 0.85f, 0.45f));

            std::string dcStr = "DRAWCALLS: " + std::to_string(t.drawCalls) + " | VERTS: " + std::to_string(t.vertexCount);
            LabFont::drawText(startX + 12.0f, curY + 8.0f, dcStr, 1.6f, Vec3(0.90f, 0.95f, 0.90f), LabFontType::Monospace);

            curY += 44.0f;
        }

        // 6. Memory Overlay (RAM & VRAM)
        if (log_memory || log_all) {
            Renderer::drawRect(startX, curY, boxW, 36.0f, Vec3(0.11f, 0.12f, 0.13f));
            Renderer::drawRect(startX, curY, 3.0f, 36.0f, Vec3(0.75f, 0.40f, 0.85f));

            std::string memStr = "RAM: " + std::to_string(t.ramWorkingSetMB) + " MB | VRAM (Est): " + std::to_string(t.vramTexturesMB) + " MB";
            LabFont::drawText(startX + 12.0f, curY + 8.0f, memStr, 1.6f, Vec3(0.92f, 0.88f, 0.95f), LabFontType::Monospace);

            curY += 44.0f;
        }

        // 7. AI Bot Navigation & Combat Telemetry Overlay
        if (log_ai || log_all) {
            Renderer::drawRect(startX, curY, boxW, 58.0f, Vec3(0.11f, 0.12f, 0.13f));
            Renderer::drawRect(startX, curY, 3.0f, 58.0f, Vec3(0.95f, 0.45f, 0.20f));

            std::string aiHead = "AI SYNTHS: " + std::to_string(t.activeBots) + " ACTIVE IN ARENA";
            LabFont::drawText(startX + 12.0f, curY + 6.0f, aiHead, 1.6f, Vec3(0.98f, 0.90f, 0.85f), LabFontType::Monospace);

            std::string aiSub = t.closestBotInfo.empty() ? "NO COMBAT TARGETS IN SECTOR" : t.closestBotInfo;
            LabFont::drawText(startX + 12.0f, curY + 30.0f, aiSub, 1.45f, Vec3(0.85f, 0.80f, 0.75f), LabFontType::Monospace);

            curY += 66.0f;
        }

        Renderer::endUI();
    }

} // namespace Lab
