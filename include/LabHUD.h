#pragma once
#include "LabMath.h"
#include "LabRenderer.h"
#include <algorithm>
#include <string>

namespace Lab {

    // Authentic Half-Life 2 7-Segment Amber / Cryo Cyan HUD
    class LabHUD {
    public:
        float health = 100.0f;
        float maxHealth = 100.0f;
        float suitArmor = 85.0f;
        float maxSuitArmor = 100.0f;
        int ammoClip = 18;
        int ammoReserve = 144;
        bool hasSuit = true;

        // HL2 Iconic Palette
        Vec3 hl2Amber{ 1.0f, 0.68f, 0.10f };       // Bright active Amber
        Vec3 hl2DarkAmber{ 0.28f, 0.15f, 0.02f };   // Inactive 7-segment segment
        Vec3 hl2Bg{ 0.04f, 0.05f, 0.07f };          // Deep dark translucent panel
        Vec3 cryoCyan{ 0.22f, 0.88f, 1.0f };        // Cryo armor cyan
        Vec3 cryoDarkCyan{ 0.05f, 0.20f, 0.25f };   // Inactive cyan

        // Draws a stylized digital 7-segment numeral (0-9)
        static void drawDigit(float x, float y, int digit, float w, float h, float thick, const Vec3& onColor, const Vec3& offColor) {
            // Segment bitmask: a(top), b(top-right), c(bot-right), d(bottom), e(bot-left), f(top-left), g(middle)
            static const unsigned char segs[10] = {
                0x3F, // 0: a,b,c,d,e,f
                0x06, // 1: b,c
                0x5B, // 2: a,b,g,e,d
                0x4F, // 3: a,b,g,c,d
                0x66, // 4: f,g,b,c
                0x6D, // 5: a,f,g,c,d
                0x7D, // 6: a,f,e,d,c,g
                0x07, // 7: a,b,c
                0x7F, // 8: all
                0x6F  // 9: a,b,c,d,f,g
            };

            unsigned char mask = (digit >= 0 && digit <= 9) ? segs[digit] : 0;
            float midY = y + (h - thick) * 0.5f;
            float halfH = (h - thick * 3.0f) * 0.5f;

            // a: Top horizontal
            Renderer::drawRect(x + thick, y, w - thick * 2, thick, (mask & 0x01) ? onColor : offColor);
            // b: Top-right vertical
            Renderer::drawRect(x + w - thick, y + thick, thick, halfH, (mask & 0x02) ? onColor : offColor);
            // c: Bottom-right vertical
            Renderer::drawRect(x + w - thick, midY + thick, thick, halfH, (mask & 0x04) ? onColor : offColor);
            // d: Bottom horizontal
            Renderer::drawRect(x + thick, y + h - thick, w - thick * 2, thick, (mask & 0x08) ? onColor : offColor);
            // e: Bottom-left vertical
            Renderer::drawRect(x, midY + thick, thick, halfH, (mask & 0x10) ? onColor : offColor);
            // f: Top-left vertical
            Renderer::drawRect(x, y + thick, thick, halfH, (mask & 0x20) ? onColor : offColor);
            // g: Middle horizontal
            Renderer::drawRect(x + thick, midY, w - thick * 2, thick, (mask & 0x40) ? onColor : offColor);
        }

        // Draws multi-digit integer with leading zeroes / ghost segments
        static void drawNumber(float x, float y, int val, int digits, float digitW, float digitH, float thick, float gap, const Vec3& onColor, const Vec3& offColor) {
            val = std::clamp(val, 0, 9999);
            std::string s = std::to_string(val);
            while ((int)s.size() < digits) s = " " + s;

            for (int i = 0; i < digits; ++i) {
                float dx = x + i * (digitW + gap);
                if (s[i] == ' ') {
                    // Draw inactive ghost background segments
                    drawDigit(dx, y, -1, digitW, digitH, thick, onColor, offColor);
                } else {
                    drawDigit(dx, y, s[i] - '0', digitW, digitH, thick, onColor, offColor);
                }
            }
        }

