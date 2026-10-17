#include "LabHUD.h"
#include <cmath>
#include <cstdio>

namespace Lab {

    void LabHUD::triggerHitmarker(bool headshot) {
        hitmarkerTimer = 0.15f;
        hitmarkerHeadshot = headshot;
    }

    void LabHUD::triggerDamageFlash() {
        damageFlashTimer = 0.25f;
    }

    void LabHUD::showCombatMessage(const std::string& msg, float duration) {
        combatMessage = msg;
        combatMessageTimer = duration;
    }

    void LabHUD::update(float dt) {
        if (hitmarkerTimer > 0.0f) hitmarkerTimer = (std::max)(0.0f, hitmarkerTimer - dt);
        if (damageFlashTimer > 0.0f) damageFlashTimer = (std::max)(0.0f, damageFlashTimer - dt);
        if (combatMessageTimer > 0.0f) combatMessageTimer = (std::max)(0.0f, combatMessageTimer - dt);
        if (weaponSelectorTimer > 0.0f) weaponSelectorTimer = (std::max)(0.0f, weaponSelectorTimer - dt);
    }

    void LabHUD::drawCard(float x, float y, float w, float h, const Vec3& bg, const Vec3& border,
                          float alpha, float radius, float borderAlpha) {
        // Minimalist borderless dark translucent glass pill (no heavy borders)
        (void)border; (void)borderAlpha;
        Renderer::drawRoundedRect(x, y, w, h, bg, alpha, radius, 0.0f);
    }

    void LabHUD::drawHealthIcon(float x, float y, float size, const Vec3& color, float alpha) {
        float arm = std::floor(size * 0.34f);
        float offset = std::floor((size - arm) * 0.5f);
        // Subtle dark drop shadow
        Renderer::drawRect(x + offset + 1.0f, y + 1.0f, arm, size, Vec3(0, 0, 0), alpha * 0.4f);
        Renderer::drawRect(x + 1.0f, y + offset + 1.0f, size, arm, Vec3(0, 0, 0), alpha * 0.4f);
        // Crisp minimal cross
        Renderer::drawRect(x + offset, y, arm, size, color, alpha);
        Renderer::drawRect(x, y + offset, size, arm, color, alpha);
    }

    void LabHUD::drawSuitIcon(float x, float y, float w, float h, const Vec3& color, float alpha) {
        // Minimal HEV shield mark
        Renderer::drawRect(x, y, w, h * 0.65f, color, alpha);
        Renderer::drawRect(x + w * 0.2f, y + h * 0.65f, w * 0.6f, h * 0.35f, color, alpha);
        Renderer::drawRect(x + 1.5f, y + 1.5f, w - 3.0f, h * 0.45f, Vec3(0.04f, 0.05f, 0.07f), alpha * 0.85f);
    }

    void LabHUD::drawClockIcon(float x, float y, float r, const Vec3& color, float alpha) {
        float d = r * 2.0f;
        Renderer::drawRoundedRect(x, y, d, d, Vec3(0.04f, 0.05f, 0.07f), alpha * 0.35f, r, 0.0f);
        // Clock hands
        Renderer::drawRect(x + r - 0.5f, y + 2.5f, 1.0f, r - 2.0f, color, alpha);
        Renderer::drawRect(x + r - 0.5f, y + r - 0.5f, r - 2.5f, 1.0f, color, alpha);
    }

    void LabHUD::drawAmmoIcon(float x, float y, const Vec3& color, float alpha) {
        (void)x; (void)y; (void)color; (void)alpha;
        // Clean minimalist HUD omits redundant cartoon bullets
    }

    static void drawHUDTextWithGlow(float x, float y, const std::string& text, float scale,
                                    const Vec3& color, float alpha = 1.0f) {
        // Clean single-pass text with soft 1px drop shadow (no blurry double-offset halo)
        LabFont::drawText(x + 1.0f, y + 1.0f, text, scale, Vec3(0.0f, 0.0f, 0.0f), LabFontType::GeoSans, alpha * 0.50f);
        LabFont::drawText(x, y, text, scale, color, LabFontType::GeoSans, alpha);
    }

