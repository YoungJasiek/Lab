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
        RetinalScanner, // Biometric Retinal Scanner with animated eye calibration & laser sweep
        Terminal,       // Auxiliary computer console
        WallSwitch,     // Heavy-duty industrial toggle switch with red/green LED
        Keypad          // Security keypad with passcode entry
    };

    struct InteractiveEntity {
        int id = 0;
        InteractiveType type = InteractiveType::RetinalScanner;
        Vec3 position{ 0.0f, 0.0f, 0.0f };
        Vec3 size{ 0.70f, 0.55f, 0.12f };
        Vec3 normal{ 0.0f, 0.0f, 1.0f };
        
        std::string title = "RETINAL SCANNER";
        std::string subtitle = "BIOMETRIC AIRLOCK GATE";
        std::string statusText = "STANDBY - READY";
        Vec3 themeColor{ 0.2f, 0.85f, 1.0f }; // Cyan default

        bool isLocked = true;
        bool isActivated = false;
        int targetDoorIndex = -1; // Index of door in LabMap to operate
        float interactionRadius = 2.8f;
        float cooldown = 0.0f;
        float displayTimer = 0.0f;

        // Biometric Retinal Scanning State
        bool isScanning = false;
        float scanProgress = 0.0f;       // 0.0 to 1.0
        float scanDuration = 1.25f;      // Seconds to complete retinal match
        std::string authorizedUser = "DR. VANCE";
        int clearanceLevel = 3;
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

        // Retinal scan triggers
        bool triggerRetinalScan(int entityId, LabMap* map = nullptr);

        // Renders all in-world scanner screens, housings, and status telemetry in 3D
        void render(const Camera& cam);

        // Renders 2D HUD prompt & scanning progress widget
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
        std::unique_ptr<Texture> _texMonitorFace;
        bool _initialized = false;

        void initScreenTexture();
        void updateMonitorFaceTexture(const InteractiveEntity& ent);
    };

} // namespace Lab
