#pragma once
#include "LabMath.h"
#include "LabRenderer.h"
#include "LabFont.h"
#include <algorithm>
#include <string>

namespace Lab {

    // Authentic HUD: Strictly SUIT & HEALTH + Top Map/Timer + Ammo
    // Icons drawn cleanly with vector geometry & dot-matrix font
    class LabHUD {
    public:
        float health = 150.0f;
        float maxHealth = 150.0f;
        float suitArmor = 0.0f;
        float maxSuitArmor = 100.0f;
        int timerSecs = 480;
        std::string mapName = "ffa_baseplate2021";
        int ammoClip = 18;
        int ammoReserve = 144;
        int frags = 0;
        std::string gameModeName = "FFA";
        float hitmarkerTimer = 0.0f;
        bool hitmarkerHeadshot = false;
        float damageFlashTimer = 0.0f;
        std::string combatMessage = "";
        float combatMessageTimer = 0.0f;

        void triggerHitmarker(bool headshot) {
            hitmarkerTimer = 0.15f;
            hitmarkerHeadshot = headshot;
        }

        void triggerDamageFlash() {
            damageFlashTimer = 0.22f;
        }

        void showCombatMessage(const std::string& msg, float duration = 2.5f) {
            combatMessage = msg;
            combatMessageTimer = duration;
        }

        void update(float dt) {
            if (hitmarkerTimer > 0.0f) hitmarkerTimer -= dt;
            if (damageFlashTimer > 0.0f) damageFlashTimer -= dt;
            if (combatMessageTimer > 0.0f) combatMessageTimer -= dt;
        }

        // Colors strictly matched to user reference:
        Vec3 textYellow{ 0.98f, 0.78f, 0.08f };      // Warm golden yellow (#FBC014)
        Vec3 textDimYellow{ 0.45f, 0.35f, 0.04f };   // Darker yellow
        Vec3 tileBg{ 0.16f, 0.17f, 0.19f };          // Reference dark translucent card tile
        Vec3 tileBorder{ 0.23f, 0.24f, 0.27f };      // Subtle card edge

        static void drawCard(float x, float y, float w, float h, const Vec3& bg, const Vec3& border) {
            Renderer::drawRect(x, y, w, h, bg);
            Renderer::drawRect(x, y, w, 1.0f, border);
            Renderer::drawRect(x, y + h - 1.0f, w, 1.0f, border);
            Renderer::drawRect(x, y, 1.0f, h, border);
            Renderer::drawRect(x + w - 1.0f, y, 1.0f, h, border);
        }

        // Draw iconic Health Cross Icon
        static void drawHealthIcon(float x, float y, float size, const Vec3& color) {
            float arm = size * 0.333f;
            // Vertical bar
            Renderer::drawRect(x + arm, y, arm, size, color);
            // Horizontal bar
            Renderer::drawRect(x, y + arm, size, arm, color);
        }

        // Draw iconic Suit Shield / Battery Icon
        static void drawSuitIcon(float x, float y, float w, float h, const Vec3& color) {
            // Shield top & body
            Renderer::drawRect(x, y, w, h * 0.7f, color);
            // Shield bottom wedge
            Renderer::drawRect(x + w * 0.2f, y + h * 0.7f, w * 0.6f, h * 0.2f, color);
            Renderer::drawRect(x + w * 0.4f, y + h * 0.9f, w * 0.2f, h * 0.1f, color);
            // Hollow inner core
            Renderer::drawRect(x + 2.0f, y + 2.0f, w - 4.0f, h * 0.5f, Vec3(0.16f, 0.17f, 0.19f));
        }

        // Draw Clock / Timer Icon
        static void drawClockIcon(float x, float y, float r, const Vec3& color) {
            Renderer::drawRect(x, y, r * 2.0f, r * 2.0f, color);
            Renderer::drawRect(x + 2.0f, y + 2.0f, r * 2.0f - 4.0f, r * 2.0f - 4.0f, Vec3(0.16f, 0.17f, 0.19f));
            // Hands
            Renderer::drawRect(x + r - 1.0f, y + 3.0f, 2.0f, r - 2.0f, color);
            Renderer::drawRect(x + r - 1.0f, y + r - 1.0f, r - 3.0f, 2.0f, color);
        }

