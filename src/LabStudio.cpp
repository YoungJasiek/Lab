#include "LabStudio.h"
#include "Lab.h"
#include "LabFont.h"
#include "LabDialogs.h"
#include <glad/gl.h>
#include <GLFW/glfw3.h>
#include <fstream>
#include <sstream>
#include <iostream>
#include <filesystem>
#include <cmath>
#include <algorithm>
#include <iomanip>
#include <functional>

namespace Lab {

    static constexpr float PI = 3.14159265358979323846f;

    static Vec3 rotateOffsetVec(const Vec3& offset, const Vec3& rot) {
        float rx = rot.x * PI / 180.0f;
        float ry = rot.y * PI / 180.0f;
        float rz = rot.z * PI / 180.0f;

        // Roll (Z)
        float x1 = offset.x * std::cos(rz) - offset.y * std::sin(rz);
        float y1 = offset.x * std::sin(rz) + offset.y * std::cos(rz);
        float z1 = offset.z;

        // Pitch (X)
        float y2 = y1 * std::cos(rx) - z1 * std::sin(rx);
        float z2 = y1 * std::sin(rx) + z1 * std::cos(rx);
        float x2 = x1;

        // Yaw (Y)
        float x3 = x2 * std::cos(ry) + z2 * std::sin(ry);
        float z3 = -x2 * std::sin(ry) + z2 * std::cos(ry);
        float y3 = y2;

        return Vec3(x3, y3, z3);
    }

    CharacterStudio::CharacterStudio() {
        resetDefaults();
    }

    CharacterStudio::~CharacterStudio() {
        shutdown();
    }

    void CharacterStudio::resetDefaults() {
        // Weapon 0: Pipe
        _weaponGrips[0] = {
            Vec3(0.0f, -0.065f, -0.04f), Vec3(8.0f, 0.0f, -4.0f),
            Vec3(-0.01f, -0.125f, -0.02f), Vec3(12.0f, -5.0f, 6.0f),
            Vec3(0.0f, -0.04f, 0.08f)
        };
        _weaponSkins[0] = { "pipe.stl", "weapon_pipe.bmp", 1.0f, Vec3(1.0f, 1.0f, 1.0f), 0.7f, 0.3f };

        // Weapon 1: Pistol
        _weaponGrips[1] = {
            Vec3(-0.015f, -0.065f, -0.04f), Vec3(8.0f, 0.0f, -4.0f),
            Vec3(-0.055f, -0.105f, -0.035f), Vec3(14.0f, -12.0f, 15.0f),
            Vec3(0.0f, -0.06f, 0.12f)
        };
        _weaponSkins[1] = { "", "weapon_pistol.bmp", 1.0f, Vec3(1.0f, 1.0f, 1.0f), 0.8f, 0.2f };

        // Weapon 2: Shotgun
        _weaponGrips[2] = {
            Vec3(-0.015f, -0.065f, -0.04f), Vec3(8.0f, 0.0f, -4.0f),
            Vec3(-0.065f, -0.060f, -0.26f), Vec3(22.0f, 12.0f, -22.0f),
            Vec3(0.0f, -0.07f, 0.14f)
        };
        _weaponSkins[2] = { "", "weapon_shotgun.bmp", 1.0f, Vec3(1.0f, 1.0f, 1.0f), 0.6f, 0.4f };

        // Weapon 3: M4A4-S
        _weaponGrips[3] = {
            Vec3(-0.015f, -0.065f, -0.04f), Vec3(8.0f, 0.0f, -4.0f),
            Vec3(-0.065f, -0.060f, -0.34f), Vec3(22.0f, 12.0f, -22.0f),
            Vec3(0.0f, -0.07f, 0.15f)
        };
        _weaponSkins[3] = { "", "weapon_m4a4s.bmp", 1.0f, Vec3(1.0f, 1.0f, 1.0f), 0.6f, 0.4f };

        // Weapon 4: SG553
        _weaponGrips[4] = {
            Vec3(-0.015f, -0.065f, -0.04f), Vec3(8.0f, 0.0f, -4.0f),
            Vec3(-0.065f, -0.060f, -0.34f), Vec3(22.0f, 12.0f, -22.0f),
            Vec3(0.0f, -0.07f, 0.15f)
        };
        _weaponSkins[4] = { "", "weapon_sg553.bmp", 1.0f, Vec3(1.0f, 1.0f, 1.0f), 0.5f, 0.5f };

        // Weapon 5: Minigun
        _weaponGrips[5] = {
            Vec3(0.0f, -0.05f, 0.02f), Vec3(0.0f, 0.0f, 0.0f),
            Vec3(0.0f, 0.08f, -0.15f), Vec3(-15.0f, 0.0f, 0.0f),
            Vec3(0.0f, -0.05f, 0.10f)
        };
        _weaponSkins[5] = { "", "weapon_minigun.bmp", 1.0f, Vec3(1.0f, 1.0f, 1.0f), 0.8f, 0.2f };

        // Weapon 6: Plasma Rifle
        _weaponGrips[6] = {
            Vec3(-0.015f, -0.065f, -0.04f), Vec3(8.0f, 0.0f, -4.0f),
            Vec3(-0.065f, -0.060f, -0.30f), Vec3(20.0f, 10.0f, -20.0f),
            Vec3(0.0f, -0.07f, 0.15f)
        };
        _weaponSkins[6] = { "", "weapon_plasma.bmp", 1.0f, Vec3(1.0f, 1.0f, 1.0f), 0.9f, 0.1f };

        // Weapon 7: Railgun
        _weaponGrips[7] = {
            Vec3(-0.015f, -0.065f, -0.04f), Vec3(8.0f, 0.0f, -4.0f),
            Vec3(-0.065f, -0.060f, -0.36f), Vec3(20.0f, 10.0f, -20.0f),
            Vec3(0.0f, -0.07f, 0.15f)
        };
        _weaponSkins[7] = { "railgun.stl", "weapon_railgun.bmp", 1.0f, Vec3(1.0f, 1.0f, 1.0f), 0.9f, 0.1f };

        // Weapon 8: RPG
        _weaponGrips[8] = {
            Vec3(-0.02f, -0.08f, -0.02f), Vec3(5.0f, 0.0f, -2.0f),
            Vec3(-0.06f, -0.05f, -0.28f), Vec3(25.0f, 15.0f, -15.0f),
            Vec3(0.0f, -0.08f, 0.16f)
        };
        _weaponSkins[8] = { "", "weapon_rpg.bmp", 1.0f, Vec3(1.0f, 1.0f, 1.0f), 0.4f, 0.6f };

        // Reload timeline defaults
        _reloadTimeline = { 0.35f, 0.28f, 0.62f, 0.88f, 0.08f, 18.0f };

        // Appearance defaults
        _appearance.armorClass = ArmorClass::CryoMarine;
        _appearance.helmetType = HelmetType::CombatVisor;
        _appearance.visorGlowColor = Vec3(0.2f, 0.85f, 1.0f);
        _appearance.camo = CamoPattern::UrbanSlate;
        _appearance.fatiguesColor = Vec3(0.18f, 0.20f, 0.24f);
        _appearance.armorPlateColor = Vec3(0.26f, 0.28f, 0.32f);

        // Face morph defaults
        _faceMorphs = { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };

        // Bot weapon socket defaults
        for (int i = 0; i < 9; ++i) {
            _weaponGrips[i].botSocket = { Vec3(0.0f, -0.05f, 0.02f), Vec3(5.73f, -11.46f, 0.0f), Vec3(1.0f, 1.0f, 1.0f) };
        }

        // Animation preview defaults
        _activeAnimState = StudioAnimState::Idle;
        _animPlaybackSpeed = 1.0f;
        _animPlaying = true;
        _viewmodelWalkSpeed = 0.0f;
        _activeBotClipName = "Idle";
    }

    void CharacterStudio::pushUndoState() {
        StudioSnapshot snap;
        snap.grips = _weaponGrips;
        snap.skins = _weaponSkins;
        snap.reloadTimeline = _reloadTimeline;
        snap.appearance = _appearance;
        snap.faceMorphs = _faceMorphs;
        _undoStack.push_back(snap);
        if (_undoStack.size() > 30) {
            _undoStack.erase(_undoStack.begin());
        }
        _redoStack.clear();
    }

    void CharacterStudio::undo() {
        if (_undoStack.empty()) {
            log("Undo: Nothing to undo.");
            return;
        }
        StudioSnapshot current;
        current.grips = _weaponGrips;
        current.skins = _weaponSkins;
        current.reloadTimeline = _reloadTimeline;
        current.appearance = _appearance;
        current.faceMorphs = _faceMorphs;
        _redoStack.push_back(current);

        auto snap = _undoStack.back();
        _undoStack.pop_back();
        _weaponGrips = snap.grips;
        _weaponSkins = snap.skins;
        _reloadTimeline = snap.reloadTimeline;
        _appearance = snap.appearance;
        _faceMorphs = snap.faceMorphs;
        log("Undo: reverted to previous modification.");
    }

    void CharacterStudio::redo() {
        if (_redoStack.empty()) {
            log("Redo: Nothing to redo.");
            return;
        }
        StudioSnapshot current;
        current.grips = _weaponGrips;
        current.skins = _weaponSkins;
        current.reloadTimeline = _reloadTimeline;
        current.appearance = _appearance;
        current.faceMorphs = _faceMorphs;
        _undoStack.push_back(current);

        auto snap = _redoStack.back();
        _redoStack.pop_back();
        _weaponGrips = snap.grips;
        _weaponSkins = snap.skins;
        _reloadTimeline = snap.reloadTimeline;
        _appearance = snap.appearance;
        _faceMorphs = snap.faceMorphs;
        log("Redo: re-applied modification.");
    }

    void CharacterStudio::copyGrip() {
        _clipboardGrip = _weaponGrips[_selectedWeaponIndex];
        _hasClipboardGrip = true;
        log("Copied weapon grip sockets to clipboard.");
    }

    void CharacterStudio::pasteGrip() {
        if (!_hasClipboardGrip) {
            log("Clipboard is empty! Copy a grip first (Ctrl+C).");
            return;
        }
        pushUndoState();
        _weaponGrips[_selectedWeaponIndex] = _clipboardGrip;
        log("Pasted grip sockets to active weapon.");
    }

    void CharacterStudio::handleKeyDown(int key, bool ctrl, bool shift) {
        (void)shift;
        if (key == 256) { // GLFW_KEY_ESCAPE
            if (_activeDropdown != DropdownMenu::None) {
                _activeDropdown = DropdownMenu::None;
            } else {
                _requestExit = true;
            }
            return;
        }

        if (ctrl) {
            if (key == 83 || key == 'S') { // Ctrl+S
                saveConfig("assets/configs/character_studio.cfg");
                return;
            }
            if (key == 79 || key == 'O') { // Ctrl+O
                std::string p = LabDialogs::openFileDialog(_window, "Studio Config Files (*.cfg)\0*.cfg\0All Files (*.*)\0*.*\0", "assets\\configs");
                if (!p.empty()) loadConfig(p);
                else loadConfig("assets/configs/character_studio.cfg");
                return;
            }
            if (key == 90 || key == 'Z') { // Ctrl+Z
                undo();
                return;
            }
            if (key == 89 || key == 'Y') { // Ctrl+Y
                redo();
                return;
            }
            if (key == 67 || key == 'C') { // Ctrl+C
                copyGrip();
                return;
            }
            if (key == 86 || key == 'V') { // Ctrl+V
                pasteGrip();
                return;
            }
        } else {
            if (key == 268) { // GLFW_KEY_HOME
                resetCamera();
                return;
            }
            if (key == 71 || key == 'G') {
                _showGrid = !_showGrid;
                log(_showGrid ? "Ground grid ON" : "Ground grid OFF");
                return;
            }
            if (key == 90 || key == 'Z') {
                _showGizmos = !_showGizmos;
                log(_showGizmos ? "Gizmos ON" : "Gizmos OFF");
                return;
            }
            if (key == 87 || key == 'W') {
                _viewportToolMode = ViewportToolMode::MoveWeapon;
                log("Tool Mode: Move Weapon [W]");
                return;
            }
            if (key == 69 || key == 'E') {
                _viewportToolMode = ViewportToolMode::RotateWeapon;
                log("Tool Mode: Rotate Weapon [E]");
                return;
            }
            if (key == 84 || key == 'T') {
                _viewportToolMode = ViewportToolMode::ScaleWeapon;
                log("Tool Mode: Scale Weapon [T]");
                return;
            }
            if (key == 81 || key == 'Q') {
                _viewportToolMode = ViewportToolMode::OrbitCamera;
                log("Tool Mode: Camera Orbit 360 [Q]");
                return;
            }
            if (key == 82 || key == 'R') {
                _turntableAutoRotate = !_turntableAutoRotate;
                return;
            }
            if (key == 76 || key == 'L') {
                pushUndoState();
                auto& curGrip = _weaponGrips[_selectedWeaponIndex];
                curGrip.lockHands = !curGrip.lockHands;
                log(curGrip.lockHands 
                    ? "Lock Hands ON [L]: Moving/rotating weapon will NOT move character hands."
                    : "Lock Hands OFF [L]: Weapon and hands move together.");
                return;
            }
            if (key == 49 || key == '1') {
                _activeTab = StudioTab::GripPoser;
                resetCamera();
                return;
            }
            if (key == 50 || key == '2') {
                _activeTab = StudioTab::ReloadTimeline;
                resetCamera();
                return;
            }
            if (key == 51 || key == '3') {
                _activeTab = StudioTab::WeaponSkins;
                resetCamera();
                return;
            }
            if (key == 52 || key == '4') {
                _activeTab = StudioTab::Appearance;
                resetCamera();
                return;
            }
            if (key == 53 || key == '5') {
                _activeTab = StudioTab::FaceDialogue;
                resetCamera();
                return;
            }
            if (key == 32) { // GLFW_KEY_SPACE
                if (_activeTab == StudioTab::ReloadTimeline) {
                    _scrubberPlaying = !_scrubberPlaying;
                } else if (_activeTab == StudioTab::FaceDialogue) {
                    _dialoguePlaying = !_dialoguePlaying;
                    if (_dialoguePlaying) _dialogueTimer = 0.0f;
                } else if (_activeTab == StudioTab::GripPoser) {
                    _animPlaying = !_animPlaying;
                    log(_animPlaying ? "Animation playback resumed [SPACE]." : "Animation playback paused [SPACE].");
                }
                return;
            }
        }
    }

    void CharacterStudio::init() {
        LabLog::info("CharacterStudio: Initializing Lab Character & Weapon Studio...");

        _arms = std::make_unique<ViewModelArms>();
        _animator = std::make_unique<WeaponAnimator>();
        _facialHead = FacialMesh::createProceduralHead();

        // Dialogue Test lines
        _dialogueLines = {
            { "Dr. Vance", "Access Granted. Welcome to Sector 4 Cryo Lab.", 3.2f, 4.2f, 1337 },
            { "Tactical AI", "Warning: Cryogenic containment pressure critical.", 2.8f, 4.8f, 2048 },
            { "Marine Lead", "Hostile combat bots approaching perimeter! Open fire!", 3.4f, 5.2f, 9999 },
            { "Security Control", "Sector perimeter secured. All airlocks sealed.", 2.6f, 4.0f, 4242 }
        };

        // Generate speech track for default line
        _speechTrackSamples = LipSyncEvaluator::generateSpeechTrack(
            _dialogueLines[0].duration, _dialogueLines[0].syllablesPerSec, _dialogueLines[0].seed);

        // Build 3D Ground Grid (Green/grey coordinate plane)
        std::vector<Vertex> gridVerts;
        const int halfExtents = 24;
        const float spacing = 0.5f;

        for (int i = -halfExtents; i <= halfExtents; ++i) {
            float coord = static_cast<float>(i) * spacing;
            Vec3 color = (i == 0) ? Vec3(0.8f, 0.2f, 0.2f) : ((i % 4 == 0) ? Vec3(0.40f, 0.48f, 0.42f) : Vec3(0.24f, 0.28f, 0.25f));

            // Line parallel to Z
            Vertex v1, v2;
            v1.position = Vec3(coord, -0.001f, -halfExtents * spacing);
            v1.color = color;
            v1.normal = Vec3(0, 1, 0);
            v2.position = Vec3(coord, -0.001f, halfExtents * spacing);
            v2.color = color;
            v2.normal = Vec3(0, 1, 0);
            gridVerts.push_back(v1);
            gridVerts.push_back(v2);

            // Line parallel to X
            color = (i == 0) ? Vec3(0.2f, 0.4f, 0.85f) : ((i % 4 == 0) ? Vec3(0.40f, 0.48f, 0.42f) : Vec3(0.24f, 0.28f, 0.25f));
            Vertex v3, v4;
            v3.position = Vec3(-halfExtents * spacing, -0.001f, coord);
            v3.color = color;
            v3.normal = Vec3(0, 1, 0);
            v4.position = Vec3(halfExtents * spacing, -0.001f, coord);
            v4.color = color;
            v4.normal = Vec3(0, 1, 0);
            gridVerts.push_back(v3);
            gridVerts.push_back(v4);
        }
        _gridLineVertexCount = static_cast<int>(gridVerts.size());

        glCreateVertexArrays(1, &_gridVAO);
        glCreateBuffers(1, &_gridVBO);
        glNamedBufferData(_gridVBO, sizeof(Vertex) * gridVerts.size(), gridVerts.data(), GL_STATIC_DRAW);

        glVertexArrayVertexBuffer(_gridVAO, 0, _gridVBO, 0, sizeof(Vertex));
        glEnableVertexArrayAttrib(_gridVAO, 0);
        glVertexArrayAttribFormat(_gridVAO, 0, 3, GL_FLOAT, GL_FALSE, offsetof(Vertex, position));
        glVertexArrayAttribBinding(_gridVAO, 0, 0);

        glEnableVertexArrayAttrib(_gridVAO, 1);
        glVertexArrayAttribFormat(_gridVAO, 1, 3, GL_FLOAT, GL_FALSE, offsetof(Vertex, normal));
        glVertexArrayAttribBinding(_gridVAO, 1, 0);

        glEnableVertexArrayAttrib(_gridVAO, 2);
        glVertexArrayAttribFormat(_gridVAO, 2, 2, GL_FLOAT, GL_FALSE, offsetof(Vertex, texCoords));
        glVertexArrayAttribBinding(_gridVAO, 2, 0);

        glEnableVertexArrayAttrib(_gridVAO, 3);
        glVertexArrayAttribFormat(_gridVAO, 3, 3, GL_FLOAT, GL_FALSE, offsetof(Vertex, color));
        glVertexArrayAttribBinding(_gridVAO, 3, 0);

        // Preload core textures
        getTexture("weapon_pipe.bmp");
        getTexture("weapon_pistol.bmp");
        getTexture("weapon_shotgun.bmp");
        getTexture("weapon_m4a4s.bmp");
        getTexture("weapon_sg553.bmp");
        getTexture("weapon_minigun.bmp");
        getTexture("weapon_plasma.bmp");
        getTexture("weapon_railgun.bmp");
        getTexture("weapon_rpg.bmp");
        getTexture("cryo_ice.bmp");
        getTexture("metal_hull.bmp");
        getTexture("hazard_stripes.bmp");
        getTexture("snow_frost.bmp");

        // Load Bot Skeletal Assets for Bot Socket 3D Preview
        bool loadedBot = GLTFLoader::load("assets/models/t-800_run.glb", _botSkeleton, _botAnimations, _botMesh);
        if (!loadedBot || !_botMesh) {
            loadedBot = GLTFLoader::load("assets/inwork/t-800_run.glb", _botSkeleton, _botAnimations, _botMesh);
        }
        if (!loadedBot || !_botMesh) {
            GLTFLoader::createProceduralCombatBot(_botSkeleton, _botAnimations, _botMesh);
        }

        // Ingest all FBX / binary animations for Bot Studio
        FBXLoader::loadAllFromDirectory("assets/animations", _botAnimations, _botSkeleton.get());

        if (_botSkeleton && _botMesh) {
            _botAnimator.setSkeleton(_botSkeleton);
            for (const auto& c : _botAnimations) {
                _botAnimator.addClip(c);
            }
            _botAnimator.playAnimation("Idle", true);
        }

        // Try load saved config
        loadConfig("assets/configs/character_studio.cfg");

        log("Lab Character & Weapon Studio ready.");
        log("Select tool from left palette or press 1-5.");
    }

