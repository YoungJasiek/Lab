#pragma once
#include <string>
#include <vector>
#include <memory>
#include "LabMath.h"
#include "LabCamera.h"
#include "LabMap.h"
#include "LabAudio.h"

namespace Lab {

    enum class InteractiveType {
        Terminal,       // Wall-mounted CRT terminal with diagnostic screen & access override
        WallSwitch,     // Heavy-duty industrial toggle switch with red/green LED
        Keypad,         // Security keypad with passcode entry
        AirLockConsole  // Large airlock control console
    };

    struct InteractiveEntity {
        int id = 0;
        InteractiveType type = InteractiveType::Terminal;
        Vec3 position{ 0.0f, 0.0f, 0.0f };
        Vec3 size{ 0.8f, 0.6f, 0.12f };
        Vec3 normal{ 0.0f, 0.0f, 1.0f };
        
        std::string title = "SECURITY TERMINAL";
        std::string subtitle = "SECTOR ACCESS CONTROL";
        std::string statusText = "STANDBY - READY";
        Vec3 themeColor{ 0.2f, 0.8f, 1.0f }; // Cyan default

        bool isLocked = false;
        bool isActivated = false;
        int targetDoorIndex = -1; // Index of door in LabMap to operate
        float interactionRadius = 2.8f;
        float cooldown = 0.0f;
        float displayTimer = 0.0f;

        std::string accessCode = "0451";
        std::vector<std::string> logLines;
    };

    class InteractiveSystem {
    public:
        InteractiveSystem();
        ~InteractiveSystem();

        void init();
        void shutdown();
        void clear();

        void addEntity(const InteractiveEntity& entity);
        InteractiveEntity* getEntity(int id);
        std::vector<InteractiveEntity>& getEntities() { return _entities; }
        const std::vector<InteractiveEntity>& getEntities() const { return _entities; }

        InteractiveEntity* getHoveredEntity() const { return _hoveredEntity; }
        int getHoveredEntityIndex() const { return _hoveredIndex; }

        // Updates interaction detection and handles [E] use input
        void update(float dt, const Vec3& playerPos, const Vec3& lookDir, bool useKeyPressed, LabMap* map = nullptr);

        // Renders all in-world terminal screens, housings, and status telemetry in 3D
        void render(const Camera& cam);

        // Renders 2D HUD prompt (e.g. "[E] USE TERMINAL") when looking at an interactive entity
        void renderHUD(int screenWidth, int screenHeight);

        const std::string& getLastNotice() const { return _lastInteractionNotice; }
        float getNoticeTimer() const { return _noticeTimer; }

    private:
        std::vector<InteractiveEntity> _entities;
        InteractiveEntity* _hoveredEntity = nullptr;
        int _hoveredIndex = -1;
        float _hudFadeAlpha = 0.0f;
        std::string _lastInteractionNotice;
        float _noticeTimer = 0.0f;
        Vec3 _noticeColor{ 1.0f, 1.0f, 1.0f };

        std::unique_ptr<Texture> _texScreenGrid;
        bool _initialized = false;

        void initScreenTexture();
    };

} // namespace Lab
