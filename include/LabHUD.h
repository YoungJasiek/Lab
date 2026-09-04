#pragma once
#include "LabMath.h"
#include "LabRenderer.h"
#include <algorithm>
#include <string>

namespace Lab {

    // Half-Life 2 Inspired Amber & Cyan HUD
    class LabHUD {
    public:
        float health = 100.0f;
        float maxHealth = 100.0f;
        float suitArmor = 85.0f;
        float maxSuitArmor = 100.0f;
        int ammoClip = 18;
        int ammoReserve = 144;
        bool hasSuit = true;

        // Colors (HL2 Classic Warm Amber / Cryo Cyan)
        Vec3 hl2Amber{ 1.0f, 0.65f, 0.12f };
        Vec3 hl2DarkAmber{ 0.35f, 0.2f, 0.04f };
        Vec3 cryoCyan{ 0.2f, 0.85f, 1.0f };
        Vec3 darkBg{ 0.05f, 0.06f, 0.08f };

        void render(int screenW, int screenH) {
            Renderer::beginUI(screenW, screenH);

            float bottomY = (float)screenH - 75.0f;

            // ==================== 1. HEALTH MODULE (Bottom Left) ====================
            // Frame container
            Renderer::drawRect(40.0f, bottomY, 190.0f, 50.0f, darkBg);
            Renderer::drawRect(40.0f, bottomY, 190.0f, 2.0f, hl2Amber * 0.5f);
            
            // Health Icon (Cross)
            Renderer::drawRect(55.0f, bottomY + 16.0f, 18.0f, 6.0f, hl2Amber);
            Renderer::drawRect(61.0f, bottomY + 10.0f, 6.0f, 18.0f, hl2Amber);

            // Health Bar Fill
            float healthRatio = std::clamp(health / maxHealth, 0.0f, 1.0f);
            Renderer::drawRect(85.0f, bottomY + 15.0f, 130.0f, 18.0f, hl2DarkAmber);
            Renderer::drawRect(85.0f, bottomY + 15.0f, 130.0f * healthRatio, 18.0f, hl2Amber);

            // ==================== 2. SUIT / ARMOR MODULE (Bottom Left-Center) ====================
            if (hasSuit) {
                float suitX = 245.0f;
                Renderer::drawRect(suitX, bottomY, 190.0f, 50.0f, darkBg);
                Renderer::drawRect(suitX, bottomY, 190.0f, 2.0f, cryoCyan * 0.5f);

                // Suit Icon (Shield / Battery block)
                Renderer::drawRect(suitX + 15.0f, bottomY + 12.0f, 14.0f, 16.0f, cryoCyan);

                // Suit Bar Fill
                float suitRatio = std::clamp(suitArmor / maxSuitArmor, 0.0f, 1.0f);
                Renderer::drawRect(suitX + 45.0f, bottomY + 15.0f, 130.0f, 18.0f, Vec3(0.08f, 0.22f, 0.3f));
                Renderer::drawRect(suitX + 45.0f, bottomY + 15.0f, 130.0f * suitRatio, 18.0f, cryoCyan);
            }

            // ==================== 3. AMMO MODULE (Bottom Right) ====================
            float ammoX = (float)screenW - 230.0f;
            Renderer::drawRect(ammoX, bottomY, 190.0f, 50.0f, darkBg);
            Renderer::drawRect(ammoX, bottomY, 190.0f, 2.0f, hl2Amber * 0.5f);

            // Ammo Clip bars (HL2 style ammo ticks)
            int totalTicks = 18;
            float tickW = 4.0f;
            float tickGap = 2.0f;
            float tickStartX = ammoX + 18.0f;
            for (int i = 0; i < totalTicks; ++i) {
                Vec3 color = (i < ammoClip) ? hl2Amber : hl2DarkAmber;
                Renderer::drawRect(tickStartX + i * (tickW + tickGap), bottomY + 18.0f, tickW, 14.0f, color);
            }

            // Reserve Ammo Indicator Box
            Renderer::drawRect(ammoX + 135.0f, bottomY + 12.0f, 40.0f, 26.0f, hl2DarkAmber);
            Renderer::drawRect(ammoX + 138.0f, bottomY + 15.0f, 34.0f, 20.0f, darkBg);
            Renderer::drawRect(ammoX + 142.0f, bottomY + 22.0f, 26.0f, 6.0f, hl2Amber);

            // ==================== 4. DYNAMIC CROSSHAIR (Screen Center) ====================
            float cx = (float)screenW * 0.5f;
            float cy = (float)screenH * 0.5f;
            float gap = 7.0f;
            float len = 8.0f;
            float thick = 2.0f;

            // Half-Life 2 style brackets
            Renderer::drawRect(cx - gap - len, cy - thick * 0.5f, len, thick, hl2Amber);
            Renderer::drawRect(cx + gap, cy - thick * 0.5f, len, thick, hl2Amber);
            Renderer::drawRect(cx - thick * 0.5f, cy - gap - len, thick, len, hl2Amber);
            Renderer::drawRect(cx - thick * 0.5f, cy + gap, thick, len, hl2Amber);

            // Center pip
            Renderer::drawRect(cx - 1.0f, cy - 1.0f, 2.0f, 2.0f, hl2Amber);

            Renderer::endUI();
        }
    };

}