    void LabHUD::render(int screenW, int screenH) {
        Renderer::beginUI(screenW, screenH);

        // ==================== DAMAGE FLASH VIGNETTE ====================
        if (damageFlashTimer > 0.0f) {
            float flashA = std::clamp(damageFlashTimer / 0.25f, 0.0f, 1.0f) * 0.35f;
            Vec3 flashCol(0.85f, 0.06f, 0.05f);
            float edge = 24.0f;
            Renderer::drawRect(0.0f, 0.0f, (float)screenW, edge, flashCol, flashA);
            Renderer::drawRect(0.0f, (float)screenH - edge, (float)screenW, edge, flashCol, flashA);
            Renderer::drawRect(0.0f, edge, edge, (float)screenH - edge * 2.0f, flashCol, flashA);
            Renderer::drawRect((float)screenW - edge, edge, edge, (float)screenH - edge * 2.0f, flashCol, flashA);
        }

        // ==================== 1. TOP-RIGHT MINIMAL TELEMETRY & MATCH BADGE ====================
        std::string headerStr = mapName + "  [" + gameModeName + "]";
        std::string fragStr = "KILLS: " + std::to_string(frags);

        int mins = (std::max)(0, timerSecs) / 60;
        int secs = (std::max)(0, timerSecs) % 60;
        char timeBuf[16];
        std::snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d", mins, secs);
        std::string timeStr(timeBuf);

        float headerW = LabFont::getTextWidth(headerStr, 1.35f, LabFontType::GeoSans);
        float fragW   = LabFont::getTextWidth(fragStr, 1.35f, LabFontType::GeoSans);
        float timeW   = LabFont::getTextWidth(timeStr, 1.45f, LabFontType::GeoSans);

        float padX = 14.0f;
        float sepGap = 18.0f;
        float topW = padX + headerW + sepGap + fragW + sepGap + timeW + padX;
        topW = (std::max)(topW, 280.0f);
        float topH = 26.0f;
        float topX = (float)screenW - topW - 24.0f;
        float topY = 18.0f;

        drawCard(topX, topY, topW, topH, tileBg, tileBorder, 0.28f, 5.0f, 0.0f);

        float curX = topX + padX;
        drawHUDTextWithGlow(curX, topY + 7.0f, headerStr, 1.35f, textYellow, 0.85f);
        curX += headerW + sepGap * 0.5f;

        // Subtle hairline divider 1
        Renderer::drawRect(curX, topY + 5.0f, 1.0f, topH - 10.0f, textYellow, 0.20f);
        curX += sepGap * 0.5f;

        drawHUDTextWithGlow(curX, topY + 7.0f, fragStr, 1.35f, Vec3(0.55f, 0.90f, 1.0f), 0.90f);
        curX += fragW + sepGap * 0.5f;

        // Subtle hairline divider 2
        Renderer::drawRect(curX, topY + 5.0f, 1.0f, topH - 10.0f, textYellow, 0.20f);
        curX += sepGap * 0.5f;

        drawHUDTextWithGlow(curX, topY + 6.0f, timeStr, 1.45f, textYellow, 0.92f);

        // Combat notification banner (Upper center) - sleek and narrow
        if (combatMessageTimer > 0.0f && !combatMessage.empty()) {
            float fade = std::clamp(combatMessageTimer / 0.4f, 0.0f, 1.0f);
            float msgTextW = LabFont::getTextWidth(combatMessage, 1.45f, LabFontType::GeoSans);
            float msgW = msgTextW + 36.0f;
            float msgH = 26.0f;
            float msgX = ((float)screenW - msgW) * 0.5f;
            float msgY = 56.0f;
            drawCard(msgX, msgY, msgW, msgH, tileBg, textYellow, 0.35f * fade, 5.0f, 0.0f);
            drawHUDTextWithGlow(msgX + (msgW - msgTextW) * 0.5f, msgY + 7.0f, combatMessage, 1.45f, textYellow, fade);
        }

        // ==================== 2. BOTTOM-LEFT MINIMAL HEALTH & SUIT PILLS ====================
        float startX = 24.0f;
        float cardH = 54.0f;
        float cardY = (float)screenH - cardH - 24.0f;
        float gapX = 10.0f;

        // --- HEALTH PILL ---
        float hpCardW = 152.0f;
        float hpX = startX;
        bool lowHp = (health < 30.0f);
        Vec3 hpAccent = lowHp ? Vec3(1.0f, 0.22f, 0.18f) : textYellow;
        drawCard(hpX, cardY, hpCardW, cardH, tileBg, hpAccent, lowHp ? 0.45f : 0.32f, 5.0f, 0.0f);

        drawHealthIcon(hpX + 14.0f, cardY + 14.0f, 11.0f, hpAccent, 0.90f);
        drawHUDTextWithGlow(hpX + 28.0f, cardY + 31.0f, "HEALTH", 1.15f, hpAccent, 0.75f);

        int dispHp = (std::max)(0, (int)std::ceil(health));
        std::string hpStr = std::to_string(dispHp);
        float hpNumW = LabFont::getTextWidth(hpStr, 3.4f, LabFontType::GeoSans);
        drawHUDTextWithGlow(hpX + hpCardW - hpNumW - 14.0f, cardY + 8.0f, hpStr, 3.4f, hpAccent, 0.98f);

        // Minimal continuous hairline gauge line at bottom of pill
        float hpRatio = std::clamp(health / (std::max)(1.0f, maxHealth), 0.0f, 1.0f);
        Renderer::drawRect(hpX + 8.0f, cardY + cardH - 3.0f, (hpCardW - 16.0f) * hpRatio, 2.0f, hpAccent, 0.85f);
        Renderer::drawRect(hpX + 8.0f + (hpCardW - 16.0f) * hpRatio, cardY + cardH - 3.0f, (hpCardW - 16.0f) * (1.0f - hpRatio), 2.0f, hpAccent, 0.15f);

        // --- SUIT PILL (HEV Armor) ---
        float suitCardW = 152.0f;
        float suitX = hpX + hpCardW + gapX;
        float suitAlpha = (suitArmor > 0.5f) ? 0.32f : 0.16f;
        float suitTextAlpha = (suitArmor > 0.5f) ? 0.95f : 0.40f;
        drawCard(suitX, cardY, suitCardW, cardH, tileBg, tileBorder, suitAlpha, 5.0f, 0.0f);

        drawSuitIcon(suitX + 14.0f, cardY + 13.0f, 11.0f, 13.0f, textYellow, suitTextAlpha);
        drawHUDTextWithGlow(suitX + 28.0f, cardY + 31.0f, "SUIT", 1.15f, textYellow, suitTextAlpha * 0.75f);

        int dispSuit = (std::max)(0, (int)std::ceil(suitArmor));
        std::string suitStr = std::to_string(dispSuit);
        float suitNumW = LabFont::getTextWidth(suitStr, 3.4f, LabFontType::GeoSans);
        drawHUDTextWithGlow(suitX + suitCardW - suitNumW - 14.0f, cardY + 8.0f, suitStr, 3.4f, textYellow, suitTextAlpha);

        float suitRatio = std::clamp(suitArmor / (std::max)(1.0f, maxSuitArmor), 0.0f, 1.0f);
        Renderer::drawRect(suitX + 8.0f, cardY + cardH - 3.0f, (suitCardW - 16.0f) * suitRatio, 2.0f, textYellow, suitTextAlpha * 0.85f);
        Renderer::drawRect(suitX + 8.0f + (suitCardW - 16.0f) * suitRatio, cardY + cardH - 3.0f, (suitCardW - 16.0f) * (1.0f - suitRatio), 2.0f, textYellow, 0.12f);

        // ==================== 3. BOTTOM-RIGHT MINIMAL AMMO PILL ====================
        float ammoW = 162.0f;
        float ammoX = (float)screenW - ammoW - 24.0f;
        drawCard(ammoX, cardY, ammoW, cardH, tileBg, tileBorder, 0.32f, 5.0f, 0.0f);

        drawHUDTextWithGlow(ammoX + 14.0f, cardY + 31.0f, "AMMO", 1.15f, textYellow, 0.75f);

        if (isMeleeWeapon) {
            std::string infStr = "INF";
            float infW = LabFont::getTextWidth(infStr, 2.8f, LabFontType::GeoSans);
            drawHUDTextWithGlow(ammoX + ammoW - infW - 16.0f, cardY + 12.0f, infStr, 2.8f, textYellow, 0.95f);
        } else {
            std::string resStr = std::to_string(ammoReserve);
            float resW = LabFont::getTextWidth(resStr, 1.85f, LabFontType::GeoSans);
            float resX = ammoX + ammoW - resW - 14.0f;
            drawHUDTextWithGlow(resX, cardY + 23.0f, resStr, 1.85f, textYellow, 0.70f);

            // Subtle hairline vertical divider
            float divX = resX - 8.0f;
            Renderer::drawRect(divX, cardY + 14.0f, 1.0f, cardH - 28.0f, textYellow, 0.22f);

            std::string clipStr = std::to_string(ammoClip);
            float clipW = LabFont::getTextWidth(clipStr, 3.4f, LabFontType::GeoSans);
            Vec3 clipCol = (ammoClip == 0) ? Vec3(1.0f, 0.25f, 0.2f) : textYellow;
            drawHUDTextWithGlow(divX - clipW - 8.0f, cardY + 8.0f, clipStr, 3.4f, clipCol, 0.98f);
        }

        // ==================== 3B. TRANSLUCENT WEAPON SELECTION STRIP ====================
        if (weaponSelectorTimer > 0.0f && !slotWeaponNames.empty()) {
            float fade = std::clamp(weaponSelectorTimer / 0.6f, 0.0f, 1.0f);
            float totalW = (std::min)((float)screenW - 80.0f, (float)slotWeaponNames.size() * 96.0f);
            float startStripX = ((float)screenW - totalW) * 0.5f;
            float stripY = 56.0f;
            float slotW = totalW / (float)slotWeaponNames.size();
            float slotH = 38.0f;

            for (size_t s = 0; s < slotWeaponNames.size(); ++s) {
                float sx = startStripX + s * slotW;
                bool isActive = ((int)s + 1 == activeWeaponSlot);
                bool isUnl = (s < slotUnlocked.size()) ? slotUnlocked[s] : false;

                Vec3 sBg = isActive ? Vec3(0.12f, 0.10f, 0.04f) : tileBg;
                float boxAlpha = (isActive ? 0.65f : 0.35f) * fade;

                Renderer::drawRoundedRect(sx, stripY, slotW - 4.0f, slotH, sBg, boxAlpha, 4.0f, 0.0f);

                std::string numTag = std::to_string(s + 1);
                Vec3 numColor = isActive ? textYellow : (isUnl ? Vec3(0.85f, 0.72f, 0.35f) : Vec3(0.40f, 0.40f, 0.40f));
                LabFont::drawText(sx + 6.0f, stripY + 5.0f, numTag, 1.35f, numColor, LabFontType::GeoSans, fade * 0.95f);

                std::string wTitle = isUnl ? slotWeaponNames[s] : "LOCKED";
                Vec3 titleColor = isActive ? textYellow : (isUnl ? Vec3(0.88f, 0.85f, 0.78f) : Vec3(0.40f, 0.40f, 0.42f));
                LabFont::drawText(sx + 6.0f, stripY + 20.0f, wTitle, 1.15f, titleColor, LabFontType::GeoSans, fade * (isUnl ? 0.90f : 0.40f));
            }
        }

        // ==================== 4. MINIMAL DYNAMIC HL2 CROSSHAIR & HITMARKER ====================
        float cx = std::floor((float)screenW * 0.5f);
        float cy = std::floor((float)screenH * 0.5f);

        if (showCrosshair) {
            float crossGap = 3.5f;
            float crossLen = 5.0f;
            float crossThick = 1.5f;
            Vec3 cCol = crosshairColor;
            Vec3 shadowCol(0.0f, 0.0f, 0.0f);

            // Subtle dark contrast backing
            Renderer::drawRect(cx - crossGap - crossLen - 0.5f, cy - crossThick * 0.5f - 0.5f, crossLen + 1.0f, crossThick + 1.0f, shadowCol, 0.40f);
            Renderer::drawRect(cx + crossGap - 0.5f, cy - crossThick * 0.5f - 0.5f, crossLen + 1.0f, crossThick + 1.0f, shadowCol, 0.40f);
            Renderer::drawRect(cx - crossThick * 0.5f - 0.5f, cy - crossGap - crossLen - 0.5f, crossThick + 1.0f, crossLen + 1.0f, shadowCol, 0.40f);
            Renderer::drawRect(cx - crossThick * 0.5f - 0.5f, cy + crossGap - 0.5f, crossThick + 1.0f, crossLen + 1.0f, shadowCol, 0.40f);

            // Razor-sharp amber ticks + center dot
            Renderer::drawRect(cx - crossGap - crossLen, cy - crossThick * 0.5f, crossLen, crossThick, cCol, 0.85f);
            Renderer::drawRect(cx + crossGap, cy - crossThick * 0.5f, crossLen, crossThick, cCol, 0.85f);
            Renderer::drawRect(cx - crossThick * 0.5f, cy - crossGap - crossLen, crossThick, crossLen, cCol, 0.85f);
            Renderer::drawRect(cx - crossThick * 0.5f, cy + crossGap, crossThick, crossLen, cCol, 0.85f);
            Renderer::drawRect(cx - 0.75f, cy - 0.75f, 1.5f, 1.5f, cCol, 0.90f);
        }

        if (hitmarkerTimer > 0.0f) {
            float hmAlpha = std::clamp(hitmarkerTimer / 0.15f, 0.0f, 1.0f);
            Vec3 hmCol = hitmarkerHeadshot ? Vec3(1.0f, 0.20f, 0.15f) : Vec3(1.0f, 0.92f, 0.30f);
            float hThick = 1.5f;

            Renderer::drawRect(cx - 8.0f, cy - 8.0f, 4.0f, hThick, hmCol, hmAlpha);
            Renderer::drawRect(cx - 8.0f, cy - 8.0f, hThick, 4.0f, hmCol, hmAlpha);

            Renderer::drawRect(cx + 4.0f, cy - 8.0f, 4.0f, hThick, hmCol, hmAlpha);
            Renderer::drawRect(cx + 6.5f, cy - 8.0f, hThick, 4.0f, hmCol, hmAlpha);

            Renderer::drawRect(cx - 8.0f, cy + 6.5f, 4.0f, hThick, hmCol, hmAlpha);
            Renderer::drawRect(cx - 8.0f, cy + 4.0f, hThick, 4.0f, hmCol, hmAlpha);

            Renderer::drawRect(cx + 4.0f, cy + 6.5f, 4.0f, hThick, hmCol, hmAlpha);
            Renderer::drawRect(cx + 6.5f, cy + 4.0f, hThick, 4.0f, hmCol, hmAlpha);
        }

        Renderer::endUI();
    }