    void CharacterStudio::shutdown() {
        if (_gridVAO) {
            glDeleteVertexArrays(1, &_gridVAO);
            _gridVAO = 0;
        }
        if (_gridVBO) {
            glDeleteBuffers(1, &_gridVBO);
            _gridVBO = 0;
        }
        _textures.clear();
        _meshes.clear();
        _facialHead.reset();
        _arms.reset();
        _animator.reset();
        _botMesh.reset();
        _botSkeleton.reset();
        _botAnimations.clear();
    }

    Texture* CharacterStudio::getTexture(const std::string& filename) {
        if (filename.empty()) return nullptr;
        auto it = _textures.find(filename);
        if (it != _textures.end()) return it->second.get();

        std::vector<std::string> searchPaths = {
            filename,
            "assets/models/textures/" + filename,
            "assets/textures/" + filename,
            "assets/models/" + filename,
            "assets/" + filename,
            "../assets/models/textures/" + filename,
            "../assets/textures/" + filename,
            "../../assets/models/textures/" + filename,
            "../../assets/textures/" + filename
        };
        for (const auto& path : searchPaths) {
            if (std::filesystem::exists(path)) {
                auto tex = std::make_unique<Texture>(path);
                if (tex && tex->getId() != 0) {
                    Texture* ptr = tex.get();
                    _textures[filename] = std::move(tex);
                    return ptr;
                }
            }
        }
        // Last resort: pass to Texture constructor which calls resolveAssetPath
        auto tex = std::make_unique<Texture>(filename);
        if (tex && tex->getId() != 0) {
            Texture* ptr = tex.get();
            _textures[filename] = std::move(tex);
            return ptr;
        }
        return nullptr;
    }

    Mesh* CharacterStudio::getMesh(const std::string& filename) {
        if (filename.empty() || filename == "NONE" || filename == "PROCEDURAL") return nullptr;
        auto it = _meshes.find(filename);
        if (it != _meshes.end()) return it->second.get();

        std::vector<std::string> searchPaths = {
            filename,
            "assets/models/" + filename,
            "assets/" + filename,
            "../assets/models/" + filename,
            "../../assets/models/" + filename,
            "build/Release/assets/models/" + filename
        };
        for (const auto& path : searchPaths) {
            if (std::filesystem::exists(path)) {
                Mesh* m = Mesh::loadSTL(path);
                if (m) {
                    _meshes[filename] = std::unique_ptr<Mesh>(m);
                    return m;
                }
            }
        }
        Mesh* m = Mesh::loadSTL(filename);
        if (m) {
            _meshes[filename] = std::unique_ptr<Mesh>(m);
            return m;
        }
        return nullptr;
    }

    void CharacterStudio::log(const std::string& msg) {
        _consoleLogs.push_back(msg);
        if (_consoleLogs.size() > 7) {
            _consoleLogs.erase(_consoleLogs.begin());
        }
    }

    void CharacterStudio::resetCamera() {
        if (_activeTab == StudioTab::FaceDialogue) {
            _cameraDist = 0.85f;
            _cameraTarget = Vec3(0.0f, 1.62f, 0.0f);
            _turntableYaw = 0.0f;
            _turntablePitch = 4.0f;
        } else if (_activeTab == StudioTab::Appearance) {
            _cameraDist = 2.4f;
            _cameraTarget = Vec3(0.0f, 1.0f, 0.0f);
            _turntableYaw = 15.0f;
            _turntablePitch = 8.0f;
        } else {
            _cameraDist = 1.2f;
            _cameraTarget = Vec3(0.0f, 0.0f, 0.0f);
            _turntableYaw = 0.0f;
            _turntablePitch = 12.0f;
        }
        log("Camera reset to optimal view pose.");
    }

    void CharacterStudio::centerActiveWeapon() {
        pushUndoState();
        _weaponGrips[_selectedWeaponIndex].weaponOffset = Vec3(0.0f, 0.0f, 0.0f);
        log("Centered weapon model offset to (0, 0, 0).");
    }

    void CharacterStudio::resetActiveWeaponRotation() {
        pushUndoState();
        _weaponGrips[_selectedWeaponIndex].weaponRotation = Vec3(0.0f, 0.0f, 0.0f);
        log("Reset weapon model rotation to (0, 0, 0).");
    }

    void CharacterStudio::resetActiveWeaponScale() {
        pushUndoState();
        _weaponGrips[_selectedWeaponIndex].weaponScale = Vec3(1.0f, 1.0f, 1.0f);
        log("Reset weapon model scale to (1.0, 1.0, 1.0).");
    }

    void CharacterStudio::resetActiveWeaponAllTransforms() {
        pushUndoState();
        _weaponGrips[_selectedWeaponIndex].weaponOffset = Vec3(0.0f, 0.0f, 0.0f);
        _weaponGrips[_selectedWeaponIndex].weaponRotation = Vec3(0.0f, 0.0f, 0.0f);
        _weaponGrips[_selectedWeaponIndex].weaponScale = Vec3(1.0f, 1.0f, 1.0f);
        log("Reset all weapon transforms (offset, rotation, scale) to identity.");
    }

    void CharacterStudio::resetBotWeaponSocket() {
        pushUndoState();
        _weaponGrips[_selectedWeaponIndex].botSocket = { Vec3(0.0f, -0.05f, 0.02f), Vec3(5.73f, -11.46f, 0.0f), Vec3(1.0f, 1.0f, 1.0f) };
        log("Reset bot weapon socket to default hand grip.");
    }

    const WeaponGripConfig& CharacterStudio::getWeaponGrip(WeaponID id) const {
        int idx = std::clamp(static_cast<int>(id), 0, 8);
        return _weaponGrips[idx];
    }

    WeaponGripConfig& CharacterStudio::getWeaponGripMut(WeaponID id) {
        int idx = std::clamp(static_cast<int>(id), 0, 8);
        return _weaponGrips[idx];
    }

    const WeaponSkinConfig& CharacterStudio::getWeaponSkin(WeaponID id) const {
        int idx = std::clamp(static_cast<int>(id), 0, 8);
        return _weaponSkins[idx];
    }

    WeaponSkinConfig& CharacterStudio::getWeaponSkinMut(WeaponID id) {
        int idx = std::clamp(static_cast<int>(id), 0, 8);
        return _weaponSkins[idx];
    }

    void CharacterStudio::setStudioAnimation(StudioAnimState state) {
        _activeAnimState = state;
        switch (state) {
            case StudioAnimState::Idle:
                _viewmodelWalkSpeed = 0.0f;
                if (_animator) _animator->state = WeaponAnimState::Idle;
                if (_botAnimator.getSkeleton()) _botAnimator.playAnimation("Idle", true);
                _activeBotClipName = "Idle";
                log("Animation: Idle (Viewmodel stationary, Bot idle stance)");
                break;
            case StudioAnimState::Walk:
                _viewmodelWalkSpeed = 4.8f; // Tactical walking speed triggers dynamic walk sway & bobbing
                if (_animator) _animator->state = WeaponAnimState::Idle;
                if (_botAnimator.getSkeleton()) _botAnimator.playAnimation("Walk", true);
                _activeBotClipName = "Walk";
                log("Animation: Walk (Viewmodel tactical walking bob/sway, Bot run/walk cycle)");
                break;
            case StudioAnimState::Shoot:
                _viewmodelWalkSpeed = 0.0f;
                if (_animator) _animator->onFire();
                if (_botAnimator.getSkeleton()) _botAnimator.playAnimation("Shoot", false);
                _activeBotClipName = "Shoot";
                log("Animation: Shoot (Viewmodel weapon recoil kick, Bot human-like firing arc)");
                break;
            case StudioAnimState::Reload:
                _viewmodelWalkSpeed = 0.0f;
                if (_animator) _animator->onReload();
                if (_botAnimator.getSkeleton()) _botAnimator.playAnimation("Reload", false);
                _activeBotClipName = "Reload";
                log("Animation: Reload (Viewmodel reload choreography, Bot reload clip)");
                break;
            case StudioAnimState::Inspect:
                _viewmodelWalkSpeed = 0.0f;
                if (_animator) _animator->onInspect();
                if (_botAnimator.getSkeleton()) _botAnimator.playAnimation("Inspect", false);
                _activeBotClipName = "Inspect";
                log("Animation: Inspect (Viewmodel weapon inspect rotation, Bot showcase pose)");
                break;
        }
    }

    void CharacterStudio::setBotAnimationClip(const std::string& clipName) {
        _activeBotClipName = clipName;
        if (_botAnimator.getSkeleton()) {
            _botAnimator.playAnimation(clipName, true);
        }
        log("Bot Animation Clip set: " + clipName);
    }

    void CharacterStudio::update(float dt, float mouseX, float mouseY, bool lmbPressed, bool rmbPressed, float scrollDelta) {
        _mouseX = mouseX;
        _mouseY = mouseY;
        _lmbClicked = (lmbPressed && !_lastLmb);
        _lmbPressed = lmbPressed;
        _rmbPressed = rmbPressed;

        float dx = mouseX - _lastMouseX;
        float dy = mouseY - _lastMouseY;

        // Turntable Auto-Rotation
        if (_turntableAutoRotate) {
            _turntableYaw += dt * 30.0f;
            if (_turntableYaw > 360.0f) _turntableYaw -= 360.0f;
        }

        // Viewport Dragging (LMB or RMB inside center 3D viewport only)
        float leftBarW = 72.0f;
        float topBarsH = 52.0f;
        float sbH = 24.0f;
        float conH = std::clamp(_screenHeight * 0.16f, 100.0f, 160.0f);
        float bottomBarsH = conH + sbH;
        float rightPanelW = std::clamp(_screenWidth * 0.28f, 350.0f, 460.0f);

        float vpX = leftBarW;
        float vpY = topBarsH;
        float vpW = static_cast<float>(_screenWidth) - leftBarW - rightPanelW;
        float vpH = static_cast<float>(_screenHeight) - topBarsH - bottomBarsH;

        bool inViewport = (mouseX >= vpX && mouseX <= vpX + vpW && mouseY >= vpY && mouseY <= vpY + vpH);

        if (_activeDropdown == DropdownMenu::None && !_isDraggingScrubber && _activeSliderId == -1) {
            if (_activeTab == StudioTab::GripPoser && inViewport) {
                auto& grip = _weaponGrips[_selectedWeaponIndex];
                if (_poserSubMode == PoserSubMode::BotWeaponSocket) {
                    if (_viewportToolMode == ViewportToolMode::MoveWeapon) {
                        if (lmbPressed) {
                            if (_lmbClicked) pushUndoState();
                            grip.botSocket.offset.x += dx * 0.0012f;
                            grip.botSocket.offset.y -= dy * 0.0012f;
                        }
                        if (std::abs(scrollDelta) > 0.001f) {
                            pushUndoState();
                            grip.botSocket.offset.z += scrollDelta * 0.02f;
                        }
                    } else if (_viewportToolMode == ViewportToolMode::RotateWeapon) {
                        if (lmbPressed) {
                            if (_lmbClicked) pushUndoState();
                            grip.botSocket.rotation.y += dx * 0.4f;
                            grip.botSocket.rotation.x += dy * 0.4f;
                        }
                        if (std::abs(scrollDelta) > 0.001f) {
                            pushUndoState();
                            grip.botSocket.rotation.z += scrollDelta * 4.0f;
                        }
                    } else if (_viewportToolMode == ViewportToolMode::ScaleWeapon) {
                        if (lmbPressed) {
                            if (_lmbClicked) pushUndoState();
                            float sDelta = (dx - dy) * 0.005f;
                            grip.botSocket.scale.x = std::clamp(grip.botSocket.scale.x + sDelta, 0.05f, 5.0f);
                            grip.botSocket.scale.y = std::clamp(grip.botSocket.scale.y + sDelta, 0.05f, 5.0f);
                            grip.botSocket.scale.z = std::clamp(grip.botSocket.scale.z + sDelta, 0.05f, 5.0f);
                        }
                        if (std::abs(scrollDelta) > 0.001f) {
                            pushUndoState();
                            float sDelta = scrollDelta * 0.05f;
                            grip.botSocket.scale.x = std::clamp(grip.botSocket.scale.x + sDelta, 0.05f, 5.0f);
                            grip.botSocket.scale.y = std::clamp(grip.botSocket.scale.y + sDelta, 0.05f, 5.0f);
                            grip.botSocket.scale.z = std::clamp(grip.botSocket.scale.z + sDelta, 0.05f, 5.0f);
                        }
                    } else {
                        // Orbit camera
                        if (lmbPressed) {
                            _turntableYaw += dx * 0.4f;
                            _turntablePitch = std::clamp(_turntablePitch + dy * 0.4f, -85.0f, 85.0f);
                        }
                        if (std::abs(scrollDelta) > 0.001f) {
                            _cameraDist = std::clamp(_cameraDist - scrollDelta * 0.15f, 0.4f, 5.0f);
                        }
                    }
                } else if (_viewportToolMode == ViewportToolMode::MoveWeapon) {
                    if (lmbPressed) {
                        if (_lmbClicked) pushUndoState();
                        grip.weaponOffset.x += dx * 0.0012f;
                        grip.weaponOffset.y -= dy * 0.0012f;
                    }
                    if (std::abs(scrollDelta) > 0.001f) {
                        pushUndoState();
                        grip.weaponOffset.z += scrollDelta * 0.02f;
                    }
                } else if (_viewportToolMode == ViewportToolMode::RotateWeapon) {
                    if (lmbPressed) {
                        if (_lmbClicked) pushUndoState();
                        grip.weaponRotation.y += dx * 0.4f;
                        grip.weaponRotation.x += dy * 0.4f;
                    }
                    if (std::abs(scrollDelta) > 0.001f) {
                        pushUndoState();
                        grip.weaponRotation.z += scrollDelta * 4.0f;
                    }
                } else if (_viewportToolMode == ViewportToolMode::ScaleWeapon) {
                    if (lmbPressed) {
                        if (_lmbClicked) pushUndoState();
                        float sDelta = (dx - dy) * 0.005f;
                        grip.weaponScale.x = std::clamp(grip.weaponScale.x + sDelta, 0.05f, 5.0f);
                        grip.weaponScale.y = std::clamp(grip.weaponScale.y + sDelta, 0.05f, 5.0f);
                        grip.weaponScale.z = std::clamp(grip.weaponScale.z + sDelta, 0.05f, 5.0f);
                    }
                    if (std::abs(scrollDelta) > 0.001f) {
                        pushUndoState();
                        float sDelta = scrollDelta * 0.05f;
                        grip.weaponScale.x = std::clamp(grip.weaponScale.x + sDelta, 0.05f, 5.0f);
                        grip.weaponScale.y = std::clamp(grip.weaponScale.y + sDelta, 0.05f, 5.0f);
                        grip.weaponScale.z = std::clamp(grip.weaponScale.z + sDelta, 0.05f, 5.0f);
                    }
                } else {
                    // Orbit camera
                    if (lmbPressed) {
                        _turntableYaw += dx * 0.4f;
                        _turntablePitch = std::clamp(_turntablePitch + dy * 0.4f, -85.0f, 85.0f);
                    }
                    if (std::abs(scrollDelta) > 0.001f) {
                        _cameraDist = std::clamp(_cameraDist - scrollDelta * 0.15f, 0.4f, 5.0f);
                    }
                }
            } else {
                if (lmbPressed && inViewport) {
                    _turntableYaw += dx * 0.4f;
                    _turntablePitch = std::clamp(_turntablePitch + dy * 0.4f, -85.0f, 85.0f);
                }
                if (std::abs(scrollDelta) > 0.001f && inViewport) {
                    _cameraDist = std::clamp(_cameraDist - scrollDelta * 0.15f, 0.4f, 5.0f);
                }
            }

            // RMB Orbit / Zoom inside viewport in any mode
            if (rmbPressed && inViewport) {
                _cameraDist = std::clamp(_cameraDist + dy * 0.01f, 0.4f, 5.0f);
            }
        }

        // Reload Timeline Scrubber update
        if (_scrubberPlaying) {
            _scrubberPos += (dt * _scrubberSpeed) / _scrubberTotalDuration;
            if (_scrubberPos >= 1.0f) {
                if (_scrubberLoop) {
                    _scrubberPos = std::fmod(_scrubberPos, 1.0f);
                } else {
                    _scrubberPos = 1.0f;
                    _scrubberPlaying = false;
                }
            }
        }

        // Dialogue Lip-Sync Update
        if (_dialoguePlaying && !_speechTrackSamples.empty()) {
            _dialogueTimer += dt;
            const auto& curLine = _dialogueLines[_selectedDialogueIndex];
            if (_dialogueTimer >= curLine.duration) {
                _dialoguePlaying = false;
                _dialogueTimer = 0.0f;
                _lipSyncEvaluator.update(0.0f, dt);
            } else {
                int sampleIdx = static_cast<int>((_dialogueTimer / curLine.duration) * _speechTrackSamples.size());
                sampleIdx = std::clamp(sampleIdx, 0, static_cast<int>(_speechTrackSamples.size()) - 1);
                float amp = _speechTrackSamples[sampleIdx];
                _lipSyncEvaluator.update(amp, dt);

                if (_facialHead) {
                    _lipSyncEvaluator.applyTo(*_facialHead);
                }

                // Record waveform trace for visualizer
                _dialogueWaveform.push_back(amp);
                if (_dialogueWaveform.size() > 80) {
                    _dialogueWaveform.erase(_dialogueWaveform.begin());
                }
            }
        } else if (_facialHead && _activeTab == StudioTab::FaceDialogue) {
            // Apply manual face sliders when speech is inactive
            _facialHead->setWeight("Jaw_Open", _faceMorphs.jawOpen);
            _facialHead->setWeight("Mouth_Narrow", _faceMorphs.mouthNarrow);
            _facialHead->setWeight("Mouth_Smile", _faceMorphs.mouthSmile);
            _facialHead->setWeight("Brow_Raise", _faceMorphs.browRaise);
            _facialHead->setWeight("Eyes_Squint", _faceMorphs.eyesSquint);
            _facialHead->setWeight("Mouth_Frown", _faceMorphs.mouthFrown);
            _facialHead->evaluate();
        }

        if (_animPlaying) {
            float effDt = dt * _animPlaybackSpeed;
            if (_animator) {
                _animator->update(effDt, Vec2(0.0f, 0.0f), _viewmodelWalkSpeed);
                // Return to Idle once single shot recoil animation finishes
                if (_activeAnimState == StudioAnimState::Shoot && _animator->state == WeaponAnimState::Idle) {
                    _activeAnimState = StudioAnimState::Idle;
                }
            }
            if (_botAnimator.getSkeleton()) {
                _botAnimator.update(effDt);
                if (_activeAnimState == StudioAnimState::Shoot && !_botAnimator.isPlaying()) {
                    _botAnimator.playAnimation("Idle", true, 0.15f);
                    _activeAnimState = StudioAnimState::Idle;
                }
            }
        }

        _lastMouseX = mouseX;
        _lastMouseY = mouseY;
        _lastLmb = lmbPressed;

        if (!lmbPressed) {
            _activeSliderId = -1;
            _isDraggingScrubber = false;
        }
    }

