#pragma once
#include "LabMath.h"
#include "LabRenderer.h"
#include "LabFont.h"
#include <algorithm>
#include <string>

namespace Lab {

    // Authentic HUD following exact reference layout & Half-Life 2 / Frozen-Life color palette
    class LabHUD {
    public:
        float health = 150.0f;
        float maxHealth = 150.0f;
        float suitArmor = 0.0f;
        float maxSuitArmor = 100.0f;
        int jumpVal = 50;
        int attackMult = 1;
        int speedVal = 16;
        int timerSecs = 480;
        std::string mapName = "ffa_baseplate2021";
        int ammoClip = 18;
        int ammoReserve = 144;

        // Colors strictly matched to user reference:
        // Translucent dark charcoal panel tiles with vibrant warm golden-yellow text / digits
        Vec3 textYellow{ 0.98f, 0.78f, 0.08f };      // Warm golden yellow (#FBC014)
        Vec3 tileBg{ 0.16f, 0.17f, 0.19f };          // Reference dark translucent card tile
        Vec3 tileBorder{ 0.22f, 0.23f, 0.26f };      // Subtle card edge
        Vec3 cryoCyan{ 0.22f, 0.88f, 1.0f };         // Cryo accent

        // Render card container tile with subtle border
        static void drawCard(float x, float y, float w, float h, const Vec3& bg, const Vec3& border) {
            Renderer::drawRect(x, y, w, h, bg);
            // 1px border highlight
            Renderer::drawRect(x, y, w, 1.0f, border);
            Renderer::drawRect(x, y + h - 1.0f, w, 1.0f, border);
            Renderer::drawRect(x, y, 1.0f, h, border);
            Renderer::drawRect(x + w - 1.0f, y, 1.0f, h, border);
        }

        void render(int screenW, int screenH) {
            Renderer::beginUI(screenW, screenH);

            // ==================== 1. TOP-RIGHT MAP & TIMER BADGE ====================
            // Matched directly to top row in reference: [ffa_baseplate2021   480]
            float topW = 280.0f;
            float topH = 34.0f;
            float topX = (float)screenW - topW - 40.0f;
            float topY = 30.0f;

            drawCard(topX, topY, topW, topH, tileBg, tileBorder);
            // Yellow Map Name label (uppercase / lowercase dot matrix)
            LabFont::drawText(topX + 14.0f, topY + 10.0f, mapName, 2.0f, textYellow);
            // Yellow Timer digits on the right
            std::string timeStr = std::to_string(timerSecs);
            LabFont::drawText(topX + topW - 55.0f, topY + 8.0f, timeStr, 2.4f, textYellow);

            // ==================== 2. MAIN HUD CARDS GRID (BOTTOM LEFT) ====================
            // Following reference 2-row layout:
            // Row 1: [JUMP 50]  [ATTACK 1x]  [SPEED 16]
            // Row 2: [SUIT  0%] [HEALTH 150]
            float startX = 40.0f;
            float cardW = 145.0f;
            float cardH = 55.0f;
            float gapX = 14.0f;
            float gapY = 12.0f;
            
            float row2Y = (float)screenH - cardH - 35.0f;
            float row1Y = row2Y - cardH - gapY;

            // --- Row 1, Card 1: JUMP ---
            float jx = startX;
            drawCard(jx, row1Y, cardW, cardH, tileBg, tileBorder);
            LabFont::drawText(jx + 12.0f, row1Y + 22.0f, "JUMP", 1.8f, textYellow);
            std::string jumpStr = std::to_string(jumpVal);
            LabFont::drawText(jx + cardW - 42.0f, row1Y + 12.0f, jumpStr, 3.8f, textYellow);

            // --- Row 1, Card 2: ATTACK ---
            float ax = jx + cardW + gapX;
            drawCard(ax, row1Y, cardW + 15.0f, cardH, tileBg, tileBorder);
            LabFont::drawText(ax + 12.0f, row1Y + 22.0f, "ATTACK", 1.7f, textYellow);
            std::string atkStr = std::to_string(attackMult) + "x";
            LabFont::drawText(ax + cardW - 35.0f, row1Y + 12.0f, atkStr, 3.8f, textYellow);

            // --- Row 1, Card 3: SPEED ---
            float sx = ax + cardW + 15.0f + gapX;
            drawCard(sx, row1Y, cardW, cardH, tileBg, tileBorder);
            LabFont::drawText(sx + 12.0f, row1Y + 22.0f, "SPEED", 1.7f, textYellow);
            std::string spdStr = std::to_string(speedVal);
            LabFont::drawText(sx + cardW - 42.0f, row1Y + 12.0f, spdStr, 3.8f, textYellow);

            // --- Row 2, Card 1: SUIT ---
            float suitCardW = 210.0f;
            float suitX = startX;
            drawCard(suitX, row2Y, suitCardW, cardH, tileBg, tileBorder);
            LabFont::drawText(suitX + 16.0f, row2Y + 22.0f, "SUIT", 1.9f, textYellow);
            std::string suitStr = std::to_string((int)suitArmor) + "%";
            LabFont::drawText(suitX + suitCardW - 75.0f, row2Y + 12.0f, suitStr, 3.8f, textYellow);

            // --- Row 2, Card 2: HEALTH ---
            float hpCardW = 225.0f;
            float hpX = suitX + suitCardW + gapX;
            drawCard(hpX, row2Y, hpCardW, cardH, tileBg, tileBorder);
            LabFont::drawText(hpX + 16.0f, row2Y + 22.0f, "HEALTH", 1.9f, textYellow);
            std::string hpStr = std::to_string((int)health);
            LabFont::drawText(hpX + hpCardW - 75.0f, row2Y + 12.0f, hpStr, 3.8f, textYellow);

            // ==================== 3. AMMO & WEAPON CARD (BOTTOM RIGHT) ====================
            float ammoW = 160.0f;
            float ammoX = (float)screenW - ammoW - 40.0f;
            drawCard(ammoX, row2Y, ammoW, cardH, tileBg, tileBorder);
            LabFont::drawText(ammoX + 12.0f, row2Y + 22.0f, "AMMO", 1.7f, textYellow);
            std::string clipStr = std::to_string(ammoClip);
            LabFont::drawText(ammoX + 60.0f, row2Y + 12.0f, clipStr, 3.8f, textYellow);
            LabFont::drawText(ammoX + 115.0f, row2Y + 24.0f, "/" + std::to_string(ammoReserve), 1.6f, textYellow * 0.75f);

            // ==================== 4. DYNAMIC CROSSHAIR (Screen Center) ====================
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

            Renderer::endUI();
        }
    };

}
