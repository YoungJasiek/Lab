#include "LabStudio.h"
#include "Lab.h"
#include "LabFont.h"
#include <glad/gl.h>
#include <GLFW/glfw3.h>
#include <fstream>
#include <sstream>
#include <iostream>
#include <filesystem>
#include <cmath>
#include <algorithm>
#include <iomanip>

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
        _weaponSkins[0] = { "weapon_pipe.bmp", 1.0f, Vec3(1.0f, 1.0f, 1.0f), 0.7f, 0.3f };

        // Weapon 1: Pistol
        _weaponGrips[1] = {
            Vec3(-0.015f, -0.065f, -0.04f), Vec3(8.0f, 0.0f, -4.0f),
            Vec3(-0.055f, -0.105f, -0.035f), Vec3(14.0f, -12.0f, 15.0f),
            Vec3(0.0f, -0.06f, 0.12f)
        };
        _weaponSkins[1] = { "weapon_pistol.bmp", 1.0f, Vec3(1.0f, 1.0f, 1.0f), 0.8f, 0.2f };

        // Weapon 2: Shotgun
        _weaponGrips[2] = {
            Vec3(-0.015f, -0.065f, -0.04f), Vec3(8.0f, 0.0f, -4.0f),
            Vec3(-0.065f, -0.060f, -0.26f), Vec3(22.0f, 12.0f, -22.0f),
            Vec3(0.0f, -0.07f, 0.14f)
        };
        _weaponSkins[2] = { "weapon_shotgun.bmp", 1.0f, Vec3(1.0f, 1.0f, 1.0f), 0.6f, 0.4f };

        // Weapon 3: M4A4-S
        _weaponGrips[3] = {
            Vec3(-0.015f, -0.065f, -0.04f), Vec3(8.0f, 0.0f, -4.0f),
            Vec3(-0.065f, -0.060f, -0.34f), Vec3(22.0f, 12.0f, -22.0f),
            Vec3(0.0f, -0.07f, 0.15f)
        };
        _weaponSkins[3] = { "weapon_m4a4s.bmp", 1.0f, Vec3(1.0f, 1.0f, 1.0f), 0.6f, 0.4f };

        // Weapon 4: SG553
        _weaponGrips[4] = {
            Vec3(-0.015f, -0.065f, -0.04f), Vec3(8.0f, 0.0f, -4.0f),
            Vec3(-0.065f, -0.060f, -0.34f), Vec3(22.0f, 12.0f, -22.0f),
            Vec3(0.0f, -0.07f, 0.15f)
        };
        _weaponSkins[4] = { "weapon_sg553.bmp", 1.0f, Vec3(1.0f, 1.0f, 1.0f), 0.5f, 0.5f };

        // Weapon 5: Minigun
        _weaponGrips[5] = {
            Vec3(0.0f, -0.05f, 0.02f), Vec3(0.0f, 0.0f, 0.0f),
            Vec3(0.0f, 0.08f, -0.15f), Vec3(-15.0f, 0.0f, 0.0f),
            Vec3(0.0f, -0.05f, 0.10f)
        };
        _weaponSkins[5] = { "weapon_minigun.bmp", 1.0f, Vec3(1.0f, 1.0f, 1.0f), 0.8f, 0.2f };

        // Weapon 6: Plasma Rifle
        _weaponGrips[6] = {
            Vec3(-0.015f, -0.065f, -0.04f), Vec3(8.0f, 0.0f, -4.0f),
            Vec3(-0.065f, -0.060f, -0.30f), Vec3(20.0f, 10.0f, -20.0f),
            Vec3(0.0f, -0.07f, 0.15f)
        };
        _weaponSkins[6] = { "weapon_plasma.bmp", 1.0f, Vec3(1.0f, 1.0f, 1.0f), 0.9f, 0.1f };

        // Weapon 7: Railgun
        _weaponGrips[7] = {
            Vec3(-0.015f, -0.065f, -0.04f), Vec3(8.0f, 0.0f, -4.0f),
            Vec3(-0.065f, -0.060f, -0.36f), Vec3(20.0f, 10.0f, -20.0f),
            Vec3(0.0f, -0.07f, 0.15f)
        };
        _weaponSkins[7] = { "weapon_railgun.bmp", 1.0f, Vec3(1.0f, 1.0f, 1.0f), 0.9f, 0.1f };

        // Weapon 8: RPG
        _weaponGrips[8] = {
            Vec3(-0.02f, -0.08f, -0.02f), Vec3(5.0f, 0.0f, -2.0f),
            Vec3(-0.06f, -0.05f, -0.28f), Vec3(25.0f, 15.0f, -15.0f),
            Vec3(0.0f, -0.08f, 0.16f)
        };
        _weaponSkins[8] = { "weapon_rpg.bmp", 1.0f, Vec3(1.0f, 1.0f, 1.0f), 0.4f, 0.6f };

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
    }

    void CharacterStudio::init() {
        LabLog::info("CharacterStudio: Initializing Valve Hammer styled Studio...");

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

        // Build 3D Ground Grid (Valve Hammer green/grey coordinate plane)
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

        // Try load saved config
        loadConfig("assets/configs/character_studio.cfg");

        log("Valve Hammer Character & Weapon Studio ready.");
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
    }

    Texture* CharacterStudio::getTexture(const std::string& filename) {
        if (filename.empty()) return nullptr;
        auto it = _textures.find(filename);
        if (it != _textures.end()) return it->second.get();

        std::vector<std::string> searchPaths = {
            "assets/textures/" + filename,
            "assets/" + filename,
            filename
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
        return nullptr;
    }

    Mesh* CharacterStudio::getMesh(const std::string& filename) {
        if (filename.empty()) return nullptr;
        auto it = _meshes.find(filename);
        if (it != _meshes.end()) return it->second.get();

        std::vector<std::string> searchPaths = {
            "assets/models/" + filename,
            "assets/" + filename,
            filename
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

        // Viewport Dragging (LMB or RMB inside 3D viewport)
        // Viewport bounds: x: 70..w-380, y: 52..h-165
        float vpX = 70.0f;
        float vpY = 52.0f;

        if (lmbPressed && mouseX >= vpX && mouseY >= vpY && !_isDraggingScrubber && _activeSliderId == -1) {
            _turntableYaw += dx * 0.4f;
            _turntablePitch = std::clamp(_turntablePitch + dy * 0.4f, -85.0f, 85.0f);
        }

        // RMB Orbit / Zoom inside viewport
        if (rmbPressed && mouseX >= vpX && mouseY >= vpY) {
            _cameraDist = std::clamp(_cameraDist + dy * 0.01f, 0.4f, 5.0f);
        }

        // Scroll wheel zoom
        if (std::abs(scrollDelta) > 0.001f) {
            _cameraDist = std::clamp(_cameraDist - scrollDelta * 0.15f, 0.4f, 5.0f);
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

        _lastMouseX = mouseX;
        _lastMouseY = mouseY;
        _lastLmb = lmbPressed;

        if (!lmbPressed) {
            _activeSliderId = -1;
            _isDraggingScrubber = false;
        }
    }

    void CharacterStudio::render(int screenWidth, int screenHeight) {
        float leftBarW = 70.0f;
        float rightPanelW = 390.0f;
        float topBarsH = 54.0f;
        float bottomBarsH = 160.0f;

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

        // Model lookup (STL or procedural)
        const char* stlNames[9] = {
            "pipe.stl", "pistol.stl", "shotgun.stl",
            "m4a4s.stl", "sg553.stl", "minigun.stl",
            "plasma.stl", "railgun.stl", "rpg.stl"
        };
        Mesh* weaponMesh = getMesh(stlNames[_selectedWeaponIndex]);

        // Weapon Base Transform
        Vec3 weaponPos(0.0f, 0.0f, 0.0f);
        Vec3 weaponRot(0.0f, 0.0f, 0.0f);

        // Render Weapon with active skin and tint
        if (weaponMesh) {
            Renderer::drawMesh(*weaponMesh, weaponPos, weaponRot, Vec3(1.0f, 1.0f, 1.0f), skin.tintColor, skinTex);
        } else {
            // High-detail procedural weapon fallback
            Renderer::drawCube(weaponPos, weaponRot, Vec3(0.06f, 0.12f, 0.65f), skin.tintColor, skinTex, true);
            Renderer::drawCube(weaponPos + Vec3(0.0f, -0.08f, -0.06f), weaponRot + Vec3(-12.0f, 0.0f, 0.0f), Vec3(0.045f, 0.11f, 0.05f), Vec3(0.12f, 0.13f, 0.15f), nullptr, true);
        }

        // Render Tactical Arms aligned to configured sockets!
        if (_arms && _animator) {
            _arms->render(weaponPos, weaponRot, wid, *_animator, nullptr,
                          &grip.rightSocketPos, &grip.rightSocketRot,
                          &grip.leftSocketPos, &grip.leftSocketRot);
        }

        // Render Socket Tripod Gizmos
        if (_showGizmos) {
            renderSocketGizmo(weaponPos + grip.rightSocketPos, weaponRot + grip.rightSocketRot);
            renderSocketGizmo(weaponPos + grip.leftSocketPos, weaponRot + grip.leftSocketRot);
        }
    }

    void CharacterStudio::renderReloadTimelineView() {
        const auto& grip = _weaponGrips[_selectedWeaponIndex];
        const auto& skin = _weaponSkins[_selectedWeaponIndex];
        WeaponID wid = static_cast<WeaponID>(_selectedWeaponIndex);
        Texture* skinTex = getTexture(skin.textureFile);

        const char* stlNames[9] = {
            "pipe.stl", "pistol.stl", "shotgun.stl",
            "m4a4s.stl", "sg553.stl", "minigun.stl",
            "plasma.stl", "railgun.stl", "rpg.stl"
        };
        Mesh* weaponMesh = getMesh(stlNames[_selectedWeaponIndex]);

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

        Vec3 weaponPos(0.0f, dipY, 0.0f);
        Vec3 weaponRot(0.0f, 0.0f, tiltRoll);

        if (weaponMesh) {
            Renderer::drawMesh(*weaponMesh, weaponPos, weaponRot, Vec3(1.0f, 1.0f, 1.0f), skin.tintColor, skinTex);
        } else {
            Renderer::drawCube(weaponPos, weaponRot, Vec3(0.06f, 0.12f, 0.65f), skin.tintColor, skinTex, true);
        }

        // Draw fresh or dropping magazine
        if (magDetached) {
            Vec3 magPos = weaponPos + Vec3(0.0f, -0.15f + magY, -0.05f);
            Renderer::drawCube(magPos, weaponRot, Vec3(0.035f, 0.14f, 0.065f), Vec3(0.12f, 0.14f, 0.16f), nullptr, true);
        }

        // Arms following reload kinematics
        if (_arms && _animator) {
            Vec3 leftHandOffset = grip.leftSocketPos + Vec3(0.0f, magY * 0.7f, 0.0f);
            _arms->render(weaponPos, weaponRot, wid, *_animator, nullptr,
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
        bool clicked = (hovered && _lmbClicked);

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
        if (_lmbPressed && _mouseX >= trackX && _mouseX <= trackX + trackW && _mouseY >= y && _mouseY <= y + h) {
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

        struct MenuBtn { std::string name; float x; float w; };
        MenuBtn items[] = {
            { "File", 6.0f, 42.0f },
            { "Edit", 52.0f, 42.0f },
            { "View", 98.0f, 42.0f },
            { "Tools", 144.0f, 46.0f },
            { "Help", 194.0f, 42.0f }
        };

        for (int i = 0; i < 5; ++i) {
            bool hov = (_mouseX >= items[i].x && _mouseX <= items[i].x + items[i].w && _mouseY >= 2.0f && _mouseY <= 22.0f);
            if (hov) {
                Renderer::drawRect(items[i].x, 2.0f, items[i].w, 20.0f, Vec3(0.35f, 0.38f, 0.42f));
                drawHammerBevel(items[i].x, 2.0f, items[i].w, 20.0f, false);
            }
            LabFont::drawText(items[i].x + 8.0f, 5.0f, items[i].name, 1.4f, Vec3(0.95f, 0.95f, 0.95f), LabFontType::System);
        }

        // Title Tag
        LabFont::drawText(w - 380.0f, 5.0f, "Valve Hammer Character & Weapon Studio - [Lab Studio 2026]", 1.4f, Vec3(0.85f, 0.88f, 0.92f), LabFontType::System);
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
    }

    void CharacterStudio::renderHammerLeftToolPalette(float h) {
        float barW = 70.0f;
        float barY = 52.0f;
        float barH = h - barY - 160.0f;

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

            if (drawHammerButton(6.0f, btnY, 58.0f, 48.0f, tools[i].code, active)) {
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
                 "Weapon: %s | Right Socket: (%.2f, %.2f, %.2f) | Left Socket: (%.2f, %.2f, %.2f) | Style: Valve Hammer",
                 weaponNames[_selectedWeaponIndex],
                 grip.rightSocketPos.x, grip.rightSocketPos.y, grip.rightSocketPos.z,
                 grip.leftSocketPos.x, grip.leftSocketPos.y, grip.leftSocketPos.z);

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
        for (int i = 0; i < 9; ++i) {
            float bx = x + 10.0f + (i % 3) * 122.0f;
            float by = curY + (i / 3) * 26.0f;
            if (drawHammerButton(bx, by, 116.0f, 22.0f, shortNames[i], _selectedWeaponIndex == i)) {
                _selectedWeaponIndex = i;
                log(std::string("Loaded grip & skin profile for: ") + shortNames[i]);
            }
        }
        curY += 84.0f;

        auto& grip = _weaponGrips[_selectedWeaponIndex];

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

        // Group Box: ADS Alignment
        Renderer::drawRect(x + 8.0f, curY, w - 16.0f, 80.0f, Vec3(0.18f, 0.19f, 0.21f));
        drawHammerBevel(x + 8.0f, curY, w - 16.0f, 80.0f, true);
        LabFont::drawText(x + 14.0f, curY + 6.0f, "ADS OPTICAL CENTER ALIGNMENT:", 1.4f, Vec3(0.95f, 0.65f, 0.2f), LabFontType::System);

        drawHammerSlider(x + 14.0f, curY + 26.0f, w - 28.0f, 18.0f, "Sight X Align:", grip.adsOffset.x, -0.10f, 0.10f);
        drawHammerSlider(x + 14.0f, curY + 46.0f, w - 28.0f, 18.0f, "Sight Y Height:", grip.adsOffset.y, -0.15f, 0.05f);
        curY += 88.0f;

        // Action Buttons
        if (drawHammerButton(x + 12.0f, curY, 175.0f, 26.0f, "Reset This Grip")) {
            resetDefaults();
            log("Reset weapon grip to default anatomical socket pose.");
        }
        if (drawHammerButton(x + 195.0f, curY, 175.0f, 26.0f, "Apply In-Game", false, true)) {
            saveConfig("assets/configs/character_studio.cfg");
            log("Saved and applied custom weapon sockets to active game!");
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
        if (drawHammerButton(x + 12.0f, curY, 70.0f, 24.0f, _scrubberPlaying ? "PAUSE" : "PLAY")) {
            _scrubberPlaying = !_scrubberPlaying;
        }
        if (drawHammerButton(x + 88.0f, curY, 65.0f, 24.0f, "STOP")) {
            _scrubberPlaying = false;
            _scrubberPos = 0.0f;
        }
        if (drawHammerButton(x + 158.0f, curY, 95.0f, 24.0f, _scrubberLoop ? "LOOP: ON" : "LOOP: OFF", _scrubberLoop)) {
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
        if (drawHammerButton(x + 12.0f, curY, 110.0f, 22.0f, "Mag Out SFX")) {
            AudioEngine::playSound(SoundID::Reload, 1.0f, 1.15f);
            log("Auditioned sound: Mag Out SFX");
        }
        if (drawHammerButton(x + 128.0f, curY, 110.0f, 22.0f, "Mag In SFX")) {
            AudioEngine::playSound(SoundID::Reload, 1.0f, 0.95f);
            log("Auditioned sound: Mag In SFX");
        }
        if (drawHammerButton(x + 244.0f, curY, 110.0f, 22.0f, "Bolt Rack SFX")) {
            AudioEngine::playSound(SoundID::Reload, 1.0f, 1.35f);
            log("Auditioned sound: Bolt Rack SFX");
        }
    }

    void CharacterStudio::renderTabWeaponSkins(float x, float y, float w, float h) {
        (void)h;
        float curY = y + 26.0f;

        LabFont::drawText(x + 10.0f, curY, "SURFACE TEXTURES & CAMO MATERIALS", 1.5f, Vec3(0.95f, 0.85f, 0.3f), LabFontType::System);
        curY += 20.0f;

        auto& skin = _weaponSkins[_selectedWeaponIndex];

        struct SkinPreset { std::string name; std::string file; };
        SkinPreset skins[] = {
            { "Standard Army", "weapon_m4a4s.bmp" },
            { "Frostbite Glaze", "cryo_ice.bmp" },
            { "Titanium Hull", "metal_hull.bmp" },
            { "Hazard Caution", "hazard_stripes.bmp" },
            { "Sub-Zero Camo", "snow_frost.bmp" }
        };

        for (int i = 0; i < 5; ++i) {
            float by = curY + i * 26.0f;
            bool active = (skin.textureFile == skins[i].file);
            if (drawHammerButton(x + 12.0f, by, w - 24.0f, 22.0f, skins[i].name + " (" + skins[i].file + ")", active)) {
                skin.textureFile = skins[i].file;
                log("Applied material texture: " + skins[i].name);
            }
        }
        curY += 140.0f;

        // Material Parameters Group
        Renderer::drawRect(x + 8.0f, curY, w - 16.0f, 140.0f, Vec3(0.18f, 0.19f, 0.21f));
        drawHammerBevel(x + 8.0f, curY, w - 16.0f, 140.0f, true);
        LabFont::drawText(x + 14.0f, curY + 6.0f, "MATERIAL PROPERTIES & SHADING:", 1.4f, Vec3(0.4f, 0.85f, 1.0f), LabFontType::System);

        drawHammerSlider(x + 14.0f, curY + 26.0f, w - 28.0f, 18.0f, "UV Tiling:", skin.uvScale, 0.25f, 4.0f);
        drawHammerSlider(x + 14.0f, curY + 46.0f, w - 28.0f, 18.0f, "Tint Red:", skin.tintColor.x, 0.1f, 1.5f);
        drawHammerSlider(x + 14.0f, curY + 66.0f, w - 28.0f, 18.0f, "Tint Green:", skin.tintColor.y, 0.1f, 1.5f);
        drawHammerSlider(x + 14.0f, curY + 86.0f, w - 28.0f, 18.0f, "Tint Blue:", skin.tintColor.z, 0.1f, 1.5f);
        drawHammerSlider(x + 14.0f, curY + 106.0f, w - 28.0f, 18.0f, "Metallic/Rough:", skin.metallic, 0.0f, 1.0f);
        curY += 150.0f;

        if (drawHammerButton(x + 12.0f, curY, w - 24.0f, 26.0f, "Save Weapon Skin Preset", false, true)) {
            saveConfig("assets/configs/character_studio.cfg");
            log("Saved weapon skin config to assets/configs/character_studio.cfg");
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
        if (drawHammerButton(x + 12.0f, curY, 110.0f, 24.0f, "Light Scout", _appearance.armorClass == ArmorClass::LightScout)) {
            _appearance.armorClass = ArmorClass::LightScout;
            log("Operative Armor set to: Light Scout");
        }
        if (drawHammerButton(x + 128.0f, curY, 110.0f, 24.0f, "Cryo Marine", _appearance.armorClass == ArmorClass::CryoMarine)) {
            _appearance.armorClass = ArmorClass::CryoMarine;
            log("Operative Armor set to: Heavy Cryo Marine");
        }
        if (drawHammerButton(x + 244.0f, curY, 110.0f, 24.0f, "Tactical Officer", _appearance.armorClass == ArmorClass::TacticalOfficer)) {
            _appearance.armorClass = ArmorClass::TacticalOfficer;
            log("Operative Armor set to: Tactical Officer");
        }
        curY += 34.0f;

        // Helmet Type
        LabFont::drawText(x + 12.0f, curY, "Headgear & Visor:", 1.4f, Vec3(0.85f, 0.90f, 0.95f), LabFontType::System);
        curY += 18.0f;
        if (drawHammerButton(x + 12.0f, curY, 110.0f, 24.0f, "Combat Visor", _appearance.helmetType == HelmetType::CombatVisor)) {
            _appearance.helmetType = HelmetType::CombatVisor;
        }
        if (drawHammerButton(x + 128.0f, curY, 110.0f, 24.0f, "Sealed Helmet", _appearance.helmetType == HelmetType::SealedHelmet)) {
            _appearance.helmetType = HelmetType::SealedHelmet;
        }
        if (drawHammerButton(x + 244.0f, curY, 110.0f, 24.0f, "Tactical Beanie", _appearance.helmetType == HelmetType::TacticalBeanie)) {
            _appearance.helmetType = HelmetType::TacticalBeanie;
        }
        curY += 34.0f;

        // Visor LED Color
        LabFont::drawText(x + 12.0f, curY, "Visor HUD LED Glow:", 1.4f, Vec3(0.85f, 0.90f, 0.95f), LabFontType::System);
        curY += 18.0f;
        if (drawHammerButton(x + 12.0f, curY, 82.0f, 22.0f, "Cyan")) _appearance.visorGlowColor = Vec3(0.2f, 0.85f, 1.0f);
        if (drawHammerButton(x + 100.0f, curY, 82.0f, 22.0f, "Amber")) _appearance.visorGlowColor = Vec3(1.0f, 0.75f, 0.1f);
        if (drawHammerButton(x + 188.0f, curY, 82.0f, 22.0f, "Crimson")) _appearance.visorGlowColor = Vec3(1.0f, 0.2f, 0.2f);
        if (drawHammerButton(x + 276.0f, curY, 82.0f, 22.0f, "Acid Green")) _appearance.visorGlowColor = Vec3(0.2f, 1.0f, 0.3f);
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

        if (drawHammerButton(x + 14.0f, curY, 155.0f, 26.0f, _dialoguePlaying ? "SPEAKING..." : "TEST DIALOGUE", _dialoguePlaying, true)) {
            _dialoguePlaying = true;
            _dialogueTimer = 0.0f;
            log("Started speech playback and phoneme lip-sync track.");
        }
        if (drawHammerButton(x + 180.0f, curY, 155.0f, 26.0f, "Reset Mimics")) {
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

        Renderer::beginUI(screenWidth, screenHeight);

        // 1. Top Menu Bar
        renderHammerTopMenuBar(w);

        // 2. Top Toolbar
        renderHammerToolbar(w);

        // 3. Left Tool Palette
        renderHammerLeftToolPalette(h);

        // 4. Center 3D Viewport Frame Border
        float vpX = 70.0f;
        float vpY = 52.0f;
        float vpW = w - 70.0f - 390.0f;
        float vpH = h - 52.0f - 160.0f;
        drawHammerBevel(vpX, vpY, vpW, vpH, true);

        // Viewport Header Label (Valve Hammer iconic text)
        Renderer::drawRect(vpX + 4.0f, vpY + 4.0f, 320.0f, 20.0f, Vec3(0.10f, 0.12f, 0.14f));
        LabFont::drawText(vpX + 8.0f, vpY + 7.0f, "[camera 3D shaded] | Grid: 16 | Turntable 360", 1.4f, Vec3(0.4f, 0.95f, 0.4f), LabFontType::System);

        // 5. Right Inspector Panel
        renderHammerRightInspector(w - 390.0f, 52.0f, 390.0f, h - 52.0f - 160.0f);

        // 6. Bottom Console Panel
        renderHammerConsole(70.0f, h - 160.0f, w - 70.0f, 136.0f);

        // 7. Bottom Status Bar
        renderHammerStatusBar(w, h);

        Renderer::endUI();
    }

    // =========================================================================
    // Configuration File Persistence (.cfg)
    // =========================================================================

    bool CharacterStudio::saveConfig(const std::string& filepath) {
        std::filesystem::path p(filepath);
        if (p.has_parent_path()) {
            std::filesystem::create_directories(p.parent_path());
        }

        std::ofstream out(filepath);
        if (!out.is_open()) {
            log("ERROR: Could not open config for writing: " + filepath);
            return false;
        }

        out << "# Frozen-Life Character & Weapon Studio Configuration\n";
        out << "[Studio]\nVersion=1.0\n\n";

        // Save all 9 weapon grips and skins
        for (int i = 0; i < 9; ++i) {
            out << "[WeaponGrip_" << i << "]\n";
            out << "RightPos=" << _weaponGrips[i].rightSocketPos.x << "," << _weaponGrips[i].rightSocketPos.y << "," << _weaponGrips[i].rightSocketPos.z << "\n";
            out << "RightRot=" << _weaponGrips[i].rightSocketRot.x << "," << _weaponGrips[i].rightSocketRot.y << "," << _weaponGrips[i].rightSocketRot.z << "\n";
            out << "LeftPos=" << _weaponGrips[i].leftSocketPos.x << "," << _weaponGrips[i].leftSocketPos.y << "," << _weaponGrips[i].leftSocketPos.z << "\n";
            out << "LeftRot=" << _weaponGrips[i].leftSocketRot.x << "," << _weaponGrips[i].leftSocketRot.y << "," << _weaponGrips[i].leftSocketRot.z << "\n";
            out << "AdsOffset=" << _weaponGrips[i].adsOffset.x << "," << _weaponGrips[i].adsOffset.y << "," << _weaponGrips[i].adsOffset.z << "\n\n";

            out << "[WeaponSkin_" << i << "]\n";
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

        out.close();
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
        if (!std::filesystem::exists(filepath)) {
            return false;
        }

        std::ifstream in(filepath);
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
                    if (key == "RightPos") _weaponGrips[id].rightSocketPos = parseVec3(val, _weaponGrips[id].rightSocketPos);
                    else if (key == "RightRot") _weaponGrips[id].rightSocketRot = parseVec3(val, _weaponGrips[id].rightSocketRot);
                    else if (key == "LeftPos") _weaponGrips[id].leftSocketPos = parseVec3(val, _weaponGrips[id].leftSocketPos);
                    else if (key == "LeftRot") _weaponGrips[id].leftSocketRot = parseVec3(val, _weaponGrips[id].leftSocketRot);
                    else if (key == "AdsOffset") _weaponGrips[id].adsOffset = parseVec3(val, _weaponGrips[id].adsOffset);
                }
            } else if (currentSection.rfind("WeaponSkin_", 0) == 0) {
                int id = std::stoi(currentSection.substr(11));
                if (id >= 0 && id < 9) {
                    if (key == "Texture") _weaponSkins[id].textureFile = val;
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

        log("Loaded user studio configuration: " + filepath);
        return true;
    }

} // namespace Lab
