#include "LabInteractive.h"
#include "LabFont.h"
#include "LabCollision.h"
#include <cmath>
#include <algorithm>
#include <iostream>

namespace Lab {

    InteractiveSystem::InteractiveSystem() = default;

    InteractiveSystem::~InteractiveSystem() {
        shutdown();
    }

    void InteractiveSystem::init() {
        if (_initialized) return;
        initScreenTexture();
        _initialized = true;
    }

    void InteractiveSystem::shutdown() {
        if (!_initialized) return;
        _texScreenGrid.reset();
        _texMonitorFace.reset();
        _entities.clear();
        _hoveredEntity = nullptr;
        _hoveredIndex = -1;
        _initialized = false;
    }

    void InteractiveSystem::clear() {
        _entities.clear();
        _hoveredEntity = nullptr;
        _hoveredIndex = -1;
        _lastInteractionNotice.clear();
        _noticeTimer = 0.0f;
    }

    bool InteractiveSystem::isAnyTerminalOpen() const {
        for (const auto& e : _entities) {
            if (e.isTerminalOpen) return true;
        }
        return false;
    }

    InteractiveEntity* InteractiveSystem::getActiveTerminal() {
        for (auto& e : _entities) {
            if (e.isTerminalOpen) return &e;
        }
        return nullptr;
    }

    void InteractiveSystem::closeActiveTerminal() {
        for (auto& e : _entities) {
            if (e.isTerminalOpen) {
                e.isTerminalOpen = false;
                e.currentPage = TerminalPage::Main;
                AudioEngine::playSound(SoundID::TerminalBeep, 0.8f);
            }
        }
    }

    void InteractiveSystem::initScreenTexture() {
        const int S = 64;
        std::vector<unsigned char> img(S * S * 4, 0);

        for (int y = 0; y < S; ++y) {
            for (int x = 0; x < S; ++x) {
                int idx = (y * S + x) * 4;

                bool scanline = (y % 3 == 0);
                bool gridline = (x % 16 == 0 || y % 16 == 0);

                float dx = (x - 31.5f) / 32.0f;
                float dy = (y - 31.5f) / 32.0f;
                float vign = std::clamp(1.0f - (dx * dx + dy * dy) * 0.45f, 0.4f, 1.0f);

                unsigned char base = scanline ? 170 : 235;
                if (gridline) base = (unsigned char)std::min(255, base + 20);
                base = (unsigned char)(base * vign);

                img[idx + 0] = (unsigned char)(base * 0.70f);
                img[idx + 1] = base;
                img[idx + 2] = (unsigned char)(base * 0.85f);
                img[idx + 3] = 255;
            }
        }
        _texScreenGrid = std::make_unique<Texture>(img.data(), S, S, 4);
    }

    // Helper to set a pixel with top-left origin (y=0 is TOP of monitor)
    static inline void setPixel(std::vector<unsigned char>& pixels, int w, int h, int x, int y, unsigned char r, unsigned char g, unsigned char b, unsigned char a = 255) {
        if (x < 0 || x >= w || y < 0 || y >= h) return;
        int texY = (h - 1 - y); // Invert so y=0 is at TOP of texture (v=1.0 in OpenGL)
        int idx = (texY * w + x) * 4;
        pixels[idx + 0] = r;
        pixels[idx + 1] = g;
        pixels[idx + 2] = b;
        pixels[idx + 3] = a;
    }

    // Rasterizes dot-matrix string onto monitor texture (origin at top-left)
    static void drawTextOnMonitor(std::vector<unsigned char>& pixels, int w, int h, int startX, int startY, const std::string& text, unsigned char r, unsigned char g, unsigned char b) {
        int curX = startX;
        for (char c : text) {
            const unsigned char* glyph = LabFont::getDotMatrixGlyph(c);
            for (int col = 0; col < 5; ++col) {
                unsigned char bits = glyph[col];
                for (int row = 0; row < 7; ++row) {
                    if ((bits >> row) & 1) {
                        int px = curX + col;
                        int py = startY + row;
                        setPixel(pixels, w, h, px, py, r, g, b);
                    }
                }
            }
            curX += 6; // 5 columns + 1 pixel space
            if (curX + 6 >= w) break;
        }
    }