    void CharacterStudio::render(int screenWidth, int screenHeight) {
        _screenWidth = screenWidth;
        _screenHeight = screenHeight;

        float leftBarW = 72.0f;
        float topBarsH = 52.0f;
        float sbH = 24.0f;
        float conH = std::clamp(static_cast<float>(screenHeight) * 0.16f, 100.0f, 160.0f);
        float bottomBarsH = conH + sbH;
        float rightPanelW = std::clamp(static_cast<float>(screenWidth) * 0.28f, 350.0f, 460.0f);

        int vpX = static_cast<int>(leftBarW);
        int vpBottomY = static_cast<int>(bottomBarsH);
        int vpW = screenWidth - static_cast<int>(leftBarW + rightPanelW);
        int vpH = screenHeight - static_cast<int>(topBarsH + bottomBarsH);

        // 1. Render 3D Scene in center viewport
        render3DScene(vpX, vpBottomY, vpW, vpH);

        // 2. Render Valve Hammer styled UI overlay
        renderHammerUI(screenWidth, screenHeight);
    }

    void CharacterStudio::render3DScene(int vpX, int vpY, int vpW, int vpH) {
        if (vpW <= 10 || vpH <= 10) return;

        // Direct Core Profile viewport setup
        glViewport(vpX, vpY, vpW, vpH);
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LESS);

        // Clear depth only for viewport area, preserving backdrop
        glClear(GL_DEPTH_BUFFER_BIT);

        // Setup orbital camera
        float radYaw = _turntableYaw * PI / 180.0f;
        float radPitch = _turntablePitch * PI / 180.0f;

        Vec3 camEye;
        camEye.x = _cameraTarget.x + _cameraDist * std::cos(radPitch) * std::sin(radYaw);
        camEye.y = _cameraTarget.y + _cameraDist * std::sin(radPitch);
        camEye.z = _cameraTarget.z + _cameraDist * std::cos(radPitch) * std::cos(radYaw);

        Camera studioCam(60.0f, static_cast<float>(vpW) / static_cast<float>(vpH), 0.01f, 100.0f);
        studioCam.setPosition(camEye);

        Vec3 camForward = (_cameraTarget - camEye).normalized();
        float yawDeg = std::atan2(camForward.x, camForward.z) * 180.0f / PI;
        float pitchDeg = std::asin(std::clamp(camForward.y, -1.0f, 1.0f)) * 180.0f / PI;
        studioCam.setYaw(yawDeg);
        studioCam.setPitch(pitchDeg);

        Renderer::beginFrame(studioCam);

        // 3-point Studio Lighting
        Renderer::setSunLight(Vec3(0.5f, -1.0f, 0.7f).normalized(), Vec3(1.2f, 1.15f, 1.1f), Vec3(0.38f, 0.40f, 0.45f));
        Renderer::setFog(false);

        // Draw Ground Grid if enabled
        if (_showGrid) {
            renderGroundGrid();
        }

        // Draw Content according to Tab
        if (_activeTab == StudioTab::GripPoser) {
            renderWeaponGripView();
        } else if (_activeTab == StudioTab::ReloadTimeline) {
            renderReloadTimelineView();
        } else if (_activeTab == StudioTab::WeaponSkins) {
            renderWeaponGripView();
        } else if (_activeTab == StudioTab::Appearance) {
            renderCharacterBody();
        } else if (_activeTab == StudioTab::FaceDialogue) {
            renderFaceDialogueView();
        }