        void render(int screenW, int screenH) {
            Renderer::beginUI(screenW, screenH);

            float panelH = 58.0f;
            float bottomY = (float)screenH - panelH - 25.0f;
            float digW = 20.0f;
            float digH = 36.0f;
            float digThick = 4.0f;
            float digGap = 5.0f;

            // ==================== 1. HEALTH MODULE (Bottom Left) ====================
            float healthPanelW = 220.0f;
            Renderer::drawRect(35.0f, bottomY, healthPanelW, panelH, hl2Bg);
            Renderer::drawRect(35.0f, bottomY, healthPanelW, 2.0f, hl2Amber * 0.6f);
            
            // Health Label Box (HL2 Cross + Label)
            Renderer::drawRect(48.0f, bottomY + 22.0f, 18.0f, 6.0f, hl2Amber);
            Renderer::drawRect(54.0f, bottomY + 16.0f, 6.0f, 18.0f, hl2Amber);

            // Large 3-digit Amber Health Number
            drawNumber(90.0f, bottomY + 11.0f, (int)health, 3, digW, digH, digThick, digGap, hl2Amber, hl2DarkAmber);

            // Miniature Health Bar fill underneath
            float hpRatio = std::clamp(health / maxHealth, 0.0f, 1.0f);
            Renderer::drawRect(35.0f, bottomY + panelH - 4.0f, healthPanelW * hpRatio, 4.0f, hl2Amber);

            // ==================== 2. SUIT / ARMOR MODULE (Bottom Left-Center) ====================
            if (hasSuit) {
                float suitX = 275.0f;
                float suitPanelW = 220.0f;
                Renderer::drawRect(suitX, bottomY, suitPanelW, panelH, hl2Bg);
                Renderer::drawRect(suitX, bottomY, suitPanelW, 2.0f, cryoCyan * 0.6f);

                // Suit Shield Icon
                Renderer::drawRect(suitX + 16.0f, bottomY + 16.0f, 14.0f, 18.0f, cryoCyan);
                Renderer::drawRect(suitX + 18.0f, bottomY + 20.0f, 10.0f, 10.0f, hl2Bg);

                // Large 3-digit Cyan Armor Number
                drawNumber(suitX + 55.0f, bottomY + 11.0f, (int)suitArmor, 3, digW, digH, digThick, digGap, cryoCyan, cryoDarkCyan);

                // Suit Bar fill
                float suitRatio = std::clamp(suitArmor / maxSuitArmor, 0.0f, 1.0f);
                Renderer::drawRect(suitX, bottomY + panelH - 4.0f, suitPanelW * suitRatio, 4.0f, cryoCyan);
            }

            // ==================== 3. AMMO MODULE (Bottom Right) ====================
            float ammoPanelW = 240.0f;
            float ammoX = (float)screenW - ammoPanelW - 35.0f;
            Renderer::drawRect(ammoX, bottomY, ammoPanelW, panelH, hl2Bg);
            Renderer::drawRect(ammoX, bottomY, ammoPanelW, 2.0f, hl2Amber * 0.6f);

            // Ammo Clip Number (2 Digits, Big)
            drawNumber(ammoX + 20.0f, bottomY + 11.0f, ammoClip, 2, digW, digH, digThick, digGap, hl2Amber, hl2DarkAmber);

            // Divider strip
            Renderer::drawRect(ammoX + 85.0f, bottomY + 10.0f, 2.0f, 38.0f, hl2DarkAmber);

            // Reserve Ammo Number (3 Digits, slightly smaller)
            drawNumber(ammoX + 105.0f, bottomY + 16.0f, ammoReserve, 3, 15.0f, 28.0f, 3.0f, 4.0f, hl2Amber * 0.85f, hl2DarkAmber);

            // Ammo Tick Indicators (18 small tick bars)
            float tickW = 7.0f;
            float tickGap = 4.0f;
            float tickStartX = ammoX + 16.0f;
            for (int i = 0; i < 18; ++i) {
                Vec3 color = (i < ammoClip) ? hl2Amber : hl2DarkAmber;
                Renderer::drawRect(tickStartX + i * (tickW + tickGap) * 0.55f, bottomY + panelH - 6.0f, tickW * 0.5f, 4.0f, color);
            }

            // ==================== 4. SOURCE / HL2 DYNAMIC CROSSHAIR ====================
            float cx = (float)screenW * 0.5f;
            float cy = (float)screenH * 0.5f;
            float crosshairGap = 6.0f;
            float crosshairLen = 9.0f;
            float crosshairThick = 2.0f;

            // 4 directional brackets
            Renderer::drawRect(cx - crosshairGap - crosshairLen, cy - crosshairThick * 0.5f, crosshairLen, crosshairThick, hl2Amber);
            Renderer::drawRect(cx + crosshairGap, cy - crosshairThick * 0.5f, crosshairLen, crosshairThick, hl2Amber);
            Renderer::drawRect(cx - crosshairThick * 0.5f, cy - crosshairGap - crosshairLen, crosshairThick, crosshairLen, hl2Amber);
            Renderer::drawRect(cx - crosshairThick * 0.5f, cy + crosshairGap, crosshairThick, crosshairLen, hl2Amber);
            // Center pip
            Renderer::drawRect(cx - 1.0f, cy - 1.0f, 2.0f, 2.0f, hl2Amber);

            Renderer::endUI();
        }
    };

}