    void InteractiveSystem::updateMonitorFaceTexture(const InteractiveEntity& ent) {
        const int W = 128;
        const int H = 96;
        std::vector<unsigned char> pixels(W * H * 4, 15);

        // Scanline raster background
        for (int y = 0; y < H; ++y) {
            for (int x = 0; x < W; ++x) {
                bool isScan = (y % 2 == 0);
                unsigned char r = isScan ? 8 : 16;
                unsigned char g = isScan ? 22 : 36;
                unsigned char b = isScan ? 14 : 26;
                setPixel(pixels, W, H, x, y, r, g, b);
            }
        }

        // Header bar at top of screen (y = 2 to 13)
        for (int x = 2; x < W - 2; ++x) {
            for (int y = 2; y < 14; ++y) {
                setPixel(pixels, W, H, x, y, 20, 70, 40);
            }
        }

        // Title text in header bar
        drawTextOnMonitor(pixels, W, H, 6, 4, "LAB-OS // SEC-04", 60, 255, 140);

        // Status Line
        if (ent.isLocked) {
            drawTextOnMonitor(pixels, W, H, 6, 22, "STATUS: LOCKED", 255, 70, 60);
            drawTextOnMonitor(pixels, W, H, 6, 34, "SEAL: [ENGAGED]", 255, 120, 90);
        } else {
            drawTextOnMonitor(pixels, W, H, 6, 22, "STATUS: UNLOCKED", 70, 255, 120);
            drawTextOnMonitor(pixels, W, H, 6, 34, "SEAL: [DISENGAGED]", 100, 255, 160);
        }

        // Subtitle / Door info
        drawTextOnMonitor(pixels, W, H, 6, 48, "AIRLOCK ACTUATOR", 180, 220, 240);

        // Blinking Prompt cursor
        bool blink = ((int)(ent.displayTimer * 2.5f) % 2 == 0);
        std::string promptText = blink ? "> PRESS [E] _" : "> PRESS [E]  ";
        drawTextOnMonitor(pixels, W, H, 6, 64, promptText, 255, 220, 40);

        // Bottom status border
        for (int x = 4; x < W - 4; ++x) {
            unsigned char r = ent.isLocked ? 200 : 50;
            unsigned char g = ent.isLocked ? 40 : 220;
            unsigned char b = ent.isLocked ? 40 : 80;
            setPixel(pixels, W, H, x, 80, r, g, b);
            setPixel(pixels, W, H, x, 81, r, g, b);
        }

        if (!_texMonitorFace) {
            _texMonitorFace = std::make_unique<Texture>(pixels.data(), W, H, 4);
        } else {
            _texMonitorFace->updateData(pixels.data(), W, H, 4);
        }
    }

    void InteractiveSystem::addEntity(const InteractiveEntity& entity) {
        _entities.push_back(entity);
    }

    InteractiveEntity* InteractiveSystem::getEntity(int id) {
        for (auto& e : _entities) {
            if (e.id == id) return &e;
        }
        return nullptr;
    }

