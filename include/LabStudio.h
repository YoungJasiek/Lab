#pragma once
#include <string>
#include <vector>
#include <array>
#include <memory>
#include <unordered_map>
#include "LabMath.h"
#include "LabRenderer.h"
#include "LabCamera.h"
#include "LabAnim.h"
#include "LabArms.h"
#include "LabFace.h"
#include "LabWeapon.h"

struct GLFWwindow;

namespace Lab {

    // --- Weapon Grip & Socket Configuration ---
    struct WeaponGripConfig {
        // Hand Sockets
        Vec3 rightSocketPos{-0.015f, -0.065f, -0.04f};
        Vec3 rightSocketRot{8.0f, 0.0f, -4.0f};
        Vec3 leftSocketPos{0.015f, 0.012f, -0.045f};
        Vec3 leftSocketRot{25.0f, 0.0f, -20.0f};
        Vec3 adsOffset{0.0f, -0.05f, 0.12f};

        // Weapon 3D Model Transform (Translation, Rotation, Scale)
        Vec3 weaponOffset{0.0f, 0.0f, 0.0f};     // Translation (X, Y, Z in meters)
        Vec3 weaponRotation{0.0f, 0.0f, 0.0f};   // Rotation (Pitch X, Yaw Y, Roll Z in degrees)
        Vec3 weaponScale{1.0f, 1.0f, 1.0f};      // Scale (Scale X, Y, Z, default 1.0)
    };

    enum class PoserSubMode : int {
        WeaponTransform = 0,
        HandSockets = 1,
        AdsAlignment = 2
    };

    enum class ViewportToolMode : int {
        OrbitCamera = 0,
        MoveWeapon = 1,
        RotateWeapon = 2,
        ScaleWeapon = 3
    };

    // --- Reload Animation Timeline Choreography ---
    struct ReloadTimelineConfig {
        float dipDuration = 0.35f;
        float magDropTime = 0.28f;
        float magInsertTime = 0.62f;
        float boltRackTime = 0.88f;
        float dipDepth = 0.08f;
        float tiltAngle = 18.0f;
    };

    // --- Weapon Skin & Material System ---
    struct WeaponSkinConfig {
        std::string textureFile = "weapon_m4a4s.bmp";
        float uvScale = 1.0f;
        Vec3 tintColor{1.0f, 1.0f, 1.0f};
        float metallic = 0.5f;
        float roughness = 0.5f;
    };

    // --- Character Outfit & Appearance ---
    enum class ArmorClass : int {
        LightScout = 0,
        CryoMarine = 1,
        TacticalOfficer = 2
    };

    enum class HelmetType : int {
        CombatVisor = 0,
        SealedHelmet = 1,
        TacticalBeanie = 2
    };

    enum class CamoPattern : int {
        UrbanSlate = 0,
        ArcticWhite = 1,
        CryoNavy = 2,
        HazardRust = 3
    };

    struct CharacterAppearance {
        ArmorClass armorClass = ArmorClass::CryoMarine;
        HelmetType helmetType = HelmetType::CombatVisor;
        Vec3 visorGlowColor{0.2f, 0.85f, 1.0f};
        CamoPattern camo = CamoPattern::UrbanSlate;
        Vec3 fatiguesColor{0.18f, 0.20f, 0.24f};
        Vec3 armorPlateColor{0.26f, 0.28f, 0.32f};
    };

    // --- Face & Dialogue Lip-Sync Session ---
    struct FaceMorphWeights {
        float jawOpen = 0.0f;
        float mouthNarrow = 0.0f;
        float mouthSmile = 0.0f;
        float browRaise = 0.0f;
        float eyesSquint = 0.0f;
        float mouthFrown = 0.0f;
    };

    struct DialogueLine {
        std::string speaker;
        std::string text;
        float duration;
        float syllablesPerSec;
        unsigned int seed;
    };

    enum class StudioTab : int {
        GripPoser = 0,
        ReloadTimeline = 1,
        WeaponSkins = 2,
        Appearance = 3,
        FaceDialogue = 4
    };

    enum class StudioViewMode : int {
        ArmsFPP = 0,
        CharacterTurntable = 1,
        FaceCloseUp = 2
    };