    void LabHUD::renderDeathScreen(int screenW, int screenH, float respawnTimer) {
        Renderer::beginUI(screenW, screenH);

        // Translucent crimson wash over the 3D scene
        Renderer::drawRect(0.0f, 0.0f, (float)screenW, (float)screenH, Vec3(0.28f, 0.03f, 0.03f), 0.58f);

        float cardW = 560.0f;
        float cardH = 180.0f;
        float cardX = ((float)screenW - cardW) * 0.5f;
        float cardY = ((float)screenH - cardH) * 0.5f;

        drawCard(cardX, cardY, cardW, cardH, Vec3(0.06f, 0.04f, 0.04f), Vec3(0.95f, 0.22f, 0.18f), 0.78f, 10.0f, 0.65f);

        std::string title = "SUBJECT TERMINATED";
        float titleW = LabFont::getTextWidth(title, 3.0f, LabFontType::GeoSans);
        drawHUDTextWithGlow(cardX + (cardW - titleW) * 0.5f, cardY + 28.0f, title, 3.0f, Vec3(1.0f, 0.25f, 0.22f), 0.98f);

        int sec = static_cast<int>(std::ceil(respawnTimer));
        std::string cdText = "REDEPLOYMENT IN " + std::to_string((std::max)(0, sec)) + "s ...";
        float cdW = LabFont::getTextWidth(cdText, 2.1f, LabFontType::GeoSans);
        drawHUDTextWithGlow(cardX + (cardW - cdW) * 0.5f, cardY + 82.0f, cdText, 2.1f, textYellow, 0.95f);

        std::string promptText = "PRESS [SPACE] OR [ENTER] FOR IMMEDIATE DEPLOYMENT";
        float prW = LabFont::getTextWidth(promptText, 1.65f, LabFontType::GeoSans);
        drawHUDTextWithGlow(cardX + (cardW - prW) * 0.5f, cardY + 130.0f, promptText, 1.65f, Vec3(0.85f, 0.88f, 0.92f), 0.85f);

        Renderer::endUI();
    }

