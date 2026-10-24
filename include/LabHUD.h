#pragma once
#include "LabMath.h"
#include "LabRenderer.h"
#include "LabFont.h"
#include <algorithm>
#include <string>
#include <vector>

namespace Lab {

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

    // Authentic Source Engine / Half-Life 2 Translucent HUD
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

        bool showCrosshair = true;
        Vec3 crosshairColor{ 1.0f, 0.74f, 0.14f };

        // Source Engine / HL2 HEV Amber Palette
        Vec3 textYellow{ 1.0f, 0.74f, 0.14f };        // Warm HL2 Amber (#FFBC24)
        Vec3 textDimYellow{ 0.55f, 0.40f, 0.08f };    // Muted amber label
        Vec3 tileBg{ 0.04f, 0.05f, 0.07f };           // Dark smoked glass
        Vec3 tileBorder{ 1.0f, 0.72f, 0.14f };        // Subtle amber rim
        float cardAlpha = 0.56f;
        float cardRadius = 8.0f;

        void triggerHitmarker(bool headshot);
        void triggerDamageFlash();
        void showCombatMessage(const std::string& msg, float duration = 2.5f);
        void update(float dt);

        static void drawCard(float x, float y, float w, float h, const Vec3& bg, const Vec3& border,
                             float alpha = 0.56f, float radius = 8.0f, float borderAlpha = 0.28f);
        static void drawHealthIcon(float x, float y, float size, const Vec3& color, float alpha = 1.0f);
        static void drawSuitIcon(float x, float y, float w, float h, const Vec3& color, float alpha = 1.0f);
        static void drawClockIcon(float x, float y, float r, const Vec3& color, float alpha = 1.0f);
        static void drawAmmoIcon(float x, float y, const Vec3& color, float alpha = 1.0f);

        void render(int screenW, int screenH);
        void renderDeathScreen(int screenW, int screenH, float respawnTimer);
        void renderScoreboard(int screenW, int screenH, const std::vector<ScoreboardEntry>& entries,
                              const std::string& matchTitle, const std::string& modeName, int fragLimit);
    };

} // namespace Lab
