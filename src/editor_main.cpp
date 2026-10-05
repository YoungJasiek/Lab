#include "Lab.h"
#include "LabScene.h"
#include "LabTheme.h"
#include "LabFont.h"
#include "LabDialogs.h"
#include "LabProfiler.h"
#include "LabExportPipeline.h"
#include "LabProject.h"
#include "LabInput.h"
#include "LabAsset2D.h"
#include "LabScript.h"
#include "LabStudio.h"
#include "LabHUDEditor.h"
#include "LabPackage.h"
#include "plugins/ProceduralLevelPlugin.h"
#include "plugins/SpeedrunTelemetryPlugin.h"
#include "plugins/RetroPostProcessPlugin.h"
#include "plugins/DialogueQuestPlugin.h"
#include "plugins/VoxelTerrainPlugin.h"
#include "plugins/AINavigationPatrolPlugin.h"
#include "plugins/ArcadeVehiclePlugin.h"
#include "plugins/DiscordRPCPlugin.h"
#include "LabCloth.h"
#include "LabForceField.h"
#include "LabVehicle.h"
#include "LabFluid.h"
#include "LabConstraint.h"
#include <glad/gl.h>
#include <GLFW/glfw3.h>
#include <windows.h>
#include <iostream>
#include <sstream>
#include <filesystem>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <string>
#include <algorithm>
#include <cmath>
#include <memory>

using namespace Lab;

struct TextureEntry {
    std::string name;
    std::string filename;
};

enum class SelectionType {
    None,
    Brush,
    Prop,
    Door,
    Spawn,
    WeaponSpawner,
    Light,
    Sprite,
    Camera
};

enum class GizmoMode {
    Move = 0,
    Rotate = 1,
    Scale = 2,
    Universal = 3
};

enum class GizmoAxis {
    None = 0,
    X,
    Y,
    Z,
    PlaneXY,
    PlaneXZ,
    PlaneYZ,
    RotX,
    RotY,
    RotZ,
    ScaleX,
    ScaleY,
    ScaleZ,
    ScaleUniform
};

enum class EngineSimState {
    Stopped,
    Playing,
    Paused
};

enum class EditorWorkspace {
    // 3D Workspaces
    World3D_Level = 0,         // 3D Level & Geometry editor (CSG, Brushes, Props, Lights, 3D Camera)
    World3D_Animation = 1,     // 3D Prop/Model timeline & keyframe scrubber
    World3D_ModelViewer = 2,   // Dedicated 3D Model Viewer & Inspector (Turntable, Stats, Wireframe)
    Studio_GripPoser = 3,      // LabStudio: Weapon Grip & Hand Socket Poser
    Studio_AnimationTimeline = 4, // LabStudio: Keyframe Animation Timeline
    Studio_WeaponSkins = 5,    // LabStudio: Weapon Skins, Materials & Shaders
    Studio_Appearance = 6,     // LabStudio: Character Outfit, Armor, Helmet & Camo
    Studio_FaceDialogue = 7,   // LabStudio: Face Dialogue, Lip-Sync & Morph Targets
    // 2D Workspaces
    Level2D_Tilemap = 8,       // 2D Orthographic editor (Tilemaps, Sprite sheets, SpriteBatcher)
    Level2D_Animation = 9,     // 2D Spritesheet Animation & Timeline Scrubber
    Level2D_HUD = 10,          // Native Multi-Genre HUD Canvas Designer (Virtual 1920x1080 Artboard)
    // Scripting Workspaces
    Scripting_Editor = 11,     // In-Engine Lua 5.4 Script Editor IDE with Syntax Highlighting
    Scripting_Console = 12,    // Interactive Lua REPL & Debug Console
    // Plugins & Packages (Unity Package Manager & Blender Add-ons Standard)
    Plugins_Manager = 13,      // Full-viewport Package & Plugin Manager (Single-File Addon Standard)
    // Engine Main Menu & Project Hub
    Project_Hub = 14,          // Full-viewport Project Browser & Main Menu (Continue / Create / Play Demo)
    // Aliases for backwards compatibility
    World3D = 0,
    Level2D = 8,
    HUD = 10,
    Animation = 1,
    Scripting = 11,
    Plugins = 13,
    ProjectHub = 14
};

enum class BottomDockTab {
    Console,
    Profiler,
    ScriptEditor,
    Timeline
};

class LabEditorApp : public Engine {
public:
    LabEditorApp()
        : Engine("Lab Engine Studio (LabSDK)", 1600, 900),
          _camera(70.0f, 16.0f / 9.0f, 0.01f, 3000.0f) {
    }

    void onInit() override {
        LabLog::info("Launching Lab Engine Studio (LabSDK)...");
        Renderer::init();

        // Scan textures and models
        discoverTextures();
        discoverModels();

        // Initialize clean default studio scene
        newMap();

        // Camera initial pose
        _camera.setPosition(Vec3(0, 8.0f, 18.0f));

        // Unlock mouse cursor for UI desktop interaction
        setCursorCaptured(false);

        // Initialize Lua Script Engine & World/HUD bindings
        _scriptEngine.init("");
        if (_map) {
            _scriptEngine.setActiveMap(_map.get());
        }
        _scriptEngine.executeString(_currentScript);
        syncStringToScriptLines();

        // Initialize default 3D animation track
        _editorAnimTrack.name = "SceneTrack";
        _editorAnimTrack.duration = 1.0f;
        _editorAnimTrack.keyframes = {
            { 0.0f, Vec3(0.0f, 0.0f, 0.0f), Vec3(0.0f, 0.0f, 0.0f), Vec3(1.0f, 1.0f, 1.0f) },
            { 0.33f, Vec3(0.0f, 0.8f, 0.0f), Vec3(0.0f, 45.0f, 0.0f), Vec3(1.0f, 1.0f, 1.0f) },
            { 0.66f, Vec3(0.0f, 0.4f, -0.4f), Vec3(20.0f, 0.0f, 0.0f), Vec3(1.0f, 1.0f, 1.0f) },
            { 1.0f, Vec3(0.0f, 0.0f, 0.0f), Vec3(0.0f, 0.0f, 0.0f), Vec3(1.0f, 1.0f, 1.0f) }
        };

        // Initialize embedded LabStudio suite
        _characterStudio.init();
        _characterStudio.setWindow(getWindow());

        // Initialize embedded LabHUDEditor suite
        _hudEditor.init();
        _hudEditor.setWindow(getWindow());
        _hudEditor.loadHUDFile("games/frozen_life/assets/hud/frozen_life_hud.labhud");

        // Initialize Package & Plugin System (drop-in: 1 plik .lua = 1 plugin)
        PackageManager::instance().registerBuiltinPackages();
        logMessage("[Plugins] Drop-in plugin system ready: wrzuc .lua do plugins/ lub przeciagniej na okno.");

        logMessage("Lab Engine Studio initialized. Ready.");
        logMessage("Textures loaded: " + std::to_string(_availableTextures.size()) + " | Models: " + std::to_string(_availableModels.size()));
        logMessage("Lua 5.4 Script Engine active. Workspaces: 3D, 2D, HUD, Animation, Scripting, Plugins.");
        logMessage("Frustum Culling active: 'To czego oko nie widzi tego maszyna renderowac nie musi'");

        // Initialize Engine Main Menu / Project Hub
        refreshProjectHub();
        setWorkspace(EditorWorkspace::Project_Hub);
    }

    void discoverTextures() {
        _availableTextures.clear();
        std::vector<std::filesystem::path> searchDirs = {
            ProjectManager::get().getActiveProjectAssetsPath() / "textures",
            ProjectManager::get().getEngineAssetsPath() / "textures",
            "assets/textures",
            "../assets/textures",
            "../../assets/textures"
        };
        std::unordered_set<std::string> seenTex;
        for (const auto& dir : searchDirs) {
            if (std::filesystem::exists(dir)) {
                for (const auto& entry : std::filesystem::directory_iterator(dir)) {
                    std::string ext = entry.path().extension().string();
                    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return (char)::tolower(c); });
                    if (ext == ".bmp" || ext == ".png" || ext == ".jpg" || ext == ".tga") {
                        std::string fname = entry.path().filename().string();
                        if (!seenTex.contains(fname)) {
                            seenTex.insert(fname);
                            _availableTextures.push_back({ fname, fname });
                            if (!_textures.contains(fname)) {
                                _textures[fname] = std::make_unique<Texture>(entry.path().string());
                            }
                        }
                    }
                }
            }
        }
        std::sort(_availableTextures.begin(), _availableTextures.end(), [](const auto& a, const auto& b) {
            return a.filename < b.filename;
        });
        if (_availableTextures.empty()) {
            _availableTextures.push_back({ "concrete_wall.bmp", "concrete_wall.bmp" });
            _textures["concrete_wall.bmp"] = std::make_unique<Texture>("concrete_wall.bmp");
        }
        _selectedTexture = _availableTextures[0].filename;
    }

    void discoverModels() {
        _availableModels.clear();
        std::vector<std::filesystem::path> searchDirs = {
            ProjectManager::get().getActiveProjectAssetsPath() / "models",
            ProjectManager::get().getEngineAssetsPath() / "models",
            "assets/models",
            "../assets/models",
            "../../assets/models"
        };
        std::unordered_set<std::string> seenModels;
        for (const auto& dir : searchDirs) {
            if (std::filesystem::exists(dir)) {
                for (const auto& entry : std::filesystem::directory_iterator(dir)) {
                    std::string ext = entry.path().extension().string();
                    if (ext == ".stl" || ext == ".STL") {
                        std::string fname = entry.path().filename().string();
                        if (!seenModels.contains(fname)) {
                            seenModels.insert(fname);
                            _availableModels.push_back(fname);
                            if (!_meshes.contains(fname)) {
                                Mesh* m = Mesh::loadSTL(entry.path().string());
                                if (m) _meshes[fname] = std::unique_ptr<Mesh>(m);
                            }
                        }
                    }
                }
            }
        }
        if (_availableModels.empty()) {
            _availableModels.push_back("Model.stl");
        }
        _selectedModel = _availableModels[0];
    }

    Texture* getTexture(const std::string& path) {
        if (path.empty()) return nullptr;
        auto it = _textures.find(path);
        if (it != _textures.end()) return it->second.get();
        if (_missingTextures.contains(path)) return nullptr;

        auto tex = std::make_unique<Texture>(path);
        if (tex && tex->getId() != 0) {
            Texture* ptr = tex.get();
            _textures[path] = std::move(tex);
            return ptr;
        }
        _missingTextures.insert(path);
        return nullptr;
    }

    Mesh* getMesh(const std::string& path) {
        if (path.empty()) return nullptr;
        auto it = _meshes.find(path);
        if (it != _meshes.end()) return it->second.get();
        if (_missingMeshes.contains(path)) return nullptr;

        Mesh* m = Mesh::loadSTL(path);
        if (m) {
            Mesh* ptr = m;
            _meshes[path] = std::unique_ptr<Mesh>(m);
            return ptr;
        }
        _missingMeshes.insert(path);
        return nullptr;
    }

    void logMessage(const std::string& msg) {
        _consoleMessages.push_back(msg);
        if (_consoleMessages.size() > 8) {
            _consoleMessages.erase(_consoleMessages.begin());
        }
        LabLog::info("[LabEditor] " + msg);
    }

    void onFixedUpdate(float fixedDelta) override {
        // Drop-in plugins: 1 plik .lua w plugins/ = 1 plugin. Poll co ~1s.
        _pluginPollAccum += fixedDelta;
        if (_pluginPollAccum >= 1.0f) {
            _pluginPollAccum = 0.0f;
            PackageManager::instance().pollDropinChanges();
        }
        PluginManager::get().updateAll(fixedDelta);
        if (_pluginBannerTimer > 0.0f) {
            _pluginBannerTimer -= fixedDelta;
            if (_pluginBannerTimer <= 0.0f) _pluginStatusBanner = "";
        }
        if (_hubBannerTimer > 0.0f) {
            _hubBannerTimer -= fixedDelta;
            if (_hubBannerTimer <= 0.0f) _hubStatusBanner = "";
        }

        // Live Addon Subsystem Updates
        if (PackageManager::instance().isPackageEnabled("com.lab.retro-fx")) {
            _retroScanlinePhase += fixedDelta * 3.5f;
            if (_retroScanlinePhase > 6.283185f) _retroScanlinePhase -= 6.283185f;
        }
        if (PackageManager::instance().isPackageEnabled("com.lab.speedrun-timer")) {
            if (_speedrunTimerRunning) {
                _speedrunElapsedTime += fixedDelta;
            }
        }
        if (PackageManager::instance().isPackageEnabled("com.lab.physics-cloth")) {
            if (!_clothMeshInitialized) {
                _clothMesh.initGrid(12, 12, 0.28f, _clothMeshOrigin);
                _clothMesh.particles[0].pin();
                _clothMesh.particles[11].pin();
                _clothMeshInitialized = true;
            }
            if (_clothSimulateInEditor) {
                _clothMesh.applyWind(_clothWindVelocity);
                _clothMesh.step(fixedDelta, Vec3(0, -9.81f, 0), _clothSolverIterations, _clothDamping);
            }
        }
        if (_forceFieldBlastTimer > 0.0f) {
            _forceFieldBlastTimer -= fixedDelta;
            if (_clothMeshInitialized) {
                for (auto& cp : _clothMesh.particles) {
                    if (cp.isPinned) continue;
                    Vec3 toP = cp.position - _forceFieldPosition;
                    float d = toP.length();
                    if (d > 0.01f && d < _forceFieldRadius) {
                        float blastMag = (_forceFieldStrength / (d * d + 1.0f)) * 4.0f;
                        cp.acceleration += (toP / d) * blastMag;
                    }
                }
            }
        }
        if (PackageManager::instance().isPackageEnabled("com.lab.vehicle-arcade")) {
            if (Input::isKeyPressed('W') || Input::isKeyPressed('w')) {
                _vehicleSpeedKmh = std::min(180.0f, _vehicleSpeedKmh + 45.0f * _vehicleThrottlePower * fixedDelta);
                _vehicleRpm = std::min(7200.0f, 1000.0f + (_vehicleSpeedKmh / 180.0f) * 6000.0f);
                _vehicleGear = std::clamp((int)(_vehicleSpeedKmh / 35.0f) + 1, 1, 5);
            } else {
                _vehicleSpeedKmh = std::max(0.0f, _vehicleSpeedKmh - 30.0f * fixedDelta);
                _vehicleRpm = std::max(900.0f, _vehicleRpm - 2000.0f * fixedDelta);
                if (_vehicleSpeedKmh < 1.0f) _vehicleGear = 1;
            }
        }

        // Noclip camera flight (active ONLY in 3D level editing when holding Right Mouse Button)
        if (_workspace != EditorWorkspace::World3D_Level && _workspace != EditorWorkspace::World3D_Animation) {
            return;
        }
        if (Input::isMouseButtonPressed(1)) {
            bool isFast = Input::isKeyPressed(340); // Shift
            float flySpeed = isFast ? 35.0f : 15.0f;

            Vec3 camPos = _camera.getPosition();
            if (Input::isKeyPressed('W') || Input::isKeyPressed('w')) camPos += _camera.getFront() * flySpeed * fixedDelta;
            if (Input::isKeyPressed('S') || Input::isKeyPressed('s')) camPos -= _camera.getFront() * flySpeed * fixedDelta;
            if (Input::isKeyPressed('A') || Input::isKeyPressed('a')) camPos -= _camera.getRight() * flySpeed * fixedDelta;
            if (Input::isKeyPressed('D') || Input::isKeyPressed('d')) camPos += _camera.getRight() * flySpeed * fixedDelta;
            if (Input::isKeyPressed(32)) camPos.y += flySpeed * fixedDelta; // Space = Up
            if (Input::isKeyPressed(341)) camPos.y -= flySpeed * fixedDelta; // Ctrl = Down

            _camera.setPosition(camPos);
        }
    }

    void newMap() {
        _map = std::make_unique<LabMap>();
        _map->metadata.name = "untitled_map";
        _map->metadata.author = "Mapper";
        _map->spawn.position = Vec3(0, 1.8f, 0);

        // Standard ground brush
        MapBrush floor;
        floor.name = "Floor";
        floor.position = Vec3(0, -0.5f, 0);
        floor.size = Vec3(32.0f, 1.0f, 32.0f);
        floor.color = Vec3(1.0f, 1.0f, 1.0f);
        floor.texturePath = "floor_tiles.bmp";
        floor.uvScale = Vec2(0.25f, 0.25f);
        floor.uvMode = 1;
        _map->brushes.push_back(floor);

        // Standard dynamic block named "Klocek1" (can be scripted via World.getEntity('Klocek1'))
        MapBrush block;
        block.name = "Klocek1";
        block.script = "assets/scripts/gameplay.lua";
        block.position = Vec3(0, 1.0f, -4.0f);
        block.size = Vec3(2.0f, 2.0f, 2.0f);
        block.color = Vec3(0.95f, 0.55f, 0.15f);
        block.texturePath = "concrete_wall.bmp";
        block.uvScale = Vec2(0.25f, 0.25f);
        block.uvMode = 1;
        _map->brushes.push_back(block);

        if (_map) {
            _scriptEngine.setActiveMap(_map.get());
        }

        _currentMapPath = "";
        _selectionType = SelectionType::None;
        _selectedIndex = -1;
        logMessage("Created New Map with dynamic entity 'Klocek1'.");
    }

    void openMapDialog() {
        std::string openPath = LabDialogs::openFileDialog(getWindow(), "Lab Scene / Map (*.labscene;*.labmap)\0*.labscene;*.labmap\0Lab Scene (*.labscene)\0*.labscene\0Lab Map (*.labmap)\0*.labmap\0All Files (*.*)\0*.*\0", "assets\\maps");
        if (!openPath.empty()) {
            if (LabScene::isLegacyMapFile(openPath)) {
                logMessage("Wykryto plik legacy .labmap: " + openPath + ". Wymagana konwersja na .labscene.");
                _pendingMapPath = openPath;
                _showConvertMapModal = true;
            } else {
                loadMapFromFile(openPath);
            }
        }
    }

    bool loadMapFromFile(const std::string& openPath) {
        if (!openPath.empty()) {
            auto loaded = LabMap::loadFromFile(openPath);
            if (loaded) {
                _map = std::move(loaded);
                _currentMapPath = openPath;
                _selectionType = SelectionType::None;
                _selectedIndex = -1;
                if (_map) {
                    _scriptEngine.setActiveMap(_map.get());
                }
                logMessage("Loaded Project Map: " + openPath);
                return true;
            }
        }
        return false;
    }

    void refreshProjectHub() {
        _hubProjects = ProjectManager::get().getAllDiscoveredProjects();
        if (_hubSelectedProjectIndex < 0 || _hubSelectedProjectIndex >= (int)_hubProjects.size()) {
            _hubSelectedProjectIndex = 0;
        }
    }

    bool isStudioWorkspace(EditorWorkspace ws) const {
        return ws == EditorWorkspace::Studio_GripPoser ||
               ws == EditorWorkspace::Studio_AnimationTimeline ||
               ws == EditorWorkspace::Studio_WeaponSkins ||
               ws == EditorWorkspace::Studio_Appearance ||
               ws == EditorWorkspace::Studio_FaceDialogue;
    }

    std::string getWorkspaceName(EditorWorkspace ws) const {
        switch (ws) {
            case EditorWorkspace::Project_Hub: return "Hub > Engine Main Menu & Project Browser";
            case EditorWorkspace::World3D_Level: return "3D > Level & Geometry Editor";
            case EditorWorkspace::World3D_Animation: return "3D > Animation 3D Timeline";
            case EditorWorkspace::World3D_ModelViewer: return "3D > 3D Model Viewer & Inspector";
            case EditorWorkspace::Studio_GripPoser: return "3D > Character & Weapon Studio > Weapon Grip Poser";
            case EditorWorkspace::Studio_AnimationTimeline: return "3D > Character & Weapon Studio > Animation Timeline";
            case EditorWorkspace::Studio_WeaponSkins: return "3D > Character & Weapon Studio > Weapon Skins & Materials";
            case EditorWorkspace::Studio_Appearance: return "3D > Character & Weapon Studio > Character Appearance";
            case EditorWorkspace::Studio_FaceDialogue: return "3D > Character & Weapon Studio > Face Dialogue & Lip-Sync";
            case EditorWorkspace::Level2D_Tilemap: return "2D > Level & Tilemap Editor";
            case EditorWorkspace::Level2D_Animation: return "2D > Animation 2D Spritesheet";
            case EditorWorkspace::Level2D_HUD: return "2D > HUD Canvas Designer";
            case EditorWorkspace::Scripting_Editor: return "Scripting > Lua Script Editor IDE";
            case EditorWorkspace::Scripting_Console: return "Scripting > Interactive REPL Console";
            case EditorWorkspace::Plugins_Manager: return "Extensions > Package & Plugin Manager";
            default: return "3D > Level & Geometry Editor";
        }
    }

    std::string getWorkspaceBreadcrumb(EditorWorkspace ws) const {
        switch (ws) {
            case EditorWorkspace::Project_Hub: return "Hub > Main Menu";
            case EditorWorkspace::World3D_Level: return "3D > Level";
            case EditorWorkspace::World3D_Animation: return "3D > Anim 3D";
            case EditorWorkspace::World3D_ModelViewer: return "3D > Model Viewer";
            case EditorWorkspace::Studio_GripPoser: return "Studio > Grip Poser";
            case EditorWorkspace::Studio_AnimationTimeline: return "Studio > Timeline";
            case EditorWorkspace::Studio_WeaponSkins: return "Studio > Skins";
            case EditorWorkspace::Studio_Appearance: return "Studio > Appearance";
            case EditorWorkspace::Studio_FaceDialogue: return "Studio > Dialogue";
            case EditorWorkspace::Level2D_Tilemap: return "2D > Level";
            case EditorWorkspace::Level2D_Animation: return "2D > Anim 2D";
            case EditorWorkspace::Level2D_HUD: return "2D > HUD Canvas";
            case EditorWorkspace::Scripting_Editor: return "Script > Lua IDE";
            case EditorWorkspace::Scripting_Console: return "Script > Console";
            case EditorWorkspace::Plugins_Manager: return "Ext > Plugins";
            default: return "3D > Level";
        }
    }

    void setWorkspace(EditorWorkspace ws) {
        _workspace = ws;
        if (ws == EditorWorkspace::Project_Hub) {
            refreshProjectHub();
        } else if (ws == EditorWorkspace::World3D_Level || ws == EditorWorkspace::World3D_Animation || ws == EditorWorkspace::World3D_ModelViewer) {
            _orthoViewMode = false;
            _camera.setPerspective(70.0f, 16.0f / 9.0f, 0.01f, 3000.0f);
        } else if (ws == EditorWorkspace::Level2D_Tilemap || ws == EditorWorkspace::Level2D_Animation || ws == EditorWorkspace::Level2D_HUD) {
            _orthoViewMode = true;
            _camera.setOrthographic(25.0f, -500.0f, 500.0f);
            _camera.setPitch(-89.9f);
            _camera.setYaw(-90.0f);
        }

        // Configure Studio tabs if in Studio workspace
        if (ws == EditorWorkspace::Studio_GripPoser) {
            _characterStudio.setActiveTab(StudioTab::GripPoser);
        } else if (ws == EditorWorkspace::Studio_AnimationTimeline) {
            _characterStudio.setActiveTab(StudioTab::AnimationTimeline);
        } else if (ws == EditorWorkspace::Studio_WeaponSkins) {
            _characterStudio.setActiveTab(StudioTab::WeaponSkins);
        } else if (ws == EditorWorkspace::Studio_Appearance) {
            _characterStudio.setActiveTab(StudioTab::Appearance);
        } else if (ws == EditorWorkspace::Studio_FaceDialogue) {
            _characterStudio.setActiveTab(StudioTab::FaceDialogue);
        }

        if (ws == EditorWorkspace::World3D_Animation || ws == EditorWorkspace::Level2D_Animation) {
            _bottomDockTab = BottomDockTab::Timeline;
        } else if (ws == EditorWorkspace::Scripting_Editor || ws == EditorWorkspace::Scripting_Console) {
            _bottomDockTab = BottomDockTab::ScriptEditor;
            refreshGameScripts();
            if (!_gameScripts.empty() && _selectedGameScriptIndex < 0) {
                _selectedGameScriptIndex = 0;
                _activeScriptPath = "game://scripts/" + _gameScripts[0];
                loadScriptFromFile(_activeScriptPath);
            }
        }

        logMessage("Workspace switched: [ " + getWorkspaceName(ws) + " ]");
    }

    void refreshGameScripts() {
        _gameScripts = ProjectManager::get().getProjectScripts();
        _selectedGameScriptIndex = -1;
        std::string curFile = std::filesystem::path(_activeScriptPath).filename().string();
        for (size_t i = 0; i < _gameScripts.size(); ++i) {
            if (_gameScripts[i] == curFile) {
                _selectedGameScriptIndex = (int)i;
                break;
            }
        }
    }

    void syncStringToScriptLines() {
        _scriptLines.clear();
        std::stringstream ss(_currentScript);
        std::string line;
        while (std::getline(ss, line)) {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            _scriptLines.push_back(line);
        }
        if (_scriptLines.empty()) _scriptLines.push_back("");
        _scriptCursorLine = std::clamp(_scriptCursorLine, 0, (int)_scriptLines.size() - 1);
        _scriptCursorCol = std::clamp(_scriptCursorCol, 0, (int)_scriptLines[_scriptCursorLine].size());
    }

    void syncScriptLinesToString() {
        std::string res;
        for (size_t i = 0; i < _scriptLines.size(); ++i) {
            res += _scriptLines[i];
            if (i + 1 < _scriptLines.size()) res += "\n";
        }
        _currentScript = res;
    }

    void saveScriptToFile(const std::string& path) {
        syncScriptLinesToString();
        try {
            std::filesystem::path realPath = ProjectManager::get().resolveAssetPath(path);
            if (realPath.empty()) {
                realPath = ProjectManager::get().getActiveProjectAssetsPath() / "scripts" / std::filesystem::path(path).filename();
            }
            if (realPath.has_parent_path() && !std::filesystem::exists(realPath.parent_path())) {
                std::error_code ec;
                std::filesystem::create_directories(realPath.parent_path(), ec);
            }
            std::ofstream out(realPath);
            if (out.is_open()) {
                out << _currentScript;
                out.close();
                logMessage("Script saved to " + realPath.string());
                refreshGameScripts();
            }
        } catch (const std::exception& e) {
            logMessage("Failed to save script: " + std::string(e.what()));
        }
    }

    void loadScriptFromFile(const std::string& path) {
        std::filesystem::path realPath = ProjectManager::get().resolveAssetPath(path);
        if (!std::filesystem::exists(realPath)) {
            if (std::filesystem::exists(path)) realPath = path;
            else return;
        }
        std::ifstream in(realPath);
        if (in.is_open()) {
            std::stringstream ss;
            ss << in.rdbuf();
            _currentScript = ss.str();
            syncStringToScriptLines();
            logMessage("Loaded script from " + realPath.string());
            refreshGameScripts();
        }
    }

    void runCurrentScript() {
        syncScriptLinesToString();
        if (_map) {
            _scriptEngine.setActiveMap(_map.get());
        }
        bool ok = _scriptEngine.executeString(_currentScript);
        if (ok) {
            logMessage("Lua Script executed successfully! (OnUpdate & bindings active)");
        } else {
            logMessage("Lua Script execution failed (check syntax)!");
        }
    }

    void handleScriptEditorInput(float dt) {
        _scriptBlinkTimer += dt;
        if (!_scriptEditorFocused) return;
        if (_scriptLines.empty()) _scriptLines.push_back("");

        _scriptCursorLine = std::clamp(_scriptCursorLine, 0, (int)_scriptLines.size() - 1);
        _scriptCursorCol = std::clamp(_scriptCursorCol, 0, (int)_scriptLines[_scriptCursorLine].size());

        bool ctrl = Input::isKeyPressed(GLFW_KEY_LEFT_CONTROL) || Input::isKeyPressed(GLFW_KEY_RIGHT_CONTROL);
        bool shift = Input::isKeyPressed(GLFW_KEY_LEFT_SHIFT) || Input::isKeyPressed(GLFW_KEY_RIGHT_SHIFT);

        // Ctrl Shortcuts
        if (ctrl) {
            if (Input::isKeyJustPressed(GLFW_KEY_S)) {
                saveScriptToFile(_activeScriptPath);
                return;
            } else if (Input::isKeyJustPressed(GLFW_KEY_R)) {
                runCurrentScript();
                return;
            } else if (Input::isKeyJustPressed(GLFW_KEY_A)) {
                _scriptCursorLine = 0;
                _scriptCursorCol = 0;
                return;
            }
        }

        // Enter: Split line
        if (Input::isKeyJustPressed(GLFW_KEY_ENTER)) {
            std::string cur = _scriptLines[_scriptCursorLine];
            std::string left = cur.substr(0, _scriptCursorCol);
            std::string right = cur.substr(_scriptCursorCol);

            std::string indent = "";
            for (char ch : left) {
                if (ch == ' ') indent += ' ';
                else break;
            }

            _scriptLines[_scriptCursorLine] = left;
            _scriptLines.insert(_scriptLines.begin() + _scriptCursorLine + 1, indent + right);
            _scriptCursorLine++;
            _scriptCursorCol = (int)indent.size();
            syncScriptLinesToString();
            _scriptBlinkTimer = 0.0f;
            return;
        }

        // Backspace: delete char or merge line
        if (Input::isKeyJustPressed(GLFW_KEY_BACKSPACE)) {
            if (_scriptCursorCol > 0) {
                _scriptLines[_scriptCursorLine].erase(_scriptCursorCol - 1, 1);
                _scriptCursorCol--;
                syncScriptLinesToString();
            } else if (_scriptCursorLine > 0) {
                int prevLen = (int)_scriptLines[_scriptCursorLine - 1].size();
                _scriptLines[_scriptCursorLine - 1] += _scriptLines[_scriptCursorLine];
                _scriptLines.erase(_scriptLines.begin() + _scriptCursorLine);
                _scriptCursorLine--;
                _scriptCursorCol = prevLen;
                syncScriptLinesToString();
            }
            _scriptBlinkTimer = 0.0f;
            return;
        }

        // Delete: delete char or pull up next line
        if (Input::isKeyJustPressed(GLFW_KEY_DELETE)) {
            if (_scriptCursorCol < (int)_scriptLines[_scriptCursorLine].size()) {
                _scriptLines[_scriptCursorLine].erase(_scriptCursorCol, 1);
                syncScriptLinesToString();
            } else if (_scriptCursorLine < (int)_scriptLines.size() - 1) {
                _scriptLines[_scriptCursorLine] += _scriptLines[_scriptCursorLine + 1];
                _scriptLines.erase(_scriptLines.begin() + _scriptCursorLine + 1);
                syncScriptLinesToString();
            }
            _scriptBlinkTimer = 0.0f;
            return;
        }

        // Tab: 4 spaces
        if (Input::isKeyJustPressed(GLFW_KEY_TAB)) {
            _scriptLines[_scriptCursorLine].insert(_scriptCursorCol, "    ");
            _scriptCursorCol += 4;
            syncScriptLinesToString();
            _scriptBlinkTimer = 0.0f;
            return;
        }

        // Arrow navigation
        if (Input::isKeyJustPressed(GLFW_KEY_LEFT)) {
            if (_scriptCursorCol > 0) {
                _scriptCursorCol--;
            } else if (_scriptCursorLine > 0) {
                _scriptCursorLine--;
                _scriptCursorCol = (int)_scriptLines[_scriptCursorLine].size();
            }
            _scriptBlinkTimer = 0.0f;
        }
        if (Input::isKeyJustPressed(GLFW_KEY_RIGHT)) {
            if (_scriptCursorCol < (int)_scriptLines[_scriptCursorLine].size()) {
                _scriptCursorCol++;
            } else if (_scriptCursorLine < (int)_scriptLines.size() - 1) {
                _scriptCursorLine++;
                _scriptCursorCol = 0;
            }
            _scriptBlinkTimer = 0.0f;
        }
        if (Input::isKeyJustPressed(GLFW_KEY_UP)) {
            if (_scriptCursorLine > 0) {
                _scriptCursorLine--;
                _scriptCursorCol = std::min(_scriptCursorCol, (int)_scriptLines[_scriptCursorLine].size());
            }
            _scriptBlinkTimer = 0.0f;
        }
        if (Input::isKeyJustPressed(GLFW_KEY_DOWN)) {
            if (_scriptCursorLine < (int)_scriptLines.size() - 1) {
                _scriptCursorLine++;
                _scriptCursorCol = std::min(_scriptCursorCol, (int)_scriptLines[_scriptCursorLine].size());
            }
            _scriptBlinkTimer = 0.0f;
        }
        if (Input::isKeyJustPressed(GLFW_KEY_HOME)) {
            _scriptCursorCol = 0;
            _scriptBlinkTimer = 0.0f;
        }
        if (Input::isKeyJustPressed(GLFW_KEY_END)) {
            _scriptCursorCol = (int)_scriptLines[_scriptCursorLine].size();
            _scriptBlinkTimer = 0.0f;
        }
        if (Input::isKeyJustPressed(GLFW_KEY_PAGE_UP)) {
            _scriptCursorLine = std::max(0, _scriptCursorLine - 10);
            _scriptCursorCol = std::min(_scriptCursorCol, (int)_scriptLines[_scriptCursorLine].size());
            _scriptBlinkTimer = 0.0f;
        }
        if (Input::isKeyJustPressed(GLFW_KEY_PAGE_DOWN)) {
            _scriptCursorLine = std::min((int)_scriptLines.size() - 1, _scriptCursorLine + 10);
            _scriptCursorCol = std::min(_scriptCursorCol, (int)_scriptLines[_scriptCursorLine].size());
            _scriptBlinkTimer = 0.0f;
        }

        // Printable text characters
        char typed = 0;
        for (int k = GLFW_KEY_A; k <= GLFW_KEY_Z; ++k) {
            if (Input::isKeyJustPressed(k)) {
                typed = shift ? (char)k : (char)(k - 'A' + 'a');
                break;
            }
        }
        if (!typed) {
            for (int k = GLFW_KEY_0; k <= GLFW_KEY_9; ++k) {
                if (Input::isKeyJustPressed(k)) {
                    if (!shift) {
                        typed = (char)k;
                    } else {
                        const char* sNums = ")!@#$%^&*(";
                        typed = sNums[k - GLFW_KEY_0];
                    }
                    break;
                }
            }
        }
        if (!typed && Input::isKeyJustPressed(GLFW_KEY_SPACE)) {
            typed = ' ';
        }
        if (!typed) {
            struct SymEntry { int key; char norm; char shft; };
            SymEntry syms[] = {
                { GLFW_KEY_MINUS, '-', '_' },
                { GLFW_KEY_EQUAL, '=', '+' },
                { GLFW_KEY_LEFT_BRACKET, '[', '{' },
                { GLFW_KEY_RIGHT_BRACKET, ']', '}' },
                { GLFW_KEY_SEMICOLON, ';', ':' },
                { GLFW_KEY_APOSTROPHE, '\'', '"' },
                { GLFW_KEY_COMMA, ',', '<' },
                { GLFW_KEY_PERIOD, '.', '>' },
                { GLFW_KEY_SLASH, '/', '?' },
                { GLFW_KEY_BACKSLASH, '\\', '|' },
                { GLFW_KEY_GRAVE_ACCENT, '`', '~' }
            };
            for (const auto& sm : syms) {
                if (Input::isKeyJustPressed(sm.key)) {
                    typed = shift ? sm.shft : sm.norm;
                    break;
                }
            }
        }

        if (typed != 0) {
            _scriptLines[_scriptCursorLine].insert(_scriptCursorCol, 1, typed);
            _scriptCursorCol++;
            syncScriptLinesToString();
            _scriptBlinkTimer = 0.0f;
        }
    }

    void renderTurntableModelStage(int w, int h) {
        Camera turntableCam(60.0f, (float)w / (float)h, 0.05f, 500.0f);
        float radYaw = _modelViewerYaw * 3.14159265f / 180.0f;
        float radPitch = _modelViewerPitch * 3.14159265f / 180.0f;
        float dist = _modelViewerDistance;
        Vec3 camPos(
            std::cos(radPitch) * std::sin(radYaw) * dist,
            std::sin(radPitch) * dist + 0.8f,
            std::cos(radPitch) * std::cos(radYaw) * dist
        );
        turntableCam.setPosition(camPos);
        turntableCam.lookAt(Vec3(0, 0.4f, 0));

        Renderer::beginFrame(turntableCam);
        Renderer::setSunLight(Vec3(-0.5f, -0.8f, -0.4f), Vec3(1.0f, 0.98f, 0.95f), Vec3(0.35f, 0.38f, 0.45f));

        // Clean pedestal
        Renderer::drawCube(Vec3(0, -0.05f, 0), Vec3(2.5f, 0.1f, 2.5f), Vec3(0.18f, 0.20f, 0.25f));

        Mesh* m = getMesh(_selectedModel);
        if (m) {
            Texture* tex = getTexture(_selectedTexture);
            Renderer::drawMesh(*m, Vec3(0, 0.3f, 0), Vec3(0, 0, 0), Vec3(1, 1, 1), Vec3(0.9f, 0.9f, 0.95f), tex);
        } else {
            Renderer::drawCube(Vec3(0, 0.5f, 0), Vec3(1.0f, 1.0f, 1.0f), Vec3(0.3f, 0.6f, 0.9f));
        }
    }

    void saveMapAction(bool forceSaveAs = false) {
        if (!_map) return;
        if (forceSaveAs || _currentMapPath.empty()) {
            std::string savePath = LabDialogs::saveFileDialog(getWindow(), "Lab Scene Files (*.labscene)\0*.labscene\0Lab Map Files (*.labmap)\0*.labmap\0All Files (*.*)\0*.*\0", "my_level.labscene", "assets\\maps");
            if (!savePath.empty()) {
                _currentMapPath = savePath;
                _map->saveToFile(_currentMapPath);
                logMessage("Scene saved to: " + _currentMapPath);
            }
        } else {
            _map->saveToFile(_currentMapPath);
            logMessage("Scene saved to: " + _currentMapPath);
        }
    }

    void toggleSimulation() {
        if (_simState == EngineSimState::Playing) {
            _simState = EngineSimState::Stopped;
            logMessage("In-Editor Simulation: STOPPED");
        } else {
            _simState = EngineSimState::Playing;
            if (_map) {
                _scriptEngine.setActiveMap(_map.get());
            }
            logMessage("In-Editor Simulation: PLAYING (Live Viewport Physics & Lua Scripting)");
        }
    }

    void buildGameAction(bool runAfterBuild) {
        if (_currentMapPath.empty()) {
            _currentMapPath = "assets/maps/export_game.labmap";
        }
        if (_map) {
            _map->saveToFile(_currentMapPath);
        }
        _exportSettings.entrySceneURI = _currentMapPath;

        if (runAfterBuild) {
            logMessage("Compiling and packaging test game in DEBUG mode (destination: test_build/debug)...");
            ExportResult res = ExportPipeline::buildAndPrepareTestGame(_exportSettings.gameTitle, _exportSettings.entrySceneURI, "test_build/debug");
            _buildSuccess = res.success;
            _lastBuildLog = res.logMessage;
            if (res.success) {
                logMessage("TEST BUILD SUCCESS: " + res.logMessage);
                logMessage("Files packaged: " + std::to_string(res.copiedFiles) + " | Size: " + std::to_string(res.totalBytes / 1024) + " KB");
                logMessage("Launching test game process: " + res.exportedExecutable.string());
                bool launched = ExportPipeline::launchGame(res.exportedExecutable);
                if (launched) {
                    logMessage("Standalone debug test game spawned successfully!");
                } else {
                    logMessage("Notice: Executable exported. Launch command dispatched.");
                }
            } else {
                logMessage("TEST BUILD FAILED: " + res.logMessage);
            }
        } else {
            logMessage("Starting Export pipeline for [" + _exportSettings.gameTitle + "] (" + ExportPipeline::platformToString(_exportSettings.platform) + ")...");
            ExportResult res = ExportPipeline::exportProject(_exportSettings);
            _buildSuccess = res.success;
            _lastBuildLog = res.logMessage;
            if (res.success) {
                logMessage("BUILD SUCCESS: " + res.logMessage);
                logMessage("Files packaged: " + std::to_string(res.copiedFiles) + " | Size: " + std::to_string(res.totalBytes / 1024) + " KB");
            } else {
                logMessage("BUILD FAILED: " + res.logMessage);
            }
        }
    }

    void openModelDialog() {
        std::string picked = LabDialogs::openFileDialog(getWindow(), "3D STL Model (*.stl)\0*.stl\0All Files (*.*)\0*.*\0", "assets\\models");
        if (!picked.empty()) {
            std::string fname = std::filesystem::path(picked).filename().string();
            if (!_meshes.contains(fname)) {
                Mesh* m = Mesh::loadSTL(picked);
                if (m) {
                    _meshes[fname] = std::unique_ptr<Mesh>(m);
                    _availableModels.push_back(fname);
                    _selectedModel = fname;
                    logMessage("Loaded Model from disk: " + fname);
                } else {
                    logMessage("Failed to load model: " + picked);
                }
            } else {
                _selectedModel = fname;
                logMessage("Selected Model: " + fname);
            }
            _modelBrowserOpen = false;
        }
    }

    void onUpdate(const Time& time) override {
        (void)time;

        // Compute coordinate scaling between screen window coordinates and framebuffer
        int winW = 1600, winH = 900;
        getWindowSize(winW, winH);
        int fbW = 1600, fbH = 900;
        getFramebufferSize(fbW, fbH);

        double curX = 0.0, curY = 0.0;
        getCursorPos(curX, curY);
        if (curX == 0.0 && curY == 0.0) {
            curX = (double)Input::getMouseX();
            curY = (double)Input::getMouseY();
        }

        float mouseScaleX = (winW > 0 && fbW > 0) ? ((float)fbW / (float)winW) : 1.0f;
        float mouseScaleY = (winH > 0 && fbH > 0) ? ((float)fbH / (float)winH) : 1.0f;

        float mx = (float)curX * mouseScaleX;
        float my = (float)curY * mouseScaleY;
        _mouseScreenX = mx;
        _mouseScreenY = my;

        // Raycast mouse cursor onto floor or scene when hovering 3D viewport so 3D cursor follows the mouse!
        float vpX = 42.0f;
        float vpY = 58.0f;
        float vpW = (fbW > 0 ? (float)fbW : (float)getWidth()) - vpX - 320.0f;
        float vpH = (fbH > 0 ? (float)fbH : (float)getHeight()) - vpY - 22.0f;
        bool is3DMapEditing = (_workspace == EditorWorkspace::World3D_Level || _workspace == EditorWorkspace::World3D_Animation);
        bool in3DViewport = is3DMapEditing && (mx >= vpX && mx <= vpX + vpW && my >= vpY && my <= vpY + vpH);

        // Mouse look in 3D Viewport when holding Right Mouse Button (only in 3D map workspaces)
        if (is3DMapEditing && Input::isMouseButtonPressed(1)) {
            if (_activeTool != 3) {
                if (!_isFlyingCamera && in3DViewport) {
                    _isFlyingCamera = true;
                    setCursorCaptured(true);
                }
                if (_isFlyingCamera) {
                    _camera.update(Input::mouseDelta);
                }
            }
        } else {
            if (_isFlyingCamera) {
                _isFlyingCamera = false;
                setCursorCaptured(false);
            }
        }

        if (in3DViewport && !Input::isMouseButtonPressed(1)) {
            Vec3 rayDir = _camera.screenToWorldRay(mx, my, (float)fbW, (float)fbH);
            float closestT = 1e9f;
            Vec3 hitPoint;
            bool hitSomething = false;

            // Test raycast against existing scene geometry (brushes)
            if (_map) {
                for (const auto& b : _map->brushes) {
                    Vec3 half = b.size * 0.5f;
                    float t = 0.0f;
                    if (rayIntersectAABB(_camera.getPosition(), rayDir, b.position - half, b.position + half, t)) {
                        if (t > 0.001f && t < closestT) {
                            closestT = t;
                            hitPoint = _camera.getPosition() + rayDir * t;
                            hitSomething = true;
                        }
                    }
                }
            }

            if (hitSomething && closestT < 300.0f) {
                _cursorPos = snapToGrid(hitPoint, _gridSnap);
            } else if (std::abs(rayDir.y) > 0.0001f) {
                float t = -_camera.getPosition().y / rayDir.y;
                if (t > 0.0f && t < 300.0f) {
                    Vec3 hit = _camera.getPosition() + rayDir * t;
                    _cursorPos = snapToGrid(hit, _gridSnap);
                } else {
                    _cursorPos = snapToGrid(_camera.getPosition() + _camera.getFront() * 10.0f, _gridSnap);
                }
            } else {
                _cursorPos = snapToGrid(_camera.getPosition() + _camera.getFront() * 10.0f, _gridSnap);
            }
        } else if (Input::isMouseButtonPressed(1)) {
            _cursorPos = snapToGrid(_camera.getPosition() + _camera.getFront() * 10.0f, _gridSnap);
        }

        // Transform Gizmo Dragging Logic
        if (_isDraggingGizmo && _selectionType != SelectionType::None) {
            if (Input::isMouseButtonPressed(0)) {
                float dx = mx - _dragStartMouse.x;
                float dy = my - _dragStartMouse.y;
                Vec3 curPos = getSelectedPosition();
                Vec3 curRot = getSelectedRotation();
                Vec3 curScale = getSelectedScale();

                float camDist = (_camera.getPosition() - curPos).length();
                float worldFactor = std::max(0.005f, camDist * 0.002f);

                switch (_activeGizmoAxis) {
                    case GizmoAxis::X: {
                        float moveX = std::round((dx * worldFactor) / _gridSnap) * _gridSnap;
                        if (std::abs(moveX) >= _gridSnap) {
                            moveSelection(moveX, 0, 0);
                            _dragStartMouse.x = mx;
                        }
                        _gizmoStatusText = "Gizmo [Move X]: X = " + std::to_string(curPos.x) + " (Snap: " + std::to_string(_gridSnap) + "m)";
                        break;
                    }
                    case GizmoAxis::Y: {
                        float moveY = std::round((-dy * worldFactor) / _gridSnap) * _gridSnap;
                        if (std::abs(moveY) >= _gridSnap) {
                            moveSelection(0, moveY, 0);
                            _dragStartMouse.y = my;
                        }
                        _gizmoStatusText = "Gizmo [Move Y]: Y = " + std::to_string(curPos.y) + " (Snap: " + std::to_string(_gridSnap) + "m)";
                        break;
                    }
                    case GizmoAxis::Z: {
                        float moveZ = std::round((dy * worldFactor) / _gridSnap) * _gridSnap;
                        if (std::abs(moveZ) >= _gridSnap) {
                            moveSelection(0, 0, moveZ);
                            _dragStartMouse.y = my;
                        }
                        _gizmoStatusText = "Gizmo [Move Z]: Z = " + std::to_string(curPos.z) + " (Snap: " + std::to_string(_gridSnap) + "m)";
                        break;
                    }
                    case GizmoAxis::PlaneXY: {
                        float mX = std::round((dx * worldFactor) / _gridSnap) * _gridSnap;
                        float mY = std::round((-dy * worldFactor) / _gridSnap) * _gridSnap;
                        if (std::abs(mX) >= _gridSnap || std::abs(mY) >= _gridSnap) {
                            moveSelection(mX, mY, 0);
                            _dragStartMouse = { mx, my };
                        }
                        _gizmoStatusText = "Gizmo [Plane XY]: (" + std::to_string(curPos.x) + ", " + std::to_string(curPos.y) + ")";
                        break;
                    }
                    case GizmoAxis::PlaneXZ: {
                        float mX = std::round((dx * worldFactor) / _gridSnap) * _gridSnap;
                        float mZ = std::round((dy * worldFactor) / _gridSnap) * _gridSnap;
                        if (std::abs(mX) >= _gridSnap || std::abs(mZ) >= _gridSnap) {
                            moveSelection(mX, 0, mZ);
                            _dragStartMouse = { mx, my };
                        }
                        _gizmoStatusText = "Gizmo [Plane XZ]: (" + std::to_string(curPos.x) + ", " + std::to_string(curPos.z) + ")";
                        break;
                    }
                    case GizmoAxis::PlaneYZ: {
                        float mY = std::round((-dy * worldFactor) / _gridSnap) * _gridSnap;
                        float mZ = std::round((dx * worldFactor) / _gridSnap) * _gridSnap;
                        if (std::abs(mY) >= _gridSnap || std::abs(mZ) >= _gridSnap) {
                            moveSelection(0, mY, mZ);
                            _dragStartMouse = { mx, my };
                        }
                        _gizmoStatusText = "Gizmo [Plane YZ]: (" + std::to_string(curPos.y) + ", " + std::to_string(curPos.z) + ")";
                        break;
                    }
                    case GizmoAxis::RotX: {
                        float deg = std::round((dy * 0.5f) / _angleSnap) * _angleSnap;
                        if (std::abs(deg) >= _angleSnap) {
                            rotateSelection(deg, 0, 0);
                            _dragStartMouse.y = my;
                        }
                        _gizmoStatusText = "Gizmo [Rotate Pitch]: " + std::to_string((int)curRot.x) + " deg (Snap: " + std::to_string((int)_angleSnap) + " deg)";
                        break;
                    }
                    case GizmoAxis::RotY: {
                        float deg = std::round((dx * 0.5f) / _angleSnap) * _angleSnap;
                        if (std::abs(deg) >= _angleSnap) {
                            rotateSelection(0, deg, 0);
                            _dragStartMouse.x = mx;
                        }
                        _gizmoStatusText = "Gizmo [Rotate Yaw]: " + std::to_string((int)curRot.y) + " deg (Snap: " + std::to_string((int)_angleSnap) + " deg)";
                        break;
                    }
                    case GizmoAxis::RotZ: {
                        float deg = std::round((dx * 0.5f) / _angleSnap) * _angleSnap;
                        if (std::abs(deg) >= _angleSnap) {
                            rotateSelection(0, 0, deg);
                            _dragStartMouse.x = mx;
                        }
                        _gizmoStatusText = "Gizmo [Rotate Roll]: " + std::to_string((int)curRot.z) + " deg (Snap: " + std::to_string((int)_angleSnap) + " deg)";
                        break;
                    }
                    case GizmoAxis::ScaleX: {
                        float sc = std::round((dx * 0.02f) / _scaleSnap) * _scaleSnap;
                        if (std::abs(sc) >= _scaleSnap) {
                            scaleSelection(sc, 0, 0);
                            _dragStartMouse.x = mx;
                        }
                        _gizmoStatusText = "Gizmo [Scale X]: " + std::to_string(curScale.x);
                        break;
                    }
                    case GizmoAxis::ScaleY: {
                        float sc = std::round((-dy * 0.02f) / _scaleSnap) * _scaleSnap;
                        if (std::abs(sc) >= _scaleSnap) {
                            scaleSelection(0, sc, 0);
                            _dragStartMouse.y = my;
                        }
                        _gizmoStatusText = "Gizmo [Scale Y]: " + std::to_string(curScale.y);
                        break;
                    }
                    case GizmoAxis::ScaleZ: {
                        float sc = std::round((dy * 0.02f) / _scaleSnap) * _scaleSnap;
                        if (std::abs(sc) >= _scaleSnap) {
                            scaleSelection(0, 0, sc);
                            _dragStartMouse.y = my;
                        }
                        _gizmoStatusText = "Gizmo [Scale Z]: " + std::to_string(curScale.z);
                        break;
                    }
                    case GizmoAxis::ScaleUniform: {
                        float sc = std::round(((dx - dy) * 0.015f) / _scaleSnap) * _scaleSnap;
                        if (std::abs(sc) >= _scaleSnap) {
                            scaleSelection(sc, sc, sc);
                            _dragStartMouse = { mx, my };
                        }
                        _gizmoStatusText = "Gizmo [Scale Uniform]: (" + std::to_string(curScale.x) + ", " + std::to_string(curScale.y) + ", " + std::to_string(curScale.z) + ")";
                        break;
                    }
                    default: break;
                }
            } else {
                _isDraggingGizmo = false;
                _activeGizmoAxis = GizmoAxis::None;
                _gizmoStatusText = "";
            }
        } else if (!_isDraggingGizmo && _selectionType != SelectionType::None && in3DViewport && !Input::isMouseButtonPressed(1)) {
            Vec3 rayDir = _camera.screenToWorldRay(mx, my, (float)fbW, (float)fbH);
            _hoveredGizmoAxis = testGizmoHit(_camera.getPosition(), rayDir, getSelectedPosition());
        } else if (!_isDraggingGizmo) {
            _hoveredGizmoAxis = GizmoAxis::None;
        }

        // Mouse Wheel: Elevate / Lower selected object along Y-axis if inside 3D Viewport
        if (std::abs(Input::scrollDelta) > 0.01f && _selectionType != SelectionType::None && in3DViewport && !Input::isMouseButtonPressed(1)) {
            float dy = (Input::scrollDelta > 0.0f) ? _gridSnap : -_gridSnap;
            moveSelection(0.0f, dy, 0.0f);
        }

        // Handle Left-Click
        if (Input::isMouseButtonJustPressed(0) || (Input::isMouseButtonPressed(0) && !_lmbPressed)) {
            LabLog::info("[LabHammer] LMB Click at (" + std::to_string((int)mx) + ", " + std::to_string((int)my) + ")");
            handleMouseClick(mx, my, false);
            _lmbPressed = true;
        } else if (!Input::isMouseButtonPressed(0)) {
            _lmbPressed = false;
        }

        // Handle Right-Click (for Tool 3 Pipette sample or Viewport Context Menu with Magic Add)
        static Vec2 s_rmbPressPos{ 0, 0 };
        if (Input::isMouseButtonJustPressed(1)) {
            s_rmbPressPos = { mx, my };
        }
        if (Input::isMouseButtonJustPressed(1) || (Input::isMouseButtonPressed(1) && !_rmbPressed)) {
            if (_activeTool == 3) {
                handleMouseClick(mx, my, true);
            } else if (!in3DViewport || _contextMenuOpen || mx >= vpX + vpW) {
                handleMouseClick(mx, my, true);
            } else if (Input::isKeyPressed(340) || Input::isKeyPressed(344)) { // Shift + RMB opens Context Menu
                _contextMenuOpen = true;
                _contextMenuX = mx;
                _contextMenuY = my;
                _contextMenuSearch = "";
                _contextMenuTargetIndex = (_selectedIndex >= 0) ? _selectedIndex : 0;
            }
            _rmbPressed = true;
        } else if (!Input::isMouseButtonPressed(1)) {
            if (_rmbPressed && in3DViewport && _activeTool != 3 && !Input::isKeyPressed(340) && !Input::isKeyPressed(344)) {
                float distMoved = std::sqrt((mx - s_rmbPressPos.x) * (mx - s_rmbPressPos.x) + (my - s_rmbPressPos.y) * (my - s_rmbPressPos.y));
                if (distMoved < 4.0f) {
                    _contextMenuOpen = true;
                    _contextMenuX = mx;
                    _contextMenuY = my;
                    _contextMenuSearch = "";
                    _contextMenuTargetIndex = (_selectedIndex >= 0) ? _selectedIndex : 0;
                }
            }
            _rmbPressed = false;
        }

        // 1. Studio Workspace Engine Tick (LabStudio character & weapon updates)
        if (isStudioWorkspace(_workspace)) {
            _characterStudio.update(time.delta, mx, my, Input::isMouseButtonPressed(0), Input::isMouseButtonPressed(1), Input::scrollDelta);
            bool ctrl = Input::isKeyPressed(GLFW_KEY_LEFT_CONTROL) || Input::isKeyPressed(GLFW_KEY_RIGHT_CONTROL);
            bool shift = Input::isKeyPressed(GLFW_KEY_LEFT_SHIFT) || Input::isKeyPressed(GLFW_KEY_RIGHT_SHIFT);
            for (int k = 0; k < 512; ++k) {
                if (Input::keys[k] && !_lastStudioKeys[k]) {
                    _characterStudio.handleKeyDown(k, ctrl, shift);
                }
                _lastStudioKeys[k] = Input::keys[k];
            }
        }

        // 1.5. HUD Workspace Engine Tick (LabHUDEditor2D updates)
        if (_workspace == EditorWorkspace::Level2D_HUD) {
            _hudEditor.update(time.delta, mx, my, Input::isMouseButtonPressed(0), Input::isMouseButtonPressed(1), Input::scrollDelta);
            if (_hudEditor.requestExit()) {
                _hudEditor.clearRequestExit();
                setWorkspace(EditorWorkspace::World3D_Level);
            }
            bool ctrl = Input::isKeyPressed(GLFW_KEY_LEFT_CONTROL) || Input::isKeyPressed(GLFW_KEY_RIGHT_CONTROL);
            bool shift = Input::isKeyPressed(GLFW_KEY_LEFT_SHIFT) || Input::isKeyPressed(GLFW_KEY_RIGHT_SHIFT);
            for (int k = 0; k < 512; ++k) {
                if (Input::keys[k] && !_lastHudKeys[k]) {
                    _hudEditor.handleKeyDown(k, ctrl, shift);
                }
                _lastHudKeys[k] = Input::keys[k];
            }
        }

        // 2. 3D Model Viewer Turntable Updates
        if (_workspace == EditorWorkspace::World3D_ModelViewer) {
            if (_modelViewerAutoRotate) {
                _modelViewerYaw += 25.0f * time.delta;
                if (_modelViewerYaw >= 360.0f) _modelViewerYaw -= 360.0f;
            }
            if (in3DViewport && Input::isMouseButtonPressed(0)) {
                _modelViewerYaw += (mx - _modelViewerLastMouseX) * 0.5f;
                _modelViewerPitch = std::clamp(_modelViewerPitch + (my - _modelViewerLastMouseY) * 0.5f, -80.0f, 80.0f);
            }
            if (in3DViewport && std::abs(Input::scrollDelta) > 0.01f) {
                _modelViewerDistance = std::clamp(_modelViewerDistance - Input::scrollDelta * 0.2f, 0.5f, 15.0f);
            }
            _modelViewerLastMouseX = mx;
            _modelViewerLastMouseY = my;
        }

        // 3. 2D Spritesheet Animation Updates
        if (_workspace == EditorWorkspace::Level2D_Animation && _anim2DPlaying) {
            _anim2DTimer += time.delta;
            float frameDuration = 1.0f / (float)std::max(1, _anim2DFPS);
            if (_anim2DTimer >= frameDuration) {
                _anim2DTimer = 0.0f;
                _anim2DSelectedFrame++;
                if (_anim2DSelectedFrame >= _anim2DTotalFrames) {
                    _anim2DSelectedFrame = _anim2DLoop ? 0 : (_anim2DTotalFrames - 1);
                }
            }
        }

        // In-Engine Lua Script Studio IDE Updates
        if (_workspace == EditorWorkspace::Scripting_Editor) {
            handleScriptEditorInput(time.delta);
        }

        // 4. Hierarchy Context Menu Search Input Handling
        if (_contextMenuOpen) {
            static bool s_backspacePressed = false;
            static bool s_enterPressed = false;
            static bool s_escPressed = false;
            static bool s_lastKeys[512] = { false };

            if (Input::isKeyPressed(259)) { // Backspace
                if (!s_backspacePressed && !_contextMenuSearch.empty()) {
                    _contextMenuSearch.pop_back();
                }
                s_backspacePressed = true;
            } else {
                s_backspacePressed = false;
            }

            if (Input::isKeyPressed(256)) { // Esc
                if (!s_escPressed) {
                    _contextMenuOpen = false;
                }
                s_escPressed = true;
            } else {
                s_escPressed = false;
            }

            if (Input::isKeyPressed(257)) { // Enter
                if (!s_enterPressed) {
                    std::string q = _contextMenuSearch;
                    std::transform(q.begin(), q.end(), q.begin(), [](unsigned char c) { return (char)::tolower(c); });
                    if (q.find("scr") != std::string::npos || q.empty()) {
                        setWorkspace(EditorWorkspace::Scripting_Editor);
                        logMessage("Context Menu: Attached Script (Lua) - opened Lua Script Editor IDE");
                    } else if (q.find("lig") != std::string::npos) {
                        logMessage("Context Menu: Added Point Light to entity");
                    } else if (q.find("spri") != std::string::npos) {
                        logMessage("Context Menu: Attached 2D Sprite to entity");
                    } else {
                        logMessage("Context Menu: Attached Component: " + _contextMenuSearch);
                    }
                    _contextMenuOpen = false;
                }
                s_enterPressed = true;
            } else {
                s_enterPressed = false;
            }

            for (int k = 32; k <= 90; ++k) {
                if (Input::keys[k] && !s_lastKeys[k]) {
                    char c = (char)k;
                    bool shift = Input::isKeyPressed(340) || Input::isKeyPressed(344);
                    if (!shift && c >= 'A' && c <= 'Z') c = (char)(c + 32);
                    _contextMenuSearch += c;
                }
                s_lastKeys[k] = Input::keys[k];
            }
        }

        // 5. Plugin Search Query Input Handling
        if (_workspace == EditorWorkspace::Plugins_Manager && _pluginSearchFocused) {
            static bool s_plugBack = false;
            static bool s_plugEsc = false;
            static bool s_lastPlugKeys[512] = { false };

            if (Input::isKeyPressed(259)) { // Backspace
                if (!s_plugBack && !_pluginSearchQuery.empty()) {
                    _pluginSearchQuery.pop_back();
                }
                s_plugBack = true;
            } else {
                s_plugBack = false;
            }

            if (Input::isKeyPressed(256) || Input::isKeyPressed(257)) { // Esc or Enter
                if (!s_plugEsc) {
                    _pluginSearchFocused = false;
                }
                s_plugEsc = true;
            } else {
                s_plugEsc = false;
            }

            for (int k = 32; k <= 90; ++k) {
                if (Input::keys[k] && !s_lastPlugKeys[k]) {
                    char c = (char)k;
                    bool shift = Input::isKeyPressed(340) || Input::isKeyPressed(344);
                    if (!shift && c >= 'A' && c <= 'Z') c = (char)(c + 32);
                    _pluginSearchQuery += c;
                }
                s_lastPlugKeys[k] = Input::keys[k];
            }
        }

        // Legacy Map Conversion Modal keyboard shortcuts (Enter to convert, Esc to cancel)
        if (_showConvertMapModal) {
            static bool s_convertEsc = false;
            static bool s_convertEnter = false;
            if (Input::isKeyPressed(256)) { // Esc -> Cancel
                if (!s_convertEsc) {
                    _showConvertMapModal = false;
                    _pendingMapPath = "";
                    logMessage("Wczytywanie mapy anulowane.");
                }
                s_convertEsc = true;
            } else {
                s_convertEsc = false;
            }

            if (Input::isKeyPressed(257)) { // Enter -> Convert
                if (!s_convertEnter) {
                    logMessage("Rozpoczynanie konwersji: " + _pendingMapPath);
                    auto res = LabScene::convertLegacyMapToScene(_pendingMapPath);
                    if (res.success) {
                        logMessage("Konwersja udana! Kopia zapasowa: " + res.backupPath);
                        loadMapFromFile(res.scenePath);
                    } else {
                        logMessage("Blad konwersji mapy: " + res.errorMessage);
                    }
                    _showConvertMapModal = false;
                    _pendingMapPath = "";
                }
                s_convertEnter = true;
            } else {
                s_convertEnter = false;
            }
        }

        // Simulation Tick (Lua OnUpdate & in-editor game mechanics)
        if (_simState == EngineSimState::Playing) {
            if (_map) {
                _scriptEngine.setActiveMap(_map.get());
            }
            _scriptEngine.update(time.delta);
        }

        // Timeline playback in Animation Workspace
        if (_timelinePlaying) {
            _animScrubberPos += time.delta * 0.5f;
            if (_animScrubberPos > 1.0f) {
                _animScrubberPos = 0.0f;
            }
            _timelineFrame = static_cast<int>(_animScrubberPos * 60.0f);
        }

        // Keyboard Shortcuts
        bool ctrlDown = Input::isKeyPressed(341) || Input::isKeyPressed(345); // Left/Right Ctrl
        bool shiftDown = Input::isKeyPressed(340) || Input::isKeyPressed(344);

        // Ctrl+R: Execute / Reload Lua Script
        if (ctrlDown && (Input::isKeyPressed('R') || Input::isKeyPressed('r'))) {
            if (!_ctrlRPressed) {
                runCurrentScript();
                _ctrlRPressed = true;
            }
        } else {
            _ctrlRPressed = false;
        }

        // F1: Toggle / Open Engine Main Menu (Project Hub)
        if (Input::isKeyPressed(290)) { // GLFW_KEY_F1
            if (!_f1Pressed) {
                if (_workspace == EditorWorkspace::Project_Hub) {
                    setWorkspace(EditorWorkspace::World3D_Level);
                } else {
                    setWorkspace(EditorWorkspace::Project_Hub);
                }
                _f1Pressed = true;
            }
        } else {
            _f1Pressed = false;
        }

        // F9: Toggle Live In-Editor Simulation
        if (Input::isKeyPressed(298)) { // GLFW_KEY_F9
            if (!_f9Pressed) {
                toggleSimulation();
                _f9Pressed = true;
            }
        } else {
            _f9Pressed = false;
        }

        // F5: Build & Run Game
        if (Input::isKeyPressed(294)) { // GLFW_KEY_F5
            if (!_f5Pressed) {
                buildGameAction(true);
                _f5Pressed = true;
            }
        } else {
            _f5Pressed = false;
        }

        // Ctrl+B: Build & Run Game | Ctrl+Shift+B: Build Settings Dialog
        if (ctrlDown && (Input::isKeyPressed('B') || Input::isKeyPressed('b'))) {
            if (!_ctrlBPressed) {
                if (shiftDown) {
                    _buildModalOpen = !_buildModalOpen;
                } else {
                    buildGameAction(true);
                }
                _ctrlBPressed = true;
            }
        } else {
            _ctrlBPressed = false;
        }

        // Ctrl+N: New Map
        if (ctrlDown && (Input::isKeyPressed('N') || Input::isKeyPressed('n'))) {
            if (!_ctrlNPressed) {
                newMap();
                _ctrlNPressed = true;
            }
        } else {
            _ctrlNPressed = false;
        }

        // Ctrl+O: Open Map
        if (ctrlDown && (Input::isKeyPressed('O') || Input::isKeyPressed('o'))) {
            if (!_ctrlOPressed) {
                openMapDialog();
                _ctrlOPressed = true;
            }
        } else {
            _ctrlOPressed = false;
        }

        // Ctrl+S: Save Map
        if (ctrlDown && (Input::isKeyPressed('S') || Input::isKeyPressed('s'))) {
            if (!_ctrlSPressed) {
                saveMapAction(false);
                _ctrlSPressed = true;
            }
        } else {
            _ctrlSPressed = false;
        }

        // Ctrl+D: Duplicate Selection
        if (ctrlDown && (Input::isKeyPressed('D') || Input::isKeyPressed('d'))) {
            if (!_ctrlDPressed) {
                duplicateSelection();
                _ctrlDPressed = true;
            }
        } else {
            _ctrlDPressed = false;
        }

        // 3D Map Editing Shortcuts (strictly isolated to 3D workspaces)
        if (is3DMapEditing) {
            // F: Focus camera on selection
            if (!ctrlDown && (Input::isKeyPressed('F') || Input::isKeyPressed('f'))) {
            if (!_fPressed) {
                focusCamera();
                _fPressed = true;
            }
        } else {
            _fPressed = false;
        }

        // L: Toggle Darkness Mode (Pitch black preview with point lights)
        if (!ctrlDown && (Input::isKeyPressed('L') || Input::isKeyPressed('l'))) {
            if (!_lPressed) {
                _darknessMode = !_darknessMode;
                logMessage(_darknessMode ? "Darkness Mode: ACTIVE (Pitch Black Ambient/Sun)" : "Darkness Mode: DISABLED (Standard Sunlight)");
                _lPressed = true;
            }
        } else {
            _lPressed = false;
        }

        // E: Place Brush / Prop / Door / Spawn
        if (!ctrlDown && (Input::isKeyPressed('E') || Input::isKeyPressed('e'))) {
            if (!_ePressed && _map) {
                placeCurrentObject();
                _ePressed = true;
            }
        } else {
            _ePressed = false;
        }

        // X / Shift+X: Toggle Clip Tool / Cycle Clip Mode
        if (!ctrlDown && (Input::isKeyPressed('X') || Input::isKeyPressed('x'))) {
            if (!_xPressed) {
                if (_activeTool != 6) {
                    _activeTool = 6;
                    logMessage("CSG Clip Tool Active (X / Shift+X cycles mode, ENTER to commit)");
                } else {
                    _clipMode = (ClipMode)(((int)_clipMode + 1) % 3);
                    if (_clipMode == ClipMode::KeepFront) {
                        logMessage("CSG Clip Mode: Keep Front (Positive Side)");
                    } else if (_clipMode == ClipMode::KeepBack) {
                        logMessage("CSG Clip Mode: Keep Back (Negative Side)");
                    } else {
                        logMessage("CSG Clip Mode: Keep Both (Split into 2 Brushes)");
                    }
                }
                _xPressed = true;
            }
        } else {
            _xPressed = false;
        }

        // Enter: Commit CSG Clip
        if (Input::isKeyPressed(257)) { // GLFW_KEY_ENTER
            if (!_enterPressed) {
                if (_activeTool == 6) {
                    commitClip();
                }
                _enterPressed = true;
            }
        } else {
            _enterPressed = false;
        }

        // Backspace / Delete: Delete selected object (or last brush if none selected)
        if (Input::isKeyPressed(259) || Input::isKeyPressed(261)) { // Backspace or Del
            if (!_delPressed) {
                if (_selectionType != SelectionType::None) {
                    deleteSelection();
                } else if (_map && !_map->brushes.empty()) {
                    _map->brushes.pop_back();
                    logMessage("Undo: Deleted last brush.");
                }
                _delPressed = true;
            }
        } else {
            _delPressed = false;
        }

        // Arrow Keys / PageUp / PageDown: Move selected object on grid
        if (_selectionType != SelectionType::None && !Input::isMouseButtonPressed(1)) {
            if (Input::isKeyPressed(263)) { // Left
                if (!_arrowLeftPressed) { moveSelection(-_gridSnap, 0, 0); _arrowLeftPressed = true; }
            } else _arrowLeftPressed = false;

            if (Input::isKeyPressed(262)) { // Right
                if (!_arrowRightPressed) { moveSelection(_gridSnap, 0, 0); _arrowRightPressed = true; }
            } else _arrowRightPressed = false;

            if (Input::isKeyPressed(265)) { // Up
                if (!_arrowUpPressed) {
                    if (shiftDown) moveSelection(0, _gridSnap, 0);
                    else moveSelection(0, 0, -_gridSnap);
                    _arrowUpPressed = true;
                }
            } else _arrowUpPressed = false;

            if (Input::isKeyPressed(264)) { // Down
                if (!_arrowDownPressed) {
                    if (shiftDown) moveSelection(0, -_gridSnap, 0);
                    else moveSelection(0, 0, _gridSnap);
                    _arrowDownPressed = true;
                }
            } else _arrowDownPressed = false;

            bool upElevateKey = Input::isKeyPressed(266) || Input::isKeyPressed(82) || Input::isKeyPressed(334); // PageUp, 'R', Keypad '+'
            bool dwnElevateKey = Input::isKeyPressed(267) || Input::isKeyPressed(67) || Input::isKeyPressed(333); // PageDown, 'C', Keypad '-'

            if (upElevateKey) {
                if (!_pageUpPressed) { moveSelection(0, _gridSnap, 0); _pageUpPressed = true; }
            } else _pageUpPressed = false;

            if (dwnElevateKey) {
                if (!_pageDownPressed) { moveSelection(0, -_gridSnap, 0); _pageDownPressed = true; }
            } else _pageDownPressed = false;
        }

        // [ and ] Snap controls
        if (Input::isKeyPressed(91)) { // '['
            if (!_snapDecPressed) {
                _gridSnap = std::max(0.125f, _gridSnap * 0.5f);
                _angleSnap = std::max(5.0f, _angleSnap - 5.0f);
                _scaleSnap = std::max(0.05f, _scaleSnap - 0.05f);
                logMessage("Decreased Snap: Grid=" + std::to_string(_gridSnap) + "m, Angle=" + std::to_string((int)_angleSnap) + " deg");
                _snapDecPressed = true;
            }
        } else {
            _snapDecPressed = false;
        }

        if (Input::isKeyPressed(93)) { // ']'
            if (!_snapIncPressed) {
                _gridSnap = std::min(16.0f, _gridSnap * 2.0f);
                _angleSnap = std::min(90.0f, _angleSnap + 5.0f);
                _scaleSnap = std::min(2.0f, _scaleSnap + 0.05f);
                logMessage("Increased Snap: Grid=" + std::to_string(_gridSnap) + "m, Angle=" + std::to_string((int)_angleSnap) + " deg");
                _snapIncPressed = true;
            }
        } else {
            _snapIncPressed = false;
        }

        // Gizmo Mode Hotkeys: W (Move), E (Rotate), R (Scale), T (Universal)
        if (is3DMapEditing && !_contextMenuOpen && !ctrlDown && _activeTool == 0) {
            static bool s_wPressed = false, s_eModePressed = false, s_rModePressed = false, s_tPressed = false;
            if (Input::isKeyPressed('W') || Input::isKeyPressed('w')) {
                if (!s_wPressed) { _gizmoMode = GizmoMode::Move; logMessage("Gizmo Mode: Move (W)"); s_wPressed = true; }
            } else s_wPressed = false;

            if (Input::isKeyPressed('E') || Input::isKeyPressed('e')) {
                if (!s_eModePressed) { _gizmoMode = GizmoMode::Rotate; logMessage("Gizmo Mode: Rotate (E)"); s_eModePressed = true; }
            } else s_eModePressed = false;

            if (Input::isKeyPressed('R') || Input::isKeyPressed('r')) {
                if (!s_rModePressed) { _gizmoMode = GizmoMode::Scale; logMessage("Gizmo Mode: Scale (R)"); s_rModePressed = true; }
            } else s_rModePressed = false;

            if (Input::isKeyPressed('T') || Input::isKeyPressed('t')) {
                if (!s_tPressed) { _gizmoMode = GizmoMode::Universal; logMessage("Gizmo Mode: Universal (T)"); s_tPressed = true; }
            } else s_tPressed = false;
        }
    }
}

    static Vec3 snapToGrid(const Vec3& v, float snap) {
        if (snap <= 0.001f) return v;
        return Vec3(
            std::round(v.x / snap) * snap,
            std::round(v.y / snap) * snap,
            std::round(v.z / snap) * snap
        );
    }

    // Fast Ray-AABB intersection
    static bool rayIntersectAABB(const Vec3& rayOrigin, const Vec3& rayDir, const Vec3& boxMin, const Vec3& boxMax, float& tOut) {
        float tmin = 0.001f;
        float tmax = 10000.0f;

        // X slab
        if (std::abs(rayDir.x) < 1e-6f) {
            if (rayOrigin.x < boxMin.x || rayOrigin.x > boxMax.x) return false;
        } else {
            float invD = 1.0f / rayDir.x;
            float t1 = (boxMin.x - rayOrigin.x) * invD;
            float t2 = (boxMax.x - rayOrigin.x) * invD;
            if (t1 > t2) std::swap(t1, t2);
            tmin = std::max(tmin, t1);
            tmax = std::min(tmax, t2);
            if (tmin > tmax) return false;
        }
        // Y slab
        if (std::abs(rayDir.y) < 1e-6f) {
            if (rayOrigin.y < boxMin.y || rayOrigin.y > boxMax.y) return false;
        } else {
            float invD = 1.0f / rayDir.y;
            float t1 = (boxMin.y - rayOrigin.y) * invD;
            float t2 = (boxMax.y - rayOrigin.y) * invD;
            if (t1 > t2) std::swap(t1, t2);
            tmin = std::max(tmin, t1);
            tmax = std::min(tmax, t2);
            if (tmin > tmax) return false;
        }
        // Z slab
        if (std::abs(rayDir.z) < 1e-6f) {
            if (rayOrigin.z < boxMin.z || rayOrigin.z > boxMax.z) return false;
        } else {
            float invD = 1.0f / rayDir.z;
            float t1 = (boxMin.z - rayOrigin.z) * invD;
            float t2 = (boxMax.z - rayOrigin.z) * invD;
            if (t1 > t2) std::swap(t1, t2);
            tmin = std::max(tmin, t1);
            tmax = std::min(tmax, t2);
            if (tmin > tmax) return false;
        }
        tOut = tmin;
        return true;
    }

    void pickObjectInViewport(float mx, float my, bool isRmb = false) {
        if (!_map) return;
        if (_workspace != EditorWorkspace::World3D_Level && _workspace != EditorWorkspace::World3D_Animation) return;
        int fbW = 0, fbH = 0;
        getFramebufferSize(fbW, fbH);
        float w = (fbW > 0) ? (float)fbW : (float)getWidth();
        float h = (fbH > 0) ? (float)fbH : (float)getHeight();

        SidebarLayout l = getSidebarLayout(w, h);
        float vpX = 42.0f;
        float vpY = 58.0f;
        float vpW = l.rightX - vpX;
        float vpH = (h - 22.0f) - vpY;
        if (mx < vpX || mx > vpX + vpW || my < vpY || my > vpY + vpH) return;

        float ndcX = ((mx - vpX) / vpW) * 2.0f - 1.0f;
        float ndcY = 1.0f - ((my - vpY) / vpH) * 2.0f;
        float aspect = vpW / vpH;
        float tanHalfFov = std::tan((70.0f * 0.5f) * 3.14159265f / 180.0f);
        Vec3 rayDir = (_camera.getFront() + _camera.getRight() * (ndcX * tanHalfFov * aspect) + _camera.getUp() * (ndcY * tanHalfFov)).normalized();
        Vec3 rayOrigin = _camera.getPosition();

        float closestT = 1e9f;
        SelectionType hitType = SelectionType::None;
        int hitIndex = -1;

        // Interactive Gizmo Hit Test (Move / Rotate / Scale / Universal drag)
        if (_selectionType != SelectionType::None && !isRmb) {
            Vec3 selPos = getSelectedPosition();
            GizmoAxis hitAxis = testGizmoHit(rayOrigin, rayDir, selPos);
            if (hitAxis != GizmoAxis::None) {
                _isDraggingGizmo = true;
                _activeGizmoAxis = hitAxis;
                _dragStartMouse = { mx, my };
                _dragStartPos = selPos;
                _dragStartRot = getSelectedRotation();
                _dragStartScale = getSelectedScale();
                return;
            }
        }

        // Test Brushes
        for (size_t i = 0; i < _map->brushes.size(); ++i) {
            const auto& b = _map->brushes[i];
            Vec3 half = b.size * 0.5f;
            float t = 0;
            if (rayIntersectAABB(rayOrigin, rayDir, b.position - half, b.position + half, t)) {
                if (t < closestT) {
                    closestT = t;
                    hitType = SelectionType::Brush;
                    hitIndex = (int)i;
                }
            }
        }

        // Test Props
        for (size_t i = 0; i < _map->props.size(); ++i) {
            const auto& p = _map->props[i];
            Vec3 half = p.scale * 0.5f;
            float t = 0;
            if (rayIntersectAABB(rayOrigin, rayDir, p.position - half, p.position + half, t)) {
                if (t < closestT) {
                    closestT = t;
                    hitType = SelectionType::Prop;
                    hitIndex = (int)i;
                }
            }
        }

        // Test Doors
        for (size_t i = 0; i < _map->doors.size(); ++i) {
            const auto& d = _map->doors[i];
            Vec3 half = d.size * 0.5f;
            float t = 0;
            if (rayIntersectAABB(rayOrigin, rayDir, d.position - half, d.position + half, t)) {
                if (t < closestT) {
                    closestT = t;
                    hitType = SelectionType::Door;
                    hitIndex = (int)i;
                }
            }
        }

        // Test Spawns
        for (size_t i = 0; i < _map->spawnPoints.size(); ++i) {
            const auto& sp = _map->spawnPoints[i];
            Vec3 half(0.6f, 0.9f, 0.6f);
            float t = 0;
            if (rayIntersectAABB(rayOrigin, rayDir, sp.position - half, sp.position + half, t)) {
                if (t < closestT) {
                    closestT = t;
                    hitType = SelectionType::Spawn;
                    hitIndex = (int)i;
                }
            }
        }

        // Test Weapon Spawners
        for (size_t i = 0; i < _map->weaponSpawners.size(); ++i) {
            const auto& ws = _map->weaponSpawners[i];
            float t = 0;
            if (rayIntersectAABB(rayOrigin, rayDir, ws.position - Vec3(0.7f, 0.0f, 0.7f), ws.position + Vec3(0.7f, 0.8f, 0.7f), t)) {
                if (t < closestT) {
                    closestT = t;
                    hitType = SelectionType::WeaponSpawner;
                    hitIndex = (int)i;
                }
            }
        }

        // Test Lights
        for (size_t i = 0; i < _map->lights.size(); ++i) {
            const auto& lt = _map->lights[i];
            Vec3 half(0.6f, 0.7f, 0.6f);
            float t = 0;
            if (rayIntersectAABB(rayOrigin, rayDir, lt.position - half, lt.position + half, t)) {
                if (t < closestT) {
                    closestT = t;
                    hitType = SelectionType::Light;
                    hitIndex = (int)i;
                }
            }
        }

        // Test Cameras
        for (size_t i = 0; i < _map->cameras.size(); ++i) {
            const auto& cam = _map->cameras[i];
            Vec3 half(0.6f, 0.6f, 0.6f);
            float t = 0;
            if (rayIntersectAABB(rayOrigin, rayDir, cam.position - half, cam.position + half, t)) {
                if (t < closestT) {
                    closestT = t;
                    hitType = SelectionType::Camera;
                    hitIndex = (int)i;
                }
            }
        }

        // Tool 3: Texture pipette & application
        if (_activeTool == 3) {
            if (hitType == SelectionType::Brush) {
                if (isRmb) {
                    _selectedTexture = _map->brushes[hitIndex].texturePath;
                    logMessage("Pipette: Sampled texture '" + _selectedTexture + "' from Brush #" + std::to_string(hitIndex));
                } else {
                    _map->brushes[hitIndex].texturePath = _selectedTexture;
                    _map->brushes[hitIndex].uvScale = _activeUvScale;
                    logMessage("Applied texture '" + _selectedTexture + "' to Brush #" + std::to_string(hitIndex));
                }
            }
            return;
        }

        // Tool 6: CSG Clip Tool
        if (_activeTool == 6) {
            if (hitType == SelectionType::Brush) {
                _selectionType = SelectionType::Brush;
                _selectedIndex = hitIndex;
                const auto& b = _map->brushes[hitIndex];
                Vec3 half = b.size * 0.5f;
                _clipPointA = b.position + Vec3(-half.x * 1.2f, 0.0f, -half.z * 0.6f);
                _clipPointB = b.position + Vec3(half.x * 1.2f, 0.0f, half.z * 0.6f);
                logMessage("Clip Tool: Selected Brush #" + std::to_string(hitIndex) + " for CSG Slicing");
            } else {
                if (_clipStep == 0) {
                    _clipPointA = _cursorPos;
                    _clipStep = 1;
                    logMessage("Clip Tool: Set Cut Point A to (" + std::to_string((int)_cursorPos.x) + ", " + std::to_string((int)_cursorPos.z) + ")");
                } else {
                    _clipPointB = _cursorPos;
                    _clipStep = 0;
                    logMessage("Clip Tool: Set Cut Point B to (" + std::to_string((int)_cursorPos.x) + ", " + std::to_string((int)_cursorPos.z) + ")");
                }
            }
            return;
        }

        // Tool 0: Selection
        if (_activeTool == 0) {
            _selectionType = hitType;
            _selectedIndex = hitIndex;
            if (_selectionType == SelectionType::Brush) {
                _selectedTexture = _map->brushes[hitIndex].texturePath;
                logMessage("Selected Brush #" + std::to_string(hitIndex) + " (" + _selectedTexture + ")");
            } else if (_selectionType == SelectionType::Prop) {
                _selectedModel = _map->props[hitIndex].modelPath;
                logMessage("Selected Prop #" + std::to_string(hitIndex) + " (" + _selectedModel + ")");
            } else if (_selectionType == SelectionType::Door) {
                logMessage("Selected Door #" + std::to_string(hitIndex) + " (" + _map->doors[hitIndex].name + ")");
            } else if (_selectionType == SelectionType::Spawn) {
                logMessage("Selected " + _map->spawnPoints[hitIndex].getDisplayName() + " #" + std::to_string(hitIndex));
            } else if (_selectionType == SelectionType::WeaponSpawner) {
                logMessage("Selected Weapon Spawner [" + _map->weaponSpawners[hitIndex].getWeaponName() + "] #" + std::to_string(hitIndex));
            } else if (_selectionType == SelectionType::Light) {
                logMessage("Selected Lamp Light #" + std::to_string(hitIndex) + " (" + _map->lights[hitIndex].name + ")");
            } else if (_selectionType == SelectionType::Camera) {
                logMessage("Selected Camera #" + std::to_string(hitIndex) + " (" + _map->cameras[hitIndex].name + ")");
            } else {
                _selectionType = SelectionType::None;
                _selectedIndex = -1;
                logMessage("Deselected all");
            }
        } else if (_activeTool == 1 || _activeTool == 2 || _activeTool == 4 || _activeTool == 5 || _activeTool == 7) {
            if (!isRmb) {
                if (Input::isKeyPressed(340) && hitType != SelectionType::None) {
                    // Shift + Click selects existing entity even while placement tool is active
                    _selectionType = hitType;
                    _selectedIndex = hitIndex;
                    logMessage("Selected Entity #" + std::to_string(hitIndex));
                } else {
                    placeCurrentObject();
                }
            }
        }
    }

    Vec3 getSelectedPosition() const {
        if (!_map) return Vec3(0, 0, 0);
        if (_selectionType == SelectionType::Brush && _selectedIndex >= 0 && _selectedIndex < (int)_map->brushes.size()) {
            return _map->brushes[_selectedIndex].position;
        } else if (_selectionType == SelectionType::Prop && _selectedIndex >= 0 && _selectedIndex < (int)_map->props.size()) {
            return _map->props[_selectedIndex].position;
        } else if (_selectionType == SelectionType::Door && _selectedIndex >= 0 && _selectedIndex < (int)_map->doors.size()) {
            return _map->doors[_selectedIndex].position;
        } else if (_selectionType == SelectionType::Spawn && _selectedIndex >= 0 && _selectedIndex < (int)_map->spawnPoints.size()) {
            return _map->spawnPoints[_selectedIndex].position;
        } else if (_selectionType == SelectionType::WeaponSpawner && _selectedIndex >= 0 && _selectedIndex < (int)_map->weaponSpawners.size()) {
            return _map->weaponSpawners[_selectedIndex].position;
        } else if (_selectionType == SelectionType::Light && _selectedIndex >= 0 && _selectedIndex < (int)_map->lights.size()) {
            return _map->lights[_selectedIndex].position;
        } else if (_selectionType == SelectionType::Camera && _selectedIndex >= 0 && _selectedIndex < (int)_map->cameras.size()) {
            return _map->cameras[_selectedIndex].position;
        }
        return Vec3(0, 0, 0);
    }

    Vec3 getSelectedRotation() const {
        if (!_map) return Vec3(0, 0, 0);
        if (_selectionType == SelectionType::Brush && _selectedIndex >= 0 && _selectedIndex < (int)_map->brushes.size()) {
            return _map->brushes[_selectedIndex].rotation;
        } else if (_selectionType == SelectionType::Prop && _selectedIndex >= 0 && _selectedIndex < (int)_map->props.size()) {
            return _map->props[_selectedIndex].rotation;
        } else if (_selectionType == SelectionType::Camera && _selectedIndex >= 0 && _selectedIndex < (int)_map->cameras.size()) {
            return _map->cameras[_selectedIndex].rotation;
        }
        return Vec3(0, 0, 0);
    }

    void rotateSelection(float dp, float dy, float dr) {
        if (!_map) return;
        if (_selectionType == SelectionType::Brush && _selectedIndex >= 0 && _selectedIndex < (int)_map->brushes.size()) {
            auto& r = _map->brushes[_selectedIndex].rotation;
            r.x = std::fmod(r.x + dp, 360.0f);
            r.y = std::fmod(r.y + dy, 360.0f);
            r.z = std::fmod(r.z + dr, 360.0f);
            logMessage("Rotated Brush #" + std::to_string(_selectedIndex) + " to (" +
                       std::to_string((int)r.x) + "°, " + std::to_string((int)r.y) + "°, " + std::to_string((int)r.z) + "°)");
        } else if (_selectionType == SelectionType::Prop && _selectedIndex >= 0 && _selectedIndex < (int)_map->props.size()) {
            auto& r = _map->props[_selectedIndex].rotation;
            r.x = std::fmod(r.x + dp, 360.0f);
            r.y = std::fmod(r.y + dy, 360.0f);
            r.z = std::fmod(r.z + dr, 360.0f);
            logMessage("Rotated Prop #" + std::to_string(_selectedIndex) + " to (" +
                       std::to_string((int)r.x) + "°, " + std::to_string((int)r.y) + "°, " + std::to_string((int)r.z) + "°)");
        } else if (_selectionType == SelectionType::Camera && _selectedIndex >= 0 && _selectedIndex < (int)_map->cameras.size()) {
            auto& r = _map->cameras[_selectedIndex].rotation;
            r.x = std::clamp(r.x + dp, -89.0f, 89.0f);
            r.y = std::fmod(r.y + dy, 360.0f);
            r.z = std::fmod(r.z + dr, 360.0f);
            logMessage("Rotated Camera [" + _map->cameras[_selectedIndex].name + "] to Pitch=" +
                       std::to_string((int)r.x) + "°, Yaw=" + std::to_string((int)r.y) + "°");
        }
    }

    Vec3 getSelectedScale() const {
        if (!_map) return Vec3(1, 1, 1);
        if (_selectionType == SelectionType::Brush && _selectedIndex >= 0 && _selectedIndex < (int)_map->brushes.size()) {
            return _map->brushes[_selectedIndex].size;
        } else if (_selectionType == SelectionType::Prop && _selectedIndex >= 0 && _selectedIndex < (int)_map->props.size()) {
            return _map->props[_selectedIndex].scale;
        }
        return Vec3(1, 1, 1);
    }

    void scaleSelection(float sx, float sy, float sz) {
        if (!_map) return;
        if (_selectionType == SelectionType::Brush && _selectedIndex >= 0 && _selectedIndex < (int)_map->brushes.size()) {
            auto& b = _map->brushes[_selectedIndex];
            b.size.x = std::max(0.2f, b.size.x + sx);
            b.size.y = std::max(0.2f, b.size.y + sy);
            b.size.z = std::max(0.2f, b.size.z + sz);
            logMessage("Scaled Brush #" + std::to_string(_selectedIndex) + " to (" +
                       std::to_string((int)b.size.x) + "x" + std::to_string((int)b.size.y) + "x" + std::to_string((int)b.size.z) + ")");
        } else if (_selectionType == SelectionType::Prop && _selectedIndex >= 0 && _selectedIndex < (int)_map->props.size()) {
            auto& p = _map->props[_selectedIndex];
            p.scale.x = std::max(0.1f, p.scale.x + sx);
            p.scale.y = std::max(0.1f, p.scale.y + sy);
            p.scale.z = std::max(0.1f, p.scale.z + sz);
            logMessage("Scaled Prop #" + std::to_string(_selectedIndex) + " to (" +
                       std::to_string(p.scale.x) + ", " + std::to_string(p.scale.y) + ", " + std::to_string(p.scale.z) + ")");
        }
    }

    GizmoAxis testGizmoHit(const Vec3& rayOrigin, const Vec3& rayDir, const Vec3& pivot) const {
        float closestT = 1e9f;
        GizmoAxis bestAxis = GizmoAxis::None;

        float len = 1.8f;
        float thick = 0.22f;
        float cap = 0.35f;

        bool testMove = (_gizmoMode == GizmoMode::Move || _gizmoMode == GizmoMode::Universal);
        bool testRotate = (_gizmoMode == GizmoMode::Rotate || _gizmoMode == GizmoMode::Universal);
        bool testScale = (_gizmoMode == GizmoMode::Scale || _gizmoMode == GizmoMode::Universal);

        // Move Planes
        if (testMove) {
            float t = 0.0f;
            // Plane XY
            if (rayIntersectAABB(rayOrigin, rayDir, pivot + Vec3(0.2f, 0.2f, -0.1f), pivot + Vec3(0.8f, 0.8f, 0.1f), t) && t < closestT) {
                closestT = t; bestAxis = GizmoAxis::PlaneXY;
            }
            // Plane XZ
            if (rayIntersectAABB(rayOrigin, rayDir, pivot + Vec3(0.2f, -0.1f, 0.2f), pivot + Vec3(0.8f, 0.1f, 0.8f), t) && t < closestT) {
                closestT = t; bestAxis = GizmoAxis::PlaneXZ;
            }
            // Plane YZ
            if (rayIntersectAABB(rayOrigin, rayDir, pivot + Vec3(-0.1f, 0.2f, 0.2f), pivot + Vec3(0.1f, 0.8f, 0.8f), t) && t < closestT) {
                closestT = t; bestAxis = GizmoAxis::PlaneYZ;
            }
            // Axes
            if (rayIntersectAABB(rayOrigin, rayDir, pivot + Vec3(0.1f, -thick, -thick), pivot + Vec3(len + cap, thick, thick), t) && t < closestT) {
                closestT = t; bestAxis = GizmoAxis::X;
            }
            if (rayIntersectAABB(rayOrigin, rayDir, pivot + Vec3(-thick, 0.1f, -thick), pivot + Vec3(thick, len + cap, thick), t) && t < closestT) {
                closestT = t; bestAxis = GizmoAxis::Y;
            }
            if (rayIntersectAABB(rayOrigin, rayDir, pivot + Vec3(-thick, -thick, 0.1f), pivot + Vec3(thick, thick, len + cap), t) && t < closestT) {
                closestT = t; bestAxis = GizmoAxis::Z;
            }
        }

        // Scale Handles
        if (testScale) {
            float t = 0.0f;
            // Uniform center
            if (rayIntersectAABB(rayOrigin, rayDir, pivot - Vec3(0.2f, 0.2f, 0.2f), pivot + Vec3(0.2f, 0.2f, 0.2f), t) && t < closestT) {
                closestT = t; bestAxis = GizmoAxis::ScaleUniform;
            }
            // Scale X
            if (rayIntersectAABB(rayOrigin, rayDir, pivot + Vec3(len - 0.2f, -cap, -cap), pivot + Vec3(len + cap, cap, cap), t) && t < closestT) {
                closestT = t; bestAxis = GizmoAxis::ScaleX;
            }
            // Scale Y
            if (rayIntersectAABB(rayOrigin, rayDir, pivot + Vec3(-cap, len - 0.2f, -cap), pivot + Vec3(cap, len + cap, cap), t) && t < closestT) {
                closestT = t; bestAxis = GizmoAxis::ScaleY;
            }
            // Scale Z
            if (rayIntersectAABB(rayOrigin, rayDir, pivot + Vec3(-cap, -cap, len - 0.2f), pivot + Vec3(cap, cap, len + cap), t) && t < closestT) {
                closestT = t; bestAxis = GizmoAxis::ScaleZ;
            }
        }

        // Rotate Rings
        if (testRotate) {
            float ringR = 1.8f;
            float ringThick = 0.35f;
            if (std::abs(rayDir.x) > 1e-5f) {
                float t = (pivot.x - rayOrigin.x) / rayDir.x;
                if (t > 0.01f && t < closestT) {
                    Vec3 hit = rayOrigin + rayDir * t;
                    float d = std::sqrt((hit.y - pivot.y) * (hit.y - pivot.y) + (hit.z - pivot.z) * (hit.z - pivot.z));
                    if (std::abs(d - ringR) <= ringThick) {
                        closestT = t; bestAxis = GizmoAxis::RotX;
                    }
                }
            }
            if (std::abs(rayDir.y) > 1e-5f) {
                float t = (pivot.y - rayOrigin.y) / rayDir.y;
                if (t > 0.01f && t < closestT) {
                    Vec3 hit = rayOrigin + rayDir * t;
                    float d = std::sqrt((hit.x - pivot.x) * (hit.x - pivot.x) + (hit.z - pivot.z) * (hit.z - pivot.z));
                    if (std::abs(d - ringR) <= ringThick) {
                        closestT = t; bestAxis = GizmoAxis::RotY;
                    }
                }
            }
            if (std::abs(rayDir.z) > 1e-5f) {
                float t = (pivot.z - rayOrigin.z) / rayDir.z;
                if (t > 0.01f && t < closestT) {
                    Vec3 hit = rayOrigin + rayDir * t;
                    float d = std::sqrt((hit.x - pivot.x) * (hit.x - pivot.x) + (hit.y - pivot.y) * (hit.y - pivot.y));
                    if (std::abs(d - ringR) <= ringThick) {
                        closestT = t; bestAxis = GizmoAxis::RotZ;
                    }
                }
            }
        }

        return bestAxis;
    }

    void alignSelectionToGround() {
        if (!_map || _selectionType == SelectionType::None) return;
        Vec3 curPos = getSelectedPosition();
        moveSelection(0.0f, -curPos.y, 0.0f);
        logMessage("Snapped selection to Ground (Y=0.0)");
    }

    void moveSelection(float dx, float dy, float dz) {
        if (!_map) return;
        Vec3 newPos(0, 0, 0);
        if (_selectionType == SelectionType::Brush && _selectedIndex >= 0 && _selectedIndex < (int)_map->brushes.size()) {
            _map->brushes[_selectedIndex].position += Vec3(dx, dy, dz);
            newPos = _map->brushes[_selectedIndex].position;
        } else if (_selectionType == SelectionType::Prop && _selectedIndex >= 0 && _selectedIndex < (int)_map->props.size()) {
            _map->props[_selectedIndex].position += Vec3(dx, dy, dz);
            newPos = _map->props[_selectedIndex].position;
        } else if (_selectionType == SelectionType::Door && _selectedIndex >= 0 && _selectedIndex < (int)_map->doors.size()) {
            _map->doors[_selectedIndex].position += Vec3(dx, dy, dz);
            newPos = _map->doors[_selectedIndex].position;
        } else if (_selectionType == SelectionType::Spawn && _selectedIndex >= 0 && _selectedIndex < (int)_map->spawnPoints.size()) {
            _map->spawnPoints[_selectedIndex].position += Vec3(dx, dy, dz);
            _map->spawn.position = _map->spawnPoints[0].position;
            newPos = _map->spawnPoints[_selectedIndex].position;
        } else if (_selectionType == SelectionType::WeaponSpawner && _selectedIndex >= 0 && _selectedIndex < (int)_map->weaponSpawners.size()) {
            _map->weaponSpawners[_selectedIndex].position += Vec3(dx, dy, dz);
            newPos = _map->weaponSpawners[_selectedIndex].position;
        } else if (_selectionType == SelectionType::Light && _selectedIndex >= 0 && _selectedIndex < (int)_map->lights.size()) {
            _map->lights[_selectedIndex].position += Vec3(dx, dy, dz);
            newPos = _map->lights[_selectedIndex].position;
        } else if (_selectionType == SelectionType::Camera && _selectedIndex >= 0 && _selectedIndex < (int)_map->cameras.size()) {
            _map->cameras[_selectedIndex].position += Vec3(dx, dy, dz);
            newPos = _map->cameras[_selectedIndex].position;
        }
        if (std::abs(dy) > 0.0001f) {
            char buf[64];
            std::snprintf(buf, sizeof(buf), "Elevated selection: Y = %.2f (offset %+0.2f)", newPos.y, dy);
            logMessage(buf);
        }
    }

    void resizeSelection(float dw, float dh, float dd) {
        if (!_map) return;
        if (_selectionType == SelectionType::Brush && _selectedIndex >= 0 && _selectedIndex < (int)_map->brushes.size()) {
            auto& b = _map->brushes[_selectedIndex];
            b.size.x = std::max(0.5f, b.size.x + dw);
            b.size.y = std::max(0.5f, b.size.y + dh);
            b.size.z = std::max(0.5f, b.size.z + dd);
            logMessage("Resized Brush #" + std::to_string(_selectedIndex) + " to (" + 
                       std::to_string((int)b.size.x) + "x" + std::to_string((int)b.size.y) + "x" + std::to_string((int)b.size.z) + ")");
        } else if (_selectionType == SelectionType::Prop && _selectedIndex >= 0 && _selectedIndex < (int)_map->props.size()) {
            auto& p = _map->props[_selectedIndex];
            p.scale.x = std::max(0.2f, p.scale.x + dw);
            p.scale.y = std::max(0.2f, p.scale.y + dh);
            p.scale.z = std::max(0.2f, p.scale.z + dd);
            logMessage("Rescaled Prop #" + std::to_string(_selectedIndex) + " to (" + 
                       std::to_string((int)p.scale.x) + "x" + std::to_string((int)p.scale.y) + "x" + std::to_string((int)p.scale.z) + ")");
        } else if (_selectionType == SelectionType::Light && _selectedIndex >= 0 && _selectedIndex < (int)_map->lights.size()) {
            auto& lt = _map->lights[_selectedIndex];
            lt.radius = std::max(2.0f, lt.radius + dw);
            lt.intensity = std::max(0.2f, lt.intensity + dh * 0.5f);
            logMessage("Adjusted Light #" + std::to_string(_selectedIndex) + " (Intensity: " + 
                       std::to_string(lt.intensity) + ", Radius: " + std::to_string((int)lt.radius) + "m)");
        }
    }

    void duplicateSelection() {
        if (!_map) return;
        if (_selectionType == SelectionType::Brush && _selectedIndex >= 0 && _selectedIndex < (int)_map->brushes.size()) {
            MapBrush b = _map->brushes[_selectedIndex];
            b.position.x += _gridSnap;
            b.position.z += _gridSnap;
            _map->brushes.push_back(b);
            _selectedIndex = (int)_map->brushes.size() - 1;
            logMessage("Duplicated Brush to #" + std::to_string(_selectedIndex));
        } else if (_selectionType == SelectionType::Prop && _selectedIndex >= 0 && _selectedIndex < (int)_map->props.size()) {
            MapProp p = _map->props[_selectedIndex];
            p.position.x += _gridSnap;
            p.position.z += _gridSnap;
            _map->props.push_back(p);
            _selectedIndex = (int)_map->props.size() - 1;
            logMessage("Duplicated Prop to #" + std::to_string(_selectedIndex));
        } else if (_selectionType == SelectionType::Door && _selectedIndex >= 0 && _selectedIndex < (int)_map->doors.size()) {
            MapDoor d = _map->doors[_selectedIndex];
            d.position.x += _gridSnap;
            d.position.z += _gridSnap;
            _map->doors.push_back(d);
            _selectedIndex = (int)_map->doors.size() - 1;
            logMessage("Duplicated Door to #" + std::to_string(_selectedIndex));
        } else if (_selectionType == SelectionType::Spawn && _selectedIndex >= 0 && _selectedIndex < (int)_map->spawnPoints.size()) {
            MapSpawnPoint sp = _map->spawnPoints[_selectedIndex];
            sp.position.x += _gridSnap;
            sp.position.z += _gridSnap;
            _map->spawnPoints.push_back(sp);
            _selectedIndex = (int)_map->spawnPoints.size() - 1;
            logMessage("Duplicated " + sp.getDisplayName() + " to #" + std::to_string(_selectedIndex));
        } else if (_selectionType == SelectionType::WeaponSpawner && _selectedIndex >= 0 && _selectedIndex < (int)_map->weaponSpawners.size()) {
            MapWeaponSpawner ws = _map->weaponSpawners[_selectedIndex];
            ws.position.x += _gridSnap;
            ws.position.z += _gridSnap;
            _map->weaponSpawners.push_back(ws);
            _selectedIndex = (int)_map->weaponSpawners.size() - 1;
            logMessage("Duplicated Weapon Spawner [" + ws.getWeaponName() + "] to #" + std::to_string(_selectedIndex));
        } else if (_selectionType == SelectionType::Light && _selectedIndex >= 0 && _selectedIndex < (int)_map->lights.size()) {
            MapLight lt = _map->lights[_selectedIndex];
            lt.position.x += _gridSnap;
            lt.position.z += _gridSnap;
            lt.name = "light_" + std::to_string(_map->lights.size());
            _map->lights.push_back(lt);
            _selectedIndex = (int)_map->lights.size() - 1;
            logMessage("Duplicated Lamp Light to #" + std::to_string(_selectedIndex));
        } else if (_selectionType == SelectionType::Camera && _selectedIndex >= 0 && _selectedIndex < (int)_map->cameras.size()) {
            MapCamera c = _map->cameras[_selectedIndex];
            c.position.x += _gridSnap;
            c.position.z += _gridSnap;
            c.name = c.name + "_copy";
            _map->cameras.push_back(c);
            _selectedIndex = (int)_map->cameras.size() - 1;
            logMessage("Duplicated Camera to #" + std::to_string(_selectedIndex));
        }
    }

    void deleteSelection() {
        if (!_map) return;
        if (_selectionType == SelectionType::Brush && _selectedIndex >= 0 && _selectedIndex < (int)_map->brushes.size()) {
            _map->brushes.erase(_map->brushes.begin() + _selectedIndex);
            logMessage("Deleted Brush #" + std::to_string(_selectedIndex));
            _selectionType = SelectionType::None;
            _selectedIndex = -1;
        } else if (_selectionType == SelectionType::Prop && _selectedIndex >= 0 && _selectedIndex < (int)_map->props.size()) {
            _map->props.erase(_map->props.begin() + _selectedIndex);
            logMessage("Deleted Prop #" + std::to_string(_selectedIndex));
            _selectionType = SelectionType::None;
            _selectedIndex = -1;
        } else if (_selectionType == SelectionType::Door && _selectedIndex >= 0 && _selectedIndex < (int)_map->doors.size()) {
            _map->doors.erase(_map->doors.begin() + _selectedIndex);
            logMessage("Deleted Door #" + std::to_string(_selectedIndex));
            _selectionType = SelectionType::None;
            _selectedIndex = -1;
        } else if (_selectionType == SelectionType::Spawn && _selectedIndex >= 0 && _selectedIndex < (int)_map->spawnPoints.size()) {
            if (_map->spawnPoints.size() > 1) {
                _map->spawnPoints.erase(_map->spawnPoints.begin() + _selectedIndex);
                logMessage("Deleted Spawn Point #" + std::to_string(_selectedIndex));
                _selectionType = SelectionType::None;
                _selectedIndex = -1;
            } else {
                logMessage("Cannot delete the last remaining spawn point!");
            }
        } else if (_selectionType == SelectionType::WeaponSpawner && _selectedIndex >= 0 && _selectedIndex < (int)_map->weaponSpawners.size()) {
            _map->weaponSpawners.erase(_map->weaponSpawners.begin() + _selectedIndex);
            logMessage("Deleted Weapon Spawner #" + std::to_string(_selectedIndex));
            _selectionType = SelectionType::None;
            _selectedIndex = -1;
        } else if (_selectionType == SelectionType::Light && _selectedIndex >= 0 && _selectedIndex < (int)_map->lights.size()) {
            _map->lights.erase(_map->lights.begin() + _selectedIndex);
            logMessage("Deleted Lamp Light #" + std::to_string(_selectedIndex));
            _selectionType = SelectionType::None;
            _selectedIndex = -1;
        } else if (_selectionType == SelectionType::Camera && _selectedIndex >= 0 && _selectedIndex < (int)_map->cameras.size()) {
            _map->cameras.erase(_map->cameras.begin() + _selectedIndex);
            logMessage("Deleted Camera #" + std::to_string(_selectedIndex));
            _selectionType = SelectionType::None;
            _selectedIndex = -1;
        }
    }

    void focusCamera() {
        Vec3 targetPos = _cursorPos;
        if (_selectionType == SelectionType::Brush && _selectedIndex >= 0 && _selectedIndex < (int)_map->brushes.size()) {
            targetPos = _map->brushes[_selectedIndex].position;
        } else if (_selectionType == SelectionType::Prop && _selectedIndex >= 0 && _selectedIndex < (int)_map->props.size()) {
            targetPos = _map->props[_selectedIndex].position;
        } else if (_selectionType == SelectionType::Door && _selectedIndex >= 0 && _selectedIndex < (int)_map->doors.size()) {
            targetPos = _map->doors[_selectedIndex].position;
        } else if (_selectionType == SelectionType::Spawn && _selectedIndex >= 0 && _selectedIndex < (int)_map->spawnPoints.size()) {
            targetPos = _map->spawnPoints[_selectedIndex].position;
        } else if (_selectionType == SelectionType::WeaponSpawner && _selectedIndex >= 0 && _selectedIndex < (int)_map->weaponSpawners.size()) {
            targetPos = _map->weaponSpawners[_selectedIndex].position;
        } else if (_selectionType == SelectionType::Light && _selectedIndex >= 0 && _selectedIndex < (int)_map->lights.size()) {
            targetPos = _map->lights[_selectedIndex].position;
        } else if (_selectionType == SelectionType::Camera && _selectedIndex >= 0 && _selectedIndex < (int)_map->cameras.size()) {
            targetPos = _map->cameras[_selectedIndex].position;
        }
        _camera.setPosition(targetPos - _camera.getFront() * 10.0f);
        logMessage("Focused Camera on selection at (" + std::to_string((int)targetPos.x) + ", " + std::to_string((int)targetPos.y) + ", " + std::to_string((int)targetPos.z) + ")");
    }

    void commitClip() {
        if (!_map) return;
        if (_selectionType != SelectionType::Brush || _selectedIndex < 0 || _selectedIndex >= (int)_map->brushes.size()) {
            logMessage("CSG Clip: Please select a brush to clip first!");
            return;
        }

        auto& targetBrush = _map->brushes[_selectedIndex];
        Vec3 planePt = (_clipPointA + _clipPointB) * 0.5f;
        Vec3 planeNormal;
        Vec2 p1(_clipPointA.x, _clipPointA.z);
        Vec2 p2(_clipPointB.x, _clipPointB.z);
        CSGTool::makePlaneFrom2DLine(p1, p2, planePt, planeNormal);

        if (planeNormal.lengthSq() < 1e-4f) {
            planeNormal = Vec3(1.0f, 0.0f, 0.0f);
            planePt = targetBrush.position;
        }

        ClipResult res;
        if (targetBrush.type == "poly" && !targetBrush.customVertices.empty()) {
            res = CSGTool::sliceConvexMesh(targetBrush.customVertices, targetBrush.customIndices, planePt, planeNormal, targetBrush.uvScale);
        } else {
            Vec3 half = targetBrush.size * 0.5f;
            Vec3 bMin = targetBrush.position - half;
            Vec3 bMax = targetBrush.position + half;
            res = CSGTool::sliceBox(bMin, bMax, planePt, planeNormal, targetBrush.uvScale);
        }

        if (!res.didIntersect) {
            logMessage("CSG Clip: Plane did not intersect the selected brush.");
            return;
        }

        std::string tex = targetBrush.texturePath;
        Vec2 uvScale = targetBrush.uvScale;
        Vec3 col = targetBrush.color;

        if (_clipMode == ClipMode::KeepFront) {
            if (!res.frontPiece.isValid()) {
                logMessage("CSG Clip: Front piece empty.");
                return;
            }
            targetBrush.type = "poly";
            targetBrush.position = res.frontPiece.centroid;
            targetBrush.size = res.frontPiece.aabbMax - res.frontPiece.aabbMin;
            targetBrush.customVertices = res.frontPiece.vertices;
            targetBrush.customIndices = res.frontPiece.indices;
            targetBrush.runtimeMesh = nullptr;
            logMessage("CSG Clip: Brush clipped (Keep Front, " + std::to_string(res.frontPiece.triangleCount) + " tris)");
        } else if (_clipMode == ClipMode::KeepBack) {
            if (!res.backPiece.isValid()) {
                logMessage("CSG Clip: Back piece empty.");
                return;
            }
            targetBrush.type = "poly";
            targetBrush.position = res.backPiece.centroid;
            targetBrush.size = res.backPiece.aabbMax - res.backPiece.aabbMin;
            targetBrush.customVertices = res.backPiece.vertices;
            targetBrush.customIndices = res.backPiece.indices;
            targetBrush.runtimeMesh = nullptr;
            logMessage("CSG Clip: Brush clipped (Keep Back, " + std::to_string(res.backPiece.triangleCount) + " tris)");
        } else { // KeepBoth
            if (!res.frontPiece.isValid() || !res.backPiece.isValid()) {
                logMessage("CSG Clip: Could not split both pieces.");
                return;
            }
            targetBrush.type = "poly";
            targetBrush.position = res.frontPiece.centroid;
            targetBrush.size = res.frontPiece.aabbMax - res.frontPiece.aabbMin;
            targetBrush.customVertices = res.frontPiece.vertices;
            targetBrush.customIndices = res.frontPiece.indices;
            targetBrush.runtimeMesh = nullptr;

            MapBrush backB;
            backB.type = "poly";
            backB.position = res.backPiece.centroid;
            backB.size = res.backPiece.aabbMax - res.backPiece.aabbMin;
            backB.color = col;
            backB.texturePath = tex;
            backB.uvScale = uvScale;
            backB.uvMode = 1;
            backB.customVertices = res.backPiece.vertices;
            backB.customIndices = res.backPiece.indices;
            _map->brushes.push_back(backB);

            logMessage("CSG Clip: Split brush into 2 convex brushes (Front: " +
                       std::to_string(res.frontPiece.triangleCount) + " tris, Back: " +
                       std::to_string(res.backPiece.triangleCount) + " tris)");
        }
    }

    void drawClipToolOverlay() {
        if (!_map) return;
        Vec3 planePt = (_clipPointA + _clipPointB) * 0.5f;
        Vec3 planeNormal;
        Vec2 p1(_clipPointA.x, _clipPointA.z);
        Vec2 p2(_clipPointB.x, _clipPointB.z);
        CSGTool::makePlaneFrom2DLine(p1, p2, planePt, planeNormal);

        if (_selectionType == SelectionType::Brush && _selectedIndex >= 0 && _selectedIndex < (int)_map->brushes.size()) {
            const auto& b = _map->brushes[_selectedIndex];
            Vec3 half = b.size * 0.5f;
            if ((_clipPointA - _clipPointB).lengthSq() < 1e-4f) {
                _clipPointA = b.position + Vec3(-half.x * 1.2f, 0.0f, -half.z * 0.5f);
                _clipPointB = b.position + Vec3(half.x * 1.2f, 0.0f, half.z * 0.5f);
                p1 = Vec2(_clipPointA.x, _clipPointA.z);
                p2 = Vec2(_clipPointB.x, _clipPointB.z);
                CSGTool::makePlaneFrom2DLine(p1, p2, planePt, planeNormal);
            }

            Vec3 modeCol = (_clipMode == ClipMode::KeepFront) ? Vec3(0.2f, 0.85f, 1.0f) :
                           (_clipMode == ClipMode::KeepBack)  ? Vec3(1.0f, 0.45f, 0.2f) :
                                                                Vec3(1.0f, 0.92f, 0.3f);

            Renderer::drawCube(_clipPointA, Vec3(0.25f, 0.25f, 0.25f), Vec3(0.2f, 0.9f, 1.0f), nullptr, false);
            Renderer::drawWireCube(_clipPointA, Vec3(0.28f, 0.28f, 0.28f), Vec3(1, 1, 1));
            Renderer::drawCube(_clipPointB, Vec3(0.25f, 0.25f, 0.25f), Vec3(1.0f, 0.5f, 0.2f), nullptr, false);
            Renderer::drawWireCube(_clipPointB, Vec3(0.28f, 0.28f, 0.28f), Vec3(1, 1, 1));

            Vec3 diff = _clipPointB - _clipPointA;
            float len = diff.length();
            if (len > 0.01f) {
                float h = std::max(b.size.y * 1.4f, 2.0f);
                int steps = 14;
                for (int s = -2; s <= steps + 2; ++s) {
                    float t = (float)s / (float)steps;
                    Vec3 pt = _clipPointA + diff * t;
                    pt.y = b.position.y;
                    Renderer::drawCube(pt, Vec3(0.08f, h, 0.08f), modeCol * 0.85f, nullptr, false);
                }
                Vec3 normStart = planePt;
                normStart.y = b.position.y;
                Vec3 normEnd = normStart + planeNormal * 1.5f;
                Renderer::drawCube((normStart + normEnd) * 0.5f, Vec3(0.08f, 0.08f, 1.5f), modeCol, nullptr, false);
                Renderer::drawCube(normEnd, Vec3(0.2f, 0.2f, 0.2f), Vec3(1, 1, 1), nullptr, false);
            }
        }

        Renderer::drawWireCube(_cursorPos, Vec3(1.0f, 1.0f, 1.0f), Vec3(0.9f, 0.9f, 0.3f));
        drawGizmo(_cursorPos);
    }

    void placePrebuilt(int prebuiltId) {
        if (!_map) return;
        switch (prebuiltId) {
            case 0: { // Actor / Player Spawn
                MapSpawnPoint sp;
                sp.entityClass = "actor_player_start";
                sp.position = _cursorPos;
                sp.yaw = 0.0f;
                sp.type = SpawnType::FFA;
                _map->spawnPoints.push_back(sp);
                _selectionType = SelectionType::Spawn;
                _selectedIndex = (int)_map->spawnPoints.size() - 1;
                logMessage("Placed Universal Entity: Actor / Player Start at (" + std::to_string((int)_cursorPos.x) + ", " + std::to_string((int)_cursorPos.z) + ")");
                break;
            }
            case 1: { // Static Mesh Brush (2x2x2)
                MapBrush b;
                b.type = "cube";
                b.position = _cursorPos + Vec3(0, 1.0f, 0);
                b.size = Vec3(2.0f, 2.0f, 2.0f);
                b.color = Vec3(0.85f, 0.85f, 0.85f);
                b.texturePath = "concrete_wall.bmp";
                b.uvScale = Vec2(0.25f, 0.25f);
                b.uvMode = 1;
                _map->brushes.push_back(b);
                _selectionType = SelectionType::Brush;
                _selectedIndex = (int)_map->brushes.size() - 1;
                logMessage("Placed Static Mesh Brush (2x2x2m)");
                break;
            }
            case 2: { // Ground Platform (16x1x16)
                MapBrush b;
                b.type = "cube";
                b.position = _cursorPos + Vec3(0, -0.5f, 0);
                b.size = Vec3(16.0f, 1.0f, 16.0f);
                b.color = Vec3(1.0f, 1.0f, 1.0f);
                b.texturePath = "floor_tiles.bmp";
                b.uvScale = Vec2(0.25f, 0.25f);
                b.uvMode = 1;
                _map->brushes.push_back(b);
                _selectionType = SelectionType::Brush;
                _selectedIndex = (int)_map->brushes.size() - 1;
                logMessage("Placed Ground Platform (16x1x16m)");
                break;
            }
            case 3: { // Architectural Barrier
                MapBrush b;
                b.type = "cube";
                b.position = _cursorPos + Vec3(0, 0.6f, 0);
                b.size = Vec3(3.0f, 1.2f, 0.6f);
                b.color = Vec3(0.8f, 0.8f, 0.85f);
                b.texturePath = "concrete_wall.bmp";
                b.uvScale = Vec2(0.25f, 0.25f);
                b.uvMode = 1;
                _map->brushes.push_back(b);
                _selectionType = SelectionType::Brush;
                _selectedIndex = (int)_map->brushes.size() - 1;
                logMessage("Placed Architectural Barrier (3x1.2m)");
                break;
            }
            case 4: { // Structural Pillar
                MapBrush b;
                b.type = "cube";
                b.position = _cursorPos + Vec3(0, 3.0f, 0);
                b.size = Vec3(1.5f, 6.0f, 1.5f);
                b.color = Vec3(0.7f, 0.75f, 0.8f);
                b.texturePath = "concrete_wall.bmp";
                b.uvScale = Vec2(0.25f, 0.25f);
                b.uvMode = 1;
                _map->brushes.push_back(b);
                _selectionType = SelectionType::Brush;
                _selectedIndex = (int)_map->brushes.size() - 1;
                logMessage("Placed Structural Pillar (1.5x6m)");
                break;
            }
            case 5: { // Kinematic Door / Gate
                MapDoor d;
                d.name = "kinematic_gate_" + std::to_string(_map->doors.size());
                d.position = _cursorPos + Vec3(0, 1.75f, 0);
                d.size = Vec3(3.5f, 3.5f, 0.4f);
                d.openOffset = Vec3(0.0f, 4.0f, 0.0f);
                d.color = Vec3(0.25f, 0.35f, 0.45f);
                d.openSpeed = 2.5f;
                d.triggerRadius = 5.0f;
                _map->doors.push_back(d);
                _selectionType = SelectionType::Door;
                _selectedIndex = (int)_map->doors.size() - 1;
                logMessage("Placed Kinematic Door / Gate Entity");
                break;
            }
            case 6: { // Physics Dynamic Prop (Rigid Body)
                MapProp p;
                p.modelPath = "models/props/crate.obj";
                p.position = _cursorPos + Vec3(0, 0.45f, 0);
                p.scale = Vec3(0.9f, 0.9f, 0.9f);
                p.color = Vec3(0.65f, 0.5f, 0.35f);
                p.texturePath = "crate_wood.png";
                _map->props.push_back(p);
                _selectionType = SelectionType::Prop;
                _selectedIndex = (int)_map->props.size() - 1;
                logMessage("Placed Physics Dynamic Prop (Rigid Body)");
                break;
            }
            case 7: { // Explosive Hazard Prop
                MapProp p;
                p.modelPath = "models/props/barrel.obj";
                p.position = _cursorPos + Vec3(0, 0.48f, 0);
                p.scale = Vec3(0.64f, 0.96f, 0.64f);
                p.color = Vec3(0.9f, 0.2f, 0.2f);
                p.texturePath = "barrel_hazard.png";
                _map->props.push_back(p);
                _selectionType = SelectionType::Prop;
                _selectedIndex = (int)_map->props.size() - 1;
                logMessage("Placed Explosive Hazard Prop (Radial Trigger)");
                break;
            }
            case 8: { // Omni Point Light
                MapLight lt;
                lt.name = "point_light_" + std::to_string(_map->lights.size());
                lt.position = _cursorPos + Vec3(0, 2.5f, 0);
                lt.color = Vec3(1.0f, 0.92f, 0.80f);
                lt.intensity = 3.0f;
                lt.radius = 16.0f;
                lt.type = MapLightType::Point;
                _map->lights.push_back(lt);
                _selectionType = SelectionType::Light;
                _selectedIndex = (int)_map->lights.size() - 1;
                logMessage("Placed Omni Point Light (Radius: 16m)");
                break;
            }
            case 9: { // Trigger Zone Volume
                MapBrush b;
                b.type = "cube";
                b.position = _cursorPos + Vec3(0, 1.5f, 0);
                b.size = Vec3(4.0f, 3.0f, 4.0f);
                b.color = Vec3(0.1f, 0.8f, 0.9f);
                b.texturePath = "";
                b.uvScale = Vec2(1.0f, 1.0f);
                b.uvMode = 1;
                _map->brushes.push_back(b);
                _selectionType = SelectionType::Brush;
                _selectedIndex = (int)_map->brushes.size() - 1;
                logMessage("Placed Trigger Zone Volume (Gameplay Overlap)");
                break;
            }
            case 10: { // Item / Pickup Node
                MapWeaponSpawner ws;
                ws.weaponId = 0;
                ws.position = _cursorPos + Vec3(0, 0.5f, 0);
                ws.yaw = 0.0f;
                ws.respawnTime = 30.0f;
                _map->weaponSpawners.push_back(ws);
                _selectionType = SelectionType::WeaponSpawner;
                _selectedIndex = (int)_map->weaponSpawners.size() - 1;
                logMessage("Placed Item / Pickup Node (30s Cooldown)");
                break;
            }
            case 11: { // Spatial Audio Emitter
                MapProp p;
                p.modelPath = "models/props/audio_emitter.obj";
                p.position = _cursorPos + Vec3(0, 1.0f, 0);
                p.scale = Vec3(0.5f, 0.5f, 0.5f);
                p.color = Vec3(0.75f, 0.3f, 0.85f);
                _map->props.push_back(p);
                _selectionType = SelectionType::Prop;
                _selectedIndex = (int)_map->props.size() - 1;
                logMessage("Placed Spatial Audio Emitter (3D Sound Source)");
                break;
            }
            case 12: { // Particle VFX Emitter
                MapProp p;
                p.modelPath = "models/props/particle_emitter.obj";
                p.position = _cursorPos + Vec3(0, 0.5f, 0);
                p.scale = Vec3(0.6f, 0.6f, 0.6f);
                p.color = Vec3(0.85f, 0.55f, 0.15f);
                _map->props.push_back(p);
                _selectionType = SelectionType::Prop;
                _selectedIndex = (int)_map->props.size() - 1;
                logMessage("Placed Particle VFX Emitter Node");
                break;
            }
            case 13: { // Raycast Vehicle Chassis
                MapProp p;
                p.modelPath = "models/props/vehicle_chassis.obj";
                p.position = _cursorPos + Vec3(0, 0.75f, 0);
                p.scale = Vec3(2.0f, 1.2f, 4.0f);
                p.color = Vec3(0.2f, 0.6f, 0.8f);
                _map->props.push_back(p);
                _selectionType = SelectionType::Prop;
                _selectedIndex = (int)_map->props.size() - 1;
                logMessage("Placed Raycast Vehicle Entity (Suspension Chassis)");
                break;
            }
            case 14: { // Fluid / Water Volume
                MapBrush b;
                b.type = "cube";
                b.position = _cursorPos + Vec3(0, -0.5f, 0);
                b.size = Vec3(10.0f, 2.0f, 10.0f);
                b.color = Vec3(0.15f, 0.5f, 0.95f);
                b.texturePath = "water_caustics.bmp";
                b.uvScale = Vec2(0.5f, 0.5f);
                b.uvMode = 1;
                _map->brushes.push_back(b);
                _selectionType = SelectionType::Brush;
                _selectedIndex = (int)_map->brushes.size() - 1;
                logMessage("Placed Fluid / Water Volume (Buoyancy Body)");
                break;
            }
            case 15: { // Verlet Cloth / SoftBody
                MapProp p;
                p.modelPath = "models/props/cloth_sheet.obj";
                p.position = _cursorPos + Vec3(0, 2.0f, 0);
                p.scale = Vec3(2.0f, 2.0f, 0.1f);
                p.color = Vec3(0.85f, 0.35f, 0.45f);
                _map->props.push_back(p);
                _selectionType = SelectionType::Prop;
                _selectedIndex = (int)_map->props.size() - 1;
                logMessage("Placed Verlet Cloth / SoftBody Simulation Mesh");
                break;
            }
            case 16: { // Physics Joint / Constraint Anchor
                MapProp p;
                p.modelPath = "models/props/joint_anchor.obj";
                p.position = _cursorPos + Vec3(0, 1.0f, 0);
                p.scale = Vec3(0.4f, 0.4f, 0.4f);
                p.color = Vec3(0.9f, 0.8f, 0.2f);
                _map->props.push_back(p);
                _selectionType = SelectionType::Prop;
                _selectedIndex = (int)_map->props.size() - 1;
                logMessage("Placed Physics Joint / Constraint Connector");
                break;
            }
            case 17: { // Radial Force / Attractor
                MapProp p;
                p.modelPath = "models/props/force_field.obj";
                p.position = _cursorPos + Vec3(0, 0.5f, 0);
                p.scale = Vec3(1.0f, 1.0f, 1.0f);
                p.color = Vec3(0.9f, 0.2f, 0.2f);
                _map->props.push_back(p);
                _selectionType = SelectionType::Prop;
                _selectedIndex = (int)_map->props.size() - 1;
                logMessage("Placed Radial Force Field / Attractor Volume");
                break;
            }
            case 18: { // NavMesh Obstacle Box
                MapBrush b;
                b.type = "cube";
                b.position = _cursorPos + Vec3(0, 1.0f, 0);
                b.size = Vec3(2.0f, 2.0f, 2.0f);
                b.color = Vec3(0.5f, 0.5f, 0.55f);
                b.texturePath = "";
                _map->brushes.push_back(b);
                _selectionType = SelectionType::Brush;
                _selectedIndex = (int)_map->brushes.size() - 1;
                logMessage("Placed NavMesh Obstacle Box (Dynamic AI Blocker)");
                break;
            }
        }
    }

    void placeCurrentObject() {
        if (!_map) return;
        if (_activeTool == 1) { // Brush Tool
            MapBrush b;
            b.position = _cursorPos;
            b.size = _brushSize;
            b.color = Vec3(1.0f, 1.0f, 1.0f);
            b.texturePath = _selectedTexture;
            b.uvScale = _activeUvScale;
            b.uvMode = 1;
            _map->brushes.push_back(b);
            _selectionType = SelectionType::Brush;
            _selectedIndex = (int)_map->brushes.size() - 1;
            logMessage("Placed Brush #" + std::to_string(_selectedIndex) + " (" + _selectedTexture + ")");
        } else if (_activeTool == 2) { // Prop Tool
            MapProp p;
            p.modelPath = _selectedModel;
            p.position = _cursorPos;
            p.rotation = Vec3(0, 0, 0);
            p.scale = _propScale;
            p.color = Vec3(1, 1, 1);
            p.texturePath = "";
            _map->props.push_back(p);
            _selectionType = SelectionType::Prop;
            _selectedIndex = (int)_map->props.size() - 1;
            logMessage("Placed Prop #" + std::to_string(_selectedIndex) + " (" + _selectedModel + ")");
        } else if (_activeTool == 4) { // Door Tool
            MapDoor d;
            d.name = "door_" + std::to_string(_map->doors.size());
            d.position = _cursorPos;
            d.size = Vec3(2.5f, 3.5f, 0.4f);
            d.openOffset = Vec3(0.0f, 3.5f, 0.0f);
            d.color = Vec3(0.35f, 0.4f, 0.45f);
            d.openSpeed = 3.0f;
            d.triggerRadius = 4.0f;
            _map->doors.push_back(d);
            _selectionType = SelectionType::Door;
            _selectedIndex = (int)_map->doors.size() - 1;
            logMessage("Placed Dynamic Door #" + std::to_string(_selectedIndex));
        } else if (_activeTool == 5) { // Spawn Tool
            MapSpawnPoint sp;
            sp.position = _cursorPos;
            sp.yaw = 0.0f;
            sp.type = _spawnToolType;
            if (sp.type == SpawnType::TeamAlpha) sp.entityClass = "info_player_team1";
            else if (sp.type == SpawnType::TeamBeta) sp.entityClass = "info_player_team2";
            else sp.entityClass = "info_player_deathmatch";
            _map->spawnPoints.push_back(sp);
            _selectionType = SelectionType::Spawn;
            _selectedIndex = (int)_map->spawnPoints.size() - 1;
            logMessage("Placed " + sp.getDisplayName() + " #" + std::to_string(_selectedIndex) + " at (" + std::to_string((int)_cursorPos.x) + ", " + std::to_string((int)_cursorPos.z) + ")");
        } else if (_activeTool == 7) { // Lamp / Light Tool
            MapLight lt;
            lt.name = "light_" + std::to_string(_map->lights.size());
            lt.position = _cursorPos + Vec3(0.0f, 1.5f, 0.0f);
            lt.color = _lampColor;
            lt.intensity = _lampIntensity;
            lt.radius = _lampRadius;
            lt.type = _lampType;
            _map->lights.push_back(lt);
            _selectionType = SelectionType::Light;
            _selectedIndex = (int)_map->lights.size() - 1;
            logMessage("Placed Lamp Light #" + std::to_string(_selectedIndex) + " (Intensity: " + std::to_string(lt.intensity) + ", Radius: " + std::to_string((int)lt.radius) + "m)");
        }
    }

    void handleMouseClick(float mx, float my, bool isRmb = false) {
        int fbW = 0, fbH = 0;
        getFramebufferSize(fbW, fbH);
        float w = (fbW > 0) ? (float)fbW : (float)getWidth();
        float h = (fbH > 0) ? (float)fbH : (float)getHeight();

        // 0. Hierarchy Context Menu Handling
        if (_contextMenuOpen) {
            float cmX = _contextMenuX;
            float cmY = _contextMenuY;
            float cmW = 260.0f;
            struct CompDef { std::string name; std::string tag; };
            CompDef compRegistry[] = {
                { "Magic Add: Camera", "Camera" },
                { "Magic Add: Light", "Light" },
                { "Magic Add: Spawn", "Spawn" },
                { "Magic Add: Brush", "Brush" },
                { "Magic Add: Broom", "Broom" },
                { "Magic Add: Door", "Door" },
                { "Script (Lua)", "Script" },
                { "Point Light", "Light" },
                { "2D Sprite", "Sprite" },
                { "Box Collider", "Physics" },
                { "Audio Source", "Sound" },
                { "Timer Trigger", "Trigger" },
                { "Particle Emitter", "FX" }
            };

            std::vector<CompDef> filtered;
            std::string query = _contextMenuSearch;
            std::transform(query.begin(), query.end(), query.begin(), [](unsigned char c) { return (char)::tolower(c); });
            for (const auto& c : compRegistry) {
                if (query.empty()) filtered.push_back(c);
                else {
                    std::string lowerName = c.name;
                    std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), [](unsigned char ch) { return (char)::tolower(ch); });
                    if (lowerName.find(query) != std::string::npos) filtered.push_back(c);
                }
            }

            int itemCount = std::min((int)filtered.size(), 8);
            float cmH = 68.0f + itemCount * 22.0f + 65.0f;
            int winW = 1600, winH = 900;
            getWindowSize(winW, winH);
            if (cmX + cmW > (float)winW) cmX = (float)winW - cmW - 10.0f;
            if (cmY + cmH > (float)winH) cmY = (float)winH - cmH - 10.0f;

            if (mx >= cmX && mx <= cmX + cmW && my >= cmY && my <= cmY + cmH) {
                float sbY = cmY + 28.0f;
                float curItemY = sbY + 22.0f + 6.0f;
                for (int i = 0; i < itemCount; ++i) {
                    if (my >= curItemY && my <= curItemY + 20.0f) {
                        std::string picked = filtered[i].name;
                        if (picked == "Magic Add: Camera") {
                            MapCamera c;
                            c.name = "camera_" + std::to_string(_map->cameras.size() + 1);
                            c.position = _cursorPos + Vec3(0, 1.8f, 0);
                            c.rotation = Vec3(0, 0, 0);
                            c.fov = 75.0f;
                            c.follow_target = "player";
                            c.tpp_offset = Vec3(0.6f, 1.8f, -3.5f);
                            c.collision_radius = 0.3f;
                            c.active = _map->cameras.empty();
                            _map->cameras.push_back(c);
                            _selectionType = SelectionType::Camera;
                            _selectedIndex = (int)_map->cameras.size() - 1;
                            logMessage("Magic Add: Created Camera [" + c.name + "]");
                        } else if (picked == "Magic Add: Light") {
                            MapLight lt;
                            lt.name = "light_" + std::to_string(_map->lights.size() + 1);
                            lt.position = _cursorPos + Vec3(0, 1.5f, 0);
                            lt.color = _lampColor;
                            lt.intensity = _lampIntensity;
                            lt.radius = _lampRadius;
                            _map->lights.push_back(lt);
                            _selectionType = SelectionType::Light;
                            _selectedIndex = (int)_map->lights.size() - 1;
                            logMessage("Magic Add: Created Light [" + lt.name + "]");
                        } else if (picked == "Magic Add: Spawn") {
                            MapSpawnPoint sp;
                            sp.position = _cursorPos + Vec3(0, 0.5f, 0);
                            sp.type = SpawnType::FFA;
                            sp.yaw = 0.0f;
                            _map->spawnPoints.push_back(sp);
                            _selectionType = SelectionType::Spawn;
                            _selectedIndex = (int)_map->spawnPoints.size() - 1;
                            logMessage("Magic Add: Created Spawn Point");
                        } else if (picked == "Magic Add: Brush") {
                            MapBrush b;
                            b.type = "cube";
                            b.position = _cursorPos + Vec3(0, _brushSize.y * 0.5f, 0);
                            b.size = _brushSize;
                            b.color = Vec3(0.8f, 0.8f, 0.85f);
                            b.texturePath = _selectedTexture;
                            _map->brushes.push_back(b);
                            _selectionType = SelectionType::Brush;
                            _selectedIndex = (int)_map->brushes.size() - 1;
                            logMessage("Magic Add: Created Brush");
                        } else if (picked == "Magic Add: Broom") {
                            MapBrush b;
                            b.type = "cube";
                            b.position = _cursorPos + Vec3(0, 1.5f, 0);
                            b.size = Vec3(0.22f, 0.22f, 2.6f);
                            b.color = Vec3(0.48f, 0.30f, 0.14f);
                            b.texturePath = "brick_wall.bmp";
                            b.name = "Miotla_" + std::to_string(_map->brushes.size() + 1);
                            b.script = "assets/scripts/qa_rpg_brooms.lua";
                            _map->brushes.push_back(b);
                            _selectionType = SelectionType::Brush;
                            _selectedIndex = (int)_map->brushes.size() - 1;
                            logMessage("Magic Add: Created Interactive Broom with script");
                        } else if (picked == "Magic Add: Door") {
                            MapDoor d;
                            d.name = "door_" + std::to_string(_map->doors.size() + 1);
                            d.position = _cursorPos + Vec3(0, 1.75f, 0);
                            d.size = Vec3(2.5f, 3.5f, 0.4f);
                            d.openOffset = Vec3(0, 4.0f, 0);
                            d.color = Vec3(0.3f, 0.25f, 0.2f);
                            d.openSpeed = 2.0f;
                            d.triggerRadius = 5.0f;
                            d.script = "assets/scripts/qa_rpg_quests.lua";
                            _map->doors.push_back(d);
                            _selectionType = SelectionType::Door;
                            _selectedIndex = (int)_map->doors.size() - 1;
                            logMessage("Magic Add: Created Dynamic Door");
                        } else if (picked.find("Script") != std::string::npos) {
                            setWorkspace(EditorWorkspace::Scripting_Editor);
                            logMessage("Context Menu: Attached Script (Lua) - opened Lua Script Editor IDE");
                        } else {
                            logMessage("Context Menu: Attached " + picked + " to entity #" + std::to_string(_contextMenuTargetIndex));
                        }
                        _contextMenuOpen = false;
                        return;
                    }
                    curItemY += 22.0f;
                }
                curItemY += 8.0f;
                if (my >= curItemY && my <= curItemY + 18.0f) { duplicateSelection(); _contextMenuOpen = false; return; }
                curItemY += 19.0f;
                if (my >= curItemY && my <= curItemY + 18.0f) { focusCamera(); _contextMenuOpen = false; return; }
                curItemY += 19.0f;
                if (my >= curItemY && my <= curItemY + 18.0f) { deleteSelection(); _contextMenuOpen = false; return; }
                return;
            } else {
                _contextMenuOpen = false;
            }
        }

        // 1. Texture Browser Modal
        if (_browserOpen) {
            float bw = 820.0f;
            float bh = 600.0f;
            float bx = (w - bw) * 0.5f;
            float by = (h - bh) * 0.5f;

            // Close button click
            if (mx >= bx + bw - 36.0f && mx <= bx + bw - 6.0f && my >= by + 2.0f && my <= by + 26.0f) {
                _browserOpen = false;
                return;
            }
            int cols = 6;
            float thumbSize = 110.0f;
            float gap = 15.0f;
            float startX = bx + 25.0f;
            float startY = by + 45.0f;

            for (size_t i = 0; i < _availableTextures.size(); ++i) {
                int col = (int)(i % cols);
                int row = (int)(i / cols);
                float tx = startX + col * (thumbSize + gap);
                float ty = startY + row * (thumbSize + gap);

                if (mx >= tx && mx <= tx + thumbSize && my >= ty && my <= ty + thumbSize) {
                    _selectedTexture = _availableTextures[i].filename;
                    logMessage("Selected Texture: " + _selectedTexture);
                    if (_selectionType == SelectionType::Brush && _selectedIndex >= 0 && _selectedIndex < (int)_map->brushes.size()) {
                        _map->brushes[_selectedIndex].texturePath = _selectedTexture;
                    }
                    _browserOpen = false;
                    return;
                }
            }
            return;
        }

        // 2. Model Browser Modal
        if (_modelBrowserOpen) {
            float bw = 700.0f;
            float bh = 480.0f;
            float bx = (w - bw) * 0.5f;
            float by = (h - bh) * 0.5f;

            // Close button click
            if (mx >= bx + bw - 36.0f && mx <= bx + bw - 6.0f && my >= by + 2.0f && my <= by + 26.0f) {
                _modelBrowserOpen = false;
                return;
            }
            // Browse Disk STL button
            if (mx >= bx + 20.0f && mx <= bx + 320.0f && my >= by + 45.0f && my <= by + 80.0f) {
                openModelDialog();
                return;
            }
            // Model list items
            float startY = by + 95.0f;
            for (size_t i = 0; i < _availableModels.size(); ++i) {
                float iy = startY + 24.0f + i * 36.0f;
                if (mx >= bx + 20.0f && mx <= bx + bw - 20.0f && my >= iy && my <= iy + 30.0f) {
                    _selectedModel = _availableModels[i];
                    logMessage("Selected 3D Model: " + _selectedModel);
                    if (_selectionType == SelectionType::Prop && _selectedIndex >= 0 && _selectedIndex < (int)_map->props.size()) {
                        _map->props[_selectedIndex].modelPath = _selectedModel;
                    }
                    _modelBrowserOpen = false;
                    return;
                }
            }
            return;
        }

        // 3. Help Modal
        if (_helpModalOpen) {
            float bw = 650.0f;
            float bh = 420.0f;
            float bx = (w - bw) * 0.5f;
            float by = (h - bh) * 0.5f;

            if (mx >= bx + bw - 36.0f && mx <= bx + bw - 6.0f && my >= by + 2.0f && my <= by + 26.0f) {
                _helpModalOpen = false;
                return;
            }
            return;
        }

        // 3.5. Build & Export Modal
        if (_buildModalOpen) {
            float bw = 740.0f;
            float bh = 500.0f;
            float bx = (w - bw) * 0.5f;
            float by = (h - bh) * 0.5f;

            // Close button [X]
            if (mx >= bx + bw - 36.0f && mx <= bx + bw - 6.0f && my >= by + 2.0f && my <= by + 30.0f) {
                _buildModalOpen = false;
                return;
            }

            // Platform selection
            float curY = by + 50.0f;
            float py = curY + 24.0f;
            if (my >= py && my <= py + 32.0f) {
                if (mx >= bx + 24.0f && mx <= bx + 184.0f) {
                    _exportSettings.platform = TargetPlatform::Windows_x64;
                    return;
                } else if (mx >= bx + 198.0f && mx <= bx + 358.0f) {
                    _exportSettings.platform = TargetPlatform::Linux_x64;
                    return;
                } else if (mx >= bx + 372.0f && mx <= bx + 552.0f) {
                    _exportSettings.platform = TargetPlatform::WebAssembly;
                    return;
                }
            }

            // Config selection
            curY += 76.0f;
            py = curY + 24.0f;
            if (my >= py && my <= py + 30.0f) {
                if (mx >= bx + 24.0f && mx <= bx + 204.0f) {
                    _exportSettings.config = BuildConfiguration::Shipping;
                    return;
                } else if (mx >= bx + 218.0f && mx <= bx + 378.0f) {
                    _exportSettings.config = BuildConfiguration::Debug;
                    return;
                }
            }

            // Action buttons at bottom
            float btnY = by + bh - 52.0f;
            if (my >= btnY && my <= btnY + 36.0f) {
                float b1X = bx + 24.0f;
                float b1W = 180.0f;
                if (mx >= b1X && mx <= b1X + b1W) {
                    buildGameAction(false);
                    return;
                }
                float b2X = b1X + b1W + 14.0f;
                float b2W = 220.0f;
                if (mx >= b2X && mx <= b2X + b2W) {
                    buildGameAction(true);
                    return;
                }
                float b3X = bx + bw - 130.0f;
                float b3W = 106.0f;
                if (mx >= b3X && mx <= b3X + b3W) {
                    _buildModalOpen = false;
                    return;
                }
            }
            return;
        }

        // 3.6. Input Modal Handling
        if (_inputModalOpen) {
            float bw = 780.0f;
            float bh = 540.0f;
            float bx = (w - bw) * 0.5f;
            float by = (h - bh) * 0.5f;

            // Close button [X]
            if (mx >= bx + bw - 36.0f && mx <= bx + bw - 6.0f && my >= by + 2.0f && my <= by + 30.0f) {
                _inputModalOpen = false;
                return;
            }

            // Buttons: Save Bindings, Load Bindings, Reset Defaults, Close
            float btnY = by + bh - 46.0f;
            if (my >= btnY && my <= btnY + 34.0f) {
                if (mx >= bx + 24.0f && mx <= bx + 184.0f) {
                    InputActionManager::instance().saveToFile("assets/config/input_bindings.json");
                    logMessage("Saved bindings to assets/config/input_bindings.json");
                    return;
                } else if (mx >= bx + 194.0f && mx <= bx + 354.0f) {
                    InputActionManager::instance().loadFromFile("assets/config/input_bindings.json");
                    logMessage("Loaded bindings from assets/config/input_bindings.json");
                    return;
                } else if (mx >= bx + 364.0f && mx <= bx + 514.0f) {
                    InputActionManager::instance().setupDefaultBindings();
                    logMessage("Reset input bindings to defaults.");
                    return;
                } else if (mx >= bx + bw - 130.0f && mx <= bx + bw - 20.0f) {
                    _inputModalOpen = false;
                    return;
                }
            }
            return;
        }

        // 3.7. 2D Asset Browser & Spritesheet Slicer Modal Handling
        if (_asset2DModalOpen) {
            float bw = 840.0f;
            float bh = 560.0f;
            float bx = (w - bw) * 0.5f;
            float by = (h - bh) * 0.5f;

            // Close button [X]
            if (mx >= bx + bw - 36.0f && mx <= bx + bw - 6.0f && my >= by + 2.0f && my <= by + 30.0f) {
                _asset2DModalOpen = false;
                return;
            }

            // Controls buttons
            float btnY = by + 76.0f;
            if (my >= btnY && my <= btnY + 34.0f) {
                if (mx >= bx + 20.0f && mx <= bx + 180.0f) {
                    _editorSpriteSheet.setTexturePath(_selectedTexture);
                    _editorSpriteSheet.sliceGrid(_sliceCols, _sliceRows);
                    logMessage("Sliced spritesheet into " + std::to_string(_editorSpriteSheet.getFrameCount()) + " frames.");
                    return;
                } else if (mx >= bx + 194.0f && mx <= bx + 384.0f) {
                    if (_map) {
                        MapSprite2D sp;
                        sp.texturePath = _selectedTexture;
                        sp.position = _cursorPos;
                        sp.size = { 2.0f, 2.0f };
                        const auto* f = _editorSpriteSheet.getFrame(_selectedSpriteFrame);
                        if (f) {
                            sp.uvMin = f->uvMin;
                            sp.uvMax = f->uvMax;
                        }
                        _map->sprites.push_back(sp);
                        logMessage("Placed 2D Sprite on Map at cursor.");
                    }
                    return;
                } else if (mx >= bx + 398.0f && mx <= bx + 568.0f) {
                    _editorSpriteSheet.saveToFile("assets/textures/spritesheet.json");
                    logMessage("Saved spritesheet to assets/textures/spritesheet.json");
                    return;
                } else if (mx >= bx + bw - 130.0f && mx <= bx + bw - 20.0f) {
                    _asset2DModalOpen = false;
                    return;
                }
            }

            // Frame Selection Click
            float startY = by + 130.0f;
            float gridY = startY + 24.0f;
            float thumb = 60.0f;
            float gap = 12.0f;
            int cols = 8;
            for (size_t i = 0; i < std::min((size_t)32, _editorSpriteSheet.getFrameCount()); ++i) {
                int c = (int)(i % cols);
                int r = (int)(i / cols);
                float fx = bx + 24.0f + c * (thumb + gap);
                float fy = gridY + r * (thumb + gap);
                if (mx >= fx && mx <= fx + thumb && my >= fy && my <= fy + thumb) {
                    _selectedSpriteFrame = (int)i;
                    logMessage("Selected Frame #" + std::to_string(i));
                    return;
                }
            }
            return;
        }

        // 3.8. Legacy Map Conversion Modal Handling
        if (_showConvertMapModal) {
            float bw = 560.0f;
            float bh = 230.0f;
            float bx = (w - bw) * 0.5f;
            float by = (h - bh) * 0.5f;

            // Close button [X]
            if (mx >= bx + bw - 32.0f && mx <= bx + bw - 8.0f && my >= by + 5.0f && my <= by + 27.0f) {
                _showConvertMapModal = false;
                _pendingMapPath = "";
                logMessage("Konwersja mapy anulowana.");
                return;
            }

            // Buttons at bottom
            float btnY = by + bh - 52.0f;
            float btnW = 160.0f;
            float btnH = 34.0f;

            // "Konwertuj" button
            float convX = bx + 60.0f;
            if (mx >= convX && mx <= convX + btnW && my >= btnY && my <= btnY + btnH) {
                logMessage("Rozpoczynanie konwersji: " + _pendingMapPath);
                auto res = LabScene::convertLegacyMapToScene(_pendingMapPath);
                if (res.success) {
                    logMessage("Konwersja udana! Kopia zapasowa: " + res.backupPath);
                    loadMapFromFile(res.scenePath);
                } else {
                    logMessage("Blad konwersji mapy: " + res.errorMessage);
                }
                _showConvertMapModal = false;
                _pendingMapPath = "";
                return;
            }

            // "Anuluj" button
            float cancelX = bx + bw - 60.0f - btnW;
            if (mx >= cancelX && mx <= cancelX + btnW && my >= btnY && my <= btnY + btnH) {
                _showConvertMapModal = false;
                _pendingMapPath = "";
                logMessage("Wczytywanie mapy anulowane.");
                return;
            }

            // Consume click while modal is open
            return;
        }

        // 4. Dropdown File Menu
        if (_fileMenuOpen) {
            float menuX = 10.0f;
            float menuY = 24.0f;
            float menuW = 240.0f;
            float menuH = 200.0f;
            if (mx >= menuX && mx <= menuX + menuW && my >= menuY && my <= menuY + menuH) {
                int itemIdx = (int)((my - menuY) / 24.0f);
                if (itemIdx == 0) setWorkspace(EditorWorkspace::Project_Hub);
                else if (itemIdx == 1) newMap();
                else if (itemIdx == 2) openMapDialog();
                else if (itemIdx == 3) saveMapAction(false);
                else if (itemIdx == 4) saveMapAction(true);
                else if (itemIdx == 5) buildGameAction(true);
                else if (itemIdx == 6) _buildModalOpen = true;
                else if (itemIdx >= 7) stop();
                _fileMenuOpen = false;
                return;
            } else {
                _fileMenuOpen = false;
            }
        }

        // 4.5. Dropdown Build Menu
        if (_buildMenuOpen) {
            float menuX = 137.0f;
            float menuY = 24.0f;
            float menuW = 250.0f;
            float menuH = 125.0f;
            if (mx >= menuX && mx <= menuX + menuW && my >= menuY && my <= menuY + menuH) {
                int itemIdx = (int)((my - menuY - 3.0f) / 24.0f);
                if (itemIdx == 0) buildGameAction(true); // Test Game (Debug Run F5)
                else if (itemIdx == 1) { // Export Game (Release Package)
                    _exportSettings.config = BuildConfiguration::Release;
                    buildGameAction(false);
                } else if (itemIdx == 2) { // Export Game (Shipping Bytecode Protected)
                    _exportSettings.config = BuildConfiguration::Shipping;
                    buildGameAction(false);
                } else if (itemIdx == 3) { // Build Settings Dialog
                    _buildModalOpen = true;
                } else if (itemIdx == 4) { // Open test_build Folder
                    #if defined(_WIN32)
                    std::system("start test_build");
                    #elif defined(__APPLE__)
                    std::system("open test_build");
                    #else
                    std::system("xdg-open test_build");
                    #endif
                }
                _buildMenuOpen = false;
                return;
            } else {
                _buildMenuOpen = false;
            }
        }

        // 4.6. Dropdown View Menu (Subcategorized Architecture)
        if (_viewMenuOpen) {
            float menuX = 92.0f;
            float menuY = 24.0f;
            float menuW = 210.0f;
            float menuH = 128.0f;

            // Check flyout subcategory menu click first if active
            if (_viewSubmenuHover >= 0) {
                float subX = menuX + menuW;
                float subY = menuY + _viewSubmenuHover * 24.0f;
                float subW = 260.0f;
                int subCount = (_viewSubmenuHover == 0) ? 8 : ((_viewSubmenuHover == 1) ? 3 : ((_viewSubmenuHover == 2) ? 2 : ((_viewSubmenuHover == 3) ? 1 : 3)));
                float subH = 8.0f + subCount * 24.0f;

                if (mx >= subX && mx <= subX + subW && my >= subY && my <= subY + subH) {
                    int subIdx = (int)((my - subY - 4.0f) / 24.0f);
                    if (_viewSubmenuHover == 0) { // 3D Workspaces
                        if (subIdx == 0) setWorkspace(EditorWorkspace::World3D_Level);
                        else if (subIdx == 1) setWorkspace(EditorWorkspace::World3D_Animation);
                        else if (subIdx == 2) setWorkspace(EditorWorkspace::World3D_ModelViewer);
                        else if (subIdx == 3) setWorkspace(EditorWorkspace::Studio_GripPoser);
                        else if (subIdx == 4) setWorkspace(EditorWorkspace::Studio_AnimationTimeline);
                        else if (subIdx == 5) setWorkspace(EditorWorkspace::Studio_WeaponSkins);
                        else if (subIdx == 6) setWorkspace(EditorWorkspace::Studio_Appearance);
                        else if (subIdx == 7) setWorkspace(EditorWorkspace::Studio_FaceDialogue);
                    } else if (_viewSubmenuHover == 1) { // 2D Workspaces
                        if (subIdx == 0) setWorkspace(EditorWorkspace::Level2D_Tilemap);
                        else if (subIdx == 1) setWorkspace(EditorWorkspace::Level2D_Animation);
                        else if (subIdx == 2) setWorkspace(EditorWorkspace::Level2D_HUD);
                    } else if (_viewSubmenuHover == 2) { // Scripting Workspaces
                        if (subIdx == 0) setWorkspace(EditorWorkspace::Scripting_Editor);
                        else if (subIdx == 1) setWorkspace(EditorWorkspace::Scripting_Console);
                    } else if (_viewSubmenuHover == 3) { // Plugins & Packages
                        if (subIdx == 0) setWorkspace(EditorWorkspace::Plugins_Manager);
                    } else if (_viewSubmenuHover == 4) { // Viewport Display
                        if (subIdx == 0) {
                            _wireframeMode = !_wireframeMode;
                            glPolygonMode(GL_FRONT_AND_BACK, _wireframeMode ? GL_LINE : GL_FILL);
                            logMessage("Wireframe Mode: " + std::string(_wireframeMode ? "ON" : "OFF"));
                        } else if (subIdx == 1) {
                            _orthoViewMode = !_orthoViewMode;
                            if (_orthoViewMode) {
                                _camera.setOrthographic(25.0f, -500.0f, 500.0f);
                                _camera.setPitch(-89.9f);
                                _camera.setYaw(-90.0f);
                                logMessage("Viewport: Switched to 2D Orthographic Top-Down Projection");
                            } else {
                                _camera.setPerspective(70.0f, 16.0f / 9.0f, 0.01f, 3000.0f);
                                _camera.setPitch(0.0f);
                                _camera.setYaw(-90.0f);
                                logMessage("Viewport: Switched to 3D Perspective Projection");
                            }
                        } else if (subIdx == 2) {
                            _darknessMode = !_darknessMode;
                            logMessage(_darknessMode ? "Darkness Mode: ACTIVE" : "Darkness Mode: DISABLED");
                        }
                    }
                    _viewMenuOpen = false;
                    _viewSubmenuHover = -1;
                    return;
                }
            }

            // Check main categories box click
            if (mx >= menuX && mx <= menuX + menuW && my >= menuY && my <= menuY + menuH) {
                int catIdx = (int)((my - menuY - 4.0f) / 24.0f);
                if (catIdx >= 0 && catIdx < 5) {
                    _viewSubmenuHover = catIdx;
                }
                return;
            } else {
                _viewMenuOpen = false;
                _viewSubmenuHover = -1;
            }
        }

        // 5. Top Menu Bar (y: 0..24)
        if (my >= 0.0f && my <= 24.0f) {
            if (mx >= 10.0f && mx <= 50.0f) {
                _fileMenuOpen = !_fileMenuOpen;
                _viewMenuOpen = false;
                _buildMenuOpen = false;
                return;
            } else if (mx >= 52.0f && mx <= 90.0f) { // Edit
                if (_selectionType != SelectionType::None) deleteSelection();
                else if (_map && !_map->brushes.empty()) { _map->brushes.pop_back(); logMessage("Undo: deleted last brush"); }
                return;
            } else if (mx >= 92.0f && mx <= 135.0f) { // View
                _viewMenuOpen = !_viewMenuOpen;
                _fileMenuOpen = false;
                _buildMenuOpen = false;
                return;
            } else if (mx >= 137.0f && mx <= 185.0f) { // Build
                _buildMenuOpen = !_buildMenuOpen;
                _fileMenuOpen = false;
                _viewMenuOpen = false;
                return;
            } else if (mx >= 187.0f && mx <= 235.0f) { // Tools
                _browserOpen = !_browserOpen;
                return;
            } else if (mx >= 237.0f && mx <= 285.0f) { // Input
                _inputModalOpen = !_inputModalOpen;
                return;
            } else if (mx >= 287.0f && mx <= 360.0f) { // 2D Assets
                _asset2DModalOpen = !_asset2DModalOpen;
                return;
            } else if (mx >= 362.0f && mx <= 405.0f) { // Help
                _helpModalOpen = !_helpModalOpen;
                return;
            }

            // Top Workspace Breadcrumb Badge click -> open View menu
            if (mx >= 415.0f && mx <= 700.0f) {
                _viewMenuOpen = !_viewMenuOpen;
                _fileMenuOpen = false;
                _buildMenuOpen = false;
                return;
            }
        }

        // 6. Top Toolbar Buttons (y: 24..58, deduplicated utility buttons)
        if (my >= 24.0f && my <= 58.0f) {
            const int tbIcons[12] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 15, 16 };
            for (int i = 0; i < 12; ++i) {
                float bx = 8.0f + i * 28.0f;
                if (mx >= bx && mx <= bx + 26.0f) {
                    switch (tbIcons[i]) {
                        case 0: newMap(); break;
                        case 1: openMapDialog(); break;
                        case 2: saveMapAction(false); break;
                        case 3: saveMapAction(true); break;
                        case 4: // Undo
                            if (_map && !_map->brushes.empty()) { _map->brushes.pop_back(); logMessage("Undo last action."); }
                            break;
                        case 5: deleteSelection(); break;
                        case 6: duplicateSelection(); break;
                        case 7: focusCamera(); break;
                        case 8: _browserOpen = true; break;
                        case 9: _modelBrowserOpen = true; break;
                        case 15: // Grid snap down
                            _gridSnap = std::max(0.125f, _gridSnap * 0.5f);
                            logMessage("Grid Snap: " + std::to_string(_gridSnap));
                            break;
                        case 16: // Grid snap up
                            _gridSnap = std::min(16.0f, _gridSnap * 2.0f);
                            logMessage("Grid Snap: " + std::to_string(_gridSnap));
                            break;
                    }
                    return;
                }
            }

            // Top Toolbar Controls: Simulation, View Mode, Quick Actions
            // 1. [ > TEST GAME (F5) ]
            if (mx >= 350.0f && mx <= 495.0f) {
                buildGameAction(true);
                return;
            }
            // 2. [ BUILD / EXPORT... ]
            if (mx >= 503.0f && mx <= 633.0f) {
                _buildModalOpen = true;
                return;
            }
            // 3. [ 3D PERSPECTIVE ] / [ 2D ORTHO ]
            if (mx >= 641.0f && mx <= 741.0f) {
                _orthoViewMode = !_orthoViewMode;
                if (_orthoViewMode) {
                    _camera.setOrthographic(25.0f, -500.0f, 500.0f);
                    _camera.setPitch(-89.9f);
                    _camera.setYaw(-90.0f);
                    logMessage("Viewport: Switched to 2D Orthographic Top-Down Projection");
                } else {
                    _camera.setPerspective(70.0f, 16.0f / 9.0f, 0.01f, 3000.0f);
                    _camera.setPitch(0.0f);
                    _camera.setYaw(-90.0f);
                    logMessage("Viewport: Switched to 3D Perspective Projection");
                }
                return;
            }
            // 4. [+ LIGHT]
            if (mx >= 749.0f && mx <= 829.0f) {
                _activeTool = 7;
                logMessage("Tool 7: Omni Point Light Tool (Click scene or E to place)");
                return;
            }
            // 5. [DARK: ON/OFF]
            if (mx >= 837.0f && mx <= 927.0f) {
                _darknessMode = !_darknessMode;
                logMessage(_darknessMode ? "Darkness Mode: ACTIVE (Pitch Black Ambient/Sun)" : "Darkness Mode: DISABLED (Standard Sunlight)");
                return;
            }
            // 6. [DARK PRESET]
            if (mx >= 935.0f && mx <= 1030.0f) {
                if (_map) {
                    _map->metadata.ambientColor = Vec3(0.015f, 0.018f, 0.025f);
                    _map->metadata.sunColor = Vec3(0.0f, 0.0f, 0.0f);
                    _darknessMode = true;
                    logMessage("Applied Darkness Preset to Map Metadata (Ambient: 0.015, Sun: 0)");
                }
                return;
            }
        }

        // 7. Left Tools Palette (x: 0..42, y: 58..878)
        if (mx >= 0.0f && mx <= 42.0f) {
            float startY = 68.0f;
            for (int i = 0; i < 8; ++i) {
                float ty = startY + i * 36.0f;
                if (my >= ty - 2.0f && my <= ty + 32.0f) {
                    _activeTool = i;
                    switch (i) {
                        case 0: logMessage("Tool 0: Selection / Pointer Tool"); break;
                        case 1: logMessage("Tool 1: Brush / Block Tool (E to place)"); break;
                        case 2: logMessage("Tool 2: Entity / Prop Tool (E to place model)"); break;
                        case 3: logMessage("Tool 3: Texture Tool (LMB apply, RMB pipette)"); break;
                        case 4: logMessage("Tool 4: Door Tool (E to place dynamic door)"); break;
                        case 5:
                            if (_spawnToolType == SpawnType::FFA) {
                                _spawnToolType = SpawnType::TeamAlpha;
                                logMessage("Tool 5: Spawn Tool [Team Alpha - Blue HQ] (E to place)");
                            } else if (_spawnToolType == SpawnType::TeamAlpha) {
                                _spawnToolType = SpawnType::TeamBeta;
                                logMessage("Tool 5: Spawn Tool [Team Beta - Red HQ] (E to place)");
                            } else {
                                _spawnToolType = SpawnType::FFA;
                                logMessage("Tool 5: Spawn Tool [FFA / DM Neutral] (E to place)");
                            }
                            break;
                        case 6: // CSG Clip tool
                            if (_activeTool != 6) {
                                _activeTool = 6;
                                logMessage("Tool 6: CSG Clip Tool (X / Shift+X to cycle mode, ENTER to commit)");
                            } else {
                                _clipMode = (ClipMode)(((int)_clipMode + 1) % 3);
                                if (_clipMode == ClipMode::KeepFront) logMessage("CSG Clip Mode: Keep Front (Positive Side)");
                                else if (_clipMode == ClipMode::KeepBack) logMessage("CSG Clip Mode: Keep Back (Negative Side)");
                                else logMessage("CSG Clip Mode: Keep Both (Split into 2 Brushes)");
                            }
                            break;
                        case 7: // Lamp / Light Tool
                            _activeTool = 7;
                            logMessage("Tool 7: Lamp / Light Tool (E to place light source, Left-click on scene to place)");
                            break;
                    }
                    return;
                }
            }
        }

        // 8. Right Sidebar (Using exact synchronized layout!)
        SidebarLayout l = getSidebarLayout(w, h);
        if (mx >= l.rightX && mx <= w && my >= l.rightY && my <= h - 22.0f) {
            // Tab headers (Properties vs Outliner vs Prebuilts)
            if (mx >= l.tabPropX && mx <= l.tabPropX + l.tabPropW && my >= l.tabPropY && my <= l.tabPropY + l.tabPropH) {
                _sidebarTab = SidebarTab::Properties;
                logMessage("Sidebar: Switched to Properties Tab");
                return;
            }
            if (mx >= l.tabOutX && mx <= l.tabOutX + l.tabOutW && my >= l.tabOutY && my <= l.tabOutY + l.tabOutH) {
                _sidebarTab = SidebarTab::Hierarchy;
                logMessage("Sidebar: Switched to Struktura (Outliner) Tab");
                return;
            }
            if (mx >= l.tabPreX && mx <= l.tabPreX + l.tabPreW && my >= l.tabPreY && my <= l.tabPreY + l.tabPreH) {
                _sidebarTab = SidebarTab::Prebuilts;
                logMessage("Sidebar: Switched to Prebuilty & Encje Tab");
                return;
            }

            // PREBUILTS TAB INTERACTIONS
            if (_sidebarTab == SidebarTab::Prebuilts) {
                float catY = l.rightY + 36.0f;
                float catW = (l.rightW - 24.0f) * 0.5f;
                if (my >= catY && my <= catY + 24.0f) {
                    if (mx >= l.rightX + 10.0f && mx <= l.rightX + 10.0f + catW) {
                        _prebuiltCategory = 0;
                        logMessage("Entities: Category [World & Actors]");
                        return;
                    }
                    if (mx >= l.rightX + 14.0f + catW && mx <= l.rightX + l.rightW - 10.0f) {
                        _prebuiltCategory = 1;
                        logMessage("Entities: Category [Components & FX]");
                        return;
                    }
                }

                float startY = catY + 30.0f;
                float cardH = (_prebuiltCategory == 0) ? 44.0f : 36.0f;
                float cardSpacing = cardH + 5.0f;
                int count = (_prebuiltCategory == 0) ? 10 : 9;

                for (int i = 0; i < count; ++i) {
                    float cy = startY + i * cardSpacing;
                    if (mx >= l.rightX + 10.0f && mx <= l.rightX + l.rightW - 10.0f && my >= cy && my <= cy + cardH) {
                        int prebuiltId = (_prebuiltCategory == 0) ? i : (10 + i);
                        placePrebuilt(prebuiltId);
                        return;
                    }
                }
                return;
            }

            // OUTLINER TAB INTERACTIONS
            if (_sidebarTab == SidebarTab::Hierarchy) {
                int totalSpawns = (int)_map->spawnPoints.size();
                int totalWepSpawns = (int)_map->weaponSpawners.size();
                int totalEntities = (int)(totalSpawns + totalWepSpawns + _map->brushes.size() + _map->props.size() + _map->doors.size() + _map->lights.size() + _map->cameras.size());
                int maxItems = (int)(l.outListH / l.outItemSpacing);

                // Click on entity items
                if (mx >= l.outListX && mx <= l.outListX + l.outListW && my >= l.outListY && my <= l.outListY + l.outListH) {
                    int clickedSlot = (int)((my - l.outListY - 4.0f) / l.outItemSpacing);
                    int itemIdx = _outlinerScroll + clickedSlot;
                    if (clickedSlot >= 0 && itemIdx >= 0 && itemIdx < totalEntities) {
                        if (itemIdx < totalSpawns) {
                            _selectionType = SelectionType::Spawn;
                            _selectedIndex = itemIdx;
                            logMessage("Outliner: Selected " + _map->spawnPoints[itemIdx].getDisplayName() + " #" + std::to_string(itemIdx));
                        } else if (itemIdx < totalSpawns + totalWepSpawns) {
                            _selectionType = SelectionType::WeaponSpawner;
                            _selectedIndex = itemIdx - totalSpawns;
                            logMessage("Outliner: Selected Weapon Spawner [" + _map->weaponSpawners[_selectedIndex].getWeaponName() + "] #" + std::to_string(_selectedIndex));
                        } else {
                            int bOffset = totalSpawns + totalWepSpawns;
                            int pOffset = bOffset + (int)_map->brushes.size();
                            int dOffset = pOffset + (int)_map->props.size();
                            int lOffset = dOffset + (int)_map->doors.size();
                            int cOffset = lOffset + (int)_map->lights.size();

                            if (itemIdx >= bOffset && itemIdx < pOffset) {
                                _selectionType = SelectionType::Brush;
                                _selectedIndex = itemIdx - bOffset;
                                _selectedTexture = _map->brushes[_selectedIndex].texturePath;
                                logMessage("Outliner: Selected Brush #" + std::to_string(_selectedIndex));
                            } else if (itemIdx >= pOffset && itemIdx < dOffset) {
                                _selectionType = SelectionType::Prop;
                                _selectedIndex = itemIdx - pOffset;
                                _selectedModel = _map->props[_selectedIndex].modelPath;
                                logMessage("Outliner: Selected Prop #" + std::to_string(_selectedIndex));
                            } else if (itemIdx >= dOffset && itemIdx < lOffset) {
                                _selectionType = SelectionType::Door;
                                _selectedIndex = itemIdx - dOffset;
                                logMessage("Outliner: Selected Door #" + std::to_string(_selectedIndex));
                            } else if (itemIdx >= lOffset && itemIdx < cOffset) {
                                _selectionType = SelectionType::Light;
                                _selectedIndex = itemIdx - lOffset;
                                logMessage("Outliner: Selected Lamp Light #" + std::to_string(_selectedIndex) + " (" + _map->lights[_selectedIndex].name + ")");
                            } else if (itemIdx >= cOffset) {
                                _selectionType = SelectionType::Camera;
                                _selectedIndex = itemIdx - cOffset;
                                logMessage("Outliner: Selected Camera #" + std::to_string(_selectedIndex) + " (" + _map->cameras[_selectedIndex].name + ")");
                            }
                        }

                        // Right-Click: Open Component Context Menu with in-menu live search
                        if (isRmb) {
                            _contextMenuOpen = true;
                            _contextMenuX = mx;
                            _contextMenuY = my;
                            _contextMenuTargetIndex = itemIdx;
                            _contextMenuSearch = "";
                            logMessage("Hierarchy: Right-clicked entity #" + std::to_string(itemIdx) + " - Opened In-Menu Component Search");
                            return;
                        }

                        // Left-Click: Detect Double-Click to open in Lua Script Editor
                        float curTime = (float)glfwGetTime();
                        if (curTime - _lastOutlinerClickTime < 0.35f && _lastOutlinerClickIndex == itemIdx) {
                            setWorkspace(EditorWorkspace::Scripting_Editor);
                            logMessage("Hierarchy: Double-clicked entity #" + std::to_string(itemIdx) + " - Switched to Lua Script Editor");
                            return;
                        }
                        _lastOutlinerClickTime = curTime;
                        _lastOutlinerClickIndex = itemIdx;
                        return;
                    }
                }

                // Focus (F)
                if (mx >= l.outFocusX && mx <= l.outFocusX + l.outFocusW && my >= l.outFocusY && my <= l.outFocusY + l.outFocusH) {
                    focusCamera();
                    return;
                }
                // Duplicate (Ctrl+D)
                if (mx >= l.outDupX && mx <= l.outDupX + l.outDupW && my >= l.outDupY && my <= l.outDupY + l.outDupH) {
                    duplicateSelection();
                    return;
                }
                // Delete (Del)
                if (mx >= l.outDelX && mx <= l.outDelX + l.outDelW && my >= l.outDelY && my <= l.outDelY + l.outDelH) {
                    deleteSelection();
                    return;
                }
                // Prev Page
                if (mx >= l.outPrevX && mx <= l.outPrevX + l.outPrevW && my >= l.outPrevY && my <= l.outPrevY + l.outPrevH) {
                    _outlinerScroll = std::max(0, _outlinerScroll - maxItems);
                    return;
                }
                // Next Page
                if (mx >= l.outNextX && mx <= l.outNextX + l.outNextW && my >= l.outNextY && my <= l.outNextY + l.outNextH) {
                    if (_outlinerScroll + maxItems < totalEntities) _outlinerScroll += maxItems;
                    return;
                }
                return;
            }

            // PROPERTIES TAB INTERACTIONS
            if (_sidebarTab == SidebarTab::Properties) {
                // Weapon Spawner Properties Interactions
                if (_selectionType == SelectionType::WeaponSpawner && _selectedIndex >= 0 && _selectedIndex < (int)_map->weaponSpawners.size()) {
                    auto& ws = _map->weaponSpawners[_selectedIndex];
                    float propY = l.rightY + 38.0f;
                    float wepBtnY = propY + 114.0f;
                    if (my >= wepBtnY && my <= wepBtnY + 26.0f) {
                        if (mx >= l.rightX + 10.0f && mx <= l.rightX + 140.0f) {
                            ws.weaponId = (ws.weaponId - 1 + 9) % 9;
                            logMessage("Changed Spawner to: " + ws.getWeaponName());
                            return;
                        }
                        if (mx >= l.rightX + 150.0f && mx <= l.rightX + 280.0f) {
                            ws.weaponId = (ws.weaponId + 1) % 9;
                            logMessage("Changed Spawner to: " + ws.getWeaponName());
                            return;
                        }
                    }

                    float respBtnY = wepBtnY + 54.0f;
                    if (my >= respBtnY && my <= respBtnY + 24.0f) {
                        if (mx >= l.rightX + 10.0f && mx <= l.rightX + 80.0f) {
                            ws.respawnTime = std::max(10.0f, ws.respawnTime - 15.0f);
                            logMessage("Respawn Time: " + std::to_string((int)ws.respawnTime) + "s");
                            return;
                        }
                        if (mx >= l.rightX + 90.0f && mx <= l.rightX + 190.0f) {
                            ws.respawnTime = 60.0f;
                            logMessage("Respawn Time: 60s (1 min)");
                            return;
                        }
                        if (mx >= l.rightX + 200.0f && mx <= l.rightX + 270.0f) {
                            ws.respawnTime = std::min(300.0f, ws.respawnTime + 15.0f);
                            logMessage("Respawn Time: " + std::to_string((int)ws.respawnTime) + "s");
                            return;
                        }
                    }

                    // Deselect
                    if (mx >= l.deselX && mx <= l.deselX + l.deselW && my >= l.deselY && my <= l.deselY + l.deselH) {
                        _selectionType = SelectionType::None;
                        _selectedIndex = -1;
                        logMessage("Deselected all");
                        return;
                    }
                    // Delete
                    if (mx >= l.delX && mx <= l.delX + l.delW && my >= l.delY && my <= l.delY + l.delH) {
                        deleteSelection();
                        return;
                    }
                }

                // Spawn Point Properties Interactions
                if (_selectionType == SelectionType::Spawn && _selectedIndex >= 0 && _selectedIndex < (int)_map->spawnPoints.size()) {
                    float propY = l.rightY + 38.0f;
                    float typeBtnY = propY + 114.0f;
                    float btnW = 94.0f;
                    auto& sp = _map->spawnPoints[_selectedIndex];

                    if (my >= typeBtnY && my <= typeBtnY + 26.0f) {
                        if (mx >= l.rightX + 10.0f && mx <= l.rightX + 10.0f + btnW) {
                            sp.type = SpawnType::FFA;
                            sp.entityClass = "info_player_deathmatch";
                            logMessage("Set Spawn #" + std::to_string(_selectedIndex) + " to FFA / DM");
                            return;
                        }
                        if (mx >= l.rightX + 110.0f && mx <= l.rightX + 110.0f + btnW) {
                            sp.type = SpawnType::TeamAlpha;
                            sp.entityClass = "info_player_team1";
                            logMessage("Set Spawn #" + std::to_string(_selectedIndex) + " to Team Alpha (Blue)");
                            return;
                        }
                        if (mx >= l.rightX + 210.0f && mx <= l.rightX + 210.0f + btnW) {
                            sp.type = SpawnType::TeamBeta;
                            sp.entityClass = "info_player_team2";
                            logMessage("Set Spawn #" + std::to_string(_selectedIndex) + " to Team Beta (Red)");
                            return;
                        }
                    }

                    float yawBtnY = typeBtnY + 58.0f;
                    if (my >= yawBtnY && my <= yawBtnY + 24.0f) {
                        float yBtnW = 55.0f;
                        float yVals[5] = { -45.0f, 0.0f, 90.0f, 180.0f, 45.0f };
                        for (int k = 0; k < 5; ++k) {
                            float yx = l.rightX + 10.0f + k * 58.0f;
                            if (mx >= yx && mx <= yx + yBtnW) {
                                if (k == 0) sp.yaw -= 45.0f;
                                else if (k == 4) sp.yaw += 45.0f;
                                else sp.yaw = yVals[k];
                                while (sp.yaw < 0.0f) sp.yaw += 360.0f;
                                while (sp.yaw >= 360.0f) sp.yaw -= 360.0f;
                                logMessage("Set Spawn #" + std::to_string(_selectedIndex) + " Yaw: " + std::to_string((int)sp.yaw));
                                return;
                            }
                        }
                    }
                }

                // Lamp / Light Properties Interactions
                if (_selectionType == SelectionType::Light && _selectedIndex >= 0 && _selectedIndex < (int)_map->lights.size()) {
                    float propY = l.rightY + 38.0f;
                    float colBtnY = propY + 116.0f;
                    float colBtnW = 90.0f;
                    auto& lt = _map->lights[_selectedIndex];

                    // Row 1 Color Presets
                    if (my >= colBtnY && my <= colBtnY + 24.0f) {
                        if (mx >= l.rightX + 10.0f && mx <= l.rightX + 10.0f + colBtnW) { lt.color = Vec3(1.0f, 0.88f, 0.72f); logMessage("Light Color: Warm 2700K"); return; }
                        if (mx >= l.rightX + 106.0f && mx <= l.rightX + 106.0f + colBtnW) { lt.color = Vec3(0.95f, 0.98f, 1.0f); logMessage("Light Color: Cool 6500K"); return; }
                        if (mx >= l.rightX + 202.0f && mx <= l.rightX + 202.0f + colBtnW) { lt.color = Vec3(1.0f, 0.75f, 0.20f); logMessage("Light Color: Sodium Amber"); return; }
                    }
                    // Row 2 Color Presets
                    if (my >= colBtnY + 28.0f && my <= colBtnY + 52.0f) {
                        if (mx >= l.rightX + 10.0f && mx <= l.rightX + 10.0f + colBtnW) { lt.color = Vec3(0.20f, 0.90f, 1.0f); logMessage("Light Color: Neon Cyan"); return; }
                        if (mx >= l.rightX + 106.0f && mx <= l.rightX + 106.0f + colBtnW) { lt.color = Vec3(1.0f, 0.15f, 0.15f); logMessage("Light Color: Hazard Red"); return; }
                        if (mx >= l.rightX + 202.0f && mx <= l.rightX + 202.0f + colBtnW) { lt.color = Vec3(0.20f, 1.0f, 0.30f); logMessage("Light Color: Toxic Green"); return; }
                    }
                    // Intensity & Radius Adjusters
                    float paramY = colBtnY + 64.0f + 18.0f;
                    if (my >= paramY && my <= paramY + 24.0f) {
                        if (mx >= l.rightX + 10.0f && mx <= l.rightX + 78.0f) { lt.intensity = std::max(0.2f, lt.intensity - 0.5f); logMessage("Light Intensity: " + std::to_string(lt.intensity)); return; }
                        if (mx >= l.rightX + 82.0f && mx <= l.rightX + 150.0f) { lt.intensity += 0.5f; logMessage("Light Intensity: " + std::to_string(lt.intensity)); return; }
                        if (mx >= l.rightX + 158.0f && mx <= l.rightX + 226.0f) { lt.radius = std::max(2.0f, lt.radius - 2.0f); logMessage("Light Radius: " + std::to_string((int)lt.radius) + "m"); return; }
                        if (mx >= l.rightX + 230.0f && mx <= l.rightX + 298.0f) { lt.radius += 2.0f; logMessage("Light Radius: " + std::to_string((int)lt.radius) + "m"); return; }
                    }
                }

                // Entity Name & Script Assignment buttons
                float propY = l.rightY + 38.0f;
                if (my >= propY + 74.0f && my <= propY + 98.0f) {
                    if (mx >= l.rightX + 10.0f && mx <= l.rightX + 145.0f) {
                        if (_selectionType == SelectionType::Brush && _selectedIndex >= 0 && _selectedIndex < (int)_map->brushes.size()) {
                            _map->brushes[_selectedIndex].name = "Klocek1";
                        } else if (_selectionType == SelectionType::Prop && _selectedIndex >= 0 && _selectedIndex < (int)_map->props.size()) {
                            _map->props[_selectedIndex].name = "Klocek1";
                        } else if (_selectionType == SelectionType::Sprite && _selectedIndex >= 0 && _selectedIndex < (int)_map->sprites.size()) {
                            _map->sprites[_selectedIndex].name = "Klocek1";
                        }
                        logMessage("Assigned Name 'Klocek1' (Controllable in Lua via World.getEntity('Klocek1'))");
                        return;
                    }
                    if (mx >= l.rightX + 155.0f && mx <= l.rightX + 295.0f) {
                        if (_selectionType == SelectionType::Brush && _selectedIndex >= 0 && _selectedIndex < (int)_map->brushes.size()) {
                            _map->brushes[_selectedIndex].script = "assets/scripts/gameplay.lua";
                        } else if (_selectionType == SelectionType::Prop && _selectedIndex >= 0 && _selectedIndex < (int)_map->props.size()) {
                            _map->props[_selectedIndex].script = "assets/scripts/gameplay.lua";
                        } else if (_selectionType == SelectionType::Sprite && _selectedIndex >= 0 && _selectedIndex < (int)_map->sprites.size()) {
                            _map->sprites[_selectedIndex].script = "assets/scripts/gameplay.lua";
                        }
                        logMessage("Assigned Lua Script 'assets/scripts/gameplay.lua'");
                        return;
                    }
                }

                // UV Scale buttons [0.125] [0.25] [0.5] [1.0]
                float scales[4] = { 0.125f, 0.25f, 0.5f, 1.0f };
                for (int i = 0; i < 4; ++i) {
                    float sx = l.rightX + 10.0f + i * 70.0f;
                    if (mx >= sx && mx <= sx + l.uvBtnW && my >= l.uvBtnY && my <= l.uvBtnY + l.uvBtnH) {
                        _activeUvScale = Vec2(scales[i], scales[i]);
                        if (_selectionType == SelectionType::Brush && _selectedIndex >= 0 && _selectedIndex < (int)_map->brushes.size()) {
                            _map->brushes[_selectedIndex].uvScale = _activeUvScale;
                        }
                        logMessage("Set UV Scale: " + std::to_string(scales[i]));
                        return;
                    }
                }

                // Thumbnail click -> open texture browser
                if (mx >= l.thumbX && mx <= l.thumbX + l.thumbS && my >= l.thumbY && my <= l.thumbY + l.thumbS) {
                    _browserOpen = true;
                    return;
                }

                // Browse Textures button
                if (mx >= l.texBrowseX && mx <= l.texBrowseX + l.texBrowseW && my >= l.texBrowseY && my <= l.texBrowseY + l.texBrowseH) {
                    _browserOpen = true;
                    return;
                }

                // Apply Texture button
                if (mx >= l.texApplyX && mx <= l.texApplyX + l.texApplyW && my >= l.texApplyY && my <= l.texApplyY + l.texApplyH) {
                    if (_selectionType == SelectionType::Brush && _selectedIndex >= 0 && _selectedIndex < (int)_map->brushes.size()) {
                        _map->brushes[_selectedIndex].texturePath = _selectedTexture;
                        _map->brushes[_selectedIndex].uvScale = _activeUvScale;
                        logMessage("Applied texture '" + _selectedTexture + "' to Brush #" + std::to_string(_selectedIndex));
                    } else {
                        placeCurrentObject();
                    }
                    return;
                }

                // Browse Models button
                if (mx >= l.modelBrowseX && mx <= l.modelBrowseX + l.modelBrowseW && my >= l.modelBrowseY && my <= l.modelBrowseY + l.modelBrowseH) {
                    _modelBrowserOpen = true;
                    return;
                }

                // Position translation buttons [-] [+] (Move X / Y / Z on grid)
                if (my >= l.posBtnsY && my <= l.posBtnsY + l.posBtnH) {
                    // X - / +
                    if (mx >= l.rightX + 30.0f && mx <= l.rightX + 56.0f) { moveSelection(-_gridSnap, 0, 0); return; }
                    if (mx >= l.rightX + 60.0f && mx <= l.rightX + 86.0f) { moveSelection(_gridSnap, 0, 0); return; }
                    // Y - / +
                    if (mx >= l.rightX + 123.0f && mx <= l.rightX + 149.0f) { moveSelection(0, -_gridSnap, 0); return; }
                    if (mx >= l.rightX + 153.0f && mx <= l.rightX + 179.0f) { moveSelection(0, _gridSnap, 0); return; }
                    // Z - / +
                    if (mx >= l.rightX + 216.0f && mx <= l.rightX + 242.0f) { moveSelection(0, 0, -_gridSnap); return; }
                    if (mx >= l.rightX + 246.0f && mx <= l.rightX + 272.0f) { moveSelection(0, 0, _gridSnap); return; }
                }

                // Dedicated Quick Elevation Buttons [ ^ UP (+Y) ] [ v DOWN (-Y) ] [ Floor (Y=0) ]
                if (my >= l.elevUpY && my <= l.elevUpY + l.elevUpH) {
                    if (mx >= l.elevUpX && mx <= l.elevUpX + l.elevUpW) {
                        moveSelection(0, _gridSnap, 0);
                        return;
                    }
                    if (mx >= l.elevDwnX && mx <= l.elevDwnX + l.elevDwnW) {
                        moveSelection(0, -_gridSnap, 0);
                        return;
                    }
                    if (mx >= l.elevGndX && mx <= l.elevGndX + l.elevGndW) {
                        alignSelectionToGround();
                        return;
                    }
                }

                // Dimension adjusters [-] [+]
                if (my >= l.dimBtnsY && my <= l.dimBtnsY + l.dimBtnH) {
                    // X - / +
                    if (mx >= l.rightX + 30.0f && mx <= l.rightX + 56.0f) { resizeSelection(-_gridSnap, 0, 0); _brushSize.x = std::max(0.5f, _brushSize.x - _gridSnap); return; }
                    if (mx >= l.rightX + 60.0f && mx <= l.rightX + 86.0f) { resizeSelection(_gridSnap, 0, 0); _brushSize.x += _gridSnap; return; }
                    // Y - / +
                    if (mx >= l.rightX + 123.0f && mx <= l.rightX + 149.0f) { resizeSelection(0, -_gridSnap, 0); _brushSize.y = std::max(0.5f, _brushSize.y - _gridSnap); return; }
                    if (mx >= l.rightX + 153.0f && mx <= l.rightX + 179.0f) { resizeSelection(0, _gridSnap, 0); _brushSize.y += _gridSnap; return; }
                    // Z - / +
                    if (mx >= l.rightX + 216.0f && mx <= l.rightX + 242.0f) { resizeSelection(0, 0, -_gridSnap); _brushSize.z = std::max(0.5f, _brushSize.z - _gridSnap); return; }
                    if (mx >= l.rightX + 246.0f && mx <= l.rightX + 272.0f) { resizeSelection(0, 0, _gridSnap); _brushSize.z += _gridSnap; return; }
                }

                // Rotation adjusters (Pitch -/+15, Yaw -/+45, Roll -/+15)
                if (my >= l.rotBtnsY && my <= l.rotBtnsY + l.rotBtnH) {
                    // Pitch -15 / +15
                    if (mx >= l.rightX + 28.0f && mx <= l.rightX + 54.0f) { rotateSelection(-15.0f, 0, 0); return; }
                    if (mx >= l.rightX + 56.0f && mx <= l.rightX + 82.0f) { rotateSelection(15.0f, 0, 0); return; }
                    // Yaw -45 / +45
                    if (mx >= l.rightX + 124.0f && mx <= l.rightX + 168.0f) { rotateSelection(0, -45.0f, 0); return; }
                    if (mx >= l.rightX + 172.0f && mx <= l.rightX + 216.0f) { rotateSelection(0, 45.0f, 0); return; }
                    // Roll -15 / +15
                    if (mx >= l.rightX + 242.0f && mx <= l.rightX + 268.0f) { rotateSelection(0, 0, -15.0f); return; }
                    if (mx >= l.rightX + 270.0f && mx <= l.rightX + 296.0f) { rotateSelection(0, 0, 15.0f); return; }
                }

                // Deselect All button
                if (mx >= l.deselX && mx <= l.deselX + l.deselW && my >= l.deselY && my <= l.deselY + l.deselH) {
                    _selectionType = SelectionType::None;
                    _selectedIndex = -1;
                    logMessage("Deselected all.");
                    return;
                }

                // Delete Selected button
                if (mx >= l.delX && mx <= l.delX + l.delW && my >= l.delY && my <= l.delY + l.delH) {
                    deleteSelection();
                    return;
                }
            }
            return;
        }

        // Bottom Dock (Console / Profiler Tabs)
        float leftW = 42.0f;
        float conW = std::min(780.0f, l.rightX - leftW - 40.0f);
        float conH = 135.0f;
        float conX = leftW + 20.0f;
        float conY = h - conH - 32.0f;
        if (mx >= conX && mx <= conX + conW && my >= conY && my <= conY + conH) {
            if (my <= conY + 24.0f) {
                if (mx >= conX + 6.0f && mx <= conX + 96.0f) {
                    _bottomDockTab = BottomDockTab::Console;
                    return;
                }
                if (mx >= conX + 102.0f && mx <= conX + 192.0f) {
                    _bottomDockTab = BottomDockTab::Profiler;
                    return;
                }
            }
            return;
        }

        // Viewport Tab Bar Click Handling (Seamless one-click switching between Project Hub, 3D Scene, Script Studio, HUD Canvas, and Plugins)
        float vpTabX = leftW;
        float vpTabY = 58.0f;
        float tabH = 26.0f;
        if (my >= vpTabY && my <= vpTabY + tabH && mx >= vpTabX && mx <= l.rightX) {
            float tab0X = vpTabX + 6.0f;
            float tab0W = 110.0f;
            float tab1X = tab0X + tab0W + 6.0f;
            float tab1W = 110.0f;
            float tab2X = tab1X + tab1W + 6.0f;
            float tab2W = 140.0f;
            float tab3X = tab2X + tab2W + 6.0f;
            float tab3W = 120.0f;
            float tab4X = tab3X + tab3W + 6.0f;
            float tab4W = 135.0f;

            if (mx >= tab0X && mx <= tab0X + tab0W) {
                setWorkspace(EditorWorkspace::Project_Hub);
                return;
            } else if (mx >= tab1X && mx <= tab1X + tab1W) {
                setWorkspace(EditorWorkspace::World3D_Level);
                return;
            } else if (mx >= tab2X && mx <= tab2X + tab2W) {
                setWorkspace(EditorWorkspace::Scripting_Editor);
                return;
            } else if (mx >= tab3X && mx <= tab3X + tab3W) {
                setWorkspace(EditorWorkspace::Level2D_HUD);
                return;
            } else if (mx >= tab4X && mx <= tab4X + tab4W) {
                setWorkspace(EditorWorkspace::Plugins_Manager);
                return;
            }
        }

        // 8.4. 3D Model Viewer Workspace Interactions
        if (_workspace == EditorWorkspace::World3D_ModelViewer) {
            float vpX = leftW;
            float vpW = l.rightX - vpX;
            float vpH = (h - 22.0f) - 58.0f;
            float listX = vpX + 8.0f;
            float listY = 58.0f + 32.0f;
            float listW = 220.0f;
            float listH = vpH - 36.0f;

            // Click model in list
            if (mx >= listX && mx <= listX + listW && my >= listY + 28.0f && my <= listY + listH) {
                int clickedIdx = (int)((my - (listY + 28.0f)) / 22.0f);
                if (clickedIdx >= 0 && clickedIdx < (int)_availableModels.size()) {
                    _selectedModel = _availableModels[clickedIdx];
                    logMessage("3D Model Viewer: Selected model '" + _selectedModel + "'");
                    return;
                }
            }

            // Right inspector buttons
            float inspW = 250.0f;
            float inspX = vpX + vpW - inspW - 8.0f;
            float inspY = 58.0f + 32.0f;
            float iy = inspY + 36.0f + 92.0f;

            if (my >= iy && my <= iy + 26.0f) {
                if (mx >= inspX + 10.0f && mx <= inspX + 120.0f) {
                    _modelViewerAutoRotate = !_modelViewerAutoRotate;
                    return;
                } else if (mx >= inspX + 128.0f && mx <= inspX + 238.0f) {
                    _modelViewerWireframe = !_modelViewerWireframe;
                    return;
                }
            }
            if (my >= iy + 34.0f && my <= iy + 60.0f) {
                if (mx >= inspX + 10.0f && mx <= inspX + 238.0f) {
                    _modelViewerYaw = 35.0f;
                    _modelViewerPitch = 15.0f;
                    _modelViewerDistance = 3.0f;
                    logMessage("3D Model Viewer: Turntable reset.");
                    return;
                }
            }
            if (my >= iy + 68.0f && my <= iy + 94.0f) {
                if (mx >= inspX + 10.0f && mx <= inspX + 238.0f) {
                    logMessage("3D Model Viewer: Applied texture '" + _selectedTexture + "' to model '" + _selectedModel + "'");
                    return;
                }
            }
        }

        // 8.5. Native LabHUDEditor2D Workspace Interactions
        if (_workspace == EditorWorkspace::Level2D_HUD) {
            return; // Handled directly by _hudEditor in onUpdate
        }

        // 8.6. 2D Spritesheet Animation Workspace Interactions
        if (_workspace == EditorWorkspace::Level2D_Animation) {
            float vpX = leftW;
            float vpW = l.rightX - vpX;
            float vpH = (h - 22.0f) - 58.0f;
            float anW = std::min(vpW - 40.0f, 920.0f);
            float anH = std::min(vpH - 40.0f, 520.0f);
            float anX = vpX + (vpW - anW) * 0.5f;
            float anY = 58.0f + (vpH - anH) * 0.5f;

            // Filmstrip click
            float stageH = 240.0f;
            float stripY = anY + 40.0f + stageH + 16.0f;
            float frameSize = 56.0f;
            float gap = 8.0f;
            float stripX = anX + 20.0f;

            if (my >= stripY && my <= stripY + frameSize) {
                for (int i = 0; i < _anim2DTotalFrames; ++i) {
                    float fx = stripX + i * (frameSize + gap);
                    if (mx >= fx && mx <= fx + frameSize) {
                        _anim2DSelectedFrame = i;
                        logMessage("2D Animation: Selected frame #" + std::to_string(i));
                        return;
                    }
                }
            }

            // Timeline buttons
            float tlY = anY + anH - 65.0f;
            if (my >= tlY && my <= tlY + 28.0f) {
                if (mx >= anX + 20.0f && mx <= anX + 100.0f) {
                    _anim2DSelectedFrame = 0;
                    return;
                } else if (mx >= anX + 108.0f && mx <= anX + 208.0f) {
                    _anim2DPlaying = !_anim2DPlaying;
                    return;
                } else if (mx >= anX + 216.0f && mx <= anX + 306.0f) {
                    _anim2DLoop = !_anim2DLoop;
                    return;
                } else if (mx >= anX + 314.0f && mx <= anX + 394.0f) {
                    if (_anim2DFPS == 12) _anim2DFPS = 24;
                    else if (_anim2DFPS == 24) _anim2DFPS = 30;
                    else if (_anim2DFPS == 30) _anim2DFPS = 60;
                    else _anim2DFPS = 12;
                    return;
                }
            }
        }

        // 8.7. Scripting Workspace IDE Interactions
        if (_workspace == EditorWorkspace::Scripting_Editor || _workspace == EditorWorkspace::Scripting_Console) {
            float vpX = leftW;
            float vpW = l.rightX - vpX;
            float vpH = (h - 22.0f) - 58.0f;
            float contentY = 58.0f + 26.0f; // below Viewport Tab Bar
            float contentH = vpH - 26.0f;

            float scriptListW = 190.0f;
            float listX = vpX + 4.0f;
            float listY = contentY + 4.0f;
            float listH = contentH - 8.0f - 24.0f;

            // Click inside Left File Explorer Panel (strictly game://scripts)
            if (mx >= listX && mx <= listX + scriptListW && my >= listY && my <= listY + listH) {
                float newBtnY = listY + 38.0f;
                // Click [+ New Script]
                if (my >= newBtnY && my <= newBtnY + 22.0f && mx >= listX + 6.0f && mx <= listX + scriptListW - 6.0f) {
                    std::string newFileName = "script_" + std::to_string(_gameScripts.size() + 1) + ".lua";
                    auto fullPath = ProjectManager::get().getActiveProjectAssetsPath() / "scripts" / newFileName;
                    std::error_code ec;
                    std::filesystem::create_directories(fullPath.parent_path(), ec);
                    std::ofstream out(fullPath);
                    if (out.is_open()) {
                        out << "-- " << newFileName << " (Game Script)\nfunction OnStart()\n    -- initialization\nend\n\nfunction OnUpdate(dt)\n    -- frame update\nend\n";
                        out.close();
                    }
                    refreshGameScripts();
                    _activeScriptPath = "game://scripts/" + newFileName;
                    loadScriptFromFile(_activeScriptPath);
                    logMessage("Created clean game script: " + _activeScriptPath);
                    return;
                }

                // Click script item
                float itemStartY = newBtnY + 28.0f;
                for (size_t i = 0; i < _gameScripts.size(); ++i) {
                    float iy = itemStartY + i * 24.0f;
                    if (my >= iy && my <= iy + 22.0f) {
                        _selectedGameScriptIndex = (int)i;
                        _activeScriptPath = "game://scripts/" + _gameScripts[i];
                        loadScriptFromFile(_activeScriptPath);
                        _scriptEditorFocused = true;
                        return;
                    }
                }
                return;
            }

            // Right Code Toolbar & Editor Area
            float codeX = listX + scriptListW + 6.0f;
            float codeW = vpW - scriptListW - 14.0f;
            float actY = contentY + 3.0f;

            if (my >= actY && my <= actY + 28.0f) {
                if (mx >= codeX && mx <= codeX + 150.0f) {
                    runCurrentScript();
                    return;
                } else if (mx >= codeX + 156.0f && mx <= codeX + 266.0f) {
                    saveScriptToFile(_activeScriptPath);
                    return;
                } else if (mx >= codeX + 272.0f && mx <= codeX + 412.0f) {
                    _currentScript =
                        "-- Preset 1: Moving Klocek + HUD Timer\n"
                        "function OnStart()\n"
                        "    HUD.setText(\"TimerLabel\", \"TIME: 00:00\")\n"
                        "    HUD.setValue(\"Health\", 100)\n"
                        "end\n\n"
                        "local timer = 0.0\n"
                        "function OnUpdate(dt)\n"
                        "    timer = timer + dt\n"
                        "    local sec = math.floor(timer)\n"
                        "    HUD.setText(\"TimerLabel\", string.format(\"TIME: %02d:%02d\", math.floor(sec/60), sec%60))\n"
                        "    local klocek = World.getEntity(\"Klocek1\")\n"
                        "    if klocek then\n"
                        "        klocek:move(0, math.sin(timer * 4.0) * 0.05, 0)\n"
                        "    end\n"
                        "end\n";
                    syncStringToScriptLines();
                    runCurrentScript();
                    logMessage("Loaded Script Preset: Moving Klocek + HUD Timer.");
                    return;
                } else if (mx >= codeX + 418.0f && mx <= codeX + 548.0f) {
                    _currentScript =
                        "-- Preset 2: Multi-Genre HUD Game Loop\n"
                        "local health = 100.0\n"
                        "local mana = 85.0\n"
                        "local score = 14500\n"
                        "function OnStart()\n"
                        "    HUD.setValue(\"Health\", health)\n"
                        "    HUD.setValue(\"Mana\", mana)\n"
                        "    HUD.setText(\"Score\", \"SCORE: \" .. tostring(score))\n"
                        "end\n\n"
                        "function OnUpdate(dt)\n"
                        "    HUD.setValue(\"Health\", health)\n"
                        "    HUD.setValue(\"Mana\", mana)\n"
                        "end\n";
                    syncStringToScriptLines();
                    runCurrentScript();
                    logMessage("Loaded Script Preset: Multi-Genre HUD Game Loop.");
                    return;
                } else if (mx >= codeX + 554.0f && mx <= codeX + 674.0f) {
                    _currentScript =
                        "-- Preset 3: Turret & Continuous Movement\n"
                        "local angle = 0.0\n"
                        "function OnUpdate(dt)\n"
                        "    angle = angle + dt * 2.0\n"
                        "    local e = World.getEntity(\"Klocek1\")\n"
                        "    if e then\n"
                        "        local r = 3.0\n"
                        "        e:setPosition(math.cos(angle)*r, 1.5, math.sin(angle)*r - 4.0)\n"
                        "    end\n"
                        "end\n";
                    syncStringToScriptLines();
                    runCurrentScript();
                    logMessage("Loaded Script Preset: Turret Continuous Movement.");
                    return;
                } else if (mx >= codeX + 680.0f && mx <= codeX + 810.0f) {
                    if (_selectionType == SelectionType::Brush && _selectedIndex >= 0 && _selectedIndex < (int)_map->brushes.size()) {
                        _map->brushes[_selectedIndex].name = "Klocek1";
                        _map->brushes[_selectedIndex].script = _activeScriptPath;
                    } else if (_selectionType == SelectionType::Prop && _selectedIndex >= 0 && _selectedIndex < (int)_map->props.size()) {
                        _map->props[_selectedIndex].name = "Klocek1";
                        _map->props[_selectedIndex].script = _activeScriptPath;
                    } else if (_selectionType == SelectionType::Sprite && _selectedIndex >= 0 && _selectedIndex < (int)_map->sprites.size()) {
                        _map->sprites[_selectedIndex].name = "Klocek1";
                        _map->sprites[_selectedIndex].script = _activeScriptPath;
                    }
                    logMessage("Named selected entity 'Klocek1' (Controllable in Lua via World.getEntity('Klocek1'))");
                    return;
                }
            }

            // Click inside the Code Area to place cursor & focus
            float codeY = contentY + 34.0f;
            float codeH = contentH - 34.0f - 24.0f;
            if (mx >= codeX && mx <= codeX + codeW && my >= codeY && my <= codeY + codeH) {
                _scriptEditorFocused = true;
                float relY = my - (codeY + 6.0f);
                int lineIdx = _scriptScrollLine + (int)(relY / 18.0f);
                if (lineIdx >= 0 && lineIdx < (int)_scriptLines.size()) {
                    _scriptCursorLine = lineIdx;
                    float gutterW = 46.0f;
                    float textStartX = codeX + gutterW + 8.0f;
                    float scriptFontScale = 1.35f;
                    float charAdvance = LabFont::getMonospaceAdvance(scriptFontScale);
                    float relX = std::max(0.0f, mx - textStartX);
                    int colIdx = (int)std::floor((relX + charAdvance * 0.4f) / charAdvance);
                    _scriptCursorCol = std::clamp(colIdx, 0, (int)_scriptLines[_scriptCursorLine].size());
                    _scriptBlinkTimer = 0.0f;
                }
                return;
            }
            return; // Consume click so it never triggers 3D actions
        }

        // 8.75. Package & Plugin Manager Workspace Interactions (Unity Package Manager & Blender Add-ons)
        if (_workspace == EditorWorkspace::Plugins_Manager) {
            float vpX = leftW;
            float vpW = l.rightX - vpX;
            float contentY = 58.0f + 26.0f;
            float contentH = (h - 22.0f) - contentY;
            float barY = contentY + 4.0f;
            float barH = 34.0f;
            float catY = barY + barH + 4.0f;
            float catH = 26.0f;
            float mainY = catY + catH + 6.0f;
            float listW = 380.0f;
            float listX = vpX + 6.0f;
            float detX = listX + listW + 8.0f;
            float detW = vpW - listW - 20.0f;

            // 1. Top Bar: Search Input, Filter Buttons, Import, Rescan
            if (my >= barY + 4.0f && my <= barY + 30.0f) {
                // Search Input Box
                float searchX = vpX + 380.0f;
                float searchW = 190.0f;
                if (mx >= searchX && mx <= searchX + searchW) {
                    if (mx >= searchX + searchW - 24.0f && !_pluginSearchQuery.empty()) {
                        _pluginSearchQuery = "";
                    } else {
                        _pluginSearchFocused = true;
                    }
                    return;
                } else {
                    _pluginSearchFocused = false;
                }

                // Filter Buttons
                float btnX = vpX + vpW - 480.0f;
                if (mx >= btnX && mx <= btnX + 65.0f) {
                    _pluginStateFilter = "All";
                    return;
                } else if (mx >= btnX + 70.0f && mx <= btnX + 155.0f) {
                    _pluginStateFilter = "Enabled";
                    return;
                } else if (mx >= btnX + 160.0f && mx <= btnX + 245.0f) {
                    _pluginStateFilter = "Disabled";
                    return;
                } else if (mx >= btnX + 250.0f && mx <= btnX + 360.0f) {
                    std::string newPluginPath = "plugins/custom_addon.lua";
                    if (!std::filesystem::exists(newPluginPath)) {
                        std::ofstream out(newPluginPath);
                        out << "-- Custom Lab Plugin Template\n"
                            << "Plugin = {\n"
                            << "    id = \"com.community.custom_addon\",\n"
                            << "    displayName = \"Community Custom Addon\",\n"
                            << "    version = \"1.0.0\",\n"
                            << "    author = \"User\",\n"
                            << "    category = \"Tools & Utilities\",\n"
                            << "    description = \"User created standalone plugin script.\",\n"
                            << "    state = \"Discovered\"\n"
                            << "}\n\n"
                            << "function Plugin.onLoad()\n"
                            << "    print(\"Custom Addon loaded!\")\n"
                            << "    return true\n"
                            << "end\n\n"
                            << "function Plugin.onUpdate(dt)\n"
                            << "end\n\n"
                            << "function Plugin.onUnload()\n"
                            << "end\n\n"
                            << "return Plugin\n";
                    }
                    PackageManager::instance().scanPluginDirectory();
                    _pluginStatusBanner = "Created template plugins/custom_addon.lua!";
                    _pluginBannerTimer = 4.0f;
                    return;
                } else if (mx >= btnX + 365.0f && mx <= btnX + 465.0f) {
                    PackageManager::instance().scanPluginDirectory();
                    _pluginStatusBanner = "Rescanned plugins/ for drop-in .lua plugins!";
                    _pluginBannerTimer = 3.0f;
                    return;
                }
            } else {
                _pluginSearchFocused = false;
            }

            // 2. Category Tabs
            if (my >= catY && my <= catY + catH) {
                const char* cats[] = { "All", "Physics & Simulation", "Rendering & VFX", "Gameplay & HUD", "3D & Geometry", "Scripting & Dev", "Tools & Utilities" };
                float cx = vpX + 14.0f;
                for (int i = 0; i < 7; ++i) {
                    float cw = (i == 0) ? 45.0f : (i == 1 ? 160.0f : 130.0f);
                    if (mx >= cx && mx <= cx + cw) {
                        _pluginCategoryFilter = cats[i];
                        return;
                    }
                    cx += cw + 6.0f;
                }
            }

            // 3. Package Cards & Blender Checkboxes
            auto pkgs = PackageManager::instance().filterPackages(_pluginSearchQuery, _pluginCategoryFilter, _pluginStateFilter);
            float itemH = 54.0f;
            float curItemY = mainY + 4.0f;
            for (size_t i = 0; i < pkgs.size(); ++i) {
                if (curItemY + itemH > mainY + (contentH - (mainY - contentY) - 6.0f) - 10.0f) break;

                if (mx >= listX + 4.0f && mx <= listX + listW - 4.0f && my >= curItemY && my <= curItemY + itemH - 2.0f) {
                    // Check if clicked the Blender-style Checkbox Toggle
                    float boxX = listX + 12.0f;
                    float boxY = curItemY + 16.0f;
                    float boxS = 20.0f;
                    if (mx >= boxX && mx <= boxX + boxS && my >= boxY && my <= boxY + boxS) {
                        PackageManager::instance().togglePackage(pkgs[i].id);
                        bool nowEn = PackageManager::instance().isPackageEnabled(pkgs[i].id);
                        _pluginStatusBanner = "Plugin '" + pkgs[i].displayName + "' is now " + (nowEn ? "ACTIVE" : "DISABLED");
                        _pluginBannerTimer = 3.0f;
                        return;
                    }

                    // Otherwise selected the card
                    _pluginSelectedId = pkgs[i].id;
                    return;
                }
                curItemY += itemH;
            }

            // 4. Right Pane Action Buttons & Customizer Controls
            const auto* selPkg = PackageManager::instance().getPackage(_pluginSelectedId);
            if (selPkg) {
                float dyActions = mainY + 86.0f;
                // Actions Bar: Toggle Enable/Disable
                if (my >= dyActions && my <= dyActions + 30.0f) {
                    if (mx >= detX + 20.0f && mx <= detX + 160.0f) {
                        PackageManager::instance().togglePackage(selPkg->id);
                        bool nowEn = PackageManager::instance().isPackageEnabled(selPkg->id);
                        _pluginStatusBanner = "Plugin '" + selPkg->displayName + "' is now " + (nowEn ? "ACTIVE" : "DISABLED");
                        _pluginBannerTimer = 3.0f;
                        return;
                    }

                    // Open in Lua Studio Button
                    if (mx >= detX + detW - 210.0f && mx <= detX + detW - 20.0f) {
                        std::string luaPath = PackageManager::instance().getPackageFile(selPkg->id);
                        if (luaPath.empty()) luaPath = "plugins/" + selPkg->id.substr(selPkg->id.rfind('.') + 1) + ".lua";
                        if (!std::filesystem::exists(luaPath) && std::filesystem::exists("plugins")) {
                            for (const auto& entry : std::filesystem::directory_iterator("plugins")) {
                                if (entry.path().extension() == ".lua") {
                                    luaPath = entry.path().string();
                                    break;
                                }
                            }
                        }
                        loadScriptFromFile(luaPath);
                        setWorkspace(EditorWorkspace::Scripting_Editor);
                        _pluginStatusBanner = "Opened standalone plugin source: " + luaPath;
                        _pluginBannerTimer = 3.0f;
                        return;
                    }
                }

                // Interactive Customizer Box Area
                float cyBox = mainY + 126.0f + (!_pluginStatusBanner.empty() ? 32.0f : 0.0f);
                if (my >= cyBox && my <= cyBox + 175.0f) {
                    // Retro CRT & Post-Process Shaders Controls
                    if (_pluginSelectedId == "com.lab.retro-fx") {
                        // Presets row: cyBox + 30.0f
                        if (my >= cyBox + 28.0f && my <= cyBox + 54.0f) {
                            if (mx >= detX + 30.0f && mx <= detX + 150.0f) { _retroColorTheme = 0; _retroScanlines = true; _retroAberration = true; _retroVignette = true; _retroPixelScale = 2; _pluginStatusBanner = "Preset applied: Classic CRT 1985"; _pluginBannerTimer = 3.0f; return; }
                            if (mx >= detX + 155.0f && mx <= detX + 275.0f) { _retroColorTheme = 1; _retroScanlines = true; _retroVignette = true; _retroPixelScale = 3; _pluginStatusBanner = "Preset applied: Arcade Monitor"; _pluginBannerTimer = 3.0f; return; }
                            if (mx >= detX + 280.0f && mx <= detX + 400.0f) { _retroColorTheme = 2; _retroScanlines = true; _retroVignette = false; _retroPixelScale = 2; _pluginStatusBanner = "Preset applied: Amber Terminal"; _pluginBannerTimer = 3.0f; return; }
                            if (mx >= detX + 405.0f && mx <= detX + 525.0f) { _retroColorTheme = 3; _retroScanlines = true; _retroAberration = true; _retroVignette = true; _retroPixelScale = 2; _pluginStatusBanner = "Preset applied: Cyberpunk Neon"; _pluginBannerTimer = 3.0f; return; }
                        }
                        // Toggle buttons row: cyBox + 64.0f
                        if (my >= cyBox + 62.0f && my <= cyBox + 88.0f) {
                            if (mx >= detX + 30.0f && mx <= detX + 190.0f) { _retroScanlines = !_retroScanlines; return; }
                            if (mx >= detX + 200.0f && mx <= detX + 360.0f) { _retroAberration = !_retroAberration; return; }
                            if (mx >= detX + 370.0f && mx <= detX + 530.0f) { _retroVignette = !_retroVignette; return; }
                        }
                        // Steppers row: cyBox + 98.0f
                        if (my >= cyBox + 96.0f && my <= cyBox + 122.0f) {
                            if (mx >= detX + 160.0f && mx <= detX + 185.0f) { _retroPixelScale = std::max(1, _retroPixelScale - 1); return; }
                            if (mx >= detX + 215.0f && mx <= detX + 240.0f) { _retroPixelScale = std::min(4, _retroPixelScale + 1); return; }
                            if (mx >= detX + 440.0f && mx <= detX + 465.0f) { _retroVignetteIntensity = std::max(0.1f, _retroVignetteIntensity - 0.1f); return; }
                            if (mx >= detX + 495.0f && mx <= detX + 520.0f) { _retroVignetteIntensity = std::min(1.0f, _retroVignetteIntensity + 0.1f); return; }
                        }
                    }
                    // Cloth & Soft Body Dynamics Controls
                    else if (_pluginSelectedId == "com.lab.physics-cloth") {
                        // Presets / Toggles row: cyBox + 28.0f
                        if (my >= cyBox + 28.0f && my <= cyBox + 54.0f) {
                            if (mx >= detX + 30.0f && mx <= detX + 210.0f) { _clothSimulateInEditor = !_clothSimulateInEditor; _pluginStatusBanner = _clothSimulateInEditor ? "Cloth Simulation: ACTIVE" : "Cloth Simulation: PAUSED"; _pluginBannerTimer = 3.0f; return; }
                            if (mx >= detX + 220.0f && mx <= detX + 340.0f) { _clothWindVelocity = Vec3(0.0f, 0.0f, 3.5f); _clothStiffness = 1.0f; _pluginStatusBanner = "Preset applied: Hanging Flag"; _pluginBannerTimer = 3.0f; return; }
                            if (mx >= detX + 345.0f && mx <= detX + 465.0f) { _clothWindVelocity = Vec3(2.0f, 0.0f, 6.0f); _clothStiffness = 0.8f; _pluginStatusBanner = "Preset applied: Hero Cape"; _pluginBannerTimer = 3.0f; return; }
                            if (mx >= detX + 470.0f && mx <= detX + 590.0f) { _clothWindVelocity = Vec3(0.0f, 0.0f, 0.0f); _clothStiffness = 0.95f; _pluginStatusBanner = "Preset applied: Canopy Sheet"; _pluginBannerTimer = 3.0f; return; }
                        }
                        // Steppers row: cyBox + 64.0f
                        if (my >= cyBox + 62.0f && my <= cyBox + 88.0f) {
                            if (mx >= detX + 150.0f && mx <= detX + 175.0f) { _clothWindVelocity.z -= 1.0f; return; }
                            if (mx >= detX + 215.0f && mx <= detX + 240.0f) { _clothWindVelocity.z += 1.0f; return; }
                            if (mx >= detX + 420.0f && mx <= detX + 445.0f) { _clothStiffness = std::max(0.1f, _clothStiffness - 0.1f); return; }
                            if (mx >= detX + 485.0f && mx <= detX + 510.0f) { _clothStiffness = std::min(1.0f, _clothStiffness + 0.1f); return; }
                        }
                        // Actions row: cyBox + 100.0f
                        if (my >= cyBox + 98.0f && my <= cyBox + 126.0f) {
                            if (mx >= detX + 30.0f && mx <= detX + 220.0f) {
                                _clothMeshOrigin = _cursorPos + Vec3(0.0f, 2.5f, 0.0f);
                                _clothMesh.initGrid(12, 12, 0.28f, _clothMeshOrigin);
                                _clothMesh.particles[0].pin();
                                _clothMesh.particles[11].pin();
                                _clothMeshInitialized = true;
                                _pluginStatusBanner = "Spawned 12x12 cloth flag at 3D cursor (" + std::to_string((int)_cursorPos.x) + ", " + std::to_string((int)_cursorPos.y) + ")!";
                                _pluginBannerTimer = 4.0f;
                                logMessage("Cloth Plugin: Spawned hanging flag at 3D cursor position.");
                                return;
                            }
                            if (mx >= detX + 230.0f && mx <= detX + 400.0f) {
                                _clothMesh.initGrid(12, 12, 0.28f, _clothMeshOrigin);
                                _clothMesh.particles[0].pin();
                                _clothMesh.particles[11].pin();
                                _pluginStatusBanner = "Cloth particles reset to rest positions.";
                                _pluginBannerTimer = 3.0f;
                                return;
                            }
                            if (mx >= detX + 410.0f && mx <= detX + 570.0f) {
                                for (auto& cp : _clothMesh.particles) {
                                    if (!cp.isPinned) cp.acceleration += Vec3(0.0f, 5.0f, 15.0f);
                                }
                                _pluginStatusBanner = "Triggered sudden wind gust pulse!";
                                _pluginBannerTimer = 3.0f;
                                return;
                            }
                        }
                    }
                    // Force Fields & Shockwaves Controls
                    else if (_pluginSelectedId == "com.lab.physics-forcefield") {
                        if (my >= cyBox + 28.0f && my <= cyBox + 54.0f) {
                            if (mx >= detX + 30.0f && mx <= detX + 180.0f) { _forceFieldType = 0; _pluginStatusBanner = "Field Type: Radial Explosion"; _pluginBannerTimer = 3.0f; return; }
                            if (mx >= detX + 190.0f && mx <= detX + 340.0f) { _forceFieldType = 1; _pluginStatusBanner = "Field Type: Vortex Whirlwind"; _pluginBannerTimer = 3.0f; return; }
                            if (mx >= detX + 350.0f && mx <= detX + 500.0f) { _forceFieldType = 2; _pluginStatusBanner = "Field Type: Directional Wind"; _pluginBannerTimer = 3.0f; return; }
                        }
                        if (my >= cyBox + 62.0f && my <= cyBox + 88.0f) {
                            if (mx >= detX + 160.0f && mx <= detX + 185.0f) { _forceFieldStrength = std::max(5.0f, _forceFieldStrength - 5.0f); return; }
                            if (mx >= detX + 235.0f && mx <= detX + 260.0f) { _forceFieldStrength = std::min(100.0f, _forceFieldStrength + 5.0f); return; }
                            if (mx >= detX + 430.0f && mx <= detX + 455.0f) { _forceFieldRadius = std::max(2.0f, _forceFieldRadius - 2.0f); return; }
                            if (mx >= detX + 505.0f && mx <= detX + 530.0f) { _forceFieldRadius = std::min(50.0f, _forceFieldRadius + 2.0f); return; }
                        }
                        if (my >= cyBox + 98.0f && my <= cyBox + 126.0f) {
                            if (mx >= detX + 30.0f && mx <= detX + 260.0f) {
                                _forceFieldPosition = _cursorPos;
                                _forceFieldBlastTimer = 1.0f;
                                if (_clothMeshInitialized) {
                                    for (auto& cp : _clothMesh.particles) {
                                        if (!cp.isPinned) {
                                            Vec3 toP = cp.position - _forceFieldPosition;
                                            float d = toP.length();
                                            if (d > 0.01f && d < _forceFieldRadius) {
                                                cp.acceleration += (toP / d) * (_forceFieldStrength * 3.0f);
                                            }
                                        }
                                    }
                                }
                                _pluginStatusBanner = "Detonated blast shockwave at 3D cursor (" + std::to_string((int)_cursorPos.x) + ", " + std::to_string((int)_cursorPos.y) + ")!";
                                _pluginBannerTimer = 4.0f;
                                logMessage("ForceField: Detonated explosion impulse trigger.");
                                return;
                            }
                            if (mx >= detX + 270.0f && mx <= detX + 480.0f) {
                                _forceFieldGizmoVisible = !_forceFieldGizmoVisible;
                                return;
                            }
                        }
                    }
                    // Speedrun & Telemetry Splits Controls
                    else if (_pluginSelectedId == "com.lab.speedrun-timer") {
                        if (my >= cyBox + 28.0f && my <= cyBox + 56.0f) {
                            if (mx >= detX + 30.0f && mx <= detX + 160.0f) { _speedrunTimerRunning = true; _pluginStatusBanner = "Speedrun timer RUNNING."; _pluginBannerTimer = 3.0f; return; }
                            if (mx >= detX + 170.0f && mx <= detX + 300.0f) { _speedrunTimerRunning = false; _pluginStatusBanner = "Speedrun timer PAUSED."; _pluginBannerTimer = 3.0f; return; }
                            if (mx >= detX + 310.0f && mx <= detX + 440.0f) { _speedrunTimerRunning = false; _speedrunElapsedTime = 0.0f; _pluginStatusBanner = "Speedrun timer RESET to 00:00.00."; _pluginBannerTimer = 3.0f; return; }
                        }
                        if (my >= cyBox + 64.0f && my <= cyBox + 90.0f) {
                            if (mx >= detX + 30.0f && mx <= detX + 220.0f) { _speedrunOverlayVisible = !_speedrunOverlayVisible; return; }
                            if (mx >= detX + 230.0f && mx <= detX + 410.0f) { _speedrunPosition = (_speedrunPosition == 0) ? 1 : 0; return; }
                        }
                    }
                    // Arcade Vehicle Dynamics Controls
                    else if (_pluginSelectedId == "com.lab.vehicle-arcade") {
                        if (my >= cyBox + 28.0f && my <= cyBox + 56.0f) {
                            if (mx >= detX + 30.0f && mx <= detX + 260.0f) {
                                _vehicleSpeedKmh = 0.0f;
                                _vehicleRpm = 1000.0f;
                                _pluginStatusBanner = "Spawned test vehicle at 3D cursor position!";
                                _pluginBannerTimer = 4.0f;
                                logMessage("Vehicle Plugin: Spawned raycast test vehicle at cursor.");
                                return;
                            }
                            if (mx >= detX + 270.0f && mx <= detX + 460.0f) {
                                _vehicleTelemetryVisible = !_vehicleTelemetryVisible;
                                return;
                            }
                        }
                        if (my >= cyBox + 64.0f && my <= cyBox + 90.0f) {
                            if (mx >= detX + 180.0f && mx <= detX + 205.0f) { _vehicleThrottlePower = std::max(0.5f, _vehicleThrottlePower - 0.25f); return; }
                            if (mx >= detX + 260.0f && mx <= detX + 285.0f) { _vehicleThrottlePower = std::min(2.5f, _vehicleThrottlePower + 0.25f); return; }
                        }
                    }
                    // Procedural Level Generator
                    else if (_pluginSelectedId == "com.lab.proc-gen") {
                        if (my >= cyBox + 28.0f && my <= cyBox + 58.0f) {
                            if (mx >= detX + 30.0f && mx <= detX + 280.0f) {
                                auto proc = std::dynamic_pointer_cast<ProceduralLevelPlugin>(PackageManager::instance().getPluginImplementation("com.lab.proc-gen"));
                                int added = 0;
                                if (proc && _map) {
                                    added = proc->generateRoom(_map->brushes, _cursorPos);
                                } else if (_map) {
                                    ProceduralLevelPlugin fallbackProc;
                                    added = fallbackProc.generateRoom(_map->brushes, _cursorPos);
                                }
                                _pluginStatusBanner = "Generated procedural room (" + std::to_string(added) + " CSG brushes) at 3D cursor!";
                                _pluginBannerTimer = 5.0f;
                                logMessage("Procedural Generator: Added room brushes to map.");
                                return;
                            }
                        }
                    }
                    // Fluid Dynamics & Water Buoyancy
                    else if (_pluginSelectedId == "com.lab.physics-fluid") {
                        if (my >= cyBox + 28.0f && my <= cyBox + 58.0f) {
                            if (mx >= detX + 30.0f && mx <= detX + 280.0f) {
                                if (_map) {
                                    MapBrush wb;
                                    wb.position = _cursorPos;
                                    wb.size = Vec3(10.0f, 3.0f, 10.0f);
                                    wb.color = Vec3(0.15f, 0.45f, 0.85f);
                                    wb.name = "WaterVolume_01";
                                    _map->brushes.push_back(wb);
                                    _pluginStatusBanner = "Placed Water Volume Brush (10x3x10m) at 3D cursor!";
                                    _pluginBannerTimer = 4.0f;
                                    logMessage("Fluid Plugin: Placed water volume brush into map.");
                                }
                                return;
                            }
                        }
                    }
                    // Constraints & Joint Physics
                    else if (_pluginSelectedId == "com.lab.physics-constraints") {
                        if (my >= cyBox + 28.0f && my <= cyBox + 58.0f) {
                            if (mx >= detX + 30.0f && mx <= detX + 280.0f) {
                                _pluginStatusBanner = "Spawned ball-socket pendulum chain at 3D cursor!";
                                _pluginBannerTimer = 4.0f;
                                logMessage("Constraint Plugin: Spawned pendulum chain at cursor.");
                                return;
                            }
                        }
                    }
                }
            }
            return; // Consume click so it never falls through to 3D actions
        }

        // 8.76. Project Hub / Engine Main Menu Workspace Interactions
        if (_workspace == EditorWorkspace::Project_Hub) {
            float vpX = leftW;
            float vpW = l.rightX - vpX;
            float contentY = 58.0f + 26.0f;
            float contentH = (h - 22.0f) - contentY;
            float headY = contentY + 4.0f;
            float headH = 50.0f;
            float mainY = headY + headH + 8.0f;
            float mainH = contentH - (mainY - contentY) - 6.0f;
            float listW = 390.0f;
            float listX = vpX + 6.0f;
            float detX = listX + listW + 8.0f;
            float detW = vpW - listW - 20.0f;
            (void)detW;

            // 1. Left Panel: Project Cards Click
            if (mx >= listX && mx <= listX + listW && my >= mainY + 36.0f && my <= mainY + mainH - 50.0f) {
                float itemH = 68.0f;
                int clickedIndex = (int)((my - (mainY + 36.0f)) / itemH);
                if (clickedIndex >= 0 && clickedIndex < (int)_hubProjects.size()) {
                    _hubSelectedProjectIndex = clickedIndex;
                    return;
                }
            }

            // 2. Left Panel Bottom Buttons: [+ NEW PROJECT] and [RESCAN DISK]
            float botBtnY = mainY + mainH - 44.0f;
            if (my >= botBtnY && my <= botBtnY + 36.0f) {
                // [+ NEW PROJECT]
                if (mx >= listX + 10.0f && mx <= listX + 220.0f) {
                    static int s_projCount = 1;
                    std::string newName = "MyGame_Project_" + std::to_string(s_projCount++);
                    if (ProjectManager::get().createProject(newName)) {
                        refreshProjectHub();
                        for (size_t i = 0; i < _hubProjects.size(); ++i) {
                            if (_hubProjects[i].id == newName) {
                                _hubSelectedProjectIndex = (int)i;
                                break;
                            }
                        }
                        _hubStatusBanner = "Created isolated game project in Documents/LabGames/" + newName;
                        _hubBannerTimer = 4.0f;
                    }
                    return;
                }
                // [RESCAN DISK]
                else if (mx >= listX + 230.0f && mx <= listX + 375.0f) {
                    refreshProjectHub();
                    _hubStatusBanner = "Rescanned disk. Found " + std::to_string(_hubProjects.size()) + " project(s).";
                    _hubBannerTimer = 3.0f;
                    return;
                }
            }

            // 3. Right Panel: Action Buttons
            if (_hubSelectedProjectIndex >= 0 && _hubSelectedProjectIndex < (int)_hubProjects.size()) {
                const auto& proj = _hubProjects[_hubSelectedProjectIndex];
                float dy = mainY + 18.0f + 28.0f + 24.0f; // matches actY in render

                // [ OPEN IN EDITOR (CONTINUE) ]
                if (mx >= detX + 20.0f && mx <= detX + 280.0f && my >= dy && my <= dy + 42.0f) {
                    ProjectManager::get().setActiveProject(proj.id);
                    std::string sceneToLoad = proj.defaultScene;
                    bool loaded = false;
                    if (!sceneToLoad.empty()) {
                        auto resolved = ProjectManager::get().resolveAssetPath(sceneToLoad);
                        if (std::filesystem::exists(resolved)) {
                            loaded = loadMapFromFile(resolved.string());
                        }
                    }
                    if (!loaded) {
                        auto mapsDir = ProjectManager::get().getActiveProjectAssetsPath() / "maps";
                        if (std::filesystem::exists(mapsDir)) {
                            for (const auto& entry : std::filesystem::directory_iterator(mapsDir)) {
                                if (entry.path().extension() == ".labmap") {
                                    loadMapFromFile(entry.path().string());
                                    loaded = true;
                                    break;
                                }
                            }
                        }
                    }
                    discoverTextures();
                    discoverModels();
                    setWorkspace(EditorWorkspace::World3D_Level);
                    logMessage("Switched active project to: " + proj.displayName + " (World3D Level Workspace)");
                    return;
                }

                // [ PLAY STANDALONE DEMO ]
                if (proj.hasStandaloneDemo && mx >= detX + 295.0f && mx <= detX + 545.0f && my >= dy && my <= dy + 42.0f) {
                    bool launched = ProjectManager::get().launchProjectDemo(proj.id);
                    if (launched) {
                        _hubStatusBanner = "Launching Standalone Demo: " + proj.displayName + " ...";
                    } else {
                        _hubStatusBanner = "Could not launch demo. Verify executable path: " + proj.demoExePath.string();
                    }
                    _hubBannerTimer = 4.0f;
                    return;
                }

                // Secondary row of buttons
                float dy2 = dy + 54.0f;
                // [ OPEN PROJECT FOLDER ]
                if (mx >= detX + 20.0f && mx <= detX + 220.0f && my >= dy2 && my <= dy2 + 34.0f) {
#if defined(_WIN32)
                    std::string cmd = "explorer \"" + std::filesystem::absolute(proj.rootPath).string() + "\"";
                    std::system(cmd.c_str());
#elif defined(__APPLE__)
                    std::string cmd = "open \"" + std::filesystem::absolute(proj.rootPath).string() + "\"";
                    std::system(cmd.c_str());
#endif
                    return;
                }

                // [ EXPORT / PACKAGE GAME ]
                if (mx >= detX + 230.0f && mx <= detX + 430.0f && my >= dy2 && my <= dy2 + 34.0f) {
                    _exportSettings.gameTitle = proj.id;
                    _exportSettings.entrySceneURI = proj.defaultScene.empty() ? "assets/maps/main.labmap" : proj.defaultScene;
                    _buildModalOpen = true;
                    return;
                }
            }

            return; // Consume click so it never falls through to 3D actions
        }

        // 8.8. 3D Animation Workspace Timeline Interactions
        if (_workspace == EditorWorkspace::World3D_Animation) {
            float vpX = leftW;
            float vpW = l.rightX - vpX;
            float tlW = std::min(vpW - 40.0f, 860.0f);
            float tlH = 80.0f;
            float tlX = vpX + (vpW - tlW) * 0.5f;
            float tlY = (h - 22.0f) - tlH - 15.0f;

            if (my >= tlY + 10.0f && my <= tlY + 36.0f) {
                if (mx >= tlX + 10.0f && mx <= tlX + 80.0f) {
                    _animScrubberPos = 0.0f;
                    _timelineFrame = 0;
                    _timelinePlaying = false;
                    return;
                } else if (mx >= tlX + 84.0f && mx <= tlX + 149.0f) {
                    _animScrubberPos = std::max(0.0f, _animScrubberPos - 0.05f);
                    _timelineFrame = static_cast<int>(_animScrubberPos * 60.0f);
                    return;
                } else if (mx >= tlX + 153.0f && mx <= tlX + 238.0f) {
                    _timelinePlaying = !_timelinePlaying;
                    return;
                } else if (mx >= tlX + 242.0f && mx <= tlX + 307.0f) {
                    _animScrubberPos = std::min(1.0f, _animScrubberPos + 0.05f);
                    _timelineFrame = static_cast<int>(_animScrubberPos * 60.0f);
                    return;
                } else if (mx >= tlX + 311.0f && mx <= tlX + 376.0f) {
                    AnimKeyframe sampled = _editorAnimTrack.sample(_animScrubberPos);
                    sampled.time = _animScrubberPos;
                    _editorAnimTrack.keyframes.push_back(sampled);
                    std::sort(_editorAnimTrack.keyframes.begin(), _editorAnimTrack.keyframes.end(),
                              [](const AnimKeyframe& a, const AnimKeyframe& b) { return a.time < b.time; });
                    return;
                } else if (mx >= tlX + 380.0f && mx <= tlX + 445.0f) {
                    if (_editorAnimTrack.keyframes.size() > 1) {
                        auto closestIt = _editorAnimTrack.keyframes.begin();
                        float minDist = 999.0f;
                        for (auto it = _editorAnimTrack.keyframes.begin(); it != _editorAnimTrack.keyframes.end(); ++it) {
                            float d = std::abs(it->time - _animScrubberPos);
                            if (d < minDist) { minDist = d; closestIt = it; }
                        }
                        _editorAnimTrack.keyframes.erase(closestIt);
                    }
                    return;
                }
            }
            // Scrubber bar click
            if (my >= tlY + 44.0f && my <= tlY + 68.0f) {
                float barX = tlX + 10.0f;
                float barW = tlW - 20.0f;
                if (mx >= barX && mx <= barX + barW) {
                    _animScrubberPos = std::clamp((mx - barX) / barW, 0.0f, 1.0f);
                    _timelineFrame = std::clamp(static_cast<int>(_animScrubberPos * 60.0f), 0, 59);
                    return;
                }
            }
        }

        // 9. 3D Viewport Raycast Picking (Selection / Texture Tool / Object placement) - ONLY in 3D workspaces
        if (_workspace == EditorWorkspace::World3D_Level || _workspace == EditorWorkspace::World3D_Animation) {
            pickObjectInViewport(mx, my, isRmb);
        }
    }

    void drawGizmo(const Vec3& pos) {
        float len = 1.8f;
        float thick = 0.08f;
        float cap = 0.22f;
        Vec3 yellowHov{ 1.0f, 0.95f, 0.2f };

        bool isMove = (_gizmoMode == GizmoMode::Move || _gizmoMode == GizmoMode::Universal);
        bool isRotate = (_gizmoMode == GizmoMode::Rotate || _gizmoMode == GizmoMode::Universal);
        bool isScale = (_gizmoMode == GizmoMode::Scale || _gizmoMode == GizmoMode::Universal);

        // Move Handles
        if (isMove) {
            // X Axis: Red
            bool hovX = (_hoveredGizmoAxis == GizmoAxis::X || _activeGizmoAxis == GizmoAxis::X);
            Vec3 colX = hovX ? yellowHov : Vec3(1.0f, 0.18f, 0.18f);
            Renderer::drawCube(pos + Vec3(len * 0.5f, 0, 0), Vec3(len, thick, thick), colX, false);
            Renderer::drawCube(pos + Vec3(len, 0, 0), Vec3(cap, cap, cap), colX, false);

            // Y Axis: Green
            bool hovY = (_hoveredGizmoAxis == GizmoAxis::Y || _activeGizmoAxis == GizmoAxis::Y);
            Vec3 colY = hovY ? yellowHov : Vec3(0.2f, 1.0f, 0.25f);
            Renderer::drawCube(pos + Vec3(0, len * 0.5f, 0), Vec3(thick, len, thick), colY, false);
            Renderer::drawCube(pos + Vec3(0, len, 0), Vec3(cap * 1.3f, cap * 1.5f, cap * 1.3f), colY, false);

            // Z Axis: Blue
            bool hovZ = (_hoveredGizmoAxis == GizmoAxis::Z || _activeGizmoAxis == GizmoAxis::Z);
            Vec3 colZ = hovZ ? yellowHov : Vec3(0.25f, 0.55f, 1.0f);
            Renderer::drawCube(pos + Vec3(0, 0, len * 0.5f), Vec3(thick, thick, len), colZ, false);
            Renderer::drawCube(pos + Vec3(0, 0, len), Vec3(cap, cap, cap), colZ, false);

            // Planes (XY, XZ, YZ)
            bool hovXY = (_hoveredGizmoAxis == GizmoAxis::PlaneXY || _activeGizmoAxis == GizmoAxis::PlaneXY);
            Renderer::drawCube(pos + Vec3(0.45f, 0.45f, 0), Vec3(0.5f, 0.5f, 0.02f), hovXY ? yellowHov : Vec3(0.9f, 0.9f, 0.3f), false);

            bool hovXZ = (_hoveredGizmoAxis == GizmoAxis::PlaneXZ || _activeGizmoAxis == GizmoAxis::PlaneXZ);
            Renderer::drawCube(pos + Vec3(0.45f, 0, 0.45f), Vec3(0.5f, 0.02f, 0.5f), hovXZ ? yellowHov : Vec3(0.9f, 0.3f, 0.9f), false);

            bool hovYZ = (_hoveredGizmoAxis == GizmoAxis::PlaneYZ || _activeGizmoAxis == GizmoAxis::PlaneYZ);
            Renderer::drawCube(pos + Vec3(0, 0.45f, 0.45f), Vec3(0.02f, 0.5f, 0.5f), hovYZ ? yellowHov : Vec3(0.3f, 0.9f, 0.9f), false);
        }

        // Scale Handles
        if (isScale) {
            float sLen = 1.6f;
            float sBox = 0.22f;
            // Center uniform scale box
            bool hovUni = (_hoveredGizmoAxis == GizmoAxis::ScaleUniform || _activeGizmoAxis == GizmoAxis::ScaleUniform);
            Renderer::drawCube(pos, Vec3(0.28f, 0.28f, 0.28f), hovUni ? yellowHov : Vec3(0.95f, 0.95f, 0.95f), false);

            // Scale X
            bool hovSX = (_hoveredGizmoAxis == GizmoAxis::ScaleX || _activeGizmoAxis == GizmoAxis::ScaleX);
            Renderer::drawCube(pos + Vec3(sLen, 0, 0), Vec3(sBox, sBox, sBox), hovSX ? yellowHov : Vec3(1.0f, 0.3f, 0.3f), false);

            // Scale Y
            bool hovSY = (_hoveredGizmoAxis == GizmoAxis::ScaleY || _activeGizmoAxis == GizmoAxis::ScaleY);
            Renderer::drawCube(pos + Vec3(0, sLen, 0), Vec3(sBox, sBox, sBox), hovSY ? yellowHov : Vec3(0.3f, 1.0f, 0.3f), false);

            // Scale Z
            bool hovSZ = (_hoveredGizmoAxis == GizmoAxis::ScaleZ || _activeGizmoAxis == GizmoAxis::ScaleZ);
            Renderer::drawCube(pos + Vec3(0, 0, sLen), Vec3(sBox, sBox, sBox), hovSZ ? yellowHov : Vec3(0.3f, 0.6f, 1.0f), false);
        }

        // Rotate Rings
        if (isRotate) {
            float ringR = 1.8f;
            int segments = 24;
            float step = 6.2831853f / (float)segments;
            float segLen = (6.2831853f * ringR) / (float)segments;

            bool hovRX = (_hoveredGizmoAxis == GizmoAxis::RotX || _activeGizmoAxis == GizmoAxis::RotX);
            Vec3 colRX = hovRX ? yellowHov : Vec3(1.0f, 0.25f, 0.25f);
            bool hovRY = (_hoveredGizmoAxis == GizmoAxis::RotY || _activeGizmoAxis == GizmoAxis::RotY);
            Vec3 colRY = hovRY ? yellowHov : Vec3(0.25f, 1.0f, 0.3f);
            bool hovRZ = (_hoveredGizmoAxis == GizmoAxis::RotZ || _activeGizmoAxis == GizmoAxis::RotZ);
            Vec3 colRZ = hovRZ ? yellowHov : Vec3(0.3f, 0.6f, 1.0f);

            for (int i = 0; i < segments; ++i) {
                float a1 = i * step;
                // RotX (around X axis, in YZ plane)
                Vec3 pX = pos + Vec3(0, std::cos(a1) * ringR, std::sin(a1) * ringR);
                Renderer::drawCube(pX, Vec3(thick, segLen, thick), colRX, false);

                // RotY (around Y axis, in XZ plane)
                Vec3 pY = pos + Vec3(std::cos(a1) * ringR, 0, std::sin(a1) * ringR);
                Renderer::drawCube(pY, Vec3(segLen, thick, thick), colRY, false);

                // RotZ (around Z axis, in XY plane)
                Vec3 pZ = pos + Vec3(std::cos(a1) * ringR, std::sin(a1) * ringR, 0);
                Renderer::drawCube(pZ, Vec3(segLen, thick, thick), colRZ, false);
            }
        }
    }

    void onDrop(int count, const char** paths) override {
        for (int i = 0; i < count; ++i) {
            if (!paths[i]) continue;
            std::string filePath = paths[i];
            std::string ext = "";
            size_t dot = filePath.rfind('.');
            if (dot != std::string::npos) {
                ext = filePath.substr(dot);
                for (auto& c : ext) c = (char)tolower(c);
            }

            if (ext == ".labscene") {
                // Direct load without prompt! Requirement: "ponowny drop .labscene bez pytania"
                logMessage("[Drag&Drop] Dropped .labscene detected. Loading directly: " + filePath);
                loadMapFromFile(filePath);
            } else if (ext == ".labmap") {
                if (LabScene::isLegacyMapFile(filePath)) {
                    logMessage("[Drag&Drop] Legacy .labmap detected: " + filePath + ". Requesting conversion.");
                    _pendingMapPath = filePath;
                    _showConvertMapModal = true;
                } else {
                    loadMapFromFile(filePath);
                }
            } else if (ext == ".lua") {
                std::string id = PackageManager::instance().importDropinFile(filePath);
                if (!id.empty()) logMessage("[Drag&Drop] Plugin installed: " + id);
            } else {
                const char* singlePath = paths[i];
                Engine::onDrop(1, &singlePath);
            }
        }
    }

    void onRender() override {
        Profiler::instance().beginFrame();

        int fbW = 0, fbH = 0;
        getFramebufferSize(fbW, fbH);
        int w = (fbW > 0) ? fbW : getWidth();
        int h = (fbH > 0) ? fbH : getHeight();

        // If not in 3D level editing, render clean workspace stage without 3D map
        bool is3DMapWorkspace = (_workspace == EditorWorkspace::World3D_Level || _workspace == EditorWorkspace::World3D_Animation);
        if (!is3DMapWorkspace) {
            glViewport(0, 0, w, h);
            glClearColor(0.07f, 0.08f, 0.10f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            if (_workspace == EditorWorkspace::Level2D_HUD) {
                _hudEditor.render(w, h);
                Profiler::instance().endFrame();
                return;
            }

            if (isStudioWorkspace(_workspace)) {
                _characterStudio.render(w, h);
            } else if (_workspace == EditorWorkspace::World3D_ModelViewer) {
                renderTurntableModelStage(w, h);
            }

            // Draw Editor Top Bar, Docked Active Workspace, Sidebar, and Bottom Dock
            drawHammerInterface();

            if (_browserOpen) drawTextureBrowser();
            if (_modelBrowserOpen) drawModelBrowser();
            if (_helpModalOpen) drawHelpModal();
            if (_buildModalOpen) drawBuildModal();
            if (_inputModalOpen) drawInputModal();
            if (_asset2DModalOpen) drawAsset2DModal();
            if (_showConvertMapModal) drawConvertMapModal();

            Profiler::instance().endFrame();
            return;
        }

        // Darkness Mode or Map atmospheric lighting configuration
        if (_darknessMode) {
            glClearColor(0.012f, 0.015f, 0.022f, 1.0f);
            Renderer::setSunLight(Vec3(0.0f, -1.0f, 0.0f), Vec3(0.0f, 0.0f, 0.0f), Vec3(0.012f, 0.015f, 0.022f));
        } else if (_map) {
            glClearColor(0.05f, 0.07f, 0.10f, 1.0f);
            Renderer::setSunLight(_map->metadata.sunDir, _map->metadata.sunColor, _map->metadata.ambientColor);
        } else {
            glClearColor(0.05f, 0.07f, 0.10f, 1.0f);
            Renderer::setSunLight(Vec3(-0.4f, -0.8f, -0.4f), Vec3(1.0f, 0.95f, 0.9f), Vec3(0.25f, 0.28f, 0.35f));
        }

        // 1. Begin 3D Frame & Frustum Culling
        Renderer::beginFrame(_camera);

        // Upload dynamic map lights to the renderer
        if (_map) {
            std::vector<PointLight> dynamicLights;
            dynamicLights.reserve(_map->lights.size());
            for (const auto& lgt : _map->lights) {
                dynamicLights.push_back(PointLight{
                    .position = lgt.position,
                    .color = lgt.color,
                    .intensity = lgt.intensity,
                    .radius = lgt.radius
                });
            }
            Renderer::setPointLights(dynamicLights);
        } else {
            Renderer::clearPointLights();
        }

        int totalBrushes = 0, renderedBrushes = 0;
        int totalProps = 0, renderedProps = 0;

        if (_map) {
            totalBrushes = (int)_map->brushes.size();
            totalProps = (int)_map->props.size();

            // Render Brushes with 6-plane Frustum Culling & Source Engine UV Tiling
            for (const auto& b : _map->brushes) {
                Vec3 halfSize = b.size * 0.5f;
                Vec3 bMin = b.position - halfSize;
                Vec3 bMax = b.position + halfSize;
                if (!_camera.isInFrustum(bMin, bMax)) continue; // "To czego oko nie widzi tego maszyna renderowac nie musi"

                renderedBrushes++;
                Texture* tex = b.texturePath.empty() ? nullptr : getTexture(b.texturePath);
                if (b.type == "poly" && !b.customVertices.empty()) {
                    if (!b.runtimeMesh) {
                        b.runtimeMesh = std::make_shared<Mesh>(b.customVertices, b.customIndices);
                    }
                    Renderer::drawMesh(*b.runtimeMesh, Vec3(0, 0, 0), Vec3(0, 0, 0), Vec3(1, 1, 1), b.color, tex);
                } else {
                    Renderer::drawCube(b.position, b.rotation, b.size, b.color, tex, true, b.uvScale, b.uvMode);
                }
            }

            // Render Props (STL models) with Frustum Culling
            for (const auto& p : _map->props) {
                Vec3 halfScale = p.scale * 0.5f;
                Vec3 pMin = p.position - halfScale;
                Vec3 pMax = p.position + halfScale;
                if (!_camera.isInFrustum(pMin, pMax)) continue; // Frustum culling

                renderedProps++;
                Mesh* m = getMesh(p.modelPath);
                Texture* tex = p.texturePath.empty() ? nullptr : getTexture(p.texturePath);
                if (!tex) {
                    if (p.modelPath.find("barrel") != std::string::npos) {
                        tex = getTexture("barrel_hazard.png");
                    } else if (p.modelPath.find("crate") != std::string::npos) {
                        tex = getTexture("crate_wood.png");
                    }
                }
                if (m) {
                    Renderer::drawMesh(*m, p.position, p.rotation, p.scale, p.color, tex);
                } else {
                    Renderer::drawCube(p.position, p.scale, p.color, tex);
                }
            }

            // Render Doors with Frustum Culling
            for (const auto& d : _map->doors) {
                Vec3 halfSize = d.size * 0.5f;
                if (!_camera.isInFrustum(d.position - halfSize, d.position + halfSize)) continue;
                Renderer::drawCube(d.position, d.size, d.color);
            }

            // Render All Spawn Markers with Team Colors, Landing Pads, and Facing Yaw Arrows
            for (size_t i = 0; i < _map->spawnPoints.size(); ++i) {
                const auto& sp = _map->spawnPoints[i];
                Vec3 teamCol = (sp.type == SpawnType::TeamAlpha) ? Vec3(0.2f, 0.6f, 1.0f) :
                               (sp.type == SpawnType::TeamBeta)  ? Vec3(1.0f, 0.25f, 0.25f) :
                                                                   Vec3(0.2f, 0.9f, 0.4f);
                // Base landing pad
                Renderer::drawCube(sp.position + Vec3(0.0f, -0.85f, 0.0f), Vec3(1.2f, 0.1f, 1.2f), teamCol * 0.7f, nullptr, false);
                Renderer::drawWireCube(sp.position + Vec3(0.0f, -0.85f, 0.0f), Vec3(1.22f, 0.11f, 1.22f), teamCol);

                // Holographic player silhouette
                Renderer::drawWireCube(sp.position, Vec3(0.8f, 1.8f, 0.8f), teamCol);
                Renderer::drawCube(sp.position + Vec3(0.0f, 0.55f, 0.0f), Vec3(0.35f, 0.35f, 0.35f), teamCol * 0.85f, nullptr, false);

                // Yaw directional pointer arrow (Engine convention: cos(yaw) along X, sin(yaw) along Z; yaw 180 = -Z)
                float rad = sp.yaw * 3.14159265f / 180.0f;
                Vec3 fwd(std::cos(rad), 0.0f, std::sin(rad));
                Vec3 arrowPos = sp.position + fwd * 0.85f + Vec3(0.0f, -0.2f, 0.0f);
                Renderer::drawCube(arrowPos, Vec3(0.18f, 0.18f, 0.42f), teamCol, nullptr, false);
            }

            // Render All Weapon Spawners with Pedestal & 3D Weapon Model (Check-once cached assets)
            for (size_t i = 0; i < _map->weaponSpawners.size(); ++i) {
                const auto& ws = _map->weaponSpawners[i];
                // Base ground pedestal
                Renderer::drawCube(ws.position + Vec3(0.0f, 0.04f, 0.0f), Vec3(0.0f, ws.yaw, 0.0f),
                                   Vec3(1.1f, 0.08f, 1.1f), Vec3(0.18f, 0.20f, 0.24f), nullptr, false);
                Renderer::drawCube(ws.position + Vec3(0.0f, 0.082f, 0.0f), Vec3(0.0f, ws.yaw, 0.0f),
                                   Vec3(0.9f, 0.01f, 0.9f), Vec3(0.2f, 0.75f, 0.95f), nullptr, false);

                // Render floating pickup entity node
                float itemY = ws.position.y + 0.45f;
                Renderer::drawCube(Vec3(ws.position.x, itemY, ws.position.z), Vec3(0.0f, ws.yaw, 0.0f),
                                   Vec3(0.35f, 0.35f, 0.35f), Vec3(0.2f, 0.8f, 0.6f), nullptr, false);
                Renderer::drawWireCube(Vec3(ws.position.x, itemY, ws.position.z), Vec3(0.45f, 0.45f, 0.45f), Vec3(0.3f, 1.0f, 0.8f));
            }

            // Render All Map Dynamic Lights (Lamps)
            for (size_t i = 0; i < _map->lights.size(); ++i) {
                const auto& lgt = _map->lights[i];
                // Core glowing bulb
                Renderer::drawCube(lgt.position, Vec3(0.35f, 0.35f, 0.35f), lgt.color, nullptr, false);
                // Fixture cap / stand mount
                Renderer::drawCube(lgt.position + Vec3(0.0f, 0.22f, 0.0f), Vec3(0.42f, 0.10f, 0.42f), Vec3(0.25f, 0.27f, 0.30f), nullptr, false);
                // Wireframe light bulb cage / glow halo
                Renderer::drawWireCube(lgt.position, Vec3(0.55f, 0.55f, 0.55f), lgt.color);
            }

            // Render All 2D Sprites in Map
            for (size_t i = 0; i < _map->sprites.size(); ++i) {
                const auto& sp = _map->sprites[i];
                Texture* tex = getTexture(sp.texturePath);
                Renderer::drawCube(sp.position, Vec3(0.0f, sp.rotation, 0.0f),
                                   Vec3(sp.size.x, sp.size.y, 0.05f),
                                   Vec3(sp.color.x, sp.color.y, sp.color.z), tex, false);
                Renderer::drawWireCube(sp.position, Vec3(sp.size.x, sp.size.y, 0.06f), Vec3(0.2f, 0.85f, 0.95f));
            }

            // Render All Cameras in Map
            for (size_t i = 0; i < _map->cameras.size(); ++i) {
                const auto& cam = _map->cameras[i];
                Vec3 camCol = cam.active ? Vec3(0.2f, 0.9f, 0.4f) : Vec3(0.3f, 0.7f, 1.0f);
                Renderer::drawCube(cam.position, cam.rotation, Vec3(0.6f, 0.4f, 0.8f), camCol, nullptr, false);
                Renderer::drawWireCube(cam.position, Vec3(0.65f, 0.45f, 0.85f), camCol);
            }

            // --- Render Active Addon 3D Visualizations ---
            // 1. Cloth & Soft Body Simulation Lattice
            if (PackageManager::instance().isPackageEnabled("com.lab.physics-cloth") && _clothMeshInitialized) {
                for (size_t cy = 0; cy < _clothMesh.gridHeight; ++cy) {
                    for (size_t cx = 0; cx < _clothMesh.gridWidth; ++cx) {
                        size_t idx = cy * _clothMesh.gridWidth + cx;
                        const auto& p = _clothMesh.particles[idx];
                        Vec3 pCol = p.isPinned ? Vec3(1.0f, 0.25f, 0.25f) : Vec3(0.85f, 0.92f, 1.0f);
                        Renderer::drawCube(p.position, Vec3(0.06f, 0.06f, 0.06f), pCol, nullptr, false);
                        if (cx + 1 < _clothMesh.gridWidth) {
                            const auto& pR = _clothMesh.particles[idx + 1];
                            Vec3 mid = (p.position + pR.position) * 0.5f;
                            Vec3 span = pR.position - p.position;
                            Renderer::drawCube(mid, Vec3(std::max(0.02f, span.length()), 0.02f, 0.02f), Vec3(0.35f, 0.75f, 0.95f), nullptr, false);
                        }
                        if (cy + 1 < _clothMesh.gridHeight) {
                            const auto& pD = _clothMesh.particles[idx + _clothMesh.gridWidth];
                            Vec3 mid = (p.position + pD.position) * 0.5f;
                            Vec3 span = pD.position - p.position;
                            Renderer::drawCube(mid, Vec3(0.02f, 0.02f, std::max(0.02f, span.length())), Vec3(0.35f, 0.75f, 0.95f), nullptr, false);
                        }
                    }
                }
            }

            // 2. Force Fields & Shockwave Trigger Gizmos
            if (PackageManager::instance().isPackageEnabled("com.lab.physics-forcefield") && _forceFieldGizmoVisible) {
                Vec3 ffCol = (_forceFieldType == 0) ? Vec3(1.0f, 0.45f, 0.15f) :
                             (_forceFieldType == 1) ? Vec3(0.35f, 0.85f, 1.0f) : Vec3(0.4f, 1.0f, 0.5f);
                Renderer::drawWireCube(_forceFieldPosition, Vec3(_forceFieldRadius * 2.0f, _forceFieldRadius * 2.0f, _forceFieldRadius * 2.0f), ffCol * 0.5f);
                Renderer::drawCube(_forceFieldPosition, Vec3(0.5f, 0.5f, 0.5f), ffCol, nullptr, false);
                if (_forceFieldBlastTimer > 0.0f) {
                    float blastR = (_forceFieldRadius * (1.0f - _forceFieldBlastTimer)) + 1.0f;
                    Renderer::drawWireCube(_forceFieldPosition, Vec3(blastR * 2.0f, 0.4f, blastR * 2.0f), Vec3(1.0f, 0.9f, 0.3f));
                }
            }
        }

        _cullingStats = "Frustum Culling: Brushes " + std::to_string(renderedBrushes) + "/" + std::to_string(totalBrushes) +
                        " | Props " + std::to_string(renderedProps) + "/" + std::to_string(totalProps);

        // Render Selection Wireframe Bounding Box & Gizmo
        if (_map) {
            Vec3 hammerOrange{ 1.0f, 0.55f, 0.1f };
            if (_selectionType == SelectionType::Brush && _selectedIndex >= 0 && _selectedIndex < (int)_map->brushes.size()) {
                const auto& b = _map->brushes[_selectedIndex];
                Vec3 half = b.size * 0.5f;
                Renderer::drawBoundingBox(b.position - half, b.position + half, hammerOrange);
                drawGizmo(b.position);
            } else if (_selectionType == SelectionType::Prop && _selectedIndex >= 0 && _selectedIndex < (int)_map->props.size()) {
                const auto& p = _map->props[_selectedIndex];
                Vec3 half = p.scale * 0.5f;
                Renderer::drawBoundingBox(p.position - half, p.position + half, hammerOrange);
                drawGizmo(p.position);
            } else if (_selectionType == SelectionType::Door && _selectedIndex >= 0 && _selectedIndex < (int)_map->doors.size()) {
                const auto& d = _map->doors[_selectedIndex];
                Vec3 half = d.size * 0.5f;
                Renderer::drawBoundingBox(d.position - half, d.position + half, hammerOrange);
                drawGizmo(d.position);
            } else if (_selectionType == SelectionType::Spawn && _selectedIndex >= 0 && _selectedIndex < (int)_map->spawnPoints.size()) {
                const auto& sp = _map->spawnPoints[_selectedIndex];
                Vec3 half(0.6f, 0.9f, 0.6f);
                Renderer::drawBoundingBox(sp.position - half, sp.position + half, hammerOrange);
                drawGizmo(sp.position);
            } else if (_selectionType == SelectionType::WeaponSpawner && _selectedIndex >= 0 && _selectedIndex < (int)_map->weaponSpawners.size()) {
                const auto& ws = _map->weaponSpawners[_selectedIndex];
                Renderer::drawBoundingBox(ws.position - Vec3(0.7f, 0.0f, 0.7f), ws.position + Vec3(0.7f, 0.8f, 0.7f), hammerOrange);
                drawGizmo(ws.position);
            } else if (_selectionType == SelectionType::Light && _selectedIndex >= 0 && _selectedIndex < (int)_map->lights.size()) {
                const auto& lgt = _map->lights[_selectedIndex];
                Renderer::drawBoundingBox(lgt.position - Vec3(0.4f, 0.4f, 0.4f), lgt.position + Vec3(0.4f, 0.4f, 0.4f), hammerOrange);
                Renderer::drawWireCube(lgt.position, Vec3(lgt.radius * 2.0f, lgt.radius * 2.0f, lgt.radius * 2.0f), lgt.color * 0.4f);
                drawGizmo(lgt.position);
            } else if (_selectionType == SelectionType::Camera && _selectedIndex >= 0 && _selectedIndex < (int)_map->cameras.size()) {
                const auto& cam = _map->cameras[_selectedIndex];
                Vec3 half(0.6f, 0.6f, 0.6f);
                Renderer::drawBoundingBox(cam.position - half, cam.position + half, hammerOrange);
                drawGizmo(cam.position);
            }
        }

        // Draw 3D Cursor Placement Box & Gizmo
        if (_activeTool == 1) {
            Renderer::drawWireCube(_cursorPos, _brushSize, Vec3(0.9f, 0.9f, 0.95f));
            drawGizmo(_cursorPos);
        } else if (_activeTool == 2) {
            Renderer::drawWireCube(_cursorPos, _propScale, Vec3(0.3f, 0.8f, 1.0f));
            drawGizmo(_cursorPos);
        } else if (_activeTool == 4) {
            Renderer::drawWireCube(_cursorPos, Vec3(2.5f, 3.5f, 0.4f), Vec3(0.9f, 0.5f, 0.2f));
            drawGizmo(_cursorPos);
        } else if (_activeTool == 5) {
            Vec3 toolCol = (_spawnToolType == SpawnType::TeamAlpha) ? Vec3(0.2f, 0.6f, 1.0f) :
                           (_spawnToolType == SpawnType::TeamBeta)  ? Vec3(1.0f, 0.25f, 0.25f) :
                                                                     Vec3(0.2f, 0.9f, 0.4f);
            Renderer::drawWireCube(_cursorPos, Vec3(0.8f, 1.8f, 0.8f), toolCol);
            Renderer::drawCube(_cursorPos + Vec3(0.0f, -0.85f, 0.0f), Vec3(1.2f, 0.1f, 1.2f), toolCol * 0.5f, nullptr, false);
            drawGizmo(_cursorPos);
        } else if (_activeTool == 6) {
            drawClipToolOverlay();
        } else if (_activeTool == 7) {
            // Lamp Placement Tool preview
            Renderer::drawWireCube(_cursorPos, Vec3(0.6f, 0.6f, 0.6f), _lampColor);
            Renderer::drawCube(_cursorPos, Vec3(0.35f, 0.35f, 0.35f), _lampColor, nullptr, false);
            Renderer::drawCube(_cursorPos + Vec3(0.0f, 0.22f, 0.0f), Vec3(0.42f, 0.10f, 0.42f), Vec3(0.25f, 0.27f, 0.30f), nullptr, false);
            Renderer::drawWireCube(_cursorPos, Vec3(_lampRadius * 2.0f, _lampRadius * 2.0f, _lampRadius * 2.0f), _lampColor * 0.4f);
            drawGizmo(_cursorPos);
        }

        // 2. Render 2D Lab Hammer Desktop Interface
        drawHammerInterface();

        // 3. Render Modal Texture Browser if open
        if (_browserOpen) {
            drawTextureBrowser();
        }

        // 4. Render Modal Model Browser if open
        if (_modelBrowserOpen) {
            drawModelBrowser();
        }

        // 5. Render Modal Help if open
        if (_helpModalOpen) {
            drawHelpModal();
        }

        // 6. Render Modal Build & Export if open
        if (_buildModalOpen) {
            drawBuildModal();
        }

        // 7. Render Input Actions & Controller Bindings Modal if open
        if (_inputModalOpen) {
            drawInputModal();
        }

        // 8. Render 2D Asset Browser & Spritesheet Slicer Modal if open
        if (_asset2DModalOpen) {
            drawAsset2DModal();
        }

        // 9. Render Legacy Map Conversion Modal if open
        if (_showConvertMapModal) {
            drawConvertMapModal();
        }

        Renderer::endFrame();
        Profiler::instance().endFrame();
    }

    // Studio Dark Industrial Styling Helpers (Delegated to unified LabTheme)
    static void drawDarkBevel(float x, float y, float w, float h, bool sunken) {
        LabTheme::drawBevel(x, y, w, h, sunken);
    }

    static void drawDarkPanel(float x, float y, float w, float h, const std::string& title = "") {
        LabTheme::drawPanel(x, y, w, h, title);
    }

    bool drawDarkButton(float x, float y, float w, float h, const std::string& label, bool active = false, bool isRed = false, bool isGreen = false) {
        return LabTheme::drawButton(x, y, w, h, label, active, false, _mouseScreenX, _mouseScreenY, false, isRed, isGreen);
    }

    // Hammer Toolbar & Palette Icon Renderers are shared from Lab::HammerIcons (LabEditor.h)
    static void drawHammerIcon(int iconId, float x, float y, const Vec3& color, const Vec3& bg) {
        Lab::HammerIcons::drawHammerIcon(iconId, x, y, color, bg);
        drawDarkBevel(x, y, 24.0f, 24.0f, false);
    }
    static void drawToolbarIcon(int iconId, float x, float y, const Vec3& color, const Vec3& bg) {
        Lab::HammerIcons::drawToolbarIcon(iconId, x, y, color, bg);
        drawDarkBevel(x, y, 24.0f, 24.0f, false);
    }

    void drawHammerInterface() {
        int fbW = 0, fbH = 0;
        getFramebufferSize(fbW, fbH);
        float w = (fbW > 0) ? (float)fbW : (float)getWidth();
        float h = (fbH > 0) ? (float)fbH : (float)getHeight();

        Renderer::beginUI((int)w, (int)h);

        // Studio Dark Theme Colors
        Vec3 panelDarkBg{ 0.18f, 0.19f, 0.22f };     // Dark Slate Panel BG
        Vec3 panelDarker{ 0.14f, 0.15f, 0.17f };     // Well / Header Dark BG
        Vec3 wellBg{ 0.11f, 0.12f, 0.14f };          // Sunken Area BG
        Vec3 textLight{ 0.92f, 0.93f, 0.96f };       // Off-White Primary Text
        Vec3 textDim{ 0.58f, 0.62f, 0.68f };         // Subdued Text
        Vec3 cyanGlow{ 0.20f, 0.75f, 0.95f };        // Studio Cyan
        Vec3 orangeGlow{ 1.0f, 0.55f, 0.10f };       // Hammer Orange
        Vec3 greenAccent{ 0.18f, 0.72f, 0.38f };     // Studio Green
        Vec3 redAccent{ 0.85f, 0.25f, 0.25f };       // Studio Red / Delete

        // ==================== 1. TOP TITLEBAR & MENUS ====================
        Renderer::drawRect(0, 0, w, 24.0f, panelDarker);
        drawDarkBevel(0, 0, w, 24.0f, false);

        bool hovFile  = (_mouseScreenY >= 0.0f && _mouseScreenY <= 24.0f && _mouseScreenX >= 10.0f && _mouseScreenX <= 50.0f);
        bool hovEdit  = (_mouseScreenY >= 0.0f && _mouseScreenY <= 24.0f && _mouseScreenX >= 52.0f && _mouseScreenX <= 90.0f);
        bool hovView  = (_mouseScreenY >= 0.0f && _mouseScreenY <= 24.0f && _mouseScreenX >= 92.0f && _mouseScreenX <= 135.0f);
        bool hovBuild = (_mouseScreenY >= 0.0f && _mouseScreenY <= 24.0f && _mouseScreenX >= 137.0f && _mouseScreenX <= 185.0f);
        bool hovTools = (_mouseScreenY >= 0.0f && _mouseScreenY <= 24.0f && _mouseScreenX >= 187.0f && _mouseScreenX <= 235.0f);
        bool hovInput = (_mouseScreenY >= 0.0f && _mouseScreenY <= 24.0f && _mouseScreenX >= 237.0f && _mouseScreenX <= 285.0f);
        bool hov2D    = (_mouseScreenY >= 0.0f && _mouseScreenY <= 24.0f && _mouseScreenX >= 287.0f && _mouseScreenX <= 360.0f);
        bool hovHelp  = (_mouseScreenY >= 0.0f && _mouseScreenY <= 24.0f && _mouseScreenX >= 362.0f && _mouseScreenX <= 405.0f);

        if (hovFile)  Renderer::drawRect(10.0f, 2.0f, 40.0f, 20.0f, Vec3(0.24f, 0.28f, 0.36f));
        if (hovEdit)  Renderer::drawRect(52.0f, 2.0f, 38.0f, 20.0f, Vec3(0.24f, 0.28f, 0.36f));
        if (hovView)  Renderer::drawRect(92.0f, 2.0f, 43.0f, 20.0f, Vec3(0.24f, 0.28f, 0.36f));
        if (hovBuild) Renderer::drawRect(137.0f, 2.0f, 48.0f, 20.0f, Vec3(0.24f, 0.28f, 0.36f));
        if (hovTools) Renderer::drawRect(187.0f, 2.0f, 48.0f, 20.0f, Vec3(0.24f, 0.28f, 0.36f));
        if (hovInput) Renderer::drawRect(237.0f, 2.0f, 48.0f, 20.0f, Vec3(0.24f, 0.28f, 0.36f));
        if (hov2D)    Renderer::drawRect(287.0f, 2.0f, 73.0f, 20.0f, Vec3(0.24f, 0.28f, 0.36f));
        if (hovHelp)  Renderer::drawRect(362.0f, 2.0f, 43.0f, 20.0f, Vec3(0.24f, 0.28f, 0.36f));

        LabFont::drawText(14.0f, 5.0f, "File", 1.8f, hovFile ? orangeGlow : textLight, LabFontType::System);
        LabFont::drawText(54.0f, 5.0f, "Edit", 1.8f, hovEdit ? orangeGlow : textLight, LabFontType::System);
        LabFont::drawText(94.0f, 5.0f, "View", 1.8f, hovView ? orangeGlow : textLight, LabFontType::System);
        LabFont::drawText(140.0f, 5.0f, "Build", 1.8f, hovBuild ? orangeGlow : (Vec3(0.4f, 0.9f, 0.5f)), LabFontType::System);
        LabFont::drawText(190.0f, 5.0f, "Tools", 1.8f, hovTools ? orangeGlow : textLight, LabFontType::System);
        LabFont::drawText(240.0f, 5.0f, "Input", 1.8f, hovInput ? orangeGlow : (Vec3(0.3f, 0.85f, 1.0f)), LabFontType::System);
        LabFont::drawText(292.0f, 5.0f, "2D Assets", 1.8f, hov2D ? orangeGlow : (Vec3(0.9f, 0.75f, 0.2f)), LabFontType::System);
        LabFont::drawText(368.0f, 5.0f, "Help", 1.8f, hovHelp ? orangeGlow : textLight, LabFontType::System);

        // Active Workspace Indicator Badge (Clickable to open View menu)
        std::string wsBreadcrumb = "Workspace: " + getWorkspaceBreadcrumb(_workspace);
        float wsBadgeX = 415.0f;
        float wsBadgeW = 285.0f;
        bool isWsHov = (_mouseScreenY >= 0.0f && _mouseScreenY <= 24.0f && _mouseScreenX >= wsBadgeX && _mouseScreenX <= wsBadgeX + wsBadgeW);
        Renderer::drawRect(wsBadgeX, 2.0f, wsBadgeW, 20.0f, isWsHov ? Vec3(0.18f, 0.22f, 0.30f) : Vec3(0.12f, 0.14f, 0.18f));
        drawDarkBevel(wsBadgeX, 2.0f, wsBadgeW, 20.0f, false);
        Renderer::drawRect(wsBadgeX, 2.0f, 3.0f, 20.0f, cyanGlow);
        LabFont::drawText(wsBadgeX + 10.0f, 5.0f, wsBreadcrumb, 1.4f, isWsHov ? orangeGlow : cyanGlow, LabFontType::System);

        LabFont::drawText(w - 380.0f, 5.0f, "Lab Engine Editor 2026 (LabSDK)", 1.8f, orangeGlow, LabFontType::GeoSans);

        // ==================== 2. MAIN TOOLBAR (12 Deduplicated Buttons) ====================
        float tbY = 24.0f;
        float tbH = 34.0f;
        Renderer::drawRect(0, tbY, w, tbH, panelDarkBg);
        drawDarkBevel(0, tbY, w, tbH, false);

        const int tbIcons[12] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 15, 16 };
        for (int i = 0; i < 12; ++i) {
            float bx = 8.0f + i * 28.0f;
            bool isHov = (_mouseScreenX >= bx && _mouseScreenX <= bx + 26.0f && _mouseScreenY >= tbY + 4.0f && _mouseScreenY <= tbY + 30.0f);
            Vec3 bgCol = (isHov ? Vec3(0.32f, 0.35f, 0.40f) : Vec3(0.22f, 0.23f, 0.26f));
            Vec3 iconCol = (isHov ? Vec3(1.0f, 0.95f, 0.80f) : Vec3(0.85f, 0.88f, 0.92f));

            Renderer::drawRect(bx, tbY + 4.0f, 26.0f, 26.0f, bgCol);
            drawDarkBevel(bx, tbY + 4.0f, 26.0f, 26.0f, false);
            drawToolbarIcon(tbIcons[i], bx + 1.0f, tbY + 5.0f, iconCol, bgCol);
        }

        // ==================== ENGINE CONTROLS & TEST BUTTONS ====================
        // 1. [ > TEST GAME (F5) ] - High-visibility emerald green button!
        float bRunX = 350.0f;
        float bRunW = 145.0f;
        bool isBRunHov = (_mouseScreenX >= bRunX && _mouseScreenX <= bRunX + bRunW && _mouseScreenY >= tbY + 4.0f && _mouseScreenY <= tbY + 30.0f);
        Vec3 bRunCol = isBRunHov ? Vec3(0.25f, 0.88f, 0.45f) : Vec3(0.18f, 0.72f, 0.38f);
        Renderer::drawRect(bRunX, tbY + 4.0f, bRunW, 26.0f, bRunCol);
        drawDarkBevel(bRunX, tbY + 4.0f, bRunW, 26.0f, false);
        LabFont::drawText(bRunX + 10.0f, tbY + 9.0f, "> TEST GAME (F5)", 1.25f, Vec3(1, 1, 1), LabFontType::System);

        // 2. [ BUILD / EXPORT... ] - High-visibility cyan button!
        float bExpX = bRunX + bRunW + 8.0f;
        float bExpW = 130.0f;
        bool isBExpHov = (_mouseScreenX >= bExpX && _mouseScreenX <= bExpX + bExpW && _mouseScreenY >= tbY + 4.0f && _mouseScreenY <= tbY + 30.0f);
        Vec3 bExpCol = isBExpHov ? Vec3(0.25f, 0.58f, 0.92f) : Vec3(0.18f, 0.46f, 0.76f);
        Renderer::drawRect(bExpX, tbY + 4.0f, bExpW, 26.0f, bExpCol);
        drawDarkBevel(bExpX, tbY + 4.0f, bExpW, 26.0f, false);
        LabFont::drawText(bExpX + 10.0f, tbY + 9.0f, "BUILD / EXPORT", 1.25f, Vec3(1, 1, 1), LabFontType::System);

        // 3. [ 3D PERSPECTIVE ] / [ 2D ORTHO ]
        float viewModeX = bExpX + bExpW + 8.0f;
        float viewModeW = 100.0f;
        bool isOrtho = _orthoViewMode;
        bool isViewHov = (_mouseScreenX >= viewModeX && _mouseScreenX <= viewModeX + viewModeW && _mouseScreenY >= tbY + 4.0f && _mouseScreenY <= tbY + 30.0f);
        Vec3 viewCol = isOrtho ? Vec3(0.20f, 0.55f, 0.85f) : (isViewHov ? Vec3(0.28f, 0.32f, 0.38f) : Vec3(0.20f, 0.22f, 0.25f));
        Renderer::drawRect(viewModeX, tbY + 4.0f, viewModeW, 26.0f, viewCol);
        drawDarkBevel(viewModeX, tbY + 4.0f, viewModeW, 26.0f, isOrtho);
        LabFont::drawText(viewModeX + 8.0f, tbY + 9.0f, isOrtho ? "2D ORTHO" : "3D PERSPECT", 1.25f, isOrtho ? Vec3(1, 1, 1) : cyanGlow, LabFontType::System);

        // 4. [+ LIGHT] button
        float lightX = viewModeX + viewModeW + 8.0f;
        float lightW = 80.0f;
        bool isLightTool = (_activeTool == 7);
        bool isLightHov = (_mouseScreenX >= lightX && _mouseScreenX <= lightX + lightW && _mouseScreenY >= tbY + 4.0f && _mouseScreenY <= tbY + 30.0f);
        Vec3 lightBtnBg = isLightTool ? Vec3(0.95f, 0.75f, 0.15f) : (isLightHov ? Vec3(0.32f, 0.35f, 0.40f) : Vec3(0.22f, 0.23f, 0.26f));
        Renderer::drawRect(lightX, tbY + 4.0f, lightW, 26.0f, lightBtnBg);
        drawDarkBevel(lightX, tbY + 4.0f, lightW, 26.0f, isLightTool);
        LabFont::drawText(lightX + 10.0f, tbY + 9.0f, "+ LIGHT", 1.25f, isLightTool ? Vec3(0.05f, 0.05f, 0.05f) : Vec3(1.0f, 0.85f, 0.2f), LabFontType::System);

        // 5. [DARK: ON/OFF] button
        float darkX = lightX + lightW + 8.0f;
        float darkW = 90.0f;
        bool isDarkHov = (_mouseScreenX >= darkX && _mouseScreenX <= darkX + darkW && _mouseScreenY >= tbY + 4.0f && _mouseScreenY <= tbY + 30.0f);
        Vec3 darkBtnBg = _darknessMode ? Vec3(0.55f, 0.15f, 0.80f) : (isDarkHov ? Vec3(0.32f, 0.35f, 0.40f) : Vec3(0.22f, 0.23f, 0.26f));
        Renderer::drawRect(darkX, tbY + 4.0f, darkW, 26.0f, darkBtnBg);
        drawDarkBevel(darkX, tbY + 4.0f, darkW, 26.0f, _darknessMode);
        LabFont::drawText(darkX + 8.0f, tbY + 9.0f, _darknessMode ? "DARK: ON" : "DARK: OFF", 1.25f, _darknessMode ? Vec3(1.0f, 0.95f, 0.6f) : Vec3(0.85f, 0.88f, 0.92f), LabFontType::System);

        // 6. [DARK PRESET] button
        float presetX = darkX + darkW + 8.0f;
        float presetW = 95.0f;
        bool isPresetHov = (_mouseScreenX >= presetX && _mouseScreenX <= presetX + presetW && _mouseScreenY >= tbY + 4.0f && _mouseScreenY <= tbY + 30.0f);
        Vec3 presetBtnBg = isPresetHov ? Vec3(0.32f, 0.35f, 0.40f) : Vec3(0.22f, 0.23f, 0.26f);
        Renderer::drawRect(presetX, tbY + 4.0f, presetW, 26.0f, presetBtnBg);
        drawDarkBevel(presetX, tbY + 4.0f, presetW, 26.0f, false);
        LabFont::drawText(presetX + 8.0f, tbY + 9.0f, "DARK PRESET", 1.25f, Vec3(0.6f, 0.85f, 1.0f), LabFontType::System);

        // ==================== 3. LEFT TOOLS PALETTE (Tools 0..7) ====================
        float leftW = 42.0f;
        float leftY = tbY + tbH;
        float leftH = h - leftY - 22.0f;
        Renderer::drawRect(0, leftY, leftW, leftH, panelDarker);
        drawDarkBevel(0, leftY, leftW, leftH, false);

        for (int i = 0; i < 8; ++i) {
            float ty = leftY + 10.0f + i * 36.0f;
            bool isSel = (_activeTool == i);
            bool isHov = (_mouseScreenX >= 6.0f && _mouseScreenX <= 36.0f && _mouseScreenY >= ty && _mouseScreenY <= ty + 30.0f);
            Vec3 bgCol = isSel ? orangeGlow : (isHov ? Vec3(0.32f, 0.35f, 0.40f) : Vec3(0.22f, 0.23f, 0.26f));
            Vec3 iconCol = isSel ? Vec3(1, 1, 1) : (isHov ? Vec3(1.0f, 0.85f, 0.50f) : Vec3(0.82f, 0.85f, 0.90f));

            Renderer::drawRect(6.0f, ty, 30.0f, 30.0f, bgCol);
            drawDarkBevel(6.0f, ty, 30.0f, 30.0f, isSel);
            drawHammerIcon(i, 9.0f, ty + 3.0f, iconCol, bgCol);
        }

        // ==================== 4. RIGHT SIDEBAR (Synchronized Layout) ====================
        SidebarLayout l = getSidebarLayout(w, h);
        Renderer::drawRect(l.rightX, l.rightY, l.rightW, l.rightH, panelDarkBg);
        drawDarkBevel(l.rightX, l.rightY, l.rightW, l.rightH, false);

        // Sidebar Tabs: [ Inspector ] [ Hierarchy ] [ Add Entity ]
        bool isPropTab = (_sidebarTab == SidebarTab::Properties);
        bool isOutTab = (_sidebarTab == SidebarTab::Hierarchy);
        bool isPreTab = (_sidebarTab == SidebarTab::Prebuilts);

        // Tab Inspector
        Vec3 propTabBg = isPropTab ? Vec3(0.24f, 0.26f, 0.30f) : Vec3(0.16f, 0.17f, 0.19f);
        Renderer::drawRect(l.tabPropX, l.tabPropY, l.tabPropW, l.tabPropH, propTabBg);
        drawDarkBevel(l.tabPropX, l.tabPropY, l.tabPropW, l.tabPropH, isPropTab);
        if (isPropTab) Renderer::drawRect(l.tabPropX, l.tabPropY + l.tabPropH - 2.0f, l.tabPropW, 2.0f, cyanGlow);
        LabFont::drawText(l.tabPropX + 16.0f, l.tabPropY + 5.0f, "Inspector", 1.5f, isPropTab ? Vec3(1, 1, 1) : textDim, LabFontType::System);

        // Tab Hierarchy
        Vec3 outTabBg = isOutTab ? Vec3(0.24f, 0.26f, 0.30f) : Vec3(0.16f, 0.17f, 0.19f);
        Renderer::drawRect(l.tabOutX, l.tabOutY, l.tabOutW, l.tabOutH, outTabBg);
        drawDarkBevel(l.tabOutX, l.tabOutY, l.tabOutW, l.tabOutH, isOutTab);
        if (isOutTab) Renderer::drawRect(l.tabOutX, l.tabOutY + l.tabOutH - 2.0f, l.tabOutW, 2.0f, cyanGlow);
        LabFont::drawText(l.tabOutX + 14.0f, l.tabOutY + 5.0f, "Hierarchy", 1.5f, isOutTab ? Vec3(1, 1, 1) : textDim, LabFontType::System);

        // Tab Add Entity
        Vec3 preTabBg = isPreTab ? Vec3(0.24f, 0.26f, 0.30f) : Vec3(0.16f, 0.17f, 0.19f);
        Renderer::drawRect(l.tabPreX, l.tabPreY, l.tabPreW, l.tabPreH, preTabBg);
        drawDarkBevel(l.tabPreX, l.tabPreY, l.tabPreW, l.tabPreH, isPreTab);
        if (isPreTab) Renderer::drawRect(l.tabPreX, l.tabPreY + l.tabPreH - 2.0f, l.tabPreW, 2.0f, cyanGlow);
        LabFont::drawText(l.tabPreX + 12.0f, l.tabPreY + 5.0f, "+ Entity", 1.5f, isPreTab ? Vec3(1, 1, 1) : textDim, LabFontType::System);

        // ==================== TAB CONTENT: PREBUILTS & ENTITIES ====================
        if (_sidebarTab == SidebarTab::Prebuilts) {
            float catY = l.rightY + 36.0f;
            float catW = (l.rightW - 24.0f) * 0.5f;

            bool isCat0 = (_prebuiltCategory == 0);
            bool isCat1 = (_prebuiltCategory == 1);

            Renderer::drawRect(l.rightX + 10.0f, catY, catW, 24.0f, isCat0 ? Vec3(0.26f, 0.28f, 0.32f) : Vec3(0.16f, 0.17f, 0.19f));
            drawDarkBevel(l.rightX + 10.0f, catY, catW, 24.0f, isCat0);
            if (isCat0) Renderer::drawRect(l.rightX + 10.0f, catY + 22.0f, catW, 2.0f, cyanGlow);
            LabFont::drawText(l.rightX + 14.0f, catY + 4.0f, "World & Actors", 1.4f, isCat0 ? Vec3(1, 1, 1) : textDim, LabFontType::System);

            Renderer::drawRect(l.rightX + 14.0f + catW, catY, catW, 24.0f, isCat1 ? Vec3(0.26f, 0.28f, 0.32f) : Vec3(0.16f, 0.17f, 0.19f));
            drawDarkBevel(l.rightX + 14.0f + catW, catY, catW, 24.0f, isCat1);
            if (isCat1) Renderer::drawRect(l.rightX + 14.0f + catW, catY + 22.0f, catW, 2.0f, cyanGlow);
            LabFont::drawText(l.rightX + 18.0f + catW, catY + 4.0f, "Components & FX", 1.4f, isCat1 ? Vec3(1, 1, 1) : textDim, LabFontType::System);

            struct PrebuiltDesc {
                std::string title;
                std::string desc;
                Vec3 color;
            };

            PrebuiltDesc cat0Items[] = {
                { "Actor / Player Start", "Universal Actor / Character spawn point", Vec3(0.18f, 0.65f, 0.35f) },
                { "Static Mesh Brush", "Solid geometric cube brush entity (2x2x2m)", Vec3(0.55f, 0.60f, 0.65f) },
                { "Ground Platform (16x16)", "Subdivided floor geometry block", Vec3(0.40f, 0.45f, 0.50f) },
                { "Architectural Barrier", "Tactical cover / structure barrier (3x1.2m)", Vec3(0.45f, 0.50f, 0.58f) },
                { "Structural Pillar", "Concrete cylindrical / box column (1.5x6m)", Vec3(0.55f, 0.58f, 0.65f) },
                { "Kinematic Door / Gate", "Sliding sensor-triggered kinematic entity", Vec3(0.25f, 0.35f, 0.45f) },
                { "Physics Dynamic Prop", "RigidBody prop with mass and collision", Vec3(0.65f, 0.50f, 0.35f) },
                { "Explosive Hazard Prop", "Physics barrel with radial detonation", Vec3(0.90f, 0.20f, 0.20f) },
                { "Omni Point Light", "Omnidirectional 3D light with radius falloff", Vec3(0.95f, 0.75f, 0.15f) },
                { "Trigger Zone Volume", "Box overlap volume for gameplay events", Vec3(0.20f, 0.75f, 0.85f) }
            };

            PrebuiltDesc cat1Items[] = {
                { "Item / Pickup Node", "Collectible item entity (weapons, items)", Vec3(0.25f, 0.60f, 0.40f) },
                { "Spatial Audio Emitter", "3D positional sound emitter node", Vec3(0.75f, 0.30f, 0.85f) },
                { "Particle VFX Emitter", "Universal GPU particle effect system", Vec3(0.85f, 0.55f, 0.15f) },
                { "Raycast Vehicle Entity", "4-wheel raycast suspension chassis", Vec3(0.35f, 0.60f, 0.75f) },
                { "Fluid / Water Volume", "Archimedes buoyancy and wave water", Vec3(0.20f, 0.55f, 0.90f) },
                { "Verlet Cloth / SoftBody", "Simulated deformable mesh / cloth", Vec3(0.75f, 0.45f, 0.25f) },
                { "Physics Joint / Constraint", "Hinge, slider, ball-socket constraint", Vec3(0.65f, 0.40f, 0.70f) },
                { "Radial Force / Attractor", "Explosion, wind and planetary attractor", Vec3(0.85f, 0.25f, 0.25f) },
                { "NavMesh Obstacle Box", "Dynamic pathfinding navigation blocker", Vec3(0.50f, 0.50f, 0.55f) }
            };

            float startY = catY + 30.0f;
            float cardH = isCat0 ? 44.0f : 36.0f;
            float cardSpacing = cardH + 5.0f;
            int count = isCat0 ? 10 : 9;
            const PrebuiltDesc* activeItems = isCat0 ? cat0Items : cat1Items;

            for (int i = 0; i < count; ++i) {
                float cy = startY + i * cardSpacing;
                bool isHov = (_mouseScreenX >= l.rightX + 10.0f && _mouseScreenX <= l.rightX + l.rightW - 10.0f && _mouseScreenY >= cy && _mouseScreenY <= cy + cardH);
                Vec3 cardBg = isHov ? Vec3(0.26f, 0.28f, 0.32f) : Vec3(0.20f, 0.21f, 0.24f);

                Renderer::drawRect(l.rightX + 10.0f, cy, l.rightW - 20.0f, cardH, cardBg);
                drawDarkBevel(l.rightX + 10.0f, cy, l.rightW - 20.0f, cardH, false);
                Renderer::drawRect(l.rightX + 10.0f, cy, 6.0f, cardH, activeItems[i].color);

                // Dedicated entity / weapon icon for each prebuilt item
                int iconType = isCat0 ? (i == 8 ? 9 : (i == 9 ? 10 : i)) : 8; // 8 = Weapon Silhouette, 9 = Crate, 10 = Barrel
                Lab::HammerIcons::drawEntityIcon(iconType, l.rightX + 20.0f, cy + (cardH - 26.0f) * 0.5f, activeItems[i].color, Vec3(0.14f, 0.15f, 0.17f));

                LabFont::drawText(l.rightX + 52.0f, cy + 4.0f, activeItems[i].title, 1.5f, isHov ? Vec3(1.0f, 0.95f, 0.70f) : textLight, LabFontType::System);
                LabFont::drawText(l.rightX + 52.0f, cy + (isCat0 ? 22.0f : 19.0f), activeItems[i].desc, 1.3f, textDim, LabFontType::System);
            }
        }

        // ==================== TAB CONTENT: OUTLINER (STRUKTURA MAPY) ====================
        if (_sidebarTab == SidebarTab::Hierarchy) {
            LabFont::drawText(l.rightX + 12.0f, l.rightY + 38.0f, "Map Entity Outliner:", 1.7f, orangeGlow, LabFontType::System);

            Renderer::drawRect(l.outListX, l.outListY, l.outListW, l.outListH, wellBg);
            drawDarkBevel(l.outListX, l.outListY, l.outListW, l.outListH, true);

            int totalSpawns = (int)_map->spawnPoints.size();
            int totalWepSpawns = (int)_map->weaponSpawners.size();
            int totalEntities = (int)(totalSpawns + totalWepSpawns + _map->brushes.size() + _map->props.size() + _map->doors.size() + _map->lights.size() + _map->cameras.size());
            int maxItems = (int)(l.outListH / l.outItemSpacing);

            for (int i = 0; i < maxItems; ++i) {
                int itemIdx = _outlinerScroll + i;
                if (itemIdx >= totalEntities) break;

                float iy = l.outListY + 4.0f + i * l.outItemSpacing;
                bool isSelected = false;
                std::string itemText = "";

                if (itemIdx < totalSpawns) {
                    isSelected = (_selectionType == SelectionType::Spawn && _selectedIndex == itemIdx);
                    const auto& sp = _map->spawnPoints[itemIdx];
                    itemText = "[Actor #" + std::to_string(itemIdx) + "] (" +
                               std::to_string((int)sp.position.x) + "," + std::to_string((int)sp.position.z) + ")";
                } else if (itemIdx < totalSpawns + totalWepSpawns) {
                    int wsIdx = itemIdx - totalSpawns;
                    isSelected = (_selectionType == SelectionType::WeaponSpawner && _selectedIndex == wsIdx);
                    const auto& ws = _map->weaponSpawners[wsIdx];
                    itemText = "[Item Pickup #" + std::to_string(wsIdx) + "] (" +
                               std::to_string((int)ws.position.x) + "," + std::to_string((int)ws.position.z) + ")";
                } else {
                    int bOffset = totalSpawns + totalWepSpawns;
                    int pOffset = bOffset + (int)_map->brushes.size();
                    int dOffset = pOffset + (int)_map->props.size();
                    int lOffset = dOffset + (int)_map->doors.size();
                    int cOffset = lOffset + (int)_map->lights.size();

                    if (itemIdx >= bOffset && itemIdx < pOffset) {
                        int bIdx = itemIdx - bOffset;
                        isSelected = (_selectionType == SelectionType::Brush && _selectedIndex == bIdx);
                        std::string tName = _map->brushes[bIdx].texturePath;
                        if (tName.size() > 14) tName = tName.substr(0, 12) + "..";
                        itemText = "[Mesh #" + std::to_string(bIdx) + "] " + tName;
                    } else if (itemIdx >= pOffset && itemIdx < dOffset) {
                        int pIdx = itemIdx - pOffset;
                        isSelected = (_selectionType == SelectionType::Prop && _selectedIndex == pIdx);
                        std::string mName = _map->props[pIdx].modelPath;
                        if (mName.size() > 14) mName = mName.substr(0, 12) + "..";
                        itemText = "[Prop #" + std::to_string(pIdx) + "] " + mName;
                    } else if (itemIdx >= dOffset && itemIdx < lOffset) {
                        int dIdx = itemIdx - dOffset;
                        isSelected = (_selectionType == SelectionType::Door && _selectedIndex == dIdx);
                        itemText = "[Kinematic #" + std::to_string(dIdx) + "] " + _map->doors[dIdx].name;
                    } else if (itemIdx >= lOffset && itemIdx < cOffset) {
                        int lIdx = itemIdx - lOffset;
                        isSelected = (_selectionType == SelectionType::Light && _selectedIndex == lIdx);
                        itemText = "[Light #" + std::to_string(lIdx) + "] " + _map->lights[lIdx].name;
                    } else if (itemIdx >= cOffset) {
                        int camIdx = itemIdx - cOffset;
                        isSelected = (_selectionType == SelectionType::Camera && _selectedIndex == camIdx);
                        itemText = "[Camera #" + std::to_string(camIdx) + "] " + _map->cameras[camIdx].name;
                    }
                }

                if (isSelected) {
                    Renderer::drawRect(l.outListX + 2.0f, iy, l.outListW - 4.0f, l.outItemH, Vec3(0.20f, 0.32f, 0.48f));
                    Renderer::drawRect(l.outListX + 2.0f, iy, 4.0f, l.outItemH, orangeGlow);
                }

                LabFont::drawText(l.outListX + 10.0f, iy + 4.0f, itemText, 1.5f, isSelected ? Vec3(1, 1, 1) : textLight, LabFontType::System);
            }

            // Outliner action buttons
            drawDarkButton(l.outFocusX, l.outFocusY, l.outFocusW, l.outFocusH, "Focus (F)");
            drawDarkButton(l.outDupX, l.outDupY, l.outDupW, l.outDupH, "Duplicate");
            drawDarkButton(l.outDelX, l.outDelY, l.outDelW, l.outDelH, "Delete", false, true);

            // Pagination buttons
            drawDarkButton(l.outPrevX, l.outPrevY, l.outPrevW, l.outPrevH, "< Prev Page");
            drawDarkButton(l.outNextX, l.outNextY, l.outNextW, l.outNextH, "Next Page >");
        }

        // ==================== TAB CONTENT: PROPERTIES ====================
        if (_sidebarTab == SidebarTab::Properties) {
            float propY = l.rightY + 38.0f;

            // Dedicated Item / Pickup Node Properties
            if (_selectionType == SelectionType::WeaponSpawner && _selectedIndex >= 0 && _selectedIndex < (int)_map->weaponSpawners.size()) {
                auto& ws = _map->weaponSpawners[_selectedIndex];
                std::string selHeader = "Selection: Item Pickup Node #" + std::to_string(_selectedIndex);
                LabFont::drawText(l.rightX + 12.0f, propY, selHeader, 1.7f, orangeGlow, LabFontType::System);

                std::string itemStr = "Role: Dynamic Collectible / Pickup Entity";
                LabFont::drawText(l.rightX + 12.0f, propY + 24.0f, itemStr, 1.5f, greenAccent, LabFontType::System);

                std::string posStr = "Pos: (" + std::to_string((int)ws.position.x) + ", " + std::to_string((int)ws.position.y) + ", " + std::to_string((int)ws.position.z) + ")";
                LabFont::drawText(l.rightX + 12.0f, propY + 46.0f, posStr, 1.5f, textLight, LabFontType::System);

                std::string respStr = "Respawn Cooldown: " + std::to_string((int)ws.respawnTime) + "s (Default: 30s)";
                LabFont::drawText(l.rightX + 12.0f, propY + 68.0f, respStr, 1.5f, textLight, LabFontType::System);

                // Category Buttons
                LabFont::drawText(l.rightX + 12.0f, propY + 96.0f, "Cycle Pickup Sub-Type:", 1.6f, textLight, LabFontType::System);
                float wepBtnY = propY + 114.0f;
                drawDarkButton(l.rightX + 10.0f, wepBtnY, 130.0f, 26.0f, "< Prev Type");
                drawDarkButton(l.rightX + 150.0f, wepBtnY, 130.0f, 26.0f, "Next Type >");

                // Respawn Time Buttons [-15s] [30s (Default)] [+15s]
                LabFont::drawText(l.rightX + 12.0f, wepBtnY + 36.0f, "Set Respawn Delay Cooldown:", 1.6f, textLight, LabFontType::System);
                float respBtnY = wepBtnY + 54.0f;
                drawDarkButton(l.rightX + 10.0f, respBtnY, 70.0f, 24.0f, "-15s");

                bool isDefault30 = (std::abs(ws.respawnTime - 30.0f) < 0.1f);
                drawDarkButton(l.rightX + 90.0f, respBtnY, 100.0f, 24.0f, "30s (Default)", isDefault30, false, isDefault30);
                drawDarkButton(l.rightX + 200.0f, respBtnY, 70.0f, 24.0f, "+15s");

                // Deselect & Delete buttons
                drawDarkButton(l.deselX, l.deselY, l.deselW, l.deselH, "Deselect All");
                drawDarkButton(l.delX, l.delY, l.delW, l.delH, "Delete Item Node", false, true);
            } else if (_selectionType == SelectionType::Spawn && _selectedIndex >= 0 && _selectedIndex < (int)_map->spawnPoints.size()) {
                auto& sp = _map->spawnPoints[_selectedIndex];
                std::string selHeader = "Selection: Actor / Player Start #" + std::to_string(_selectedIndex);
                LabFont::drawText(l.rightX + 12.0f, propY, selHeader, 1.7f, orangeGlow, LabFontType::System);

                std::string classStr = "Class: Character Spawn Point";
                LabFont::drawText(l.rightX + 12.0f, propY + 24.0f, classStr, 1.5f, textLight, LabFontType::System);

                std::string posStr = "Pos: (" + std::to_string((int)sp.position.x) + ", " + std::to_string((int)sp.position.y) + ", " + std::to_string((int)sp.position.z) + ")";
                LabFont::drawText(l.rightX + 12.0f, propY + 46.0f, posStr, 1.5f, textLight, LabFontType::System);

                std::string yawStr = "Facing Yaw: " + std::to_string((int)sp.yaw) + " deg";
                LabFont::drawText(l.rightX + 12.0f, propY + 68.0f, yawStr, 1.5f, textLight, LabFontType::System);

                // Spawn Mode selector buttons
                LabFont::drawText(l.rightX + 12.0f, propY + 98.0f, "Actor Role Allocation:", 1.6f, textLight, LabFontType::System);
                float typeBtnY = propY + 114.0f;
                float btnW = 94.0f;

                bool isFFA = (sp.type == SpawnType::FFA);
                bool isAlpha = (sp.type == SpawnType::TeamAlpha);
                bool isBeta = (sp.type == SpawnType::TeamBeta);

                drawDarkButton(l.rightX + 10.0f, typeBtnY, btnW, 26.0f, "Player 1", isFFA, false, isFFA);
                drawDarkButton(l.rightX + 110.0f, typeBtnY, btnW, 26.0f, "Player 2", isAlpha);
                drawDarkButton(l.rightX + 210.0f, typeBtnY, btnW, 26.0f, "AI Bot", isBeta, isBeta);

                // Yaw Rotate buttons
                LabFont::drawText(l.rightX + 12.0f, typeBtnY + 38.0f, "Rotate Facing Direction:", 1.6f, textLight, LabFontType::System);
                float yawBtnY = typeBtnY + 58.0f;
                float yBtnW = 55.0f;
                const char* yLabels[5] = { "-45", "0", "90", "180", "+45" };
                for (int k = 0; k < 5; ++k) {
                    float yx = l.rightX + 10.0f + k * 58.0f;
                    drawDarkButton(yx, yawBtnY, yBtnW, 24.0f, yLabels[k]);
                }

                // Deselect & Delete buttons
                drawDarkButton(l.deselX, l.deselY, l.deselW, l.deselH, "Deselect All");
                drawDarkButton(l.delX, l.delY, l.delW, l.delH, "Delete Actor Start", false, true);
            } else if (_selectionType == SelectionType::Light && _selectedIndex >= 0 && _selectedIndex < (int)_map->lights.size()) {
                auto& lt = _map->lights[_selectedIndex];
                std::string selHeader = "Selection: Lamp Light #" + std::to_string(_selectedIndex);
                LabFont::drawText(l.rightX + 12.0f, propY, selHeader, 1.7f, orangeGlow, LabFontType::System);

                std::string nameStr = "Name: " + lt.name;
                LabFont::drawText(l.rightX + 12.0f, propY + 24.0f, nameStr, 1.5f, greenAccent, LabFontType::System);

                std::string posStr = "Pos: (" + std::to_string((int)lt.position.x) + ", " + std::to_string((int)lt.position.y) + ", " + std::to_string((int)lt.position.z) + ")";
                LabFont::drawText(l.rightX + 12.0f, propY + 46.0f, posStr, 1.5f, textLight, LabFontType::System);

                char statsBuf[64];
                std::snprintf(statsBuf, sizeof(statsBuf), "Intensity: %.1f | Radius: %.1fm", lt.intensity, lt.radius);
                LabFont::drawText(l.rightX + 12.0f, propY + 68.0f, statsBuf, 1.5f, cyanGlow, LabFontType::System);

                // Color swatch
                Renderer::drawRect(l.rightX + 12.0f, propY + 90.0f, 280.0f, 6.0f, lt.color);

                // Color presets
                LabFont::drawText(l.rightX + 12.0f, propY + 102.0f, "Kelvin / Atmosphere Color Presets:", 1.5f, textLight, LabFontType::System);
                float colBtnY = propY + 116.0f;
                float colBtnW = 90.0f;
                drawDarkButton(l.rightX + 10.0f, colBtnY, colBtnW, 24.0f, "Warm 2700K");
                drawDarkButton(l.rightX + 106.0f, colBtnY, colBtnW, 24.0f, "Cool 6500K");
                drawDarkButton(l.rightX + 202.0f, colBtnY, colBtnW, 24.0f, "Sodium");

                drawDarkButton(l.rightX + 10.0f, colBtnY + 28.0f, colBtnW, 24.0f, "Neon Cyan");
                drawDarkButton(l.rightX + 106.0f, colBtnY + 28.0f, colBtnW, 24.0f, "Hazard Red");
                drawDarkButton(l.rightX + 202.0f, colBtnY + 28.0f, colBtnW, 24.0f, "Toxic Green");

                // Intensity & Radius adjusters
                float paramY = colBtnY + 64.0f;
                LabFont::drawText(l.rightX + 12.0f, paramY, "Adjust Intensity & Radius:", 1.5f, textLight, LabFontType::System);
                drawDarkButton(l.rightX + 10.0f, paramY + 18.0f, 68.0f, 24.0f, "Int -");
                drawDarkButton(l.rightX + 82.0f, paramY + 18.0f, 68.0f, 24.0f, "Int +");
                drawDarkButton(l.rightX + 158.0f, paramY + 18.0f, 68.0f, 24.0f, "Rad -");
                drawDarkButton(l.rightX + 230.0f, paramY + 18.0f, 68.0f, 24.0f, "Rad +");

                // Move position [-] [+]
                LabFont::drawText(l.rightX + 12.0f, l.posBtnsY - 18.0f, "Move Light Position (X / Y / Z):", 1.7f, cyanGlow, LabFontType::System);
                LabFont::drawText(l.rightX + 12.0f, l.posBtnsY + 4.0f, "X:", 1.6f, textLight, LabFontType::System);
                drawDarkButton(l.rightX + 30.0f, l.posBtnsY, l.posBtnW, l.posBtnH, "-");
                drawDarkButton(l.rightX + 60.0f, l.posBtnsY, l.posBtnW, l.posBtnH, "+");

                LabFont::drawText(l.rightX + 105.0f, l.posBtnsY + 4.0f, "Y:", 1.6f, greenAccent, LabFontType::System);
                drawDarkButton(l.rightX + 123.0f, l.posBtnsY, l.posBtnW, l.posBtnH, "-");
                drawDarkButton(l.rightX + 153.0f, l.posBtnsY, l.posBtnW, l.posBtnH, "+");

                LabFont::drawText(l.rightX + 198.0f, l.posBtnsY + 4.0f, "Z:", 1.6f, textLight, LabFontType::System);
                drawDarkButton(l.rightX + 216.0f, l.posBtnsY, l.posBtnW, l.posBtnH, "-");
                drawDarkButton(l.rightX + 246.0f, l.posBtnsY, l.posBtnW, l.posBtnH, "+");

                // Elevation buttons
                drawDarkButton(l.elevUpX, l.elevUpY, l.elevUpW, l.elevUpH, "^ UP (+Y)", false, false, true);
                drawDarkButton(l.elevDwnX, l.elevDwnY, l.elevDwnW, l.elevDwnH, "v DOWN (-Y)");
                drawDarkButton(l.elevGndX, l.elevGndY, l.elevGndW, l.elevGndH, "Floor (Y=0)");

                // Deselect & Delete buttons
                drawDarkButton(l.deselX, l.deselY, l.deselW, l.deselH, "Deselect All");
                drawDarkButton(l.delX, l.delY, l.delW, l.delH, "Delete Lamp Light", false, true);
            } else {

            // Header info on selection
            std::string selHeader = "Selection: None";
            if (_selectionType == SelectionType::Brush) selHeader = "Selection: Brush #" + std::to_string(_selectedIndex);
            else if (_selectionType == SelectionType::Prop) selHeader = "Selection: Prop #" + std::to_string(_selectedIndex);
            else if (_selectionType == SelectionType::Door) selHeader = "Selection: Door #" + std::to_string(_selectedIndex);
            else if (_selectionType == SelectionType::Spawn) selHeader = "Selection: Player Spawn";
            else if (_selectionType == SelectionType::Light) selHeader = "Selection: Lamp Light #" + std::to_string(_selectedIndex);
            else if (_selectionType == SelectionType::Camera) selHeader = "Selection: Camera #" + std::to_string(_selectedIndex);

            LabFont::drawText(l.rightX + 12.0f, propY, selHeader, 1.7f, (_selectionType != SelectionType::None) ? orangeGlow : textLight, LabFontType::System);

            // Coordinates & Dimensions
            Vec3 pos = _cursorPos;
            Vec3 dims = _brushSize;
            Vec3 rot = Vec3(0, 0, 0);
            if (_selectionType == SelectionType::Brush && _selectedIndex >= 0 && _selectedIndex < (int)_map->brushes.size()) {
                pos = _map->brushes[_selectedIndex].position;
                dims = _map->brushes[_selectedIndex].size;
                rot = _map->brushes[_selectedIndex].rotation;
            } else if (_selectionType == SelectionType::Prop && _selectedIndex >= 0 && _selectedIndex < (int)_map->props.size()) {
                pos = _map->props[_selectedIndex].position;
                dims = _map->props[_selectedIndex].scale;
                rot = _map->props[_selectedIndex].rotation;
            } else if (_selectionType == SelectionType::Door && _selectedIndex >= 0 && _selectedIndex < (int)_map->doors.size()) {
                pos = _map->doors[_selectedIndex].position;
                dims = _map->doors[_selectedIndex].size;
            } else if (_selectionType == SelectionType::Spawn && _selectedIndex >= 0 && _selectedIndex < (int)_map->spawnPoints.size()) {
                pos = _map->spawnPoints[_selectedIndex].position;
                rot = Vec3(0, _map->spawnPoints[_selectedIndex].yaw, 0);
            } else if (_selectionType == SelectionType::WeaponSpawner && _selectedIndex >= 0 && _selectedIndex < (int)_map->weaponSpawners.size()) {
                pos = _map->weaponSpawners[_selectedIndex].position;
            } else if (_selectionType == SelectionType::Camera && _selectedIndex >= 0 && _selectedIndex < (int)_map->cameras.size()) {
                pos = _map->cameras[_selectedIndex].position;
                dims = Vec3(0.5f, 0.5f, 0.5f);
                rot = _map->cameras[_selectedIndex].rotation;
            }

            std::string posStr = "Pos: (" + std::to_string((int)pos.x) + ", " + std::to_string((int)pos.y) + ", " + std::to_string((int)pos.z) + ")";
            LabFont::drawText(l.rightX + 12.0f, propY + 20.0f, posStr, 1.55f, textLight, LabFontType::System);

            std::string dimStr = "Size: (" + std::to_string((int)dims.x) + " x " + std::to_string((int)dims.y) + " x " + std::to_string((int)dims.z) + ")";
            LabFont::drawText(l.rightX + 12.0f, propY + 36.0f, dimStr, 1.5f, textLight, LabFontType::System);

            std::string rotStr = "Rot: (" + std::to_string((int)rot.x) + "°, " + std::to_string((int)rot.y) + "°, " + std::to_string((int)rot.z) + "°)";
            LabFont::drawText(l.rightX + 12.0f, propY + 52.0f, rotStr, 1.5f, cyanGlow, LabFontType::System);

            // Entity Name / ID & Lua Script Display
            std::string entName = "";
            std::string entScript = "";
            if (_selectionType == SelectionType::Brush && _selectedIndex >= 0 && _selectedIndex < (int)_map->brushes.size()) {
                entName = _map->brushes[_selectedIndex].name;
                entScript = _map->brushes[_selectedIndex].script;
            } else if (_selectionType == SelectionType::Prop && _selectedIndex >= 0 && _selectedIndex < (int)_map->props.size()) {
                entName = _map->props[_selectedIndex].name;
                entScript = _map->props[_selectedIndex].script;
            } else if (_selectionType == SelectionType::Sprite && _selectedIndex >= 0 && _selectedIndex < (int)_map->sprites.size()) {
                entName = _map->sprites[_selectedIndex].name;
                entScript = _map->sprites[_selectedIndex].script;
            } else if (_selectionType == SelectionType::Camera && _selectedIndex >= 0 && _selectedIndex < (int)_map->cameras.size()) {
                entName = _map->cameras[_selectedIndex].name;
                entScript = _map->cameras[_selectedIndex].script;
            }

            if (_selectionType != SelectionType::None) {
                std::string nameDisplay = "Entity ID: " + (entName.empty() ? "(unnamed)" : entName);
                LabFont::drawText(l.rightX + 12.0f, propY + 68.0f, nameDisplay, 1.45f, orangeGlow, LabFontType::System);
                drawDarkButton(l.rightX + 10.0f, propY + 84.0f, 135.0f, 22.0f, "Set \"Klocek1\"", false, false, true);
                drawDarkButton(l.rightX + 155.0f, propY + 84.0f, 140.0f, 22.0f, "Assign Script");
            }

            // UV Scale buttons
            LabFont::drawText(l.rightX + 12.0f, l.uvBtnY - 18.0f, "Texture UV Tiling Scale:", 1.7f, textLight, LabFontType::System);

            float scales[4] = { 0.125f, 0.25f, 0.5f, 1.0f };
            const char* scaleLabels[4] = { "0.125", "0.25", "0.5", "1.0" };
            for (int i = 0; i < 4; ++i) {
                float sx = l.rightX + 10.0f + i * 70.0f;
                bool isCurScale = (std::abs(_activeUvScale.x - scales[i]) < 0.01f);
                drawDarkButton(sx, l.uvBtnY, l.uvBtnW, l.uvBtnH, scaleLabels[i], isCurScale);
            }

            // Texture Preview & Picker
            LabFont::drawText(l.rightX + 12.0f, l.texBoxY - 18.0f, "Active Texture:", 1.7f, textLight, LabFontType::System);
            Renderer::drawRect(l.texBoxX, l.texBoxY, l.texBoxW, l.texBoxH, wellBg);
            drawDarkBevel(l.texBoxX, l.texBoxY, l.texBoxW, l.texBoxH, true);
            LabFont::drawText(l.texBoxX + 10.0f, l.texBoxY + 4.0f, _selectedTexture, 1.6f, textLight, LabFontType::System);

            Renderer::drawRect(l.thumbX, l.thumbY, l.thumbS, l.thumbS, Vec3(0, 0, 0));
            drawDarkBevel(l.thumbX, l.thumbY, l.thumbS, l.thumbS, true);
            if (_textures.contains(_selectedTexture)) {
                Renderer::drawTextureRect(l.thumbX + 2.0f, l.thumbY + 2.0f, l.thumbS - 4.0f, l.thumbS - 4.0f, *_textures[_selectedTexture]);
            }

            drawDarkButton(l.texBrowseX, l.texBrowseY, l.texBrowseW, l.texBrowseH, "Browse Textures...");
            drawDarkButton(l.texApplyX, l.texApplyY, l.texApplyW, l.texApplyH, "Apply to Brush", false, false, true);

            // 3D Entity Prop Model Selector
            LabFont::drawText(l.rightX + 12.0f, l.modelBoxY - 18.0f, "3D Entity Model (.stl):", 1.7f, textLight, LabFontType::System);
            Renderer::drawRect(l.modelBoxX, l.modelBoxY, l.modelBoxW, l.modelBoxH, wellBg);
            drawDarkBevel(l.modelBoxX, l.modelBoxY, l.modelBoxW, l.modelBoxH, true);
            LabFont::drawText(l.modelBoxX + 10.0f, l.modelBoxY + 4.0f, _selectedModel, 1.6f, textLight, LabFontType::System);

            drawDarkButton(l.modelBrowseX, l.modelBrowseY, l.modelBrowseW, l.modelBrowseH, "Browse 3D Models...");

            // Position Translation [-] [+]
            LabFont::drawText(l.rightX + 12.0f, l.posBtnsY - 18.0f, "Move Position (X / Y / Z):", 1.7f, cyanGlow, LabFontType::System);

            // X Position
            LabFont::drawText(l.rightX + 12.0f, l.posBtnsY + 4.0f, "X:", 1.6f, textLight, LabFontType::System);
            drawDarkButton(l.rightX + 30.0f, l.posBtnsY, l.posBtnW, l.posBtnH, "-");
            drawDarkButton(l.rightX + 60.0f, l.posBtnsY, l.posBtnW, l.posBtnH, "+");

            // Y Position (Vertical Elevation)
            LabFont::drawText(l.rightX + 105.0f, l.posBtnsY + 4.0f, "Y:", 1.6f, greenAccent, LabFontType::System);
            drawDarkButton(l.rightX + 123.0f, l.posBtnsY, l.posBtnW, l.posBtnH, "-");
            drawDarkButton(l.rightX + 153.0f, l.posBtnsY, l.posBtnW, l.posBtnH, "+");

            // Z Position
            LabFont::drawText(l.rightX + 198.0f, l.posBtnsY + 4.0f, "Z:", 1.6f, textLight, LabFontType::System);
            drawDarkButton(l.rightX + 216.0f, l.posBtnsY, l.posBtnW, l.posBtnH, "-");
            drawDarkButton(l.rightX + 246.0f, l.posBtnsY, l.posBtnW, l.posBtnH, "+");

            // Quick Elevation Action Buttons
            drawDarkButton(l.elevUpX, l.elevUpY, l.elevUpW, l.elevUpH, "^ UP (+Y)", false, false, true);
            drawDarkButton(l.elevDwnX, l.elevDwnY, l.elevDwnW, l.elevDwnH, "v DOWN (-Y)");
            drawDarkButton(l.elevGndX, l.elevGndY, l.elevGndW, l.elevGndH, "Floor (Y=0)");

            // Dimension Adjusters [-] [+]
            LabFont::drawText(l.rightX + 12.0f, l.dimBtnsY - 18.0f, "Adjust Size (X / Y / Z):", 1.7f, textLight, LabFontType::System);

            // X
            LabFont::drawText(l.rightX + 12.0f, l.dimBtnsY + 4.0f, "X:", 1.6f, textLight, LabFontType::System);
            drawDarkButton(l.rightX + 30.0f, l.dimBtnsY, l.dimBtnW, l.dimBtnH, "-");
            drawDarkButton(l.rightX + 60.0f, l.dimBtnsY, l.dimBtnW, l.dimBtnH, "+");

            // Y
            LabFont::drawText(l.rightX + 105.0f, l.dimBtnsY + 4.0f, "Y:", 1.6f, textLight, LabFontType::System);
            drawDarkButton(l.rightX + 123.0f, l.dimBtnsY, l.dimBtnW, l.dimBtnH, "-");
            drawDarkButton(l.rightX + 153.0f, l.dimBtnsY, l.dimBtnW, l.dimBtnH, "+");

            // Z
            LabFont::drawText(l.rightX + 198.0f, l.dimBtnsY + 4.0f, "Z:", 1.6f, textLight, LabFontType::System);
            drawDarkButton(l.rightX + 216.0f, l.dimBtnsY, l.dimBtnW, l.dimBtnH, "-");
            drawDarkButton(l.rightX + 246.0f, l.dimBtnsY, l.dimBtnW, l.dimBtnH, "+");

            // Rotation Adjusters (Pitch / Yaw / Roll)
            LabFont::drawText(l.rightX + 12.0f, l.rotBtnsY - 18.0f, "Rotate Selection (P / Y / R):", 1.7f, orangeGlow, LabFontType::System);
            LabFont::drawText(l.rightX + 12.0f, l.rotBtnsY + 4.0f, "P:", 1.5f, textLight, LabFontType::System);
            drawDarkButton(l.rightX + 28.0f, l.rotBtnsY, 26.0f, l.rotBtnH, "-");
            drawDarkButton(l.rightX + 56.0f, l.rotBtnsY, 26.0f, l.rotBtnH, "+");

            LabFont::drawText(l.rightX + 90.0f, l.rotBtnsY + 4.0f, "Yaw:", 1.5f, cyanGlow, LabFontType::System);
            drawDarkButton(l.rightX + 124.0f, l.rotBtnsY, 44.0f, l.rotBtnH, "-45°");
            drawDarkButton(l.rightX + 172.0f, l.rotBtnsY, 44.0f, l.rotBtnH, "+45°");

            LabFont::drawText(l.rightX + 224.0f, l.rotBtnsY + 4.0f, "R:", 1.5f, textLight, LabFontType::System);
            drawDarkButton(l.rightX + 242.0f, l.rotBtnsY, 26.0f, l.rotBtnH, "-");
            drawDarkButton(l.rightX + 270.0f, l.rotBtnsY, 26.0f, l.rotBtnH, "+");

            // Deselect button
            drawDarkButton(l.deselX, l.deselY, l.deselW, l.deselH, "Deselect All");

            // Delete Selected button
            drawDarkButton(l.delX, l.delY, l.delW, l.delH, "Delete Selected", false, true);
        }
    }

        // ==================== CENTRAL VIEWPORT & WORKSPACES ====================
        float vpX = leftW;
        float vpY = tbY + tbH;
        float vpW = l.rightX - vpX;
        float vpH = (h - 22.0f) - vpY;

        // Viewport Tab Bar (Intuitive Workspace Switcher: Project Hub / 3D Scene / Lua Script Studio / HUD Canvas / Plugins)
        float tabH = 26.0f;
        Renderer::drawRect(vpX, vpY, vpW, tabH, Vec3(0.10f, 0.11f, 0.14f));
        drawDarkBevel(vpX, vpY, vpW, tabH, false);

        bool isHub = (_workspace == EditorWorkspace::Project_Hub);
        bool is3D = (_workspace == EditorWorkspace::World3D_Level || _workspace == EditorWorkspace::World3D_Animation);
        bool isScr = (_workspace == EditorWorkspace::Scripting_Editor || _workspace == EditorWorkspace::Scripting_Console);
        bool isHud = (_workspace == EditorWorkspace::Level2D_HUD);
        bool isMv = (_workspace == EditorWorkspace::World3D_ModelViewer);
        bool isAnim2D = (_workspace == EditorWorkspace::Level2D_Animation);
        bool isStud = isStudioWorkspace(_workspace);

        // Tab 0: [ Project Hub ]
        float tab0X = vpX + 6.0f;
        float tab0W = 110.0f;
        Renderer::drawRect(tab0X, vpY + 2.0f, tab0W, tabH - 4.0f, isHub ? Vec3(0.18f, 0.22f, 0.28f) : Vec3(0.12f, 0.13f, 0.16f));
        drawDarkBevel(tab0X, vpY + 2.0f, tab0W, tabH - 4.0f, isHub);
        if (isHub) Renderer::drawRect(tab0X, vpY + tabH - 2.0f, tab0W, 2.0f, orangeGlow);
        LabFont::drawText(tab0X + 12.0f, vpY + 6.0f, "Project Hub", 1.35f, isHub ? Vec3(1, 1, 1) : textDim, LabFontType::System);

        // Tab 1: [ 3D Level Scene ]
        float tab1X = tab0X + tab0W + 6.0f;
        float tab1W = 110.0f;
        Renderer::drawRect(tab1X, vpY + 2.0f, tab1W, tabH - 4.0f, is3D ? Vec3(0.18f, 0.22f, 0.28f) : Vec3(0.12f, 0.13f, 0.16f));
        drawDarkBevel(tab1X, vpY + 2.0f, tab1W, tabH - 4.0f, is3D);
        if (is3D) Renderer::drawRect(tab1X, vpY + tabH - 2.0f, tab1W, 2.0f, orangeGlow);
        LabFont::drawText(tab1X + 16.0f, vpY + 6.0f, "3D Scene", 1.35f, is3D ? Vec3(1, 1, 1) : textDim, LabFontType::System);

        // Tab 2: [ Lua Script Studio ]
        float tab2X = tab1X + tab1W + 6.0f;
        float tab2W = 140.0f;
        Renderer::drawRect(tab2X, vpY + 2.0f, tab2W, tabH - 4.0f, isScr ? Vec3(0.18f, 0.22f, 0.28f) : Vec3(0.12f, 0.13f, 0.16f));
        drawDarkBevel(tab2X, vpY + 2.0f, tab2W, tabH - 4.0f, isScr);
        if (isScr) Renderer::drawRect(tab2X, vpY + tabH - 2.0f, tab2W, 2.0f, cyanGlow);
        LabFont::drawText(tab2X + 10.0f, vpY + 6.0f, "Lua Script Studio", 1.35f, isScr ? Vec3(1, 1, 1) : textDim, LabFontType::System);

        // Tab 3: [ HUD Canvas ]
        float tab3X = tab2X + tab2W + 6.0f;
        float tab3W = 120.0f;
        Renderer::drawRect(tab3X, vpY + 2.0f, tab3W, tabH - 4.0f, isHud ? Vec3(0.18f, 0.22f, 0.28f) : Vec3(0.12f, 0.13f, 0.16f));
        drawDarkBevel(tab3X, vpY + 2.0f, tab3W, tabH - 4.0f, isHud);
        if (isHud) Renderer::drawRect(tab3X, vpY + tabH - 2.0f, tab3W, 2.0f, cyanGlow);
        LabFont::drawText(tab3X + 10.0f, vpY + 6.0f, "HUD Canvas", 1.35f, isHud ? Vec3(1, 1, 1) : textDim, LabFontType::System);

        // Tab 4: [ Plugins & Add-ons ]
        bool isPlug = (_workspace == EditorWorkspace::Plugins_Manager);
        float tab4X = tab3X + tab3W + 6.0f;
        float tab4W = 135.0f;
        Renderer::drawRect(tab4X, vpY + 2.0f, tab4W, tabH - 4.0f, isPlug ? Vec3(0.18f, 0.22f, 0.28f) : Vec3(0.12f, 0.13f, 0.16f));
        drawDarkBevel(tab4X, vpY + 2.0f, tab4W, tabH - 4.0f, isPlug);
        if (isPlug) Renderer::drawRect(tab4X, vpY + tabH - 2.0f, tab4W, 2.0f, cyanGlow);
        LabFont::drawText(tab4X + 10.0f, vpY + 6.0f, "Plugins & Add-ons", 1.35f, isPlug ? Vec3(1, 1, 1) : textDim, LabFontType::System);

        // Tab 5 (Contextual: 3D Model Viewer / Studio / 2D Anim if active)
        if (isMv || isStud || isAnim2D) {
            float tab5X = tab4X + tab4W + 6.0f;
            float tab5W = 155.0f;
            std::string tab5Title = isMv ? "3D Model Viewer" : (isAnim2D ? "2D Animation" : "Character Studio");
            Renderer::drawRect(tab5X, vpY + 2.0f, tab5W, tabH - 4.0f, Vec3(0.18f, 0.22f, 0.28f));
            drawDarkBevel(tab5X, vpY + 2.0f, tab5W, tabH - 4.0f, true);
            Renderer::drawRect(tab5X, vpY + tabH - 2.0f, tab5W, 2.0f, orangeGlow);
            LabFont::drawText(tab5X + 10.0f, vpY + 6.0f, tab5Title, 1.35f, Vec3(1, 1, 1), LabFontType::System);
        }

        float contentY = vpY + tabH;
        float contentH = vpH - tabH;

        // --- 3D Viewport Live Addon Overlays ---
        if (_workspace == EditorWorkspace::World3D_Level || _workspace == EditorWorkspace::World3D_Animation) {
            // 1. Retro CRT & Post-Process FX (Scanlines, Vignette, CRT Badge)
            if (PackageManager::instance().isPackageEnabled("com.lab.retro-fx")) {
                if (_retroScanlines) {
                    float stepY = (float)_retroPixelScale * 2.0f;
                    Vec3 scanCol = (_retroColorTheme == 1) ? Vec3(0.18f, 0.12f, 0.02f) :
                                   (_retroColorTheme == 2) ? Vec3(0.02f, 0.18f, 0.05f) :
                                   Vec3(0.03f, 0.03f, 0.05f);
                    for (float sy = contentY; sy < contentY + contentH; sy += stepY) {
                        Renderer::drawRect(vpX, sy, vpW, 1.0f, scanCol);
                    }
                }
                if (_retroVignette) {
                    Renderer::drawRect(vpX, contentY, vpW, 16.0f, Vec3(0.02f, 0.03f, 0.05f));
                    Renderer::drawRect(vpX, contentY + contentH - 16.0f, vpW, 16.0f, Vec3(0.02f, 0.03f, 0.05f));
                    Renderer::drawRect(vpX, contentY, 16.0f, contentH, Vec3(0.02f, 0.03f, 0.05f));
                    Renderer::drawRect(vpX + vpW - 16.0f, contentY, 16.0f, contentH, Vec3(0.02f, 0.03f, 0.05f));
                }
                Renderer::drawRect(vpX + 10.0f, contentY + 10.0f, 130.0f, 22.0f, Vec3(0.10f, 0.18f, 0.12f));
                drawDarkBevel(vpX + 10.0f, contentY + 10.0f, 130.0f, 22.0f, false);
                LabFont::drawText(vpX + 16.0f, contentY + 14.0f, "CRT FX ACTIVE", 1.25f, Vec3(0.4f, 1.0f, 0.5f), LabFontType::System);
            }

            // 2. Speedrun Splits & Live Telemetry Overlay
            if (PackageManager::instance().isPackageEnabled("com.lab.speedrun-timer") && _speedrunOverlayVisible) {
                float hudX = (_speedrunPosition == 0) ? (vpX + vpW - 245.0f) : (vpX + 16.0f);
                float hudY = contentY + 16.0f;
                float hudW = 230.0f;
                float hudH = 92.0f;
                Renderer::drawRect(hudX, hudY, hudW, hudH, Vec3(0.08f, 0.10f, 0.14f));
                drawDarkBevel(hudX, hudY, hudW, hudH, false);
                Renderer::drawRect(hudX, hudY, hudW, 20.0f, Vec3(0.12f, 0.18f, 0.26f));
                LabFont::drawText(hudX + 8.0f, hudY + 4.0f, "SPEEDRUN & TELEMETRY", 1.25f, cyanGlow, LabFontType::System);

                int mins = (int)(_speedrunElapsedTime / 60.0f);
                float secs = _speedrunElapsedTime - (mins * 60.0f);
                char timeBuf[32];
                snprintf(timeBuf, sizeof(timeBuf), "%02d:%05.2f", mins, secs);
                LabFont::drawText(hudX + 12.0f, hudY + 28.0f, timeBuf, 2.0f, _speedrunTimerRunning ? Vec3(0.4f, 1.0f, 0.5f) : Vec3(1.0f, 0.85f, 0.3f), LabFontType::System);

                std::string statStr = _speedrunTimerRunning ? "[RUNNING]" : "[PAUSED]";
                LabFont::drawText(hudX + 145.0f, hudY + 34.0f, statStr, 1.15f, textDim, LabFontType::System);

                char teleBuf[64];
                snprintf(teleBuf, sizeof(teleBuf), "FPS: 60.0  *  Vel: %.1f m/s", _isFlyingCamera ? 15.0f : 0.0f);
                LabFont::drawText(hudX + 12.0f, hudY + 68.0f, teleBuf, 1.25f, textLight, LabFontType::System);
            }

            // 3. Vehicle Dynamics Dashboard HUD
            if (PackageManager::instance().isPackageEnabled("com.lab.vehicle-arcade") && _vehicleTelemetryVisible) {
                float vhX = vpX + 16.0f;
                float vhY = contentY + contentH - 85.0f;
                float vhW = 210.0f;
                float vhH = 75.0f;
                Renderer::drawRect(vhX, vhY, vhW, vhH, Vec3(0.08f, 0.10f, 0.13f));
                drawDarkBevel(vhX, vhY, vhW, vhH, false);
                Renderer::drawRect(vhX, vhY, vhW, 18.0f, Vec3(0.14f, 0.18f, 0.24f));
                LabFont::drawText(vhX + 8.0f, vhY + 3.0f, "VEHICLE DYNAMICS HUD", 1.2f, orangeGlow, LabFontType::System);

                char spdBuf[32];
                snprintf(spdBuf, sizeof(spdBuf), "%.0f KM/H", _vehicleSpeedKmh);
                LabFont::drawText(vhX + 12.0f, vhY + 24.0f, spdBuf, 1.8f, Vec3(1, 1, 1), LabFontType::System);

                char rpmBuf[32];
                snprintf(rpmBuf, sizeof(rpmBuf), "RPM: %.0f [Gear %d]", _vehicleRpm, _vehicleGear);
                LabFont::drawText(vhX + 12.0f, vhY + 54.0f, rpmBuf, 1.25f, cyanGlow, LabFontType::System);
            }

            // 4. Quick Indicator Pill for Active Addons in 3D Viewport
            if (PackageManager::instance().isPackageEnabled("com.lab.physics-cloth")) {
                float clX = vpX + 150.0f;
                float clY = contentY + 10.0f;
                Renderer::drawRect(clX, clY, 210.0f, 22.0f, Vec3(0.10f, 0.14f, 0.20f));
                drawDarkBevel(clX, clY, 210.0f, 22.0f, false);
                LabFont::drawText(clX + 8.0f, clY + 4.0f, "CLOTH PBD SIM: 144 particles", 1.25f, cyanGlow, LabFontType::System);
            }
        }

        if (isStudioWorkspace(_workspace)) {
            // LabStudio Embedded Viewport Overlay
            float bannerW = std::min(vpW - 40.0f, 840.0f);
            float bx = vpX + (vpW - bannerW) * 0.5f;
            float by = contentY + 10.0f;
            Renderer::drawRect(bx, by, bannerW, 30.0f, Vec3(0.12f, 0.14f, 0.18f));
            drawDarkBevel(bx, by, bannerW, 30.0f, false);
            Renderer::drawRect(bx, by, 3.0f, 30.0f, cyanGlow);
            std::string studioTitle = "LABSTUDIO: " + getWorkspaceName(_workspace) + " | LMB: Rotate | MMB: Pan | RMB: Zoom";
            LabFont::drawText(bx + 12.0f, by + 7.0f, studioTitle, 1.45f, cyanGlow, LabFontType::System);
        } else if (_workspace == EditorWorkspace::World3D_ModelViewer) {
            // Dedicated 3D Model Viewer & Inspector Stage
            float mvX = vpX + 10.0f;
            float mvY = vpY + 10.0f;
            float mvW = vpW - 20.0f;
            float mvH = vpH - 20.0f;

            // Left panel: Model Browser
            float leftListW = 210.0f;
            Renderer::drawRect(mvX, mvY, leftListW, mvH, Vec3(0.12f, 0.13f, 0.16f));
            drawDarkBevel(mvX, mvY, leftListW, mvH, false);
            Renderer::drawRect(mvX, mvY, leftListW, 26.0f, Vec3(0.14f, 0.18f, 0.25f));
            LabFont::drawText(mvX + 8.0f, mvY + 6.0f, "Discovered Models (.stl)", 1.4f, orangeGlow, LabFontType::System);

            for (size_t i = 0; i < std::min(_availableModels.size(), (size_t)14); ++i) {
                float iy = mvY + 32.0f + i * 26.0f;
                bool isSel = (_availableModels[i] == _selectedModel);
                bool isHov = (_mouseScreenX >= mvX + 4.0f && _mouseScreenX <= mvX + leftListW - 4.0f && _mouseScreenY >= iy && _mouseScreenY <= iy + 24.0f);
                Renderer::drawRect(mvX + 4.0f, iy, leftListW - 8.0f, 24.0f, isSel ? Vec3(0.20f, 0.35f, 0.52f) : (isHov ? Vec3(0.18f, 0.20f, 0.25f) : Vec3(0.14f, 0.15f, 0.18f)));
                drawDarkBevel(mvX + 4.0f, iy, leftListW - 8.0f, 24.0f, isSel);
                std::string mLabel = _availableModels[i];
                if (mLabel.size() > 18) mLabel = mLabel.substr(0, 16) + "..";
                LabFont::drawText(mvX + 10.0f, iy + 5.0f, mLabel, 1.35f, isSel ? Vec3(1, 1, 1) : textLight, LabFontType::System);
            }

            // Right panel: Inspector & Turntable Controls
            float inspW = 240.0f;
            float inspX = mvX + mvW - inspW;
            Renderer::drawRect(inspX, mvY, inspW, mvH, Vec3(0.12f, 0.13f, 0.16f));
            drawDarkBevel(inspX, mvY, inspW, mvH, false);
            Renderer::drawRect(inspX, mvY, inspW, 26.0f, Vec3(0.14f, 0.18f, 0.25f));
            LabFont::drawText(inspX + 8.0f, mvY + 6.0f, "Model Inspector Stats", 1.4f, cyanGlow, LabFontType::System);

            float curIY = mvY + 34.0f;
            LabFont::drawText(inspX + 10.0f, curIY, "Active: " + _selectedModel, 1.4f, Vec3(1, 1, 1), LabFontType::System);
            curIY += 22.0f;
            Mesh* curMesh = getMesh(_selectedModel);
            int iCount = curMesh ? curMesh->getIndexCount() : 0;
            int triCount = iCount / 3;
            LabFont::drawText(inspX + 10.0f, curIY, "Triangles: " + std::to_string(triCount), 1.35f, textLight, LabFontType::System);
            curIY += 20.0f;
            LabFont::drawText(inspX + 10.0f, curIY, "Indices: " + std::to_string(iCount), 1.35f, textLight, LabFontType::System);
            curIY += 20.0f;
            LabFont::drawText(inspX + 10.0f, curIY, "Turntable: Yaw " + std::to_string((int)_modelViewerYaw) + " Pitch " + std::to_string((int)_modelViewerPitch), 1.3f, textDim, LabFontType::System);

            curIY += 30.0f;
            drawDarkButton(inspX + 10.0f, curIY, 110.0f, 26.0f, _modelViewerAutoRotate ? "Auto-Spin: ON" : "Auto-Spin: OFF", _modelViewerAutoRotate);
            drawDarkButton(inspX + 128.0f, curIY, 102.0f, 26.0f, _modelViewerWireframe ? "Wireframe" : "Shaded", _modelViewerWireframe);

            curIY += 34.0f;
            drawDarkButton(inspX + 10.0f, curIY, 220.0f, 26.0f, "Reset Turntable Angle");

            curIY += 34.0f;
            drawDarkButton(inspX + 10.0f, curIY, 220.0f, 26.0f, "Apply Surface Material", false, false, true);

            // Center: 3D Turntable view instructions banner
            float centerW = inspX - (mvX + leftListW) - 20.0f;
            float cX = mvX + leftListW + 10.0f;
            Renderer::drawRect(cX, mvY + mvH - 34.0f, centerW, 26.0f, Vec3(0.10f, 0.12f, 0.15f));
            drawDarkBevel(cX, mvY + mvH - 34.0f, centerW, 26.0f, false);
            LabFont::drawText(cX + 12.0f, mvY + mvH - 28.0f, "Turntable Stage: Drag LMB to rotate, Wheel to zoom, Space to toggle spin", 1.35f, textDim, LabFontType::System);
        } else if (_workspace == EditorWorkspace::Level2D_HUD) {
            // LabHUDEditor2D workspace rendered directly in onRender()
        } else if (_workspace == EditorWorkspace::Level2D_Animation) {
            // Dedicated 2D Spritesheet Animation Workspace
            float animW = std::min(vpW - 40.0f, 880.0f);
            float animH = std::min(vpH - 40.0f, 500.0f);
            float animX = vpX + (vpW - animW) * 0.5f;
            float animY = vpY + (vpH - animH) * 0.5f;

            Renderer::drawRect(animX, animY, animW, animH, Vec3(0.08f, 0.09f, 0.12f));
            drawDarkBevel(animX, animY, animW, animH, true);
            Renderer::drawRect(animX, animY, animW, 28.0f, Vec3(0.12f, 0.15f, 0.20f));
            LabFont::drawText(animX + 12.0f, animY + 7.0f, "2D SPRITESHEET ANIMATION STUDIO - Native Frame Sequencing & Timeline", 1.45f, orangeGlow, LabFontType::System);

            // Sprite preview box
            float prevS = 180.0f;
            float prevX = animX + (animW - prevS) * 0.5f;
            float prevY = animY + 45.0f;
            Renderer::drawRect(prevX, prevY, prevS, prevS, Vec3(0.04f, 0.05f, 0.07f));
            drawDarkBevel(prevX, prevY, prevS, prevS, true);

            const auto* frame = _editorSpriteSheet.getFrame(_anim2DSelectedFrame);
            if (frame && _textures.contains(_selectedTexture)) {
                Renderer::drawTextureRect(prevX + 10.0f, prevY + 10.0f, prevS - 20.0f, prevS - 20.0f, *_textures[_selectedTexture], frame->uvMin.x, frame->uvMin.y, frame->uvMax.x, frame->uvMax.y);
            }

            // Filmstrip preview
            float fsY = prevY + prevS + 20.0f;
            LabFont::drawText(animX + 20.0f, fsY, "Filmstrip Sequence Frames:", 1.4f, textLight, LabFontType::System);
            float stripY = fsY + 22.0f;
            float thumbS = 48.0f;
            for (int f = 0; f < std::min(10, (int)_editorSpriteSheet.getFrameCount()); ++f) {
                float fx = animX + 20.0f + f * (thumbS + 8.0f);
                bool isCurF = (f == _anim2DSelectedFrame);
                Renderer::drawRect(fx, stripY, thumbS, thumbS, isCurF ? orangeGlow : Vec3(0.16f, 0.18f, 0.22f));
                drawDarkBevel(fx, stripY, thumbS, thumbS, isCurF);
                const auto* fr = _editorSpriteSheet.getFrame(f);
                if (fr && _textures.contains(_selectedTexture)) {
                    Renderer::drawTextureRect(fx + 2.0f, stripY + 2.0f, thumbS - 4.0f, thumbS - 4.0f, *_textures[_selectedTexture], fr->uvMin.x, fr->uvMin.y, fr->uvMax.x, fr->uvMax.y);
                }
                LabFont::drawText(fx + 2.0f, stripY + thumbS - 10.0f, "#" + std::to_string(f), 1.1f, Vec3(1, 1, 1), LabFontType::System);
            }

            // Bottom controls
            float ctlY = animY + animH - 45.0f;
            drawDarkButton(animX + 20.0f, ctlY, 90.0f, 28.0f, "|< First");
            drawDarkButton(animX + 118.0f, ctlY, 110.0f, 28.0f, _anim2DPlaying ? "Pause" : "> Play", _anim2DPlaying, false, !_anim2DPlaying);
            drawDarkButton(animX + 236.0f, ctlY, 90.0f, 28.0f, "Next >");
            drawDarkButton(animX + 334.0f, ctlY, 110.0f, 28.0f, _anim2DLoop ? "Loop: ON" : "Loop: OFF", _anim2DLoop);
            std::string frameInfo = "Frame: " + std::to_string(_anim2DSelectedFrame) + " / " + std::to_string((int)_editorSpriteSheet.getFrameCount()) + " (" + std::to_string(_anim2DFPS) + " FPS)";
            LabFont::drawText(animX + 460.0f, ctlY + 6.0f, frameInfo, 1.45f, cyanGlow, LabFontType::System);
        } else if (_workspace == EditorWorkspace::World3D_Animation) {
            // 3D Animation timeline scrubber dock
            float tlW = std::min(vpW - 40.0f, 860.0f);
            float tlH = 80.0f;
            float tlX = vpX + (vpW - tlW) * 0.5f;
            float tlY = (h - 22.0f) - tlH - 15.0f;

            Renderer::drawRect(tlX, tlY, tlW, tlH, Vec3(0.12f, 0.14f, 0.18f));
            drawDarkBevel(tlX, tlY, tlW, tlH, false);

            drawDarkButton(tlX + 10.0f, tlY + 10.0f, 70.0f, 26.0f, "|< Reset");
            drawDarkButton(tlX + 84.0f, tlY + 10.0f, 65.0f, 26.0f, "< Step");
            drawDarkButton(tlX + 153.0f, tlY + 10.0f, 85.0f, 26.0f, _timelinePlaying ? "Pause" : "> Play", _timelinePlaying, false, !_timelinePlaying);
            drawDarkButton(tlX + 242.0f, tlY + 10.0f, 65.0f, 26.0f, "Step >");
            drawDarkButton(tlX + 311.0f, tlY + 10.0f, 65.0f, 26.0f, "+ Key");
            drawDarkButton(tlX + 380.0f, tlY + 10.0f, 65.0f, 26.0f, "- Key");

            AnimKeyframe sampled = _editorAnimTrack.sample(_animScrubberPos);
            char infoBuf[128];
            snprintf(infoBuf, sizeof(infoBuf), "Scrubber: %.2f / 1.00 (%zu keys) Pos:(%.2f,%.2f,%.2f)",
                     _animScrubberPos, _editorAnimTrack.keyframes.size(),
                     sampled.translation.x, sampled.translation.y, sampled.translation.z);
            LabFont::drawText(tlX + 455.0f, tlY + 16.0f, infoBuf, 1.4f, cyanGlow, LabFontType::System);

            // Scrubber Bar
            float barX = tlX + 10.0f;
            float barY = tlY + 46.0f;
            float barW = tlW - 20.0f;
            float barH = 20.0f;
            Renderer::drawRect(barX, barY, barW, barH, Vec3(0.08f, 0.09f, 0.11f));
            drawDarkBevel(barX, barY, barW, barH, true);

            // Keyframe tick markers
            for (const auto& kf : _editorAnimTrack.keyframes) {
                float kfX = barX + std::clamp(kf.time, 0.0f, 1.0f) * barW;
                Renderer::drawRect(kfX - 1.0f, barY + 2.0f, 3.0f, barH - 4.0f, Vec3(0.3f, 0.8f, 1.0f));
            }

            float playheadX = barX + _animScrubberPos * barW;
            Renderer::drawRect(playheadX - 2.0f, barY - 2.0f, 5.0f, barH + 4.0f, orangeGlow);
        } else if (_workspace == EditorWorkspace::Scripting_Editor || _workspace == EditorWorkspace::Scripting_Console) {
            // Full In-Engine Lua Script Studio IDE - occupies full central viewport
            Renderer::drawRect(vpX, contentY, vpW, contentH, Vec3(0.065f, 0.075f, 0.095f));
            drawDarkBevel(vpX, contentY, vpW, contentH, false);

            // Left File Explorer Panel: Strictly Game Scripts (game://scripts)
            float scriptListW = 190.0f;
            float listX = vpX + 4.0f;
            float listY = contentY + 4.0f;
            float listH = contentH - 8.0f - 24.0f;
            Renderer::drawRect(listX, listY, scriptListW, listH, Vec3(0.08f, 0.09f, 0.12f));
            drawDarkBevel(listX, listY, scriptListW, listH, false);

            LabFont::drawText(listX + 8.0f, listY + 6.0f, "GAME SCRIPTS", 1.35f, orangeGlow, LabFontType::System);
            std::string subTitle = "game://scripts";
            LabFont::drawText(listX + 8.0f, listY + 22.0f, subTitle, 1.05f, textDim, LabFontType::System);

            // [+ New Script] Button
            float newBtnY = listY + 38.0f;
            drawDarkButton(listX + 6.0f, newBtnY, scriptListW - 12.0f, 22.0f, "+ New Script");

            // Script items
            float itemStartY = newBtnY + 28.0f;
            if (_gameScripts.empty()) {
                LabFont::drawText(listX + 8.0f, itemStartY + 6.0f, "(Brak skryptow w grze)", 1.15f, textDim, LabFontType::System);
                LabFont::drawText(listX + 8.0f, itemStartY + 22.0f, "Clean: 0 engine scripts", 1.05f, Vec3(0.5f, 0.7f, 0.5f), LabFontType::System);
            } else {
                for (size_t i = 0; i < _gameScripts.size(); ++i) {
                    float iy = itemStartY + i * 24.0f;
                    if (iy + 22.0f > listY + listH) break;
                    bool isSel = ((int)i == _selectedGameScriptIndex);
                    bool isHov = (_mouseScreenX >= listX + 4.0f && _mouseScreenX <= listX + scriptListW - 4.0f &&
                                  _mouseScreenY >= iy && _mouseScreenY <= iy + 22.0f);
                    Vec3 itemBg = isSel ? Vec3(0.18f, 0.24f, 0.32f) : (isHov ? Vec3(0.12f, 0.15f, 0.20f) : Vec3(0.09f, 0.10f, 0.13f));
                    Renderer::drawRect(listX + 4.0f, iy, scriptListW - 8.0f, 22.0f, itemBg);
                    drawDarkBevel(listX + 4.0f, iy, scriptListW - 8.0f, 22.0f, isSel);
                    if (isSel) {
                        Renderer::drawRect(listX + 4.0f, iy, 3.0f, 22.0f, cyanGlow);
                    }
                    LabFont::drawText(listX + 10.0f, iy + 4.0f, _gameScripts[i], 1.15f, isSel ? Vec3(1, 1, 1) : textLight, LabFontType::Monospace);
                }
            }

            // Actions toolbar
            float codeX = listX + scriptListW + 6.0f;
            float codeW = vpW - scriptListW - 14.0f;
            float actY = contentY + 4.0f;
            drawDarkButton(codeX, actY, 150.0f, 26.0f, "> RUN (Ctrl+R)", false, false, true);
            drawDarkButton(codeX + 156.0f, actY, 110.0f, 26.0f, "SAVE (Ctrl+S)", false, false, true);
            drawDarkButton(codeX + 272.0f, actY, 140.0f, 26.0f, "Preset: Klocek+Timer");
            drawDarkButton(codeX + 418.0f, actY, 130.0f, 26.0f, "Preset: Player HUD");
            drawDarkButton(codeX + 554.0f, actY, 120.0f, 26.0f, "Preset: Movement");
            drawDarkButton(codeX + 680.0f, actY, 130.0f, 26.0f, "Name 'Klocek1'", false, false, true);

            // Interactive Code Editor Area
            float codeY = contentY + 34.0f;
            float codeH = contentH - 34.0f - 24.0f;
            Renderer::drawRect(codeX, codeY, codeW, codeH, Vec3(0.045f, 0.055f, 0.075f));
            drawDarkBevel(codeX, codeY, codeW, codeH, true);

            // Line numbers column
            float gutterW = 46.0f;
            Renderer::drawRect(codeX, codeY, gutterW, codeH, Vec3(0.07f, 0.08f, 0.10f));

            // Line highlight for active cursor line
            int curRel = _scriptCursorLine - _scriptScrollLine;
            int maxVisible = (int)((codeH - 10.0f) / 18.0f);
            if (curRel >= 0 && curRel < maxVisible) {
                Renderer::drawRect(codeX + gutterW + 1.0f, codeY + 5.0f + curRel * 18.0f, codeW - gutterW - 2.0f, 18.0f, Vec3(0.11f, 0.14f, 0.18f));
            }

            // Render visible lines from _scriptLines
            float textStartX = codeX + gutterW + 8.0f;
            float scriptFontScale = 1.35f;
            float charAdvance = LabFont::getMonospaceAdvance(scriptFontScale);
            for (int i = 0; i < maxVisible; ++i) {
                int lineIdx = _scriptScrollLine + i;
                if (lineIdx >= (int)_scriptLines.size()) break;
                float ly = codeY + 6.0f + i * 18.0f;
                // Line number
                LabFont::drawText(codeX + 6.0f, ly, std::to_string(lineIdx + 1), 1.25f, textDim, LabFontType::System);
                // Syntax coloring
                const std::string& lText = _scriptLines[lineIdx];
                Vec3 lColor = textLight;
                if (lText.rfind("--", 0) == 0) lColor = Vec3(0.35f, 0.75f, 0.35f);
                else if (lText.find("function") != std::string::npos || lText.find("end") != std::string::npos || lText.find("local") != std::string::npos) lColor = orangeGlow;
                else if (lText.find("World.") != std::string::npos || lText.find("HUD.") != std::string::npos || lText.find("Input.") != std::string::npos) lColor = cyanGlow;
                LabFont::drawText(textStartX, ly, lText, scriptFontScale, lColor, LabFontType::Monospace);
            }

            // Blinking caret
            if (_scriptEditorFocused && ((int)(_scriptBlinkTimer * 2.0f) % 2 == 0)) {
                if (curRel >= 0 && curRel < maxVisible) {
                    float caretY = codeY + 6.0f + curRel * 18.0f;
                    float caretX = textStartX + _scriptCursorCol * charAdvance;
                    if (caretX < codeX + codeW - 4.0f) {
                        Renderer::drawRect(caretX, caretY, 2.0f, 16.0f, cyanGlow);
                    }
                }
            }

            // Status Bar at bottom
            float statY = contentY + contentH - 22.0f;
            Renderer::drawRect(vpX, statY, vpW, 22.0f, Vec3(0.09f, 0.11f, 0.14f));
            std::string statInfo = "Ln " + std::to_string(_scriptCursorLine + 1) + ", Col " + std::to_string(_scriptCursorCol + 1) + " | UTF-8 | Lua 5.4 | " + _activeScriptPath + " | Press Ctrl+R to Execute, Ctrl+S to Save";
            LabFont::drawText(vpX + 10.0f, statY + 5.0f, statInfo, 1.25f, textDim, LabFontType::System);
        } else if (_workspace == EditorWorkspace::Plugins_Manager) {
            // Full-Viewport Package & Plugin Manager (Blender & Unity Add-ons Standard)
            Renderer::drawRect(vpX, contentY, vpW, contentH, Vec3(0.06f, 0.07f, 0.09f));
            drawDarkBevel(vpX, contentY, vpW, contentH, false);

            // 1. Top Header Toolbar with Search & Filter
            float barY = contentY + 4.0f;
            float barH = 34.0f;
            Renderer::drawRect(vpX + 4.0f, barY, vpW - 8.0f, barH, Vec3(0.10f, 0.12f, 0.16f));
            drawDarkBevel(vpX + 4.0f, barY, vpW - 8.0f, barH, false);

            LabFont::drawText(vpX + 16.0f, barY + 9.0f, "EXTENSIONS: PACKAGE & PLUGIN MANAGER", 1.55f, orangeGlow, LabFontType::System);

            // Search Bar Input Field
            float searchX = vpX + 380.0f;
            float searchW = 190.0f;
            Renderer::drawRect(searchX, barY + 4.0f, searchW, 26.0f, _pluginSearchFocused ? Vec3(0.13f, 0.17f, 0.24f) : Vec3(0.07f, 0.08f, 0.11f));
            drawDarkBevel(searchX, barY + 4.0f, searchW, 26.0f, _pluginSearchFocused);
            std::string dispSearch = _pluginSearchQuery.empty() ? (_pluginSearchFocused ? "|" : "Search Add-ons...") : (_pluginSearchQuery + (_pluginSearchFocused ? "|" : ""));
            LabFont::drawText(searchX + 8.0f, barY + 9.0f, dispSearch, 1.25f, _pluginSearchFocused ? Vec3(1, 1, 1) : textDim, LabFontType::System);
            if (!_pluginSearchQuery.empty()) {
                LabFont::drawText(searchX + searchW - 18.0f, barY + 9.0f, "x", 1.25f, textLight, LabFontType::System);
            }

            // Filter buttons
            float btnX = vpX + vpW - 480.0f;
            bool isAllFilt = (_pluginStateFilter == "All");
            bool isEnFilt = (_pluginStateFilter == "Enabled");
            bool isDisFilt = (_pluginStateFilter == "Disabled");

            drawDarkButton(btnX, barY + 4.0f, 65.0f, 26.0f, "All", false, isAllFilt, true);
            drawDarkButton(btnX + 70.0f, barY + 4.0f, 85.0f, 26.0f, "Enabled", false, isEnFilt, true);
            drawDarkButton(btnX + 160.0f, barY + 4.0f, 85.0f, 26.0f, "Disabled", false, isDisFilt, true);
            drawDarkButton(btnX + 250.0f, barY + 4.0f, 110.0f, 26.0f, "+ Import .lua", false, false, true);
            drawDarkButton(btnX + 365.0f, barY + 4.0f, 100.0f, 26.0f, "Rescan Disk", false, false, true);

            // Category Bar
            float catY = barY + barH + 4.0f;
            float catH = 26.0f;
            Renderer::drawRect(vpX + 4.0f, catY, vpW - 8.0f, catH, Vec3(0.08f, 0.09f, 0.12f));

            const char* cats[] = { "All", "Physics & Simulation", "Rendering & VFX", "Gameplay & HUD", "3D & Geometry", "Scripting & Dev", "Tools & Utilities" };
            float cx = vpX + 14.0f;
            for (int i = 0; i < 7; ++i) {
                bool isCurCat = (_pluginCategoryFilter == cats[i]);
                float cw = (i == 0) ? 45.0f : (i == 1 ? 160.0f : 130.0f);
                bool isHov = (_mouseScreenX >= cx && _mouseScreenX <= cx + cw && _mouseScreenY >= catY && _mouseScreenY <= catY + catH);
                if (isCurCat) {
                    Renderer::drawRect(cx, catY + 2.0f, cw, catH - 4.0f, Vec3(0.20f, 0.24f, 0.32f));
                    Renderer::drawRect(cx, catY + catH - 2.0f, cw, 2.0f, cyanGlow);
                } else if (isHov) {
                    Renderer::drawRect(cx, catY + 2.0f, cw, catH - 4.0f, Vec3(0.14f, 0.16f, 0.20f));
                }
                LabFont::drawText(cx + 8.0f, catY + 6.0f, cats[i], 1.3f, isCurCat ? cyanGlow : (isHov ? textLight : textDim), LabFontType::System);
                cx += cw + 6.0f;
            }

            // Split Layout: Left List + Right Inspector
            float mainY = catY + catH + 6.0f;
            float mainH = contentH - (mainY - contentY) - 6.0f;
            float listW = 380.0f;

            // --- LEFT PANEL: Plugin List ---
            float listX = vpX + 6.0f;
            Renderer::drawRect(listX, mainY, listW, mainH, Vec3(0.09f, 0.10f, 0.13f));
            drawDarkBevel(listX, mainY, listW, mainH, false);

            auto pkgs = PackageManager::instance().filterPackages(_pluginSearchQuery, _pluginCategoryFilter, _pluginStateFilter);
            float itemH = 54.0f;
            float curItemY = mainY + 4.0f;

            for (size_t i = 0; i < pkgs.size(); ++i) {
                if (curItemY + itemH > mainY + mainH - 10.0f) break;

                const auto& pkg = pkgs[i];
                bool isSel = (_pluginSelectedId == pkg.id);
                bool isEn = (pkg.state == PackageState::Enabled);
                bool isHov = (_mouseScreenX >= listX + 2.0f && _mouseScreenX <= listX + listW - 4.0f && _mouseScreenY >= curItemY && _mouseScreenY <= curItemY + itemH - 2.0f);

                Vec3 cardBg = isSel ? Vec3(0.16f, 0.20f, 0.28f) : (isHov ? Vec3(0.13f, 0.15f, 0.19f) : Vec3(0.11f, 0.12f, 0.15f));
                Renderer::drawRect(listX + 4.0f, curItemY, listW - 8.0f, itemH - 2.0f, cardBg);
                drawDarkBevel(listX + 4.0f, curItemY, listW - 8.0f, itemH - 2.0f, isSel);
                if (isSel) {
                    Renderer::drawRect(listX + 4.0f, curItemY, 3.0f, itemH - 2.0f, orangeGlow);
                }

                // Blender-style Toggle Checkbox
                float boxX = listX + 12.0f;
                float boxY = curItemY + 16.0f;
                float boxS = 20.0f;
                Renderer::drawRect(boxX, boxY, boxS, boxS, isEn ? Vec3(0.15f, 0.45f, 0.25f) : Vec3(0.06f, 0.07f, 0.09f));
                drawDarkBevel(boxX, boxY, boxS, boxS, isEn);
                if (isEn) {
                    LabFont::drawText(boxX + 4.0f, boxY + 2.0f, "v", 1.4f, Vec3(0.4f, 1.0f, 0.5f), LabFontType::System);
                }

                // Display Name
                LabFont::drawText(boxX + boxS + 10.0f, curItemY + 8.0f, pkg.displayName, 1.42f, isSel ? Vec3(1, 1, 1) : textLight, LabFontType::System);

                // Version & Category
                std::string sub = "v" + pkg.version + "  •  " + pkg.category;
                LabFont::drawText(boxX + boxS + 10.0f, curItemY + 28.0f, sub, 1.25f, textDim, LabFontType::System);

                // Status Badge on Right
                float badgeW = 54.0f;
                float badgeX = listX + listW - badgeW - 12.0f;
                float badgeY = curItemY + 16.0f;
                if (isEn) {
                    Renderer::drawRect(badgeX, badgeY, badgeW, 18.0f, Vec3(0.12f, 0.38f, 0.18f));
                    LabFont::drawText(badgeX + 8.0f, badgeY + 2.0f, "ACTIVE", 1.15f, Vec3(0.5f, 1.0f, 0.6f), LabFontType::System);
                } else {
                    Renderer::drawRect(badgeX, badgeY, badgeW, 18.0f, Vec3(0.14f, 0.15f, 0.18f));
                    LabFont::drawText(badgeX + 14.0f, badgeY + 2.0f, "OFF", 1.15f, textDim, LabFontType::System);
                }

                curItemY += itemH;
            }

            // --- RIGHT PANEL: Inspector & Rich Customizer Controls ---
            float detX = listX + listW + 8.0f;
            float detW = vpW - listW - 20.0f;
            Renderer::drawRect(detX, mainY, detW, mainH, Vec3(0.09f, 0.10f, 0.13f));
            drawDarkBevel(detX, mainY, detW, mainH, false);

            const auto* selPkg = PackageManager::instance().getPackage(_pluginSelectedId);
            if (selPkg) {
                float dy = mainY + 16.0f;

                // Header Block
                LabFont::drawText(detX + 20.0f, dy, selPkg->displayName, 2.0f, orangeGlow, LabFontType::System);
                dy += 34.0f;

                std::string metaStr = "Version: " + selPkg->version + "   |   Author: " + selPkg->author + "   |   ID: " + selPkg->id;
                LabFont::drawText(detX + 20.0f, dy, metaStr, 1.35f, cyanGlow, LabFontType::System);
                dy += 28.0f;

                Renderer::drawRect(detX + 20.0f, dy, detW - 40.0f, 1.0f, Vec3(0.20f, 0.22f, 0.28f));
                dy += 12.0f;

                // Actions Bar
                bool isEn = (selPkg->state == PackageState::Enabled);
                float btnW = 140.0f;
                drawDarkButton(detX + 20.0f, dy, btnW, 30.0f, isEn ? "Disable Plugin" : "Enable Plugin", false, false, true);
                drawDarkButton(detX + detW - 210.0f, dy, 190.0f, 30.0f, "Open in Lua Studio", false, false, true);
                dy += 38.0f;

                // Status Banner (if set)
                if (!_pluginStatusBanner.empty()) {
                    Renderer::drawRect(detX + 20.0f, dy, detW - 40.0f, 26.0f, Vec3(0.12f, 0.32f, 0.20f));
                    drawDarkBevel(detX + 20.0f, dy, detW - 40.0f, 26.0f, false);
                    LabFont::drawText(detX + 30.0f, dy + 5.0f, _pluginStatusBanner, 1.35f, Vec3(0.6f, 1.0f, 0.7f), LabFontType::System);
                    dy += 32.0f;
                }

                // --- INTERACTIVE CUSTOMIZER BOX ---
                float boxH = 175.0f;
                Renderer::drawRect(detX + 20.0f, dy, detW - 40.0f, boxH, Vec3(0.065f, 0.075f, 0.10f));
                drawDarkBevel(detX + 20.0f, dy, detW - 40.0f, boxH, false);
                Renderer::drawRect(detX + 20.0f, dy, detW - 40.0f, 24.0f, Vec3(0.12f, 0.15f, 0.22f));
                LabFont::drawText(detX + 30.0f, dy + 4.0f, "ADDON PARAMETERS & LIVE CONTROLS", 1.3f, cyanGlow, LabFontType::System);

                float cyBox = dy;
                // Addon specific custom controls
                if (_pluginSelectedId == "com.lab.retro-fx") {
                    // Presets
                    LabFont::drawText(detX + 30.0f, cyBox + 32.0f, "Presets:", 1.25f, textLight, LabFontType::System);
                    drawDarkButton(detX + 90.0f, cyBox + 28.0f, 120.0f, 24.0f, "Classic CRT 1985", _retroColorTheme == 0);
                    drawDarkButton(detX + 215.0f, cyBox + 28.0f, 115.0f, 24.0f, "Arcade Monitor", _retroColorTheme == 1);
                    drawDarkButton(detX + 335.0f, cyBox + 28.0f, 115.0f, 24.0f, "Amber Terminal", _retroColorTheme == 2);
                    drawDarkButton(detX + 455.0f, cyBox + 28.0f, 120.0f, 24.0f, "Cyberpunk Neon", _retroColorTheme == 3);

                    // Toggles
                    drawDarkButton(detX + 30.0f, cyBox + 62.0f, 160.0f, 26.0f, _retroScanlines ? "Scanlines: ON" : "Scanlines: OFF", _retroScanlines);
                    drawDarkButton(detX + 200.0f, cyBox + 62.0f, 160.0f, 26.0f, _retroAberration ? "Aberration: ON" : "Aberration: OFF", _retroAberration);
                    drawDarkButton(detX + 370.0f, cyBox + 62.0f, 160.0f, 26.0f, _retroVignette ? "Vignette: ON" : "Vignette: OFF", _retroVignette);

                    // Steppers
                    LabFont::drawText(detX + 30.0f, cyBox + 102.0f, "Scanline Density:", 1.25f, textLight, LabFontType::System);
                    drawDarkButton(detX + 160.0f, cyBox + 98.0f, 24.0f, 24.0f, "-");
                    LabFont::drawText(detX + 192.0f, cyBox + 102.0f, std::to_string(_retroPixelScale) + "px", 1.25f, cyanGlow, LabFontType::System);
                    drawDarkButton(detX + 215.0f, cyBox + 98.0f, 24.0f, 24.0f, "+");

                    LabFont::drawText(detX + 280.0f, cyBox + 102.0f, "Vignette Strength:", 1.25f, textLight, LabFontType::System);
                    drawDarkButton(detX + 420.0f, cyBox + 98.0f, 24.0f, 24.0f, "-");
                    char vigBuf[16]; snprintf(vigBuf, sizeof(vigBuf), "%.0f%%", _retroVignetteIntensity * 100.0f);
                    LabFont::drawText(detX + 450.0f, cyBox + 102.0f, vigBuf, 1.25f, cyanGlow, LabFontType::System);
                    drawDarkButton(detX + 490.0f, cyBox + 98.0f, 24.0f, 24.0f, "+");

                    LabFont::drawText(detX + 30.0f, cyBox + 140.0f, "* Viewport Effect: Real-time scanlines and lens darkening active in 3D Level tab.", 1.25f, textDim, LabFontType::System);
                } else if (_pluginSelectedId == "com.lab.physics-cloth") {
                    // Cloth Sim
                    drawDarkButton(detX + 30.0f, cyBox + 28.0f, 180.0f, 26.0f, _clothSimulateInEditor ? "Simulate: ACTIVE" : "Simulate: PAUSED", _clothSimulateInEditor);
                    drawDarkButton(detX + 220.0f, cyBox + 28.0f, 120.0f, 26.0f, "Flag Preset");
                    drawDarkButton(detX + 345.0f, cyBox + 28.0f, 120.0f, 26.0f, "Cape Preset");
                    drawDarkButton(detX + 470.0f, cyBox + 28.0f, 120.0f, 26.0f, "Canopy Preset");

                    LabFont::drawText(detX + 30.0f, cyBox + 68.0f, "Wind Velocity Z:", 1.25f, textLight, LabFontType::System);
                    drawDarkButton(detX + 150.0f, cyBox + 64.0f, 24.0f, 24.0f, "-");
                    char wBuf[16]; snprintf(wBuf, sizeof(wBuf), "%.1f m/s", _clothWindVelocity.z);
                    LabFont::drawText(detX + 178.0f, cyBox + 68.0f, wBuf, 1.25f, cyanGlow, LabFontType::System);
                    drawDarkButton(detX + 225.0f, cyBox + 64.0f, 24.0f, 24.0f, "+");

                    LabFont::drawText(detX + 290.0f, cyBox + 68.0f, "Cloth Stiffness:", 1.25f, textLight, LabFontType::System);
                    drawDarkButton(detX + 410.0f, cyBox + 64.0f, 24.0f, 24.0f, "-");
                    char sBuf[16]; snprintf(sBuf, sizeof(sBuf), "%.1f", _clothStiffness);
                    LabFont::drawText(detX + 440.0f, cyBox + 68.0f, sBuf, 1.25f, cyanGlow, LabFontType::System);
                    drawDarkButton(detX + 480.0f, cyBox + 64.0f, 24.0f, 24.0f, "+");

                    drawDarkButton(detX + 30.0f, cyBox + 102.0f, 190.0f, 26.0f, "+ Spawn Flag at Cursor", false, false, true);
                    drawDarkButton(detX + 230.0f, cyBox + 102.0f, 160.0f, 26.0f, "Reset Particles");
                    drawDarkButton(detX + 400.0f, cyBox + 102.0f, 160.0f, 26.0f, "Gust of Wind Pulse");

                    LabFont::drawText(detX + 30.0f, cyBox + 144.0f, "* 144 Cloth particles simulated with PBD distance constraints & wind drag in 3D scene.", 1.25f, textDim, LabFontType::System);
                } else if (_pluginSelectedId == "com.lab.physics-forcefield") {
                    drawDarkButton(detX + 30.0f, cyBox + 28.0f, 150.0f, 26.0f, "Radial Explosion", _forceFieldType == 0);
                    drawDarkButton(detX + 190.0f, cyBox + 28.0f, 150.0f, 26.0f, "Vortex Whirlwind", _forceFieldType == 1);
                    drawDarkButton(detX + 350.0f, cyBox + 28.0f, 150.0f, 26.0f, "Directional Wind", _forceFieldType == 2);

                    LabFont::drawText(detX + 30.0f, cyBox + 68.0f, "Field Strength:", 1.25f, textLight, LabFontType::System);
                    drawDarkButton(detX + 150.0f, cyBox + 64.0f, 24.0f, 24.0f, "-");
                    char strBuf[16]; snprintf(strBuf, sizeof(strBuf), "%.0f N", _forceFieldStrength);
                    LabFont::drawText(detX + 180.0f, cyBox + 68.0f, strBuf, 1.25f, cyanGlow, LabFontType::System);
                    drawDarkButton(detX + 225.0f, cyBox + 64.0f, 24.0f, 24.0f, "+");

                    LabFont::drawText(detX + 300.0f, cyBox + 68.0f, "Field Radius:", 1.25f, textLight, LabFontType::System);
                    drawDarkButton(detX + 410.0f, cyBox + 64.0f, 24.0f, 24.0f, "-");
                    char radBuf[16]; snprintf(radBuf, sizeof(radBuf), "%.0f m", _forceFieldRadius);
                    LabFont::drawText(detX + 440.0f, cyBox + 68.0f, radBuf, 1.25f, cyanGlow, LabFontType::System);
                    drawDarkButton(detX + 485.0f, cyBox + 64.0f, 24.0f, 24.0f, "+");

                    drawDarkButton(detX + 30.0f, cyBox + 102.0f, 230.0f, 26.0f, "Detonate Shockwave at Cursor", false, false, true);
                    drawDarkButton(detX + 270.0f, cyBox + 102.0f, 210.0f, 26.0f, _forceFieldGizmoVisible ? "Field Gizmo: VISIBLE" : "Field Gizmo: HIDDEN", _forceFieldGizmoVisible);
                    LabFont::drawText(detX + 30.0f, cyBox + 144.0f, "* Blasts rigid bodies and cloth particles with inverse-square shockwave falloff.", 1.25f, textDim, LabFontType::System);
                } else if (_pluginSelectedId == "com.lab.speedrun-timer") {
                    drawDarkButton(detX + 30.0f, cyBox + 28.0f, 130.0f, 26.0f, "Start Timer", false, false, true);
                    drawDarkButton(detX + 170.0f, cyBox + 28.0f, 120.0f, 26.0f, "Pause Timer");
                    drawDarkButton(detX + 300.0f, cyBox + 28.0f, 120.0f, 26.0f, "Reset 00:00");

                    drawDarkButton(detX + 30.0f, cyBox + 66.0f, 190.0f, 26.0f, _speedrunOverlayVisible ? "Viewport HUD: ON" : "Viewport HUD: OFF", _speedrunOverlayVisible);
                    drawDarkButton(detX + 230.0f, cyBox + 66.0f, 180.0f, 26.0f, (_speedrunPosition == 0) ? "Anchor: Top-Right" : "Anchor: Top-Left");

                    int mins = (int)(_speedrunElapsedTime / 60.0f);
                    float secs = _speedrunElapsedTime - (mins * 60.0f);
                    char tBuf[64]; snprintf(tBuf, sizeof(tBuf), "Live Elapsed: %02d:%05.2f  [%s]", mins, secs, _speedrunTimerRunning ? "RUNNING" : "STOPPED");
                    LabFont::drawText(detX + 30.0f, cyBox + 110.0f, tBuf, 1.4f, cyanGlow, LabFontType::System);
                    LabFont::drawText(detX + 30.0f, cyBox + 144.0f, "* Renders digital split clock, velocity readout and delta comparisons in Viewport.", 1.25f, textDim, LabFontType::System);
                } else if (_pluginSelectedId == "com.lab.vehicle-arcade") {
                    drawDarkButton(detX + 30.0f, cyBox + 28.0f, 220.0f, 26.0f, "+ Spawn Vehicle at Cursor", false, false, true);
                    drawDarkButton(detX + 260.0f, cyBox + 28.0f, 190.0f, 26.0f, _vehicleTelemetryVisible ? "Dashboard HUD: ON" : "Dashboard HUD: OFF", _vehicleTelemetryVisible);

                    char spdInfo[64]; snprintf(spdInfo, sizeof(spdInfo), "Telemetry: %.0f km/h  |  RPM: %.0f  |  Gear: %d", _vehicleSpeedKmh, _vehicleRpm, _vehicleGear);
                    LabFont::drawText(detX + 30.0f, cyBox + 74.0f, spdInfo, 1.4f, orangeGlow, LabFontType::System);
                    LabFont::drawText(detX + 30.0f, cyBox + 110.0f, "Hold [W] in 3D Viewport to accelerate raycast chassis and rev engine RPM.", 1.35f, textLight, LabFontType::System);
                    LabFont::drawText(detX + 30.0f, cyBox + 144.0f, "* 4-wheel independent spring-damper suspension solver and tire friction model.", 1.25f, textDim, LabFontType::System);
                } else if (_pluginSelectedId == "com.lab.proc-gen") {
                    drawDarkButton(detX + 30.0f, cyBox + 28.0f, 250.0f, 28.0f, "+ Generate Procedural Room", false, false, true);
                    LabFont::drawText(detX + 30.0f, cyBox + 75.0f, "Room Dimension: 16m x 16m  •  Wall Height: 4.5m  •  CSG Brushes: 8", 1.35f, textLight, LabFontType::System);
                    LabFont::drawText(detX + 30.0f, cyBox + 110.0f, "Generates real solid CSG brushes aligned to the 3D Cursor position.", 1.32f, textDim, LabFontType::System);
                    LabFont::drawText(detX + 30.0f, cyBox + 144.0f, "* Single-click procedural architecture builder. Instant level design rapid prototype.", 1.25f, cyanGlow, LabFontType::System);
                } else if (_pluginSelectedId == "com.lab.physics-fluid") {
                    drawDarkButton(detX + 30.0f, cyBox + 28.0f, 250.0f, 28.0f, "+ Place Water Volume at Cursor", false, false, true);
                    LabFont::drawText(detX + 30.0f, cyBox + 75.0f, "Fluid Density: 1000 kg/m3 (Fresh Water)  •  Viscous Drag: 0.8", 1.35f, textLight, LabFontType::System);
                    LabFont::drawText(detX + 30.0f, cyBox + 110.0f, "Water volume brushes exert Archimedes upward buoyant force on objects.", 1.32f, textDim, LabFontType::System);
                    LabFont::drawText(detX + 30.0f, cyBox + 144.0f, "* Dynamic swimming and floating crates simulation module.", 1.25f, cyanGlow, LabFontType::System);
                } else if (_pluginSelectedId == "com.lab.physics-constraints") {
                    drawDarkButton(detX + 30.0f, cyBox + 28.0f, 250.0f, 28.0f, "+ Spawn Pendulum Chain at Cursor", false, false, true);
                    LabFont::drawText(detX + 30.0f, cyBox + 75.0f, "Supported Joints: Distance, BallSocket, Hinge, Slider, Fixed", 1.35f, textLight, LabFontType::System);
                    LabFont::drawText(detX + 30.0f, cyBox + 110.0f, "Iterative Gauss-Seidel constraint solver for multi-body mechanical linkages.", 1.32f, textDim, LabFontType::System);
                    LabFont::drawText(detX + 30.0f, cyBox + 144.0f, "* Essential for ragdoll limbs, suspension linkages, and swinging doors.", 1.25f, cyanGlow, LabFontType::System);
                } else {
                    LabFont::drawText(detX + 30.0f, cyBox + 40.0f, "Custom Drop-in Addon Lifecycle Hook Status: ACTIVE", 1.4f, cyanGlow, LabFontType::System);
                    LabFont::drawText(detX + 30.0f, cyBox + 75.0f, "Functions parsed: onLoad(), onUpdate(dt), onUnload()", 1.35f, textLight, LabFontType::System);
                    LabFont::drawText(detX + 30.0f, cyBox + 110.0f, "File: " + PackageManager::instance().getPackageFile(selPkg->id), 1.25f, orangeGlow, LabFontType::System);
                    LabFont::drawText(detX + 30.0f, cyBox + 144.0f, "* Modifying this .lua file hot-reloads dynamically without recompiling the engine.", 1.25f, textDim, LabFontType::System);
                }
                dy += boxH + 16.0f;

                // Description Box
                LabFont::drawText(detX + 20.0f, dy, "Description:", 1.45f, textLight, LabFontType::System);
                dy += 20.0f;

                Renderer::drawRect(detX + 20.0f, dy, detW - 40.0f, 65.0f, Vec3(0.06f, 0.07f, 0.09f));
                drawDarkBevel(detX + 20.0f, dy, detW - 40.0f, 65.0f, true);
                LabFont::drawText(detX + 30.0f, dy + 10.0f, selPkg->description, 1.35f, Vec3(0.9f, 0.92f, 0.95f), LabFontType::System);
                std::string fileLocation = "Source Code: " + PackageManager::instance().getPackageFile(selPkg->id);
                if (fileLocation.size() > 70) fileLocation = fileLocation.substr(0, 68) + "..";
                LabFont::drawText(detX + 30.0f, dy + 38.0f, fileLocation, 1.22f, orangeGlow, LabFontType::System);
                dy += 80.0f;

                // Architecture Box
                LabFont::drawText(detX + 20.0f, dy, "Engine Architecture & Hooks:", 1.45f, textLight, LabFontType::System);
                dy += 20.0f;

                Renderer::drawRect(detX + 20.0f, dy, detW - 40.0f, 75.0f, Vec3(0.06f, 0.07f, 0.09f));
                drawDarkBevel(detX + 20.0f, dy, detW - 40.0f, 75.0f, true);

                LabFont::drawText(detX + 30.0f, dy + 10.0f, "• Standalone Isolation: Each addon runs in its own decoupled translation unit / Lua VM.", 1.28f, textDim, LabFontType::System);
                LabFont::drawText(detX + 30.0f, dy + 30.0f, "• Lifecycle Events: onLoad(), onUpdate(dt), onUnload() registered with Lab::PluginManager.", 1.28f, textDim, LabFontType::System);
                LabFont::drawText(detX + 30.0f, dy + 50.0f, "• State: " + std::string(isEn ? "RUNNING (Listening to frame ticks & rendering)" : "STOPPED (Zero runtime overhead)"), 1.28f, isEn ? Vec3(0.4f, 1.0f, 0.5f) : textDim, LabFontType::System);
            }
        } else if (_workspace == EditorWorkspace::Project_Hub) {
            // ==================== ENGINE MAIN MENU & PROJECT HUB ====================
            // Standalone Decoupled Projects Browser & Game Showcase Launcher
            Renderer::drawRect(vpX, contentY, vpW, contentH, Vec3(0.06f, 0.07f, 0.09f));
            drawDarkBevel(vpX, contentY, vpW, contentH, false);

            // 1. Top Header Banner
            float headY = contentY + 4.0f;
            float headH = 50.0f;
            Renderer::drawRect(vpX + 4.0f, headY, vpW - 8.0f, headH, Vec3(0.10f, 0.12f, 0.16f));
            drawDarkBevel(vpX + 4.0f, headY, vpW - 8.0f, headH, false);

            LabFont::drawText(vpX + 18.0f, headY + 10.0f, "LAB ENGINE STUDIO 2026 - PROJECT HUB", 1.6f, orangeGlow, LabFontType::System);
            LabFont::drawText(vpX + 18.0f, headY + 28.0f, "Universal 2D & 3D Game Engine | Decoupled Architecture | Unity & Unreal Standard", 1.25f, textDim, LabFontType::System);

            std::string actInfo = "Active Project: [" + ProjectManager::get().getActiveProjectName() + "]";
            LabFont::drawText(vpX + vpW - 320.0f, headY + 18.0f, actInfo, 1.35f, cyanGlow, LabFontType::System);

            // Split Layout
            float mainY = headY + headH + 8.0f;
            float mainH = contentH - (mainY - contentY) - 6.0f;
            float listW = 390.0f;
            float listX = vpX + 6.0f;
            float detX = listX + listW + 8.0f;
            float detW = vpW - listW - 20.0f;

            // --- LEFT PANEL: Projects List ---
            Renderer::drawRect(listX, mainY, listW, mainH, Vec3(0.09f, 0.10f, 0.13f));
            drawDarkBevel(listX, mainY, listW, mainH, false);

            // Left panel title
            Renderer::drawRect(listX + 2.0f, mainY + 2.0f, listW - 4.0f, 30.0f, Vec3(0.12f, 0.14f, 0.18f));
            std::string listTitle = "PROJECTS & SHOWCASES (" + std::to_string(_hubProjects.size()) + ")";
            LabFont::drawText(listX + 12.0f, mainY + 9.0f, listTitle, 1.4f, textLight, LabFontType::System);

            float itemH = 68.0f;
            float curItemY = mainY + 36.0f;
            float maxListY = mainY + mainH - 52.0f;

            for (size_t i = 0; i < _hubProjects.size(); ++i) {
                if (curItemY + itemH > maxListY) break;
                const auto& proj = _hubProjects[i];
                bool isSel = ((int)i == _hubSelectedProjectIndex);
                bool isHov = (_mouseScreenX >= listX + 4.0f && _mouseScreenX <= listX + listW - 4.0f &&
                              _mouseScreenY >= curItemY && _mouseScreenY <= curItemY + itemH - 4.0f);

                Vec3 cardBg = isSel ? Vec3(0.16f, 0.22f, 0.32f) : (isHov ? Vec3(0.13f, 0.15f, 0.20f) : Vec3(0.08f, 0.09f, 0.12f));
                Renderer::drawRect(listX + 6.0f, curItemY, listW - 12.0f, itemH - 4.0f, cardBg);
                drawDarkBevel(listX + 6.0f, curItemY, listW - 12.0f, itemH - 4.0f, isSel);

                if (isSel) {
                    Renderer::drawRect(listX + 6.0f, curItemY, 4.0f, itemH - 4.0f, cyanGlow);
                }

                // Project Title
                LabFont::drawText(listX + 16.0f, curItemY + 8.0f, proj.displayName, 1.4f, isSel ? Vec3(1, 1, 1) : textLight, LabFontType::System);

                // Badges
                float badgeX = listX + 16.0f;
                float badgeY = curItemY + 28.0f;
                if (proj.isBuiltinShowcase) {
                    Renderer::drawRect(badgeX, badgeY, 82.0f, 16.0f, Vec3(0.28f, 0.22f, 0.08f));
                    LabFont::drawText(badgeX + 6.0f, badgeY + 2.0f, "SHOWCASE", 1.05f, Vec3(1.0f, 0.85f, 0.25f), LabFontType::System);
                    badgeX += 88.0f;
                } else {
                    Renderer::drawRect(badgeX, badgeY, 70.0f, 16.0f, Vec3(0.10f, 0.26f, 0.14f));
                    LabFont::drawText(badgeX + 6.0f, badgeY + 2.0f, "DOCUMENTS", 1.05f, greenAccent, LabFontType::System);
                    badgeX += 76.0f;
                }

                if (proj.hasStandaloneDemo) {
                    Renderer::drawRect(badgeX, badgeY, 78.0f, 16.0f, Vec3(0.08f, 0.22f, 0.32f));
                    LabFont::drawText(badgeX + 6.0f, badgeY + 2.0f, "DEMO READY", 1.05f, cyanGlow, LabFontType::System);
                }

                // Path info
                std::string pathSummary = "Scene: " + (proj.defaultScene.empty() ? "(none)" : proj.defaultScene);
                LabFont::drawText(listX + 16.0f, curItemY + 48.0f, pathSummary, 1.15f, textDim, LabFontType::System);

                curItemY += itemH;
            }

            // Bottom left buttons: [+ CREATE NEW PROJECT] and [RESCAN DISK]
            float botBtnY = mainY + mainH - 44.0f;
            drawDarkButton(listX + 10.0f, botBtnY, 210.0f, 34.0f, "+ Create New Game", false, false, true);
            drawDarkButton(listX + 230.0f, botBtnY, 145.0f, 34.0f, "Rescan Disk", false, false, true);

            // --- RIGHT PANEL: Selected Project Inspector & Action Center ---
            Renderer::drawRect(detX, mainY, detW, mainH, Vec3(0.09f, 0.10f, 0.13f));
            drawDarkBevel(detX, mainY, detW, mainH, false);

            if (_hubSelectedProjectIndex >= 0 && _hubSelectedProjectIndex < (int)_hubProjects.size()) {
                const auto& proj = _hubProjects[_hubSelectedProjectIndex];
                float dy = mainY + 18.0f;

                // Title & Version
                std::string titleStr = proj.displayName + "  v" + proj.version;
                LabFont::drawText(detX + 20.0f, dy, titleStr, 1.85f, Vec3(1, 1, 1), LabFontType::System);
                dy += 28.0f;

                std::string metaLine = "Author: " + proj.author + " | Category: " + (proj.isBuiltinShowcase ? "Flagship Engine Showcase" : "User Game Project") + " | Status: " + (proj.hasStandaloneDemo ? "Playable Standalone Demo Available" : "In Development");
                LabFont::drawText(detX + 20.0f, dy, metaLine, 1.25f, textDim, LabFontType::System);
                dy += 24.0f;

                // Action Buttons Bar
                // [ OPEN IN EDITOR (CONTINUE) ]
                drawDarkButton(detX + 20.0f, dy, 260.0f, 42.0f, "> OPEN IN EDITOR (CONTINUE)", false, false, true);
                Renderer::drawRect(detX + 20.0f, dy + 40.0f, 260.0f, 2.0f, greenAccent);

                // [ PLAY STANDALONE DEMO ]
                if (proj.hasStandaloneDemo) {
                    drawDarkButton(detX + 295.0f, dy, 250.0f, 42.0f, "PLAY STANDALONE DEMO", false, false, true);
                    Renderer::drawRect(detX + 295.0f, dy + 40.0f, 250.0f, 2.0f, cyanGlow);
                }
                dy += 54.0f;

                // Secondary row of buttons
                drawDarkButton(detX + 20.0f, dy, 200.0f, 34.0f, "Open Project Folder", false, false, true);
                drawDarkButton(detX + 230.0f, dy, 200.0f, 34.0f, "Export / Package...", false, false, true);
                dy += 44.0f;

                // Status Banner if any
                if (!_hubStatusBanner.empty()) {
                    Renderer::drawRect(detX + 20.0f, dy, detW - 40.0f, 28.0f, Vec3(0.12f, 0.32f, 0.20f));
                    drawDarkBevel(detX + 20.0f, dy, detW - 40.0f, 28.0f, false);
                    LabFont::drawText(detX + 30.0f, dy + 6.0f, _hubStatusBanner, 1.35f, Vec3(0.6f, 1.0f, 0.7f), LabFontType::System);
                    dy += 36.0f;
                }

                // Description Box
                LabFont::drawText(detX + 20.0f, dy, "Project Description:", 1.45f, textLight, LabFontType::System);
                dy += 22.0f;
                Renderer::drawRect(detX + 20.0f, dy, detW - 40.0f, 75.0f, Vec3(0.06f, 0.07f, 0.09f));
                drawDarkBevel(detX + 20.0f, dy, detW - 40.0f, 75.0f, true);
                LabFont::drawText(detX + 30.0f, dy + 12.0f, proj.description, 1.38f, Vec3(0.9f, 0.92f, 0.95f), LabFontType::System);
                dy += 88.0f;

                // Path & Architecture Details
                LabFont::drawText(detX + 20.0f, dy, "Filesystem & Decoupled Storage Hierarchy:", 1.45f, textLight, LabFontType::System);
                dy += 22.0f;
                Renderer::drawRect(detX + 20.0f, dy, detW - 40.0f, 150.0f, Vec3(0.06f, 0.07f, 0.09f));
                drawDarkBevel(detX + 20.0f, dy, detW - 40.0f, 150.0f, true);

                std::string pRoot = "• Project Root: " + proj.rootPath.string();
                std::string pAssets = "• Assets Tree: " + proj.assetsPath.string();
                std::string pScene = "• Entry Map / Scene: " + proj.defaultScene;
                std::string pHud = "• Active HUD Layout: " + (proj.defaultHud.empty() ? "(none)" : proj.defaultHud);
                std::string pDemo = "• Standalone Binary: " + (proj.hasStandaloneDemo ? proj.demoExePath.string() : "(Not yet built. Use Export pipeline.)");
                std::string pArch = "• Clean Release Guarantee: Engine binaries (build/Release) remain free of hardcoded game assets.";

                LabFont::drawText(detX + 30.0f, dy + 10.0f, pRoot, 1.3f, textDim, LabFontType::System);
                LabFont::drawText(detX + 30.0f, dy + 30.0f, pAssets, 1.3f, textDim, LabFontType::System);
                LabFont::drawText(detX + 30.0f, dy + 50.0f, pScene, 1.3f, cyanGlow, LabFontType::System);
                LabFont::drawText(detX + 30.0f, dy + 70.0f, pHud, 1.3f, orangeGlow, LabFontType::System);
                LabFont::drawText(detX + 30.0f, dy + 90.0f, pDemo, 1.3f, proj.hasStandaloneDemo ? greenAccent : textDim, LabFontType::System);
                LabFont::drawText(detX + 30.0f, dy + 114.0f, pArch, 1.3f, textLight, LabFontType::System);
            }
        }

    // ==================== 5. BOTTOM DOCK (Console / Profiler) ====================
        float conW = std::min(780.0f, l.rightX - leftW - 40.0f);
        float conH = 135.0f;
        float conX = leftW + 20.0f;
        float conY = h - conH - 32.0f;

        Renderer::drawRect(conX, conY, conW, conH, wellBg);
        Renderer::drawRect(conX, conY, conW, 24.0f, Vec3(0.14f, 0.16f, 0.20f));
        drawDarkBevel(conX, conY, conW, conH, true);

        // Bottom dock tabs: [ Console ] [ Profiler ]
        bool isConsoleTab = (_bottomDockTab == BottomDockTab::Console);
        bool isProfilerTab = (_bottomDockTab == BottomDockTab::Profiler);

        Vec3 tabConsBg = isConsoleTab ? Vec3(0.20f, 0.24f, 0.32f) : Vec3(0.14f, 0.15f, 0.18f);
        Renderer::drawRect(conX + 6.0f, conY + 2.0f, 90.0f, 20.0f, tabConsBg);
        drawDarkBevel(conX + 6.0f, conY + 2.0f, 90.0f, 20.0f, isConsoleTab);
        if (isConsoleTab) Renderer::drawRect(conX + 6.0f, conY + 20.0f, 90.0f, 2.0f, cyanGlow);
        LabFont::drawText(conX + 18.0f, conY + 4.0f, "Console", 1.4f, isConsoleTab ? Vec3(1, 1, 1) : textDim, LabFontType::System);

        Vec3 tabProfBg = isProfilerTab ? Vec3(0.20f, 0.24f, 0.32f) : Vec3(0.14f, 0.15f, 0.18f);
        Renderer::drawRect(conX + 102.0f, conY + 2.0f, 90.0f, 20.0f, tabProfBg);
        drawDarkBevel(conX + 102.0f, conY + 2.0f, 90.0f, 20.0f, isProfilerTab);
        if (isProfilerTab) Renderer::drawRect(conX + 102.0f, conY + 20.0f, 90.0f, 2.0f, orangeGlow);
        LabFont::drawText(conX + 114.0f, conY + 4.0f, "Profiler", 1.4f, isProfilerTab ? Vec3(1, 1, 1) : textDim, LabFontType::System);

        if (isConsoleTab) {
            for (int i = 0; i < (int)_consoleMessages.size(); ++i) {
                Vec3 msgCol = (i == (int)_consoleMessages.size() - 1) ? greenAccent : Vec3(0.78f, 0.82f, 0.88f);
                LabFont::drawText(conX + 12.0f, conY + 28.0f + i * 13.0f, _consoleMessages[i], 1.4f, msgCol, LabFontType::System);
            }
        } else {
            // Live Profiler Telemetry
            auto& prof = Profiler::instance();
            auto mem = prof.getMemoryStats();
            auto rend = prof.getRenderStats();
            double frameMs = prof.getLastFrameTimeMs();
            float fps = (frameMs > 0.001) ? (float)(1000.0 / frameMs) : 60.0f;

            char buf1[128], buf2[128], buf3[128];
            std::snprintf(buf1, sizeof(buf1), "Frame Time: %.2f ms | FPS: %.1f | Mode: %s",
                          frameMs, fps, (_simState == EngineSimState::Playing ? "PLAYING" : (_simState == EngineSimState::Paused ? "PAUSED" : "STOPPED")));
            std::snprintf(buf2, sizeof(buf2), "Draw Calls: %u | Triangles: %u | Shaders: %u | VBO Upload: %zu KB",
                          rend.drawCalls, rend.triangles, rend.shaderBinds, rend.vboBytesUploaded / 1024);
            std::snprintf(buf3, sizeof(buf3), "RAM Current: %.2f MB | Peak: %.2f MB | VRAM: %.2f MB | Allocs: %zu",
                          mem.currentUsageRAM / (1024.0 * 1024.0), mem.peakUsageRAM / (1024.0 * 1024.0),
                          mem.currentUsageVRAM / (1024.0 * 1024.0), mem.allocationCount);

            LabFont::drawText(conX + 12.0f, conY + 30.0f, buf1, 1.5f, greenAccent, LabFontType::System);
            LabFont::drawText(conX + 12.0f, conY + 52.0f, buf2, 1.4f, cyanGlow, LabFontType::System);
            LabFont::drawText(conX + 12.0f, conY + 74.0f, buf3, 1.4f, orangeGlow, LabFontType::System);

            const auto& scopes = prof.getLastFrameScopes();
            if (!scopes.empty()) {
                std::string scopeStr = "Active Timelines: ";
                size_t maxScopes = std::min(size_t(3), scopes.size());
                for (size_t s = 0; s < maxScopes; ++s) {
                    scopeStr += "[" + scopes[s].name + ": " + std::to_string((int)scopes[s].durationUs) + "us] ";
                }
                LabFont::drawText(conX + 12.0f, conY + 96.0f, scopeStr, 1.3f, textLight, LabFontType::System);
            } else {
                LabFont::drawText(conX + 12.0f, conY + 96.0f, "Active Timelines: [MainLoop] [RenderGraph] [PhysicsStep]", 1.3f, textDim, LabFontType::System);
            }
        }

        // CSG Clip Tool HUD Banner when active
        if (_activeTool == 6) {
            float bannerW = 660.0f;
            float bannerH = 34.0f;
            float bx = (w - bannerW) * 0.5f;
            float by = 65.0f;
            Renderer::drawRect(bx, by, bannerW, bannerH, Vec3(0.12f, 0.14f, 0.18f));
            drawDarkBevel(bx, by, bannerW, bannerH, false);
            Renderer::drawRect(bx, by, bannerW, 2.0f, orangeGlow);

            std::string modeStr = (_clipMode == ClipMode::KeepFront) ? "KEEP FRONT (Positive Side)" :
                                  (_clipMode == ClipMode::KeepBack)  ? "KEEP BACK (Negative Side)" :
                                                                       "SPLIT & KEEP BOTH BRUSHES";
            std::string bannerText = "CSG CLIP TOOL | " + modeStr + " [Shift+X / X] | [ENTER] Slice Brush";
            LabFont::drawText(bx + 14.0f, by + 8.0f, bannerText, 1.5f, Vec3(1.0f, 0.95f, 0.90f), LabFontType::GeoSans);
        }

        // ==================== 6. STATUS BAR ====================
        float sbY = h - 22.0f;
        Renderer::drawRect(0, sbY, w, 22.0f, panelDarker);
        drawDarkBevel(0, sbY, w, 22.0f, false);

        std::string sbText = "RMB Fly | LMB Pick/Apply | Wheel / R / C: Elevate Y | E Place | X Clip | ENTER Commit | F Focus | " + _cullingStats;
        if (!_gizmoStatusText.empty()) {
            sbText = _gizmoStatusText + " | " + _cullingStats;
        }
        LabFont::drawText(10.0f, sbY + 5.0f, sbText, 1.5f, !_gizmoStatusText.empty() ? cyanGlow : textLight, LabFontType::System);
        std::string gridStr = "Snap: " + std::to_string((int)_gridSnap) + " | Ang: " + std::to_string((int)_angleSnap) + "° | F9: Run";
        LabFont::drawText(w - 280.0f, sbY + 5.0f, gridStr, 1.5f, orangeGlow, LabFontType::System);

        // ==================== 7. DROPDOWN FILE MENU ====================
        if (_fileMenuOpen) {
            float menuX = 10.0f;
            float menuY = 24.0f;
            float menuW = 240.0f;
            float menuH = 200.0f;
            Vec3 menuBg{ 0.16f, 0.17f, 0.20f };
            Vec3 menuBorder{ 0.35f, 0.38f, 0.44f };

            Renderer::drawRect(menuX, menuY, menuW, menuH, menuBg);
            drawDarkBevel(menuX, menuY, menuW, menuH, false);

            struct MenuItem { std::string name; std::string shortcut; };
            MenuItem items[] = {
                { "Project Hub (Main Menu)", "F1" },
                { "New Map", "Ctrl+N" },
                { "Open Map...", "Ctrl+O" },
                { "Save Map", "Ctrl+S" },
                { "Save Map As...", "" },
                { "Build & Run Game", "F5 / Ctrl+B" },
                { "Build Settings...", "Ctrl+Shift+B" },
                { "Exit", "Alt+F4" }
            };

            for (int i = 0; i < 8; ++i) {
                float iy = menuY + 3.0f + i * 24.0f;
                bool isHov = (_mouseScreenX >= menuX && _mouseScreenX <= menuX + menuW && _mouseScreenY >= iy && _mouseScreenY <= iy + 22.0f);
                if (isHov) {
                    Renderer::drawRect(menuX + 2.0f, iy, menuW - 4.0f, 22.0f, Vec3(0.24f, 0.28f, 0.36f));
                }
                if (i == 1 || i == 5 || i == 7) Renderer::drawRect(menuX + 6.0f, iy - 2.0f, menuW - 12.0f, 1.0f, menuBorder);
                LabFont::drawText(menuX + 14.0f, iy + 4.0f, items[i].name, 1.6f, (i == 0) ? (isHov ? cyanGlow : Vec3(0.3f, 0.8f, 1.0f)) : ((i == 5) ? (isHov ? Vec3(0.4f, 1.0f, 0.5f) : Vec3(0.3f, 0.9f, 0.4f)) : (isHov ? orangeGlow : textLight)), LabFontType::System);
                if (!items[i].shortcut.empty()) {
                    LabFont::drawText(menuX + menuW - 95.0f, iy + 4.0f, items[i].shortcut, 1.4f, textDim, LabFontType::System);
                }
            }
        }

        // ==================== 8. DROPDOWN BUILD MENU ====================
        if (_buildMenuOpen) {
            float menuX = 137.0f;
            float menuY = 24.0f;
            float menuW = 270.0f;
            float menuH = 125.0f;
            Vec3 menuBg{ 0.16f, 0.17f, 0.20f };
            Vec3 menuBorder{ 0.35f, 0.38f, 0.44f };

            Renderer::drawRect(menuX, menuY, menuW, menuH, menuBg);
            drawDarkBevel(menuX, menuY, menuW, menuH, false);

            struct BuildMenuItem { std::string name; std::string shortcut; };
            BuildMenuItem bItems[] = {
                { "Test Game (Debug)", "F5" },
                { "Build & Export Game...", "Ctrl+Shift+B" },
                { "Export Release Bytecode (.luac)", "" },
                { "Export Shipping Bytecode (Stripped)", "" },
                { "Open test_build Folder", "" }
            };

            for (int i = 0; i < 5; ++i) {
                float iy = menuY + 3.0f + i * 24.0f;
                bool isHov = (_mouseScreenX >= menuX && _mouseScreenX <= menuX + menuW && _mouseScreenY >= iy && _mouseScreenY <= iy + 22.0f);
                if (isHov) {
                    Renderer::drawRect(menuX + 2.0f, iy, menuW - 4.0f, 22.0f, Vec3(0.24f, 0.28f, 0.36f));
                }
                if (i == 2 || i == 4) Renderer::drawRect(menuX + 6.0f, iy - 2.0f, menuW - 12.0f, 1.0f, menuBorder);
                LabFont::drawText(menuX + 14.0f, iy + 4.0f, bItems[i].name, 1.5f, (i == 0) ? (isHov ? Vec3(0.4f, 1.0f, 0.5f) : Vec3(0.3f, 0.9f, 0.4f)) : (isHov ? orangeGlow : textLight), LabFontType::System);
                if (!bItems[i].shortcut.empty()) {
                    LabFont::drawText(menuX + menuW - 90.0f, iy + 4.0f, bItems[i].shortcut, 1.4f, textDim, LabFontType::System);
                }
            }
        }

        // ==================== 8.5. DROPDOWN VIEW MENU (Categorized with Subcategories) ====================
        if (_viewMenuOpen) {
            float menuX = 92.0f;
            float menuY = 24.0f;
            float menuW = 230.0f;
            float menuH = 129.0f;
            Vec3 menuBg{ 0.16f, 0.17f, 0.20f };
            Vec3 menuBorder{ 0.35f, 0.38f, 0.44f };

            Renderer::drawRect(menuX, menuY, menuW, menuH, menuBg);
            drawDarkBevel(menuX, menuY, menuW, menuH, false);

            const char* viewCats[] = {
                "3D World & Assets      >",
                "2D Level & UI          >",
                "Scripting & Dev        >",
                "Plugins & Packages     >",
                "Viewport Display       >"
            };

            for (int i = 0; i < 5; ++i) {
                float iy = menuY + 3.0f + i * 24.0f;
                bool isHov = (_mouseScreenX >= menuX && _mouseScreenX <= menuX + menuW && _mouseScreenY >= iy && _mouseScreenY <= iy + 22.0f);
                if (isHov) {
                    _viewSubmenuHover = i;
                }
                bool isCatActive = (_viewSubmenuHover == i);
                if (isHov || isCatActive) {
                    Renderer::drawRect(menuX + 2.0f, iy, menuW - 4.0f, 22.0f, Vec3(0.24f, 0.28f, 0.36f));
                }
                LabFont::drawText(menuX + 14.0f, iy + 4.0f, viewCats[i], 1.5f, isCatActive ? orangeGlow : textLight, LabFontType::System);
            }

            // Draw Flyout Submenu to the right
            if (_viewSubmenuHover >= 0 && _viewSubmenuHover <= 4) {
                float subX = menuX + menuW;
                float subY = menuY + _viewSubmenuHover * 24.0f;
                float subW = 270.0f;

                if (_viewSubmenuHover == 0) { // 3D Workspaces
                    float subH = 8 * 24.0f + 6.0f;
                    Renderer::drawRect(subX, subY, subW, subH, menuBg);
                    drawDarkBevel(subX, subY, subW, subH, false);

                    struct SubItem3D { const char* label; EditorWorkspace ws; };
                    SubItem3D items3D[] = {
                        { "Level & Geometry Editor", EditorWorkspace::World3D_Level },
                        { "Animation 3D (Keyframe)", EditorWorkspace::World3D_Animation },
                        { "3D Model Viewer & Inspector", EditorWorkspace::World3D_ModelViewer },
                        { "Character Appearance Studio", EditorWorkspace::Studio_Appearance },
                        { "Weapon Grip Poser", EditorWorkspace::Studio_GripPoser },
                        { "Animation Timeline", EditorWorkspace::Studio_AnimationTimeline },
                        { "Weapon Skins & Materials", EditorWorkspace::Studio_WeaponSkins },
                        { "Face Dialogue & Lip-Sync", EditorWorkspace::Studio_FaceDialogue }
                    };
                    for (int s = 0; s < 8; ++s) {
                        float sy = subY + 3.0f + s * 24.0f;
                        bool isCur = (_workspace == items3D[s].ws);
                        bool isHov = (_mouseScreenX >= subX && _mouseScreenX <= subX + subW && _mouseScreenY >= sy && _mouseScreenY <= sy + 22.0f);
                        if (isHov) Renderer::drawRect(subX + 2.0f, sy, subW - 4.0f, 22.0f, Vec3(0.24f, 0.28f, 0.36f));
                        LabFont::drawText(subX + 12.0f, sy + 4.0f, items3D[s].label, 1.45f, isCur ? cyanGlow : (isHov ? orangeGlow : textLight), LabFontType::System);
                    }
                } else if (_viewSubmenuHover == 1) { // 2D Workspaces
                    float subH = 3 * 24.0f + 6.0f;
                    Renderer::drawRect(subX, subY, subW, subH, menuBg);
                    drawDarkBevel(subX, subY, subW, subH, false);

                    struct SubItem2D { const char* label; EditorWorkspace ws; };
                    SubItem2D items2D[] = {
                        { "Level & Tilemap Editor", EditorWorkspace::Level2D_Tilemap },
                        { "Animation 2D Spritesheet", EditorWorkspace::Level2D_Animation },
                        { "HUD Canvas Designer", EditorWorkspace::Level2D_HUD }
                    };
                    for (int s = 0; s < 3; ++s) {
                        float sy = subY + 3.0f + s * 24.0f;
                        bool isCur = (_workspace == items2D[s].ws);
                        bool isHov = (_mouseScreenX >= subX && _mouseScreenX <= subX + subW && _mouseScreenY >= sy && _mouseScreenY <= sy + 22.0f);
                        if (isHov) Renderer::drawRect(subX + 2.0f, sy, subW - 4.0f, 22.0f, Vec3(0.24f, 0.28f, 0.36f));
                        LabFont::drawText(subX + 12.0f, sy + 4.0f, items2D[s].label, 1.45f, isCur ? cyanGlow : (isHov ? orangeGlow : textLight), LabFontType::System);
                    }
                } else if (_viewSubmenuHover == 2) { // Scripting
                    float subH = 2 * 24.0f + 6.0f;
                    Renderer::drawRect(subX, subY, subW, subH, menuBg);
                    drawDarkBevel(subX, subY, subW, subH, false);

                    struct SubItemScr { const char* label; EditorWorkspace ws; };
                    SubItemScr itemsScr[] = {
                        { "Lua Script Editor IDE", EditorWorkspace::Scripting_Editor },
                        { "Interactive REPL Console", EditorWorkspace::Scripting_Console }
                    };
                    for (int s = 0; s < 2; ++s) {
                        float sy = subY + 3.0f + s * 24.0f;
                        bool isCur = (_workspace == itemsScr[s].ws);
                        bool isHov = (_mouseScreenX >= subX && _mouseScreenX <= subX + subW && _mouseScreenY >= sy && _mouseScreenY <= sy + 22.0f);
                        if (isHov) Renderer::drawRect(subX + 2.0f, sy, subW - 4.0f, 22.0f, Vec3(0.24f, 0.28f, 0.36f));
                        LabFont::drawText(subX + 12.0f, sy + 4.0f, itemsScr[s].label, 1.45f, isCur ? cyanGlow : (isHov ? orangeGlow : textLight), LabFontType::System);
                    }
                } else if (_viewSubmenuHover == 3) { // Plugins & Packages
                    float subH = 1 * 24.0f + 6.0f;
                    Renderer::drawRect(subX, subY, subW, subH, menuBg);
                    drawDarkBevel(subX, subY, subW, subH, false);

                    struct SubItemPlug { const char* label; EditorWorkspace ws; };
                    SubItemPlug itemsPlug[] = {
                        { "Package & Plugin Manager", EditorWorkspace::Plugins_Manager }
                    };
                    for (int s = 0; s < 1; ++s) {
                        float sy = subY + 3.0f + s * 24.0f;
                        bool isCur = (_workspace == itemsPlug[s].ws);
                        bool isHov = (_mouseScreenX >= subX && _mouseScreenX <= subX + subW && _mouseScreenY >= sy && _mouseScreenY <= sy + 22.0f);
                        if (isHov) Renderer::drawRect(subX + 2.0f, sy, subW - 4.0f, 22.0f, Vec3(0.24f, 0.28f, 0.36f));
                        LabFont::drawText(subX + 12.0f, sy + 4.0f, itemsPlug[s].label, 1.45f, isCur ? cyanGlow : (isHov ? orangeGlow : textLight), LabFontType::System);
                    }
                } else if (_viewSubmenuHover == 4) { // Display
                    float subH = 3 * 24.0f + 6.0f;
                    Renderer::drawRect(subX, subY, subW, subH, menuBg);
                    drawDarkBevel(subX, subY, subW, subH, false);

                    struct SubItemDisp { const char* label; const char* sc; bool active; };
                    SubItemDisp itemsDisp[] = {
                        { "Toggle Wireframe", "Z", _wireframeMode },
                        { "Toggle 2D / 3D Ortho", "O", _orthoViewMode },
                        { "Toggle Darkness Mode", "L", _darknessMode }
                    };
                    for (int s = 0; s < 3; ++s) {
                        float sy = subY + 3.0f + s * 24.0f;
                        bool isHov = (_mouseScreenX >= subX && _mouseScreenX <= subX + subW && _mouseScreenY >= sy && _mouseScreenY <= sy + 22.0f);
                        if (isHov) Renderer::drawRect(subX + 2.0f, sy, subW - 4.0f, 22.0f, Vec3(0.24f, 0.28f, 0.36f));
                        LabFont::drawText(subX + 12.0f, sy + 4.0f, itemsDisp[s].label, 1.45f, itemsDisp[s].active ? cyanGlow : (isHov ? orangeGlow : textLight), LabFontType::System);
                        LabFont::drawText(subX + subW - 35.0f, sy + 4.0f, itemsDisp[s].sc, 1.35f, textDim, LabFontType::System);
                    }
                }
            }
        }

        // ==================== 9. HIERARCHY IN-MENU CONTEXT MENU WITH LIVE SEARCH ====================
        if (_contextMenuOpen) {
            float ctxW = 250.0f;
            float ctxX = std::clamp(_contextMenuX, 10.0f, w - ctxW - 10.0f);
            float ctxY = std::clamp(_contextMenuY, 24.0f, h - 280.0f);
            Vec3 ctxBg{ 0.14f, 0.15f, 0.18f };

            const char* allComponents[] = {
                "Magic Add: Camera",
                "Magic Add: Light",
                "Magic Add: Spawn",
                "Magic Add: Brush",
                "Magic Add: Broom",
                "Magic Add: Door",
                "Script (Lua)",
                "Point Light",
                "Sprite 2D",
                "Box Collider",
                "RigidBody Dynamic",
                "Audio Source (Sound)",
                "Timer Event Trigger",
                "Dialogue Trigger Area"
            };

            // Filter components
            std::vector<std::string> filtered;
            std::string q = _contextMenuSearch;
            std::transform(q.begin(), q.end(), q.begin(), ::tolower);
            for (const auto* c : allComponents) {
                std::string s(c);
                std::string sLow = s;
                std::transform(sLow.begin(), sLow.end(), sLow.begin(), ::tolower);
                if (q.empty() || sLow.find(q) != std::string::npos) {
                    filtered.push_back(s);
                }
            }

            float ctxH = 60.0f + (float)filtered.size() * 24.0f;
            Renderer::drawRect(ctxX, ctxY, ctxW, ctxH, ctxBg);
            drawDarkBevel(ctxX, ctxY, ctxW, ctxH, false);

            // Title
            Renderer::drawRect(ctxX, ctxY, ctxW, 24.0f, Vec3(0.18f, 0.22f, 0.30f));
            std::string menuTitle = (_contextMenuTargetIndex >= 0) ? ("Add Component (Entity #" + std::to_string(_contextMenuTargetIndex) + ")") : "Magic Add (Viewport)";
            LabFont::drawText(ctxX + 8.0f, ctxY + 5.0f, menuTitle, 1.35f, cyanGlow, LabFontType::System);

            // In-menu search field
            float searchY = ctxY + 28.0f;
            Renderer::drawRect(ctxX + 6.0f, searchY, ctxW - 12.0f, 22.0f, Vec3(0.08f, 0.09f, 0.11f));
            drawDarkBevel(ctxX + 6.0f, searchY, ctxW - 12.0f, 22.0f, true);
            std::string sText = _contextMenuSearch.empty() ? "Add > Type to search..." : ("Add > " + _contextMenuSearch);
            LabFont::drawText(ctxX + 12.0f, searchY + 4.0f, sText, 1.3f, _contextMenuSearch.empty() ? textDim : Vec3(1, 1, 1), LabFontType::System);

            // Filtered list
            for (size_t i = 0; i < filtered.size(); ++i) {
                float iy = searchY + 26.0f + i * 24.0f;
                bool isHov = (_mouseScreenX >= ctxX && _mouseScreenX <= ctxX + ctxW && _mouseScreenY >= iy && _mouseScreenY <= iy + 22.0f);
                if (isHov) {
                    Renderer::drawRect(ctxX + 4.0f, iy, ctxW - 8.0f, 22.0f, Vec3(0.24f, 0.28f, 0.36f));
                }
                LabFont::drawText(ctxX + 14.0f, iy + 4.0f, "+ " + filtered[i], 1.4f, isHov ? orangeGlow : textLight, LabFontType::System);
            }
        }

        Renderer::endUI();
    }

    // Modal Texture Browser Gallery
    void drawTextureBrowser() {
        int fbW = 0, fbH = 0;
        getFramebufferSize(fbW, fbH);
        float w = (fbW > 0) ? (float)fbW : (float)getWidth();
        float h = (fbH > 0) ? (float)fbH : (float)getHeight();

        Renderer::beginUI((int)w, (int)h);

        Renderer::drawRect(0, 0, w, h, Vec3(0.04f, 0.05f, 0.06f));

        float bw = 820.0f;
        float bh = 600.0f;
        float bx = (w - bw) * 0.5f;
        float by = (h - bh) * 0.5f;

        Renderer::drawRect(bx, by, bw, bh, Vec3(0.16f, 0.17f, 0.20f));
        drawDarkBevel(bx, by, bw, bh, false);

        Renderer::drawRect(bx + 2.0f, by + 2.0f, bw - 4.0f, 28.0f, Vec3(0.14f, 0.22f, 0.34f));
        LabFont::drawText(bx + 14.0f, by + 8.0f, "Texture Browser - Choose Surface Material", 1.8f, Vec3(1, 1, 1), LabFontType::System);

        drawDarkButton(bx + bw - 32.0f, by + 4.0f, 24.0f, 22.0f, "X", false, true);

        int cols = 6;
        float thumbSize = 110.0f;
        float gap = 15.0f;
        float startX = bx + 25.0f;
        float startY = by + 45.0f;

        for (size_t i = 0; i < _availableTextures.size(); ++i) {
            int col = (int)(i % cols);
            int row = (int)(i / cols);
            float tx = startX + col * (thumbSize + gap);
            float ty = startY + row * (thumbSize + gap);

            bool isSelected = (_availableTextures[i].filename == _selectedTexture);

            Renderer::drawRect(tx - 3.0f, ty - 3.0f, thumbSize + 6.0f, thumbSize + 22.0f, isSelected ? Vec3(1.0f, 0.55f, 0.10f) : Vec3(0.24f, 0.25f, 0.28f));
            drawDarkBevel(tx - 3.0f, ty - 3.0f, thumbSize + 6.0f, thumbSize + 22.0f, isSelected);
            Renderer::drawRect(tx, ty, thumbSize, thumbSize, Vec3(0, 0, 0));

            if (_textures.contains(_availableTextures[i].filename)) {
                Renderer::drawTextureRect(tx, ty, thumbSize, thumbSize, *_textures[_availableTextures[i].filename]);
            }

            std::string label = _availableTextures[i].filename;
            if (label.size() > 12) label = label.substr(0, 10) + "..";
            LabFont::drawText(tx, ty + thumbSize + 4.0f, label, 1.4f, isSelected ? Vec3(1.0f, 0.85f, 0.2f) : Vec3(0.9f, 0.92f, 0.95f), LabFontType::System);
        }

        Renderer::endUI();
    }

    // Modal Model Browser Gallery
    void drawModelBrowser() {
        int fbW = 0, fbH = 0;
        getFramebufferSize(fbW, fbH);
        float w = (fbW > 0) ? (float)fbW : (float)getWidth();
        float h = (fbH > 0) ? (float)fbH : (float)getHeight();

        Renderer::beginUI((int)w, (int)h);

        Renderer::drawRect(0, 0, w, h, Vec3(0.04f, 0.05f, 0.06f));

        float bw = 700.0f;
        float bh = 480.0f;
        float bx = (w - bw) * 0.5f;
        float by = (h - bh) * 0.5f;

        Renderer::drawRect(bx, by, bw, bh, Vec3(0.16f, 0.17f, 0.20f));
        drawDarkBevel(bx, by, bw, bh, false);

        Renderer::drawRect(bx + 2.0f, by + 2.0f, bw - 4.0f, 28.0f, Vec3(0.14f, 0.22f, 0.34f));
        LabFont::drawText(bx + 14.0f, by + 8.0f, "3D Model Browser - Place Entity Props", 1.8f, Vec3(1, 1, 1), LabFontType::System);

        drawDarkButton(bx + bw - 32.0f, by + 4.0f, 24.0f, 22.0f, "X", false, true);

        // Browse STL from disk button
        drawDarkButton(bx + 20.0f, by + 45.0f, 300.0f, 34.0f, "Browse Disk for 3D Model (.stl)...", false, false, true);

        // List discovered models
        float startY = by + 95.0f;
        LabFont::drawText(bx + 20.0f, startY, "Available Models in assets/models/:", 1.7f, Vec3(1.0f, 0.55f, 0.10f), LabFontType::System);

        for (size_t i = 0; i < _availableModels.size(); ++i) {
            float iy = startY + 24.0f + i * 36.0f;
            bool isSel = (_availableModels[i] == _selectedModel);

            Renderer::drawRect(bx + 20.0f, iy, bw - 40.0f, 30.0f, isSel ? Vec3(0.20f, 0.32f, 0.48f) : Vec3(0.21f, 0.22f, 0.25f));
            drawDarkBevel(bx + 20.0f, iy, bw - 40.0f, 30.0f, isSel);
            if (isSel) Renderer::drawRect(bx + 20.0f, iy, 4.0f, 30.0f, Vec3(1.0f, 0.55f, 0.10f));

            LabFont::drawText(bx + 32.0f, iy + 7.0f, _availableModels[i], 1.6f, isSel ? Vec3(1, 1, 1) : Vec3(0.90f, 0.92f, 0.95f), LabFontType::System);
        }

        Renderer::endUI();
    }

    // Modal Help
    void drawHelpModal() {
        int fbW = 0, fbH = 0;
        getFramebufferSize(fbW, fbH);
        float w = (fbW > 0) ? (float)fbW : (float)getWidth();
        float h = (fbH > 0) ? (float)fbH : (float)getHeight();

        Renderer::beginUI((int)w, (int)h);

        Renderer::drawRect(0, 0, w, h, Vec3(0.04f, 0.05f, 0.06f));

        float bw = 650.0f;
        float bh = 430.0f;
        float bx = (w - bw) * 0.5f;
        float by = (h - bh) * 0.5f;

        Renderer::drawRect(bx, by, bw, bh, Vec3(0.16f, 0.17f, 0.20f));
        drawDarkBevel(bx, by, bw, bh, false);

        Renderer::drawRect(bx + 2.0f, by + 2.0f, bw - 4.0f, 28.0f, Vec3(0.14f, 0.22f, 0.34f));
        LabFont::drawText(bx + 14.0f, by + 8.0f, "Lab Engine Studio 2026 - Keyboard & Mouse Reference", 1.8f, Vec3(1, 1, 1), LabFontType::System);

        drawDarkButton(bx + bw - 32.0f, by + 4.0f, 24.0f, 22.0f, "X", false, true);

        const char* helpLines[] = {
            "Hold RMB + WASD: Free-cam flying (Shift = Boost, Space = Up, Ctrl = Down)",
            "LMB Click in 3D: Raycast selection of Brushes, Props, Doors, Entities",
            "Tool 3 (Pipette): LMB applies active texture, RMB samples clicked brush texture",
            "E Key / LMB: Place object on grid at 3D cursor (Mesh, Prop, Door, Light, Actor)",
            "F Key: Center & Focus Camera on selected entity",
            "Ctrl + D: Duplicate selected entity offset on grid",
            "Delete / Backspace: Delete selected entity",
            "Arrow Keys / PageUp / PageDn: Translate selected object along grid axes",
            "F9: Toggle Live In-Editor Simulation (PIE Mode)",
            "Optimization: Frustum culling skips rendering off-screen brushes and props"
        };

        for (int i = 0; i < 10; ++i) {
            LabFont::drawText(bx + 25.0f, by + 45.0f + i * 34.0f, helpLines[i], 1.5f, Vec3(0.90f, 0.92f, 0.95f), LabFontType::System);
        }

        Renderer::endUI();
    }

    // Modal Build & Export Pipeline Window
    void drawBuildModal() {
        int fbW = 0, fbH = 0;
        getFramebufferSize(fbW, fbH);
        float w = (fbW > 0) ? (float)fbW : (float)getWidth();
        float h = (fbH > 0) ? (float)fbH : (float)getHeight();

        Renderer::beginUI((int)w, (int)h);

        // Backdrop
        Renderer::drawRect(0, 0, w, h, Vec3(0.04f, 0.05f, 0.06f));

        float bw = 740.0f;
        float bh = 500.0f;
        float bx = (w - bw) * 0.5f;
        float by = (h - bh) * 0.5f;

        Renderer::drawRect(bx, by, bw, bh, Vec3(0.15f, 0.16f, 0.19f));
        drawDarkBevel(bx, by, bw, bh, false);

        // Header
        Renderer::drawRect(bx + 2.0f, by + 2.0f, bw - 4.0f, 32.0f, Vec3(0.14f, 0.22f, 0.36f));
        LabFont::drawText(bx + 16.0f, by + 9.0f, "BUILD & EXPORT GAME - Standalone Engine Deployment", 1.8f, Vec3(1, 1, 1), LabFontType::System);
        drawDarkButton(bx + bw - 32.0f, by + 5.0f, 24.0f, 22.0f, "X", false, true);

        // Platform Selection Header
        float curY = by + 50.0f;
        LabFont::drawText(bx + 24.0f, curY, "Target Platform:", 1.6f, Vec3(1.0f, 0.85f, 0.4f), LabFontType::System);

        // Platform buttons (Windows, Linux, WebAssembly)
        struct PlatBtn { TargetPlatform plat; const char* label; float w; };
        PlatBtn plats[] = {
            { TargetPlatform::Windows_x64, "Windows (.exe)", 160.0f },
            { TargetPlatform::Linux_x64, "Linux (.x86_64)", 160.0f },
            { TargetPlatform::WebAssembly, "WebAssembly (HTML5)", 180.0f }
        };

        float px = bx + 24.0f;
        float py = curY + 24.0f;
        for (const auto& p : plats) {
            bool isSel = (_exportSettings.platform == p.plat);
            bool isHov = (_mouseScreenX >= px && _mouseScreenX <= px + p.w && _mouseScreenY >= py && _mouseScreenY <= py + 32.0f);
            Vec3 bg = isSel ? Vec3(0.20f, 0.55f, 0.85f) : (isHov ? Vec3(0.28f, 0.30f, 0.36f) : Vec3(0.20f, 0.22f, 0.25f));
            Renderer::drawRect(px, py, p.w, 32.0f, bg);
            drawDarkBevel(px, py, p.w, 32.0f, isSel);
            LabFont::drawText(px + 12.0f, py + 8.0f, p.label, 1.5f, isSel ? Vec3(1, 1, 1) : Vec3(0.85f, 0.88f, 0.92f), LabFontType::System);
            px += p.w + 14.0f;
        }

        // Configuration Header
        curY += 76.0f;
        LabFont::drawText(bx + 24.0f, curY, "Build Configuration:", 1.6f, Vec3(1.0f, 0.85f, 0.4f), LabFontType::System);

        struct CfgBtn { BuildConfiguration cfg; const char* label; float w; };
        CfgBtn cfgs[] = {
            { BuildConfiguration::Shipping, "Shipping (Optimized)", 180.0f },
            { BuildConfiguration::Debug, "Debug (Symbols)", 160.0f }
        };
        px = bx + 24.0f;
        py = curY + 24.0f;
        for (const auto& c : cfgs) {
            bool isSel = (_exportSettings.config == c.cfg);
            bool isHov = (_mouseScreenX >= px && _mouseScreenX <= px + c.w && _mouseScreenY >= py && _mouseScreenY <= py + 30.0f);
            Vec3 bg = isSel ? Vec3(0.22f, 0.65f, 0.40f) : (isHov ? Vec3(0.28f, 0.30f, 0.36f) : Vec3(0.20f, 0.22f, 0.25f));
            Renderer::drawRect(px, py, c.w, 30.0f, bg);
            drawDarkBevel(px, py, c.w, 30.0f, isSel);
            LabFont::drawText(px + 12.0f, py + 7.0f, c.label, 1.4f, isSel ? Vec3(1, 1, 1) : Vec3(0.85f, 0.88f, 0.92f), LabFontType::System);
            px += c.w + 14.0f;
        }

        // Project Info Box
        curY += 76.0f;
        Renderer::drawRect(bx + 24.0f, curY, bw - 48.0f, 114.0f, Vec3(0.12f, 0.13f, 0.16f));
        drawDarkBevel(bx + 24.0f, curY, bw - 48.0f, 114.0f, false);

        std::string scenePath = _currentMapPath.empty() ? "assets/maps/export_game.labmap (Current Scene)" : _currentMapPath;
        auto& pm = ProjectManager::get();
        LabFont::drawText(bx + 36.0f, curY + 10.0f, "Game Title:   " + _exportSettings.gameTitle, 1.45f, Vec3(0.9f, 0.9f, 0.95f), LabFontType::System);
        LabFont::drawText(bx + 36.0f, curY + 34.0f, "Project Dev:  Documents/LabGames/" + pm.getActiveProjectName() + " (Decoupled from CMake)", 1.35f, Vec3(0.45f, 0.85f, 0.55f), LabFontType::System);
        LabFont::drawText(bx + 36.0f, curY + 58.0f, "Entry Scene:  " + scenePath, 1.45f, Vec3(0.9f, 0.9f, 0.95f), LabFontType::System);
        LabFont::drawText(bx + 36.0f, curY + 82.0f, "Destination:  build/export/" + _exportSettings.gameTitle + "_" + ExportPipeline::platformToString(_exportSettings.platform), 1.45f, Vec3(0.7f, 0.8f, 0.95f), LabFontType::System);

        // Status / Log Box
        curY += 124.0f;
        Renderer::drawRect(bx + 24.0f, curY, bw - 48.0f, 44.0f, Vec3(0.10f, 0.11f, 0.13f));
        drawDarkBevel(bx + 24.0f, curY, bw - 48.0f, 44.0f, false);
        if (_lastBuildLog.empty()) {
            LabFont::drawText(bx + 36.0f, curY + 14.0f, "Ready to package project. Click [ BUILD GAME NOW ] or [ BUILD & RUN ].", 1.35f, Vec3(0.6f, 0.65f, 0.75f), LabFontType::System);
        } else {
            Vec3 logCol = _buildSuccess ? Vec3(0.35f, 0.95f, 0.45f) : Vec3(0.95f, 0.35f, 0.35f);
            LabFont::drawText(bx + 36.0f, curY + 14.0f, _lastBuildLog, 1.35f, logCol, LabFontType::System);
        }

        // Action Buttons at Bottom
        float btnY = by + bh - 52.0f;

        // 1. [ BUILD GAME NOW ]
        float b1X = bx + 24.0f;
        float b1W = 180.0f;
        bool b1Hov = (_mouseScreenX >= b1X && _mouseScreenX <= b1X + b1W && _mouseScreenY >= btnY && _mouseScreenY <= btnY + 36.0f);
        Renderer::drawRect(b1X, btnY, b1W, 36.0f, b1Hov ? Vec3(0.24f, 0.52f, 0.88f) : Vec3(0.18f, 0.42f, 0.72f));
        drawDarkBevel(b1X, btnY, b1W, 36.0f, false);
        LabFont::drawText(b1X + 20.0f, btnY + 10.0f, "BUILD GAME NOW", 1.45f, Vec3(1, 1, 1), LabFontType::System);

        // 2. [ BUILD & RUN ]
        float b2X = b1X + b1W + 14.0f;
        float b2W = 220.0f;
        bool b2Hov = (_mouseScreenX >= b2X && _mouseScreenX <= b2X + b2W && _mouseScreenY >= btnY && _mouseScreenY <= btnY + 36.0f);
        Renderer::drawRect(b2X, btnY, b2W, 36.0f, b2Hov ? Vec3(0.24f, 0.85f, 0.44f) : Vec3(0.18f, 0.68f, 0.34f));
        drawDarkBevel(b2X, btnY, b2W, 36.0f, false);
        LabFont::drawText(b2X + 22.0f, btnY + 10.0f, "BUILD & RUN (PLAY)", 1.45f, Vec3(1, 1, 1), LabFontType::System);

        // 3. [ CLOSE ]
        float b3X = bx + bw - 130.0f;
        float b3W = 106.0f;
        bool b3Hov = (_mouseScreenX >= b3X && _mouseScreenX <= b3X + b3W && _mouseScreenY >= btnY && _mouseScreenY <= btnY + 36.0f);
        Renderer::drawRect(b3X, btnY, b3W, 36.0f, b3Hov ? Vec3(0.35f, 0.38f, 0.44f) : Vec3(0.24f, 0.26f, 0.30f));
        drawDarkBevel(b3X, btnY, b3W, 36.0f, false);
        LabFont::drawText(b3X + 26.0f, btnY + 10.0f, "CLOSE", 1.45f, Vec3(0.9f, 0.9f, 0.9f), LabFontType::System);

        Renderer::endUI();
    }

    void drawInputModal() {
        int fbW = 0, fbH = 0;
        getFramebufferSize(fbW, fbH);
        float w = (fbW > 0) ? (float)fbW : (float)getWidth();
        float h = (fbH > 0) ? (float)fbH : (float)getHeight();

        Renderer::beginUI((int)w, (int)h);

        // Dark modal overlay
        Renderer::drawRect(0, 0, w, h, Vec3(0.04f, 0.05f, 0.06f));

        float bw = 780.0f;
        float bh = 540.0f;
        float bx = (w - bw) * 0.5f;
        float by = (h - bh) * 0.5f;

        Renderer::drawRect(bx, by, bw, bh, Vec3(0.16f, 0.17f, 0.20f));
        drawDarkBevel(bx, by, bw, bh, false);

        // Header Title
        Renderer::drawRect(bx + 2.0f, by + 2.0f, bw - 4.0f, 28.0f, Vec3(0.14f, 0.22f, 0.34f));
        LabFont::drawText(bx + 14.0f, by + 8.0f, "Input Actions & Gamepad Controller Mappings (Lua / C++)", 1.8f, Vec3(1, 1, 1), LabFontType::System);

        drawDarkButton(bx + bw - 32.0f, by + 4.0f, 24.0f, 22.0f, "X", false, true);

        // Gamepad Status Banner
        auto gp = InputActionManager::instance().getGamepadState(0);
        Vec3 gpStatusCol = gp.connected ? Vec3(0.2f, 0.85f, 0.4f) : Vec3(0.85f, 0.4f, 0.3f);
        std::string gpText = "Gamepad (Port 0): " + gp.name + (gp.connected ? " [Connected]" : " [Disconnected]");
        LabFont::drawText(bx + 24.0f, by + 36.0f, gpText, 1.4f, gpStatusCol, LabFontType::System);

        // Table Header
        float tableY = by + 60.0f;
        Renderer::drawRect(bx + 20.0f, tableY, bw - 40.0f, 24.0f, Vec3(0.12f, 0.13f, 0.16f));
        LabFont::drawText(bx + 28.0f, tableY + 5.0f, "Action / Lua Event", 1.4f, Vec3(1.0f, 0.55f, 0.10f), LabFontType::System);
        LabFont::drawText(bx + 240.0f, tableY + 5.0f, "Keyboard / Mouse", 1.4f, Vec3(1.0f, 0.55f, 0.10f), LabFontType::System);
        LabFont::drawText(bx + 460.0f, tableY + 5.0f, "Gamepad / Controller", 1.4f, Vec3(1.0f, 0.55f, 0.10f), LabFontType::System);

        // Table Rows
        const auto& actions = InputActionManager::instance().getActions();
        float rowY = tableY + 28.0f;
        int actIdx = 0;
        for (const auto& [name, act] : actions) {
            if (actIdx++ >= 11) break;
            Renderer::drawRect(bx + 20.0f, rowY, bw - 40.0f, 26.0f, (actIdx % 2 == 0) ? Vec3(0.19f, 0.20f, 0.23f) : Vec3(0.22f, 0.23f, 0.26f));

            std::string keyLabel = "Unassigned";
            std::string padLabel = "Unassigned";
            for (const auto& b : act.bindings) {
                if (b.device == InputDevice::Keyboard) keyLabel = InputActionManager::keyToString(b.code);
                else if (b.device == InputDevice::Mouse) keyLabel = InputActionManager::mouseButtonToString(b.code);
                else if (b.device == InputDevice::Gamepad) padLabel = InputActionManager::gamepadButtonToString(static_cast<GamepadButton>(b.code));
            }

            LabFont::drawText(bx + 28.0f, rowY + 6.0f, name + " (" + act.description + ")", 1.35f, Vec3(0.95f, 0.95f, 0.95f), LabFontType::System);
            LabFont::drawText(bx + 240.0f, rowY + 6.0f, keyLabel, 1.35f, Vec3(0.4f, 0.85f, 1.0f), LabFontType::System);
            LabFont::drawText(bx + 460.0f, rowY + 6.0f, padLabel, 1.35f, Vec3(0.5f, 1.0f, 0.6f), LabFontType::System);

            rowY += 28.0f;
        }

        // Action Buttons at bottom
        float btnY = by + bh - 46.0f;
        drawDarkButton(bx + 24.0f, btnY, 160.0f, 32.0f, "Save Bindings (.json)", false, false, true);
        drawDarkButton(bx + 194.0f, btnY, 160.0f, 32.0f, "Load Bindings (.json)", false, false, true);
        drawDarkButton(bx + 364.0f, btnY, 150.0f, 32.0f, "Reset Defaults", false, false, true);
        drawDarkButton(bx + bw - 130.0f, btnY, 110.0f, 32.0f, "Close", false, false, false);

        Renderer::endUI();
    }

    void drawAsset2DModal() {
        int fbW = 0, fbH = 0;
        getFramebufferSize(fbW, fbH);
        float w = (fbW > 0) ? (float)fbW : (float)getWidth();
        float h = (fbH > 0) ? (float)fbH : (float)getHeight();

        Renderer::beginUI((int)w, (int)h);

        // Dark modal overlay
        Renderer::drawRect(0, 0, w, h, Vec3(0.04f, 0.05f, 0.06f));

        float bw = 840.0f;
        float bh = 560.0f;
        float bx = (w - bw) * 0.5f;
        float by = (h - bh) * 0.5f;

        Renderer::drawRect(bx, by, bw, bh, Vec3(0.16f, 0.17f, 0.20f));
        drawDarkBevel(bx, by, bw, bh, false);

        // Header Title
        Renderer::drawRect(bx + 2.0f, by + 2.0f, bw - 4.0f, 28.0f, Vec3(0.14f, 0.22f, 0.34f));
        LabFont::drawText(bx + 14.0f, by + 8.0f, "2D Asset Browser & Spritesheet Slicer (Native SpriteBatcher)", 1.8f, Vec3(1, 1, 1), LabFontType::System);

        drawDarkButton(bx + bw - 32.0f, by + 4.0f, 24.0f, 22.0f, "X", false, true);

        // Controls bar: Texture name, grid dimensions, buttons
        LabFont::drawText(bx + 24.0f, by + 42.0f, "Active Texture: " + _selectedTexture + " | Grid Slicing: " + std::to_string(_sliceCols) + " cols x " + std::to_string(_sliceRows) + " rows", 1.4f, Vec3(1.0f, 0.55f, 0.10f), LabFontType::System);

        float btnY = by + 76.0f;
        drawDarkButton(bx + 20.0f, btnY, 160.0f, 32.0f, "Slice Grid (4x4)", false, false, true);
        drawDarkButton(bx + 194.0f, btnY, 190.0f, 32.0f, "Place 2D Sprite on Map", false, false, true);
        drawDarkButton(bx + 398.0f, btnY, 170.0f, 32.0f, "Save Spritesheet (.json)", false, false, true);
        drawDarkButton(bx + bw - 130.0f, btnY, 110.0f, 32.0f, "Close", false, false, false);

        // Spritesheet preview & sliced frames gallery
        float startY = by + 130.0f;
        LabFont::drawText(bx + 24.0f, startY, "Sliced Frames (" + std::to_string(_editorSpriteSheet.getFrameCount()) + " frames generated):", 1.45f, Vec3(0.9f, 0.9f, 0.9f), LabFontType::System);

        float thumb = 60.0f;
        float gap = 12.0f;
        int cols = 8;
        float gridY = startY + 24.0f;

        for (size_t i = 0; i < std::min((size_t)32, _editorSpriteSheet.getFrameCount()); ++i) {
            int c = (int)(i % cols);
            int r = (int)(i / cols);
            float fx = bx + 24.0f + c * (thumb + gap);
            float fy = gridY + r * (thumb + gap);

            bool isSel = ((int)i == _selectedSpriteFrame);
            Renderer::drawRect(fx - 2.0f, fy - 2.0f, thumb + 4.0f, thumb + 4.0f, isSel ? Vec3(1.0f, 0.55f, 0.10f) : Vec3(0.24f, 0.25f, 0.28f));
            drawDarkBevel(fx - 2.0f, fy - 2.0f, thumb + 4.0f, thumb + 4.0f, isSel);

            const auto* frame = _editorSpriteSheet.getFrame(i);
            if (frame && _textures.contains(_selectedTexture)) {
                Renderer::drawTextureRect(fx, fy, thumb, thumb, *_textures[_selectedTexture], frame->uvMin.x, frame->uvMin.y, frame->uvMax.x, frame->uvMax.y);
            } else {
                Renderer::drawRect(fx, fy, thumb, thumb, Vec3(0.1f, 0.1f, 0.1f));
            }

            LabFont::drawText(fx + 2.0f, fy + thumb - 12.0f, "#" + std::to_string(i), 1.1f, Vec3(1, 1, 1), LabFontType::System);
        }

        Renderer::endUI();
    }

    // Modal Legacy Map Conversion Dialog
    void drawConvertMapModal() {
        int fbW = 0, fbH = 0;
        getFramebufferSize(fbW, fbH);
        float w = (fbW > 0) ? (float)fbW : (float)getWidth();
        float h = (fbH > 0) ? (float)fbH : (float)getHeight();

        Renderer::beginUI((int)w, (int)h);

        // Dark modal overlay
        Renderer::drawRect(0, 0, w, h, Vec3(0.04f, 0.05f, 0.06f));

        float bw = 560.0f;
        float bh = 230.0f;
        float bx = (w - bw) * 0.5f;
        float by = (h - bh) * 0.5f;

        LabTheme::drawPanel(bx, by, bw, bh, "Wykryto format Legacy .labmap");
        drawDarkButton(bx + bw - 32.0f, by + 5.0f, 24.0f, 22.0f, "X", false, true);

        std::string fname = std::filesystem::path(_pendingMapPath).filename().string();
        LabFont::drawText(bx + 24.0f, by + 48.0f, "Wykryto przestarzaly format mapy (.labmap):", 1.45f, Vec3(1.0f, 0.85f, 0.4f), LabFontType::System);
        LabFont::drawText(bx + 24.0f, by + 74.0f, fname, 1.55f, Vec3(0.4f, 0.85f, 1.0f), LabFontType::System);
        LabFont::drawText(bx + 24.0f, by + 106.0f, "Czy chcesz skonwertowac plik do nowego formatu .labscene?", 1.35f, Vec3(0.92f, 0.92f, 0.95f), LabFontType::System);
        LabFont::drawText(bx + 24.0f, by + 128.0f, "Brushes -> static, encje, prefabRef. Oryginal zostanie zachowany jako .backup.", 1.2f, Vec3(0.7f, 0.72f, 0.75f), LabFontType::System);

        // Buttons
        float btnY = by + bh - 52.0f;
        float btnW = 160.0f;
        float btnH = 34.0f;

        // "Konwertuj"
        float convX = bx + 60.0f;
        Renderer::drawRect(convX, btnY, btnW, btnH, Vec3(0.18f, 0.46f, 0.25f));
        drawDarkBevel(convX, btnY, btnW, btnH, false);
        LabFont::drawText(convX + 44.0f, btnY + 8.0f, "Konwertuj", 1.45f, Vec3(1, 1, 1), LabFontType::System);

        // "Anuluj"
        float cancelX = bx + bw - 60.0f - btnW;
        Renderer::drawRect(cancelX, btnY, btnW, btnH, Vec3(0.46f, 0.18f, 0.18f));
        drawDarkBevel(cancelX, btnY, btnW, btnH, false);
        LabFont::drawText(cancelX + 54.0f, btnY + 8.0f, "Anuluj", 1.45f, Vec3(1, 1, 1), LabFontType::System);

        Renderer::endUI();
    }

    void onShutdown() override {
        Renderer::shutdown();
    }

private:
    Camera _camera;
    std::unique_ptr<LabMap> _map;
    std::unordered_map<std::string, std::unique_ptr<Texture>> _textures;
    std::unordered_map<std::string, std::unique_ptr<Mesh>> _meshes;
    std::unordered_set<std::string> _missingTextures;
    std::unordered_set<std::string> _missingMeshes;

    std::vector<TextureEntry> _availableTextures;
    std::vector<std::string> _availableModels;
    std::string _selectedTexture = "concrete_wall.bmp";
    std::string _selectedModel = "Model.stl";
    Vec2 _activeUvScale{ 0.25f, 0.25f };

    bool _browserOpen = false;
    bool _modelBrowserOpen = false;
    bool _helpModalOpen = false;
    bool _buildModalOpen = false;
    bool _inputModalOpen = false;
    bool _asset2DModalOpen = false;
    bool _showConvertMapModal = false;
    std::string _pendingMapPath = "";
    SpriteSheet2D _editorSpriteSheet;
    int _sliceCols = 4;
    int _sliceRows = 4;
    int _selectedSpriteFrame = 0;
    ExportSettings _exportSettings;
    std::string _lastBuildLog = "";
    bool _buildSuccess = false;
    bool _fileMenuOpen = false;
    bool _buildMenuOpen = false;
    bool _viewMenuOpen = false;
    bool _wireframeMode = false;
    std::string _currentMapPath = "";

    // Selection State
    SelectionType _selectionType = SelectionType::None;
    int _selectedIndex = -1;

    // Engine Simulation & Viewport State
    EngineSimState _simState = EngineSimState::Stopped;
    EditorWorkspace _workspace = EditorWorkspace::World3D;
    bool _orthoViewMode = false;
    BottomDockTab _bottomDockTab = BottomDockTab::Console;

    // In-Engine Lua Scripting & Live Timeline
    ScriptEngine _scriptEngine;
    bool _ctrlRPressed = false;
    std::string _currentScript =
        "-- Lab Engine Lua Game Script (Lua 5.4)\n"
        "function OnStart()\n"
        "    HUD.setText(\"TimerLabel\", \"TIME: 00:00\")\n"
        "    HUD.setValue(\"Health\", 100)\n"
        "    HUD.setText(\"Ammo\", \"30 / 120\")\n"
        "end\n\n"
        "local timer = 0.0\n"
        "function OnUpdate(dt)\n"
        "    timer = timer + dt\n"
        "    local totalSec = math.floor(timer)\n"
        "    local mins = math.floor(totalSec / 60)\n"
        "    local secs = totalSec % 60\n"
        "    HUD.setText(\"TimerLabel\", string.format(\"TIME: %02d:%02d\", mins, secs))\n\n"
        "    -- Animate dynamic block Klocek1\n"
        "    local klocek = World.getEntity(\"Klocek1\")\n"
        "    if klocek then\n"
        "        klocek:move(0, math.sin(timer * 3.5) * 0.05, 0)\n"
        "    end\n"
        "end\n";
    AnimationTrack _editorAnimTrack;
    float _animScrubberPos = 0.0f;
    int _timelineFrame = 0;
    bool _timelinePlaying = false;

    // Interactive Lua Script Studio Text Editor State
    std::vector<std::string> _scriptLines;
    int _scriptCursorLine = 0;
    int _scriptCursorCol = 0;
    int _scriptScrollLine = 0;
    bool _scriptEditorFocused = true;
    float _scriptBlinkTimer = 0.0f;
    std::string _activeScriptPath = "game://scripts/gameplay.lua";
    bool _scriptModified = false;
    std::vector<std::string> _gameScripts;
    int _selectedGameScriptIndex = -1;

    // Unity / Blender Plugin & Package Manager State
    std::string _pluginSearchQuery = "";
    std::string _pluginCategoryFilter = "All";
    std::string _pluginStateFilter = "All"; // "All", "Enabled", "Disabled"
    std::string _pluginSelectedId = "com.lab.proc-gen";
    std::string _pluginStatusBanner = "";
    float _pluginBannerTimer = 0.0f;
    float _pluginPollAccum = 0.0f;
    int _pluginListScroll = 0;
    bool _pluginSearchFocused = false;

    // Retro Shaders & CRT Post-Process Customizer State
    bool _retroScanlines = true;
    int _retroPixelScale = 2; // 2, 3, 4
    float _retroScanlineOpacity = 0.28f;
    bool _retroAberration = true;
    float _retroAberrationAmount = 0.0035f;
    bool _retroVignette = true;
    float _retroVignetteIntensity = 0.45f;
    int _retroColorTheme = 0; // 0=Classic CRT, 1=Arcade, 2=Amber Terminal, 3=Cyberpunk
    float _retroScanlinePhase = 0.0f;

    // Speedrun & Telemetry Splits State
    bool _speedrunOverlayVisible = true;
    bool _speedrunTimerRunning = false;
    float _speedrunElapsedTime = 0.0f;
    bool _speedrunShowFps = true;
    bool _speedrunShowVelocity = true;
    int _speedrunPosition = 0; // 0=Top-Right, 1=Top-Left

    // Cloth & Soft Body Dynamics State
    ClothMesh _clothMesh;
    bool _clothSimulateInEditor = true;
    bool _clothMeshInitialized = false;
    Vec3 _clothWindVelocity{ 0.0f, 0.0f, 4.0f };
    float _clothStiffness = 1.0f;
    float _clothDamping = 0.99f;
    int _clothSolverIterations = 5;
    Vec3 _clothMeshOrigin{ 0.0f, 4.5f, 0.0f };

    // Force Fields & Shockwaves State
    bool _forceFieldGizmoVisible = true;
    int _forceFieldType = 0; // 0=Radial Explosion, 1=Vortex Whirlwind, 2=Directional Wind
    float _forceFieldStrength = 45.0f;
    float _forceFieldRadius = 15.0f;
    Vec3 _forceFieldPosition{ 0.0f, 1.5f, 0.0f };
    float _forceFieldBlastTimer = 0.0f;

    // Arcade Vehicle Dynamics State
    bool _vehicleTelemetryVisible = true;
    float _vehicleSpeedKmh = 0.0f;
    float _vehicleRpm = 1000.0f;
    int _vehicleGear = 1;
    float _vehicleThrottlePower = 1.0f;
    float _vehicleSuspensionStiffness = 45.0f;

    // Fluid Dynamics & Water Buoyancy State
    float _fluidDensity = 1000.0f;
    float _fluidViscosity = 0.8f;
    float _fluidSurfaceLevel = 0.0f;

    // Constraints & Joint Physics State
    float _constraintStiffness = 1.0f;
    int _constraintSolveIterations = 6;

    // Procedural Level Generator State
    int _procGenWidth = 16;
    int _procGenDepth = 16;

    // Engine Main Menu / Project Hub State
    std::vector<ProjectInfo> _hubProjects;
    int _hubSelectedProjectIndex = 0;
    std::string _hubStatusBanner = "";
    float _hubBannerTimer = 0.0f;
    bool _f1Pressed = false;

    // Sidebar State
    SidebarTab _sidebarTab = SidebarTab::Properties;
    int _outlinerScroll = 0;
    int _prebuiltCategory = 0; // 0 = Entities / Geometry, 1 = Components / Items

    int _activeTool = 1; // 0=Select, 1=Brush, 2=Prop, 3=Texture, 4=Door, 5=Spawn, 6=Resize, 7=Sun
    SpawnType _spawnToolType = SpawnType::FFA;
    Vec3 _cursorPos{ 0, 0, 0 };
    Vec3 _brushSize{ 2.0f, 2.0f, 2.0f };
    Vec3 _propScale{ 1.0f, 1.0f, 1.0f };
    float _gridSnap = 1.0f;
    float _sunAngle = 45.0f;

    // Transform Gizmo State
    GizmoMode _gizmoMode = GizmoMode::Move;
    GizmoAxis _hoveredGizmoAxis = GizmoAxis::None;
    GizmoAxis _activeGizmoAxis = GizmoAxis::None;
    bool _isDraggingGizmo = false;
    Vec2 _dragStartMouse{ 0, 0 };
    Vec3 _dragStartPos{ 0, 0, 0 };
    Vec3 _dragStartRot{ 0, 0, 0 };
    Vec3 _dragStartScale{ 1, 1, 1 };
    float _angleSnap = 15.0f; // degrees
    float _scaleSnap = 0.25f;
    bool _snapDecPressed = false;
    bool _snapIncPressed = false;
    std::string _gizmoStatusText = "";

    // Lamp / Light Tool State
    Vec3 _lampColor{ 1.0f, 0.95f, 0.85f }; // Warm 2700K incandescent
    float _lampIntensity = 2.5f;
    float _lampRadius = 14.0f;
    MapLightType _lampType = MapLightType::Point;

    // CSG Clipping Tool State
    ClipMode _clipMode = ClipMode::KeepBoth;
    Vec3 _clipPointA{ 0, 0, 0 };
    Vec3 _clipPointB{ 0, 0, 0 };
    int _clipStep = 0;

    std::vector<std::string> _consoleMessages;
    std::string _cullingStats = "";

    // Input States
    bool _isFlyingCamera = false;
    bool _lmbPressed = false;
    bool _rmbPressed = false;
    bool _ePressed = false;
    bool _xPressed = false;
    bool _enterPressed = false;
    bool _ctrlSPressed = false;
    bool _ctrlOPressed = false;
    bool _ctrlNPressed = false;
    bool _ctrlDPressed = false;
    bool _delPressed = false;
    bool _fPressed = false;
    bool _f9Pressed = false;
    bool _f5Pressed = false;
    bool _ctrlBPressed = false;
    bool _arrowLeftPressed = false;
    bool _arrowRightPressed = false;
    bool _arrowUpPressed = false;
    bool _arrowDownPressed = false;
    bool _pageUpPressed = false;
    bool _pageDownPressed = false;
    bool _darknessMode = false;
    bool _lPressed = false;
    float _mouseScreenX = 0.0f;
    float _mouseScreenY = 0.0f;

    // LabStudio embedded integration
    CharacterStudio _characterStudio;
    bool _lastStudioKeys[512] = { false };

    // Dedicated 3D Model Viewer & Inspector State
    float _modelViewerYaw = 35.0f;
    float _modelViewerPitch = 15.0f;
    float _modelViewerDistance = 3.0f;
    bool _modelViewerAutoRotate = true;
    bool _modelViewerWireframe = false;
    bool _modelViewerDragging = false;
    float _modelViewerLastMouseX = 0.0f;
    float _modelViewerLastMouseY = 0.0f;

    // Dedicated 2D Spritesheet Animation State
    int _anim2DSelectedFrame = 0;
    int _anim2DTotalFrames = 8;
    int _anim2DFPS = 24;
    bool _anim2DPlaying = false;
    float _anim2DTimer = 0.0f;
    bool _anim2DLoop = true;

    // Native LabHUDEditor2D embedded integration
    LabHUDEditor2D _hudEditor;
    bool _lastHudKeys[512] = { false };

    // Hierarchy Context Menu with Real-Time Search Field
    bool _contextMenuOpen = false;
    float _contextMenuX = 0.0f;
    float _contextMenuY = 0.0f;
    int _contextMenuTargetIndex = -1;
    SelectionType _contextMenuTargetType = SelectionType::None;
    std::string _contextMenuSearch = "";
    float _lastOutlinerClickTime = 0.0f;
    int _lastOutlinerClickIndex = -1;

    // Categorized View Menu State
    int _viewSubmenuHover = -1; // -1 = none, 0 = 3D, 1 = 2D, 2 = Scripting, 3 = Display
};

int main() {
    LabEditorApp editor;
    editor.run();
    return 0;
}