        // Draw Bullet / Ammo Clip Icon
        static void drawAmmoIcon(float x, float y, const Vec3& color) {
            // Bullet tip
            Renderer::drawRect(x + 3.0f, y, 4.0f, 3.0f, color);
            // Bullet body
            Renderer::drawRect(x + 1.0f, y + 3.0f, 8.0f, 10.0f, color);
            // Bullet casing rim
            Renderer::drawRect(x, y + 13.0f, 10.0f, 3.0f, color);
        }

        void render(int screenW, int screenH) {
            Renderer::beginUI(screenW, screenH);

            // Damage flash border overlay
            if (damageFlashTimer > 0.0f) {
                float alpha = std::clamp(damageFlashTimer / 0.22f, 0.0f, 1.0f);
                Vec3 flashCol = Vec3(0.85f * alpha, 0.08f * alpha, 0.08f * alpha);
                float borderThick = 12.0f;
                Renderer::drawRect(0, 0, (float)screenW, borderThick, flashCol);
                Renderer::drawRect(0, (float)screenH - borderThick, (float)screenW, borderThick, flashCol);
                Renderer::drawRect(0, 0, borderThick, (float)screenH, flashCol);
                Renderer::drawRect((float)screenW - borderThick, 0, borderThick, (float)screenH, flashCol);
            }

            // ==================== 1. TOP-RIGHT MAP & TIMER BADGE ====================
            float topW = 380.0f;
            float topH = 36.0f;
            float topX = (float)screenW - topW - 40.0f;
            float topY = 30.0f;

            drawCard(topX, topY, topW, topH, tileBg, tileBorder);
            
            // Map Name + Mode label
            std::string headerStr = mapName + " [" + gameModeName + "]";
            LabFont::drawText(topX + 16.0f, topY + 11.0f, headerStr, 1.8f, textYellow, LabFontType::GeoSans);
            
            // Frags badge
            std::string fragStr = "KILLS: " + std::to_string(frags);
            LabFont::drawText(topX + topW - 145.0f, topY + 11.0f, fragStr, 1.8f, Vec3(0.3f, 0.85f, 1.0f), LabFontType::GeoSans);

            // Clock icon + Timer digits
            drawClockIcon(topX + topW - 65.0f, topY + 10.0f, 8.0f, textYellow);
            std::string timeStr = std::to_string(timerSecs);
            LabFont::drawText(topX + topW - 42.0f, topY + 9.0f, timeStr, 2.2f, textYellow, LabFontType::GeoSans);

            // Combat notification banner (Upper center)
            if (combatMessageTimer > 0.0f && !combatMessage.empty()) {
                float msgW = 480.0f;
                float msgH = 36.0f;
                float msgX = ((float)screenW - msgW) * 0.5f;
                float msgY = 85.0f;
                drawCard(msgX, msgY, msgW, msgH, Vec3(0.12f, 0.14f, 0.18f), Vec3(0.3f, 0.7f, 1.0f));
                LabFont::drawText(msgX + 24.0f, msgY + 10.0f, combatMessage, 1.9f, Vec3(1.0f, 0.95f, 0.4f), LabFontType::GeoSans);
            }

            // ==================== 2. MAIN HUD CARDS: SUIT & HEALTH (BOTTOM LEFT) ====================
            float startX = 40.0f;
            float cardH = 68.0f;
            float cardY = (float)screenH - cardH - 40.0f;
            float gapX = 18.0f;

            // --- CARD 1: SUIT (Armor) ---
            float suitCardW = 240.0f;
            float suitX = startX;
            drawCard(suitX, cardY, suitCardW, cardH, tileBg, tileBorder);
            
            drawSuitIcon(suitX + 16.0f, cardY + 18.0f, 16.0f, 20.0f, textYellow);
            LabFont::drawText(suitX + 38.0f, cardY + 26.0f, "SUIT", 2.2f, textYellow, LabFontType::GeoSans);

            std::string suitStr = std::to_string((int)suitArmor) + "%";
            LabFont::drawText(suitX + suitCardW - 88.0f, cardY + 16.0f, suitStr, 4.4f, textYellow, LabFontType::GeoSans);

            // --- CARD 2: HEALTH ---
            float hpCardW = 255.0f;
            float hpX = suitX + suitCardW + gapX;
            drawCard(hpX, cardY, hpCardW, cardH, tileBg, tileBorder);

            drawHealthIcon(hpX + 16.0f, cardY + 19.0f, 18.0f, (health < 30.0f ? Vec3(1.0f, 0.2f, 0.2f) : textYellow));
            LabFont::drawText(hpX + 42.0f, cardY + 26.0f, "HEALTH", 2.2f, textYellow, LabFontType::GeoSans);

            std::string hpStr = std::to_string((int)health);
            Vec3 hpColor = (health < 30.0f) ? Vec3(1.0f, 0.2f, 0.2f) : textYellow;
            LabFont::drawText(hpX + hpCardW - 88.0f, cardY + 16.0f, hpStr, 4.4f, hpColor, LabFontType::GeoSans);

            // ==================== 3. AMMO CARD (BOTTOM RIGHT) ====================
            float ammoW = 180.0f;
            float ammoX = (float)screenW - ammoW - 40.0f;
            drawCard(ammoX, cardY, ammoW, cardH, tileBg, tileBorder);

            drawAmmoIcon(ammoX + 16.0f, cardY + 18.0f, textYellow);
            LabFont::drawText(ammoX + 34.0f, cardY + 26.0f, "AMMO", 2.0f, textYellow, LabFontType::GeoSans);

            std::string clipStr = std::to_string(ammoClip);
            LabFont::drawText(ammoX + 85.0f, cardY + 16.0f, clipStr, 4.4f, textYellow, LabFontType::GeoSans);
            LabFont::drawText(ammoX + 140.0f, cardY + 30.0f, "/" + std::to_string(ammoReserve), 1.7f, textYellow * 0.75f, LabFontType::GeoSans);

            // ==================== 4. DYNAMIC CROSSHAIR & HITMARKER ====================
            float cx = (float)screenW * 0.5f;
            float cy = (float)screenH * 0.5f;
            float crossGap = 5.0f;
            float crossLen = 8.0f;
            float crossThick = 2.0f;

            Renderer::drawRect(cx - crossGap - crossLen, cy - crossThick * 0.5f, crossLen, crossThick, textYellow);
            Renderer::drawRect(cx + crossGap, cy - crossThick * 0.5f, crossLen, crossThick, textYellow);
            Renderer::drawRect(cx - crossThick * 0.5f, cy - crossGap - crossLen, crossThick, crossLen, textYellow);
            Renderer::drawRect(cx - crossThick * 0.5f, cy + crossGap, crossThick, crossLen, textYellow);
            Renderer::drawRect(cx - 1.0f, cy - 1.0f, 2.0f, 2.0f, textYellow);

            // Hitmarker X
            if (hitmarkerTimer > 0.0f) {
                Vec3 hmCol = hitmarkerHeadshot ? Vec3(1.0f, 0.15f, 0.15f) : Vec3(1.0f, 0.95f, 0.2f);
                float hThick = 2.5f;
                // Diagonal X notches
                Renderer::drawRect(cx - 10.0f, cy - 10.0f, 5.0f, hThick, hmCol);
                Renderer::drawRect(cx - 10.0f, cy - 10.0f, hThick, 5.0f, hmCol);

                Renderer::drawRect(cx + 6.0f, cy - 10.0f, 5.0f, hThick, hmCol);
                Renderer::drawRect(cx + 9.0f, cy - 10.0f, hThick, 5.0f, hmCol);

                Renderer::drawRect(cx - 10.0f, cy + 9.0f, 5.0f, hThick, hmCol);
                Renderer::drawRect(cx - 10.0f, cy + 6.0f, hThick, 5.0f, hmCol);

                Renderer::drawRect(cx + 6.0f, cy + 9.0f, 5.0f, hThick, hmCol);
                Renderer::drawRect(cx + 9.0f, cy + 6.0f, hThick, 5.0f, hmCol);
            }

            Renderer::endUI();
        }
    };

}