    class CharacterStudio {
    public:
        CharacterStudio();
        ~CharacterStudio();

        void init();
        void shutdown();

        // Main interaction and rendering tick
        void update(float dt, float mouseX, float mouseY, bool lmbPressed, bool rmbPressed, float scrollDelta = 0.0f);
        void render(int screenWidth, int screenHeight);

        // Configuration Persistence
        bool saveConfig(const std::string& filepath = "assets/configs/character_studio.cfg");
        bool loadConfig(const std::string& filepath = "assets/configs/character_studio.cfg");
        void resetDefaults();

        // Getters for integration with game engine and weapon system
        const WeaponGripConfig& getWeaponGrip(WeaponID id) const;
        WeaponGripConfig& getWeaponGripMut(WeaponID id);
        const WeaponSkinConfig& getWeaponSkin(WeaponID id) const;
        WeaponSkinConfig& getWeaponSkinMut(WeaponID id);
        const CharacterAppearance& getAppearance() const { return _appearance; }
        CharacterAppearance& getAppearanceMut() { return _appearance; }
        const ReloadTimelineConfig& getReloadTimeline() const { return _reloadTimeline; }
        ReloadTimelineConfig& getReloadTimelineMut() { return _reloadTimeline; }

        StudioTab getActiveTab() const { return _activeTab; }
        void setActiveTab(StudioTab tab) { _activeTab = tab; }

        int getSelectedWeapon() const { return _selectedWeaponIndex; }
        void setSelectedWeapon(int idx) { _selectedWeaponIndex = std::clamp(idx, 0, 8); }

        // Camera control
        void resetCamera();

        // Window reference for Win32 file dialogs
        void setWindow(GLFWwindow* window) { _window = window; }
        GLFWwindow* getWindow() const { return _window; }

        // Exit Studio request handling
        bool requestExit() const { return _requestExit; }
        void clearRequestExit() { _requestExit = false; }

        // Keyboard navigation and shortcuts
        void handleKeyDown(int key, bool ctrl, bool shift);

        // Undo / Redo & Clipboard
        void pushUndoState();
        void undo();
        void redo();
        void copyGrip();
        void pasteGrip();

        // Weapon Transform & Sub-Mode controls
        PoserSubMode getPoserSubMode() const { return _poserSubMode; }
        void setPoserSubMode(PoserSubMode mode) { _poserSubMode = mode; }

        ViewportToolMode getViewportToolMode() const { return _viewportToolMode; }
        void setViewportToolMode(ViewportToolMode mode) { _viewportToolMode = mode; }

        void centerActiveWeapon();
        void resetActiveWeaponRotation();
        void resetActiveWeaponScale();
        void resetActiveWeaponAllTransforms();

        // Dropdown Menu State
        enum class DropdownMenu : int {
            None = -1,
            File = 0,
            Edit = 1,
            View = 2,
            Tools = 3,
            Help = 4
        };
        DropdownMenu getActiveDropdown() const { return _activeDropdown; }
        void setActiveDropdown(DropdownMenu menu) { _activeDropdown = menu; }

    private:
        // Studio sub-renderers (Strict Core Profile OpenGL 4.5+)
        void render3DScene(int viewportX, int viewportY, int viewportW, int viewportH);
        void renderGroundGrid();
        void renderWeaponGripView();
        void renderReloadTimelineView();
        void renderCharacterBody();
        void renderFaceDialogueView();
        void renderSocketGizmo(const Vec3& pos, const Vec3& rot);

        // Hammer UI renderers
        void renderHammerUI(int screenWidth, int screenHeight);
        void renderHammerTopMenuBar(float w);
        void renderHammerToolbar(float w);
        void renderHammerLeftToolPalette(float h);
        void renderHammerRightInspector(float panelX, float panelY, float panelW, float panelH);
        void renderHammerConsole(float conX, float conY, float conW, float conH);
        void renderHammerStatusBar(float w, float h);
        void renderHammerDropdownMenus(float w, float h);

        // Tab inspectors
        void renderTabGripPoser(float x, float y, float w, float h);
        void renderTabReloadTimeline(float x, float y, float w, float h);
        void renderTabWeaponSkins(float x, float y, float w, float h);
        void renderTabAppearance(float x, float y, float w, float h);
        void renderTabFaceDialogue(float x, float y, float w, float h);