    void InteractiveSystem::update(float dt, const Vec3& playerPos, const Vec3& lookDir, bool useKeyPressed, LabMap* map) {
        init();

        if (_noticeTimer > 0.0f) {
            _noticeTimer = std::max(0.0f, _noticeTimer - dt);
        }

        // Update entity state timers
        for (auto& ent : _entities) {
            if (ent.cooldown > 0.0f) {
                ent.cooldown = std::max(0.0f, ent.cooldown - dt);
            }
            ent.displayTimer += dt;
        }

        // If a terminal is currently open in full-screen OS mode, skip raycast picking
        if (isAnyTerminalOpen()) {
            return;
        }

        // Raycast picking for interactive entities
        Vec3 normLook = lookDir.normalized();
        if (normLook.lengthSq() < 0.001f) normLook = Vec3(0, 0, 1);

        _hoveredEntity = nullptr;
        _hoveredIndex = -1;
        float bestDist = 2.8f;

        for (size_t i = 0; i < _entities.size(); ++i) {
            auto& ent = _entities[i];
            float distToPlayer = (ent.position - playerPos).length();
            if (distToPlayer > ent.interactionRadius) continue;

            // Bounding box raycast intersection
            Vec3 half = ent.size * 0.5f + Vec3(0.08f, 0.08f, 0.08f);
            Vec3 bMin = ent.position - half;
            Vec3 bMax = ent.position + half;

            float t = 0.0f;
            Vec3 hitNorm;
            bool hitBox = Raycast::rayIntersectAABB(playerPos, normLook, bMin, bMax, t, &hitNorm);

            Vec3 toEnt = ent.position - playerPos;
            float d = toEnt.length();
            float dot = Vec3::dot(normLook, toEnt / std::max(0.001f, d));

            if ((hitBox && t < bestDist) || (d < bestDist && dot > 0.93f)) {
                float pickDist = hitBox ? t : d;
                if (pickDist < bestDist) {
                    bestDist = pickDist;
                    _hoveredEntity = &ent;
                    _hoveredIndex = (int)i;
                }
            }
        }

        // When player presses [E] on hovered entity:
        if (_hoveredEntity && useKeyPressed && _hoveredEntity->cooldown <= 0.0f) {
            _hoveredEntity->cooldown = 0.45f;

            if (_hoveredEntity->type == InteractiveType::Terminal) {
                // Open full interactive Computer Terminal OS (LAB-OS)
                _hoveredEntity->isTerminalOpen = true;
                _hoveredEntity->currentPage = TerminalPage::Main;
                AudioEngine::playSound(SoundID::TerminalBeep, 0.95f);
            } else {
                if (_hoveredEntity->isLocked) {
                    // Entity is locked (e.g. Keypad requiring higher clearance)
                    AudioEngine::playSound(SoundID::AccessDenied, 1.0f);
                    _lastInteractionNotice = "ACCESS DENIED - LEVEL 4 SECURITY CLEARANCE REQUIRED";
                    _noticeColor = Vec3(1.0f, 0.25f, 0.2f);
                    _noticeTimer = 2.5f;
                } else {
                    // Quick switch / button toggle
                    _hoveredEntity->isActivated = !_hoveredEntity->isActivated;
                    AudioEngine::playSound(SoundID::AccessGranted, 0.9f);
                    AudioEngine::playSound(SoundID::TerminalBeep, 0.8f);
                    _lastInteractionNotice = _hoveredEntity->isActivated ? "SYSTEM OVERRIDE: ACTIVATED" : "SYSTEM OVERRIDE: DEACTIVATED";
                    _noticeColor = _hoveredEntity->isActivated ? Vec3(0.2f, 0.95f, 0.4f) : Vec3(0.95f, 0.7f, 0.2f);
                    _noticeTimer = 2.5f;

                    if (_hoveredEntity->targetDoorIndex >= 0 && map) {
                        if ((size_t)_hoveredEntity->targetDoorIndex < map->doors.size()) {
                            auto& door = map->doors[_hoveredEntity->targetDoorIndex];
                            door.isLocked = !_hoveredEntity->isActivated;
                            door.isOpen = _hoveredEntity->isActivated;
                        }
                    }
                }
            }
        }
    }

    bool InteractiveSystem::handleTerminalKey(int key, LabMap* map, int /*aliveBotsCount*/) {
        InteractiveEntity* act = getActiveTerminal();
        if (!act) return false;

        // ESC (GLFW 256 or ASCII 27) or E ('E'/69 or 'e'/101) closes terminal
        if (key == 256 || key == 27 || key == 'E' || key == 'e' || key == 69 || key == 101) {
            closeActiveTerminal();
            return true;
        }

        if (act->currentPage == TerminalPage::Main) {
            if (key == '1' || key == 49 || key == 1) {
                // Toggle Door Lock / Unlock
                act->isLocked = !act->isLocked;
                act->isActivated = !act->isLocked;

                if (act->targetDoorIndex >= 0 && map && (size_t)act->targetDoorIndex < map->doors.size()) {
                    auto& door = map->doors[act->targetDoorIndex];
                    door.isLocked = act->isLocked;
                    door.isOpen = act->isActivated;
                }

                if (!act->isLocked) {
                    act->statusText = "UNLOCKED - AIRLOCK CYCLING OPEN";
                    act->themeColor = Vec3(0.2f, 0.95f, 0.40f);
                    AudioEngine::playSound(SoundID::AccessGranted, 1.0f);
                    AudioEngine::playSound(SoundID::TerminalBeep, 0.9f);
                    _lastInteractionNotice = "SECURITY OVERRIDE: AIRLOCK CYCLING OPEN";
                    _noticeColor = Vec3(0.25f, 0.95f, 0.35f);
                } else {
                    act->statusText = "LOCKED - BLAST SEAL ENGAGED";
                    act->themeColor = Vec3(1.0f, 0.25f, 0.2f);
                    AudioEngine::playSound(SoundID::AccessDenied, 0.95f);
                    _lastInteractionNotice = "AIRLOCK SEAL ENGAGED: PASSAGE LOCKED";
                    _noticeColor = Vec3(1.0f, 0.3f, 0.25f);
                }
                _noticeTimer = 3.0f;
                return true;
            } else if (key == '2' || key == 50 || key == 2) {
                act->currentPage = TerminalPage::SecurityLogs;
                AudioEngine::playSound(SoundID::TerminalBeep, 0.85f);
                return true;
            } else if (key == '3' || key == 51 || key == 3) {
                act->currentPage = TerminalPage::BotTelemetry;
                AudioEngine::playSound(SoundID::TerminalBeep, 0.85f);
                return true;
            }
        } else {
            // In sub-pages: '0' or Backspace (GLFW 259 / ASCII 8) returns to Main Menu
            if (key == '0' || key == 48 || key == 0 || key == 259 || key == 8) {
                act->currentPage = TerminalPage::Main;
                AudioEngine::playSound(SoundID::TerminalBeep, 0.8f);
                return true;
            }
        }

        return false;
    }

