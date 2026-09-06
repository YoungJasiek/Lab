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
        std::string weaponName = "PISTOLET";
        bool isMeleeWeapon = false;
        int activeWeaponSlot = 2; // 1 to 9
        float weaponSelectorTimer = 0.0f;
        std::vector<std::string> slotWeaponNames;
        std::vector<bool> slotUnlocked;

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
            if (weaponSelectorTimer > 0.0f) weaponSelectorTimer -= dt;
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

            int dispSuit = std::max(0, (int)std::ceil(suitArmor));
            std::string suitStr = std::to_string(dispSuit) + "%";
            LabFont::drawText(suitX + suitCardW - 88.0f, cardY + 16.0f, suitStr, 4.4f, textYellow, LabFontType::GeoSans);

            // --- CARD 2: HEALTH ---
            float hpCardW = 255.0f;
            float hpX = suitX + suitCardW + gapX;
            drawCard(hpX, cardY, hpCardW, cardH, tileBg, tileBorder);

            drawHealthIcon(hpX + 16.0f, cardY + 19.0f, 18.0f, (health < 30.0f ? Vec3(1.0f, 0.2f, 0.2f) : textYellow));
            LabFont::drawText(hpX + 42.0f, cardY + 26.0f, "HEALTH", 2.2f, textYellow, LabFontType::GeoSans);

            int dispHp = std::max(0, (int)std::ceil(health));
            std::string hpStr = std::to_string(dispHp);
            Vec3 hpColor = (health < 30.0f) ? Vec3(1.0f, 0.2f, 0.2f) : textYellow;
            LabFont::drawText(hpX + hpCardW - 88.0f, cardY + 16.0f, hpStr, 4.4f, hpColor, LabFontType::GeoSans);

            // ==================== 3. AMMO & WEAPON CARD (BOTTOM RIGHT) ====================
            float ammoW = 210.0f;
            float ammoX = (float)screenW - ammoW - 40.0f;
            drawCard(ammoX, cardY, ammoW, cardH, tileBg, tileBorder);

            drawAmmoIcon(ammoX + 16.0f, cardY + 18.0f, textYellow);
            LabFont::drawText(ammoX + 34.0f, cardY + 14.0f, weaponName, 1.8f, textYellow, LabFontType::GeoSans);

            if (isMeleeWeapon) {
                LabFont::drawText(ammoX + 34.0f, cardY + 36.0f, "MELEE WEAPON", 1.4f, textYellow * 0.7f, LabFontType::GeoSans);
                LabFont::drawText(ammoX + 135.0f, cardY + 18.0f, "INF", 3.8f, textYellow, LabFontType::GeoSans);
            } else {
                std::string clipStr = std::to_string(ammoClip);
                LabFont::drawText(ammoX + 105.0f, cardY + 16.0f, clipStr, 4.0f, textYellow, LabFontType::GeoSans);
                LabFont::drawText(ammoX + 155.0f, cardY + 30.0f, "/" + std::to_string(ammoReserve), 1.6f, textYellow * 0.75f, LabFontType::GeoSans);
            }

            // ==================== 3B. WEAPON SELECTION STRIP (SLOTS 1..9) ====================
            if (weaponSelectorTimer > 0.0f && !slotWeaponNames.empty()) {
                float totalW = std::min((float)screenW - 80.0f, (float)slotWeaponNames.size() * 115.0f);
                float startStripX = ((float)screenW - totalW) * 0.5f;
                float stripY = 52.0f;
                float slotW = totalW / (float)slotWeaponNames.size();
                float slotH = 46.0f;

                float alpha = std::clamp(weaponSelectorTimer, 0.0f, 1.0f);
                Vec3 bgBox = Vec3(0.08f, 0.10f, 0.14f) * alpha;

                for (size_t s = 0; s < slotWeaponNames.size(); ++s) {
                    float sx = startStripX + s * slotW;
                    bool isActive = ((int)s + 1 == activeWeaponSlot);
                    bool isUnl = (s < slotUnlocked.size()) ? slotUnlocked[s] : false;

                    Vec3 sBg = isActive ? Vec3(0.20f, 0.42f, 0.65f) : (isUnl ? bgBox : Vec3(0.04f, 0.05f, 0.07f));
                    Vec3 sBorder = isActive ? textYellow : (isUnl ? Vec3(0.35f, 0.45f, 0.55f) : Vec3(0.18f, 0.20f, 0.22f));

                    Renderer::drawRect(sx, stripY, slotW - 4.0f, slotH, sBg);
                    Renderer::drawRect(sx, stripY, slotW - 4.0f, 1.0f, sBorder);
                    Renderer::drawRect(sx, stripY + slotH - 1.0f, slotW - 4.0f, 1.0f, sBorder);
                    Renderer::drawRect(sx, stripY, 1.0f, slotH, sBorder);
                    Renderer::drawRect(sx + slotW - 5.0f, stripY, 1.0f, slotH, sBorder);

                    std::string numTag = "[" + std::to_string(s + 1) + "]";
                    Vec3 numColor = isActive ? textYellow : (isUnl ? Vec3(0.85f, 0.85f, 0.85f) : Vec3(0.4f, 0.4f, 0.4f));
                    LabFont::drawText(sx + 6.0f, stripY + 6.0f, numTag, 1.4f, numColor, LabFontType::GeoSans);

                    std::string wTitle = isUnl ? slotWeaponNames[s] : "LOCKED";
                    Vec3 titleColor = isActive ? Vec3(1, 1, 1) : (isUnl ? Vec3(0.8f, 0.9f, 1.0f) : Vec3(0.35f, 0.38f, 0.42f));
                    LabFont::drawText(sx + 6.0f, stripY + 24.0f, wTitle, 1.4f, titleColor, LabFontType::GeoSans);
                }
            }

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

        void renderDeathScreen(int screenW, int screenH, float respawnTimer) {
            Renderer::beginUI(screenW, screenH);

            // Red vignette wash over full screen
            Renderer::drawRect(0, 0, (float)screenW, (float)screenH, Vec3(0.35f, 0.05f, 0.05f));

            // Central alert card
            float cardW = 560.0f;
            float cardH = 180.0f;
            float cardX = ((float)screenW - cardW) * 0.5f;
            float cardY = ((float)screenH - cardH) * 0.5f;

            drawCard(cardX, cardY, cardW, cardH, Vec3(0.10f, 0.08f, 0.08f), Vec3(0.9f, 0.2f, 0.2f));

            LabFont::drawText(cardX + 90.0f, cardY + 28.0f, "YOU WERE ELIMINATED", 3.2f, Vec3(1.0f, 0.2f, 0.2f), LabFontType::GeoSans);

            int sec = static_cast<int>(std::ceil(respawnTimer));
            std::string cdText = "RESPAWNING IN " + std::to_string(std::max(0, sec)) + "s ...";
            LabFont::drawText(cardX + 160.0f, cardY + 80.0f, cdText, 2.2f, textYellow, LabFontType::GeoSans);

            std::string promptText = "PRESS [SPACE] OR [ENTER] TO RESPAWN IMMEDIATELY";
            LabFont::drawText(cardX + 45.0f, cardY + 128.0f, promptText, 1.7f, Vec3(0.8f, 0.85f, 0.9f), LabFontType::GeoSans);

            Renderer::endUI();
        }

        void renderScoreboard(int screenW, int screenH, const std::vector<struct ScoreboardEntry>& entries,
                              const std::string& matchTitle, const std::string& modeName, int fragLimit);
    };

    struct ScoreboardEntry {
        std::string name;
        int kills = 0;
        int deaths = 0;
        std::string ping = "5ms";
        bool isBot = false;
        bool isLocalPlayer = false;
        bool isAlive = true;
        std::string status = "ALIVE";
        std::string team = "Alpha";
    };

    inline void LabHUD::renderScoreboard(int screenW, int screenH, const std::vector<ScoreboardEntry>& entries,
                                         const std::string& matchTitle, const std::string& modeName, int fragLimit) {
        Renderer::beginUI(screenW, screenH);

        // Dark semi-transparent background overlay
        Renderer::drawRect(0, 0, (float)screenW, (float)screenH, Vec3(0.02f, 0.03f, 0.05f));

        float sbW = 780.0f;
        float sbH = 460.0f;
        float sbX = ((float)screenW - sbW) * 0.5f;
        float sbY = ((float)screenH - sbH) * 0.5f;

        // Scoreboard outer card
        drawCard(sbX, sbY, sbW, sbH, Vec3(0.07f, 0.09f, 0.12f), Vec3(0.28f, 0.38f, 0.50f));

        // Top Header banner
        float headH = 50.0f;
        Renderer::drawRect(sbX, sbY, sbW, headH, Vec3(0.12f, 0.16f, 0.22f));
        Renderer::drawRect(sbX, sbY + headH - 1.0f, sbW, 1.0f, Vec3(0.35f, 0.50f, 0.65f));

        LabFont::drawText(sbX + 20.0f, sbY + 14.0f, "FROZEN-LIFE :: " + matchTitle, 2.4f, textYellow, LabFontType::GeoSans);

        std::string matchMeta = modeName + " | FRAG LIMIT: " + std::to_string(fragLimit);
        LabFont::drawText(sbX + sbW - 270.0f, sbY + 18.0f, matchMeta, 1.8f, Vec3(0.3f, 0.85f, 1.0f), LabFontType::GeoSans);

        // Table Column Headers
        float colY = sbY + headH + 8.0f;
        float nameColX = sbX + 24.0f;
        float teamColX = sbX + 260.0f;
        float killsColX = sbX + 370.0f;
        float deathsColX = sbX + 460.0f;
        float statusColX = sbX + 550.0f;
        float pingColX = sbX + 690.0f;

        Renderer::drawRect(sbX + 10.0f, colY - 2.0f, sbW - 20.0f, 24.0f, Vec3(0.09f, 0.12f, 0.16f));
        LabFont::drawText(nameColX, colY + 3.0f, "PLAYER / SYNTH", 1.6f, textYellow * 0.9f, LabFontType::GeoSans);
        LabFont::drawText(teamColX, colY + 3.0f, "TEAM", 1.6f, textYellow * 0.9f, LabFontType::GeoSans);
        LabFont::drawText(killsColX, colY + 3.0f, "KILLS", 1.6f, textYellow * 0.9f, LabFontType::GeoSans);
        LabFont::drawText(deathsColX, colY + 3.0f, "DEATHS", 1.6f, textYellow * 0.9f, LabFontType::GeoSans);
        LabFont::drawText(statusColX, colY + 3.0f, "STATUS", 1.6f, textYellow * 0.9f, LabFontType::GeoSans);
        LabFont::drawText(pingColX, colY + 3.0f, "PING", 1.6f, textYellow * 0.9f, LabFontType::GeoSans);

        // Row entries
        float rowY = colY + 30.0f;
        float rowH = 32.0f;

        for (size_t i = 0; i < entries.size() && i < 10; ++i) {
            const auto& e = entries[i];
            Vec3 rowBg = (i % 2 == 0) ? Vec3(0.10f, 0.12f, 0.17f) : Vec3(0.08f, 0.10f, 0.14f);
            Vec3 textColor = e.isLocalPlayer ? textYellow : Vec3(0.92f, 0.92f, 0.92f);

            if (e.isLocalPlayer) {
                rowBg = Vec3(0.18f, 0.16f, 0.10f); // Amber tint for local player
                Renderer::drawRect(sbX + 10.0f, rowY, sbW - 20.0f, rowH, rowBg);
                Renderer::drawRect(sbX + 10.0f, rowY, sbW - 20.0f, 1.0f, textYellow * 0.8f);
                Renderer::drawRect(sbX + 10.0f, rowY + rowH - 1.0f, sbW - 20.0f, 1.0f, textYellow * 0.8f);
            } else {
                Renderer::drawRect(sbX + 10.0f, rowY, sbW - 20.0f, rowH, rowBg);
            }

            // Name
            LabFont::drawText(nameColX, rowY + 7.0f, e.name, 1.8f, textColor, LabFontType::GeoSans);

            // Team
            LabFont::drawText(teamColX, rowY + 7.0f, e.team, 1.7f, Vec3(0.5f, 0.7f, 0.9f), LabFontType::GeoSans);

            // Kills
            LabFont::drawText(killsColX + 12.0f, rowY + 7.0f, std::to_string(e.kills), 1.8f, Vec3(0.3f, 0.95f, 0.4f), LabFontType::GeoSans);

            // Deaths
            LabFont::drawText(deathsColX + 12.0f, rowY + 7.0f, std::to_string(e.deaths), 1.8f, Vec3(0.95f, 0.4f, 0.4f), LabFontType::GeoSans);

            // Status
            Vec3 statusColor = e.isAlive ? Vec3(0.3f, 0.9f, 0.4f) : Vec3(0.95f, 0.3f, 0.3f);
            LabFont::drawText(statusColX, rowY + 7.0f, e.status, 1.6f, statusColor, LabFontType::GeoSans);

            // Ping
            LabFont::drawText(pingColX, rowY + 7.0f, e.ping, 1.7f, Vec3(0.6f, 0.65f, 0.75f), LabFontType::GeoSans);

            rowY += rowH + 4.0f;
        }

        // Bottom controls hint
        float footerY = sbY + sbH - 32.0f;
        Renderer::drawRect(sbX, footerY, sbW, 32.0f, Vec3(0.05f, 0.07f, 0.10f));
        Renderer::drawRect(sbX, footerY, sbW, 1.0f, Vec3(0.2f, 0.28f, 0.38f));

        std::string hintStr = "[TAB] Release to close scoreboard  |  [Y / Enter] In-Game Chat  |  [M / ESC] Menu";
        LabFont::drawText(sbX + 24.0f, footerY + 8.0f, hintStr, 1.65f, Vec3(0.65f, 0.75f, 0.85f), LabFontType::GeoSans);

        Renderer::endUI();
    }

}