        // UI Helpers (Hammer-style beveled buttons, sliders, input fields)
        bool drawHammerButton(float x, float y, float w, float h, const std::string& label, bool active = false, bool highlighted = false);
        bool drawHammerSlider(float x, float y, float w, float h, const std::string& label, float& value, float minVal, float maxVal, const std::string& format = "%.2f");
        void drawHammerPanel(float x, float y, float w, float h, const std::string& title = "");
        void drawHammerBevel(float x, float y, float w, float h, bool sunken = false);

        Texture* getTexture(const std::string& filename);
        Mesh* getMesh(const std::string& filename);

        // Undo / Redo Snapshot
        struct StudioSnapshot {
            std::array<WeaponGripConfig, 9> grips;
            std::array<WeaponSkinConfig, 9> skins;
            ReloadTimelineConfig reloadTimeline;
            CharacterAppearance appearance;
            FaceMorphWeights faceMorphs;
        };
        std::vector<StudioSnapshot> _undoStack;
        std::vector<StudioSnapshot> _redoStack;

        // Clipboard
        WeaponGripConfig _clipboardGrip;
        bool _hasClipboardGrip = false;

        // Active State
        DropdownMenu _activeDropdown = DropdownMenu::None;
        GLFWwindow* _window = nullptr;
        bool _requestExit = false;

        // Subsystems and state
        std::array<WeaponGripConfig, 9> _weaponGrips;
        std::array<WeaponSkinConfig, 9> _weaponSkins;
        ReloadTimelineConfig _reloadTimeline;
        CharacterAppearance _appearance;
        FaceMorphWeights _faceMorphs;

        StudioTab _activeTab = StudioTab::GripPoser;
        PoserSubMode _poserSubMode = PoserSubMode::WeaponTransform;
        ViewportToolMode _viewportToolMode = ViewportToolMode::OrbitCamera;
        StudioViewMode _viewMode = StudioViewMode::ArmsFPP;
        int _selectedWeaponIndex = 3; // M4A4-S default
        bool _showGrid = true;
        bool _showGizmos = true;
        bool _turntableAutoRotate = false;
        float _turntableYaw = 0.0f;
        float _turntablePitch = 12.0f;
        float _cameraDist = 1.6f;
        Vec3 _cameraTarget{0.0f, 0.0f, 0.0f};

        // Reload Timeline Scrubber State
        float _scrubberPos = 0.0f; // [0.0 ... 1.0]
        bool _scrubberPlaying = false;
        bool _scrubberLoop = true;
        float _scrubberSpeed = 1.0f;
        float _scrubberTotalDuration = 2.4f;

        // Dialogue Lip-Sync State
        std::vector<DialogueLine> _dialogueLines;
        int _selectedDialogueIndex = 0;
        bool _dialoguePlaying = false;
        float _dialogueTimer = 0.0f;
        std::vector<float> _dialogueWaveform;
        std::vector<float> _speechTrackSamples;
        LipSyncEvaluator _lipSyncEvaluator;

        // Models and Assets
        std::unique_ptr<FacialMesh> _facialHead;
        std::unique_ptr<ViewModelArms> _arms;
        std::unique_ptr<WeaponAnimator> _animator;
        std::unordered_map<std::string, std::unique_ptr<Texture>> _textures;
        std::unordered_map<std::string, std::unique_ptr<Mesh>> _meshes;

        // Ground grid GPU buffer
        unsigned int _gridVAO = 0;
        unsigned int _gridVBO = 0;
        int _gridLineVertexCount = 0;

        // Diagnostics log
        std::vector<std::string> _consoleLogs;
        void log(const std::string& msg);

        // Mouse interaction state
        float _mouseX = 0.0f;
        float _mouseY = 0.0f;
        float _lastMouseX = 0.0f;
        float _lastMouseY = 0.0f;
        bool _lmbPressed = false;
        bool _lmbClicked = false;
        bool _rmbPressed = false;
        bool _lastLmb = false;
        bool _isDraggingViewport = false;
        bool _isDraggingScrubber = false;
        int _activeSliderId = -1;
        int _screenWidth = 1600;
        int _screenHeight = 900;
    };

} // namespace Lab