    void InteractiveSystem::render(const Camera& /*cam*/) {
        if (_entities.empty()) return;
        init();

        for (const auto& ent : _entities) {
            bool isHovered = (&ent == _hoveredEntity);

            // Update procedural monitor texture with real text
            updateMonitorFaceTexture(ent);

            // 1. Dark metallic console housing
            Vec3 housingCol = isHovered ? Vec3(0.18f, 0.22f, 0.28f) : Vec3(0.12f, 0.14f, 0.18f);
            Renderer::drawCube(ent.position, ent.size, housingCol, nullptr, true);

            // 2. Beveled bezel border and screen face
            Vec3 norm = ent.normal.normalized();
            if (norm.lengthSq() < 0.001f) norm = Vec3(0, 0, 1);

            Vec3 scrOffset = norm * (ent.size.z * 0.5f + 0.004f);
            Vec3 scrPos = ent.position + scrOffset;
            Vec3 scrSize(ent.size.x * 0.86f, ent.size.y * 0.82f, 0.004f);

            // 3. CRT Terminal Screen with rendered text texture!
            Vec3 screenGlow = isHovered ? Vec3(1.35f, 1.35f, 1.35f) : Vec3(1.0f, 1.0f, 1.0f);
            Renderer::drawCube(scrPos, scrSize, screenGlow, _texMonitorFace.get(), true);

            // 4. Status Indicator LED on housing bezel (Red = Locked, Green = Unlocked)
            Vec3 ledOffset = Vec3(0.0f, ent.size.y * 0.44f, ent.size.z * 0.5f + 0.008f);
            Vec3 ledPos = ent.position + ledOffset;
            Vec3 ledCol = ent.isLocked ? Vec3(1.0f, 0.15f, 0.15f) : Vec3(0.15f, 1.0f, 0.35f);
            Renderer::drawCube(ledPos, Vec3(0.05f, 0.05f, 0.02f), ledCol, nullptr, false);
        }
    }

