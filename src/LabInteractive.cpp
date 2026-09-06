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

    // Draws a circle on the monitor texture
    static void drawCircle(std::vector<unsigned char>& pixels, int w, int h, int cx, int cy, int radius, unsigned char r, unsigned char g, unsigned char b) {
        for (int a = 0; a < 360; a += 3) {
            float rad = (float)a * 3.14159265f / 180.0f;
            int x = cx + (int)(std::cos(rad) * radius);
            int y = cy + (int)(std::sin(rad) * radius);
            setPixel(pixels, w, h, x, y, r, g, b);
        }
    }

    void InteractiveSystem::updateMonitorFaceTexture(const InteractiveEntity& ent) {
        const int W = 128;
        const int H = 96;
        std::vector<unsigned char> pixels(W * H * 4, 12);

        // 1. Scanline raster background
        for (int y = 0; y < H; ++y) {
            for (int x = 0; x < W; ++x) {
                bool isScan = (y % 2 == 0);
                unsigned char r = isScan ? 6 : 14;
                unsigned char g = isScan ? 14 : 26;
                unsigned char b = isScan ? 18 : 34;
                setPixel(pixels, W, H, x, y, r, g, b);
            }
        }

        // Header bar at top of screen
        for (int x = 2; x < W - 2; ++x) {
            for (int y = 2; y < 14; ++y) {
                setPixel(pixels, W, H, x, y, 16, 38, 52);
            }
        }

        // Title text in header
        drawTextOnMonitor(pixels, W, H, 6, 4, "KORF // RETINA-SCAN", 60, 220, 255);

        // 2. Biometric Retinal Eye Graphics
        int eyeX = 64;
        int eyeY = 46;

        // Theme colors based on scanner state
        unsigned char eyeR = 220, eyeG = 45, eyeB = 40; // Default locked: Red
        if (ent.isScanning) {
            eyeR = 255; eyeG = 210; eyeB = 40; // Scanning: Amber Gold
        } else if (!ent.isLocked) {
            eyeR = 40; eyeG = 240; eyeB = 120; // Unlocked / Verified: Vibrant Green
        }

        // Outer Calibration Ring
        drawCircle(pixels, W, H, eyeX, eyeY, 26, eyeR / 2, eyeG / 2, eyeB / 2);

        // Mid Iris Ring
        drawCircle(pixels, W, H, eyeX, eyeY, 15, eyeR, eyeG, eyeB);

        // Inner Pupil Disc
        for (int dy = -4; dy <= 4; ++dy) {
            for (int dx = -4; dx <= 4; ++dx) {
                if (dx * dx + dy * dy <= 16) {
                    setPixel(pixels, W, H, eyeX + dx, eyeY + dy, eyeR, eyeG, eyeB);
                }
            }
        }

        // Crosshairs tick marks
        for (int i = 18; i <= 28; ++i) {
            setPixel(pixels, W, H, eyeX + i, eyeY, eyeR, eyeG, eyeB);
            setPixel(pixels, W, H, eyeX - i, eyeY, eyeR, eyeG, eyeB);
            setPixel(pixels, W, H, eyeX, eyeY + i * 8 / 10, eyeR, eyeG, eyeB);
            setPixel(pixels, W, H, eyeX, eyeY - i * 8 / 10, eyeR, eyeG, eyeB);
        }

        // Animated Laser Sweep Line (Sweeping up and down across retina)
        float laserPhase = std::fmod(ent.displayTimer * (ent.isScanning ? 3.0f : 1.2f), 1.0f);
        int laserY = 24 + (int)(laserPhase * 44.0f);
        for (int x = eyeX - 28; x <= eyeX + 28; ++x) {
            setPixel(pixels, W, H, x, laserY, 255, 255, 255);
            setPixel(pixels, W, H, x, laserY - 1, eyeR, eyeG, eyeB);
            setPixel(pixels, W, H, x, laserY + 1, eyeR, eyeG, eyeB);
        }

        // Status texts
        if (ent.isScanning) {
            int pct = std::min(100, (int)(ent.scanProgress * 100.0f));
            std::string scanStr = "SCAN: " + std::to_string(pct) + "%";
            drawTextOnMonitor(pixels, W, H, 6, 74, scanStr, 255, 220, 40);

            // Progress bar at bottom
            int barW = (int)(ent.scanProgress * (W - 16));
            for (int x = 8; x < 8 + barW; ++x) {
                setPixel(pixels, W, H, x, 86, 255, 220, 40);
                setPixel(pixels, W, H, x, 87, 255, 220, 40);
            }
        } else if (ent.isLocked) {
            drawTextOnMonitor(pixels, W, H, 6, 74, "SEAL: [LOCKED]", 255, 70, 60);
            bool blink = ((int)(ent.displayTimer * 2.5f) % 2 == 0);
            std::string pStr = blink ? "> PRESS [E] _" : "> PRESS [E]  ";
            drawTextOnMonitor(pixels, W, H, 6, 84, pStr, 255, 220, 40);
        } else {
            drawTextOnMonitor(pixels, W, H, 6, 74, "RETINA: VERIFIED", 70, 255, 120);
            drawTextOnMonitor(pixels, W, H, 6, 84, "SEAL: [DISENGAGED]", 100, 255, 160);
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

    bool InteractiveSystem::triggerRetinalScan(int entityId, LabMap* map) {
        InteractiveEntity* ent = getEntity(entityId);
        if (!ent) return false;

        if (ent->isLocked && !ent->isScanning) {
            ent->isScanning = true;
            ent->scanProgress = 0.0f;
            AudioEngine::playSound(SoundID::TerminalBeep, 0.95f);
            _lastInteractionNotice = "[ INITIATING BIOMETRIC RETINAL SCAN... HOLD STILL ]";
            _noticeColor = Vec3(0.3f, 0.85f, 1.0f);
            _noticeTimer = 2.0f;
            return true;
        } else if (!ent->isLocked) {
            // Already verified: toggle door state
            ent->isActivated = !ent->isActivated;
            AudioEngine::playSound(SoundID::AccessGranted, 0.9f);
            if (ent->targetDoorIndex >= 0 && map && (size_t)ent->targetDoorIndex < map->doors.size()) {
                map->doors[ent->targetDoorIndex].isOpen = ent->isActivated;
            }
            _lastInteractionNotice = ent->isActivated ? "[ AIRLOCK CYCLING: OPEN ]" : "[ AIRLOCK CYCLING: CLOSED ]";
            _noticeColor = Vec3(0.2f, 0.95f, 0.4f);
            _noticeTimer = 2.5f;
            return true;
        }
        return false;
    }

    void InteractiveSystem::update(float dt, const Vec3& playerPos, const Vec3& lookDir, bool useKeyPressed, LabMap* map) {
        init();

        if (_noticeTimer > 0.0f) {
            _noticeTimer = std::max(0.0f, _noticeTimer - dt);
        }

        // Update entity state timers and active biometric retinal scanning
        for (auto& ent : _entities) {
            if (ent.cooldown > 0.0f) {
                ent.cooldown = std::max(0.0f, ent.cooldown - dt);
            }
            ent.displayTimer += dt;

            // Retinal scan progress simulation
            if (ent.isScanning) {
                ent.scanProgress += dt / std::max(0.1f, ent.scanDuration);
                if (ent.scanProgress >= 1.0f) {
                    ent.scanProgress = 1.0f;
                    ent.isScanning = false;
                    ent.isLocked = false;
                    ent.isActivated = true;
                    ent.statusText = "VERIFIED - CLEARANCE LEVEL " + std::to_string(ent.clearanceLevel);
                    ent.themeColor = Vec3(0.2f, 0.95f, 0.40f);
                    AudioEngine::playSound(SoundID::AccessGranted, 1.0f);
                    _lastInteractionNotice = "[ ACCESS GRANTED // RETINAL MATCH CONFIRMED // " + ent.authorizedUser + " // AIRLOCK UNLOCKED ]";
                    _noticeColor = Vec3(0.25f, 0.95f, 0.45f);
                    _noticeTimer = 3.5f;

                    if (ent.targetDoorIndex >= 0 && map && (size_t)ent.targetDoorIndex < map->doors.size()) {
                        auto& door = map->doors[ent.targetDoorIndex];
                        door.isLocked = false;
                        door.isOpen = true;
                    }
                }
            }
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

            if (_hoveredEntity->type == InteractiveType::RetinalScanner) {
                triggerRetinalScan(_hoveredEntity->id, map);
            } else if (_hoveredEntity->type == InteractiveType::Keypad) {
                if (_hoveredEntity->isLocked) {
                    AudioEngine::playSound(SoundID::AccessDenied, 1.0f);
                    _lastInteractionNotice = "[ ACCESS DENIED - LEVEL 4 CLEARANCE REQUIRED ]";
                    _noticeColor = Vec3(1.0f, 0.25f, 0.2f);
                    _noticeTimer = 2.5f;
                }
            } else {
                // Wall switch / general actuator
                _hoveredEntity->isActivated = !_hoveredEntity->isActivated;
                AudioEngine::playSound(SoundID::AccessGranted, 0.9f);
                _lastInteractionNotice = _hoveredEntity->isActivated ? "[ SYSTEM OVERRIDE: ACTIVATED ]" : "[ SYSTEM OVERRIDE: DEACTIVATED ]";
                _noticeColor = _hoveredEntity->isActivated ? Vec3(0.2f, 0.95f, 0.4f) : Vec3(0.95f, 0.7f, 0.2f);
                _noticeTimer = 2.5f;

                if (_hoveredEntity->targetDoorIndex >= 0 && map && (size_t)_hoveredEntity->targetDoorIndex < map->doors.size()) {
                    auto& door = map->doors[_hoveredEntity->targetDoorIndex];
                    door.isLocked = !_hoveredEntity->isActivated;
                    door.isOpen = _hoveredEntity->isActivated;
                }
            }
        }
    }

    void InteractiveSystem::render(const Camera& /*cam*/) {
        if (_entities.empty()) return;
        init();

        for (const auto& ent : _entities) {
            bool isHovered = (&ent == _hoveredEntity);

            // Update procedural monitor texture with retinal graphics
            updateMonitorFaceTexture(ent);

            // 1. Dark metallic scanner casing
            Vec3 housingCol = isHovered ? Vec3(0.18f, 0.24f, 0.30f) : Vec3(0.12f, 0.14f, 0.18f);
            Renderer::drawCube(ent.position, ent.size, housingCol, nullptr, true);

            // 2. Beveled bezel border and screen face
            Vec3 norm = ent.normal.normalized();
            if (norm.lengthSq() < 0.001f) norm = Vec3(0, 0, 1);

            Vec3 scrOffset = norm * (ent.size.z * 0.5f + 0.004f);
            Vec3 scrPos = ent.position + scrOffset;
            Vec3 scrSize(ent.size.x * 0.86f, ent.size.y * 0.82f, 0.004f);

            // 3. Scanner Screen with rendered eye/laser texture
            Vec3 screenGlow = isHovered ? Vec3(1.35f, 1.35f, 1.35f) : Vec3(1.0f, 1.0f, 1.0f);
            Renderer::drawCube(scrPos, scrSize, screenGlow, _texMonitorFace.get(), true);

            // 4. Status Indicator LED on housing bezel (Red = Locked, Amber = Scanning, Green = Unlocked)
            Vec3 ledOffset = Vec3(0.0f, ent.size.y * 0.44f, ent.size.z * 0.5f + 0.008f);
            Vec3 ledPos = ent.position + ledOffset;
            Vec3 ledCol = Vec3(0.15f, 1.0f, 0.35f);
            if (ent.isScanning) {
                ledCol = Vec3(1.0f, 0.85f, 0.2f);
            } else if (ent.isLocked) {
                ledCol = Vec3(1.0f, 0.15f, 0.15f);
            }
            Renderer::drawCube(ledPos, Vec3(0.05f, 0.05f, 0.02f), ledCol, nullptr, false);
        }
    }

    void InteractiveSystem::renderHUD(int screenWidth, int screenHeight) {
        float sw = (float)screenWidth;
        float sh = (float)screenHeight;

        // 1. Interaction Prompt or Biometric Scanning Widget at Center-Bottom
        if (_hoveredEntity) {
            float cx = sw * 0.5f;
            float cy = sh * 0.72f;

            if (_hoveredEntity->isScanning) {
                // Sleek Retinal Scanning Progress Card
                float boxW = 520.0f;
                float boxH = 68.0f;
                float bx = cx - boxW * 0.5f;
                float by = cy;

                Renderer::drawRect(bx, by, boxW, boxH, Vec3(0.08f, 0.12f, 0.16f));
                Renderer::drawRect(bx, by, boxW, 2.0f, Vec3(0.3f, 0.85f, 1.0f));
                Renderer::drawRect(bx, by + boxH - 2.0f, boxW, 2.0f, Vec3(0.3f, 0.85f, 1.0f));

                LabFont::drawText(bx + 20.0f, by + 12.0f, "BIOMETRIC RETINAL SCAN IN PROGRESS...", 1.7f, Vec3(1.0f, 0.9f, 0.35f), LabFontType::GeoSans);

                int pct = std::min(100, (int)(_hoveredEntity->scanProgress * 100.0f));
                std::string subText = "Subject: " + _hoveredEntity->authorizedUser + " // Matching Neural Retina: " + std::to_string(pct) + "%";
                LabFont::drawText(bx + 20.0f, by + 32.0f, subText, 1.3f, Vec3(0.75f, 0.9f, 1.0f), LabFontType::GeoSans);

                // Progress Bar
                float barX = bx + 20.0f;
                float barY = by + 50.0f;
                float barW = boxW - 40.0f;
                float barH = 8.0f;
                Renderer::drawRect(barX, barY, barW, barH, Vec3(0.15f, 0.20f, 0.25f));
                Renderer::drawRect(barX, barY, barW * _hoveredEntity->scanProgress, barH, Vec3(0.2f, 0.95f, 0.45f));
            } else {
                // Static Prompt Card
                float boxW = 460.0f;
                float boxH = 50.0f;
                float bx = cx - boxW * 0.5f;
                float by = cy;

                Renderer::drawRect(bx, by, boxW, boxH, Vec3(0.10f, 0.13f, 0.17f));
                Renderer::drawRect(bx, by, boxW, 1.0f, _hoveredEntity->themeColor);
                Renderer::drawRect(bx, by, 4.0f, boxH, _hoveredEntity->themeColor);

                std::string promptText;
                if (_hoveredEntity->type == InteractiveType::RetinalScanner) {
                    promptText = _hoveredEntity->isLocked ? "[E] SCAN RETINA - AUTH CLEARANCE" : "[E] CYCLE AIRLOCK DOOR (UNLOCKED)";
                } else if (_hoveredEntity->isLocked) {
                    promptText = "[E] ACCESS RESTRICTED (KEYPAD LOCKED)";
                } else {
                    promptText = "[E] TOGGLE SWITCH";
                }

                LabFont::drawText(bx + 24.0f, by + 16.0f, promptText, 1.8f, Vec3(1.0f, 0.95f, 0.9f), LabFontType::GeoSans);
            }
        }

        // 2. Interaction Notice Banner (Upper-Mid Screen)
        if (_noticeTimer > 0.0f && !_lastInteractionNotice.empty()) {
            float nw = 620.0f;
            float nh = 40.0f;
            float nx = (sw - nw) * 0.5f;
            float ny = sh * 0.22f;

            Renderer::drawRect(nx, ny, nw, nh, Vec3(0.08f, 0.11f, 0.15f));
            Renderer::drawRect(nx, ny, nw, 2.0f, _noticeColor);
            LabFont::drawText(nx + 20.0f, ny + 12.0f, _lastInteractionNotice, 1.7f, _noticeColor, LabFontType::GeoSans);
        }
    }

} // namespace Lab