    void LabHUD::renderScoreboard(int screenW, int screenH, const std::vector<ScoreboardEntry>& entries,
                                  const std::string& matchTitle, const std::string& modeName, int fragLimit) {
        Renderer::beginUI(screenW, screenH);

        // Translucent dark backdrop so the 3D world remains visible behind the scoreboard
        Renderer::drawRect(0.0f, 0.0f, (float)screenW, (float)screenH, Vec3(0.02f, 0.03f, 0.04f), 0.55f);

        float sbW = 780.0f;
        float sbH = 460.0f;
        float sbX = ((float)screenW - sbW) * 0.5f;
        float sbY = ((float)screenH - sbH) * 0.5f;

        drawCard(sbX, sbY, sbW, sbH, tileBg, tileBorder, 0.76f, 10.0f, 0.42f);

        float headH = 50.0f;
        Renderer::drawRoundedRect(sbX + 2.0f, sbY + 2.0f, sbW - 4.0f, headH, Vec3(0.08f, 0.09f, 0.12f), 0.65f, 8.0f, 0.0f, tileBorder, 0.0f);
        Renderer::drawRect(sbX + 12.0f, sbY + headH, sbW - 24.0f, 1.0f, tileBorder, 0.35f);

        drawHUDTextWithGlow(sbX + 22.0f, sbY + 15.0f, "FROZEN-LIFE  //  " + matchTitle, 2.2f, textYellow, 0.98f);

        std::string matchMeta = modeName + "  |  FRAG LIMIT: " + std::to_string(fragLimit);
        float metaW = LabFont::getTextWidth(matchMeta, 1.7f, LabFontType::GeoSans);
        drawHUDTextWithGlow(sbX + sbW - metaW - 22.0f, sbY + 18.0f, matchMeta, 1.7f, Vec3(0.45f, 0.88f, 1.0f), 0.92f);

        float colY = sbY + headH + 10.0f;
        float nameColX = sbX + 24.0f;
        float teamColX = sbX + 260.0f;
        float killsColX = sbX + 370.0f;
        float deathsColX = sbX + 460.0f;
        float statusColX = sbX + 550.0f;
        float pingColX = sbX + 690.0f;

        Renderer::drawRect(sbX + 12.0f, colY - 2.0f, sbW - 24.0f, 24.0f, Vec3(0.08f, 0.10f, 0.14f), 0.55f);
        LabFont::drawText(nameColX, colY + 4.0f, "OPERATIVE / SYNTH", 1.55f, textYellow, LabFontType::GeoSans, 0.85f);
        LabFont::drawText(teamColX, colY + 4.0f, "FACTION", 1.55f, textYellow, LabFontType::GeoSans, 0.85f);
        LabFont::drawText(killsColX, colY + 4.0f, "KILLS", 1.55f, textYellow, LabFontType::GeoSans, 0.85f);
        LabFont::drawText(deathsColX, colY + 4.0f, "DEATHS", 1.55f, textYellow, LabFontType::GeoSans, 0.85f);
        LabFont::drawText(statusColX, colY + 4.0f, "STATUS", 1.55f, textYellow, LabFontType::GeoSans, 0.85f);
        LabFont::drawText(pingColX, colY + 4.0f, "LATENCY", 1.55f, textYellow, LabFontType::GeoSans, 0.85f);

        float rowY = colY + 30.0f;
        float rowH = 32.0f;

        for (size_t i = 0; i < entries.size() && i < 10; ++i) {
            const auto& e = entries[i];
            Vec3 rowBg = (i % 2 == 0) ? Vec3(0.09f, 0.11f, 0.15f) : Vec3(0.06f, 0.08f, 0.11f);
            Vec3 textColor = e.isLocalPlayer ? textYellow : Vec3(0.92f, 0.93f, 0.95f);

            if (e.isLocalPlayer) {
                Renderer::drawRoundedRect(sbX + 12.0f, rowY, sbW - 24.0f, rowH, Vec3(0.18f, 0.14f, 0.05f), 0.65f, 5.0f, 1.0f, textYellow, 0.65f);
            } else {
                Renderer::drawRoundedRect(sbX + 12.0f, rowY, sbW - 24.0f, rowH, rowBg, 0.45f, 4.0f, 0.0f, tileBorder, 0.0f);
            }

            LabFont::drawText(nameColX, rowY + 8.0f, e.name, 1.75f, textColor, LabFontType::GeoSans, 0.96f);
            LabFont::drawText(teamColX, rowY + 8.0f, e.team, 1.65f, Vec3(0.55f, 0.78f, 0.95f), LabFontType::GeoSans, 0.90f);
            LabFont::drawText(killsColX + 12.0f, rowY + 8.0f, std::to_string(e.kills), 1.75f, Vec3(0.35f, 0.95f, 0.45f), LabFontType::GeoSans, 0.95f);
            LabFont::drawText(deathsColX + 12.0f, rowY + 8.0f, std::to_string(e.deaths), 1.75f, Vec3(0.95f, 0.42f, 0.42f), LabFontType::GeoSans, 0.95f);

            Vec3 statusColor = e.isAlive ? Vec3(0.35f, 0.92f, 0.45f) : Vec3(0.95f, 0.32f, 0.32f);
            LabFont::drawText(statusColX, rowY + 8.0f, e.status, 1.55f, statusColor, LabFontType::GeoSans, 0.92f);
            LabFont::drawText(pingColX, rowY + 8.0f, e.ping, 1.65f, Vec3(0.68f, 0.74f, 0.82f), LabFontType::GeoSans, 0.88f);

            rowY += rowH + 4.0f;
        }

        float footerY = sbY + sbH - 34.0f;
        Renderer::drawRect(sbX + 12.0f, footerY, sbW - 24.0f, 1.0f, tileBorder, 0.25f);
        std::string hintStr = "[TAB] Close Tactical Overview   |   [Y / ENTER] Comms Channel   |   [M / ESC] Command Menu";
        LabFont::drawText(sbX + 24.0f, footerY + 10.0f, hintStr, 1.55f, Vec3(0.75f, 0.80f, 0.86f), LabFontType::GeoSans, 0.82f);

        Renderer::endUI();
    }

} // namespace Lab