    void InteractiveSystem::renderHUD(int screenWidth, int screenHeight) {
        float sw = (float)screenWidth;
        float sh = (float)screenHeight;

        // If terminal is open in full OS mode, HUD interaction prompt is hidden
        if (isAnyTerminalOpen()) return;

        // Interaction Prompt at Center-Bottom
        if (_hoveredEntity) {
            float cx = sw * 0.5f;
            float cy = sh * 0.72f;
            float boxW = 540.0f;
            float boxH = 54.0f;

            // Background panel
            Renderer::drawRect(cx - boxW * 0.5f, cy, boxW, boxH, Vec3(0.06f, 0.08f, 0.10f));

            // Accent border line
            Vec3 accent = _hoveredEntity->isLocked ? Vec3(1.0f, 0.3f, 0.25f) : Vec3(0.2f, 0.95f, 0.40f);
            Renderer::drawRect(cx - boxW * 0.5f, cy, boxW, 2.0f, accent);
            Renderer::drawRect(cx - boxW * 0.5f, cy + boxH - 2.0f, boxW, 2.0f, Vec3(0.16f, 0.20f, 0.24f));

            // Key icon box [E]
            Renderer::drawRect(cx - boxW * 0.5f + 16.0f, cy + 10.0f, 34.0f, 34.0f, Vec3(0.16f, 0.22f, 0.26f));
            Renderer::drawRect(cx - boxW * 0.5f + 16.0f, cy + 10.0f, 34.0f, 1.0f, Vec3(1.0f, 0.85f, 0.2f));
            LabFont::drawText(cx - boxW * 0.5f + 26.0f, cy + 16.0f, "E", 1.6f, Vec3(1.0f, 0.85f, 0.2f), LabFontType::GeoSans);

            // Action label
            std::string prompt = "LOG IN: " + _hoveredEntity->title + " - [" + (_hoveredEntity->isLocked ? "LOCKED" : "UNLOCKED") + "]";
            LabFont::drawText(cx - boxW * 0.5f + 62.0f, cy + 17.0f, prompt, 1.4f, Vec3(0.95f, 0.96f, 0.98f), LabFontType::GeoSans);
        }

        // Diagnostic Notice Banner
        if (_noticeTimer > 0.0f && !_lastInteractionNotice.empty()) {
            float ncy = 135.0f;
            float nbw = 580.0f;
            float nbh = 38.0f;
            float ncx = sw * 0.5f - nbw * 0.5f;

            Renderer::drawRect(ncx, ncy, nbw, nbh, Vec3(0.08f, 0.10f, 0.14f));
            Renderer::drawRect(ncx, ncy, nbw, 1.5f, _noticeColor);
            Renderer::drawRect(ncx, ncy + nbh - 1.0f, nbw, 1.0f, Vec3(0.18f, 0.22f, 0.26f));

            LabFont::drawText(ncx + 20.0f, ncy + 9.0f, _lastInteractionNotice, 1.4f, _noticeColor, LabFontType::GeoSans);
        }
    }