        Renderer::endFrame();
    }

    void CharacterStudio::renderGroundGrid() {
        if (_gridVAO && _gridLineVertexCount > 0) {
            glBindVertexArray(_gridVAO);
            glDrawArrays(GL_LINES, 0, _gridLineVertexCount);
            glBindVertexArray(0);
        }
    }

    void CharacterStudio::renderSocketGizmo(const Vec3& pos, const Vec3& rot) {
        float axisLen = 0.045f;
        float axisThick = 0.004f;

        Vec3 fwd = rotateOffsetVec(Vec3(0, 0, -axisLen * 0.5f), rot);
        Vec3 up = rotateOffsetVec(Vec3(0, axisLen * 0.5f, 0), rot);
        Vec3 right = rotateOffsetVec(Vec3(axisLen * 0.5f, 0, 0), rot);

        // X: Red, Y: Green, Z: Blue
        Renderer::drawCube(pos + right, rot, Vec3(axisLen, axisThick, axisThick), Vec3(1.0f, 0.15f, 0.15f), nullptr, false);
        Renderer::drawCube(pos + up, rot, Vec3(axisThick, axisLen, axisThick), Vec3(0.2f, 0.95f, 0.2f), nullptr, false);
        Renderer::drawCube(pos + fwd, rot, Vec3(axisThick, axisThick, axisLen), Vec3(0.2f, 0.5f, 1.0f), nullptr, false);
    }

    void CharacterStudio::renderWeaponGripView() {
        const auto& grip = _weaponGrips[_selectedWeaponIndex];
        const auto& skin = _weaponSkins[_selectedWeaponIndex];

        WeaponID wid = static_cast<WeaponID>(_selectedWeaponIndex);
        Texture* skinTex = getTexture(skin.textureFile);

        // Model lookup (Custom skin STL override, or default weapon STL)
        Mesh* weaponMesh = nullptr;
        if (!skin.modelFile.empty()) {
            if (skin.modelFile != "NONE" && skin.modelFile != "PROCEDURAL") {
                weaponMesh = getMesh(skin.modelFile);
            }
        } else {
            const char* stlNames[9] = {
                "pipe.stl", "pistol.stl", "shotgun.stl",
                "m4a4s.stl", "sg553.stl", "minigun.stl",
                "plasma.stl", "railgun.stl", "rpg.stl"
            };
            weaponMesh = getMesh(stlNames[_selectedWeaponIndex]);
        }

        // Render Bot in Bot Weapon Socket mode
        if (_poserSubMode == PoserSubMode::BotWeaponSocket) {
            Vec3 botPos(0.0f, 0.0f, 0.0f);
            if (_botMesh && _botAnimator.getSkeleton()) {
                float botScale = _botMesh->getBaseScale(1.85f);
                float yOffset = -_botMesh->getMinBounds().y * botScale;
                Mat4 botModel = Mat4::translate(botPos + Vec3(0.0f, yOffset, 0.0f)) *
                                Mat4::scale(Vec3(botScale, botScale, botScale));
                Renderer::drawSkinnedMesh(*_botMesh, botModel, _botAnimator.getSkinMatrices(), Vec3(0.32f, 0.35f, 0.40f), nullptr, true);

                float invBotScale = (botScale > 0.00001f) ? (1.0f / botScale) : 1.0f;
                Vec3 sPos = grip.botSocket.offset * invBotScale;
                Quat sRot = Quat::fromEuler(grip.botSocket.rotation.x * PI / 180.0f,
                                            grip.botSocket.rotation.y * PI / 180.0f,
                                            grip.botSocket.rotation.z * PI / 180.0f);
                Vec3 sScale = Vec3(0.016f * grip.botSocket.scale.x * invBotScale,
                                   0.016f * grip.botSocket.scale.y * invBotScale,
                                   0.016f * grip.botSocket.scale.z * invBotScale);
                Mat4 weaponSocket = _botAnimator.getSocketTransform("Socket_Weapon", botModel,
                    makeTransform(sPos, sRot, sScale));

                if (weaponMesh) {
                    Renderer::drawMesh(*weaponMesh, weaponSocket, skin.tintColor, skinTex, true);
                } else {
                    Renderer::drawCube(Vec3(weaponSocket.m[12], weaponSocket.m[13], weaponSocket.m[14]),
                                       grip.botSocket.rotation, Vec3(0.06f, 0.12f, 0.55f), skin.tintColor, skinTex, true);
                }

                if (_showGizmos) {
                    Vec3 gizmoPos(weaponSocket.m[12], weaponSocket.m[13], weaponSocket.m[14]);
                    renderSocketGizmo(gizmoPos, grip.botSocket.rotation);
                }
            } else {
                renderCharacterBody();
                Vec3 rHandPos(0.28f, 0.88f, 0.0f);
                Vec3 finalWepPos = rHandPos + grip.botSocket.offset;
                Vec3 renderScale = { 0.45f * grip.botSocket.scale.x, 0.45f * grip.botSocket.scale.y, 0.45f * grip.botSocket.scale.z };
                if (weaponMesh) {
                    Renderer::drawMesh(*weaponMesh, finalWepPos, grip.botSocket.rotation, renderScale, skin.tintColor, skinTex, true);
                } else {
                    Renderer::drawCube(finalWepPos, grip.botSocket.rotation, Vec3(0.06f, 0.12f, 0.55f), skin.tintColor, skinTex, true);
                }
                if (_showGizmos) {
                    renderSocketGizmo(finalWepPos, grip.botSocket.rotation);
                }
            }
            return;
        }

        // Weapon Base Transform (including user offset, rotation and scale, plus active procedural animator kinematics!)
        Vec3 baseWepPos = grip.weaponOffset;
        Vec3 baseWepRot = grip.weaponRotation;
        Vec3 weaponPos = _animator ? _animator->calculatePositionOffset(baseWepPos) : baseWepPos;
        Vec3 weaponRot = _animator ? _animator->calculateRotationOffset(baseWepRot) : baseWepRot;
        Vec3 weaponScale = grip.weaponScale;

        // Render Weapon with active skin, tint and scale (normalized to 0.7m base human-hand size)
        if (weaponMesh) {
            float baseScale = weaponMesh->getBaseScale(0.70f);
            Vec3 renderScale = { baseScale * weaponScale.x, baseScale * weaponScale.y, baseScale * weaponScale.z };
            Renderer::drawMesh(*weaponMesh, weaponPos, weaponRot, renderScale, skin.tintColor, skinTex, true, skin.uvScale);
        } else {
            // High-detail procedural weapon fallback
            Renderer::drawCube(weaponPos, weaponRot, Vec3(0.06f * weaponScale.x, 0.12f * weaponScale.y, 0.65f * weaponScale.z), skin.tintColor, skinTex, true);
            Renderer::drawCube(weaponPos + Vec3(0.0f, -0.08f * weaponScale.y, -0.06f * weaponScale.z), weaponRot + Vec3(-12.0f, 0.0f, 0.0f), Vec3(0.045f * weaponScale.x, 0.11f * weaponScale.y, 0.05f * weaponScale.z), Vec3(0.12f, 0.13f, 0.15f), nullptr, true);
        }

        // Render Tactical Arms aligned to configured sockets!
        if (_arms && _animator) {
            Vec3 armsPos = grip.lockHands ? Vec3(0.0f, 0.0f, 0.0f) : baseWepPos;
            Vec3 armsRot = grip.lockHands ? Vec3(0.0f, 0.0f, 0.0f) : baseWepRot;
            _arms->render(armsPos, armsRot, wid, *_animator, nullptr,
                          &grip.rightSocketPos, &grip.rightSocketRot,
                          &grip.leftSocketPos, &grip.leftSocketRot);
        }

        // Render Socket & Weapon Origin Tripod Gizmos
        if (_showGizmos) {
            Vec3 armsPos = grip.lockHands ? Vec3(0.0f, 0.0f, 0.0f) : weaponPos;
            Vec3 armsRot = grip.lockHands ? Vec3(0.0f, 0.0f, 0.0f) : weaponRot;
            renderSocketGizmo(weaponPos, weaponRot);
            renderSocketGizmo(armsPos + grip.rightSocketPos, armsRot + grip.rightSocketRot);
            renderSocketGizmo(armsPos + grip.leftSocketPos, armsRot + grip.leftSocketRot);
        }
    }

    void CharacterStudio::renderReloadTimelineView() {
        const auto& grip = _weaponGrips[_selectedWeaponIndex];
        const auto& skin = _weaponSkins[_selectedWeaponIndex];
        WeaponID wid = static_cast<WeaponID>(_selectedWeaponIndex);
        Texture* skinTex = getTexture(skin.textureFile);

        Mesh* weaponMesh = nullptr;
        if (!skin.modelFile.empty()) {
            if (skin.modelFile != "NONE" && skin.modelFile != "PROCEDURAL") {
                weaponMesh = getMesh(skin.modelFile);
            }
        } else {
            const char* stlNames[9] = {
                "pipe.stl", "pistol.stl", "shotgun.stl",
                "m4a4s.stl", "sg553.stl", "minigun.stl",
                "plasma.stl", "railgun.stl", "rpg.stl"
            };
            weaponMesh = getMesh(stlNames[_selectedWeaponIndex]);
        }

        // Evaluate timeline curve at _scrubberPos [0.0 ... 1.0]
        float p = _scrubberPos;
        float dipY = 0.0f;
        float tiltRoll = 0.0f;
        float magY = 0.0f;
        bool magDetached = false;

        // Phase 1: Dip & Tilt (0.0 -> dipDuration)
        if (p < _reloadTimeline.dipDuration) {
            float phaseP = p / _reloadTimeline.dipDuration;
            float smooth = std::sin(phaseP * PI * 0.5f);
            dipY = -_reloadTimeline.dipDepth * smooth;
            tiltRoll = _reloadTimeline.tiltAngle * smooth;
        } else if (p < _reloadTimeline.boltRackTime) {
            dipY = -_reloadTimeline.dipDepth;
            tiltRoll = _reloadTimeline.tiltAngle;
        } else {
            // Phase 4: Recover to ready pose
            float recoverP = (p - _reloadTimeline.boltRackTime) / (1.0f - _reloadTimeline.boltRackTime);
            float smooth = 1.0f - std::sin(recoverP * PI * 0.5f);
            dipY = -_reloadTimeline.dipDepth * smooth;
            tiltRoll = _reloadTimeline.tiltAngle * smooth;
        }

        // Phase 2 & 3: Magazine choreography
        if (p >= _reloadTimeline.magDropTime && p < _reloadTimeline.magInsertTime) {
            magDetached = true;
            float magP = (p - _reloadTimeline.magDropTime) / (_reloadTimeline.magInsertTime - _reloadTimeline.magDropTime);
            magY = (magP < 0.5f) ? (-magP * 2.0f * 0.25f) : (-(1.0f - magP) * 2.0f * 0.25f);
        }

        Vec3 weaponPos = grip.weaponOffset + Vec3(0.0f, dipY, 0.0f);
        Vec3 weaponRot = grip.weaponRotation + Vec3(0.0f, 0.0f, tiltRoll);
        Vec3 weaponScale = grip.weaponScale;

        if (weaponMesh) {
            float baseScale = weaponMesh->getBaseScale(0.70f);
            Vec3 renderScale = { baseScale * weaponScale.x, baseScale * weaponScale.y, baseScale * weaponScale.z };
            Renderer::drawMesh(*weaponMesh, weaponPos, weaponRot, renderScale, skin.tintColor, skinTex, true, skin.uvScale);
        } else {
            Renderer::drawCube(weaponPos, weaponRot, Vec3(0.06f * weaponScale.x, 0.12f * weaponScale.y, 0.65f * weaponScale.z), skin.tintColor, skinTex, true);
        }

        // Draw fresh or dropping magazine
        if (magDetached) {
            Vec3 magPos = weaponPos + Vec3(0.0f, -0.15f + magY, -0.05f);
            Renderer::drawCube(magPos, weaponRot, Vec3(0.035f, 0.14f, 0.065f), Vec3(0.12f, 0.14f, 0.16f), nullptr, true);
        }

        // Arms following reload kinematics
        if (_arms && _animator) {
            Vec3 leftHandOffset = grip.leftSocketPos + Vec3(0.0f, magY * 0.7f, 0.0f);
            Vec3 armsPos = grip.lockHands ? Vec3(0.0f, dipY, 0.0f) : weaponPos;
            Vec3 armsRot = grip.lockHands ? Vec3(0.0f, 0.0f, tiltRoll) : weaponRot;
            _arms->render(armsPos, armsRot, wid, *_animator, nullptr,
                          &grip.rightSocketPos, &grip.rightSocketRot,
                          &leftHandOffset, &grip.leftSocketRot);
        }
    }

    void CharacterStudio::renderCharacterBody() {
        // Base turntable platform
        Renderer::drawCube(Vec3(0, 0.03f, 0), Vec3(0, 0, 0), Vec3(0.9f, 0.06f, 0.9f), Vec3(0.18f, 0.20f, 0.24f), nullptr, true);
        Renderer::drawCube(Vec3(0, 0.065f, 0), Vec3(0, 0, 0), Vec3(0.75f, 0.02f, 0.75f), Vec3(0.24f, 0.28f, 0.32f), nullptr, true);

        // Fatigues & Armor Colors
        Vec3 fatCol = _appearance.fatiguesColor;
        Vec3 plateCol = _appearance.armorPlateColor;
        Vec3 bootCol = Vec3(0.12f, 0.13f, 0.14f);

        // 1. Legs & Combat Boots
        Renderer::drawCube(Vec3(-0.16f, 0.45f, 0.0f), Vec3(0, 0, 0), Vec3(0.14f, 0.75f, 0.15f), fatCol, nullptr, true);
        Renderer::drawCube(Vec3(0.16f, 0.45f, 0.0f), Vec3(0, 0, 0), Vec3(0.14f, 0.75f, 0.15f), fatCol, nullptr, true);
        // Knee armor pads
        Renderer::drawCube(Vec3(-0.16f, 0.42f, 0.08f), Vec3(0, 0, 0), Vec3(0.12f, 0.14f, 0.05f), plateCol, nullptr, true);
        Renderer::drawCube(Vec3(0.16f, 0.42f, 0.08f), Vec3(0, 0, 0), Vec3(0.12f, 0.14f, 0.05f), plateCol, nullptr, true);
        // Boots
        Renderer::drawCube(Vec3(-0.16f, 0.10f, 0.04f), Vec3(0, 0, 0), Vec3(0.15f, 0.16f, 0.24f), bootCol, nullptr, true);
        Renderer::drawCube(Vec3(0.16f, 0.10f, 0.04f), Vec3(0, 0, 0), Vec3(0.15f, 0.16f, 0.24f), bootCol, nullptr, true);

        // 2. Torso & Tactical Vest
        Renderer::drawCube(Vec3(0, 1.15f, 0), Vec3(0, 0, 0), Vec3(0.38f, 0.58f, 0.22f), fatCol, nullptr, true);
        // Armor chest plate
        float vestThick = (_appearance.armorClass == ArmorClass::LightScout) ? 0.04f :
                          (_appearance.armorClass == ArmorClass::CryoMarine) ? 0.07f : 0.09f;
        Renderer::drawCube(Vec3(0, 1.18f, 0.11f + vestThick * 0.5f), Vec3(0, 0, 0), Vec3(0.34f, 0.42f, vestThick), plateCol, nullptr, true);

        // 3. Arms & Tactical Sleeves
        Renderer::drawCube(Vec3(-0.28f, 1.12f, 0.0f), Vec3(0, 0, 14.0f), Vec3(0.12f, 0.54f, 0.13f), fatCol, nullptr, true);
        Renderer::drawCube(Vec3(0.28f, 1.12f, 0.0f), Vec3(0, 0, -14.0f), Vec3(0.12f, 0.54f, 0.13f), fatCol, nullptr, true);
        // Shoulder Pauldrons
        Renderer::drawCube(Vec3(-0.30f, 1.34f, 0.0f), Vec3(0, 0, 18.0f), Vec3(0.14f, 0.12f, 0.16f), plateCol, nullptr, true);
        Renderer::drawCube(Vec3(0.30f, 1.34f, 0.0f), Vec3(0, 0, -18.0f), Vec3(0.14f, 0.12f, 0.16f), plateCol, nullptr, true);

        // 4. Head & Helmet/Visor
        Vec3 headPos(0.0f, 1.58f, 0.0f);
        if (_facialHead) {
            _facialHead->evaluate();
            _facialHead->draw();
        }

        // Helmet Options
        if (_appearance.helmetType == HelmetType::CombatVisor) {
            // Armored Skullcap + Glowing HUD Visor
            Renderer::drawCube(headPos + Vec3(0, 0.12f, 0), Vec3(0, 0, 0), Vec3(0.24f, 0.15f, 0.26f), plateCol, nullptr, true);
            Renderer::drawCube(headPos + Vec3(0, 0.04f, 0.15f), Vec3(0, 0, 0), Vec3(0.21f, 0.07f, 0.06f), _appearance.visorGlowColor, nullptr, false);
        } else if (_appearance.helmetType == HelmetType::SealedHelmet) {
            // Cryo Marine sealed environmental enclosure
            Renderer::drawCube(headPos + Vec3(0, 0.06f, 0), Vec3(0, 0, 0), Vec3(0.28f, 0.32f, 0.30f), plateCol, nullptr, true);
            Renderer::drawCube(headPos + Vec3(0, 0.05f, 0.15f), Vec3(0, 0, 0), Vec3(0.22f, 0.10f, 0.06f), _appearance.visorGlowColor, nullptr, false);
        } else {
            // Tactical Comm Beanie
            Renderer::drawCube(headPos + Vec3(0, 0.14f, 0), Vec3(0, 0, 0), Vec3(0.25f, 0.14f, 0.25f), Vec3(0.14f, 0.15f, 0.16f), nullptr, true);
            Renderer::drawCube(headPos + Vec3(-0.13f, 0.02f, 0), Vec3(0, 0, 0), Vec3(0.04f, 0.08f, 0.06f), Vec3(0.22f, 0.24f, 0.26f), nullptr, true);
        }
    }

    void CharacterStudio::renderFaceDialogueView() {
        // Close-up on 3D Combat Head
        Vec3 headPos(0.0f, 1.58f, 0.0f);
        if (_facialHead) {
            _facialHead->evaluate();
            _facialHead->draw();
        }

        // Visor glow
        if (_appearance.helmetType == HelmetType::CombatVisor) {
            Renderer::drawCube(headPos + Vec3(0, 0.12f, 0), Vec3(0, 0, 0), Vec3(0.24f, 0.15f, 0.26f), _appearance.armorPlateColor, nullptr, true);
            Renderer::drawCube(headPos + Vec3(0, 0.04f, 0.15f), Vec3(0, 0, 0), Vec3(0.21f, 0.07f, 0.06f), _appearance.visorGlowColor, nullptr, false);
        }
    }

    // =========================================================================
    // Valve Hammer Editor 2D UI Components & Styling
    // =========================================================================

    void CharacterStudio::drawHammerBevel(float x, float y, float w, float h, bool sunken) {
        Vec3 light = sunken ? Vec3(0.12f, 0.13f, 0.15f) : Vec3(0.44f, 0.46f, 0.50f);
        Vec3 dark  = sunken ? Vec3(0.44f, 0.46f, 0.50f) : Vec3(0.12f, 0.13f, 0.15f);

        // Top line
        Renderer::drawRect(x, y, w, 1.0f, light);
        // Left line
        Renderer::drawRect(x, y, 1.0f, h, light);
        // Bottom line
        Renderer::drawRect(x, y + h - 1.0f, w, 1.0f, dark);
        // Right line
        Renderer::drawRect(x + w - 1.0f, y, 1.0f, h, dark);
    }

    void CharacterStudio::drawHammerPanel(float x, float y, float w, float h, const std::string& title) {
        Vec3 bgCol(0.23f, 0.24f, 0.26f);
        Renderer::drawRect(x, y, w, h, bgCol);
        drawHammerBevel(x, y, w, h, false);

        if (!title.empty()) {
            Vec3 titleBg(0.18f, 0.28f, 0.40f);
            Renderer::drawRect(x + 2.0f, y + 2.0f, w - 4.0f, 20.0f, titleBg);
            LabFont::drawText(x + 8.0f, y + 5.0f, title, 1.5f, Vec3(1.0f, 1.0f, 1.0f), LabFontType::System);
        }
    }

    bool CharacterStudio::drawHammerButton(float x, float y, float w, float h, const std::string& label, bool active, bool highlighted) {
        bool hovered = (_mouseX >= x && _mouseX <= x + w && _mouseY >= y && _mouseY <= y + h);
        bool clicked = false;
        if (hovered && _lmbClicked) {
            clicked = true;
            _lmbClicked = false; // Consumed to prevent clicking elements behind
        }

        Vec3 bgCol = active ? Vec3(0.85f, 0.48f, 0.10f) :
                     (hovered ? (highlighted ? Vec3(0.32f, 0.55f, 0.35f) : Vec3(0.35f, 0.38f, 0.42f)) :
                                (highlighted ? Vec3(0.24f, 0.42f, 0.26f) : Vec3(0.28f, 0.29f, 0.32f)));

        Renderer::drawRect(x, y, w, h, bgCol);
        drawHammerBevel(x, y, w, h, active);

        Vec3 txtCol = active ? Vec3(1.0f, 1.0f, 1.0f) : (hovered ? Vec3(1.0f, 0.95f, 0.7f) : Vec3(0.9f, 0.92f, 0.95f));
        float textX = x + 8.0f;
        float textY = y + (h - 14.0f) * 0.5f;
        LabFont::drawText(textX, textY, label, 1.4f, txtCol, LabFontType::System);

        return clicked;
    }

    bool CharacterStudio::drawHammerSlider(float x, float y, float w, float h, const std::string& label, float& value, float minVal, float maxVal, const std::string& format) {
        float labelW = 120.0f;
        float trackX = x + labelW;
        float trackW = w - labelW - 55.0f;
        float trackY = y + 5.0f;
        float trackH = h - 10.0f;

        // Label
        LabFont::drawText(x, y + 4.0f, label, 1.4f, Vec3(0.85f, 0.88f, 0.92f), LabFontType::System);

        // Sunken Track
        Renderer::drawRect(trackX, trackY, trackW, trackH, Vec3(0.14f, 0.15f, 0.17f));
        drawHammerBevel(trackX, trackY, trackW, trackH, true);

        // Filled bar portion
        float norm = std::clamp((value - minVal) / (maxVal - minVal), 0.0f, 1.0f);
        Renderer::drawRect(trackX + 1.0f, trackY + 1.0f, (trackW - 2.0f) * norm, trackH - 2.0f, Vec3(0.22f, 0.52f, 0.72f));

        // Thumb handle
        float thumbX = trackX + (trackW - 8.0f) * norm;
        Renderer::drawRect(thumbX, trackY - 2.0f, 8.0f, trackH + 4.0f, Vec3(0.80f, 0.82f, 0.85f));
        drawHammerBevel(thumbX, trackY - 2.0f, 8.0f, trackH + 4.0f, false);

        // Value readout
        char buf[32];
        snprintf(buf, sizeof(buf), format.c_str(), value);
        LabFont::drawText(trackX + trackW + 8.0f, y + 4.0f, buf, 1.4f, Vec3(0.95f, 0.80f, 0.25f), LabFontType::System);

        // Mouse Drag interaction
        bool changed = false;
        int sliderId = static_cast<int>(y * 1000.0f + x);
        if (_lmbClicked && _mouseX >= trackX && _mouseX <= trackX + trackW && _mouseY >= y && _mouseY <= y + h) {
            _activeSliderId = sliderId;
            pushUndoState();
            float newNorm = std::clamp((_mouseX - trackX) / trackW, 0.0f, 1.0f);
            value = minVal + newNorm * (maxVal - minVal);
            changed = true;
            _lmbClicked = false;
        } else if (_lmbPressed && _activeSliderId == sliderId) {
            float newNorm = std::clamp((_mouseX - trackX) / trackW, 0.0f, 1.0f);
            value = minVal + newNorm * (maxVal - minVal);
            changed = true;
        }

        return changed;
    }

    void CharacterStudio::renderHammerTopMenuBar(float w) {
        float menuH = 24.0f;
        Vec3 menuBg(0.24f, 0.25f, 0.27f);
        Renderer::drawRect(0, 0, w, menuH, menuBg);
        drawHammerBevel(0, 0, w, menuH, false);

        struct MenuBtn { std::string name; float x; float w; DropdownMenu menu; };
        MenuBtn items[] = {
            { "File", 6.0f, 44.0f, DropdownMenu::File },
            { "Edit", 52.0f, 44.0f, DropdownMenu::Edit },
            { "View", 98.0f, 44.0f, DropdownMenu::View },
            { "Tools", 144.0f, 48.0f, DropdownMenu::Tools },
            { "Help", 194.0f, 44.0f, DropdownMenu::Help }
        };

        for (int i = 0; i < 5; ++i) {
            bool hov = (_mouseX >= items[i].x && _mouseX <= items[i].x + items[i].w && _mouseY >= 2.0f && _mouseY <= 22.0f);
            bool isActive = (_activeDropdown == items[i].menu);

            // Hover switching between open menus (Valve Hammer / Windows standard behavior)
            if (hov && _activeDropdown != DropdownMenu::None && _activeDropdown != items[i].menu) {
                _activeDropdown = items[i].menu;
                isActive = true;
            }

            // Click toggles menu
            if (hov && _lmbClicked) {
                _activeDropdown = isActive ? DropdownMenu::None : items[i].menu;
                isActive = (_activeDropdown == items[i].menu);
                _lmbClicked = false; // Consumed!
            }

            if (isActive) {
                Renderer::drawRect(items[i].x, 2.0f, items[i].w, 20.0f, Vec3(0.18f, 0.28f, 0.42f));
                drawHammerBevel(items[i].x, 2.0f, items[i].w, 20.0f, true);
            } else if (hov) {
                Renderer::drawRect(items[i].x, 2.0f, items[i].w, 20.0f, Vec3(0.35f, 0.38f, 0.42f));
                drawHammerBevel(items[i].x, 2.0f, items[i].w, 20.0f, false);
            }

            Vec3 txtCol = isActive ? Vec3(1.0f, 1.0f, 1.0f) : (hov ? Vec3(1.0f, 0.95f, 0.7f) : Vec3(0.95f, 0.95f, 0.95f));
            LabFont::drawText(items[i].x + 8.0f, 5.0f, items[i].name, 1.4f, txtCol, LabFontType::System);
        }

        // Title Tag
        LabFont::drawText(std::max(250.0f, w - 420.0f), 5.0f, "Lab Character & Weapon Studio - [Lab Studio 2026]", 1.4f, Vec3(0.85f, 0.88f, 0.92f), LabFontType::System);
    }

    void CharacterStudio::renderHammerDropdownMenus(float w, float h) {
        (void)w;
        (void)h;
        if (_activeDropdown == DropdownMenu::None) return;

        struct DropdownItem {
            std::string text;
            std::string shortcut;
            bool isSeparator = false;
            bool isChecked = false;
            bool isCheckable = false;
            std::function<void()> onSelect;
        };

        std::vector<DropdownItem> items;
        float menuX = 6.0f;
        float menuY = 24.0f;
        float menuW = 230.0f;

        if (_activeDropdown == DropdownMenu::File) {
            menuX = 6.0f;
            items.push_back({ "New Preset", "", false, false, false, [this]() {
                pushUndoState();
                resetDefaults();
                log("New preset initialized with default settings.");
            }});
            items.push_back({ "Open Config...", "Ctrl+O", false, false, false, [this]() {
                std::string path = LabDialogs::openFileDialog(_window, "Studio Config Files (*.cfg)\0*.cfg\0All Files (*.*)\0*.*\0", "assets\\configs");
                if (!path.empty()) {
                    pushUndoState();
                    loadConfig(path);
                }
            }});
            items.push_back({ "Save Config", "Ctrl+S", false, false, false, [this]() {
                saveConfig("assets/configs/character_studio.cfg");
            }});
            items.push_back({ "Save Config As...", "", false, false, false, [this]() {
                std::string path = LabDialogs::saveFileDialog(_window, "Studio Config Files (*.cfg)\0*.cfg\0All Files (*.*)\0*.*\0", "character_studio.cfg", "assets\\configs");
                if (!path.empty()) {
                    saveConfig(path);
                }
            }});
            items.push_back({ "", "", true, false, false, nullptr }); // Separator
            items.push_back({ "Reset Factory Defaults", "", false, false, false, [this]() {
                pushUndoState();
                resetDefaults();
                log("Reset all character and weapon parameters to factory defaults.");
            }});
            items.push_back({ "", "", true, false, false, nullptr }); // Separator
            items.push_back({ "Exit Studio", "Esc", false, false, false, [this]() {
                _requestExit = true;
                log("Exiting Studio session...");
            }});
        } else if (_activeDropdown == DropdownMenu::Edit) {
            menuX = 52.0f;
            items.push_back({ "Undo", "Ctrl+Z", false, false, false, [this]() { undo(); }});
            items.push_back({ "Redo", "Ctrl+Y", false, false, false, [this]() { redo(); }});
            items.push_back({ "", "", true, false, false, nullptr }); // Separator
            items.push_back({ "Copy Grip & Transforms", "Ctrl+C", false, false, false, [this]() { copyGrip(); }});
            items.push_back({ "Paste Grip & Transforms", "Ctrl+V", false, false, false, [this]() { pasteGrip(); }});
            items.push_back({ "", "", true, false, false, nullptr }); // Separator
            items.push_back({ "Center Weapon Model", "", false, false, false, [this]() { centerActiveWeapon(); }});
            items.push_back({ "Reset Weapon Rotation", "", false, false, false, [this]() { resetActiveWeaponRotation(); }});
            items.push_back({ "Reset Weapon Scale", "", false, false, false, [this]() { resetActiveWeaponScale(); }});
            items.push_back({ "Reset All Transforms", "", false, false, false, [this]() { resetActiveWeaponAllTransforms(); }});
            items.push_back({ "Reset Active Grip", "", false, false, false, [this]() {
                pushUndoState();
                _weaponGrips[_selectedWeaponIndex] = WeaponGripConfig{};
                log("Reset active weapon grip sockets.");
            }});
        } else if (_activeDropdown == DropdownMenu::View) {
            menuX = 98.0f;
            items.push_back({ "Reset Camera Pose", "Home", false, false, false, [this]() { resetCamera(); }});
            items.push_back({ "Ground Reference Grid", "G", false, _showGrid, true, [this]() {
                _showGrid = !_showGrid;
                log(_showGrid ? "Ground grid ON" : "Ground grid OFF");
            }});
            items.push_back({ "Socket Tripod Gizmos", "Z", false, _showGizmos, true, [this]() {
                _showGizmos = !_showGizmos;
                log(_showGizmos ? "Gizmos ON" : "Gizmos OFF");
            }});
            items.push_back({ "Turntable 360 Rotate", "R", false, _turntableAutoRotate, true, [this]() {
                _turntableAutoRotate = !_turntableAutoRotate;
            }});
            items.push_back({ "", "", true, false, false, nullptr }); // Separator
            items.push_back({ "Weapon Grip Poser", "1", false, _activeTab == StudioTab::GripPoser, true, [this]() {
                _activeTab = StudioTab::GripPoser; resetCamera();
            }});
            items.push_back({ "Reload Timeline", "2", false, _activeTab == StudioTab::ReloadTimeline, true, [this]() {
                _activeTab = StudioTab::ReloadTimeline; resetCamera();
            }});
            items.push_back({ "Weapon Materials", "3", false, _activeTab == StudioTab::WeaponSkins, true, [this]() {
                _activeTab = StudioTab::WeaponSkins; resetCamera();
            }});
            items.push_back({ "Operative Outfits", "4", false, _activeTab == StudioTab::Appearance, true, [this]() {
                _activeTab = StudioTab::Appearance; resetCamera();
            }});
            items.push_back({ "Facial Lip-Sync", "5", false, _activeTab == StudioTab::FaceDialogue, true, [this]() {
                _activeTab = StudioTab::FaceDialogue; resetCamera();
            }});
        } else if (_activeDropdown == DropdownMenu::Tools) {
            menuX = 144.0f;
            items.push_back({ "Tool: Move Weapon", "W", false, _viewportToolMode == ViewportToolMode::MoveWeapon, true, [this]() {
                _viewportToolMode = ViewportToolMode::MoveWeapon;
            }});
            items.push_back({ "Tool: Rotate Weapon", "E", false, _viewportToolMode == ViewportToolMode::RotateWeapon, true, [this]() {
                _viewportToolMode = ViewportToolMode::RotateWeapon;
            }});
            items.push_back({ "Tool: Scale Weapon", "T", false, _viewportToolMode == ViewportToolMode::ScaleWeapon, true, [this]() {
                _viewportToolMode = ViewportToolMode::ScaleWeapon;
            }});
            items.push_back({ "Tool: Orbit Camera", "Q", false, _viewportToolMode == ViewportToolMode::OrbitCamera, true, [this]() {
                _viewportToolMode = ViewportToolMode::OrbitCamera;
            }});
            items.push_back({ "", "", true, false, false, nullptr }); // Separator
            items.push_back({ "Audition Reload SFX", "", false, false, false, [this]() {
                AudioEngine::playSound(SoundID::Reload, 1.0f, 1.0f);
                log("Auditioned active reload sound FX.");
            }});
            items.push_back({ "Test Dialogue Speech", "Space", false, _dialoguePlaying, true, [this]() {
                _dialoguePlaying = !_dialoguePlaying;
                if (_dialoguePlaying) {
                    _dialogueTimer = 0.0f;
                    log("Playing dialogue test.");
                }
            }});
            items.push_back({ "Toggle Scrubber Play", "", false, _scrubberPlaying, true, [this]() {
                _scrubberPlaying = !_scrubberPlaying;
            }});
            items.push_back({ "", "", true, false, false, nullptr }); // Separator
            items.push_back({ "Center Weapon Sockets", "", false, false, false, [this]() {
                pushUndoState();
                auto& g = _weaponGrips[_selectedWeaponIndex];
                g.rightSocketPos = Vec3(0, 0, 0);
                g.leftSocketPos = Vec3(0, 0, 0);
                log("Centered weapon socket offsets.");
            }});
        } else if (_activeDropdown == DropdownMenu::Help) {
            menuX = 194.0f;
            items.push_back({ "Studio Shortcuts", "", false, false, false, [this]() {
                log("--- KEYBOARD SHORTCUTS ---");
                log("Ctrl+S: Save Config | Ctrl+O: Open Config | Ctrl+Z: Undo | Ctrl+Y: Redo");
                log("Ctrl+C: Copy Grip   | Ctrl+V: Paste Grip");
                log("Home: Reset Cam     | G: Grid | Z: Gizmos | R: Rotate | 1-5: Tabs");
            }});
            items.push_back({ "About Hammer Studio", "", false, false, false, [this]() {
                log("Lab Character & Weapon Studio v2.0.");
                log("Strict Core Profile OpenGL 4.5 | Responsive DPI Scaling.");
            }});
        }

        // Calculate total popup height
        float totalH = 6.0f;
        for (const auto& it : items) {
            totalH += it.isSeparator ? 8.0f : 22.0f;
        }

        // Handle click outside menu: closes dropdown
        if (_lmbClicked) {
            bool inMenu = (_mouseX >= menuX && _mouseX <= menuX + menuW && _mouseY >= menuY && _mouseY <= menuY + totalH);
            bool inTopBar = (_mouseY >= 0.0f && _mouseY <= 24.0f && _mouseX <= 250.0f);
            if (!inMenu && !inTopBar) {
                _activeDropdown = DropdownMenu::None;
                _lmbClicked = false; // Consumed outside click!
                return;
            }
        }

        // Drop shadow
        Renderer::drawRect(menuX + 4.0f, menuY + 4.0f, menuW, totalH, Vec3(0.08f, 0.09f, 0.11f));

        // Background & bevel
        Renderer::drawRect(menuX, menuY, menuW, totalH, Vec3(0.24f, 0.25f, 0.27f));
        drawHammerBevel(menuX, menuY, menuW, totalH, false);

        // Render each item
        float curItemY = menuY + 3.0f;
        for (const auto& it : items) {
            if (it.isSeparator) {
                Renderer::drawRect(menuX + 4.0f, curItemY + 3.0f, menuW - 8.0f, 1.0f, Vec3(0.14f, 0.15f, 0.17f));
                Renderer::drawRect(menuX + 4.0f, curItemY + 4.0f, menuW - 8.0f, 1.0f, Vec3(0.38f, 0.40f, 0.44f));
                curItemY += 8.0f;
            } else {
                float itemH = 22.0f;
                bool hovered = (_mouseX >= menuX + 2.0f && _mouseX <= menuX + menuW - 2.0f && _mouseY >= curItemY && _mouseY <= curItemY + itemH);

                if (hovered) {
                    Renderer::drawRect(menuX + 2.0f, curItemY, menuW - 4.0f, itemH, Vec3(0.20f, 0.45f, 0.78f));
                    drawHammerBevel(menuX + 2.0f, curItemY, menuW - 4.0f, itemH, false);
                }

                Vec3 txtCol = hovered ? Vec3(1.0f, 1.0f, 1.0f) : Vec3(0.90f, 0.92f, 0.95f);
                if (it.isCheckable) {
                    std::string checkStr = it.isChecked ? "[X]" : "[ ]";
                    Vec3 checkCol = it.isChecked ? (hovered ? Vec3(1, 1, 1) : Vec3(0.4f, 0.95f, 0.4f)) : Vec3(0.6f, 0.6f, 0.6f);
                    LabFont::drawText(menuX + 8.0f, curItemY + 4.0f, checkStr, 1.3f, checkCol, LabFontType::System);
                    LabFont::drawText(menuX + 32.0f, curItemY + 4.0f, it.text, 1.4f, txtCol, LabFontType::System);
                } else {
                    LabFont::drawText(menuX + 16.0f, curItemY + 4.0f, it.text, 1.4f, txtCol, LabFontType::System);
                }

                if (!it.shortcut.empty()) {
                    LabFont::drawText(menuX + menuW - 65.0f, curItemY + 4.0f, it.shortcut, 1.3f, hovered ? Vec3(0.9f, 0.9f, 0.9f) : Vec3(0.65f, 0.68f, 0.72f), LabFontType::System);
                }

                if (hovered && _lmbClicked) {
                    if (it.onSelect) {
                        it.onSelect();
                    }
                    _activeDropdown = DropdownMenu::None;
                    _lmbClicked = false; // Consumed!
                }

                curItemY += itemH;
            }
        }
    }

    void CharacterStudio::renderHammerToolbar(float w) {
        float tbY = 24.0f;
        float tbH = 28.0f;
        Vec3 tbBg(0.22f, 0.23f, 0.25f);
        Renderer::drawRect(0, tbY, w, tbH, tbBg);
        drawHammerBevel(0, tbY, w, tbH, false);

        float btnX = 8.0f;
        if (drawHammerButton(btnX, tbY + 2.0f, 52.0f, 24.0f, "SAVE")) {
            saveConfig("assets/configs/character_studio.cfg");
        }
        btnX += 56.0f;
        if (drawHammerButton(btnX, tbY + 2.0f, 52.0f, 24.0f, "LOAD")) {
            loadConfig("assets/configs/character_studio.cfg");
        }
        btnX += 56.0f;
        if (drawHammerButton(btnX, tbY + 2.0f, 58.0f, 24.0f, "RESET")) {
            resetDefaults();
            log("All parameters reset to factory defaults.");
        }
        btnX += 66.0f;

        // Separator
        Renderer::drawRect(btnX, tbY + 4.0f, 2.0f, 20.0f, Vec3(0.14f, 0.15f, 0.17f));
        btnX += 8.0f;

        if (drawHammerButton(btnX, tbY + 2.0f, 56.0f, 24.0f, "GRID", _showGrid)) {
            _showGrid = !_showGrid;
            log(_showGrid ? "Ground Reference Grid enabled." : "Ground Reference Grid disabled.");
        }
        btnX += 60.0f;
        if (drawHammerButton(btnX, tbY + 2.0f, 62.0f, 24.0f, "GIZMOS", _showGizmos)) {
            _showGizmos = !_showGizmos;
            log(_showGizmos ? "Socket Tripod Gizmos shown." : "Socket Tripod Gizmos hidden.");
        }
        btnX += 66.0f;
        if (drawHammerButton(btnX, tbY + 2.0f, 66.0f, 24.0f, "ROTATE", _turntableAutoRotate)) {
            _turntableAutoRotate = !_turntableAutoRotate;
        }
        btnX += 70.0f;
        if (drawHammerButton(btnX, tbY + 2.0f, 62.0f, 24.0f, "CAMERA")) {
            resetCamera();
        }
        btnX += 66.0f;

        // Separator
        Renderer::drawRect(btnX, tbY + 4.0f, 2.0f, 20.0f, Vec3(0.14f, 0.15f, 0.17f));
        btnX += 8.0f;

        // Viewport Tool Modes (Hammer / 3D Transform Tools)
        if (drawHammerButton(btnX, tbY + 2.0f, 54.0f, 24.0f, "ORBIT", _viewportToolMode == ViewportToolMode::OrbitCamera)) {
            _viewportToolMode = ViewportToolMode::OrbitCamera;
            log("Viewport Mode: Camera Orbit 360 [Q]");
        }
        btnX += 58.0f;
        if (drawHammerButton(btnX, tbY + 2.0f, 52.0f, 24.0f, "MOVE", _viewportToolMode == ViewportToolMode::MoveWeapon)) {
            _viewportToolMode = ViewportToolMode::MoveWeapon;
            log("Viewport Mode: Move Weapon [W] (LMB drag X/Y, Wheel Z)");
        }
        btnX += 56.0f;
        if (drawHammerButton(btnX, tbY + 2.0f, 52.0f, 24.0f, "ROT", _viewportToolMode == ViewportToolMode::RotateWeapon)) {
            _viewportToolMode = ViewportToolMode::RotateWeapon;
            log("Viewport Mode: Rotate Weapon [E] (LMB drag Pitch/Yaw, Wheel Roll)");
        }
        btnX += 56.0f;
        if (drawHammerButton(btnX, tbY + 2.0f, 56.0f, 24.0f, "SCALE", _viewportToolMode == ViewportToolMode::ScaleWeapon)) {
            _viewportToolMode = ViewportToolMode::ScaleWeapon;
            log("Viewport Mode: Scale Weapon [T] (LMB drag / Wheel scale)");
        }
        btnX += 62.0f;

        auto& curGrip = _weaponGrips[_selectedWeaponIndex];
        std::string lockLabel = curGrip.lockHands ? "HANDS: LOCKED [L]" : "HANDS: FREE [L]";
        if (drawHammerButton(btnX, tbY + 2.0f, 116.0f, 24.0f, lockLabel, curGrip.lockHands)) {
            pushUndoState();
            curGrip.lockHands = !curGrip.lockHands;
            log(curGrip.lockHands 
                ? "Lock Hands ON: Moving weapon will NOT move character hands." 
                : "Lock Hands OFF: Weapon and hands move together.");
        }
    }

    void CharacterStudio::renderHammerLeftToolPalette(float h) {
        float barW = 72.0f;
        float barY = 52.0f;
        float conH = std::clamp(h * 0.16f, 100.0f, 160.0f);
        float barH = h - barY - conH - 24.0f;

        Vec3 barBg(0.22f, 0.23f, 0.25f);
        Renderer::drawRect(0, barY, barW, barH, barBg);
        drawHammerBevel(0, barY, barW, barH, false);

        struct ToolTab { StudioTab tab; std::string code; std::string name; };
        ToolTab tools[] = {
            { StudioTab::GripPoser, "GRIP", "Grip Poser" },
            { StudioTab::ReloadTimeline, "RELOAD", "Timeline" },
            { StudioTab::WeaponSkins, "SKINS", "Materials" },
            { StudioTab::Appearance, "BODY", "Outfits" },
            { StudioTab::FaceDialogue, "FACE", "Lip-Sync" }
        };

        for (int i = 0; i < 5; ++i) {
            float btnY = barY + 8.0f + i * 54.0f;
            bool active = (_activeTab == tools[i].tab);

            if (drawHammerButton(6.0f, btnY, 60.0f, 48.0f, tools[i].code, active)) {
                _activeTab = tools[i].tab;
                if (_activeTab == StudioTab::FaceDialogue) {
                    _cameraDist = 0.85f;
                    _cameraTarget = Vec3(0.0f, 1.62f, 0.0f);
                } else if (_activeTab == StudioTab::Appearance) {
                    _cameraDist = 2.4f;
                    _cameraTarget = Vec3(0.0f, 1.0f, 0.0f);
                } else {
                    _cameraDist = 1.2f;
                    _cameraTarget = Vec3(0.0f, 0.0f, 0.0f);
                }
                log("Switched to Tab: " + tools[i].name);
            }
        }
    }

    void CharacterStudio::renderHammerConsole(float conX, float conY, float conW, float conH) {
        drawHammerPanel(conX, conY, conW, conH, "Studio Diagnostics & Asset Pipeline Log");

        float logStartY = conY + 28.0f;
        for (size_t i = 0; i < _consoleLogs.size(); ++i) {
            Vec3 logCol = (i == _consoleLogs.size() - 1) ? Vec3(0.95f, 0.85f, 0.35f) : Vec3(0.78f, 0.82f, 0.88f);
            LabFont::drawText(conX + 12.0f, logStartY + i * 15.0f, "> " + _consoleLogs[i], 1.4f, logCol, LabFontType::System);
        }
    }

    void CharacterStudio::renderHammerStatusBar(float w, float h) {
        float sbY = h - 24.0f;
        Vec3 sbBg(0.20f, 0.21f, 0.23f);
        Renderer::drawRect(0, sbY, w, 24.0f, sbBg);
        drawHammerBevel(0, sbY, w, 24.0f, false);

        const char* weaponNames[9] = {
            "Lead Pipe", "Tactical Pistol", "Combat Shotgun",
            "M4A4-S Tactical", "SG553 Rifle", "Rotary Minigun",
            "Plasma Repeater", "Kinetic Railgun", "RPG Launcher"
        };
        const auto& grip = _weaponGrips[_selectedWeaponIndex];

        char statusBuf[256];
        snprintf(statusBuf, sizeof(statusBuf),
                 "Weapon: %s | Hands: %s | Offset: (%.2f, %.2f, %.2f) | Right: (%.2f, %.2f) | Left: (%.2f, %.2f)",
                 weaponNames[_selectedWeaponIndex],
                 grip.lockHands ? "LOCKED [L]" : "LINKED [L]",
                 grip.weaponOffset.x, grip.weaponOffset.y, grip.weaponOffset.z,
                 grip.rightSocketPos.x, grip.rightSocketPos.y,
                 grip.leftSocketPos.x, grip.leftSocketPos.y);

        LabFont::drawText(10.0f, sbY + 5.0f, statusBuf, 1.4f, Vec3(0.85f, 0.88f, 0.92f), LabFontType::System);
        LabFont::drawText(w - 180.0f, sbY + 5.0f, "OpenGL 4.5 Core | PASS", 1.4f, Vec3(0.35f, 0.90f, 0.45f), LabFontType::System);
    }

    // =========================================================================
    // Inspector Tabs Implementation
    // =========================================================================

    void CharacterStudio::renderTabGripPoser(float x, float y, float w, float h) {
        (void)h;
        float curY = y + 26.0f;

        // Weapon Selector (9 weapon buttons in 3 rows)
        LabFont::drawText(x + 10.0f, curY, "1. SELECT ARSENAL WEAPON:", 1.5f, Vec3(0.95f, 0.85f, 0.3f), LabFontType::System);
        curY += 20.0f;

        const char* shortNames[9] = {
            "PIPE", "PISTOL", "SHOTGUN",
            "M4A4-S", "SG553", "MINIGUN",
            "PLASMA", "RAILGUN", "RPG"
        };
        float colPad = 6.0f;
        float btnW = (w - 20.0f - 2.0f * colPad) / 3.0f;
        for (int i = 0; i < 9; ++i) {
            float bx = x + 10.0f + (i % 3) * (btnW + colPad);
            float by = curY + (i / 3) * 26.0f;
            if (drawHammerButton(bx, by, btnW, 22.0f, shortNames[i], _selectedWeaponIndex == i)) {
                _selectedWeaponIndex = i;
                log(std::string("Loaded grip & skin profile for: ") + shortNames[i]);
            }
        }
        curY += 84.0f;

        auto& grip = _weaponGrips[_selectedWeaponIndex];

        // Sub-mode selector bar
        float subPad = 3.0f;
        float subW = (w - 20.0f - 3.0f * subPad) / 4.0f;
        if (drawHammerButton(x + 10.0f, curY, subW, 22.0f, "1. TRNS", _poserSubMode == PoserSubMode::WeaponTransform)) {
            _poserSubMode = PoserSubMode::WeaponTransform;
            log("Switched to Weapon Model Transform (Translation, Rotation, Scaling).");
        }
        if (drawHammerButton(x + 10.0f + (subW + subPad) * 1.0f, curY, subW, 22.0f, "2. SCKT", _poserSubMode == PoserSubMode::HandSockets)) {
            _poserSubMode = PoserSubMode::HandSockets;
            log("Switched to Hand Sockets Poser.");
        }
        if (drawHammerButton(x + 10.0f + (subW + subPad) * 2.0f, curY, subW, 22.0f, "3. ADS", _poserSubMode == PoserSubMode::AdsAlignment)) {
            _poserSubMode = PoserSubMode::AdsAlignment;
            log("Switched to ADS Optical Alignment.");
        }
        if (drawHammerButton(x + 10.0f + (subW + subPad) * 3.0f, curY, subW, 22.0f, "4. BOT", _poserSubMode == PoserSubMode::BotWeaponSocket)) {
            _poserSubMode = PoserSubMode::BotWeaponSocket;
            log("Switched to Bot Weapon Socket Alignment & Scaling.");
        }
        curY += 28.0f;

        // ==================== ANIMATION PREVIEW & WEAPON KINEMATICS ====================
        Renderer::drawRect(x + 8.0f, curY, w - 16.0f, 96.0f, Vec3(0.16f, 0.17f, 0.19f));
        drawHammerBevel(x + 8.0f, curY, w - 16.0f, 96.0f, true);
        LabFont::drawText(x + 14.0f, curY + 5.0f, "ANIMATION PREVIEW & KINEMATICS:", 1.4f, Vec3(1.0f, 0.85f, 0.25f), LabFontType::System);

        float aBtnPad = 3.0f;
        float aBtnW = (w - 28.0f - 4.0f * aBtnPad) / 5.0f;
        if (drawHammerButton(x + 14.0f + 0 * (aBtnW + aBtnPad), curY + 20.0f, aBtnW, 20.0f, "IDLE", _activeAnimState == StudioAnimState::Idle)) {
            setStudioAnimation(StudioAnimState::Idle);
        }
        if (drawHammerButton(x + 14.0f + 1 * (aBtnW + aBtnPad), curY + 20.0f, aBtnW, 20.0f, "WALK", _activeAnimState == StudioAnimState::Walk)) {
            setStudioAnimation(StudioAnimState::Walk);
        }
        if (drawHammerButton(x + 14.0f + 2 * (aBtnW + aBtnPad), curY + 20.0f, aBtnW, 20.0f, "SHOOT", _activeAnimState == StudioAnimState::Shoot)) {
            setStudioAnimation(StudioAnimState::Shoot);
        }
        if (drawHammerButton(x + 14.0f + 3 * (aBtnW + aBtnPad), curY + 20.0f, aBtnW, 20.0f, "RELOAD", _activeAnimState == StudioAnimState::Reload)) {
            setStudioAnimation(StudioAnimState::Reload);
        }
        if (drawHammerButton(x + 14.0f + 4 * (aBtnW + aBtnPad), curY + 20.0f, aBtnW, 20.0f, "INSPECT", _activeAnimState == StudioAnimState::Inspect)) {
            setStudioAnimation(StudioAnimState::Inspect);
        }

        // Play/Pause + Playback Speed slider
        float pBtnW = 64.0f;
        if (drawHammerButton(x + 14.0f, curY + 44.0f, pBtnW, 20.0f, _animPlaying ? "PAUSE" : "PLAY", !_animPlaying)) {
            _animPlaying = !_animPlaying;
            log(_animPlaying ? "Animation playback resumed." : "Animation playback paused.");
        }
        drawHammerSlider(x + 14.0f + pBtnW + 8.0f, curY + 46.0f, w - 28.0f - pBtnW - 8.0f, 16.0f, "Speed:", _animPlaybackSpeed, 0.25f, 2.5f, "%.2fx");

        // Dynamic Clips loaded for Bot (Mixamo FBX / glTF / binary .anim)
        float clipX = x + 14.0f;
        float clipY = curY + 70.0f;
        LabFont::drawText(clipX, clipY + 2.0f, "FBX / BOT CLIPS:", 1.2f, Vec3(0.65f, 0.70f, 0.75f), LabFontType::System);
        clipY += 18.0f;
        if (!_botAnimations.empty()) {
            float clipBtnW = 96.0f;
            float clipBtnH = 18.0f;
            float curRowX = x + 14.0f;
            float maxRowX = x + w - 14.0f;
            for (size_t cIdx = 0; cIdx < _botAnimations.size(); ++cIdx) {
                const auto& clip = _botAnimations[cIdx];
                if (curRowX + clipBtnW > maxRowX) {
                    curRowX = x + 14.0f;
                    clipY += clipBtnH + 3.0f;
                }
                bool isActiveClip = (_activeBotClipName == clip.name);
                if (drawHammerButton(curRowX, clipY, clipBtnW, clipBtnH, clip.name.c_str(), isActiveClip)) {
                    setBotAnimationClip(clip.name);
                }
                curRowX += clipBtnW + 3.0f;
            }
            clipY += clipBtnH + 6.0f;
        } else {
            LabFont::drawText(clipX, clipY + 2.0f, "Procedural Rig (Add FBX animations to assets/animations/)", 1.1f, Vec3(0.5f, 0.5f, 0.5f), LabFontType::System);
            clipY += 22.0f;
        }
        curY = clipY;

        if (_poserSubMode == PoserSubMode::WeaponTransform) {
            // Mode Toggle: Move Weapon Only (Hands Fixed) vs Move Together
            float toggleH = 24.0f;
            std::string modeLabel = grip.lockHands 
                ? "[X] MOVE WEAPON ONLY (HANDS FIXED)" 
                : "[ ] MOVE BOTH (WEAPON + HANDS)";
            if (drawHammerButton(x + 8.0f, curY, w - 16.0f, toggleH, modeLabel, grip.lockHands)) {
                pushUndoState();
                grip.lockHands = !grip.lockHands;
                log(grip.lockHands 
                    ? "Lock Hands ON: Weapon transforms independently without moving hands."
                    : "Lock Hands OFF: Weapon and hands move together.");
            }
            curY += toggleH + 6.0f;

            // Group Box 1: Weapon Translation (Przesuwanie)
            Renderer::drawRect(x + 8.0f, curY, w - 16.0f, 100.0f, Vec3(0.18f, 0.19f, 0.21f));
            drawHammerBevel(x + 8.0f, curY, w - 16.0f, 100.0f, true);
            LabFont::drawText(x + 14.0f, curY + 5.0f, 
                grip.lockHands ? "WEAPON TRANSLATION (SAMODZIELNA BRON):" : "WEAPON TRANSLATION (PRZESUWANIE):", 
                1.4f, Vec3(0.4f, 0.85f, 1.0f), LabFontType::System);

            drawHammerSlider(x + 14.0f, curY + 22.0f, w - 28.0f, 16.0f, "Offset X (L/R):", grip.weaponOffset.x, -0.50f, 0.50f);
            drawHammerSlider(x + 14.0f, curY + 40.0f, w - 28.0f, 16.0f, "Offset Y (U/D):", grip.weaponOffset.y, -0.50f, 0.50f);
            drawHammerSlider(x + 14.0f, curY + 58.0f, w - 28.0f, 16.0f, "Offset Z (F/B):", grip.weaponOffset.z, -0.50f, 0.50f);
            if (drawHammerButton(x + 14.0f, curY + 76.0f, w - 28.0f, 18.0f, "Center Position (0, 0, 0)")) {
                centerActiveWeapon();
            }
            curY += 106.0f;

            // Group Box 2: Weapon Rotation (Obracanie)
            Renderer::drawRect(x + 8.0f, curY, w - 16.0f, 100.0f, Vec3(0.18f, 0.19f, 0.21f));
            drawHammerBevel(x + 8.0f, curY, w - 16.0f, 100.0f, true);
            LabFont::drawText(x + 14.0f, curY + 5.0f, "WEAPON ROTATION (OBRACANIE):", 1.4f, Vec3(0.4f, 0.95f, 0.4f), LabFontType::System);

            drawHammerSlider(x + 14.0f, curY + 22.0f, w - 28.0f, 16.0f, "Pitch X (Rot X):", grip.weaponRotation.x, -180.0f, 180.0f, "%.1f deg");
            drawHammerSlider(x + 14.0f, curY + 40.0f, w - 28.0f, 16.0f, "Yaw Y (Rot Y):", grip.weaponRotation.y, -180.0f, 180.0f, "%.1f deg");
            drawHammerSlider(x + 14.0f, curY + 58.0f, w - 28.0f, 16.0f, "Roll Z (Rot Z):", grip.weaponRotation.z, -180.0f, 180.0f, "%.1f deg");
            if (drawHammerButton(x + 14.0f, curY + 76.0f, w - 28.0f, 18.0f, "Zero Rotation (0, 0, 0)")) {
                resetActiveWeaponRotation();
            }
            curY += 106.0f;

            // Group Box 3: Weapon Scaling (Skalowanie)
            Renderer::drawRect(x + 8.0f, curY, w - 16.0f, 118.0f, Vec3(0.18f, 0.19f, 0.21f));
            drawHammerBevel(x + 8.0f, curY, w - 16.0f, 118.0f, true);
            LabFont::drawText(x + 14.0f, curY + 5.0f, "WEAPON SCALING (SKALOWANIE):", 1.4f, Vec3(0.95f, 0.65f, 0.2f), LabFontType::System);

            float uniformScale = (grip.weaponScale.x + grip.weaponScale.y + grip.weaponScale.z) / 3.0f;
            float prevUniform = uniformScale;
            if (drawHammerSlider(x + 14.0f, curY + 22.0f, w - 28.0f, 16.0f, "Uniform Scale:", uniformScale, 0.10f, 3.00f, "%.2f x")) {
                float ratio = (prevUniform > 0.001f) ? (uniformScale / prevUniform) : 1.0f;
                grip.weaponScale.x = std::clamp(grip.weaponScale.x * ratio, 0.05f, 5.0f);
                grip.weaponScale.y = std::clamp(grip.weaponScale.y * ratio, 0.05f, 5.0f);
                grip.weaponScale.z = std::clamp(grip.weaponScale.z * ratio, 0.05f, 5.0f);
            }
            drawHammerSlider(x + 14.0f, curY + 40.0f, w - 28.0f, 16.0f, "Scale X (Width):", grip.weaponScale.x, 0.10f, 3.00f, "%.2f x");
            drawHammerSlider(x + 14.0f, curY + 58.0f, w - 28.0f, 16.0f, "Scale Y (Height):", grip.weaponScale.y, 0.10f, 3.00f, "%.2f x");
            drawHammerSlider(x + 14.0f, curY + 76.0f, w - 28.0f, 16.0f, "Scale Z (Length):", grip.weaponScale.z, 0.10f, 3.00f, "%.2f x");
            if (drawHammerButton(x + 14.0f, curY + 94.0f, w - 28.0f, 18.0f, "Reset Scale (1.00 x)")) {
                resetActiveWeaponScale();
            }
            curY += 124.0f;

            // Bottom Action Buttons
            float actW = (w - 32.0f) * 0.5f;
            if (drawHammerButton(x + 12.0f, curY, actW, 26.0f, "Reset Transforms")) {
                resetActiveWeaponAllTransforms();
            }
            if (drawHammerButton(x + 12.0f + actW + 8.0f, curY, actW, 26.0f, "Apply In-Game", false, true)) {
                saveConfig("assets/configs/character_studio.cfg");
                if (_onApplyInGame) _onApplyInGame();
                log("Saved and applied weapon transforms to active game!");
            }
        } else if (_poserSubMode == PoserSubMode::HandSockets) {
            // Group Box: Right Hand Socket (Trigger)
            Renderer::drawRect(x + 8.0f, curY, w - 16.0f, 130.0f, Vec3(0.18f, 0.19f, 0.21f));
            drawHammerBevel(x + 8.0f, curY, w - 16.0f, 130.0f, true);
            LabFont::drawText(x + 14.0f, curY + 6.0f, "RIGHT HAND SOCKET (TRIGGER GRIP):", 1.4f, Vec3(0.4f, 0.85f, 1.0f), LabFontType::System);

            drawHammerSlider(x + 14.0f, curY + 26.0f, w - 28.0f, 18.0f, "Pos X (Offset):", grip.rightSocketPos.x, -0.20f, 0.20f);
            drawHammerSlider(x + 14.0f, curY + 46.0f, w - 28.0f, 18.0f, "Pos Y (Height):", grip.rightSocketPos.y, -0.25f, 0.15f);
            drawHammerSlider(x + 14.0f, curY + 66.0f, w - 28.0f, 18.0f, "Pos Z (Reach):", grip.rightSocketPos.z, -0.30f, 0.15f);
            drawHammerSlider(x + 14.0f, curY + 86.0f, w - 28.0f, 18.0f, "Pitch (Rot X):", grip.rightSocketRot.x, -45.0f, 45.0f, "%.1f deg");
            drawHammerSlider(x + 14.0f, curY + 106.0f, w - 28.0f, 18.0f, "Roll (Rot Z):", grip.rightSocketRot.z, -45.0f, 45.0f, "%.1f deg");
            curY += 138.0f;

            // Group Box: Left Hand Socket (Support)
            Renderer::drawRect(x + 8.0f, curY, w - 16.0f, 130.0f, Vec3(0.18f, 0.19f, 0.21f));
            drawHammerBevel(x + 8.0f, curY, w - 16.0f, 130.0f, true);
            LabFont::drawText(x + 14.0f, curY + 6.0f, "LEFT HAND SOCKET (FOREGRIP SUPPORT):", 1.4f, Vec3(0.4f, 0.95f, 0.4f), LabFontType::System);

            drawHammerSlider(x + 14.0f, curY + 26.0f, w - 28.0f, 18.0f, "Pos X (Offset):", grip.leftSocketPos.x, -0.25f, 0.20f);
            drawHammerSlider(x + 14.0f, curY + 46.0f, w - 28.0f, 18.0f, "Pos Y (Height):", grip.leftSocketPos.y, -0.25f, 0.20f);
            drawHammerSlider(x + 14.0f, curY + 66.0f, w - 28.0f, 18.0f, "Pos Z (Reach):", grip.leftSocketPos.z, -0.60f, 0.05f);
            drawHammerSlider(x + 14.0f, curY + 86.0f, w - 28.0f, 18.0f, "Pitch (Rot X):", grip.leftSocketRot.x, -60.0f, 60.0f, "%.1f deg");
            drawHammerSlider(x + 14.0f, curY + 106.0f, w - 28.0f, 18.0f, "Roll (Rot Z):", grip.leftSocketRot.z, -60.0f, 60.0f, "%.1f deg");
            curY += 138.0f;

            // Action Buttons
            float actW = (w - 32.0f) * 0.5f;
            if (drawHammerButton(x + 12.0f, curY, actW, 26.0f, "Reset Hand Sockets")) {
                pushUndoState();
                resetDefaults();
                log("Reset weapon hand sockets to defaults.");
            }
            if (drawHammerButton(x + 12.0f + actW + 8.0f, curY, actW, 26.0f, "Apply In-Game", false, true)) {
                saveConfig("assets/configs/character_studio.cfg");
                if (_onApplyInGame) _onApplyInGame();
                log("Saved and applied custom weapon sockets to active game!");
            }
        } else if (_poserSubMode == PoserSubMode::AdsAlignment) {
            // Group Box: ADS Alignment
            Renderer::drawRect(x + 8.0f, curY, w - 16.0f, 100.0f, Vec3(0.18f, 0.19f, 0.21f));
            drawHammerBevel(x + 8.0f, curY, w - 16.0f, 100.0f, true);
            LabFont::drawText(x + 14.0f, curY + 6.0f, "ADS OPTICAL CENTER ALIGNMENT:", 1.4f, Vec3(0.95f, 0.65f, 0.2f), LabFontType::System);

            drawHammerSlider(x + 14.0f, curY + 26.0f, w - 28.0f, 18.0f, "Sight X Align:", grip.adsOffset.x, -0.10f, 0.10f);
            drawHammerSlider(x + 14.0f, curY + 46.0f, w - 28.0f, 18.0f, "Sight Y Height:", grip.adsOffset.y, -0.15f, 0.05f);
            drawHammerSlider(x + 14.0f, curY + 66.0f, w - 28.0f, 18.0f, "Sight Z Eye Relief:", grip.adsOffset.z, -0.10f, 0.30f);
            curY += 108.0f;

            // Action Buttons
            float actW = (w - 32.0f) * 0.5f;
            if (drawHammerButton(x + 12.0f, curY, actW, 26.0f, "Reset ADS")) {
                pushUndoState();
                grip.adsOffset = Vec3(0.0f, -0.05f, 0.12f);
                log("Reset ADS alignment to default center.");
            }
            if (drawHammerButton(x + 12.0f + actW + 8.0f, curY, actW, 26.0f, "Apply In-Game", false, true)) {
                saveConfig("assets/configs/character_studio.cfg");
                if (_onApplyInGame) _onApplyInGame();
                log("Saved and applied custom weapon sockets to active game!");
            }
        }

        if (_poserSubMode == PoserSubMode::BotWeaponSocket) {
            // Group Box 1: Bot Weapon Translation & Offset
            Renderer::drawRect(x + 8.0f, curY, w - 16.0f, 100.0f, Vec3(0.18f, 0.19f, 0.21f));
            drawHammerBevel(x + 8.0f, curY, w - 16.0f, 100.0f, true);
            LabFont::drawText(x + 14.0f, curY + 5.0f, "BOT WEAPON SOCKET OFFSET (PRZESUNIECIE):", 1.4f, Vec3(0.4f, 0.85f, 1.0f), LabFontType::System);

            drawHammerSlider(x + 14.0f, curY + 22.0f, w - 28.0f, 16.0f, "Bot Offset X (L/R):", grip.botSocket.offset.x, -0.60f, 0.60f);
            drawHammerSlider(x + 14.0f, curY + 40.0f, w - 28.0f, 16.0f, "Bot Offset Y (U/D):", grip.botSocket.offset.y, -0.60f, 0.60f);
            drawHammerSlider(x + 14.0f, curY + 58.0f, w - 28.0f, 16.0f, "Bot Offset Z (F/B):", grip.botSocket.offset.z, -0.60f, 0.60f);
            if (drawHammerButton(x + 14.0f, curY + 76.0f, w - 28.0f, 18.0f, "Default Bot Offset (0.00, -0.05, 0.02)")) {
                pushUndoState();
                grip.botSocket.offset = Vec3(0.0f, -0.05f, 0.02f);
                log("Reset bot weapon offset to default.");
            }
            curY += 106.0f;

            // Group Box 2: Bot Weapon Rotation
            Renderer::drawRect(x + 8.0f, curY, w - 16.0f, 100.0f, Vec3(0.18f, 0.19f, 0.21f));
            drawHammerBevel(x + 8.0f, curY, w - 16.0f, 100.0f, true);
            LabFont::drawText(x + 14.0f, curY + 5.0f, "BOT WEAPON ROTATION (KAT NACHYLENIA):", 1.4f, Vec3(1.0f, 0.75f, 0.3f), LabFontType::System);

            drawHammerSlider(x + 14.0f, curY + 22.0f, w - 28.0f, 16.0f, "Bot Pitch (X deg):", grip.botSocket.rotation.x, -180.0f, 180.0f);
            drawHammerSlider(x + 14.0f, curY + 40.0f, w - 28.0f, 16.0f, "Bot Yaw (Y deg):",   grip.botSocket.rotation.y, -180.0f, 180.0f);
            drawHammerSlider(x + 14.0f, curY + 58.0f, w - 28.0f, 16.0f, "Bot Roll (Z deg):",  grip.botSocket.rotation.z, -180.0f, 180.0f);
            if (drawHammerButton(x + 14.0f, curY + 76.0f, w - 28.0f, 18.0f, "Zero Bot Rotation (0, 0, 0)")) {
                pushUndoState();
                grip.botSocket.rotation = Vec3(0.0f, 0.0f, 0.0f);
                log("Zeroed bot weapon rotation.");
            }
            curY += 106.0f;

            // Group Box 3: Bot Weapon Scale
            Renderer::drawRect(x + 8.0f, curY, w - 16.0f, 76.0f, Vec3(0.18f, 0.19f, 0.21f));
            drawHammerBevel(x + 8.0f, curY, w - 16.0f, 76.0f, true);
            LabFont::drawText(x + 14.0f, curY + 5.0f, "BOT WEAPON SCALE (SKALA BRONI BOTA):", 1.4f, Vec3(0.5f, 1.0f, 0.5f), LabFontType::System);

            drawHammerSlider(x + 14.0f, curY + 22.0f, w - 28.0f, 16.0f, "Scale Multiplier:", grip.botSocket.scale.x, 0.2f, 3.0f);
            grip.botSocket.scale.y = grip.botSocket.scale.x;
            grip.botSocket.scale.z = grip.botSocket.scale.x;
            if (drawHammerButton(x + 14.0f, curY + 44.0f, w - 28.0f, 20.0f, "Reset Scale to 1.0")) {
                pushUndoState();
                grip.botSocket.scale = Vec3(1.0f, 1.0f, 1.0f);
                log("Reset bot weapon scale to 1.0.");
            }
            curY += 82.0f;

            // Action Buttons
            float actW = (w - 32.0f) * 0.5f;
            if (drawHammerButton(x + 12.0f, curY, actW, 26.0f, "Reset Bot Socket")) {
                pushUndoState();
                resetBotWeaponSocket();
            }
            if (drawHammerButton(x + 12.0f + actW + 8.0f, curY, actW, 26.0f, "Apply In-Game", false, true)) {
                saveConfig("assets/configs/character_studio.cfg");
                if (_onApplyInGame) _onApplyInGame();
                log("Saved and applied custom bot weapon sockets to active game!");
            }
        }
    }

    void CharacterStudio::renderTabReloadTimeline(float x, float y, float w, float h) {
        (void)h;
        float curY = y + 26.0f;

        LabFont::drawText(x + 10.0f, curY, "RELOAD ANIMATION CHOREOGRAPHY & PHASES", 1.5f, Vec3(0.95f, 0.85f, 0.3f), LabFontType::System);
        curY += 22.0f;

        // Timeline Scrubber Bar
        float scrubX = x + 12.0f;
        float scrubW = w - 24.0f;
        float scrubH = 26.0f;

        Renderer::drawRect(scrubX, curY, scrubW, scrubH, Vec3(0.12f, 0.13f, 0.15f));
        drawHammerBevel(scrubX, curY, scrubW, scrubH, true);

        // Phase zone color blocks along scrubber
        float pDip = _reloadTimeline.dipDuration;
        float pDrop = _reloadTimeline.magDropTime;
        float pInsert = _reloadTimeline.magInsertTime;
        float pBolt = _reloadTimeline.boltRackTime;

        // Phase 1: Dip
        Renderer::drawRect(scrubX + 2.0f, curY + 2.0f, (scrubW - 4.0f) * pDip, scrubH - 4.0f, Vec3(0.18f, 0.28f, 0.45f));
        // Phase 2: Mag Drop
        Renderer::drawRect(scrubX + (scrubW - 4.0f) * pDrop, curY + 2.0f, (scrubW - 4.0f) * (pInsert - pDrop), scrubH - 4.0f, Vec3(0.55f, 0.32f, 0.15f));
        // Phase 3: Mag Insert
        Renderer::drawRect(scrubX + (scrubW - 4.0f) * pInsert, curY + 2.0f, (scrubW - 4.0f) * (pBolt - pInsert), scrubH - 4.0f, Vec3(0.20f, 0.55f, 0.35f));
        // Phase 4: Bolt Rack
        Renderer::drawRect(scrubX + (scrubW - 4.0f) * pBolt, curY + 2.0f, (scrubW - 4.0f) * (1.0f - pBolt), scrubH - 4.0f, Vec3(0.45f, 0.20f, 0.45f));

        // Playhead
        float headX = scrubX + (scrubW - 6.0f) * _scrubberPos;
        Renderer::drawRect(headX, curY - 3.0f, 6.0f, scrubH + 6.0f, Vec3(1.0f, 0.95f, 0.2f));

        // Scrubber Drag
        if (_lmbPressed && _mouseX >= scrubX && _mouseX <= scrubX + scrubW && _mouseY >= curY && _mouseY <= curY + scrubH) {
            _scrubberPos = std::clamp((_mouseX - scrubX) / scrubW, 0.0f, 1.0f);
            _isDraggingScrubber = true;
        }
        curY += 34.0f;

        // Readout
        char timeBuf[64];
        snprintf(timeBuf, sizeof(timeBuf), "Playhead: %.2f / %.2fs  (Phase: %s)",
                 _scrubberPos * _scrubberTotalDuration, _scrubberTotalDuration,
                 (_scrubberPos < pDrop) ? "1. Dip Weapon" :
                 (_scrubberPos < pInsert) ? "2. Mag Eject" :
                 (_scrubberPos < pBolt) ? "3. Mag Insert" : "4. Bolt Rack");
        LabFont::drawText(x + 12.0f, curY, timeBuf, 1.4f, Vec3(0.85f, 0.90f, 0.95f), LabFontType::System);
        curY += 22.0f;

        // Playback Buttons
        float pbPad = 6.0f;
        float pbW = (w - 24.0f - 2.0f * pbPad) / 3.0f;
        if (drawHammerButton(x + 12.0f, curY, pbW, 24.0f, _scrubberPlaying ? "PAUSE" : "PLAY")) {
            _scrubberPlaying = !_scrubberPlaying;
        }
        if (drawHammerButton(x + 12.0f + 1 * (pbW + pbPad), curY, pbW, 24.0f, "STOP")) {
            _scrubberPlaying = false;
            _scrubberPos = 0.0f;
        }
        if (drawHammerButton(x + 12.0f + 2 * (pbW + pbPad), curY, pbW, 24.0f, _scrubberLoop ? "LOOP: ON" : "LOOP: OFF", _scrubberLoop)) {
            _scrubberLoop = !_scrubberLoop;
        }
        curY += 32.0f;

        // Phase Timing Sliders
        Renderer::drawRect(x + 8.0f, curY, w - 16.0f, 140.0f, Vec3(0.18f, 0.19f, 0.21f));
        drawHammerBevel(x + 8.0f, curY, w - 16.0f, 140.0f, true);
        LabFont::drawText(x + 14.0f, curY + 6.0f, "KEYFRAME PHASE TIMING POINTS:", 1.4f, Vec3(0.4f, 0.85f, 1.0f), LabFontType::System);

        drawHammerSlider(x + 14.0f, curY + 26.0f, w - 28.0f, 18.0f, "Dip Duration:", _reloadTimeline.dipDuration, 0.15f, 0.50f);
        drawHammerSlider(x + 14.0f, curY + 46.0f, w - 28.0f, 18.0f, "Mag Drop Cue:", _reloadTimeline.magDropTime, 0.15f, 0.55f);
        drawHammerSlider(x + 14.0f, curY + 66.0f, w - 28.0f, 18.0f, "Mag Insert Cue:", _reloadTimeline.magInsertTime, 0.45f, 0.85f);
        drawHammerSlider(x + 14.0f, curY + 86.0f, w - 28.0f, 18.0f, "Bolt Rack Cue:", _reloadTimeline.boltRackTime, 0.70f, 0.98f);
        drawHammerSlider(x + 14.0f, curY + 106.0f, w - 28.0f, 18.0f, "Dip Depth (m):", _reloadTimeline.dipDepth, 0.03f, 0.16f);
        curY += 150.0f;

        // Sound Trigger Previews
        LabFont::drawText(x + 12.0f, curY, "SOUND FX TIMELINE CUES:", 1.4f, Vec3(0.85f, 0.90f, 0.95f), LabFontType::System);
        curY += 18.0f;
        float sfxPad = 6.0f;
        float sfxW = (w - 24.0f - 2.0f * sfxPad) / 3.0f;
        if (drawHammerButton(x + 12.0f + 0 * (sfxW + sfxPad), curY, sfxW, 22.0f, "Mag Out SFX")) {
            AudioEngine::playSound(SoundID::Reload, 1.0f, 1.15f);
            log("Auditioned sound: Mag Out SFX");
        }
        if (drawHammerButton(x + 12.0f + 1 * (sfxW + sfxPad), curY, sfxW, 22.0f, "Mag In SFX")) {
            AudioEngine::playSound(SoundID::Reload, 1.0f, 0.95f);
            log("Auditioned sound: Mag In SFX");
        }
        if (drawHammerButton(x + 12.0f + 2 * (sfxW + sfxPad), curY, sfxW, 22.0f, "Bolt Rack SFX")) {
            AudioEngine::playSound(SoundID::Reload, 1.0f, 1.35f);
            log("Auditioned sound: Bolt Rack SFX");
        }
    }

    void CharacterStudio::renderTabWeaponSkins(float x, float y, float w, float h) {
        (void)h;
        float curY = y + 26.0f;
        auto& skin = _weaponSkins[_selectedWeaponIndex];

        // 1. 3D Model Selection (.STL)
        LabFont::drawText(x + 10.0f, curY, "1. WEAPON 3D MODEL (.STL):", 1.5f, Vec3(0.95f, 0.85f, 0.3f), LabFontType::System);
        curY += 20.0f;

        std::string currentModel = skin.modelFile.empty() ? "(Default STL)" : skin.modelFile;
        if (currentModel.size() > 30) {
            currentModel = "..." + currentModel.substr(currentModel.size() - 27);
        }
        LabFont::drawText(x + 14.0f, curY, "Active: " + currentModel, 1.3f, Vec3(0.35f, 0.9f, 1.0f), LabFontType::System);
        curY += 18.0f;

        float qPad = 4.0f;
        float qW = (w - 24.0f - 3.0f * qPad) / 4.0f;
        if (drawHammerButton(x + 12.0f + 0 * (qW + qPad), curY, qW, 22.0f, "Pipe", skin.modelFile == "pipe.stl")) {
            pushUndoState();
            skin.modelFile = "pipe.stl";
            getMesh(skin.modelFile);
            log("Assigned model: pipe.stl");
        }
        if (drawHammerButton(x + 12.0f + 1 * (qW + qPad), curY, qW, 22.0f, "Railgun", skin.modelFile == "railgun.stl")) {
            pushUndoState();
            skin.modelFile = "railgun.stl";
            getMesh(skin.modelFile);
            log("Assigned model: railgun.stl");
        }
        if (drawHammerButton(x + 12.0f + 2 * (qW + qPad), curY, qW, 22.0f, "Model.stl", skin.modelFile == "Model.stl")) {
            pushUndoState();
            skin.modelFile = "Model.stl";
            getMesh(skin.modelFile);
            log("Assigned user model: Model.stl");
        }
        if (drawHammerButton(x + 12.0f + 3 * (qW + qPad), curY, qW, 22.0f, "Procedural", skin.modelFile == "PROCEDURAL")) {
            pushUndoState();
            skin.modelFile = "PROCEDURAL";
            log("Assigned procedural built-in weapon.");
        }
        curY += 26.0f;

        if (drawHammerButton(x + 12.0f, curY, w - 24.0f, 22.0f, "Browse Custom STL File...")) {
            std::string picked = LabDialogs::openFileDialog(_window, "3D STL Model (*.stl)\0*.stl\0All Files (*.*)\0*.*\0", "assets\\models");
            if (!picked.empty()) {
                pushUndoState();
                skin.modelFile = picked;
                getMesh(picked);
                log("Selected custom STL weapon model: " + picked);
            }
        }
        curY += 30.0f;

        // 2. Surface textures
        LabFont::drawText(x + 10.0f, curY, "2. SURFACE TEXTURES & CAMO:", 1.5f, Vec3(0.95f, 0.85f, 0.3f), LabFontType::System);
        curY += 20.0f;

        std::string currentTex = skin.textureFile.empty() ? "(Default Texture)" : skin.textureFile;
        if (currentTex.size() > 30) {
            currentTex = "..." + currentTex.substr(currentTex.size() - 27);
        }
        LabFont::drawText(x + 14.0f, curY, "Active: " + currentTex, 1.3f, Vec3(0.35f, 0.9f, 1.0f), LabFontType::System);
        curY += 18.0f;

        float texPad = 4.0f;
        float texW = (w - 24.0f - 2.0f * texPad) / 3.0f;
        if (drawHammerButton(x + 12.0f + 0 * (texW + texPad), curY, texW, 20.0f, "Pipe Wrench", skin.textureFile.find("PipeWrench") != std::string::npos || skin.textureFile == "weapon_pipe.bmp")) {
            pushUndoState();
            skin.textureFile = "PipeWrenchTool_baseColor.png";
            getTexture(skin.textureFile);
            log("Assigned texture: PipeWrenchTool_baseColor.png");
        }
        if (drawHammerButton(x + 12.0f + 1 * (texW + texPad), curY, texW, 20.0f, "Rail Main", skin.textureFile.find("rail_main") != std::string::npos)) {
            pushUndoState();
            skin.textureFile = "rail_main_baseColor.png";
            getTexture(skin.textureFile);
            log("Assigned texture: rail_main_baseColor.png");
        }
        if (drawHammerButton(x + 12.0f + 2 * (texW + texPad), curY, texW, 20.0f, "Rail Detail", skin.textureFile.find("rail_details") != std::string::npos)) {
            pushUndoState();
            skin.textureFile = "rail_details_baseColor.png";
            getTexture(skin.textureFile);
            log("Assigned texture: rail_details_baseColor.png");
        }
        curY += 24.0f;

        if (drawHammerButton(x + 12.0f, curY, w - 24.0f, 22.0f, "Browse Custom Texture (PNG/BMP/TGA)...")) {
            std::string picked = LabDialogs::openFileDialog(_window, "Texture Images (*.png;*.bmp;*.tga;*.jpg)\0*.png;*.bmp;*.tga;*.jpg\0All Files (*.*)\0*.*\0", "assets\\models\\textures");
            if (!picked.empty()) {
                pushUndoState();
                skin.textureFile = picked;
                getTexture(picked);
                log("Selected custom weapon texture: " + picked);
            }
        }
        curY += 26.0f;

        struct SkinPreset { std::string name; std::string file; };
        SkinPreset skins[] = {
            { "Standard Army", "weapon_m4a4s.bmp" },
            { "Frostbite Glaze", "cryo_ice.bmp" },
            { "Titanium Hull", "metal_hull.bmp" },
            { "Hazard Caution", "hazard_stripes.bmp" },
            { "Sub-Zero Camo", "snow_frost.bmp" }
        };

        for (int i = 0; i < 5; ++i) {
            float by = curY + i * 24.0f;
            bool active = (skin.textureFile == skins[i].file);
            if (drawHammerButton(x + 12.0f, by, w - 24.0f, 20.0f, skins[i].name + " (" + skins[i].file + ")", active)) {
                pushUndoState();
                skin.textureFile = skins[i].file;
                log("Applied material texture: " + skins[i].name);
            }
        }
        curY += 128.0f;

        // 3. Material Parameters Group
        Renderer::drawRect(x + 8.0f, curY, w - 16.0f, 130.0f, Vec3(0.18f, 0.19f, 0.21f));
        drawHammerBevel(x + 8.0f, curY, w - 16.0f, 130.0f, true);
        LabFont::drawText(x + 14.0f, curY + 5.0f, "MATERIAL PROPERTIES & SHADING:", 1.4f, Vec3(0.4f, 0.85f, 1.0f), LabFontType::System);

        drawHammerSlider(x + 14.0f, curY + 22.0f, w - 28.0f, 16.0f, "UV Tiling:", skin.uvScale, 0.25f, 4.0f);
        drawHammerSlider(x + 14.0f, curY + 40.0f, w - 28.0f, 16.0f, "Tint Red:", skin.tintColor.x, 0.1f, 1.5f);
        drawHammerSlider(x + 14.0f, curY + 58.0f, w - 28.0f, 16.0f, "Tint Green:", skin.tintColor.y, 0.1f, 1.5f);
        drawHammerSlider(x + 14.0f, curY + 76.0f, w - 28.0f, 16.0f, "Tint Blue:", skin.tintColor.z, 0.1f, 1.5f);
        drawHammerSlider(x + 14.0f, curY + 94.0f, w - 28.0f, 16.0f, "Metallic/Rough:", skin.metallic, 0.0f, 1.0f);
        curY += 138.0f;

        // Bottom Action Buttons
        float actW = (w - 32.0f) * 0.5f;
        if (drawHammerButton(x + 12.0f, curY, actW, 26.0f, "Save Preset")) {
            saveConfig("assets/configs/character_studio.cfg");
            log("Saved weapon skin config to assets/configs/character_studio.cfg");
        }
        if (drawHammerButton(x + 12.0f + actW + 8.0f, curY, actW, 26.0f, "Apply In-Game", false, true)) {
            saveConfig("assets/configs/character_studio.cfg");
            if (_onApplyInGame) _onApplyInGame();
            log("Applied weapon skin & STL model to active gameplay!");
        }
    }

    void CharacterStudio::renderTabAppearance(float x, float y, float w, float h) {
        (void)h;
        float curY = y + 26.0f;

        LabFont::drawText(x + 10.0f, curY, "TACTICAL OPERATIVE OUTFIT & ARMOR", 1.5f, Vec3(0.95f, 0.85f, 0.3f), LabFontType::System);
        curY += 22.0f;

        // Armor Class
        LabFont::drawText(x + 12.0f, curY, "Armor Class:", 1.4f, Vec3(0.85f, 0.90f, 0.95f), LabFontType::System);
        curY += 18.0f;
        float acPad = 6.0f;
        float acW = (w - 24.0f - 2.0f * acPad) / 3.0f;
        if (drawHammerButton(x + 12.0f + 0 * (acW + acPad), curY, acW, 24.0f, "Light Scout", _appearance.armorClass == ArmorClass::LightScout)) {
            pushUndoState();
            _appearance.armorClass = ArmorClass::LightScout;
            log("Operative Armor set to: Light Scout");
        }
        if (drawHammerButton(x + 12.0f + 1 * (acW + acPad), curY, acW, 24.0f, "Cryo Marine", _appearance.armorClass == ArmorClass::CryoMarine)) {
            pushUndoState();
            _appearance.armorClass = ArmorClass::CryoMarine;
            log("Operative Armor set to: Heavy Cryo Marine");
        }
        if (drawHammerButton(x + 12.0f + 2 * (acW + acPad), curY, acW, 24.0f, "Tactical Officer", _appearance.armorClass == ArmorClass::TacticalOfficer)) {
            pushUndoState();
            _appearance.armorClass = ArmorClass::TacticalOfficer;
            log("Operative Armor set to: Tactical Officer");
        }
        curY += 34.0f;

        // Helmet Type
        LabFont::drawText(x + 12.0f, curY, "Headgear & Visor:", 1.4f, Vec3(0.85f, 0.90f, 0.95f), LabFontType::System);
        curY += 18.0f;
        if (drawHammerButton(x + 12.0f + 0 * (acW + acPad), curY, acW, 24.0f, "Combat Visor", _appearance.helmetType == HelmetType::CombatVisor)) {
            pushUndoState();
            _appearance.helmetType = HelmetType::CombatVisor;
        }
        if (drawHammerButton(x + 12.0f + 1 * (acW + acPad), curY, acW, 24.0f, "Sealed Helmet", _appearance.helmetType == HelmetType::SealedHelmet)) {
            pushUndoState();
            _appearance.helmetType = HelmetType::SealedHelmet;
        }
        if (drawHammerButton(x + 12.0f + 2 * (acW + acPad), curY, acW, 24.0f, "Tactical Beanie", _appearance.helmetType == HelmetType::TacticalBeanie)) {
            pushUndoState();
            _appearance.helmetType = HelmetType::TacticalBeanie;
        }
        curY += 34.0f;

        // Visor LED Color
        LabFont::drawText(x + 12.0f, curY, "Visor HUD LED Glow:", 1.4f, Vec3(0.85f, 0.90f, 0.95f), LabFontType::System);
        curY += 18.0f;
        float ledPad = 6.0f;
        float ledW = (w - 24.0f - 3.0f * ledPad) / 4.0f;
        if (drawHammerButton(x + 12.0f + 0 * (ledW + ledPad), curY, ledW, 22.0f, "Cyan")) { pushUndoState(); _appearance.visorGlowColor = Vec3(0.2f, 0.85f, 1.0f); }
        if (drawHammerButton(x + 12.0f + 1 * (ledW + ledPad), curY, ledW, 22.0f, "Amber")) { pushUndoState(); _appearance.visorGlowColor = Vec3(1.0f, 0.75f, 0.1f); }
        if (drawHammerButton(x + 12.0f + 2 * (ledW + ledPad), curY, ledW, 22.0f, "Crimson")) { pushUndoState(); _appearance.visorGlowColor = Vec3(1.0f, 0.2f, 0.2f); }
        if (drawHammerButton(x + 12.0f + 3 * (ledW + ledPad), curY, ledW, 22.0f, "Acid Green")) { pushUndoState(); _appearance.visorGlowColor = Vec3(0.2f, 1.0f, 0.3f); }
        curY += 32.0f;

        // Fatigues Color
        Renderer::drawRect(x + 8.0f, curY, w - 16.0f, 90.0f, Vec3(0.18f, 0.19f, 0.21f));
        drawHammerBevel(x + 8.0f, curY, w - 16.0f, 90.0f, true);
        LabFont::drawText(x + 14.0f, curY + 6.0f, "FATIGUES & UNIFORM CAMOUFLAGE COLOR:", 1.4f, Vec3(0.4f, 0.85f, 1.0f), LabFontType::System);

        drawHammerSlider(x + 14.0f, curY + 26.0f, w - 28.0f, 18.0f, "Uniform Red:", _appearance.fatiguesColor.x, 0.05f, 1.0f);
        drawHammerSlider(x + 14.0f, curY + 46.0f, w - 28.0f, 18.0f, "Uniform Green:", _appearance.fatiguesColor.y, 0.05f, 1.0f);
        drawHammerSlider(x + 14.0f, curY + 66.0f, w - 28.0f, 18.0f, "Uniform Blue:", _appearance.fatiguesColor.z, 0.05f, 1.0f);
        curY += 100.0f;

        if (drawHammerButton(x + 12.0f, curY, w - 24.0f, 26.0f, "Save Operative Appearance", false, true)) {
            saveConfig("assets/configs/character_studio.cfg");
            log("Saved character appearance outfit.");
        }
    }

    void CharacterStudio::renderTabFaceDialogue(float x, float y, float w, float h) {
        (void)h;
        float curY = y + 26.0f;

        LabFont::drawText(x + 10.0f, curY, "FACIAL MORPH TARGETS & DIALOGUE LIP-SYNC", 1.5f, Vec3(0.95f, 0.85f, 0.3f), LabFontType::System);
        curY += 20.0f;

        // Manual Blend Shape Sliders
        Renderer::drawRect(x + 8.0f, curY, w - 16.0f, 150.0f, Vec3(0.18f, 0.19f, 0.21f));
        drawHammerBevel(x + 8.0f, curY, w - 16.0f, 150.0f, true);
        LabFont::drawText(x + 14.0f, curY + 6.0f, "FACIAL MORPH TARGETS (BLEND SHAPES):", 1.4f, Vec3(0.4f, 0.85f, 1.0f), LabFontType::System);

        drawHammerSlider(x + 14.0f, curY + 26.0f, w - 28.0f, 18.0f, "Jaw Open (A/E):", _faceMorphs.jawOpen, 0.0f, 1.0f);
        drawHammerSlider(x + 14.0f, curY + 46.0f, w - 28.0f, 18.0f, "Mouth Narrow (O/U):", _faceMorphs.mouthNarrow, 0.0f, 1.0f);
        drawHammerSlider(x + 14.0f, curY + 66.0f, w - 28.0f, 18.0f, "Mouth Smile:", _faceMorphs.mouthSmile, 0.0f, 1.0f);
        drawHammerSlider(x + 14.0f, curY + 86.0f, w - 28.0f, 18.0f, "Brow Raise:", _faceMorphs.browRaise, 0.0f, 1.0f);
        drawHammerSlider(x + 14.0f, curY + 106.0f, w - 28.0f, 18.0f, "Eyes Squint:", _faceMorphs.eyesSquint, 0.0f, 1.0f);
        drawHammerSlider(x + 14.0f, curY + 126.0f, w - 28.0f, 18.0f, "Mouth Frown:", _faceMorphs.mouthFrown, 0.0f, 1.0f);
        curY += 158.0f;

        // Dialogue Test Suite
        Renderer::drawRect(x + 8.0f, curY, w - 16.0f, 160.0f, Vec3(0.18f, 0.19f, 0.21f));
        drawHammerBevel(x + 8.0f, curY, w - 16.0f, 160.0f, true);
        LabFont::drawText(x + 14.0f, curY + 6.0f, "DIALOGUE AUDIO & LIP-SYNC TESTER:", 1.4f, Vec3(0.95f, 0.85f, 0.3f), LabFontType::System);

        for (int i = 0; i < (int)_dialogueLines.size(); ++i) {
            float lineY = curY + 26.0f + i * 22.0f;
            bool active = (_selectedDialogueIndex == i);
            std::string label = _dialogueLines[i].speaker + ": \"" + _dialogueLines[i].text.substr(0, 24) + "...\"";
            if (drawHammerButton(x + 14.0f, lineY, w - 28.0f, 20.0f, label, active)) {
                _selectedDialogueIndex = i;
                _speechTrackSamples = LipSyncEvaluator::generateSpeechTrack(
                    _dialogueLines[i].duration, _dialogueLines[i].syllablesPerSec, _dialogueLines[i].seed);
                log("Selected Dialogue: " + _dialogueLines[i].text);
            }
        }
        curY += 120.0f;

        float actW = (w - 36.0f) * 0.5f;
        if (drawHammerButton(x + 14.0f, curY, actW, 26.0f, _dialoguePlaying ? "SPEAKING..." : "TEST DIALOGUE", _dialoguePlaying, true)) {
            _dialoguePlaying = true;
            _dialogueTimer = 0.0f;
            log("Started speech playback and phoneme lip-sync track.");
        }
        if (drawHammerButton(x + 14.0f + actW + 8.0f, curY, actW, 26.0f, "Reset Mimics")) {
            pushUndoState();
            _faceMorphs = { 0, 0, 0, 0, 0, 0 };
            _dialoguePlaying = false;
        }
        curY += 38.0f;

        // Phosphor Green Oscilloscope Waveform Visualizer
        float oscW = w - 16.0f;
        float oscH = 45.0f;
        Renderer::drawRect(x + 8.0f, curY, oscW, oscH, Vec3(0.04f, 0.08f, 0.05f));
        drawHammerBevel(x + 8.0f, curY, oscW, oscH, true);

        // Center line
        Renderer::drawRect(x + 8.0f, curY + oscH * 0.5f, oscW, 1.0f, Vec3(0.12f, 0.22f, 0.15f));

        // Waveform points
        if (!_dialogueWaveform.empty()) {
            float step = oscW / 80.0f;
            for (size_t i = 0; i < _dialogueWaveform.size(); ++i) {
                float val = _dialogueWaveform[i];
                float barH = val * (oscH * 0.42f);
                float px = x + 8.0f + i * step;
                Renderer::drawRect(px, curY + oscH * 0.5f - barH, 2.0f, barH * 2.0f + 1.0f, Vec3(0.1f, 0.95f, 0.35f));
            }
        }
        LabFont::drawText(x + 14.0f, curY + 4.0f, "ACOUSTIC ENERGY OSCILLOSCOPE [RMS]", 1.2f, Vec3(0.2f, 0.75f, 0.35f), LabFontType::System);
    }

    void CharacterStudio::renderHammerRightInspector(float panelX, float panelY, float panelW, float panelH) {
        drawHammerPanel(panelX, panelY, panelW, panelH, "OBJECT PROPERTIES & ACTOR STUDIO");

        if (_activeTab == StudioTab::GripPoser) {
            renderTabGripPoser(panelX, panelY, panelW, panelH);
        } else if (_activeTab == StudioTab::ReloadTimeline) {
            renderTabReloadTimeline(panelX, panelY, panelW, panelH);
        } else if (_activeTab == StudioTab::WeaponSkins) {
            renderTabWeaponSkins(panelX, panelY, panelW, panelH);
        } else if (_activeTab == StudioTab::Appearance) {
            renderTabAppearance(panelX, panelY, panelW, panelH);
        } else if (_activeTab == StudioTab::FaceDialogue) {
            renderTabFaceDialogue(panelX, panelY, panelW, panelH);
        }
    }

    void CharacterStudio::renderHammerUI(int screenWidth, int screenHeight) {
        float w = static_cast<float>(screenWidth);
        float h = static_cast<float>(screenHeight);

        glViewport(0, 0, screenWidth, screenHeight);
        Renderer::beginUI(screenWidth, screenHeight);

        float leftBarW = 72.0f;
        float menuH = 24.0f;
        float tbH = 28.0f;
        float topBarsH = menuH + tbH;
        float sbH = 24.0f;
        float conH = std::clamp(h * 0.16f, 100.0f, 160.0f);
        float bottomBarsH = conH + sbH;
        float rightPanelW = std::clamp(w * 0.28f, 350.0f, 460.0f);

        float vpX = leftBarW;
        float vpY = topBarsH;
        float vpW = w - leftBarW - rightPanelW;
        float vpH = h - topBarsH - bottomBarsH;

        // 1. Center 3D Viewport Frame Border
        drawHammerBevel(vpX, vpY, vpW, vpH, true);

        // Viewport Header Label (Viewport header text with live tool mode and transform status)
        float headerBoxW = std::min(540.0f, vpW - 8.0f);
        Renderer::drawRect(vpX + 4.0f, vpY + 4.0f, headerBoxW, 20.0f, Vec3(0.10f, 0.12f, 0.14f));
        const char* toolNames[] = { "ORBIT", "MOVE", "ROTATE", "SCALE" };
        std::string modeStr = toolNames[static_cast<int>(_viewportToolMode)];
        const auto& grip = _weaponGrips[_selectedWeaponIndex];
        char headerBuf[256];
        std::snprintf(headerBuf, sizeof(headerBuf), "[camera 3D] | Tool: %s | Pos: (%.2f, %.2f, %.2f) | Rot: (%.0f, %.0f, %.0f) | Scl: %.2fx",
                      modeStr.c_str(),
                      grip.weaponOffset.x, grip.weaponOffset.y, grip.weaponOffset.z,
                      grip.weaponRotation.x, grip.weaponRotation.y, grip.weaponRotation.z,
                      (grip.weaponScale.x + grip.weaponScale.y + grip.weaponScale.z) / 3.0f);
        LabFont::drawText(vpX + 8.0f, vpY + 7.0f, headerBuf, 1.3f, Vec3(0.4f, 0.95f, 0.4f), LabFontType::System);

        // 2. Left Tool Palette
        renderHammerLeftToolPalette(h);

        // 3. Right Inspector Panel
        renderHammerRightInspector(w - rightPanelW, topBarsH, rightPanelW, vpH);

        // 4. Bottom Console Panel
        renderHammerConsole(leftBarW, h - bottomBarsH, w - leftBarW, conH);

        // 5. Bottom Status Bar
        renderHammerStatusBar(w, h);

        // 6. Top Toolbar
        renderHammerToolbar(w);

        // 7. Top Menu Bar
        renderHammerTopMenuBar(w);

        // 8. Dropdown Menus (drawn on top of all other elements)
        renderHammerDropdownMenus(w, h);

        Renderer::endUI();
    }

    // =========================================================================
    // Configuration File Persistence (.cfg)
    // =========================================================================

    bool CharacterStudio::saveConfig(const std::string& filepath) {
        std::stringstream out;
        out << "# Frozen-Life Character & Weapon Studio Configuration\n";
        out << "[Studio]\nVersion=1.0\n\n";

        // Save all 9 weapon grips and skins
        for (int i = 0; i < 9; ++i) {
            out << "[WeaponGrip_" << i << "]\n";
            out << "Offset=" << _weaponGrips[i].weaponOffset.x << "," << _weaponGrips[i].weaponOffset.y << "," << _weaponGrips[i].weaponOffset.z << "\n";
            out << "Rotation=" << _weaponGrips[i].weaponRotation.x << "," << _weaponGrips[i].weaponRotation.y << "," << _weaponGrips[i].weaponRotation.z << "\n";
            out << "Scale=" << _weaponGrips[i].weaponScale.x << "," << _weaponGrips[i].weaponScale.y << "," << _weaponGrips[i].weaponScale.z << "\n";
            out << "RightPos=" << _weaponGrips[i].rightSocketPos.x << "," << _weaponGrips[i].rightSocketPos.y << "," << _weaponGrips[i].rightSocketPos.z << "\n";
            out << "RightRot=" << _weaponGrips[i].rightSocketRot.x << "," << _weaponGrips[i].rightSocketRot.y << "," << _weaponGrips[i].rightSocketRot.z << "\n";
            out << "LeftPos=" << _weaponGrips[i].leftSocketPos.x << "," << _weaponGrips[i].leftSocketPos.y << "," << _weaponGrips[i].leftSocketPos.z << "\n";
            out << "LeftRot=" << _weaponGrips[i].leftSocketRot.x << "," << _weaponGrips[i].leftSocketRot.y << "," << _weaponGrips[i].leftSocketRot.z << "\n";
            out << "AdsOffset=" << _weaponGrips[i].adsOffset.x << "," << _weaponGrips[i].adsOffset.y << "," << _weaponGrips[i].adsOffset.z << "\n";
            out << "LockHands=" << (_weaponGrips[i].lockHands ? "1" : "0") << "\n";
            out << "BotOffset=" << _weaponGrips[i].botSocket.offset.x << "," << _weaponGrips[i].botSocket.offset.y << "," << _weaponGrips[i].botSocket.offset.z << "\n";
            out << "BotRotation=" << _weaponGrips[i].botSocket.rotation.x << "," << _weaponGrips[i].botSocket.rotation.y << "," << _weaponGrips[i].botSocket.rotation.z << "\n";
            out << "BotScale=" << _weaponGrips[i].botSocket.scale.x << "," << _weaponGrips[i].botSocket.scale.y << "," << _weaponGrips[i].botSocket.scale.z << "\n\n";

            out << "[WeaponSkin_" << i << "]\n";
            out << "Model=" << _weaponSkins[i].modelFile << "\n";
            out << "Texture=" << _weaponSkins[i].textureFile << "\n";
            out << "UvScale=" << _weaponSkins[i].uvScale << "\n";
            out << "Tint=" << _weaponSkins[i].tintColor.x << "," << _weaponSkins[i].tintColor.y << "," << _weaponSkins[i].tintColor.z << "\n";
            out << "Metallic=" << _weaponSkins[i].metallic << "\n\n";
        }

        // Save reload timeline
        out << "[ReloadTimeline]\n";
        out << "DipDuration=" << _reloadTimeline.dipDuration << "\n";
        out << "MagDropTime=" << _reloadTimeline.magDropTime << "\n";
        out << "MagInsertTime=" << _reloadTimeline.magInsertTime << "\n";
        out << "BoltRackTime=" << _reloadTimeline.boltRackTime << "\n";
        out << "DipDepth=" << _reloadTimeline.dipDepth << "\n";
        out << "TiltAngle=" << _reloadTimeline.tiltAngle << "\n\n";

        // Save character appearance
        out << "[CharacterAppearance]\n";
        out << "ArmorClass=" << static_cast<int>(_appearance.armorClass) << "\n";
        out << "HelmetType=" << static_cast<int>(_appearance.helmetType) << "\n";
        out << "VisorColor=" << _appearance.visorGlowColor.x << "," << _appearance.visorGlowColor.y << "," << _appearance.visorGlowColor.z << "\n";
        out << "FatiguesColor=" << _appearance.fatiguesColor.x << "," << _appearance.fatiguesColor.y << "," << _appearance.fatiguesColor.z << "\n";

        std::string content = out.str();

        std::vector<std::string> targetPaths = { filepath };
        if (filepath.find("character_studio.cfg") != std::string::npos) {
            std::vector<std::string> syncCandidates = {
                "assets/configs/character_studio.cfg",
                "build/Release/assets/configs/character_studio.cfg",
                "../assets/configs/character_studio.cfg",
                "../../assets/configs/character_studio.cfg"
            };
            for (const auto& sc : syncCandidates) {
                std::filesystem::path p(sc);
                if (std::filesystem::exists(p) || std::filesystem::exists(p.parent_path())) {
                    targetPaths.push_back(sc);
                }
            }
        }

        bool anySaved = false;
        for (const auto& tp : targetPaths) {
            try {
                std::filesystem::path p(tp);
                if (p.has_parent_path()) {
                    std::filesystem::create_directories(p.parent_path());
                }
                std::ofstream f(p);
                if (f.is_open()) {
                    f << content;
                    f.close();
                    anySaved = true;
                }
            } catch (...) {}
        }

        if (!anySaved) {
            log("ERROR: Could not open config for writing: " + filepath);
            return false;
        }

        log("Studio configuration successfully saved to: " + filepath);
        return true;
    }

    static Vec3 parseVec3(const std::string& str, const Vec3& def) {
        std::stringstream ss(str);
        std::string part;
        Vec3 v = def;
        if (std::getline(ss, part, ',')) v.x = std::stof(part);
        if (std::getline(ss, part, ',')) v.y = std::stof(part);
        if (std::getline(ss, part, ',')) v.z = std::stof(part);
        return v;
    }

    bool CharacterStudio::loadConfig(const std::string& filepath) {
        std::vector<std::string> candidates = {
            filepath,
            "assets/configs/character_studio.cfg",
            "build/Release/assets/configs/character_studio.cfg",
            "../assets/configs/character_studio.cfg",
            "../../assets/configs/character_studio.cfg"
        };
        std::string actualPath;
        std::filesystem::file_time_type newestTime;
        bool foundAny = false;

        for (const auto& c : candidates) {
            try {
                if (std::filesystem::exists(c)) {
                    auto wt = std::filesystem::last_write_time(c);
                    if (!foundAny || wt > newestTime) {
                        newestTime = wt;
                        actualPath = c;
                        foundAny = true;
                    }
                }
            } catch (...) {}
        }

        if (!foundAny || actualPath.empty()) {
            return false;
        }

        std::ifstream in(actualPath);
        if (!in.is_open()) return false;

        std::string line;
        std::string currentSection;

        while (std::getline(in, line)) {
            // Trim whitespace
            if (line.empty() || line[0] == '#' || line[0] == ';') continue;
            if (line.front() == '[' && line.back() == ']') {
                currentSection = line.substr(1, line.size() - 2);
                continue;
            }

            size_t eqPos = line.find('=');
            if (eqPos == std::string::npos) continue;

            std::string key = line.substr(0, eqPos);
            std::string val = line.substr(eqPos + 1);

            if (currentSection.rfind("WeaponGrip_", 0) == 0) {
                int id = std::stoi(currentSection.substr(11));
                if (id >= 0 && id < 9) {
                    if (key == "Offset") _weaponGrips[id].weaponOffset = parseVec3(val, _weaponGrips[id].weaponOffset);
                    else if (key == "Rotation") _weaponGrips[id].weaponRotation = parseVec3(val, _weaponGrips[id].weaponRotation);
                    else if (key == "Scale") _weaponGrips[id].weaponScale = parseVec3(val, _weaponGrips[id].weaponScale);
                    else if (key == "RightPos") _weaponGrips[id].rightSocketPos = parseVec3(val, _weaponGrips[id].rightSocketPos);
                    else if (key == "RightRot") _weaponGrips[id].rightSocketRot = parseVec3(val, _weaponGrips[id].rightSocketRot);
                    else if (key == "LeftPos") _weaponGrips[id].leftSocketPos = parseVec3(val, _weaponGrips[id].leftSocketPos);
                    else if (key == "LeftRot") _weaponGrips[id].leftSocketRot = parseVec3(val, _weaponGrips[id].leftSocketRot);
                    else if (key == "AdsOffset") _weaponGrips[id].adsOffset = parseVec3(val, _weaponGrips[id].adsOffset);
                    else if (key == "LockHands") _weaponGrips[id].lockHands = (val == "1" || val == "true" || val == "True");
                    else if (key == "BotOffset") _weaponGrips[id].botSocket.offset = parseVec3(val, _weaponGrips[id].botSocket.offset);
                    else if (key == "BotRotation") _weaponGrips[id].botSocket.rotation = parseVec3(val, _weaponGrips[id].botSocket.rotation);
                    else if (key == "BotScale") _weaponGrips[id].botSocket.scale = parseVec3(val, _weaponGrips[id].botSocket.scale);
                    else if (key == "Model" || key == "ModelFile") _weaponSkins[id].modelFile = val;
                }
            } else if (currentSection == "BotWeapon") {
                for (int i = 0; i < 9; ++i) {
                    if (key == "Offset") _weaponGrips[i].botSocket.offset = parseVec3(val, _weaponGrips[i].botSocket.offset);
                    else if (key == "Rotation") _weaponGrips[i].botSocket.rotation = parseVec3(val, _weaponGrips[i].botSocket.rotation);
                    else if (key == "Scale") _weaponGrips[i].botSocket.scale = parseVec3(val, _weaponGrips[i].botSocket.scale);
                }
            } else if (currentSection.rfind("WeaponSkin_", 0) == 0) {
                int id = std::stoi(currentSection.substr(11));
                if (id >= 0 && id < 9) {
                    if (key == "Model" || key == "ModelFile") _weaponSkins[id].modelFile = val;
                    else if (key == "Texture") _weaponSkins[id].textureFile = val;
                    else if (key == "UvScale") _weaponSkins[id].uvScale = std::stof(val);
                    else if (key == "Tint") _weaponSkins[id].tintColor = parseVec3(val, _weaponSkins[id].tintColor);
                    else if (key == "Metallic") _weaponSkins[id].metallic = std::stof(val);
                }
            } else if (currentSection == "ReloadTimeline") {
                if (key == "DipDuration") _reloadTimeline.dipDuration = std::stof(val);
                else if (key == "MagDropTime") _reloadTimeline.magDropTime = std::stof(val);
                else if (key == "MagInsertTime") _reloadTimeline.magInsertTime = std::stof(val);
                else if (key == "BoltRackTime") _reloadTimeline.boltRackTime = std::stof(val);
                else if (key == "DipDepth") _reloadTimeline.dipDepth = std::stof(val);
                else if (key == "TiltAngle") _reloadTimeline.tiltAngle = std::stof(val);
            } else if (currentSection == "CharacterAppearance") {
                if (key == "ArmorClass") _appearance.armorClass = static_cast<ArmorClass>(std::stoi(val));
                else if (key == "HelmetType") _appearance.helmetType = static_cast<HelmetType>(std::stoi(val));
                else if (key == "VisorColor") _appearance.visorGlowColor = parseVec3(val, _appearance.visorGlowColor);
                else if (key == "FatiguesColor") _appearance.fatiguesColor = parseVec3(val, _appearance.fatiguesColor);
            }
        }
        in.close();

        // Sync loaded configuration across candidate locations
        if (actualPath.find("character_studio.cfg") != std::string::npos) {
            std::vector<std::string> syncCandidates = {
                "assets/configs/character_studio.cfg",
                "build/Release/assets/configs/character_studio.cfg",
                "../assets/configs/character_studio.cfg",
                "../../assets/configs/character_studio.cfg"
            };
            for (const auto& sc : syncCandidates) {
                try {
                    std::filesystem::path sp(sc);
                    if (sc != actualPath && (std::filesystem::exists(sp) || std::filesystem::exists(sp.parent_path()))) {
                        std::filesystem::copy_file(actualPath, sp, std::filesystem::copy_options::overwrite_existing);
                    }
                } catch (...) {}
            }
        }

        log("Loaded user studio configuration: " + actualPath);
        return true;
    }

} // namespace Lab
