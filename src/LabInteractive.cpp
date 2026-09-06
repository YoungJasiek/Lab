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

                // CRT scanlines: alternating bright and dark lines
                bool scanline = (y % 3 == 0);
                // Subtle matrix vertical grid lines
                bool gridline = (x % 16 == 0 || y % 16 == 0);

                // Distance from screen center for CRT barrel vignette
                float dx = (x - 31.5f) / 32.0f;
                float dy = (y - 31.5f) / 32.0f;
                float vign = std::clamp(1.0f - (dx * dx + dy * dy) * 0.5f, 0.3f, 1.0f);

                unsigned char base = scanline ? 180 : 230;
                if (gridline) base = (unsigned char)std::min(255, base + 25);
                base = (unsigned char)(base * vign);

                img[idx + 0] = (unsigned char)(base * 0.75f);
                img[idx + 1] = base;
                img[idx + 2] = (unsigned char)(base * 0.90f);
                img[idx + 3] = 255;
            }
        }
        _texScreenGrid = std::make_unique<Texture>(img.data(), S, S, 4);
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

            // Also allow close-range forward cone check
            Vec3 toEnt = ent.position - playerPos;
            float d = toEnt.length();
            float dot = Vec3::dot(normLook, toEnt / std::max(0.001f, d));

            if ((hitBox && t < bestDist) || (d < bestDist && dot > 0.94f)) {
                float pickDist = hitBox ? t : d;
                if (pickDist < bestDist) {
                    bestDist = pickDist;
                    _hoveredEntity = &ent;
                    _hoveredIndex = (int)i;
                }
            }
        }

        // Handle player [E] interaction
        if (_hoveredEntity && useKeyPressed && _hoveredEntity->cooldown <= 0.0f) {
            _hoveredEntity->cooldown = 0.55f;

            if (_hoveredEntity->isLocked) {
                AudioEngine::playSound(SoundID::AccessDenied, 1.0f);
                _hoveredEntity->statusText = "ACCESS DENIED - SECURITY LOCKED";
                _hoveredEntity->themeColor = Vec3(1.0f, 0.25f, 0.2f);
                _lastInteractionNotice = "ACCESS DENIED: CLEARANCE REQUIRED";
                _noticeColor = Vec3(1.0f, 0.3f, 0.25f);
                _noticeTimer = 2.8f;
            } else {
                _hoveredEntity->isActivated = !_hoveredEntity->isActivated;
                AudioEngine::playSound(SoundID::AccessGranted, 0.95f);
                AudioEngine::playSound(SoundID::TerminalBeep, 0.85f);

                if (_hoveredEntity->isActivated) {
                    _hoveredEntity->statusText = "ACCESS GRANTED - AIRLOCK OPEN";
                    _hoveredEntity->themeColor = Vec3(0.2f, 0.95f, 0.35f);
                    _lastInteractionNotice = "SECURITY OVERRIDE: AIRLOCK CYCLING OPEN";
                    _noticeColor = Vec3(0.25f, 0.95f, 0.35f);
                } else {
                    _hoveredEntity->statusText = "STANDBY - AIRLOCK SECURED";
                    _hoveredEntity->themeColor = Vec3(0.2f, 0.8f, 1.0f);
                    _lastInteractionNotice = "AIRLOCK CYCLE: CLOSED";
                    _noticeColor = Vec3(0.3f, 0.85f, 1.0f);
                }
                _noticeTimer = 2.8f;

                // Operate linked target door in the map
                if (_hoveredEntity->targetDoorIndex >= 0 && map) {
                    if ((size_t)_hoveredEntity->targetDoorIndex < map->doors.size()) {
                        map->doors[_hoveredEntity->targetDoorIndex].isOpen = _hoveredEntity->isActivated;
                    }
                }
            }
        }
    }

    void InteractiveSystem::render(const Camera& /*cam*/) {
        if (_entities.empty()) return;
        init();

        for (const auto& ent : _entities) {
            bool isHovered = (&ent == _hoveredEntity);

            // 1. Dark metallic console housing
            Vec3 housingCol = Vec3(0.12f, 0.14f, 0.18f);
            if (isHovered) {
                housingCol = Vec3(0.16f, 0.18f, 0.24f);
            }
            Renderer::drawCube(ent.position, ent.size, housingCol, nullptr, true);

            // 2. Beveled bezel border
            Vec3 norm = ent.normal.normalized();
            if (norm.lengthSq() < 0.001f) norm = Vec3(0, 0, 1);

            Vec3 scrOffset = norm * (ent.size.z * 0.5f + 0.004f);
            Vec3 scrPos = ent.position + scrOffset;
            Vec3 scrSize(ent.size.x * 0.86f, ent.size.y * 0.82f, 0.004f);

            // 3. CRT Terminal Screen Face with scanline texture
            Vec3 screenGlow = ent.themeColor * (isHovered ? 1.45f : 1.0f);
            Renderer::drawCube(scrPos, scrSize, screenGlow, _texScreenGrid.get(), true);

            // 4. Status Indicator LED on housing bezel
            Vec3 ledOffset = Vec3(0.0f, ent.size.y * 0.44f, ent.size.z * 0.5f + 0.008f);
            Vec3 ledPos = ent.position + ledOffset;
            Vec3 ledCol = ent.isLocked ? Vec3(1.0f, 0.15f, 0.15f) 
                                       : (ent.isActivated ? Vec3(0.15f, 1.0f, 0.35f) : Vec3(0.2f, 0.8f, 1.0f));
            Renderer::drawCube(ledPos, Vec3(0.05f, 0.05f, 0.02f), ledCol, nullptr, false);

            // 5. In-World Telemetry Bar Gauge
            float barW = scrSize.x * 0.68f;
            Vec3 barPos = scrPos + Vec3(0.0f, -scrSize.y * 0.22f, 0.003f);
            Renderer::drawCube(barPos, Vec3(barW, 0.028f, 0.003f), screenGlow * 1.3f, nullptr, false);

            // 6. Secondary Accent LED Row
            for (int ledIdx = -2; ledIdx <= 2; ++ledIdx) {
                float lx = (float)ledIdx * 0.06f;
                Vec3 subLedPos = scrPos + Vec3(lx, scrSize.y * 0.32f, 0.003f);
                Vec3 subLedCol = (ledIdx <= 0 || ent.isActivated) ? ledCol : Vec3(0.2f, 0.25f, 0.3f);
                Renderer::drawCube(subLedPos, Vec3(0.028f, 0.028f, 0.003f), subLedCol, nullptr, false);
            }
        }
    }

    void InteractiveSystem::renderHUD(int screenWidth, int screenHeight) {
        float sw = (float)screenWidth;
        float sh = (float)screenHeight;

        // Interaction Prompt at Center-Bottom
        if (_hoveredEntity) {
            float cx = sw * 0.5f;
            float cy = sh * 0.72f;
            float boxW = 520.0f;
            float boxH = 54.0f;

            // Background panel
            Renderer::drawRect(cx - boxW * 0.5f, cy, boxW, boxH, Vec3(0.08f, 0.10f, 0.14f));

            // Accent border line
            Vec3 accent = _hoveredEntity->isLocked ? Vec3(1.0f, 0.3f, 0.25f) : _hoveredEntity->themeColor;
            Renderer::drawRect(cx - boxW * 0.5f, cy, boxW, 2.0f, accent);
            Renderer::drawRect(cx - boxW * 0.5f, cy + boxH - 2.0f, boxW, 2.0f, Vec3(0.18f, 0.22f, 0.28f));

            // Key icon box [E]
            Renderer::drawRect(cx - boxW * 0.5f + 16.0f, cy + 10.0f, 34.0f, 34.0f, Vec3(0.18f, 0.22f, 0.28f));
            Renderer::drawRect(cx - boxW * 0.5f + 16.0f, cy + 10.0f, 34.0f, 1.0f, Vec3(1.0f, 0.85f, 0.2f));
            LabFont::drawText(cx - boxW * 0.5f + 26.0f, cy + 16.0f, "E", 1.6f, Vec3(1.0f, 0.85f, 0.2f), LabFontType::GeoSans);

            // Action label
            std::string prompt = "USE " + _hoveredEntity->title + " - [" + _hoveredEntity->statusText + "]";
            LabFont::drawText(cx - boxW * 0.5f + 62.0f, cy + 17.0f, prompt, 1.4f, Vec3(0.95f, 0.96f, 0.98f), LabFontType::GeoSans);
        }

        // Diagnostic Notice Banner
        if (_noticeTimer > 0.0f && !_lastInteractionNotice.empty()) {
            float ncy = 135.0f;
            float nbw = 560.0f;
            float nbh = 38.0f;
            float ncx = sw * 0.5f - nbw * 0.5f;

            Renderer::drawRect(ncx, ncy, nbw, nbh, Vec3(0.10f, 0.12f, 0.16f));
            Renderer::drawRect(ncx, ncy, nbw, 1.5f, _noticeColor);
            Renderer::drawRect(ncx, ncy + nbh - 1.0f, nbw, 1.0f, Vec3(0.20f, 0.24f, 0.30f));

            LabFont::drawText(ncx + 20.0f, ncy + 9.0f, _lastInteractionNotice, 1.4f, _noticeColor, LabFontType::GeoSans);
        }
    }

} // namespace Lab