    void InteractiveSystem::renderTerminalOS(int screenWidth, int screenHeight, const LabMap* map, int aliveBotsCount) {
        InteractiveEntity* act = getActiveTerminal();
        if (!act) return;

        float sw = (float)screenWidth;
        float sh = (float)screenHeight;

        // 1. Dark phosphor backdrop overlay covering the screen
        Renderer::beginUI(screenWidth, screenHeight);
        Renderer::drawRect(0.0f, 0.0f, sw, sh, Vec3(0.02f, 0.04f, 0.05f));

        // 2. CRT Computer Terminal Window Frame (1080 x 620)
        float tw = 1080.0f;
        float th = 620.0f;
        float tx = (sw - tw) * 0.5f;
        float ty = (sh - th) * 0.5f;

        // Bezel and scanline background
        Renderer::drawRect(tx, ty, tw, th, Vec3(0.04f, 0.07f, 0.06f));
        Renderer::drawRect(tx, ty, tw, 2.0f, Vec3(0.2f, 0.95f, 0.45f));
        Renderer::drawRect(tx, ty + th - 2.0f, tw, 2.0f, Vec3(0.2f, 0.95f, 0.45f));
        Renderer::drawRect(tx, ty, 2.0f, th, Vec3(0.2f, 0.95f, 0.45f));
        Renderer::drawRect(tx + tw - 2.0f, ty, 2.0f, th, Vec3(0.2f, 0.95f, 0.45f));

        // 3. Header Bar
        Renderer::drawRect(tx + 4.0f, ty + 4.0f, tw - 8.0f, 52.0f, Vec3(0.07f, 0.14f, 0.10f));
        Renderer::drawRect(tx + 4.0f, ty + 56.0f, tw - 8.0f, 1.0f, Vec3(0.2f, 0.95f, 0.45f));

        LabFont::drawText(tx + 24.0f, ty + 14.0f, "LAB-OS v3.42 // KORE CRYOGENIC RESEARCH FACILITY // SEC-04", 1.8f, Vec3(0.25f, 1.0f, 0.50f), LabFontType::GeoSans);
        LabFont::drawText(tx + 24.0f, ty + 36.0f, "TERMINAL NODE: AIRLOCK OVERRIDE // CLEARANCE: LEVEL 3 GUEST", 1.3f, Vec3(0.65f, 0.85f, 0.70f), LabFontType::GeoSans);

        // 4. Page Routing
        if (act->currentPage == TerminalPage::Main) {
            // Main Menu Options
            LabFont::drawText(tx + 35.0f, ty + 85.0f, ">>> SELECT SYSTEM FUNCTION (HOTKEYS 1 - 3):", 1.6f, Vec3(0.3f, 1.0f, 0.6f), LabFontType::GeoSans);

            // Option 1: Door Actuator
            bool doorOpen = false;
            if (act->targetDoorIndex >= 0 && map && (size_t)act->targetDoorIndex < map->doors.size()) {
                doorOpen = map->doors[act->targetDoorIndex].isOpen;
            } else {
                doorOpen = act->isActivated;
            }

            Renderer::drawRect(tx + 35.0f, ty + 120.0f, tw - 70.0f, 75.0f, Vec3(0.06f, 0.10f, 0.08f));
            Renderer::drawRect(tx + 35.0f, ty + 120.0f, 4.0f, 75.0f, doorOpen ? Vec3(0.2f, 0.95f, 0.4f) : Vec3(1.0f, 0.3f, 0.2f));

            LabFont::drawText(tx + 55.0f, ty + 132.0f, "[1] AIRLOCK BLAST DOOR ACTUATOR", 1.7f, Vec3(1.0f, 0.95f, 0.4f), LabFontType::GeoSans);
            std::string doorStatus = doorOpen ? "STATUS: [ UNLOCKED // AIRLOCK OPEN ]" : "STATUS: [ LOCKED // SEAL CLAMP ENGAGED ]";
            Vec3 statusCol = doorOpen ? Vec3(0.25f, 1.0f, 0.45f) : Vec3(1.0f, 0.35f, 0.25f);
            LabFont::drawText(tx + 55.0f, ty + 158.0f, doorStatus + "  (Press [1] to toggle pneumatic seal)", 1.3f, statusCol, LabFontType::GeoSans);

            // Option 2: Security Logs
            Renderer::drawRect(tx + 35.0f, ty + 215.0f, tw - 70.0f, 75.0f, Vec3(0.06f, 0.10f, 0.08f));
            Renderer::drawRect(tx + 35.0f, ty + 215.0f, 4.0f, 75.0f, Vec3(0.2f, 0.8f, 1.0f));

            LabFont::drawText(tx + 55.0f, ty + 227.0f, "[2] FACILITY INCIDENT ARCHIVE LOGS", 1.7f, Vec3(1.0f, 0.95f, 0.4f), LabFontType::GeoSans);
            LabFont::drawText(tx + 55.0f, ty + 253.0f, "STATUS: [ 3 RECOVERED AUDIO & DATA LOGS ]  (Press [2] to read incident file #0451)", 1.3f, Vec3(0.7f, 0.85f, 0.95f), LabFontType::GeoSans);

            // Option 3: Bot Telemetry
            Renderer::drawRect(tx + 35.0f, ty + 310.0f, tw - 70.0f, 75.0f, Vec3(0.06f, 0.10f, 0.08f));
            Renderer::drawRect(tx + 35.0f, ty + 310.0f, 4.0f, 75.0f, Vec3(0.9f, 0.6f, 0.2f));

            LabFont::drawText(tx + 55.0f, ty + 322.0f, "[3] SYNTH COMBAT UNIT TELEMETRY", 1.7f, Vec3(1.0f, 0.95f, 0.4f), LabFontType::GeoSans);
            std::string botSummary = "STATUS: [ " + std::to_string(aliveBotsCount) + " ACTIVE SYNTHS DETECTED ON GRID ]  (Press [3] to query sensor feed)";
            LabFont::drawText(tx + 55.0f, ty + 348.0f, botSummary, 1.3f, Vec3(0.95f, 0.80f, 0.40f), LabFontType::GeoSans);

            // Diagnostic Terminal Logs Box
            Renderer::drawRect(tx + 35.0f, ty + 410.0f, tw - 70.0f, 120.0f, Vec3(0.03f, 0.06f, 0.05f));
            Renderer::drawRect(tx + 35.0f, ty + 410.0f, tw - 70.0f, 1.0f, Vec3(0.15f, 0.35f, 0.25f));
            LabFont::drawText(tx + 48.0f, ty + 422.0f, "DIAGNOSTIC SYSTEM TELEMETRY:", 1.3f, Vec3(0.35f, 0.85f, 0.5f), LabFontType::GeoSans);
            LabFont::drawText(tx + 48.0f, ty + 444.0f, "> HYDRAULIC PRESSURE: 450 PSI // COOLANT LOOP: 14 KELVIN", 1.2f, Vec3(0.6f, 0.75f, 0.65f), LabFontType::GeoSans);
            LabFont::drawText(tx + 48.0f, ty + 466.0f, "> SUB-ZERO CONTAINMENT FIELD: STABLE // GRID ALPHA RESTRICTED", 1.2f, Vec3(0.6f, 0.75f, 0.65f), LabFontType::GeoSans);
            LabFont::drawText(tx + 48.0f, ty + 488.0f, "> WARNING: COMBAT DRONES PATROLLING EXTERIOR PERIMETER", 1.2f, Vec3(0.95f, 0.55f, 0.35f), LabFontType::GeoSans);

        } else if (act->currentPage == TerminalPage::SecurityLogs) {
            // Logs View
            LabFont::drawText(tx + 35.0f, ty + 85.0f, ">>> RECOVERED INCIDENT LOGS // ARCHIVE FILE #0451:", 1.6f, Vec3(0.3f, 1.0f, 0.6f), LabFontType::GeoSans);

            // Log 1
            Renderer::drawRect(tx + 35.0f, ty + 120.0f, tw - 70.0f, 90.0f, Vec3(0.06f, 0.10f, 0.08f));
            Renderer::drawRect(tx + 35.0f, ty + 120.0f, 4.0f, 90.0f, Vec3(0.2f, 0.85f, 0.45f));
            LabFont::drawText(tx + 55.0f, ty + 130.0f, "LOG 01 // 02:14 UTC - DR. VANCE, CRYOGENICS LEAD", 1.4f, Vec3(1.0f, 0.9f, 0.4f), LabFontType::GeoSans);
            LabFont::drawText(tx + 55.0f, ty + 154.0f, "\"Coolant leak detected in Subterranean Facility Alpha. Primary containment valves are failing.", 1.2f, Vec3(0.85f, 0.9f, 0.85f), LabFontType::GeoSans);
            LabFont::drawText(tx + 55.0f, ty + 176.0f, "Liquid nitrogen is venting into corridors. The temperature has plunged to -45 C. We must evacuate.\"", 1.2f, Vec3(0.85f, 0.9f, 0.85f), LabFontType::GeoSans);

            // Log 2
            Renderer::drawRect(tx + 35.0f, ty + 225.0f, tw - 70.0f, 90.0f, Vec3(0.06f, 0.10f, 0.08f));
            Renderer::drawRect(tx + 35.0f, ty + 225.0f, 4.0f, 90.0f, Vec3(0.9f, 0.5f, 0.2f));
            LabFont::drawText(tx + 55.0f, ty + 235.0f, "LOG 02 // 03:45 UTC - CHIEF SECURITY OFFICER", 1.4f, Vec3(1.0f, 0.9f, 0.4f), LabFontType::GeoSans);
            LabFont::drawText(tx + 55.0f, ty + 259.0f, "\"The synthetic guard units encountered a logic lockup from the cryogenic frost.", 1.2f, Vec3(0.85f, 0.9f, 0.85f), LabFontType::GeoSans);
            LabFont::drawText(tx + 55.0f, ty + 281.0f, "They have identified human survivors as hostile intruders. Do not approach them unarmed.\"", 1.2f, Vec3(0.85f, 0.9f, 0.85f), LabFontType::GeoSans);

            // Log 3
            Renderer::drawRect(tx + 35.0f, ty + 330.0f, tw - 70.0f, 90.0f, Vec3(0.06f, 0.10f, 0.08f));
            Renderer::drawRect(tx + 35.0f, ty + 330.0f, 4.0f, 90.0f, Vec3(0.3f, 0.7f, 1.0f));
            LabFont::drawText(tx + 55.0f, ty + 340.0f, "LOG 03 // 05:10 UTC - EMERGENCY SYSTEM OVERRIDE", 1.4f, Vec3(1.0f, 0.9f, 0.4f), LabFontType::GeoSans);
            LabFont::drawText(tx + 55.0f, ty + 364.0f, "\"All automated blast doors have been clamped shut to contain the synthetic outbreak.", 1.2f, Vec3(0.85f, 0.9f, 0.85f), LabFontType::GeoSans);
            LabFont::drawText(tx + 55.0f, ty + 386.0f, "Use manual terminal option [1] to cycle the hydraulic blast door seals and advance.\"", 1.2f, Vec3(0.85f, 0.9f, 0.85f), LabFontType::GeoSans);

            LabFont::drawText(tx + 55.0f, ty + 460.0f, ">>> PRESS [0] OR [BACKSPACE] TO RETURN TO MAIN MENU", 1.5f, Vec3(1.0f, 0.85f, 0.3f), LabFontType::GeoSans);

        } else if (act->currentPage == TerminalPage::BotTelemetry) {
            // Telemetry View
            LabFont::drawText(tx + 35.0f, ty + 85.0f, ">>> SYNTH SENSOR GRID // REAL-TIME THREAT RADAR:", 1.6f, Vec3(0.3f, 1.0f, 0.6f), LabFontType::GeoSans);

            Renderer::drawRect(tx + 35.0f, ty + 120.0f, tw - 70.0f, 300.0f, Vec3(0.04f, 0.08f, 0.06f));
            Renderer::drawRect(tx + 35.0f, ty + 120.0f, tw - 70.0f, 1.0f, Vec3(0.2f, 0.8f, 0.4f));

            LabFont::drawText(tx + 55.0f, ty + 140.0f, "SECTOR-04 ACTIVE SENSORS: 6 SENSORS ONLINE", 1.4f, Vec3(0.3f, 0.9f, 0.5f), LabFontType::GeoSans);
            LabFont::drawText(tx + 55.0f, ty + 175.0f, "CONFIRMED SYNTH SQUAD: " + std::to_string(aliveBotsCount) + " UNITS CURRENTLY OPERATIONAL", 1.5f, Vec3(1.0f, 0.85f, 0.35f), LabFontType::GeoSans);
            LabFont::drawText(tx + 55.0f, ty + 215.0f, "- UNIT #1: COMBAT SYNTH // HEALTH: 100% // PATROL SCRIPT ACTIVE", 1.3f, Vec3(0.8f, 0.85f, 0.8f), LabFontType::GeoSans);
            LabFont::drawText(tx + 55.0f, ty + 245.0f, "- UNIT #2: ENFORCER SYNTH // HEALTH: 85% // ENGAGING TARGETS", 1.3f, Vec3(0.8f, 0.85f, 0.8f), LabFontType::GeoSans);
            LabFont::drawText(tx + 55.0f, ty + 275.0f, "- WEAPON PAYLOADS DETECTED: 9mm AUTOMATIC, SHOTGUN, RPG", 1.3f, Vec3(0.95f, 0.6f, 0.3f), LabFontType::GeoSans);
            LabFont::drawText(tx + 55.0f, ty + 310.0f, "- RADIAL SENSOR SWEEP: AIRLOCK PERIMETER CLEAR", 1.3f, Vec3(0.3f, 1.0f, 0.5f), LabFontType::GeoSans);

            LabFont::drawText(tx + 55.0f, ty + 460.0f, ">>> PRESS [0] OR [BACKSPACE] TO RETURN TO MAIN MENU", 1.5f, Vec3(1.0f, 0.85f, 0.3f), LabFontType::GeoSans);
        }

        // Footer Bar
        Renderer::drawRect(tx + 4.0f, ty + th - 52.0f, tw - 8.0f, 48.0f, Vec3(0.06f, 0.10f, 0.08f));
        Renderer::drawRect(tx + 4.0f, ty + th - 52.0f, tw - 8.0f, 1.0f, Vec3(0.2f, 0.95f, 0.45f));

        LabFont::drawText(tx + 24.0f, ty + th - 36.0f, "[HOTKEYS: 1-3 SELECT]       [ESC] / [E] LOG OUT & RETURN TO COMBAT", 1.5f, Vec3(0.3f, 1.0f, 0.5f), LabFontType::GeoSans);

        Renderer::endUI();
    }

} // namespace Lab
