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

    enum class TerminalPage {
        Main,
        DoorControl,
        SecurityLogs,
        BotTelemetry
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
        Vec3 themeColor{ 0.2f, 0.85f, 1.0f }; // Cyan default

        bool isLocked = true;
        bool isActivated = false;
        int targetDoorIndex = -1; // Index of door in LabMap to operate
        float interactionRadius = 2.8f;
        float cooldown = 0.0f;
        float displayTimer = 0.0f;

        // Terminal OS runtime state
        bool isTerminalOpen = false;
        TerminalPage currentPage = TerminalPage::Main;
        std::string accessCode = "0451";
        std::vector<std::string> securityLogs;
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

        bool isAnyTerminalOpen() const;
        InteractiveEntity* getActiveTerminal();
        void closeActiveTerminal();

        // Updates interaction detection and handles [E] use input
        void update(float dt, const Vec3& playerPos, const Vec3& lookDir, bool useKeyPressed, LabMap* map = nullptr);

        // Handles hotkeys (1, 2, 3, 0, ESC, E) while inside Terminal OS mode
        bool handleTerminalKey(int key, LabMap* map = nullptr, int aliveBotsCount = 0);

        // Renders all in-world terminal screens, housings, and status telemetry in 3D
        void render(const Camera& cam);

        // Renders 2D HUD prompt (e.g. "[E] USE TERMINAL") when looking at an interactive entity
        void renderHUD(int screenWidth, int screenHeight);

        // Renders the full-screen interactive Computer Terminal OS (LAB-OS)
        void renderTerminalOS(int screenWidth, int screenHeight, const LabMap* map, int aliveBotsCount = 0);

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
        std::unique_ptr<Texture> _texMonitorFace;
        bool _initialized = false;

        void initScreenTexture();
        void updateMonitorFaceTexture(const InteractiveEntity& ent);
    };

} // namespace Lab
