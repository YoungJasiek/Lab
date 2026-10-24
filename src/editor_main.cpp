#include "Lab.h"
#include "LabGraphics.h"
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
#include "LabGpuPreference.h"
#include "LabCloth.h"
#include "LabForceField.h"
#include "LabVehicle.h"
#include "LabFluid.h"
#include "LabConstraint.h"
#include "LabDecals.h"
#include "LabParticles.h"
#include "LabAudio.h"
#include "LabVCS.h"
#include "LabCCD.h"
#include "LabComponents.h"
#include "LabVFS.h"
#include <glad/gl.h>
#include <GLFW/glfw3.h>
#if defined(_WIN32)
#include <windows.h>
#endif
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
#include <fstream>
#include <chrono>
#include <ctime>

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
    Camera,
    Trigger,
    Waypoint
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
    // Project Settings Workspace
    Project_Settings = 15,     // Full-viewport Project Settings & Output Display Config (2D/3D, Res, Fullscreen)
    // Aliases for backwards compatibility
    World3D = 0,
    Level2D = 8,
    HUD = 10,
    Animation = 1,
    Scripting = 11,
    Plugins = 13,
    ProjectHub = 14,
    ProjectSettings = 15
};

enum class BottomDockTab {
    Console,
    Profiler,
    ScriptEditor,
    Timeline
};

struct HammerSnapshot {
    std::string description;
    LabMap mapSnapshot;
    SelectionType selectionType = SelectionType::None;
    int selectedIndex = -1;
};

class LabEditorApp : public Engine {
public:
    LabEditorApp()
        : Engine("Hammer Editor - Frozen-Life Level Editor", 1600, 900),
          _camera(70.0f, 16.0f / 9.0f, 0.01f, 3000.0f) {
    }

    void onInit() override {
        LabLog::info("Launching Hammer Level Editor (Frozen-Life SDK)...");
        Renderer::init();

        // Load non-hardcoded editor theme from JSON
        if (!LabTheme::loadThemeFromJson("assets/theme/theme.json")) {
            LabTheme::saveThemeToJson("assets/theme/theme.json");
        }

        // Scan textures and models
        discoverTextures();
        discoverModels();

        // Initialize clean default studio scene
        newMap();

        // Camera initial pose pointing directly at starter scene geometry
        _camera.setPosition(Vec3(0, 8.0f, 18.0f));
        _camera.lookAt(Vec3(0, 1.0f, 0.0f));

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

        logMessage("Lab Engine Studio initialized. Ready.");
        logMessage("Textures loaded: " + std::to_string(_availableTextures.size()) + " | Models: " + std::to_string(_availableModels.size()));
        logMessage("Lua 5.4 Script Engine active. Workspaces: 3D, 2D, HUD, Animation, Scripting.");
        logMessage("Frustum Culling active: 'To czego oko nie widzi tego maszyna renderowac nie musi'");

        // Initialize Native Engine Subsystems
        AudioEngine::init(true);
        _decalSystem.init();
        _particleSystem.init();
        // Initialize directly into Hammer 3D Level Editor (with %TEMP% crash recovery check)
        checkPreviousSession();
        if (!_showRecoveryPrompt) {
            if (std::filesystem::exists("assets/maps/site14_chapter1.labscene")) {
                loadMapFromFile("assets/maps/site14_chapter1.labscene");
            } else {
                newMap();
            }
        }
        setWorkspace(EditorWorkspace::World3D_Level);
        logMessage("Hammer Level Editor ready. Press [F9] or click 'Run Map' to play in Frozen-Life.");
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
        // Native Subsystem Real-Time Simulation
        _decalSystem.update(fixedDelta);
        _particleSystem.update(fixedDelta, _map.get());
        AudioEngine::update(fixedDelta);

        if (_pluginBannerTimer > 0.0f) {
            _pluginBannerTimer -= fixedDelta;
            if (_pluginBannerTimer <= 0.0f) _pluginStatusBanner = "";
        }
        if (_hubBannerTimer > 0.0f) {
            _hubBannerTimer -= fixedDelta;
            if (_hubBannerTimer <= 0.0f) _hubStatusBanner = "";
        }
        if (_settingsBannerTimer > 0.0f) {
            _settingsBannerTimer -= fixedDelta;
            if (_settingsBannerTimer <= 0.0f) _settingsStatusBanner = "";
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

    // ==================== HAMMER SESSION & %TEMP% JOURNAL ====================
    std::filesystem::path getTempDir() const {
        auto dir = std::filesystem::temp_directory_path() / "LabEngine" / "Hammer";
        std::error_code ec;
        std::filesystem::create_directories(dir, ec);
        return dir;
    }

    std::filesystem::path getAutosavePath() const {
        return getTempDir() / "session_recovery.labscene";
    }

    std::filesystem::path getJournalPath() const {
        return getTempDir() / "session_journal.txt";
    }

    std::string getTimestampString() const {
        auto now = std::chrono::system_clock::now();
        std::time_t t = std::chrono::system_clock::to_time_t(now);
        std::tm tmNow{};
#if defined(_WIN32)
        localtime_s(&tmNow, &t);
#else
        localtime_r(&t, &tmNow);
#endif
        char buf[64];
        std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tmNow);
        return std::string(buf);
    }

    std::string getSelectedEntityName() const {
        if (!_map || _selectedIndex < 0) return "Object";
        switch (_selectionType) {
            case SelectionType::Brush: return "Brush #" + std::to_string(_selectedIndex);
            case SelectionType::Prop:  return "Prop #" + std::to_string(_selectedIndex);
            case SelectionType::Door:  return "Door #" + std::to_string(_selectedIndex);
            case SelectionType::Spawn: return "Spawn #" + std::to_string(_selectedIndex);
            case SelectionType::WeaponSpawner: return "Weapon Spawner #" + std::to_string(_selectedIndex);
            case SelectionType::Light: return "Light #" + std::to_string(_selectedIndex);
            case SelectionType::Camera: return "Camera #" + std::to_string(_selectedIndex);
            case SelectionType::Trigger: return "Trigger #" + std::to_string(_selectedIndex);
            case SelectionType::Waypoint: return "Waypoint #" + std::to_string(_selectedIndex);
            default: return "Object";
        }
    }

    void writeSessionJournal(const std::string& status, const std::string& mapPath, int actions) {
        auto path = getJournalPath();
        std::ofstream out(path, std::ios::trunc);
        if (out.is_open()) {
            out << "status: " << status << "\n";
            out << "map: " << mapPath << "\n";
            out << "time: " << _sessionStartTime << "\n";
            out << "actions: " << actions << "\n";
        }
    }

    void saveSessionAutosave() {
        if (!_map) return;
        auto path = getAutosavePath();
        _map->saveToFile(path.string());
    }

    void checkPreviousSession() {
        auto journal = getJournalPath();
        auto autosave = getAutosavePath();
        if (std::filesystem::exists(journal) && std::filesystem::exists(autosave)) {
            std::ifstream in(journal);
            std::string line;
            std::string status, mapPath, timeStr;
            int actCount = 0;
            while (std::getline(in, line)) {
                if (line.rfind("status: ", 0) == 0) status = line.substr(8);
                else if (line.rfind("map: ", 0) == 0) mapPath = line.substr(5);
                else if (line.rfind("time: ", 0) == 0) timeStr = line.substr(6);
                else if (line.rfind("actions: ", 0) == 0) actCount = std::atoi(line.substr(9).c_str());
            }
            if (status == "in_progress" && actCount > 0) {
                _showRecoveryPrompt = true;
                _recoveryMapName = mapPath;
                _recoveryTimestamp = timeStr;
                _recoveryActionCount = actCount;
                logMessage("[Recovery] Wykryto niezapisana poprzednia sesje dla: " + mapPath + " (" + std::to_string(actCount) + " akcji). Wcisnij Y aby przywrocic lub N aby odrzucic.");
            }
        }
    }

    void startNewSession(const std::string& mapPath) {
        _sessionStartTime = getTimestampString();
        _sessionActionCounter = 0;
        _undoStack.clear();
        _redoStack.clear();
        _isDirty = false;
        writeSessionJournal("in_progress", mapPath, 0);
    }

    void restorePreviousSession() {
        auto path = getAutosavePath();
        if (std::filesystem::exists(path)) {
            auto loaded = LabMap::loadFromFile(path.string());
            if (loaded) {
                _map = std::move(loaded);
                _currentMapPath = _recoveryMapName;
                if (_map) {
                    _scriptEngine.setActiveMap(_map.get());
                }
                _showRecoveryPrompt = false;
                _isDirty = true;
                _sessionStartTime = getTimestampString();
                _sessionActionCounter = _recoveryActionCount;
                _undoStack.clear();
                _redoStack.clear();
                writeSessionJournal("in_progress", _currentMapPath, _sessionActionCounter);
                logMessage("[Recovery] Przywrocono poprzednia sesje robocza (" + std::to_string(_recoveryActionCount) + " modyfikacji z %TEMP%)!");
                return;
            }
        }
        logMessage("[Recovery] Blad: Nie udalo sie wczytac pliku odzyskiwania sesji z %TEMP%!");
        _showRecoveryPrompt = false;
    }

    void discardPreviousSession() {
        std::error_code ec;
        std::filesystem::remove(getAutosavePath(), ec);
        writeSessionJournal("discarded", _currentMapPath, 0);
        _showRecoveryPrompt = false;
        logMessage("[Recovery] Odrzucono sesje tymczasowa. Wczytuje biezaca mape.");
        if (std::filesystem::exists("assets/maps/site14_chapter1.labscene")) {
            loadMapFromFile("assets/maps/site14_chapter1.labscene");
        } else {
            newMap();
        }
    }

    void recordSnapshot(const std::string& description) {
        if (!_map) return;
        HammerSnapshot snap;
        snap.description = description;
        snap.mapSnapshot = *_map;
        snap.selectionType = _selectionType;
        snap.selectedIndex = _selectedIndex;

        _undoStack.push_back(std::move(snap));
        if (_undoStack.size() > 64) {
            _undoStack.erase(_undoStack.begin());
        }
        _redoStack.clear();
        _isDirty = true;
        _sessionActionCounter++;

        saveSessionAutosave();
        writeSessionJournal("in_progress", _currentMapPath, _sessionActionCounter);
    }

    void performUndo() {
        if (_undoStack.empty() || !_map) {
            logMessage("Undo: Brak akcji do cofniecia.");
            return;
        }
        HammerSnapshot currentSnap;
        currentSnap.description = "Biezacy stan";
        currentSnap.mapSnapshot = *_map;
        currentSnap.selectionType = _selectionType;
        currentSnap.selectedIndex = _selectedIndex;
        _redoStack.push_back(std::move(currentSnap));

        HammerSnapshot prev = std::move(_undoStack.back());
        _undoStack.pop_back();

        *_map = std::move(prev.mapSnapshot);
        _selectionType = prev.selectionType;
        _selectedIndex = prev.selectedIndex;
        if (_map) {
            _scriptEngine.setActiveMap(_map.get());
        }

        _sessionActionCounter++;
        saveSessionAutosave();
        writeSessionJournal("in_progress", _currentMapPath, _sessionActionCounter);
        logMessage("Undo: Cofnieto '" + prev.description + "' (Pozostalo na stosie: " + std::to_string(_undoStack.size()) + ")");
    }

    void performRedo() {
        if (_redoStack.empty() || !_map) {
            logMessage("Redo: Brak akcji do ponowienia.");
            return;
        }
        HammerSnapshot currentSnap;
        currentSnap.description = "Biezacy stan";
        currentSnap.mapSnapshot = *_map;
        currentSnap.selectionType = _selectionType;
        currentSnap.selectedIndex = _selectedIndex;
        _undoStack.push_back(std::move(currentSnap));

        HammerSnapshot next = std::move(_redoStack.back());
        _redoStack.pop_back();

        *_map = std::move(next.mapSnapshot);
        _selectionType = next.selectionType;
        _selectedIndex = next.selectedIndex;
        if (_map) {
            _scriptEngine.setActiveMap(_map.get());
        }

        _sessionActionCounter++;
        saveSessionAutosave();
        writeSessionJournal("in_progress", _currentMapPath, _sessionActionCounter);
        logMessage("Redo: Ponowiono '" + next.description + "'");
    }

    void newMap() {
        _map = std::make_unique<LabMap>();
        _map->metadata.name = "untitled_map";
        _map->metadata.author = "Mapper";
        _map->spawn.position = Vec3(0, 1.8f, 0);

        std::string activeDim = "3D";
        auto activeProj = ProjectManager::get().getProjectInfo(ProjectManager::get().getActiveProjectName());
        if (!activeProj.dimension.empty()) {
            activeDim = activeProj.dimension;
        }

        if (activeDim == "2D") {
            // Native 2D starter scene
            MapCamera cam2D;
            cam2D.name = "Camera2D";
            cam2D.position = Vec3(0.0f, 0.0f, 10.0f);
            cam2D.rotation = Vec3(0.0f, -90.0f, 0.0f);
            cam2D.fov = 10.0f;
            cam2D.orthographic = true;
            cam2D.orthoSize = 10.0f;
            cam2D.active = true;
            _map->cameras.push_back(cam2D);

            MapSprite2D spr;
            spr.name = "StarterSprite_01";
            spr.position = Vec3(0.0f, 0.0f, 0.0f);
            spr.size = Vec2(2.0f, 2.0f);
            spr.color = Vec4(0.2f, 0.6f, 0.9f, 1.0f);
            spr.texturePath = "crate.bmp";
            _map->sprites.push_back(spr);

            MapLight lgt;
            lgt.name = "Light2D";
            lgt.position = Vec3(0.0f, 0.0f, 5.0f);
            lgt.color = Vec3(1.0f, 1.0f, 1.0f);
            lgt.intensity = 1.5f;
            lgt.radius = 20.0f;
            _map->lights.push_back(lgt);
        } else {
            // Clean 3D starter scene: Exactly 1 Camera, 1 Cube, 1 Light (Zero player, zero weapons, zero ammo)
            MapCamera cam3D;
            cam3D.name = "MainCamera";
            cam3D.position = Vec3(0.0f, 2.0f, 5.0f);
            cam3D.rotation = Vec3(0.0f, -90.0f, 0.0f);
            cam3D.fov = 70.0f;
            cam3D.active = true;
            _map->cameras.push_back(cam3D);

            MapBrush floor;
            floor.name = "Floor";
            floor.position = Vec3(0, -0.5f, 0);
            floor.size = Vec3(32.0f, 1.0f, 32.0f);
            floor.color = Vec3(0.8f, 0.8f, 0.8f);
            floor.texturePath = "floor_tiles.bmp";
            floor.uvScale = Vec2(0.25f, 0.25f);
            floor.uvMode = 1;
            _map->brushes.push_back(floor);

            MapBrush block;
            block.name = "Cube1";
            block.position = Vec3(0, 1.0f, 0.0f);
            block.size = Vec3(2.0f, 2.0f, 2.0f);
            block.color = Vec3(0.95f, 0.55f, 0.15f);
            block.texturePath = "concrete_wall.bmp";
            block.uvScale = Vec2(0.25f, 0.25f);
            block.uvMode = 1;
            _map->brushes.push_back(block);

            MapLight sun;
            sun.name = "SunLight";
            sun.position = Vec3(0.0f, 8.0f, 0.0f);
            sun.color = Vec3(1.0f, 0.95f, 0.85f);
            sun.intensity = 2.5f;
            sun.radius = 35.0f;
            _map->lights.push_back(sun);
        }

        if (_map) {
            _scriptEngine.setActiveMap(_map.get());
        }

        _currentMapPath = "assets/maps/untitled_map.labscene";
        _selectionType = SelectionType::None;
        _selectedIndex = -1;
        startNewSession(_currentMapPath);
        logMessage("Created New " + activeDim + " Map (1 Camera, 1 Block, 1 Light).");
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
                startNewSession(_currentMapPath);
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
            case EditorWorkspace::Project_Settings: return "Project > Project Display & Engine Settings";
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
            case EditorWorkspace::Project_Settings: return "Project > Settings";
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
        if (_isFlyingCamera) {
            _isFlyingCamera = false;
            setCursorCaptured(false);
        }
        if (ws == EditorWorkspace::Project_Hub) {
            refreshProjectHub();
        } else if (ws == EditorWorkspace::Project_Settings) {
            auto info = ProjectManager::get().getProjectInfo(ProjectManager::get().getActiveProjectName());
            _settingsProjectTitle = info.displayName;
            _settingsDimension = info.dimension.empty() ? "3D" : info.dimension;
            _settingsWidth = info.display.width > 0 ? info.display.width : 1280;
            _settingsHeight = info.display.height > 0 ? info.display.height : 720;
            _settingsFullscreen = info.display.fullscreen;
            _settingsVSync = info.display.vsync;
            _settingsEntryScene = info.defaultScene;
            _settingsStatusBanner = "";
        } else if (ws == EditorWorkspace::World3D_Level || ws == EditorWorkspace::World3D_Animation || ws == EditorWorkspace::World3D_ModelViewer) {
            _orthoViewMode = false;
            InitializeCamera3D(_camera, 70.0f, 16.0f / 9.0f);
        } else if (ws == EditorWorkspace::Level2D_Tilemap || ws == EditorWorkspace::Level2D_Animation || ws == EditorWorkspace::Level2D_HUD) {
            _orthoViewMode = true;
            InitializeCamera2D(_camera, 25.0f, 16.0f / 9.0f);
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
                _isDirty = false;
                writeSessionJournal("in_progress", _currentMapPath, _sessionActionCounter);
                logMessage("Scene saved to: " + _currentMapPath);
            }
        } else {
            _map->saveToFile(_currentMapPath);
            _isDirty = false;
            writeSessionJournal("in_progress", _currentMapPath, _sessionActionCounter);
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

    void runMapInGame() {
        if (_currentMapPath.empty()) {
            _currentMapPath = "assets/maps/site14_chapter1.labscene";
        }
        if (_map) {
            _map->saveToFile(_currentMapPath);
        }
        logMessage("Hammer: Map saved to " + _currentMapPath);

        std::filesystem::path gameExe = "build/Release/FrozenLife.exe";
        if (!std::filesystem::exists(gameExe)) {
            gameExe = "FrozenLife.exe";
        }
        if (!std::filesystem::exists(gameExe)) {
            gameExe = "../Release/FrozenLife.exe";
        }

        std::string launchArgs = "+map " + _currentMapPath;
        logMessage("Hammer: Launching game -> " + gameExe.string() + " " + launchArgs);
        ExportPipeline::launchGame(gameExe, launchArgs);
    }

    void bakeMapAction() {
        if (!_map) {
            logMessage("Bake: No active map to bake!");
            return;
        }

        // Determine destination production path (.lmap), preserving project workspace directory
        std::filesystem::path outPath = _currentMapPath;
        if (outPath.empty()) {
            auto projAssets = ProjectManager::get().getActiveProjectAssetsPath();
            std::filesystem::path mapsDir = std::filesystem::exists(projAssets) ? (projAssets / "maps") : std::filesystem::path("assets/maps");
            _currentMapPath = (mapsDir / "untitled_map.labscene").string();
            outPath = mapsDir / "untitled_map.lmap";
        } else {
            std::filesystem::path parentDir = outPath.parent_path();
            std::string stem = outPath.stem().string();
            if (parentDir.empty()) {
                parentDir = "assets/maps";
            }
            outPath = parentDir / (stem + ".lmap");
        }

        // 1. Save active working map (dev/modding non-destructive text format) so no work is lost
        _map->saveToFile(_currentMapPath);
        _isDirty = false;
        logMessage("Bake: Synced authoring scene to " + _currentMapPath);

        // 2. Perform static lightmap baking (direct diffuse + ambient + point lights)
        logMessage("Bake: Calculating static radiosity & direct lightmaps...");
        _bakedLightmap = LightmapBaker::bake(
            *_map,
            _map->metadata.sunDir,
            _map->metadata.sunColor,
            _map->metadata.ambientColor,
            256
        );
        if (_bakedLightmap) {
            std::filesystem::path lmPath = outPath;
            lmPath.replace_extension(".lightmap");
            _bakedLightmap->saveToFile(lmPath.string());
            Renderer::setLightmap(_bakedLightmap.get());
            logMessage("Bake: Lightmap baked and applied to viewport (" + lmPath.filename().string() + ")");
        }

        // 3. Collect all unique level assets referenced by this map for the embedded production package (.lpak)
        std::vector<std::pair<std::string, std::filesystem::path>> embeddedAssets;
        std::unordered_set<std::string> visited;

        auto addAsset = [&](const std::string& vpath) {
            if (vpath.empty() || visited.count(vpath)) return;
            visited.insert(vpath);

            // Resolve asset path respecting Decoupled Projects, Documents/LabGames, and VFS
            std::filesystem::path diskPath = ProjectManager::get().resolveAssetPath(vpath);
            if (!std::filesystem::exists(diskPath)) {
                diskPath = std::filesystem::path("assets") / vpath;
            }
            if (!std::filesystem::exists(diskPath)) {
                diskPath = vpath;
            }

            if (std::filesystem::exists(diskPath) && !std::filesystem::is_directory(diskPath)) {
                embeddedAssets.emplace_back(vpath, diskPath);
            }
        };

        // Collect from all map entity types:
        // Brushes (textures, normal maps, scripts)
        for (const auto& b : _map->brushes) {
            addAsset(b.texturePath);
            addAsset(b.normalMapPath);
            addAsset(b.script);
        }
        // Props (models, custom textures, normal maps, scripts, audio)
        for (const auto& p : _map->props) {
            addAsset(p.modelPath);
            addAsset(p.texturePath);
            addAsset(p.normalMapPath);
            addAsset(p.script);
            if (p.hasAudio) addAsset(p.audioSound);
        }
        // Doors (scripts)
        for (const auto& d : _map->doors) {
            addAsset(d.script);
        }
        // Triggers (scripts, audio dialogs)
        for (const auto& tr : _map->triggers) {
            addAsset(tr.script);
            addAsset(tr.audioPath);
        }
        // Sprites (textures, scripts)
        for (const auto& s : _map->sprites) {
            addAsset(s.texturePath);
            addAsset(s.script);
        }
        // Environment cubemap
        addAsset(_map->metadata.cubemapPath);

        // 4. Bake to production binary container (.lmap)
        std::error_code ec;
        std::filesystem::create_directories(outPath.parent_path(), ec);
        bool success = _map->bakeToFile(outPath.string(), true, embeddedAssets);
        if (success) {
            logMessage("Bake: Successfully baked production map '" + outPath.string() + "' with " +
                       std::to_string(embeddedAssets.size()) + " embedded assets (Encrypted & CRC32 validated)");
        } else {
            logMessage("Bake: ERROR failed to bake production map to '" + outPath.string() + "'");
        }
    }

    void buildGameAction(bool runAfterBuild) {
        if (runAfterBuild) {
            runMapInGame();
            return;
        }
        if (_currentMapPath.empty()) {
            _currentMapPath = "assets/maps/export_game.labmap";
        }
        if (_map) {
            _map->saveToFile(_currentMapPath);
        }
        _exportSettings.entrySceneURI = _currentMapPath;
        _exportSettings.dimension = (_workspace == EditorWorkspace::Level2D_Tilemap ||
                                     _workspace == EditorWorkspace::Level2D_HUD ||
                                     _workspace == EditorWorkspace::Level2D_Animation) ? "2D" : "3D";
        _exportSettings.retroFX = false;
        _exportSettings.retroScanlines = _retroScanlines;
        _exportSettings.retroVignette = _retroVignette;
        _exportSettings.retroAberration = _retroAberration;
        _exportSettings.retroPixelScale = _retroPixelScale;
        _exportSettings.retroColorTheme = _retroColorTheme;

        if (runAfterBuild) {
            logMessage("Compiling and packaging test game in DEBUG mode (destination: test_build/debug)...");
            ExportResult res = ExportPipeline::buildAndPrepareTestGame(_exportSettings.gameTitle, _exportSettings.entrySceneURI, "test_build/debug", &_exportSettings);
            _buildSuccess = res.success;
            _lastBuildLog = res.logMessage;
            if (res.success) {
                logMessage("TEST BUILD SUCCESS: " + res.logMessage);
                logMessage("Files packaged: " + std::to_string(res.copiedFiles) + " | Size: " + std::to_string(res.totalBytes / 1024) + " KB");
                logMessage("Launching test game process: " + res.exportedExecutable.string());
                std::string mapFileName = std::filesystem::path(_currentMapPath).filename().string();
                std::string launchArgs = "--config game.config --scene assets/maps/" + mapFileName;
                bool launched = ExportPipeline::launchGame(res.exportedExecutable, launchArgs);
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

        // Orbit Camera with Middle Mouse Button (MMB / button 2) OR Alt + Left Mouse Button
        bool isAltDown = Input::isKeyPressed(342) || Input::isKeyPressed(346); // GLFW_KEY_LEFT_ALT / RIGHT_ALT
        bool isOrbiting = is3DMapEditing && in3DViewport && (Input::isMouseButtonPressed(2) || (isAltDown && Input::isMouseButtonPressed(0)));

        // Mouse look in 3D Viewport when holding Right Mouse Button (only in 3D map workspaces)
        if (isOrbiting) {
            Vec3 pivot = (_selectionType != SelectionType::None) ? getSelectedPosition() : _cursorPos;
            _camera.orbit(pivot, Input::mouseDelta, 0.25f);
            if (_isFlyingCamera) {
                _isFlyingCamera = false;
                setCursorCaptured(false);
            }
        } else if (is3DMapEditing && Input::isMouseButtonPressed(1)) {
            if (_activeTool != 3) {
                if (!_isFlyingCamera && in3DViewport) {
                    _isFlyingCamera = true;
                    setCursorCaptured(true);
                    Input::mouseDelta = { 0.0f, 0.0f }; // Reset pre-capture deltas to avoid 180-deg flip
                } else if (_isFlyingCamera) {
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

        // Blender-style drag-field update (Position / Size / Rotation)
        updateDragFields(mx);

        // Handle Left-Click
        if (Input::isMouseButtonJustPressed(0) || (Input::isMouseButtonPressed(0) && !_lmbPressed)) {
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
            bool inMvViewport = (mx >= vpX && mx <= vpX + vpW && my >= vpY && my <= vpY + vpH);
            if (inMvViewport && Input::isMouseButtonPressed(0)) {
                if (!_modelViewerDragging) {
                    _modelViewerDragging = true;
                    _modelViewerLastMouseX = mx;
                    _modelViewerLastMouseY = my;
                }
                float dx = mx - _modelViewerLastMouseX;
                float dy = my - _modelViewerLastMouseY;
                _modelViewerYaw += dx * 0.4f;
                _modelViewerPitch = std::clamp(_modelViewerPitch - dy * 0.4f, -75.0f, 75.0f);
            } else {
                _modelViewerDragging = false;
                if (_modelViewerAutoRotate) {
                    _modelViewerYaw += 25.0f * time.delta;
                    if (_modelViewerYaw >= 360.0f) _modelViewerYaw -= 360.0f;
                }
            }
            if (inMvViewport && std::abs(Input::scrollDelta) > 0.01f) {
                _modelViewerDistance = std::clamp(_modelViewerDistance - Input::scrollDelta * 0.3f, 0.5f, 25.0f);
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

        // Session Crash Recovery Modal keyboard shortcuts (Y / F1 to restore, N / Esc to discard)
        if (_showRecoveryPrompt) {
            static bool s_recY = false;
            static bool s_recN = false;
            if (Input::isKeyPressed(89) || Input::isKeyPressed(290)) { // Y or F1
                if (!s_recY) {
                    restorePreviousSession();
                }
                s_recY = true;
            } else {
                s_recY = false;
            }

            if (Input::isKeyPressed(78) || Input::isKeyPressed(256)) { // N or Esc
                if (!s_recN) {
                    discardPreviousSession();
                }
                s_recN = true;
            } else {
                s_recN = false;
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

        // Direct Numeric Field Keyboard Editing (Blender-style)
        if (_fieldEditRow >= 0) {
            _fieldEditCursorBlink += time.delta * 3.0f;
            static bool s_fBack = false, s_fEnter = false, s_fEsc = false;
            static bool s_lastFKeys[512] = { false };

            if (Input::isKeyPressed(259)) { // Backspace
                if (!s_fBack && !_fieldEditBuffer.empty()) {
                    _fieldEditBuffer.pop_back();
                }
                s_fBack = true;
            } else {
                s_fBack = false;
            }

            if (Input::isKeyPressed(256)) { // Esc -> Cancel
                if (!s_fEsc) cancelFieldEdit();
                s_fEsc = true;
            } else {
                s_fEsc = false;
            }

            if (Input::isKeyPressed(257) || Input::isKeyPressed(335)) { // Enter or Keypad Enter -> Commit
                if (!s_fEnter) commitFieldEdit();
                s_fEnter = true;
            } else {
                s_fEnter = false;
            }

            // Digits 0..9 (keys 48..57)
            for (int k = 48; k <= 57; ++k) {
                if (Input::keys[k] && !s_lastFKeys[k]) {
                    _fieldEditBuffer += (char)k;
                }
                s_lastFKeys[k] = Input::keys[k];
            }
            // Keypad 0..9 (keys 320..329)
            for (int k = 320; k <= 329; ++k) {
                if (Input::keys[k] && !s_lastFKeys[k]) {
                    _fieldEditBuffer += (char)('0' + (k - 320));
                }
                s_lastFKeys[k] = Input::keys[k];
            }
            // Period / Decimal point (46 or 330)
            if ((Input::keys[46] && !s_lastFKeys[46]) || (Input::keys[330] && !s_lastFKeys[330])) {
                if (_fieldEditBuffer.find('.') == std::string::npos) _fieldEditBuffer += '.';
            }
            s_lastFKeys[46] = Input::keys[46];
            s_lastFKeys[330] = Input::keys[330];

            // Minus / Negative sign (45 or 333)
            if ((Input::keys[45] && !s_lastFKeys[45]) || (Input::keys[333] && !s_lastFKeys[333])) {
                if (_fieldEditBuffer.empty() || _fieldEditBuffer[0] != '-') _fieldEditBuffer.insert(_fieldEditBuffer.begin(), '-');
                else _fieldEditBuffer.erase(_fieldEditBuffer.begin());
            }
            s_lastFKeys[45] = Input::keys[45];
            s_lastFKeys[333] = Input::keys[333];

            // In field edit mode, bypass all general editor hotkeys so typing doesn't trigger tool changes
            return;
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

        // F1: Quick Level Editor Focus (Hammer Level Editor)
        if (Input::isKeyPressed(290)) { // GLFW_KEY_F1
            if (!_f1Pressed) {
                setWorkspace(EditorWorkspace::World3D_Level);
                logMessage("Hammer 3D Level Editor: Ready (F9: Run map in Frozen-Life, Ctrl+S: Save)");
                _f1Pressed = true;
            }
        } else {
            _f1Pressed = false;
        }

        // F9: Run Map in FrozenLife (Source Hammer standard)
        if (Input::isKeyPressed(298)) { // GLFW_KEY_F9
            if (!_f9Pressed) {
                runMapInGame();
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
                bakeMapAction();
                runMapInGame();
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

        // Ctrl+Z: Undo | Ctrl+Shift+Z: Redo
        if (ctrlDown && (Input::isKeyPressed('Z') || Input::isKeyPressed('z') || Input::isKeyPressed(90))) {
            if (!_ctrlZPressed) {
                if (shiftDown) {
                    performRedo();
                } else {
                    performUndo();
                }
                _ctrlZPressed = true;
            }
        } else {
            _ctrlZPressed = false;
        }

        // Ctrl+Y: Redo
        if (ctrlDown && (Input::isKeyPressed('Y') || Input::isKeyPressed('y') || Input::isKeyPressed(89))) {
            if (!_ctrlYPressed) {
                performRedo();
                _ctrlYPressed = true;
            }
        } else {
            _ctrlYPressed = false;
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

        // B: Cycle Waypoint Target Bot (Guard -> Scientist -> Worker -> Synth #1 -> Synth #2 -> Any)
        if (!ctrlDown && (Input::isKeyPressed('B') || Input::isKeyPressed('b'))) {
            if (!_bPressed && _map) {
                const char* bCycle[] = { "Guard", "Scientist", "Worker", "Combat Synth #1", "Combat Synth #2", "" };
                int bIdx = 0;
                for (int k = 0; k < 6; ++k) {
                    if (_defaultWaypointBotName == bCycle[k]) { bIdx = k; break; }
                }
                _defaultWaypointBotName = bCycle[(bIdx + 1) % 6];
                if (_selectionType == SelectionType::Waypoint && _selectedIndex >= 0 && _selectedIndex < (int)_map->waypoints.size()) {
                    recordSnapshot("Change Waypoint Target Bot");
                    _map->waypoints[_selectedIndex].botName = _defaultWaypointBotName;
                    logMessage("Waypoint #" + std::to_string(_selectedIndex) + " Target Bot set to: " + (_defaultWaypointBotName.empty() ? "(any bot)" : _defaultWaypointBotName));
                } else {
                    logMessage("Active Waypoint Target Bot: [" + (_defaultWaypointBotName.empty() ? "(any bot)" : _defaultWaypointBotName) + "] (Press 'B' to cycle)");
                }
                _bPressed = true;
            }
        } else {
            _bPressed = false;
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

        // Backspace / Delete: Delete selected object
        if (Input::isKeyPressed(259) || Input::isKeyPressed(261)) { // Backspace or Del
            if (!_delPressed) {
                if (_selectionType != SelectionType::None) {
                    deleteSelection();
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

        // Gizmo Mode Hotkeys (Blender style): G (Move), R (Rotate), S (Scale); legacy W/E/T kept
        if (is3DMapEditing && !_contextMenuOpen && !ctrlDown && _activeTool == 0) {
            static bool s_wPressed = false, s_eModePressed = false, s_rModePressed = false, s_tPressed = false;
            static bool s_gPressed = false, s_sPressed = false;
            const bool flyCam = Input::isMouseButtonPressed(1);

            if (Input::isKeyPressed('W') || Input::isKeyPressed('w')) {
                if (!s_wPressed) { _gizmoMode = GizmoMode::Move; logMessage("Gizmo Mode: Move (W)"); s_wPressed = true; }
            } else s_wPressed = false;

            if (Input::isKeyPressed('E') || Input::isKeyPressed('e')) {
                if (!s_eModePressed) { _gizmoMode = GizmoMode::Rotate; logMessage("Gizmo Mode: Rotate (E)"); s_eModePressed = true; }
            } else s_eModePressed = false;

            if (Input::isKeyPressed('R') || Input::isKeyPressed('r')) {
                if (!s_rModePressed) { _gizmoMode = GizmoMode::Rotate; logMessage("Gizmo Mode: Rotate (R)"); s_rModePressed = true; }
            } else s_rModePressed = false;

            if (!flyCam && (Input::isKeyPressed('G') || Input::isKeyPressed('g'))) {
                if (!s_gPressed) { _gizmoMode = GizmoMode::Move; logMessage("Gizmo Mode: Move (G)"); s_gPressed = true; }
            } else s_gPressed = false;

            if (!flyCam && (Input::isKeyPressed('S') || Input::isKeyPressed('s'))) {
                if (!s_sPressed) { _gizmoMode = GizmoMode::Scale; logMessage("Gizmo Mode: Scale (S)"); s_sPressed = true; }
            } else s_sPressed = false;

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
                recordSnapshot("Transform " + getSelectedEntityName());
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

        // Test Trigger Volumes
        for (size_t i = 0; i < _map->triggers.size(); ++i) {
            const auto& tr = _map->triggers[i];
            Vec3 half = tr.size * 0.5f;
            float t = 0;
            if (rayIntersectAABB(rayOrigin, rayDir, tr.position - half, tr.position + half, t)) {
                if (t < closestT) {
                    closestT = t;
                    hitType = SelectionType::Trigger;
                    hitIndex = (int)i;
                }
            }
        }

        // Test Bot Waypoints
        for (size_t i = 0; i < _map->waypoints.size(); ++i) {
            const auto& wp = _map->waypoints[i];
            Vec3 half(0.5f, 0.5f, 0.5f);
            float t = 0;
            if (rayIntersectAABB(rayOrigin, rayDir, wp.position - half, wp.position + half, t)) {
                if (t < closestT) {
                    closestT = t;
                    hitType = SelectionType::Waypoint;
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
            } else if (_selectionType == SelectionType::Trigger) {
                logMessage("Selected Trigger Volume #" + std::to_string(hitIndex) + " (" + _map->triggers[hitIndex].name + ")");
            } else if (_selectionType == SelectionType::Waypoint) {
                logMessage("Selected Bot Waypoint #" + std::to_string(hitIndex) + " (" + _map->waypoints[hitIndex].name + ")");
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
        } else if (_selectionType == SelectionType::Trigger && _selectedIndex >= 0 && _selectedIndex < (int)_map->triggers.size()) {
            return _map->triggers[_selectedIndex].position;
        } else if (_selectionType == SelectionType::Waypoint && _selectedIndex >= 0 && _selectedIndex < (int)_map->waypoints.size()) {
            return _map->waypoints[_selectedIndex].position;
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
        } else if (_selectionType == SelectionType::Waypoint && _selectedIndex >= 0 && _selectedIndex < (int)_map->waypoints.size()) {
            return Vec3(0.0f, _map->waypoints[_selectedIndex].facingYaw, 0.0f);
        } else if (_selectionType == SelectionType::Spawn && _selectedIndex >= 0 && _selectedIndex < (int)_map->spawnPoints.size()) {
            return Vec3(0.0f, _map->spawnPoints[_selectedIndex].yaw, 0.0f);
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
        } else if (_selectionType == SelectionType::Waypoint && _selectedIndex >= 0 && _selectedIndex < (int)_map->waypoints.size()) {
            auto& wp = _map->waypoints[_selectedIndex];
            wp.facingYaw = std::fmod(wp.facingYaw + dy, 360.0f);
            if (wp.facingYaw < 0.0f) wp.facingYaw += 360.0f;
            _defaultWaypointYaw = wp.facingYaw;
            logMessage("Rotated Waypoint [" + wp.name + "] Facing Yaw to " + std::to_string((int)wp.facingYaw) + "°");
        }
    }

    Vec3 getSelectedScale() const {
        if (!_map) return Vec3(1, 1, 1);
        if (_selectionType == SelectionType::Brush && _selectedIndex >= 0 && _selectedIndex < (int)_map->brushes.size()) {
            return _map->brushes[_selectedIndex].size;
        } else if (_selectionType == SelectionType::Prop && _selectedIndex >= 0 && _selectedIndex < (int)_map->props.size()) {
            return _map->props[_selectedIndex].scale;
        } else if (_selectionType == SelectionType::Trigger && _selectedIndex >= 0 && _selectedIndex < (int)_map->triggers.size()) {
            return _map->triggers[_selectedIndex].size;
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
        } else if (_selectionType == SelectionType::Trigger && _selectedIndex >= 0 && _selectedIndex < (int)_map->triggers.size()) {
            auto& tr = _map->triggers[_selectedIndex];
            tr.size.x = std::max(0.5f, tr.size.x + sx);
            tr.size.y = std::max(0.5f, tr.size.y + sy);
            tr.size.z = std::max(0.5f, tr.size.z + sz);
            logMessage("Scaled Trigger Volume #" + std::to_string(_selectedIndex));
        }
    }

    float getGizmoScale(const Vec3& pivot) const {
        if (_orthoViewMode) return 1.5f;
        float dist = (_camera.getPosition() - pivot).length();
        return std::clamp(dist * 0.08f, 0.4f, 40.0f);
    }

    GizmoAxis testGizmoHit(const Vec3& rayOrigin, const Vec3& rayDir, const Vec3& pivot) const {
        float closestT = 1e9f;
        GizmoAxis bestAxis = GizmoAxis::None;

        float gScale = getGizmoScale(pivot);
        float len = 1.8f * gScale;
        float thick = 0.22f * gScale;
        float cap = 0.35f * gScale;

        bool testMove = (_gizmoMode == GizmoMode::Move || _gizmoMode == GizmoMode::Universal);
        bool testRotate = (_gizmoMode == GizmoMode::Rotate || _gizmoMode == GizmoMode::Universal);
        bool testScale = (_gizmoMode == GizmoMode::Scale || _gizmoMode == GizmoMode::Universal);

        // Move Planes
        if (testMove) {
            float t = 0.0f;
            // Plane XY
            if (rayIntersectAABB(rayOrigin, rayDir, pivot + Vec3(0.2f * gScale, 0.2f * gScale, -0.1f * gScale), pivot + Vec3(0.8f * gScale, 0.8f * gScale, 0.1f * gScale), t) && t < closestT) {
                closestT = t; bestAxis = GizmoAxis::PlaneXY;
            }
            // Plane XZ
            if (rayIntersectAABB(rayOrigin, rayDir, pivot + Vec3(0.2f * gScale, -0.1f * gScale, 0.2f * gScale), pivot + Vec3(0.8f * gScale, 0.1f * gScale, 0.8f * gScale), t) && t < closestT) {
                closestT = t; bestAxis = GizmoAxis::PlaneXZ;
            }
            // Plane YZ
            if (rayIntersectAABB(rayOrigin, rayDir, pivot + Vec3(-0.1f * gScale, 0.2f * gScale, 0.2f * gScale), pivot + Vec3(0.1f * gScale, 0.8f * gScale, 0.8f * gScale), t) && t < closestT) {
                closestT = t; bestAxis = GizmoAxis::PlaneYZ;
            }
            // Axes
            if (rayIntersectAABB(rayOrigin, rayDir, pivot + Vec3(0.1f * gScale, -thick, -thick), pivot + Vec3(len + cap, thick, thick), t) && t < closestT) {
                closestT = t; bestAxis = GizmoAxis::X;
            }
            if (rayIntersectAABB(rayOrigin, rayDir, pivot + Vec3(-thick, 0.1f * gScale, -thick), pivot + Vec3(thick, len + cap, thick), t) && t < closestT) {
                closestT = t; bestAxis = GizmoAxis::Y;
            }
            if (rayIntersectAABB(rayOrigin, rayDir, pivot + Vec3(-thick, -thick, 0.1f * gScale), pivot + Vec3(thick, thick, len + cap), t) && t < closestT) {
                closestT = t; bestAxis = GizmoAxis::Z;
            }
        }

        // Scale Handles
        if (testScale) {
            float t = 0.0f;
            // Uniform center
            if (rayIntersectAABB(rayOrigin, rayDir, pivot - Vec3(0.25f * gScale, 0.25f * gScale, 0.25f * gScale), pivot + Vec3(0.25f * gScale, 0.25f * gScale, 0.25f * gScale), t) && t < closestT) {
                closestT = t; bestAxis = GizmoAxis::ScaleUniform;
            }
            // Scale X
            if (rayIntersectAABB(rayOrigin, rayDir, pivot + Vec3(len - 0.2f * gScale, -cap, -cap), pivot + Vec3(len + cap, cap, cap), t) && t < closestT) {
                closestT = t; bestAxis = GizmoAxis::ScaleX;
            }
            // Scale Y
            if (rayIntersectAABB(rayOrigin, rayDir, pivot + Vec3(-cap, len - 0.2f * gScale, -cap), pivot + Vec3(cap, len + cap, cap), t) && t < closestT) {
                closestT = t; bestAxis = GizmoAxis::ScaleY;
            }
            // Scale Z
            if (rayIntersectAABB(rayOrigin, rayDir, pivot + Vec3(-cap, -cap, len - 0.2f * gScale), pivot + Vec3(cap, cap, len + cap), t) && t < closestT) {
                closestT = t; bestAxis = GizmoAxis::ScaleZ;
            }
        }

        // Rotate Rings
        if (testRotate) {
            float ringR = 1.8f * gScale;
            float ringThick = 0.35f * gScale;
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
        } else if (_selectionType == SelectionType::Trigger && _selectedIndex >= 0 && _selectedIndex < (int)_map->triggers.size()) {
            _map->triggers[_selectedIndex].position += Vec3(dx, dy, dz);
            newPos = _map->triggers[_selectedIndex].position;
        } else if (_selectionType == SelectionType::Waypoint && _selectedIndex >= 0 && _selectedIndex < (int)_map->waypoints.size()) {
            _map->waypoints[_selectedIndex].position += Vec3(dx, dy, dz);
            newPos = _map->waypoints[_selectedIndex].position;
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
        } else if (_selectionType == SelectionType::Trigger && _selectedIndex >= 0 && _selectedIndex < (int)_map->triggers.size()) {
            auto& tr = _map->triggers[_selectedIndex];
            tr.size.x = std::max(0.5f, tr.size.x + dw);
            tr.size.y = std::max(0.5f, tr.size.y + dh);
            tr.size.z = std::max(0.5f, tr.size.z + dd);
            logMessage("Resized Trigger Volume #" + std::to_string(_selectedIndex));
        }
    }

    void duplicateSelection() {
        if (!_map || _selectionType == SelectionType::None) return;
        recordSnapshot("Duplicate " + getSelectedEntityName());
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
        } else if (_selectionType == SelectionType::Trigger && _selectedIndex >= 0 && _selectedIndex < (int)_map->triggers.size()) {
            MapTrigger tr = _map->triggers[_selectedIndex];
            tr.position.x += _gridSnap;
            tr.position.z += _gridSnap;
            tr.name = tr.name + "_copy";
            _map->triggers.push_back(tr);
            _selectedIndex = (int)_map->triggers.size() - 1;
            logMessage("Duplicated Trigger Volume to #" + std::to_string(_selectedIndex));
        } else if (_selectionType == SelectionType::Waypoint && _selectedIndex >= 0 && _selectedIndex < (int)_map->waypoints.size()) {
            MapWaypoint wp = _map->waypoints[_selectedIndex];
            wp.position.x += _gridSnap;
            wp.position.z += _gridSnap;
            wp.name = "wp_" + std::to_string(_map->waypoints.size() + 1);
            wp.orderIndex = (int)_map->waypoints.size();
            _map->waypoints.push_back(wp);
            _selectedIndex = (int)_map->waypoints.size() - 1;
            logMessage("Duplicated & Linked Bot Waypoint to #" + std::to_string(_selectedIndex) + " for [" + (wp.botName.empty() ? "(any bot)" : wp.botName) + "]");
        }
    }

    void deleteSelection() {
        if (!_map || _selectionType == SelectionType::None) return;
        recordSnapshot("Delete " + getSelectedEntityName());
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
        } else if (_selectionType == SelectionType::Trigger && _selectedIndex >= 0 && _selectedIndex < (int)_map->triggers.size()) {
            _map->triggers.erase(_map->triggers.begin() + _selectedIndex);
            logMessage("Deleted Trigger Volume #" + std::to_string(_selectedIndex));
            _selectionType = SelectionType::None;
            _selectedIndex = -1;
        } else if (_selectionType == SelectionType::Waypoint && _selectedIndex >= 0 && _selectedIndex < (int)_map->waypoints.size()) {
            _map->waypoints.erase(_map->waypoints.begin() + _selectedIndex);
            logMessage("Deleted Bot Waypoint #" + std::to_string(_selectedIndex));
            _selectionType = SelectionType::None;
            _selectedIndex = -1;
        }
    }

    void deselectAll() {
        _selectionType = SelectionType::None;
        _selectedIndex = -1;
        logMessage("Deselected all.");
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
        } else if (_selectionType == SelectionType::Trigger && _selectedIndex >= 0 && _selectedIndex < (int)_map->triggers.size()) {
            targetPos = _map->triggers[_selectedIndex].position;
        } else if (_selectionType == SelectionType::Waypoint && _selectedIndex >= 0 && _selectedIndex < (int)_map->waypoints.size()) {
            targetPos = _map->waypoints[_selectedIndex].position;
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

        recordSnapshot("CSG Slice Brush #" + std::to_string(_selectedIndex));
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
                MapTrigger tr;
                tr.name = "trigger_story_" + std::to_string(_map->triggers.size() + 1);
                tr.position = _cursorPos + Vec3(0, 1.5f, 0);
                tr.size = Vec3(4.0f, 3.0f, 4.0f);
                tr.eventType = "story_dialog";
                tr.targetBot = "Bot_Guard";
                tr.dialogText = "Watch out! Sector C is contaminated. Follow me down the corridor!";
                tr.audioPath = "assets/audio/bot_corridor_dialog.wav";
                tr.once = true;
                _map->triggers.push_back(tr);
                _selectionType = SelectionType::Trigger;
                _selectedIndex = (int)_map->triggers.size() - 1;
                logMessage("Placed Trigger Zone Volume [" + tr.name + "] (Story / Gameplay Overlap)");
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
            case 18: { // Bot Waypoint Node
                MapWaypoint wp;
                wp.name = "wp_" + std::to_string(_map->waypoints.size() + 1);
                wp.position = _cursorPos + Vec3(0, 0.5f, 0);
                wp.botName = _defaultWaypointBotName;
                wp.orderIndex = (int)_map->waypoints.size();
                wp.animation = _defaultWaypointAnimation;
                wp.facingYaw = _defaultWaypointYaw;
                wp.waitDuration = _defaultWaypointDuration;
                _map->waypoints.push_back(wp);
                _selectionType = SelectionType::Waypoint;
                _selectedIndex = (int)_map->waypoints.size() - 1;
                logMessage("Placed Bot Waypoint [" + wp.name + "] for [" + (wp.botName.empty() ? "(any bot)" : wp.botName) + "] (" + wp.animation + ", Facing " + std::to_string((int)wp.facingYaw) + "°)");
                break;
            }
        }
    }

    void placeCurrentObject() {
        if (!_map) return;
        recordSnapshot("Place Object");
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

        // Commit numeric field edit if clicked outside
        if (_fieldEditRow >= 0) {
            SidebarLayout sl = getSidebarLayout(w, h);
            float fx, fy, fw, fh;
            dragFieldRect(sl, _fieldEditRow, _fieldEditAxis, fx, fy, fw, fh);
            if (mx < fx || mx > fx + fw || my < fy || my > fy + fh) {
                commitFieldEdit();
            }
        }

        // 0. Hierarchy Context Menu Handling
        if (_contextMenuOpen) {
            float cmX = _contextMenuX;
            float cmY = _contextMenuY;
            float cmW = 260.0f;
            struct CompDef { std::string name; std::string tag; };
            CompDef compRegistry[] = {
                { "Magic Add: Trigger Volume", "Trigger" },
                { "Magic Add: Bot Waypoint", "Waypoint" },
                { "Magic Add: Waypoint (Guard)", "Waypoint" },
                { "Magic Add: Waypoint (Scientist)", "Waypoint" },
                { "Magic Add: Waypoint (Worker)", "Waypoint" },
                { "Magic Add: Waypoint (Synth)", "Waypoint" },
                { "Magic Add: Camera", "Camera" },
                { "Magic Add: Light", "Light" },
                { "Magic Add: Spawn", "Spawn" },
                { "Magic Add: Brush", "Brush" },
                { "Magic Add: Broom", "Broom" },
                { "Magic Add: Door", "Door" },
                { "Script (Lua)", "Script" },
                { "Point Light", "Light" },
                { "Audio Source", "Audio" },
                { "RigidBody Dynamic", "Physics" },
                { "Box Collider", "Physics" },
                { "Verlet Cloth", "Cloth" },
                { "Water Volume", "Fluid" },
                { "Force Field", "Physics" },
                { "Arcade Vehicle", "Vehicle" },
                { "Decal Projected", "Decal" },
                { "Post-Process", "Render" },
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

            int itemCount = std::min((int)filtered.size(), 12);
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
                        if (picked == "Magic Add: Trigger Volume") {
                            placePrebuilt(9);
                        } else if (picked == "Magic Add: Bot Waypoint") {
                            placePrebuilt(18);
                        } else if (picked == "Magic Add: Waypoint (Guard)") {
                            _defaultWaypointBotName = "Guard";
                            placePrebuilt(18);
                        } else if (picked == "Magic Add: Waypoint (Scientist)") {
                            _defaultWaypointBotName = "Scientist";
                            placePrebuilt(18);
                        } else if (picked == "Magic Add: Waypoint (Worker)") {
                            _defaultWaypointBotName = "Worker";
                            placePrebuilt(18);
                        } else if (picked == "Magic Add: Waypoint (Synth)") {
                            _defaultWaypointBotName = "Combat Synth #1";
                            placePrebuilt(18);
                        } else if (picked == "Magic Add: Camera") {
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
                            if (_selectionType == SelectionType::Prop && _selectedIndex >= 0 && _selectedIndex < (int)_map->props.size()) {
                                auto& prop = _map->props[_selectedIndex];
                                if (picked.find("Audio") != std::string::npos) {
                                    prop.hasAudio = true;
                                    logMessage("Context Menu: Attached AudioSourceComponent to Prop #" + std::to_string(_selectedIndex));
                                } else if (picked.find("Physics") != std::string::npos || picked.find("Collider") != std::string::npos || picked.find("RigidBody") != std::string::npos) {
                                    prop.hasPhysics = true;
                                    logMessage("Context Menu: Attached RigidBody Physics Component to Prop #" + std::to_string(_selectedIndex));
                                } else if (picked.find("Cloth") != std::string::npos) {
                                    prop.hasCloth = true;
                                    logMessage("Context Menu: Attached ClothComponent to Prop #" + std::to_string(_selectedIndex));
                                } else if (picked.find("Water") != std::string::npos) {
                                    prop.hasWaterVolume = true;
                                    logMessage("Context Menu: Attached WaterVolumeComponent to Prop #" + std::to_string(_selectedIndex));
                                } else if (picked.find("Force") != std::string::npos) {
                                    prop.hasForceField = true;
                                    logMessage("Context Menu: Attached ForceFieldComponent to Prop #" + std::to_string(_selectedIndex));
                                } else if (picked.find("Vehicle") != std::string::npos) {
                                    prop.hasVehicle = true;
                                    logMessage("Context Menu: Attached VehicleComponent to Prop #" + std::to_string(_selectedIndex));
                                } else if (picked.find("Decal") != std::string::npos) {
                                    prop.hasDecal = true;
                                    logMessage("Context Menu: Attached DecalComponent to Prop #" + std::to_string(_selectedIndex));
                                } else if (picked.find("Post-Process") != std::string::npos) {
                                    prop.hasPostProcess = true;
                                    logMessage("Context Menu: Attached PostProcessComponent to Prop #" + std::to_string(_selectedIndex));
                                } else {
                                    logMessage("Context Menu: Attached " + picked + " to Prop #" + std::to_string(_selectedIndex));
                                }
                            } else {
                                if (picked.find("Audio") != std::string::npos) placePrebuilt(11);
                                else if (picked.find("Physics") != std::string::npos || picked.find("RigidBody") != std::string::npos) placePrebuilt(6);
                                else if (picked.find("Cloth") != std::string::npos) placePrebuilt(15);
                                else if (picked.find("Water") != std::string::npos) placePrebuilt(14);
                                else if (picked.find("Force") != std::string::npos) placePrebuilt(17);
                                else if (picked.find("Vehicle") != std::string::npos) placePrebuilt(13);
                                else if (picked.find("Particle") != std::string::npos) placePrebuilt(12);
                                else {
                                    logMessage("Context Menu: Action performed for " + picked);
                                }
                            }
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

        // 3.9. Session Crash Recovery Modal Handling
        if (_showRecoveryPrompt) {
            float bw = 580.0f;
            float bh = 240.0f;
            float bx = (w - bw) * 0.5f;
            float by = (h - bh) * 0.5f;

            // Close button [X]
            if (mx >= bx + bw - 32.0f && mx <= bx + bw - 8.0f && my >= by + 5.0f && my <= by + 27.0f) {
                discardPreviousSession();
                return;
            }

            float btnY = by + bh - 54.0f;
            float btnW = 200.0f;
            float btnH = 34.0f;

            // "Przywroc sesje" button
            float restX = bx + 45.0f;
            if (mx >= restX && mx <= restX + btnW && my >= btnY && my <= btnY + btnH) {
                restorePreviousSession();
                return;
            }

            // "Odrzuc sesje" button
            float discX = bx + bw - 45.0f - btnW;
            if (mx >= discX && mx <= discX + btnW && my >= btnY && my <= btnY + btnH) {
                discardPreviousSession();
                return;
            }

            // Consume click while modal is open
            return;
        }

        // 4. Dropdown File Menu (Hammer Level Editor)
        if (_fileMenuOpen) {
            float menuX = 10.0f;
            float menuY = 24.0f;
            float menuW = 240.0f;
            float menuH = 152.0f;
            if (mx >= menuX && mx <= menuX + menuW && my >= menuY && my <= menuY + menuH) {
                int itemIdx = (int)((my - menuY) / 24.0f);
                if (itemIdx == 0) newMap();
                else if (itemIdx == 1) openMapDialog();
                else if (itemIdx == 2) saveMapAction(false);
                else if (itemIdx == 3) saveMapAction(true);
                else if (itemIdx == 4) runMapInGame();
                else if (itemIdx >= 5) stop();
                _fileMenuOpen = false;
                return;
            } else {
                _fileMenuOpen = false;
            }
        }

        // 4.1. Dropdown Edit Menu (Undo, Redo, Delete, Duplicate, Deselect)
        if (_editMenuOpen) {
            float menuX = 52.0f;
            float menuY = 24.0f;
            float menuW = 260.0f;
            float menuH = 128.0f;
            if (mx >= menuX && mx <= menuX + menuW && my >= menuY && my <= menuY + menuH) {
                int itemIdx = (int)((my - menuY - 3.0f) / 24.0f);
                if (itemIdx == 0) performUndo();
                else if (itemIdx == 1) performRedo();
                else if (itemIdx == 2) deleteSelection();
                else if (itemIdx == 3) duplicateSelection();
                else if (itemIdx == 4) deselectAll();
                _editMenuOpen = false;
                return;
            } else {
                _editMenuOpen = false;
            }
        }

        // 4.5. Dropdown Map Menu (Level Settings & Compilation)
        if (_buildMenuOpen) {
            float menuX = 138.0f;
            float menuY = 24.0f;
            float menuW = 260.0f;
            float menuH = 125.0f;
            if (mx >= menuX && mx <= menuX + menuW && my >= menuY && my <= menuY + menuH) {
                int itemIdx = (int)((my - menuY - 3.0f) / 24.0f);
                if (itemIdx == 0) runMapInGame(); // Run Map in Frozen-Life (F9)
                else if (itemIdx == 1) { // Darkness Preset
                    if (_map) {
                        _map->metadata.ambientColor = Vec3(0.015f, 0.018f, 0.025f);
                        _map->metadata.sunColor = Vec3(0.0f, 0.0f, 0.0f);
                        _darknessMode = true;
                        logMessage("Applied Darkness Preset to Map Metadata");
                    }
                } else if (itemIdx == 2) { // Sunlight Preset
                    if (_map) {
                        _map->metadata.ambientColor = Vec3(0.25f, 0.28f, 0.32f);
                        _map->metadata.sunColor = Vec3(0.85f, 0.82f, 0.78f);
                        _darknessMode = false;
                        logMessage("Applied Sunlight Preset to Map Metadata");
                    }
                } else if (itemIdx == 3) { // Clear Geometry
                    newMap();
                } else if (itemIdx == 4) { // Entity Report / Outliner
                    _sidebarTab = SidebarTab::Hierarchy;
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
            if (mx >= 10.0f && mx <= 50.0f) { // File
                _fileMenuOpen = !_fileMenuOpen;
                _editMenuOpen = false;
                _viewMenuOpen = false;
                _buildMenuOpen = false;
                return;
            } else if (mx >= 52.0f && mx <= 90.0f) { // Edit
                _editMenuOpen = !_editMenuOpen;
                _fileMenuOpen = false;
                _viewMenuOpen = false;
                _buildMenuOpen = false;
                return;
            } else if (mx >= 92.0f && mx <= 136.0f) { // View
                _viewMenuOpen = !_viewMenuOpen;
                _fileMenuOpen = false;
                _editMenuOpen = false;
                _buildMenuOpen = false;
                return;
            } else if (mx >= 138.0f && mx <= 184.0f) { // Map
                _buildMenuOpen = !_buildMenuOpen;
                _fileMenuOpen = false;
                _editMenuOpen = false;
                _viewMenuOpen = false;
                return;
            } else if (mx >= 186.0f && mx <= 236.0f) { // Tools
                _browserOpen = !_browserOpen;
                return;
            } else if (mx >= 238.0f && mx <= 282.0f) { // Help
                _helpModalOpen = !_helpModalOpen;
                return;
            }

            // Top Workspace Breadcrumb Badge click -> open View menu
            if (mx >= 295.0f && mx <= 600.0f) {
                _viewMenuOpen = !_viewMenuOpen;
                _fileMenuOpen = false;
                _editMenuOpen = false;
                _buildMenuOpen = false;
                return;
            }
        }

        if (_quickFxMenuOpen) {
            float mX = 818.0f;
            float mY = 58.0f;
            float mW = 190.0f;
            float mH = 7 * 26.0f + 8.0f;
            if (mx >= mX && mx <= mX + mW && my >= mY && my <= mY + mH) {
                int itemIdx = (int)((my - (mY + 4.0f)) / 26.0f);
                if (itemIdx == 0) {
                    _darknessMode = !_darknessMode;
                    logMessage(_darknessMode ? "Darkness Mode: ACTIVE (Pitch Black Ambient/Sun)" : "Darkness Mode: DISABLED (Standard Sunlight)");
                } else if (itemIdx == 1) {
                    if (_map) {
                        _map->metadata.ambientColor = Vec3(0.015f, 0.018f, 0.025f);
                        _map->metadata.sunColor = Vec3(0.0f, 0.0f, 0.0f);
                        _darknessMode = true;
                        logMessage("Applied Darkness Preset to Map Metadata (Ambient: 0.015, Sun: 0)");
                    }
                } else if (itemIdx == 2) {
                    _particleSystem.spawnImpact(_cursorPos, Vec3(0, 1, 0), SurfaceType::Metal);
                    logMessage("Spawned metal impact sparks at 3D Cursor (" + std::to_string((int)_cursorPos.x) + ", " + std::to_string((int)_cursorPos.y) + ", " + std::to_string((int)_cursorPos.z) + ")");
                } else if (itemIdx == 3) {
                    _particleSystem.spawnExplosion(_cursorPos, 3.5f);
                    logMessage("Spawned explosion smoke burst at 3D Cursor");
                } else if (itemIdx == 4) {
                    _decalSystem.spawnExplosionScorch(_cursorPos, 2.0f, _map ? _map->brushes : std::vector<MapBrush>{});
                    logMessage("Spawned explosion scorch decal at 3D Cursor");
                } else if (itemIdx == 5) {
                    AudioEngine::playSound3D(SoundID::PistolShot, _cursorPos, 1.0f, 1.0f);
                    logMessage("Auditioned 3D sound effect at 3D Cursor position");
                } else if (itemIdx == 6) {
                    VCSManager::instance().addCommit("snap-" + std::to_string(std::time(nullptr)), "Lead Architect", "Editor Live Snapshot");
                    logMessage("VCS Manager: Created live level snapshot commit");
                }
                _quickFxMenuOpen = false;
                return;
            } else {
                _quickFxMenuOpen = false;
            }
        }

        // 6. Top Toolbar Buttons (y: 24..58, deduplicated responsive utility buttons)
        if (my >= 24.0f && my <= 58.0f) {
            const int tbIcons[12] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 15, 16 };
            for (int i = 0; i < 12; ++i) {
                float bx = 8.0f + i * 26.0f;
                if (mx >= bx && mx <= bx + 24.0f) {
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

            // Transform Gizmo Mode Toolbar Buttons
            if (mx >= 328.0f && mx <= 376.0f) {
                _gizmoMode = GizmoMode::Move;
                _activeTool = 0;
                logMessage("Gizmo Mode: Move (G)");
                return;
            }
            if (mx >= 380.0f && mx <= 432.0f) {
                _gizmoMode = GizmoMode::Rotate;
                _activeTool = 0;
                logMessage("Gizmo Mode: Rotate (R)");
                return;
            }
            if (mx >= 436.0f && mx <= 484.0f) {
                _gizmoMode = GizmoMode::Scale;
                _activeTool = 0;
                logMessage("Gizmo Mode: Scale (S)");
                return;
            }

            // 1. [ > TEST (F5) ]
            if (mx >= 496.0f && mx <= 556.0f) {
                buildGameAction(true);
                return;
            }
            // 2. [ BUILD... ]
            if (mx >= 562.0f && mx <= 618.0f) {
                bakeMapAction();
                runMapInGame();
                return;
            }
            // 3. [ Bake ]
            if (mx >= 624.0f && mx <= 680.0f) {
                bakeMapAction();
                return;
            }
            // 4. [ 2D / 3D ]
            if (mx >= 686.0f && mx <= 750.0f) {
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
            if (mx >= 756.0f && mx <= 812.0f) {
                _activeTool = 7;
                logMessage("Tool 7: Omni Point Light Tool (Click scene or E to place)");
                return;
            }
            // 5. [ QUICK FX v ]
            if (mx >= 818.0f && mx <= 874.0f) {
                _quickFxMenuOpen = !_quickFxMenuOpen;
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
                        if (prebuiltId == 18 && isRmb) {
                            const char* bCycle[] = { "Guard", "Scientist", "Worker", "Combat Synth #1", "Combat Synth #2", "" };
                            int bIdx = 0;
                            for (int k = 0; k < 6; ++k) {
                                if (_defaultWaypointBotName == bCycle[k]) { bIdx = k; break; }
                            }
                            _defaultWaypointBotName = bCycle[(bIdx + 1) % 6];
                            logMessage("Waypoint Placement Target Bot set to: " + (_defaultWaypointBotName.empty() ? "(any bot)" : _defaultWaypointBotName));
                            return;
                        }
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

                // Camera Component Properties Interactions
                if (_selectionType == SelectionType::Camera && _selectedIndex >= 0 && _selectedIndex < (int)_map->cameras.size()) {
                    auto& cam = _map->cameras[_selectedIndex];
                    float propY = l.rightY + 38.0f;

                    // MainCamera Tag Toggle & Projection Toggle
                    float tagBtnY = propY + 70.0f;
                    if (my >= tagBtnY && my <= tagBtnY + 24.0f) {
                        if (mx >= l.rightX + 10.0f && mx <= l.rightX + 140.0f) {
                            cam.active = !cam.active;
                            for (size_t ci = 0; ci < _map->cameras.size(); ++ci) {
                                if ((int)ci != _selectedIndex && cam.active) _map->cameras[ci].active = false;
                            }
                            logMessage(std::string("Camera '") + cam.name + "' MainCamera Tag: " + (cam.active ? "ENABLED" : "DISABLED"));
                            return;
                        }
                        if (mx >= l.rightX + 150.0f && mx <= l.rightX + 285.0f) {
                            cam.orthographic = !cam.orthographic;
                            logMessage(std::string("Camera Projection: ") + (cam.orthographic ? "Orthographic" : "Perspective"));
                            return;
                        }
                    }

                    // FOV / Ortho adjusters
                    float fovBtnY = tagBtnY + 44.0f;
                    if (my >= fovBtnY && my <= fovBtnY + 24.0f) {
                        if (mx >= l.rightX + 10.0f && mx <= l.rightX + 60.0f) { cam.fov = std::clamp(cam.fov - 5.0f, 20.0f, 130.0f); cam.orthoSize = std::max(1.0f, cam.orthoSize - 1.0f); return; }
                        if (mx >= l.rightX + 65.0f && mx <= l.rightX + 115.0f) { cam.fov = 60.0f; cam.orthoSize = 5.0f; return; }
                        if (mx >= l.rightX + 120.0f && mx <= l.rightX + 170.0f) { cam.fov = 75.0f; cam.orthoSize = 8.0f; return; }
                        if (mx >= l.rightX + 175.0f && mx <= l.rightX + 225.0f) { cam.fov = 90.0f; cam.orthoSize = 10.0f; return; }
                        if (mx >= l.rightX + 230.0f && mx <= l.rightX + 280.0f) { cam.fov = std::clamp(cam.fov + 5.0f, 20.0f, 130.0f); cam.orthoSize += 1.0f; return; }
                    }

                    // Clipping planes: Near & Far
                    float clipBtnY = fovBtnY + 44.0f;
                    if (my >= clipBtnY && my <= clipBtnY + 24.0f) {
                        if (mx >= l.rightX + 10.0f && mx <= l.rightX + 70.0f) { cam.nearClip = std::max(0.01f, cam.nearClip - 0.1f); return; }
                        if (mx >= l.rightX + 75.0f && mx <= l.rightX + 135.0f) { cam.nearClip += 0.1f; return; }
                        if (mx >= l.rightX + 155.0f && mx <= l.rightX + 215.0f) { cam.farClip = std::max(50.0f, cam.farClip - 100.0f); return; }
                        if (mx >= l.rightX + 220.0f && mx <= l.rightX + 280.0f) { cam.farClip += 100.0f; return; }
                    }

                    // Clear Flags: SolidColor, Skybox, DepthOnly, Nothing
                    float clearBtnY = clipBtnY + 44.0f;
                    if (my >= clearBtnY && my <= clearBtnY + 24.0f) {
                        float cW = 66.0f;
                        if (mx >= l.rightX + 10.0f && mx <= l.rightX + 10.0f + cW) { cam.clearFlags = "SolidColor"; return; }
                        if (mx >= l.rightX + 80.0f && mx <= l.rightX + 80.0f + cW) { cam.clearFlags = "Skybox"; return; }
                        if (mx >= l.rightX + 150.0f && mx <= l.rightX + 150.0f + cW) { cam.clearFlags = "DepthOnly"; return; }
                        if (mx >= l.rightX + 220.0f && mx <= l.rightX + 220.0f + cW) { cam.clearFlags = "Nothing"; return; }
                    }

                    // Depth & Viewport Rect
                    float depthBtnY = clearBtnY + 44.0f;
                    if (my >= depthBtnY && my <= depthBtnY + 24.0f) {
                        if (mx >= l.rightX + 10.0f && mx <= l.rightX + 60.0f) { cam.depth = std::max(0, cam.depth - 1); return; }
                        if (mx >= l.rightX + 65.0f && mx <= l.rightX + 115.0f) { cam.depth += 1; return; }
                        if (mx >= l.rightX + 125.0f && mx <= l.rightX + 205.0f) { cam.viewportRect = Vec4(0, 0, 1, 1); logMessage("Camera Rect: Fullscreen (0, 0, 1, 1)"); return; }
                        if (mx >= l.rightX + 210.0f && mx <= l.rightX + 290.0f) { cam.viewportRect = Vec4(0.7f, 0.7f, 0.3f, 0.3f); logMessage("Camera Rect: Minimap (0.7, 0.7, 0.3, 0.3)"); return; }
                    }

                    // Background Color Presets
                    float bgBtnY = depthBtnY + 44.0f;
                    if (my >= bgBtnY && my <= bgBtnY + 24.0f) {
                        if (mx >= l.rightX + 38.0f && mx <= l.rightX + 94.0f) {
                            cam.backgroundColor = Vec4(0.0f, 0.0f, 0.0f, 1.0f);
                            logMessage("Camera Background: Solid Black");
                            return;
                        }
                        if (mx >= l.rightX + 98.0f && mx <= l.rightX + 156.0f) {
                            cam.backgroundColor = Vec4(0.05f, 0.08f, 0.22f, 1.0f);
                            logMessage("Camera Background: Deep Navy");
                            return;
                        }
                        if (mx >= l.rightX + 160.0f && mx <= l.rightX + 218.0f) {
                            cam.backgroundColor = Vec4(0.12f, 0.14f, 0.18f, 1.0f);
                            logMessage("Camera Background: Dark Slate");
                            return;
                        }
                        if (mx >= l.rightX + 222.0f && mx <= l.rightX + 284.0f) {
                            cam.backgroundColor = Vec4(0.45f, 0.65f, 0.90f, 1.0f);
                            logMessage("Camera Background: Sky Fog");
                            return;
                        }
                    }

                    // Culling Mask & Target Texture
                    float maskBtnY = bgBtnY + 44.0f;
                    if (my >= maskBtnY && my <= maskBtnY + 24.0f) {
                        if (mx >= l.rightX + 10.0f && mx <= l.rightX + 92.0f) {
                            cam.cullingMask = 0xFFFFFFFF;
                            logMessage("Camera Culling Mask: All Layers (0xFFFFFFFF)");
                            return;
                        }
                        if (mx >= l.rightX + 96.0f && mx <= l.rightX + 178.0f) {
                            cam.cullingMask = 0x1;
                            logMessage("Camera Culling Mask: Default Layer Only (0x1)");
                            return;
                        }
                        if (mx >= l.rightX + 182.0f && mx <= l.rightX + 284.0f) {
                            cam.targetTexture = (cam.targetTexture == "Screen" ? "FBO_Minimap" : "Screen");
                            logMessage("Camera Target Texture: " + cam.targetTexture);
                            return;
                        }
                    }
                }

                // Trigger Volume Properties Interactions
                if (_selectionType == SelectionType::Trigger && _selectedIndex >= 0 && _selectedIndex < (int)_map->triggers.size()) {
                    auto& tr = _map->triggers[_selectedIndex];
                    float trPropY = l.rightY + 38.0f;
                    float trEventY = trPropY + 76.0f;
                    float trTargetY = trEventY + 50.0f;
                    float trActionY = trTargetY + 26.0f;
                    float trWpY = trActionY + 50.0f;
                    float trBotY1 = trWpY + 48.0f;
                    float trBotY2 = trBotY1 + 22.0f;

                    // Section 1: Event Type: [Dialog] [Open Door] [Unlock] [Waypoint] [Script]
                    if (my >= trEventY && my <= trEventY + 22.0f) {
                        const char* evs[5] = { "story_dialog", "door_open", "door_unlock", "waypoint_advance", "script" };
                        float xs[5] = { l.rightX + 10.0f, l.rightX + 64.0f, l.rightX + 140.0f, l.rightX + 196.0f, l.rightX + 264.0f };
                        float ws[5] = { 52.0f, 74.0f, 54.0f, 66.0f, 40.0f };
                        for (int k = 0; k < 5; ++k) {
                            if (mx >= xs[k] && mx <= xs[k] + ws[k]) {
                                recordSnapshot("Change Trigger Event");
                                tr.eventType = evs[k];
                                logMessage("Set Trigger #" + std::to_string(_selectedIndex) + " Event: " + evs[k]);
                                return;
                            }
                        }
                    }

                    // Section 2: Target Door / Entity: [Cycle Door] [Clear Target] [Once: YES/NO]
                    if (my >= trTargetY && my <= trTargetY + 22.0f) {
                        if (mx >= l.rightX + 10.0f && mx <= l.rightX + 140.0f) {
                            recordSnapshot("Cycle Target Entity");
                            if (_map->doors.empty()) {
                                logMessage("No doors in map to target.");
                            } else {
                                int curDoor = -1;
                                for (size_t d = 0; d < _map->doors.size(); ++d) {
                                    if (_map->doors[d].name == tr.targetEntity) { curDoor = (int)d; break; }
                                }
                                curDoor++;
                                if (curDoor >= (int)_map->doors.size()) tr.targetEntity = "";
                                else tr.targetEntity = _map->doors[curDoor].name;
                                logMessage("Trigger #" + std::to_string(_selectedIndex) + " Target Door: " + (tr.targetEntity.empty() ? "(none)" : tr.targetEntity));
                            }
                            return;
                        } else if (mx >= l.rightX + 144.0f && mx <= l.rightX + 214.0f) {
                            recordSnapshot("Clear Target Entity");
                            tr.targetEntity = "";
                            logMessage("Cleared Trigger #" + std::to_string(_selectedIndex) + " Target Entity");
                            return;
                        } else if (mx >= l.rightX + 218.0f && mx <= l.rightX + 294.0f) {
                            recordSnapshot("Toggle Trigger Once");
                            tr.once = !tr.once;
                            logMessage("Trigger Once Mode: " + std::string(tr.once ? "ON (Once)" : "OFF (Repeatable)"));
                            return;
                        }
                    }

                    // Section 2b: Target Action: [Open] [Close] [Toggle] [Unlock] [Lock]
                    if (my >= trActionY && my <= trActionY + 22.0f) {
                        const char* acts[5] = { "open", "close", "toggle", "unlock", "lock" };
                        float xs[5] = { l.rightX + 10.0f, l.rightX + 66.0f, l.rightX + 122.0f, l.rightX + 178.0f, l.rightX + 240.0f };
                        float ws[5] = { 54.0f, 54.0f, 54.0f, 60.0f, 54.0f };
                        for (int k = 0; k < 5; ++k) {
                            if (mx >= xs[k] && mx <= xs[k] + ws[k]) {
                                recordSnapshot("Set Target Action");
                                tr.targetAction = acts[k];
                                logMessage("Set Trigger Target Action: " + std::string(acts[k]));
                                return;
                            }
                        }
                    }

                    // Section 3: Target Waypoint: [Cycle Waypoint] [Clear WP]
                    if (my >= trWpY && my <= trWpY + 22.0f) {
                        if (mx >= l.rightX + 10.0f && mx <= l.rightX + 160.0f) {
                            recordSnapshot("Cycle Target Waypoint");
                            if (_map->waypoints.empty()) {
                                logMessage("No waypoints in map to target.");
                            } else {
                                int curWp = -1;
                                for (size_t wpIdx = 0; wpIdx < _map->waypoints.size(); ++wpIdx) {
                                    if (_map->waypoints[wpIdx].name == tr.targetWaypoint) { curWp = (int)wpIdx; break; }
                                }
                                curWp++;
                                if (curWp >= (int)_map->waypoints.size()) tr.targetWaypoint = "";
                                else tr.targetWaypoint = _map->waypoints[curWp].name;
                                logMessage("Trigger #" + std::to_string(_selectedIndex) + " Target Waypoint: " + (tr.targetWaypoint.empty() ? "(none)" : tr.targetWaypoint));
                            }
                            return;
                        } else if (mx >= l.rightX + 165.0f && mx <= l.rightX + 285.0f) {
                            recordSnapshot("Clear Target Waypoint");
                            tr.targetWaypoint = "";
                            logMessage("Cleared Trigger #" + std::to_string(_selectedIndex) + " Target Waypoint");
                            return;
                        }
                    }

                    // Section 4: Target Bot: Row 1 [Any Bot] [Guard] [Scientist]
                    if (my >= trBotY1 && my <= trBotY1 + 20.0f) {
                        float bBtnW1 = 88.0f, bBtnW2 = 88.0f, bBtnW3 = 94.0f;
                        float bx1 = l.rightX + 10.0f, bx2 = l.rightX + 103.0f, bx3 = l.rightX + 196.0f;
                        if (mx >= bx1 && mx <= bx1 + bBtnW1) {
                            recordSnapshot("Set Trigger Bot");
                            tr.targetBot = "";
                            logMessage("Set Trigger Target Bot: (Any Bot)");
                            return;
                        } else if (mx >= bx2 && mx <= bx2 + bBtnW2) {
                            recordSnapshot("Set Trigger Bot");
                            tr.targetBot = "Guard";
                            logMessage("Set Trigger Target Bot: Guard");
                            return;
                        } else if (mx >= bx3 && mx <= bx3 + bBtnW3) {
                            recordSnapshot("Set Trigger Bot");
                            tr.targetBot = "Scientist";
                            logMessage("Set Trigger Target Bot: Scientist");
                            return;
                        }
                    }

                    // Section 4: Target Bot: Row 2 [Worker] [Synth #1] [Synth #2]
                    if (my >= trBotY2 && my <= trBotY2 + 20.0f) {
                        float bBtnW1 = 88.0f, bBtnW2 = 88.0f, bBtnW3 = 94.0f;
                        float bx1 = l.rightX + 10.0f, bx2 = l.rightX + 103.0f, bx3 = l.rightX + 196.0f;
                        if (mx >= bx1 && mx <= bx1 + bBtnW1) {
                            recordSnapshot("Set Trigger Bot");
                            tr.targetBot = "Worker";
                            logMessage("Set Trigger Target Bot: Worker");
                            return;
                        } else if (mx >= bx2 && mx <= bx2 + bBtnW2) {
                            recordSnapshot("Set Trigger Bot");
                            tr.targetBot = "Combat Synth #1";
                            logMessage("Set Trigger Target Bot: Combat Synth #1");
                            return;
                        } else if (mx >= bx3 && mx <= bx3 + bBtnW3) {
                            recordSnapshot("Set Trigger Bot");
                            tr.targetBot = "Combat Synth #2";
                            logMessage("Set Trigger Target Bot: Combat Synth #2");
                            return;
                        }
                    }
                }

                // Door Properties Interactions
                if (_selectionType == SelectionType::Door && _selectedIndex >= 0 && _selectedIndex < (int)_map->doors.size()) {
                    auto& d = _map->doors[_selectedIndex];
                    float doorPropY = l.rightY + 38.0f;
                    float dActY = doorPropY + 76.0f;
                    float dKeyY = dActY + 50.0f;
                    float dDepY = dKeyY + 50.0f;

                    // Section 1: Activator Mode [Proximity] [Trigger] [Manual]
                    if (my >= dActY && my <= dActY + 22.0f) {
                        if (mx >= l.rightX + 10.0f && mx <= l.rightX + 98.0f) {
                            recordSnapshot("Set Door Activator");
                            d.activator = "proximity";
                            logMessage("Set Door Activator: Proximity");
                            return;
                        } else if (mx >= l.rightX + 103.0f && mx <= l.rightX + 191.0f) {
                            recordSnapshot("Set Door Activator");
                            d.activator = "trigger_only";
                            logMessage("Set Door Activator: Trigger Only");
                            return;
                        } else if (mx >= l.rightX + 196.0f && mx <= l.rightX + 290.0f) {
                            recordSnapshot("Set Door Activator");
                            d.activator = "manual";
                            logMessage("Set Door Activator: Manual / Use");
                            return;
                        }
                    }

                    // Section 2: Keycard & Lock [No Keycard] [Keycard L1] [Keycard L2] [Lock]
                    if (my >= dKeyY && my <= dKeyY + 22.0f) {
                        float xs[4] = { l.rightX + 10.0f, l.rightX + 80.0f, l.rightX + 155.0f, l.rightX + 230.0f };
                        float ws[4] = { 68.0f, 72.0f, 72.0f, 60.0f };
                        if (mx >= xs[0] && mx <= xs[0] + ws[0]) {
                            recordSnapshot("Set Door Keycard");
                            d.requiresKeycard = false;
                            d.keycardRequired = "";
                            logMessage("Removed Door Keycard requirement");
                            return;
                        } else if (mx >= xs[1] && mx <= xs[1] + ws[1]) {
                            recordSnapshot("Set Door Keycard");
                            d.requiresKeycard = true;
                            d.keycardRequired = "keycard_l1";
                            logMessage("Door requires Keycard Level 1");
                            return;
                        } else if (mx >= xs[2] && mx <= xs[2] + ws[2]) {
                            recordSnapshot("Set Door Keycard");
                            d.requiresKeycard = true;
                            d.keycardRequired = "keycard_l2";
                            logMessage("Door requires Keycard Level 2");
                            return;
                        } else if (mx >= xs[3] && mx <= xs[3] + ws[3]) {
                            recordSnapshot("Toggle Door Lock");
                            d.isLocked = !d.isLocked;
                            if (d.isLocked) d.isOpen = false;
                            logMessage("Door Lock: " + std::string(d.isLocked ? "LOCKED" : "UNLOCKED"));
                            return;
                        }
                    }

                    // Section 3: Dependency & Mechanism [Cycle Dependency] [Clear Dep] [AutoClose]
                    if (my >= dDepY && my <= dDepY + 22.0f) {
                        if (mx >= l.rightX + 10.0f && mx <= l.rightX + 130.0f) {
                            recordSnapshot("Cycle Door Dependency");
                            if (_map->triggers.empty()) {
                                logMessage("No triggers in map for dependency.");
                            } else {
                                int curTr = -1;
                                for (size_t t = 0; t < _map->triggers.size(); ++t) {
                                    if (_map->triggers[t].name == d.dependency) { curTr = (int)t; break; }
                                }
                                curTr++;
                                if (curTr >= (int)_map->triggers.size()) d.dependency = "";
                                else d.dependency = _map->triggers[curTr].name;
                                logMessage("Door Dependency: " + (d.dependency.empty() ? "(none)" : d.dependency));
                            }
                            return;
                        } else if (mx >= l.rightX + 134.0f && mx <= l.rightX + 204.0f) {
                            recordSnapshot("Clear Door Dependency");
                            d.dependency = "";
                            logMessage("Cleared Door Dependency");
                            return;
                        } else if (mx >= l.rightX + 208.0f && mx <= l.rightX + 294.0f) {
                            recordSnapshot("Toggle AutoClose");
                            d.autoClose = !d.autoClose;
                            logMessage("Door AutoClose: " + std::string(d.autoClose ? "ON" : "OFF"));
                            return;
                        }
                    }
                }

                // Bot Waypoint Properties Interactions
                if (_selectionType == SelectionType::Waypoint && _selectedIndex >= 0 && _selectedIndex < (int)_map->waypoints.size()) {
                    auto& wp = _map->waypoints[_selectedIndex];
                    float wpPropY = l.rightY + 38.0f;
                    float wpTrigBtnY = wpPropY + 54.0f;

                    // Linked Trigger Click Handler: [Cycle Trig] [Clear]
                    if (my >= wpTrigBtnY && my <= wpTrigBtnY + 20.0f) {
                        if (mx >= l.rightX + 155.0f && mx <= l.rightX + 235.0f) {
                            recordSnapshot("Cycle Linked Trigger");
                            if (_map->triggers.empty()) {
                                logMessage("No triggers in map to link.");
                            } else {
                                int curTrig = -1;
                                for (size_t t = 0; t < _map->triggers.size(); ++t) {
                                    if (_map->triggers[t].name == wp.targetTrigger) { curTrig = (int)t; break; }
                                }
                                curTrig++;
                                if (curTrig >= (int)_map->triggers.size()) wp.targetTrigger = "";
                                else wp.targetTrigger = _map->triggers[curTrig].name;
                                logMessage("Waypoint #" + std::to_string(_selectedIndex) + " Linked Trigger: " + (wp.targetTrigger.empty() ? "(none)" : wp.targetTrigger));
                            }
                            return;
                        } else if (mx >= l.rightX + 240.0f && mx <= l.rightX + 292.0f) {
                            recordSnapshot("Clear Linked Trigger");
                            wp.targetTrigger = "";
                            logMessage("Cleared Waypoint #" + std::to_string(_selectedIndex) + " Linked Trigger");
                            return;
                        }
                    }

                    float wpAnimBoxY = wpPropY + 86.0f;
                    float wpAnimBtn1Y = wpAnimBoxY + 28.0f;
                    float wpAnimBtn2Y = wpAnimBtn1Y + 26.0f;
                    float wpYawBtnY = wpAnimBtn2Y + 48.0f;
                    float wpDurBtnY = wpYawBtnY + 46.0f;

                    // Animation Row 1: [Idle] [Walk] [Talk] [Attack]
                    if (my >= wpAnimBtn1Y && my <= wpAnimBtn1Y + 22.0f) {
                        float aBtnW = 66.0f;
                        const char* anims[4] = { "Idle", "Walk", "Talk", "Attack" };
                        float xs[4] = { l.rightX + 10.0f, l.rightX + 81.0f, l.rightX + 152.0f, l.rightX + 223.0f };
                        for (int k = 0; k < 4; ++k) {
                            if (mx >= xs[k] && mx <= xs[k] + aBtnW) {
                                wp.animation = anims[k];
                                _defaultWaypointAnimation = anims[k];
                                logMessage("Set Bot Waypoint #" + std::to_string(_selectedIndex) + " Animation: " + anims[k]);
                                return;
                            }
                        }
                    }

                    // Animation Row 2: [Inspect] [Crouch] [Salute] [Death]
                    if (my >= wpAnimBtn2Y && my <= wpAnimBtn2Y + 22.0f) {
                        float aBtnW = 66.0f;
                        const char* anims[4] = { "Inspect", "Crouch", "Salute", "Death" };
                        float xs[4] = { l.rightX + 10.0f, l.rightX + 81.0f, l.rightX + 152.0f, l.rightX + 223.0f };
                        for (int k = 0; k < 4; ++k) {
                            if (mx >= xs[k] && mx <= xs[k] + aBtnW) {
                                wp.animation = anims[k];
                                _defaultWaypointAnimation = anims[k];
                                logMessage("Set Bot Waypoint #" + std::to_string(_selectedIndex) + " Animation: " + anims[k]);
                                return;
                            }
                        }
                    }

                    // Facing Yaw Buttons: [-15] [0] [90] [180] [270] [+15]
                    if (my >= wpYawBtnY && my <= wpYawBtnY + 22.0f) {
                        float yBtnW = 44.0f;
                        float xs[6] = { l.rightX + 10.0f, l.rightX + 57.0f, l.rightX + 104.0f, l.rightX + 151.0f, l.rightX + 198.0f, l.rightX + 245.0f };
                        for (int k = 0; k < 6; ++k) {
                            if (mx >= xs[k] && mx <= xs[k] + yBtnW) {
                                if (k == 0) wp.facingYaw = std::fmod(wp.facingYaw - 15.0f + 360.0f, 360.0f);
                                else if (k == 1) wp.facingYaw = 0.0f;
                                else if (k == 2) wp.facingYaw = 90.0f;
                                else if (k == 3) wp.facingYaw = 180.0f;
                                else if (k == 4) wp.facingYaw = 270.0f;
                                else if (k == 5) wp.facingYaw = std::fmod(wp.facingYaw + 15.0f, 360.0f);
                                _defaultWaypointYaw = wp.facingYaw;
                                logMessage("Set Bot Waypoint #" + std::to_string(_selectedIndex) + " Facing Yaw: " + std::to_string((int)wp.facingYaw) + "°");
                                return;
                            }
                        }
                    }

                    // Duration Buttons: [0s] [1s] [2s] [5s] [10s]
                    if (my >= wpDurBtnY && my <= wpDurBtnY + 22.0f) {
                        float wBtnW = 52.0f;
                        float durs[5] = { 0.0f, 1.0f, 2.0f, 5.0f, 10.0f };
                        float xs[5] = { l.rightX + 10.0f, l.rightX + 67.0f, l.rightX + 124.0f, l.rightX + 181.0f, l.rightX + 238.0f };
                        for (int k = 0; k < 5; ++k) {
                            if (mx >= xs[k] && mx <= xs[k] + wBtnW) {
                                wp.waitDuration = durs[k];
                                _defaultWaypointDuration = durs[k];
                                logMessage("Set Bot Waypoint #" + std::to_string(_selectedIndex) + " Action Duration: " + std::to_string((int)durs[k]) + "s");
                                return;
                            }
                        }
                    }

                    // Target Bot Buttons:
                    float wpBotLabelY = wpDurBtnY + 30.0f;
                    float wpBotBtn1Y = wpBotLabelY + 16.0f;
                    float wpBotBtn2Y = wpBotBtn1Y + 22.0f;

                    // Row 1: [Any Bot] [Guard] [Scientist]
                    if (my >= wpBotBtn1Y && my <= wpBotBtn1Y + 20.0f) {
                        float bBtnW1 = 88.0f, bBtnW2 = 88.0f, bBtnW3 = 94.0f;
                        float bx1 = l.rightX + 10.0f, bx2 = l.rightX + 103.0f, bx3 = l.rightX + 196.0f;
                        if (mx >= bx1 && mx <= bx1 + bBtnW1) {
                            recordSnapshot("Assign Waypoint Bot");
                            wp.botName = "";
                            _defaultWaypointBotName = "";
                            logMessage("Assigned Bot Waypoint #" + std::to_string(_selectedIndex) + " to: (any bot)");
                            return;
                        } else if (mx >= bx2 && mx <= bx2 + bBtnW2) {
                            recordSnapshot("Assign Waypoint Bot");
                            wp.botName = "Guard";
                            _defaultWaypointBotName = "Guard";
                            logMessage("Assigned Bot Waypoint #" + std::to_string(_selectedIndex) + " to: Guard");
                            return;
                        } else if (mx >= bx3 && mx <= bx3 + bBtnW3) {
                            recordSnapshot("Assign Waypoint Bot");
                            wp.botName = "Scientist";
                            _defaultWaypointBotName = "Scientist";
                            logMessage("Assigned Bot Waypoint #" + std::to_string(_selectedIndex) + " to: Scientist");
                            return;
                        }
                    }

                    // Row 2: [Worker] [Synth #1] [Synth #2]
                    if (my >= wpBotBtn2Y && my <= wpBotBtn2Y + 20.0f) {
                        float bBtnW1 = 88.0f, bBtnW2 = 88.0f, bBtnW3 = 94.0f;
                        float bx1 = l.rightX + 10.0f, bx2 = l.rightX + 103.0f, bx3 = l.rightX + 196.0f;
                        if (mx >= bx1 && mx <= bx1 + bBtnW1) {
                            recordSnapshot("Assign Waypoint Bot");
                            wp.botName = "Worker";
                            _defaultWaypointBotName = "Worker";
                            logMessage("Assigned Bot Waypoint #" + std::to_string(_selectedIndex) + " to: Worker");
                            return;
                        } else if (mx >= bx2 && mx <= bx2 + bBtnW2) {
                            recordSnapshot("Assign Waypoint Bot");
                            wp.botName = "Combat Synth #1";
                            _defaultWaypointBotName = "Combat Synth #1";
                            logMessage("Assigned Bot Waypoint #" + std::to_string(_selectedIndex) + " to: Combat Synth #1");
                            return;
                        } else if (mx >= bx3 && mx <= bx3 + bBtnW3) {
                            recordSnapshot("Assign Waypoint Bot");
                            wp.botName = "Combat Synth #2";
                            _defaultWaypointBotName = "Combat Synth #2";
                            logMessage("Assigned Bot Waypoint #" + std::to_string(_selectedIndex) + " to: Combat Synth #2");
                            return;
                        }
                    }

                    // Position Drag Row
                    for (int axis = 0; axis < 3; ++axis) {
                        float fx, fy, fw, fh;
                        dragFieldRect(l, 0, axis, fx, fy, fw, fh);
                        if (mx >= fx && mx <= fx + fw && my >= fy && my <= fy + fh) {
                            _dragRow = 0; _dragAxis = axis;
                            _dragLastX = mx; _dragPressX = mx; _dragCenterX = fx + fw * 0.5f; _dragAccum = 0.0f; _dragMoved = false;
                            return;
                        }
                    }

                    // Elevation Buttons [ ^ UP ] [ v DOWN ] [ Floor ]
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

                    // Deselect button
                    if (mx >= l.deselX && mx <= l.deselX + l.deselW && my >= l.deselY && my <= l.deselY + l.deselH) {
                        _selectionType = SelectionType::None;
                        _selectedIndex = -1;
                        logMessage("Deselected all");
                        return;
                    }

                    // Delete button
                    if (mx >= l.delX && mx <= l.delX + l.delW && my >= l.delY && my <= l.delY + l.delH) {
                        deleteSelection();
                        return;
                    }
                    return;
                }

                // Entity Name & Script Assignment buttons (placed below delete button)
                float actBtnY = l.delY + l.delH + 10.0f;
                if (my >= actBtnY && my <= actBtnY + 24.0f) {
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

                // If selection is Prop, handle Prop-specific controls (Model picker, ECS Components)
                if (_selectionType == SelectionType::Prop && _selectedIndex >= 0 && _selectedIndex < (int)_map->props.size()) {
                    auto& p = _map->props[_selectedIndex];
                    float propY2 = l.rightY + 38.0f;
                    float mBoxY = propY2 + 112.0f;

                    // Browse 3D Models
                    if (mx >= l.rightX + 10.0f && mx <= l.rightX + 290.0f && my >= mBoxY + 26.0f && my <= mBoxY + 50.0f) {
                        _modelBrowserOpen = true;
                        return;
                    }

                    // Quick Add Component Row 1
                    float compY = mBoxY + 58.0f;
                    float addBtnY = compY + 18.0f;
                    if (my >= addBtnY && my <= addBtnY + 22.0f) {
                        if (mx >= l.rightX + 10.0f && mx <= l.rightX + 75.0f) {
                            p.hasAudio = !p.hasAudio;
                            logMessage(p.hasAudio ? "Added AudioSourceComponent to Prop" : "Removed AudioSourceComponent from Prop");
                            return;
                        }
                        if (mx >= l.rightX + 80.0f && mx <= l.rightX + 145.0f) {
                            p.hasPhysics = !p.hasPhysics;
                            logMessage(p.hasPhysics ? "Added RigidBodyComponent to Prop" : "Removed RigidBodyComponent from Prop");
                            return;
                        }
                        if (mx >= l.rightX + 150.0f && mx <= l.rightX + 215.0f) {
                            p.hasCloth = !p.hasCloth;
                            logMessage(p.hasCloth ? "Added ClothComponent to Prop" : "Removed ClothComponent from Prop");
                            return;
                        }
                        if (mx >= l.rightX + 220.0f && mx <= l.rightX + 290.0f) {
                            p.hasWaterVolume = !p.hasWaterVolume;
                            logMessage(p.hasWaterVolume ? "Added WaterVolumeComponent to Prop" : "Removed WaterVolumeComponent from Prop");
                            return;
                        }
                    }

                    // Quick Add Component Row 2
                    float addBtn2Y = addBtnY + 26.0f;
                    if (my >= addBtn2Y && my <= addBtn2Y + 22.0f) {
                        if (mx >= l.rightX + 10.0f && mx <= l.rightX + 98.0f) {
                            p.hasForceField = !p.hasForceField;
                            logMessage(p.hasForceField ? "Added ForceFieldComponent to Prop" : "Removed ForceFieldComponent from Prop");
                            return;
                        }
                        if (mx >= l.rightX + 104.0f && mx <= l.rightX + 192.0f) {
                            p.hasDecal = !p.hasDecal;
                            logMessage(p.hasDecal ? "Added DecalComponent to Prop" : "Removed DecalComponent from Prop");
                            return;
                        }
                        if (mx >= l.rightX + 198.0f && mx <= l.rightX + 290.0f) {
                            p.hasPostProcess = !p.hasPostProcess;
                            logMessage(p.hasPostProcess ? "Added PostProcessComponent to Prop" : "Removed PostProcessComponent from Prop");
                            return;
                        }
                    }

                    // Active Component Mini-Panels
                    float cardY = addBtn2Y + 28.0f;
                    if (p.hasAudio) {
                        float aBtnY = cardY + 16.0f;
                        if (my >= aBtnY && my <= aBtnY + 20.0f) {
                            if (mx >= l.rightX + 10.0f && mx <= l.rightX + 58.0f) {
                                p.audioVolume = std::max(0.0f, p.audioVolume - 0.2f);
                                logMessage("Audio Volume: " + std::to_string(p.audioVolume));
                                return;
                            }
                            if (mx >= l.rightX + 62.0f && mx <= l.rightX + 110.0f) {
                                p.audioVolume = std::min(2.0f, p.audioVolume + 0.2f);
                                logMessage("Audio Volume: " + std::to_string(p.audioVolume));
                                return;
                            }
                            if (mx >= l.rightX + 114.0f && mx <= l.rightX + 166.0f) {
                                p.audioLoop = !p.audioLoop;
                                logMessage(p.audioLoop ? "Audio Loop: ON" : "Audio Loop: OFF");
                                return;
                            }
                            if (mx >= l.rightX + 170.0f && mx <= l.rightX + 238.0f) {
                                if (p.audioSound.empty() || p.audioSound == "assets/audio/shoot.wav" || p.audioSound == "shoot.wav") {
                                    AudioEngine::playSound3D(SoundID::PistolShot, p.position, p.audioVolume, p.audioPitch);
                                } else {
                                    AudioEngine::playCustomSound3D(p.audioSound, p.position, p.audioVolume);
                                }
                                logMessage("Auditioned 3D sound at entity position");
                                return;
                            }
                            if (mx >= l.rightX + 242.0f && mx <= l.rightX + 290.0f) {
                                p.hasAudio = false;
                                logMessage("Removed AudioSourceComponent");
                                return;
                            }
                        }
                        cardY += 42.0f;
                    }
                    if (p.hasPhysics) {
                        float phBtnY = cardY + 16.0f;
                        if (my >= phBtnY && my <= phBtnY + 20.0f) {
                            if (mx >= l.rightX + 10.0f && mx <= l.rightX + 64.0f) {
                                p.physicsMass = std::max(1.0f, p.physicsMass - 5.0f);
                                logMessage("RigidBody Mass: " + std::to_string((int)p.physicsMass) + " kg");
                                return;
                            }
                            if (mx >= l.rightX + 68.0f && mx <= l.rightX + 122.0f) {
                                p.physicsMass += 5.0f;
                                logMessage("RigidBody Mass: " + std::to_string((int)p.physicsMass) + " kg");
                                return;
                            }
                            if (mx >= l.rightX + 126.0f && mx <= l.rightX + 186.0f) {
                                p.physicsDynamic = !p.physicsDynamic;
                                logMessage(p.physicsDynamic ? "RigidBody: Dynamic Mode" : "RigidBody: Static Mode");
                                return;
                            }
                            if (mx >= l.rightX + 190.0f && mx <= l.rightX + 236.0f) {
                                p.physicsRestitution = (p.physicsRestitution >= 0.8f) ? 0.1f : (p.physicsRestitution + 0.3f);
                                logMessage("RigidBody Restitution (Bounce): " + std::to_string(p.physicsRestitution));
                                return;
                            }
                            if (mx >= l.rightX + 242.0f && mx <= l.rightX + 290.0f) {
                                p.hasPhysics = false;
                                logMessage("Removed RigidBodyComponent");
                                return;
                            }
                        }
                        cardY += 42.0f;
                    }
                    if (p.hasCloth) {
                        float clBtnY = cardY + 16.0f;
                        if (my >= clBtnY && my <= clBtnY + 20.0f) {
                            if (mx >= l.rightX + 10.0f && mx <= l.rightX + 68.0f) {
                                p.clothStiffness = std::max(0.1f, p.clothStiffness - 0.1f);
                                logMessage("Cloth Stiffness: " + std::to_string(p.clothStiffness));
                                return;
                            }
                            if (mx >= l.rightX + 72.0f && mx <= l.rightX + 130.0f) {
                                p.clothStiffness = std::min(1.0f, p.clothStiffness + 0.1f);
                                logMessage("Cloth Stiffness: " + std::to_string(p.clothStiffness));
                                return;
                            }
                            if (mx >= l.rightX + 242.0f && mx <= l.rightX + 290.0f) {
                                p.hasCloth = false;
                                logMessage("Removed ClothComponent");
                                return;
                            }
                        }
                        cardY += 42.0f;
                    }
                    if (p.hasWaterVolume) {
                        float wBtnY = cardY + 16.0f;
                        if (my >= wBtnY && my <= wBtnY + 20.0f) {
                            if (mx >= l.rightX + 10.0f && mx <= l.rightX + 100.0f) {
                                p.waterDensity = (p.waterDensity > 1010.0f) ? 1000.0f : 1030.0f;
                                logMessage("Water Density: " + std::to_string((int)p.waterDensity) + " kg/m3");
                                return;
                            }
                            if (mx >= l.rightX + 242.0f && mx <= l.rightX + 290.0f) {
                                p.hasWaterVolume = false;
                                logMessage("Removed WaterVolumeComponent");
                                return;
                            }
                        }
                        cardY += 42.0f;
                    }
                    if (p.hasForceField) {
                        float fBtnY = cardY + 16.0f;
                        if (my >= fBtnY && my <= fBtnY + 20.0f) {
                            if (mx >= l.rightX + 10.0f && mx <= l.rightX + 66.0f) {
                                p.forceFieldType = (p.forceFieldType + 1) % 3;
                                logMessage("ForceField Type: " + std::to_string(p.forceFieldType));
                                return;
                            }
                            if (mx >= l.rightX + 70.0f && mx <= l.rightX + 120.0f) {
                                p.forceFieldStrength = (p.forceFieldStrength >= 500.0f) ? 100.0f : (p.forceFieldStrength + 100.0f);
                                logMessage("ForceField Strength: " + std::to_string((int)p.forceFieldStrength) + " N");
                                return;
                            }
                            if (mx >= l.rightX + 124.0f && mx <= l.rightX + 174.0f) {
                                p.forceFieldRadius = (p.forceFieldRadius >= 20.0f) ? 5.0f : (p.forceFieldRadius + 5.0f);
                                logMessage("ForceField Radius: " + std::to_string((int)p.forceFieldRadius) + " m");
                                return;
                            }
                            if (mx >= l.rightX + 178.0f && mx <= l.rightX + 238.0f) {
                                _particleSystem.spawnExplosion(p.position, p.forceFieldRadius);
                                logMessage("Triggered ForceField Shockwave pulse!");
                                return;
                            }
                            if (mx >= l.rightX + 242.0f && mx <= l.rightX + 290.0f) {
                                p.hasForceField = false;
                                logMessage("Removed ForceFieldComponent");
                                return;
                            }
                        }
                        cardY += 42.0f;
                    }
                    if (p.hasDecal) {
                        float decBtnY = cardY + 16.0f;
                        if (my >= decBtnY && my <= decBtnY + 20.0f) {
                            if (mx >= l.rightX + 10.0f && mx <= l.rightX + 120.0f) {
                                p.decalTexture = (p.decalTexture == "blood_splatter.png" ? "bullet_hole.png" : "blood_splatter.png");
                                logMessage("Decal Texture: " + p.decalTexture);
                                return;
                            }
                            if (mx >= l.rightX + 124.0f && mx <= l.rightX + 184.0f) {
                                p.decalOpacity = (p.decalOpacity >= 1.0f) ? 0.2f : (p.decalOpacity + 0.2f);
                                logMessage("Decal Opacity: " + std::to_string(p.decalOpacity));
                                return;
                            }
                            if (mx >= l.rightX + 242.0f && mx <= l.rightX + 290.0f) {
                                p.hasDecal = false;
                                logMessage("Removed DecalComponent");
                                return;
                            }
                        }
                        cardY += 42.0f;
                    }
                    if (p.hasPostProcess) {
                        float ppBtnY = cardY + 16.0f;
                        if (my >= ppBtnY && my <= ppBtnY + 20.0f) {
                            if (mx >= l.rightX + 10.0f && mx <= l.rightX + 72.0f) {
                                p.ppBloom = (p.ppBloom >= 1.0f) ? 0.0f : (p.ppBloom + 0.25f);
                                logMessage("PostProcess Bloom: " + std::to_string(p.ppBloom));
                                return;
                            }
                            if (mx >= l.rightX + 76.0f && mx <= l.rightX + 138.0f) {
                                p.ppVignette = (p.ppVignette >= 1.0f) ? 0.0f : (p.ppVignette + 0.25f);
                                logMessage("PostProcess Vignette: " + std::to_string(p.ppVignette));
                                return;
                            }
                            if (mx >= l.rightX + 242.0f && mx <= l.rightX + 290.0f) {
                                p.hasPostProcess = false;
                                logMessage("Removed PostProcessComponent");
                                return;
                            }
                        }
                        cardY += 42.0f;
                    }
                } else if (_selectionType == SelectionType::Brush || _selectionType == SelectionType::Door || _selectionType == SelectionType::None) {
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
                }

                // Blender-style drag fields (row 0 = Position, 1 = Size, 2 = Rotation)
                {
                    const bool lightOrCam = (_selectionType == SelectionType::Light || _selectionType == SelectionType::Camera);
                    for (int row = 0; row < 3; ++row) {
                        if (row > 0 && lightOrCam) break;
                        for (int axis = 0; axis < 3; ++axis) {
                            float fx, fy, fw, fh;
                            dragFieldRect(l, row, axis, fx, fy, fw, fh);
                            if (mx >= fx && mx <= fx + fw && my >= fy && my <= fy + fh) {
                                _dragRow = row; _dragAxis = axis;
                                _dragLastX = mx; _dragPressX = mx; _dragCenterX = fx + fw * 0.5f; _dragAccum = 0.0f; _dragMoved = false;
                                return;
                            }
                        }
                    }
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

        // Viewport Tab Bar Click Handling (3D Viewport, Model Viewer, Script Studio, Mods Explorer, Play)
        float vpTabX = leftW;
        float vpTabY = 58.0f;
        float tabH = 26.0f;
        if (my >= vpTabY && my <= vpTabY + tabH && mx >= vpTabX && mx <= l.rightX) {
            float tab1X = vpTabX + 6.0f;
            float tab1W = 160.0f;
            float tab2X = tab1X + tab1W + 6.0f;
            float tab2W = 170.0f;
            float tab3X = tab2X + tab2W + 6.0f;
            float tab3W = 150.0f;
            float tabModsX = tab3X + tab3W + 6.0f;
            float tabModsW = 110.0f;
            float tabRunX = tabModsX + tabModsW + 6.0f;
            float tabRunW = 110.0f;

            if (mx >= tab1X && mx <= tab1X + tab1W) {
                setWorkspace(EditorWorkspace::World3D_Level);
                return;
            } else if (mx >= tab2X && mx <= tab2X + tab2W) {
                setWorkspace(EditorWorkspace::World3D_ModelViewer);
                return;
            } else if (mx >= tab3X && mx <= tab3X + tab3W) {
                setWorkspace(EditorWorkspace::Scripting_Editor);
                return;
            } else if (mx >= tabModsX && mx <= tabModsX + tabModsW) {
                _editorDiscoveredAddons = VFS::discoverAddons("addons");
                setWorkspace(EditorWorkspace::Plugins_Manager);
                return;
            } else if (mx >= tabRunX && mx <= tabRunX + tabRunW) {
                runMapInGame();
                return;
            }
        }

        if (_workspace == EditorWorkspace::Plugins_Manager) {
            float vpX = leftW;
            float barY = 58.0f + 26.0f + 4.0f;
            // [+ New Mod] button at vpX + 420 .. vpX + 540
            if (mx >= vpX + 420.0f && mx <= vpX + 540.0f && my >= barY + 4.0f && my <= barY + 30.0f) {
                std::filesystem::path newModDir = std::filesystem::path("addons") / "custom_mod";
                std::filesystem::create_directories(newModDir / "maps");
                std::filesystem::create_directories(newModDir / "models");
                std::filesystem::create_directories(newModDir / "materials");
                std::filesystem::create_directories(newModDir / "scripts");
                std::ofstream fJson(newModDir / "addon.json");
                if (fJson.is_open()) {
                    fJson << "{\n  \"title\": \"Custom Hammer Addon\",\n  \"version\": \"1.0\",\n  \"author\": \"Level Designer\",\n  \"description\": \"Created in Hammer Mods Explorer\"\n}\n";
                }
                _editorDiscoveredAddons = VFS::discoverAddons("addons");
                logMessage("Mods Explorer: Created scaffold in /addons/custom_mod");
                return;
            }
            // [Refresh] button at vpX + 550 .. vpX + 650
            if (mx >= vpX + 550.0f && mx <= vpX + 650.0f && my >= barY + 4.0f && my <= barY + 30.0f) {
                _editorDiscoveredAddons = VFS::discoverAddons("addons");
                logMessage("Mods Explorer: Refreshed /addons/ directory (" + std::to_string(_editorDiscoveredAddons.size()) + " addons)");
                return;
            }
            // Mount / Toggle buttons on addon cards
            for (size_t i = 0; i < _editorDiscoveredAddons.size() && i < 6; ++i) {
                float cardY = barY + 46.0f + static_cast<float>(i) * 68.0f;
                if (mx >= vpX + 16.0f && mx <= vpX + 680.0f && my >= cardY && my <= cardY + 60.0f) {
                    _editorDiscoveredAddons[i].enabled = !_editorDiscoveredAddons[i].enabled;
                    std::filesystem::path p = std::filesystem::path("addons") / _editorDiscoveredAddons[i].folderName;
                    if (_editorDiscoveredAddons[i].enabled) {
                        VFS::get().mountAddonLayer(p);
                        logMessage("Mounted addon VFS layer: /addons/" + _editorDiscoveredAddons[i].folderName);
                    } else {
                        VFS::get().unmountAddonLayer(p);
                        logMessage("Unmounted addon VFS layer: /addons/" + _editorDiscoveredAddons[i].folderName);
                    }
                    return;
                }
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

        // 8.75. Package & Plugin Manager Workspace Interactions (Removed in specialized engine build)
        if (_workspace == EditorWorkspace::Plugins_Manager) {
            return;
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

            // 2. Left Panel Bottom Buttons: [ 3D World ] [ 2D Canvas ] [+ CREATE GAME] and [RESCAN DISK]
            float botBtnY = mainY + mainH - 62.0f;
            if (my >= botBtnY && my <= botBtnY + 24.0f) {
                if (mx >= listX + 130.0f && mx <= listX + 245.0f) {
                    _hubNewProjectDimension = "3D";
                    return;
                } else if (mx >= listX + 252.0f && mx <= listX + 377.0f) {
                    _hubNewProjectDimension = "2D";
                    return;
                }
            }
            float row2Y = botBtnY + 26.0f;
            if (my >= row2Y && my <= row2Y + 34.0f) {
                // [+ CREATE GAME]
                if (mx >= listX + 10.0f && mx <= listX + 225.0f) {
                    static int s_projCount = 1;
                    std::string newName = "MyGame_" + _hubNewProjectDimension + "_" + std::to_string(s_projCount++);
                    if (ProjectManager::get().createProject(newName, _hubNewProjectDimension, 1280, 720, false)) {
                        refreshProjectHub();
                        for (size_t i = 0; i < _hubProjects.size(); ++i) {
                            if (_hubProjects[i].id == newName) {
                                _hubSelectedProjectIndex = (int)i;
                                break;
                            }
                        }
                        _hubStatusBanner = "Created isolated " + _hubNewProjectDimension + " project in Documents/LabGames/" + newName;
                        _hubBannerTimer = 4.0f;
                    }
                    return;
                }
                // [RESCAN DISK]
                else if (mx >= listX + 235.0f && mx <= listX + 380.0f) {
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
                    if (proj.dimension == "2D") {
                        setWorkspace(EditorWorkspace::Level2D_Tilemap);
                        logMessage("Switched active project to 2D: " + proj.displayName + " (Level2D Tilemap Workspace)");
                    } else {
                        setWorkspace(EditorWorkspace::World3D_Level);
                        logMessage("Switched active project to 3D: " + proj.displayName + " (World3D Level Workspace)");
                    }
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
                if (mx >= detX + 20.0f && mx <= detX + 200.0f && my >= dy2 && my <= dy2 + 34.0f) {
#if defined(_WIN32)
                    std::string cmd = "explorer \"" + std::filesystem::absolute(proj.rootPath).string() + "\"";
                    std::system(cmd.c_str());
#elif defined(__APPLE__)
                    std::string cmd = "open \"" + std::filesystem::absolute(proj.rootPath).string() + "\"";
                    std::system(cmd.c_str());
#elif defined(__linux__) || defined(__unix__)
                    std::string cmd = "xdg-open \"" + std::filesystem::absolute(proj.rootPath).string() + "\" &";
                    std::system(cmd.c_str());
#endif
                    return;
                }

                // [ EXPORT / PACKAGE GAME ]
                if (mx >= detX + 210.0f && mx <= detX + 390.0f && my >= dy2 && my <= dy2 + 34.0f) {
                    _exportSettings.gameTitle = proj.id;
                    _exportSettings.entrySceneURI = proj.defaultScene.empty() ? "assets/maps/main.labmap" : proj.defaultScene;
                    _buildModalOpen = true;
                    return;
                }

                // [ PROJECT SETTINGS ]
                if (mx >= detX + 400.0f && mx <= detX + 570.0f && my >= dy2 && my <= dy2 + 34.0f) {
                    ProjectManager::get().setActiveProject(proj.id);
                    setWorkspace(EditorWorkspace::Project_Settings);
                    return;
                }
            }

            return; // Consume click so it never falls through to 3D actions
        }

        // 8.77. Project Settings Workspace Interactions
        if (_workspace == EditorWorkspace::Project_Settings) {
            float vpX = leftW;
            float vpW = l.rightX - vpX;
            float contentY = 58.0f + 26.0f;
            float pX = vpX + 24.0f;
            float pW = vpW - 48.0f;
            float headY = contentY + 4.0f;
            float headH = 50.0f;
            float curY = headY + headH + 16.0f;
            if (!_settingsStatusBanner.empty()) {
                curY += 42.0f;
            }

            // Section 1: Dimension
            if (my >= curY + 48.0f && my <= curY + 74.0f) {
                if (mx >= pX + 16.0f && mx <= pX + 236.0f) {
                    _settingsDimension = "3D";
                    return;
                } else if (mx >= pX + 246.0f && mx <= pX + 506.0f) {
                    _settingsDimension = "2D";
                    return;
                }
            }
            curY += 92.0f;

            // Section 2: Resolution Presets
            if (my >= curY + 48.0f && my <= curY + 74.0f) {
                if (mx >= pX + 16.0f && mx <= pX + 146.0f) {
                    _settingsWidth = 1280; _settingsHeight = 720; return;
                } else if (mx >= pX + 154.0f && mx <= pX + 294.0f) {
                    _settingsWidth = 1920; _settingsHeight = 1080; return;
                } else if (mx >= pX + 302.0f && mx <= pX + 432.0f) {
                    _settingsWidth = 800; _settingsHeight = 600; return;
                } else if (mx >= pX + 440.0f && mx <= pX + 580.0f) {
                    _settingsWidth = 2560; _settingsHeight = 1440; return;
                }
            }
            // Fine tuning: Width & Height
            if (my >= curY + 80.0f && my <= curY + 104.0f) {
                if (mx >= pX + 170.0f && mx <= pX + 198.0f) {
                    _settingsWidth = std::max(640, _settingsWidth - 80); return;
                } else if (mx >= pX + 204.0f && mx <= pX + 232.0f) {
                    _settingsWidth = std::min(3840, _settingsWidth + 80); return;
                } else if (mx >= pX + 410.0f && mx <= pX + 438.0f) {
                    _settingsHeight = std::max(480, _settingsHeight - 60); return;
                } else if (mx >= pX + 444.0f && mx <= pX + 472.0f) {
                    _settingsHeight = std::min(2160, _settingsHeight + 60); return;
                }
            }
            // Fullscreen & VSync toggles
            if (my >= curY + 110.0f && my <= curY + 134.0f) {
                if (mx >= pX + 16.0f && mx <= pX + 196.0f) {
                    _settingsFullscreen = !_settingsFullscreen; return;
                } else if (mx >= pX + 204.0f && mx <= pX + 364.0f) {
                    _settingsVSync = !_settingsVSync; return;
                }
            }
            curY += 152.0f;

            // Section 3: Pick Scene
            if (my >= curY + 44.0f && my <= curY + 70.0f && mx >= pX + pW - 220.0f && mx <= pX + pW - 20.0f) {
                std::string scenePick = LabDialogs::openFileDialog(getWindow(), "Lab Map (*.labmap;*.labscene)\0*.labmap;*.labscene\0All Files (*.*)\0*.*\0", "assets\\maps");
                if (!scenePick.empty()) {
                    _settingsEntryScene = scenePick;
                }
                return;
            }
            curY += 88.0f;

            // Section 4: Action buttons
            if (my >= curY && my <= curY + 40.0f) {
                // [ SAVE PROJECT SETTINGS ]
                if (mx >= pX && mx <= pX + 240.0f) {
                    ProjectInfo info = ProjectManager::get().getProjectInfo(ProjectManager::get().getActiveProjectName());
                    info.dimension = _settingsDimension;
                    info.display.width = _settingsWidth;
                    info.display.height = _settingsHeight;
                    info.display.fullscreen = _settingsFullscreen;
                    info.display.vsync = _settingsVSync;
                    if (!_settingsEntryScene.empty()) info.defaultScene = _settingsEntryScene;
                    if (ProjectManager::get().saveProjectSettings(ProjectManager::get().getActiveProjectName(), info)) {
                        _settingsStatusBanner = "Project settings saved to project.labproject & game.config!";
                        _settingsBannerTimer = 4.0f;
                        refreshProjectHub();
                        logMessage("Saved project settings (" + _settingsDimension + ", " + std::to_string(_settingsWidth) + "x" + std::to_string(_settingsHeight) + ").");
                    }
                    return;
                }
                // [ SAVE & APPLY TO GAME CONFIG ]
                else if (mx >= pX + 255.0f && mx <= pX + 515.0f) {
                    ProjectInfo info = ProjectManager::get().getProjectInfo(ProjectManager::get().getActiveProjectName());
                    info.dimension = _settingsDimension;
                    info.display.width = _settingsWidth;
                    info.display.height = _settingsHeight;
                    info.display.fullscreen = _settingsFullscreen;
                    info.display.vsync = _settingsVSync;
                    if (!_settingsEntryScene.empty()) info.defaultScene = _settingsEntryScene;
                    ProjectManager::get().saveProjectSettings(ProjectManager::get().getActiveProjectName(), info);

                    // Update in-editor export settings
                    _exportSettings.width = _settingsWidth;
                    _exportSettings.height = _settingsHeight;
                    _exportSettings.fullscreen = _settingsFullscreen;
                    _exportSettings.dimension = _settingsDimension;
                    if (!_settingsEntryScene.empty()) _exportSettings.entrySceneURI = _settingsEntryScene;

                    // Write game.config to test_build/debug/game.config as well
                    std::error_code ec;
                    std::filesystem::create_directories("test_build/debug", ec);
                    std::ofstream dbgCfg("test_build/debug/game.config");
                    if (dbgCfg.is_open()) {
                        dbgCfg << "title=" << info.displayName << "\n"
                               << "dimension=" << _settingsDimension << "\n"
                               << "scene=" << info.defaultScene << "\n"
                               << "width=" << _settingsWidth << "\n"
                               << "height=" << _settingsHeight << "\n"
                               << "fullscreen=" << (_settingsFullscreen ? "1" : "0") << "\n"
                               << "vsync=" << (_settingsVSync ? "1" : "0") << "\n";
                        if (!info.defaultHud.empty()) dbgCfg << "hud=" << info.defaultHud << "\n";
                        dbgCfg.close();
                    }

                    _settingsStatusBanner = "Game config updated: " + std::to_string(_settingsWidth) + "x" + std::to_string(_settingsHeight) + (_settingsFullscreen ? " (Fullscreen)" : " (Windowed)");
                    _settingsBannerTimer = 4.0f;
                    logMessage("Applied to Game Config: " + std::to_string(_settingsWidth) + "x" + std::to_string(_settingsHeight) + ", Fullscreen=" + (_settingsFullscreen ? "true" : "false"));
                    return;
                }
                // [ Back to Scene ]
                else if (mx >= pX + 530.0f && mx <= pX + 710.0f) {
                    setWorkspace(_settingsDimension == "2D" ? EditorWorkspace::Level2D_Tilemap : EditorWorkspace::World3D_Level);
                    return;
                }
            }
            return;
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
        float gScale = getGizmoScale(pos);
        float len = 1.8f * gScale;
        float thick = 0.08f * gScale;
        float cap = 0.22f * gScale;
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
            Renderer::drawCube(pos + Vec3(0.45f * gScale, 0.45f * gScale, 0), Vec3(0.5f * gScale, 0.5f * gScale, 0.02f * gScale), hovXY ? yellowHov : Vec3(0.9f, 0.9f, 0.3f), false);

            bool hovXZ = (_hoveredGizmoAxis == GizmoAxis::PlaneXZ || _activeGizmoAxis == GizmoAxis::PlaneXZ);
            Renderer::drawCube(pos + Vec3(0.45f * gScale, 0, 0.45f * gScale), Vec3(0.5f * gScale, 0.02f * gScale, 0.5f * gScale), hovXZ ? yellowHov : Vec3(0.9f, 0.3f, 0.9f), false);

            bool hovYZ = (_hoveredGizmoAxis == GizmoAxis::PlaneYZ || _activeGizmoAxis == GizmoAxis::PlaneYZ);
            Renderer::drawCube(pos + Vec3(0, 0.45f * gScale, 0.45f * gScale), Vec3(0.02f * gScale, 0.5f * gScale, 0.5f * gScale), hovYZ ? yellowHov : Vec3(0.3f, 0.9f, 0.9f), false);
        }

        // Scale Handles
        if (isScale) {
            float sLen = 1.6f * gScale;
            float sBox = 0.22f * gScale;
            // Center uniform scale box
            bool hovUni = (_hoveredGizmoAxis == GizmoAxis::ScaleUniform || _activeGizmoAxis == GizmoAxis::ScaleUniform);
            Renderer::drawCube(pos, Vec3(0.28f * gScale, 0.28f * gScale, 0.28f * gScale), hovUni ? yellowHov : Vec3(0.95f, 0.95f, 0.95f), false);

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
            float ringR = 1.8f * gScale;
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
                loadScriptFromFile(filePath);
                setWorkspace(EditorWorkspace::Scripting_Editor);
                logMessage("[Drag&Drop] Loaded Lua script: " + filePath);
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
            if (_showRecoveryPrompt) drawSessionRecoveryModal();

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

            // Render All Trigger Volumes in Map (Cyan/Teal Wireframe Volumes)
            for (size_t i = 0; i < _map->triggers.size(); ++i) {
                const auto& tr = _map->triggers[i];
                Vec3 trCol = tr.triggered ? Vec3(0.55f, 0.55f, 0.6f) : Vec3(0.15f, 0.85f, 0.95f);
                Renderer::drawWireCube(tr.position, tr.size, trCol);
                Renderer::drawCube(tr.position, Vec3(0.35f, 0.35f, 0.35f), trCol, nullptr, false);
            }

            // Render All Bot Waypoints in Map (Color-coded by Bot Archetype + Connected Path Lines + Facing Orientation)
            for (size_t i = 0; i < _map->waypoints.size(); ++i) {
                const auto& wp = _map->waypoints[i];
                Vec3 wpCol(0.95f, 0.80f, 0.15f); // default yellow for (any bot)
                if (wp.botName.find("Guard") != std::string::npos || wp.botName.find("Security") != std::string::npos) {
                    wpCol = Vec3(0.25f, 0.90f, 0.45f); // Emerald green for Guard
                } else if (wp.botName.find("Scientist") != std::string::npos || wp.botName.find("Doctor") != std::string::npos) {
                    wpCol = Vec3(0.20f, 0.85f, 1.00f); // Cyan for Scientist
                } else if (wp.botName.find("Worker") != std::string::npos || wp.botName.find("Engineer") != std::string::npos) {
                    wpCol = Vec3(0.98f, 0.65f, 0.15f); // Hazard amber for Worker
                } else if (wp.botName.find("Synth") != std::string::npos || wp.botName.find("Combat") != std::string::npos || wp.botName.find("T800") != std::string::npos) {
                    wpCol = Vec3(0.95f, 0.35f, 0.85f); // Magenta for Synth
                }

                Renderer::drawCube(wp.position, Vec3(0.45f, 0.45f, 0.45f), wpCol, nullptr, false);
                Renderer::drawWireCube(wp.position, Vec3(0.65f, 0.65f, 0.65f), wpCol * 1.2f);

                // Direction pointer showing facing yaw
                float rad = wp.facingYaw * (3.14159265f / 180.0f);
                Vec3 fwd(std::sin(rad), 0.0f, std::cos(rad));
                Vec3 arrowPos = wp.position + fwd * 0.45f;
                Renderer::drawCube(arrowPos, Vec3(0.0f, wp.facingYaw, 0.0f), Vec3(0.10f, 0.10f, 0.35f), Vec3(0.2f, 0.9f, 1.0f), nullptr, false);

                // Connect to next waypoint in the same bot's patrol path
                size_t nextWp = (size_t)-1;
                for (size_t j = i + 1; j < _map->waypoints.size(); ++j) {
                    if (wp.botName.empty() || _map->waypoints[j].botName.empty() || _map->waypoints[j].botName == wp.botName) {
                        nextWp = j;
                        break;
                    }
                }
                if (nextWp != (size_t)-1) {
                    Vec3 pA = wp.position;
                    Vec3 pB = _map->waypoints[nextWp].position;
                    Vec3 mid = (pA + pB) * 0.5f;
                    Vec3 diff = pB - pA;
                    float len = diff.length();
                    if (len > 0.05f) {
                        float yawDeg = std::atan2(diff.x, diff.z) * (180.0f / 3.14159265f);
                        Renderer::drawCube(mid, Vec3(0.0f, yawDeg, 0.0f), Vec3(0.08f, 0.08f, len), wpCol, nullptr, false);
                    }
                }

                // Visual link line from Waypoint to its designated Linked Trigger
                if (!wp.targetTrigger.empty()) {
                    for (const auto& tr : _map->triggers) {
                        if (tr.name == wp.targetTrigger) {
                            Vec3 pA = wp.position;
                            Vec3 pB = tr.position;
                            Vec3 mid = (pA + pB) * 0.5f;
                            Vec3 diff = pB - pA;
                            float len = diff.length();
                            if (len > 0.05f) {
                                float yawDeg = std::atan2(diff.x, diff.z) * (180.0f / 3.14159265f);
                                Renderer::drawCube(mid, Vec3(0.0f, yawDeg, 0.0f), Vec3(0.04f, 0.04f, len), Vec3(0.20f, 0.85f, 1.0f), nullptr, false);
                            }
                            break;
                        }
                    }
                }
            }

            // Visual link lines from Triggers to their Target Entity / Door mechanisms
            for (const auto& tr : _map->triggers) {
                if (!tr.targetEntity.empty()) {
                    for (const auto& d : _map->doors) {
                        if (d.name == tr.targetEntity) {
                            Vec3 pA = tr.position;
                            Vec3 pB = d.position;
                            Vec3 mid = (pA + pB) * 0.5f;
                            Vec3 diff = pB - pA;
                            float len = diff.length();
                            if (len > 0.05f) {
                                float yawDeg = std::atan2(diff.x, diff.z) * (180.0f / 3.14159265f);
                                Renderer::drawCube(mid, Vec3(0.0f, yawDeg, 0.0f), Vec3(0.05f, 0.05f, len), Vec3(1.0f, 0.85f, 0.20f), nullptr, false);
                            }
                            break;
                        }
                    }
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
            } else if (_selectionType == SelectionType::Trigger && _selectedIndex >= 0 && _selectedIndex < (int)_map->triggers.size()) {
                const auto& tr = _map->triggers[_selectedIndex];
                Vec3 half = tr.size * 0.5f;
                Renderer::drawBoundingBox(tr.position - half, tr.position + half, hammerOrange);
                drawGizmo(tr.position);
            } else if (_selectionType == SelectionType::Waypoint && _selectedIndex >= 0 && _selectedIndex < (int)_map->waypoints.size()) {
                const auto& wp = _map->waypoints[_selectedIndex];
                Vec3 half(0.5f, 0.5f, 0.5f);
                Renderer::drawBoundingBox(wp.position - half, wp.position + half, hammerOrange);
                drawGizmo(wp.position);
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

        // Render Native Subsystem 3D Pass (Decals & Particles)
        _decalSystem.render(_camera);
        _particleSystem.render(_camera);

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
        // Build modal removed - Hammer uses Run Map / Bake directly

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

        // 10. Render Session Crash Recovery Modal if open
        if (_showRecoveryPrompt) {
            drawSessionRecoveryModal();
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

    // ---- Blender-style drag fields (row 0 = Position, 1 = Size, 2 = Rotation) ----
    static void dragFieldRect(const SidebarLayout& l, int row, int axis, float& x, float& y, float& w, float& h) {
        y = (row == 0) ? l.posBtnsY : (row == 1) ? l.dimBtnsY : l.rotBtnsY;
        h = 24.0f;
        w = 90.0f;
        x = l.rightX + 10.0f + axis * 95.0f;
    }

    void applyFieldStep(int row, int axis, int dir) {
        float d[3] = { 0.0f, 0.0f, 0.0f };
        if (row == 0) {
            d[axis] = (float)dir * _gridSnap;
            moveSelection(d[0], d[1], d[2]);
        } else if (row == 1) {
            d[axis] = (float)dir * _gridSnap;
            resizeSelection(d[0], d[1], d[2]);
            float* bs[3] = { &_brushSize.x, &_brushSize.y, &_brushSize.z };
            *bs[axis] = std::max(0.5f, *bs[axis] + d[axis]);
        } else {
            d[axis] = (float)dir * std::max(1.0f, _angleSnap);
            rotateSelection(d[0], d[1], d[2]);
        }
    }

    void setFieldValue(int row, int axis, float targetVal) {
        if (row == 0) {
            Vec3 curPos = getSelectedPosition();
            float curVal = (axis == 0) ? curPos.x : (axis == 1) ? curPos.y : curPos.z;
            float delta = targetVal - curVal;
            float d[3] = { 0.0f, 0.0f, 0.0f };
            d[axis] = delta;
            moveSelection(d[0], d[1], d[2]);
            logMessage("Set Position " + std::string(axis == 0 ? "X" : axis == 1 ? "Y" : "Z") + " to " + std::to_string(targetVal));
        } else if (row == 1) {
            Vec3 curSize = getSelectedScale();
            float curVal = (axis == 0) ? curSize.x : (axis == 1) ? curSize.y : curSize.z;
            float delta = targetVal - curVal;
            float d[3] = { 0.0f, 0.0f, 0.0f };
            d[axis] = delta;
            resizeSelection(d[0], d[1], d[2]);
            float* bs[3] = { &_brushSize.x, &_brushSize.y, &_brushSize.z };
            *bs[axis] = std::max(0.1f, targetVal);
            logMessage("Set Size " + std::string(axis == 0 ? "Width" : axis == 1 ? "Height" : "Depth") + " to " + std::to_string(targetVal));
        } else {
            Vec3 curRot = getSelectedRotation();
            float curVal = (axis == 0) ? curRot.x : (axis == 1) ? curRot.y : curRot.z;
            float delta = targetVal - curVal;
            float d[3] = { 0.0f, 0.0f, 0.0f };
            d[axis] = delta;
            rotateSelection(d[0], d[1], d[2]);
            logMessage("Set Rotation " + std::string(axis == 0 ? "Pitch" : axis == 1 ? "Yaw" : "Roll") + " to " + std::to_string(targetVal) + " deg");
        }
    }

    void commitFieldEdit() {
        if (_fieldEditRow >= 0 && !_fieldEditBuffer.empty()) {
            try {
                float val = std::stof(_fieldEditBuffer);
                setFieldValue(_fieldEditRow, _fieldEditAxis, val);
            } catch (...) {}
        }
        _fieldEditRow = -1;
        _fieldEditAxis = -1;
        _fieldEditBuffer = "";
    }

    void cancelFieldEdit() {
        _fieldEditRow = -1;
        _fieldEditAxis = -1;
        _fieldEditBuffer = "";
    }

    void updateDragFields(float mx) {
        if (_dragRow < 0) return;
        if (Input::isMouseButtonPressed(0)) {
            _dragAccum += mx - _dragLastX;
            _dragLastX = mx;
            if (std::abs(mx - _dragPressX) > 3.0f) _dragMoved = true;
            if (_dragMoved) {
                constexpr float kPxPerStep = 8.0f;
                while (_dragAccum >= kPxPerStep)  { applyFieldStep(_dragRow, _dragAxis, +1); _dragAccum -= kPxPerStep; }
                while (_dragAccum <= -kPxPerStep) { applyFieldStep(_dragRow, _dragAxis, -1); _dragAccum += kPxPerStep; }
            }
        } else {
            if (!_dragMoved) {
                int fbW = 0, fbH = 0;
                getFramebufferSize(fbW, fbH);
                float w = (fbW > 0) ? (float)fbW : (float)getWidth();
                float h = (fbH > 0) ? (float)fbH : (float)getHeight();
                SidebarLayout l = getSidebarLayout(w, h);
                float fx, fy, fw, fh;
                dragFieldRect(l, _dragRow, _dragAxis, fx, fy, fw, fh);
                if (_dragPressX <= fx + 20.0f) {
                    applyFieldStep(_dragRow, _dragAxis, -1);
                } else if (_dragPressX >= fx + fw - 20.0f) {
                    applyFieldStep(_dragRow, _dragAxis, +1);
                } else {
                    // Click on field body -> enter direct keyboard input!
                    _fieldEditRow = _dragRow;
                    _fieldEditAxis = _dragAxis;
                    const Vec3 v = (_dragRow == 0) ? getSelectedPosition() : (_dragRow == 1) ? getSelectedScale() : getSelectedRotation();
                    const float val = (_dragAxis == 0) ? v.x : (_dragAxis == 1) ? v.y : v.z;
                    char buf[32];
                    if (_dragRow == 2) snprintf(buf, sizeof(buf), "%.0f", val);
                    else snprintf(buf, sizeof(buf), "%.2f", val);
                    _fieldEditBuffer = buf;
                    _fieldEditCursorBlink = 0.0f;
                }
            }
            _dragRow = -1; _dragAxis = -1; _dragMoved = false;
        }
    }

    void drawDragRow(const SidebarLayout& l, int row) {
        static const char* kNames[3][3] = { { "X", "Y", "Z" }, { "W", "H", "D" }, { "Pitch", "Yaw", "Roll" } };
        const Vec3 kAxisCol[3] = { Vec3(0.95f, 0.35f, 0.35f), Vec3(0.45f, 0.85f, 0.40f), Vec3(0.40f, 0.60f, 1.0f) };
        const auto& c = LabTheme::colors();
        const Vec3 v = (row == 0) ? getSelectedPosition() : (row == 1) ? getSelectedScale() : getSelectedRotation();
        const float comp[3] = { v.x, v.y, v.z };
        for (int axis = 0; axis < 3; ++axis) {
            float fx, fy, fw, fh;
            dragFieldRect(l, row, axis, fx, fy, fw, fh);
            const bool hov = (_mouseScreenX >= fx && _mouseScreenX <= fx + fw && _mouseScreenY >= fy && _mouseScreenY <= fy + fh);
            const bool act = (_dragRow == row && _dragAxis == axis);
            const bool isEditing = (_fieldEditRow == row && _fieldEditAxis == axis);

            if (isEditing) {
                Renderer::drawRect(fx, fy, fw, fh, c.inputBg);
                Renderer::drawRect(fx, fy, fw, 1.0f, c.accent);
                Renderer::drawRect(fx, fy, 1.0f, fh, c.accent);
                Renderer::drawRect(fx, fy + fh - 1.0f, fw, 1.0f, c.accent);
                Renderer::drawRect(fx + fw - 1.0f, fy, 1.0f, fh, c.accent);
                Renderer::drawRect(fx + 2.0f, fy + 2.0f, 3.0f, fh - 4.0f, kAxisCol[axis]);

                const float th = LabFont::getTextHeight(1.4f, LabFontType::System);
                const float ty = fy + (fh - th) * 0.5f;
                LabFont::drawText(fx + 8.0f, ty, kNames[row][axis], 1.4f, c.accent, LabFontType::System);

                std::string disp = _fieldEditBuffer;
                if (std::fmod(_fieldEditCursorBlink, 1.0f) < 0.5f) disp += "|";
                LabFont::drawText(fx + 24.0f, ty, disp, 1.4f, c.textActive, LabFontType::System);
                continue;
            }

            LabTheme::drawButton(fx, fy, fw, fh, "", act, false, _mouseScreenX, _mouseScreenY, false);
            Renderer::drawRect(fx + 2.0f, fy + 2.0f, 3.0f, fh - 4.0f, kAxisCol[axis]);

            char buf[32];
            snprintf(buf, sizeof(buf), (row == 2) ? "%.0f deg" : "%.2f", comp[axis]);
            const float th = LabFont::getTextHeight(1.4f, LabFontType::System);
            const float ty = fy + (fh - th) * 0.5f;
            const float tw = LabFont::getTextWidth(buf, 1.4f, LabFontType::System);
            LabFont::drawText(fx + 10.0f, ty, hov ? "<" : kNames[row][axis], 1.4f, hov ? c.accent : c.textMuted, LabFontType::System);
            LabFont::drawText(fx + fw - 8.0f - tw, ty, buf, 1.4f, act ? c.textActive : c.textRegular, LabFontType::System);
            if (hov && !act) LabFont::drawText(fx + fw - 8.0f - tw - 12.0f, ty, ">", 1.4f, c.accent, LabFontType::System);
        }
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
        bool hovView  = (_mouseScreenY >= 0.0f && _mouseScreenY <= 24.0f && _mouseScreenX >= 92.0f && _mouseScreenX <= 136.0f);
        bool hovMap   = (_mouseScreenY >= 0.0f && _mouseScreenY <= 24.0f && _mouseScreenX >= 138.0f && _mouseScreenX <= 184.0f);
        bool hovTools = (_mouseScreenY >= 0.0f && _mouseScreenY <= 24.0f && _mouseScreenX >= 186.0f && _mouseScreenX <= 236.0f);
        bool hovHelp  = (_mouseScreenY >= 0.0f && _mouseScreenY <= 24.0f && _mouseScreenX >= 238.0f && _mouseScreenX <= 282.0f);

        if (hovFile)  Renderer::drawRect(10.0f, 2.0f, 40.0f, 20.0f, Vec3(0.24f, 0.28f, 0.36f));
        if (hovEdit)  Renderer::drawRect(52.0f, 2.0f, 38.0f, 20.0f, Vec3(0.24f, 0.28f, 0.36f));
        if (hovView)  Renderer::drawRect(92.0f, 2.0f, 44.0f, 20.0f, Vec3(0.24f, 0.28f, 0.36f));
        if (hovMap)   Renderer::drawRect(138.0f, 2.0f, 46.0f, 20.0f, Vec3(0.24f, 0.28f, 0.36f));
        if (hovTools) Renderer::drawRect(186.0f, 2.0f, 50.0f, 20.0f, Vec3(0.24f, 0.28f, 0.36f));
        if (hovHelp)  Renderer::drawRect(238.0f, 2.0f, 44.0f, 20.0f, Vec3(0.24f, 0.28f, 0.36f));

        LabFont::drawText(14.0f, 5.0f, "File", 1.8f, hovFile ? orangeGlow : textLight, LabFontType::System);
        LabFont::drawText(54.0f, 5.0f, "Edit", 1.8f, hovEdit ? orangeGlow : textLight, LabFontType::System);
        LabFont::drawText(94.0f, 5.0f, "View", 1.8f, hovView ? orangeGlow : textLight, LabFontType::System);
        LabFont::drawText(140.0f, 5.0f, "Map", 1.8f, hovMap ? orangeGlow : (Vec3(0.4f, 0.9f, 0.5f)), LabFontType::System);
        LabFont::drawText(188.0f, 5.0f, "Tools", 1.8f, hovTools ? orangeGlow : textLight, LabFontType::System);
        LabFont::drawText(242.0f, 5.0f, "Help", 1.8f, hovHelp ? orangeGlow : textLight, LabFontType::System);

        // Active Workspace Indicator Badge (Clickable to open View menu)
        std::string wsBreadcrumb = "Workspace: " + getWorkspaceBreadcrumb(_workspace);
        float wsBadgeX = 295.0f;
        float wsBadgeW = 285.0f;
        bool isWsHov = (_mouseScreenY >= 0.0f && _mouseScreenY <= 24.0f && _mouseScreenX >= wsBadgeX && _mouseScreenX <= wsBadgeX + wsBadgeW);
        Renderer::drawRect(wsBadgeX, 2.0f, wsBadgeW, 20.0f, isWsHov ? Vec3(0.18f, 0.22f, 0.30f) : Vec3(0.12f, 0.14f, 0.18f));
        drawDarkBevel(wsBadgeX, 2.0f, wsBadgeW, 20.0f, false);
        Renderer::drawRect(wsBadgeX, 2.0f, 3.0f, 20.0f, cyanGlow);
        LabFont::drawText(wsBadgeX + 10.0f, 5.0f, wsBreadcrumb, 1.4f, isWsHov ? orangeGlow : cyanGlow, LabFontType::System);

        LabFont::drawText(w - 380.0f, 5.0f, "Hammer Editor - Frozen-Life SDK", 1.8f, orangeGlow, LabFontType::GeoSans);

        // ==================== 2. MAIN TOOLBAR (12 Deduplicated Buttons) ====================
        float tbY = 24.0f;
        float tbH = 34.0f;
        Renderer::drawRect(0, tbY, w, tbH, panelDarkBg);
        drawDarkBevel(0, tbY, w, tbH, false);

        const int tbIcons[12] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 15, 16 };
        for (int i = 0; i < 12; ++i) {
            float bx = 8.0f + i * 26.0f;
            bool isHov = (_mouseScreenX >= bx && _mouseScreenX <= bx + 24.0f && _mouseScreenY >= tbY + 4.0f && _mouseScreenY <= tbY + 28.0f);
            Vec3 bgCol = (isHov ? Vec3(0.32f, 0.35f, 0.40f) : Vec3(0.22f, 0.23f, 0.26f));
            Vec3 iconCol = (isHov ? Vec3(1.0f, 0.95f, 0.80f) : Vec3(0.85f, 0.88f, 0.92f));

            Renderer::drawRect(bx, tbY + 4.0f, 24.0f, 24.0f, bgCol);
            drawDarkBevel(bx, tbY + 4.0f, 24.0f, 24.0f, false);
            drawToolbarIcon(tbIcons[i], bx, tbY + 4.0f, iconCol, bgCol);
        }

        // Separator
        Renderer::drawRect(324.0f, tbY + 4.0f, 1.0f, 24.0f, Vec3(0.32f, 0.35f, 0.40f));

        // Transform Gizmo Mode Buttons (Move, Rotate, Scale)
        const struct { const char* label; GizmoMode mode; float x; float w; } kGizmoBtns[] = {
            { "Move",   GizmoMode::Move,   328.0f, 48.0f },
            { "Rotate", GizmoMode::Rotate, 380.0f, 52.0f },
            { "Scale",  GizmoMode::Scale,  436.0f, 48.0f }
        };
        for (const auto& gb : kGizmoBtns) {
            bool isAct = (_gizmoMode == gb.mode && _activeTool == 0);
            bool isHov = (_mouseScreenX >= gb.x && _mouseScreenX <= gb.x + gb.w && _mouseScreenY >= tbY + 4.0f && _mouseScreenY <= tbY + 28.0f);
            Vec3 bgCol = isAct ? orangeGlow : (isHov ? Vec3(0.36f, 0.38f, 0.42f) : Vec3(0.24f, 0.25f, 0.28f));
            Vec3 txtCol = isAct ? Vec3(1, 1, 1) : (isHov ? Vec3(1.0f, 0.9f, 0.7f) : Vec3(0.85f, 0.88f, 0.92f));
            Renderer::drawRect(gb.x, tbY + 4.0f, gb.w, 24.0f, bgCol);
            drawDarkBevel(gb.x, tbY + 4.0f, gb.w, 24.0f, isAct);
            if (isAct) Renderer::drawRect(gb.x, tbY + 26.0f, gb.w, 2.0f, Vec3(1.0f, 0.9f, 0.4f));
            float tw = LabFont::getTextWidth(gb.label, 1.25f, LabFontType::System);
            LabFont::drawText(gb.x + (gb.w - tw) * 0.5f, tbY + 8.0f, gb.label, 1.25f, txtCol, LabFontType::System);
        }

        // Separator
        Renderer::drawRect(490.0f, tbY + 4.0f, 1.0f, 24.0f, Vec3(0.32f, 0.35f, 0.40f));

        // 1. [ > PLAY (F5) ]
        float bRunX = 496.0f;
        float bRunW = 60.0f;
        bool isBRunHov = (_mouseScreenX >= bRunX && _mouseScreenX <= bRunX + bRunW && _mouseScreenY >= tbY + 4.0f && _mouseScreenY <= tbY + 28.0f);
        Vec3 bRunCol = isBRunHov ? Vec3(0.25f, 0.88f, 0.45f) : Vec3(0.18f, 0.72f, 0.38f);
        Renderer::drawRect(bRunX, tbY + 4.0f, bRunW, 24.0f, bRunCol);
        drawDarkBevel(bRunX, tbY + 4.0f, bRunW, 24.0f, false);
        LabFont::drawText(bRunX + 16.0f, tbY + 8.0f, "Play", 1.2f, Vec3(1, 1, 1), LabFontType::System);

        // 2. [ BUILD ]
        float bExpX = 562.0f;
        float bExpW = 56.0f;
        bool isBExpHov = (_mouseScreenX >= bExpX && _mouseScreenX <= bExpX + bExpW && _mouseScreenY >= tbY + 4.0f && _mouseScreenY <= tbY + 28.0f);
        Vec3 bExpCol = isBExpHov ? Vec3(0.25f, 0.58f, 0.92f) : Vec3(0.18f, 0.46f, 0.76f);
        Renderer::drawRect(bExpX, tbY + 4.0f, bExpW, 24.0f, bExpCol);
        drawDarkBevel(bExpX, tbY + 4.0f, bExpW, 24.0f, false);
        LabFont::drawText(bExpX + 12.0f, tbY + 8.0f, "Build", 1.2f, Vec3(1, 1, 1), LabFontType::System);

        // 3. [ BAKE ]
        float bBakeX = 624.0f;
        float bBakeW = 56.0f;
        bool isBBakeHov = (_mouseScreenX >= bBakeX && _mouseScreenX <= bBakeX + bBakeW && _mouseScreenY >= tbY + 4.0f && _mouseScreenY <= tbY + 28.0f);
        Vec3 bBakeCol = isBBakeHov ? Vec3(0.95f, 0.55f, 0.20f) : Vec3(0.80f, 0.42f, 0.12f);
        Renderer::drawRect(bBakeX, tbY + 4.0f, bBakeW, 24.0f, bBakeCol);
        drawDarkBevel(bBakeX, tbY + 4.0f, bBakeW, 24.0f, false);
        LabFont::drawText(bBakeX + 12.0f, tbY + 8.0f, "Bake", 1.2f, Vec3(1, 1, 1), LabFontType::System);

        // 3. [ 2D / 3D ]
        float viewModeX = 686.0f;
        float viewModeW = 64.0f;
        bool isOrtho = _orthoViewMode;
        bool isViewHov = (_mouseScreenX >= viewModeX && _mouseScreenX <= viewModeX + viewModeW && _mouseScreenY >= tbY + 4.0f && _mouseScreenY <= tbY + 28.0f);
        Vec3 viewCol = isOrtho ? Vec3(0.20f, 0.55f, 0.85f) : (isViewHov ? Vec3(0.28f, 0.32f, 0.38f) : Vec3(0.20f, 0.22f, 0.25f));
        Renderer::drawRect(viewModeX, tbY + 4.0f, viewModeW, 24.0f, viewCol);
        drawDarkBevel(viewModeX, tbY + 4.0f, viewModeW, 24.0f, isOrtho);
        LabFont::drawText(viewModeX + 16.0f, tbY + 8.0f, isOrtho ? "2D" : "3D", 1.2f, isOrtho ? Vec3(1, 1, 1) : cyanGlow, LabFontType::System);

        // 4. [+ LIGHT]
        float lightX = 756.0f;
        float lightW = 56.0f;
        bool isLightTool = (_activeTool == 7);
        bool isLightHov = (_mouseScreenX >= lightX && _mouseScreenX <= lightX + lightW && _mouseScreenY >= tbY + 4.0f && _mouseScreenY <= tbY + 28.0f);
        Vec3 lightBtnBg = isLightTool ? Vec3(0.95f, 0.75f, 0.15f) : (isLightHov ? Vec3(0.32f, 0.35f, 0.40f) : Vec3(0.22f, 0.23f, 0.26f));
        Renderer::drawRect(lightX, tbY + 4.0f, lightW, 24.0f, lightBtnBg);
        drawDarkBevel(lightX, tbY + 4.0f, lightW, 24.0f, isLightTool);
        LabFont::drawText(lightX + 10.0f, tbY + 8.0f, "Light", 1.2f, isLightTool ? Vec3(0.05f, 0.05f, 0.05f) : Vec3(1.0f, 0.85f, 0.2f), LabFontType::System);

        // 5. [ QUICK FX v ]
        float qfxX = 818.0f;
        float qfxW = 56.0f;
        bool isQfxHov = (_mouseScreenX >= qfxX && _mouseScreenX <= qfxX + qfxW && _mouseScreenY >= tbY + 4.0f && _mouseScreenY <= tbY + 28.0f);
        Vec3 qfxBg = _quickFxMenuOpen ? Vec3(0.55f, 0.25f, 0.80f) : (isQfxHov ? Vec3(0.35f, 0.30f, 0.45f) : Vec3(0.25f, 0.22f, 0.30f));
        Renderer::drawRect(qfxX, tbY + 4.0f, qfxW, 24.0f, qfxBg);
        drawDarkBevel(qfxX, tbY + 4.0f, qfxW, 24.0f, _quickFxMenuOpen);
        LabFont::drawText(qfxX + 18.0f, tbY + 8.0f, "FX", 1.2f, Vec3(0.9f, 0.85f, 1.0f), LabFontType::System);

        // Floating Quick FX Dropdown Menu
        if (_quickFxMenuOpen) {
            float mX = 818.0f;
            float mY = 58.0f;
            float mW = 190.0f;
            float mH = 7 * 26.0f + 8.0f;
            Renderer::drawRect(mX, mY, mW, mH, Vec3(0.14f, 0.15f, 0.18f));
            drawDarkBevel(mX, mY, mW, mH, false);

            const char* kFxItems[7] = {
                _darknessMode ? "Darkness Mode: [ON]" : "Darkness Mode: [OFF]",
                "Apply Darkness Preset",
                "* Sparks Particle FX",
                "* Smoke Explosion FX",
                "* Scorch Decal FX",
                "> 3D Audio SFX Test",
                "VCS Snapshot Commit"
            };
            for (int k = 0; k < 7; ++k) {
                float iy = mY + 4.0f + k * 26.0f;
                bool isItemHov = (_mouseScreenX >= mX + 2.0f && _mouseScreenX <= mX + mW - 2.0f && _mouseScreenY >= iy && _mouseScreenY <= iy + 24.0f);
                if (isItemHov) {
                    Renderer::drawRect(mX + 2.0f, iy, mW - 4.0f, 24.0f, Vec3(0.28f, 0.32f, 0.42f));
                }
                Vec3 itemCol = (k == 0 && _darknessMode) ? Vec3(0.9f, 0.6f, 1.0f) :
                               (k == 1) ? Vec3(0.6f, 0.85f, 1.0f) :
                               (k == 2) ? Vec3(1.0f, 0.85f, 0.3f) :
                               (k == 3) ? Vec3(1.0f, 0.6f, 0.3f) :
                               (k == 4) ? Vec3(0.8f, 0.9f, 1.0f) :
                               (k == 5) ? Vec3(0.5f, 1.0f, 0.8f) :
                               Vec3(0.7f, 0.85f, 1.0f);
                LabFont::drawText(mX + 10.0f, iy + 5.0f, kFxItems[k], 1.2f, itemCol, LabFontType::System);
            }
        }

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
                { "Bot Waypoint Node", "AI patrol / story path waypoint node", Vec3(0.95f, 0.80f, 0.15f) }
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

                std::string itemDesc = activeItems[i].desc;
                if (!isCat0 && i == 8) {
                    itemDesc = "AI patrol [Bot: " + (_defaultWaypointBotName.empty() ? "Any" : _defaultWaypointBotName) + "] (RMB cycle)";
                }
                LabFont::drawText(l.rightX + 52.0f, cy + (isCat0 ? 22.0f : 19.0f), itemDesc, 1.3f, textDim, LabFontType::System);
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
            drawDarkButton(l.outFocusX, l.outFocusY, l.outFocusW, l.outFocusH, "Focus");
            drawDarkButton(l.outDupX, l.outDupY, l.outDupW, l.outDupH, "Duplicate");
            drawDarkButton(l.outDelX, l.outDelY, l.outDelW, l.outDelH, "Delete", false, true);

            // Pagination buttons
            drawDarkButton(l.outPrevX, l.outPrevY, l.outPrevW, l.outPrevH, "< Prev");
            drawDarkButton(l.outNextX, l.outNextY, l.outNextW, l.outNextH, "Next >");
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
                drawDarkButton(l.rightX + 10.0f, wepBtnY, 130.0f, 26.0f, "< Prev");
                drawDarkButton(l.rightX + 150.0f, wepBtnY, 130.0f, 26.0f, "Next >");

                // Respawn Time Buttons [-15s] [30s (Default)] [+15s]
                LabFont::drawText(l.rightX + 12.0f, wepBtnY + 36.0f, "Set Respawn Delay Cooldown:", 1.6f, textLight, LabFontType::System);
                float respBtnY = wepBtnY + 54.0f;
                drawDarkButton(l.rightX + 10.0f, respBtnY, 70.0f, 24.0f, "-15s");

                bool isDefault30 = (std::abs(ws.respawnTime - 30.0f) < 0.1f);
                drawDarkButton(l.rightX + 90.0f, respBtnY, 100.0f, 24.0f, "Default", isDefault30, false, isDefault30);
                drawDarkButton(l.rightX + 200.0f, respBtnY, 70.0f, 24.0f, "+15s");

                // Deselect & Delete buttons
                drawDarkButton(l.deselX, l.deselY, l.deselW, l.deselH, "Deselect");
                drawDarkButton(l.delX, l.delY, l.delW, l.delH, "Delete", false, true);
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
                drawDarkButton(l.deselX, l.deselY, l.deselW, l.deselH, "Deselect");
                drawDarkButton(l.delX, l.delY, l.delW, l.delH, "Delete", false, true);
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
                drawDragRow(l, 0);

                // Elevation buttons
                drawDarkButton(l.elevUpX, l.elevUpY, l.elevUpW, l.elevUpH, "^ Up", false, false, true);
                drawDarkButton(l.elevDwnX, l.elevDwnY, l.elevDwnW, l.elevDwnH, "v Down");
                drawDarkButton(l.elevGndX, l.elevGndY, l.elevGndW, l.elevGndH, "Floor");

                // Deselect & Delete buttons
                drawDarkButton(l.deselX, l.deselY, l.deselW, l.deselH, "Deselect");
                drawDarkButton(l.delX, l.delY, l.delW, l.delH, "Delete", false, true);
            } else if (_selectionType == SelectionType::Camera && _selectedIndex >= 0 && _selectedIndex < (int)_map->cameras.size()) {
                auto& cam = _map->cameras[_selectedIndex];
                std::string selHeader = "Selection: Camera Component #" + std::to_string(_selectedIndex);
                LabFont::drawText(l.rightX + 12.0f, propY, selHeader, 1.7f, orangeGlow, LabFontType::System);

                std::string classStr = "Type: Lab::Camera (ECS Component)";
                LabFont::drawText(l.rightX + 12.0f, propY + 22.0f, classStr, 1.5f, greenAccent, LabFontType::System);

                std::string posStr = "Pos: (" + std::to_string((int)cam.position.x) + ", " + std::to_string((int)cam.position.y) + ", " + std::to_string((int)cam.position.z) + ")";
                LabFont::drawText(l.rightX + 12.0f, propY + 44.0f, posStr, 1.5f, textLight, LabFontType::System);

                // MainCamera Tag & Projection Toggles
                float tagBtnY = propY + 68.0f;
                drawDarkButton(l.rightX + 10.0f, tagBtnY, 130.0f, 24.0f, cam.active ? "[*] MainCamera" : "Tag: MainCam", cam.active, false, cam.active);
                drawDarkButton(l.rightX + 150.0f, tagBtnY, 135.0f, 24.0f, cam.orthographic ? "Orthographic" : "Perspective", cam.orthographic);

                // FOV / Ortho Size adjusters
                float fovY = tagBtnY + 30.0f;
                if (!cam.orthographic) {
                    char fovBuf[64];
                    std::snprintf(fovBuf, sizeof(fovBuf), "Field of View (FOV): %.0f deg", cam.fov);
                    LabFont::drawText(l.rightX + 12.0f, fovY, fovBuf, 1.55f, cyanGlow, LabFontType::System);
                } else {
                    char orthoBuf[64];
                    std::snprintf(orthoBuf, sizeof(orthoBuf), "Orthographic Size: %.1f", cam.orthoSize);
                    LabFont::drawText(l.rightX + 12.0f, fovY, orthoBuf, 1.55f, cyanGlow, LabFontType::System);
                }
                float fovBtnY = fovY + 18.0f;
                drawDarkButton(l.rightX + 10.0f, fovBtnY, 48.0f, 22.0f, "-5");
                drawDarkButton(l.rightX + 62.0f, fovBtnY, 52.0f, 22.0f, "60 deg", std::abs(cam.fov - 60.0f) < 1.0f);
                drawDarkButton(l.rightX + 118.0f, fovBtnY, 52.0f, 22.0f, "75 deg", std::abs(cam.fov - 75.0f) < 1.0f);
                drawDarkButton(l.rightX + 174.0f, fovBtnY, 52.0f, 22.0f, "90 deg", std::abs(cam.fov - 90.0f) < 1.0f);
                drawDarkButton(l.rightX + 230.0f, fovBtnY, 48.0f, 22.0f, "+5");

                // Near & Far Clipping Planes
                float clipY = fovBtnY + 28.0f;
                char clipBuf[64];
                std::snprintf(clipBuf, sizeof(clipBuf), "Clipping: Near=%.2fm | Far=%.0fm", cam.nearClip, cam.farClip);
                LabFont::drawText(l.rightX + 12.0f, clipY, clipBuf, 1.5f, textLight, LabFontType::System);
                float clipBtnY = clipY + 18.0f;
                drawDarkButton(l.rightX + 10.0f, clipBtnY, 60.0f, 22.0f, "Near -");
                drawDarkButton(l.rightX + 74.0f, clipBtnY, 60.0f, 22.0f, "Near +");
                drawDarkButton(l.rightX + 148.0f, clipBtnY, 64.0f, 22.0f, "Far -100");
                drawDarkButton(l.rightX + 216.0f, clipBtnY, 64.0f, 22.0f, "Far +100");

                // Clear Flags
                float clrY = clipBtnY + 28.0f;
                LabFont::drawText(l.rightX + 12.0f, clrY, "Clear Flags (Background Buffer):", 1.5f, textLight, LabFontType::System);
                float clrBtnY = clrY + 18.0f;
                float cW = 66.0f;
                bool isSolid = (cam.clearFlags == "SolidColor");
                bool isSky = (cam.clearFlags == "Skybox");
                bool isDepth = (cam.clearFlags == "DepthOnly");
                bool isNoth = (cam.clearFlags == "Nothing");
                drawDarkButton(l.rightX + 10.0f, clrBtnY, cW, 22.0f, "Solid", isSolid, false, isSolid);
                drawDarkButton(l.rightX + 80.0f, clrBtnY, cW, 22.0f, "Skybox", isSky, false, isSky);
                drawDarkButton(l.rightX + 150.0f, clrBtnY, cW, 22.0f, "Depth", isDepth, false, isDepth);
                drawDarkButton(l.rightX + 220.0f, clrBtnY, cW, 22.0f, "Nothing", isNoth, false, isNoth);

                // Depth & Viewport Rect
                float dpY = clrBtnY + 28.0f;
                char dpBuf[64];
                std::snprintf(dpBuf, sizeof(dpBuf), "Depth: %d | Rect: (%.1f, %.1f, %.1f, %.1f)", cam.depth, cam.viewportRect.x, cam.viewportRect.y, cam.viewportRect.z, cam.viewportRect.w);
                LabFont::drawText(l.rightX + 12.0f, dpY, dpBuf, 1.5f, cyanGlow, LabFontType::System);
                float dpBtnY = dpY + 18.0f;
                drawDarkButton(l.rightX + 10.0f, dpBtnY, 52.0f, 22.0f, "Dp -");
                drawDarkButton(l.rightX + 66.0f, dpBtnY, 52.0f, 22.0f, "Dp +");
                bool isFull = (cam.viewportRect.z >= 0.99f && cam.viewportRect.w >= 0.99f);
                drawDarkButton(l.rightX + 124.0f, dpBtnY, 78.0f, 22.0f, "Full (1x1)", isFull);
                drawDarkButton(l.rightX + 206.0f, dpBtnY, 82.0f, 22.0f, "Minimap", !isFull, false, !isFull);

                // Background Clear Color Presets & Swatch
                float bgY = dpBtnY + 28.0f;
                char bgBuf[64];
                std::snprintf(bgBuf, sizeof(bgBuf), "Background Color: (%.2f, %.2f, %.2f)", cam.backgroundColor.x, cam.backgroundColor.y, cam.backgroundColor.z);
                LabFont::drawText(l.rightX + 12.0f, bgY, bgBuf, 1.5f, textLight, LabFontType::System);
                float bgBtnY = bgY + 18.0f;
                Renderer::drawRect(l.rightX + 10.0f, bgBtnY, 22.0f, 22.0f, Vec3(cam.backgroundColor.x, cam.backgroundColor.y, cam.backgroundColor.z));
                drawDarkBevel(l.rightX + 10.0f, bgBtnY, 22.0f, 22.0f, true);
                bool isBgBlack = (cam.backgroundColor.x < 0.05f && cam.backgroundColor.y < 0.05f && cam.backgroundColor.z < 0.05f);
                bool isBgNavy = (cam.backgroundColor.z > 0.15f && cam.backgroundColor.x < 0.1f);
                bool isBgSlate = (cam.backgroundColor.x > 0.08f && cam.backgroundColor.x < 0.2f);
                bool isBgSky = (cam.backgroundColor.y > 0.4f);
                drawDarkButton(l.rightX + 38.0f, bgBtnY, 56.0f, 22.0f, "Black", isBgBlack, false, isBgBlack);
                drawDarkButton(l.rightX + 98.0f, bgBtnY, 58.0f, 22.0f, "Navy", isBgNavy, false, isBgNavy);
                drawDarkButton(l.rightX + 160.0f, bgBtnY, 58.0f, 22.0f, "Slate", isBgSlate, false, isBgSlate);
                drawDarkButton(l.rightX + 222.0f, bgBtnY, 62.0f, 22.0f, "Sky", isBgSky, false, isBgSky);

                // Culling Mask & Target Texture
                float maskY = bgBtnY + 28.0f;
                char maskBuf[64];
                std::snprintf(maskBuf, sizeof(maskBuf), "Mask: 0x%X | Target: %s", cam.cullingMask, cam.targetTexture.c_str());
                LabFont::drawText(l.rightX + 12.0f, maskY, maskBuf, 1.45f, orangeGlow, LabFontType::System);
                float maskBtnY = maskY + 18.0f;
                drawDarkButton(l.rightX + 10.0f, maskBtnY, 82.0f, 22.0f, "Mask: All", cam.cullingMask == 0xFFFFFFFF);
                drawDarkButton(l.rightX + 96.0f, maskBtnY, 82.0f, 22.0f, "Mask: Def", cam.cullingMask == 0x1);
                drawDarkButton(l.rightX + 182.0f, maskBtnY, 102.0f, 22.0f, cam.targetTexture == "Screen" ? "Target: Screen" : "Target: FBO", cam.targetTexture == "Screen");

                // Position Translation [-] [+]
                LabFont::drawText(l.rightX + 12.0f, l.posBtnsY - 18.0f, "Move Camera (X / Y / Z):", 1.7f, cyanGlow, LabFontType::System);
                drawDragRow(l, 0);

                // Elevation buttons
                drawDarkButton(l.elevUpX, l.elevUpY, l.elevUpW, l.elevUpH, "^ Up", false, false, true);
                drawDarkButton(l.elevDwnX, l.elevDwnY, l.elevDwnW, l.elevDwnH, "v Down");
                drawDarkButton(l.elevGndX, l.elevGndY, l.elevGndW, l.elevGndH, "Eye (Y=1.8)");

                // Deselect & Delete buttons
                drawDarkButton(l.deselX, l.deselY, l.deselW, l.deselH, "Deselect");
                drawDarkButton(l.delX, l.delY, l.delW, l.delH, "Delete", false, true);
            } else if (_selectionType == SelectionType::Waypoint && _selectedIndex >= 0 && _selectedIndex < (int)_map->waypoints.size()) {
                auto& wp = _map->waypoints[_selectedIndex];
                float wpPropY = l.rightY + 38.0f;
                float wpAnimBoxY = wpPropY + 86.0f;
                float wpAnimBtn1Y = wpAnimBoxY + 28.0f;
                float wpAnimBtn2Y = wpAnimBtn1Y + 26.0f;
                float wpYawBtnY = wpAnimBtn2Y + 48.0f;
                float wpDurBtnY = wpYawBtnY + 46.0f;

                // Header
                std::string selHeader = "Selection: Bot Waypoint #" + std::to_string(_selectedIndex);
                LabFont::drawText(l.rightX + 12.0f, wpPropY, selHeader, 1.7f, orangeGlow, LabFontType::System);

                // Waypoint Identity
                std::string roleStr = "Target Bot: " + (wp.botName.empty() ? "(any bot)" : wp.botName) + " | ID: " + wp.name;
                LabFont::drawText(l.rightX + 12.0f, wpPropY + 18.0f, roleStr, 1.45f, greenAccent, LabFontType::System);

                // Coordinates & Step
                char posBuf[64];
                std::snprintf(posBuf, sizeof(posBuf), "Pos: (%.1f, %.1f, %.1f) | Step #%d", wp.position.x, wp.position.y, wp.position.z, wp.orderIndex);
                LabFont::drawText(l.rightX + 12.0f, wpPropY + 34.0f, posBuf, 1.4f, textLight, LabFontType::System);

                // Linked Story/Gameplay Trigger
                std::string trigStr = "Trig: " + (wp.targetTrigger.empty() ? "(none)" : wp.targetTrigger);
                LabFont::drawText(l.rightX + 12.0f, wpPropY + 52.0f, trigStr, 1.45f, orangeGlow, LabFontType::System);
                drawDarkButton(l.rightX + 155.0f, wpPropY + 50.0f, 80.0f, 18.0f, "Cycle Trig", !wp.targetTrigger.empty(), false, !wp.targetTrigger.empty());
                drawDarkButton(l.rightX + 240.0f, wpPropY + 50.0f, 52.0f, 18.0f, "Clear");

                // Section 1: Bot Animation
                LabFont::drawText(l.rightX + 12.0f, wpAnimBoxY - 18.0f, "Bot Action / Animation:", 1.6f, cyanGlow, LabFontType::System);
                Renderer::drawRect(l.rightX + 10.0f, wpAnimBoxY, 280.0f, 22.0f, wellBg);
                drawDarkBevel(l.rightX + 10.0f, wpAnimBoxY, 280.0f, 22.0f, true);
                std::string animDisp = "Current: " + (wp.animation.empty() ? "Idle" : wp.animation);
                LabFont::drawText(l.rightX + 18.0f, wpAnimBoxY + 4.0f, animDisp, 1.45f, Vec3(1.0f, 0.95f, 0.70f), LabFontType::System);

                // Animation Quick Select: Row 1 [Idle] [Walk] [Talk] [Attack]
                float aBtnW = 66.0f, aBtnH = 22.0f;
                drawDarkButton(l.rightX + 10.0f, wpAnimBtn1Y, aBtnW, aBtnH, "Idle", wp.animation == "Idle", false, wp.animation == "Idle");
                drawDarkButton(l.rightX + 81.0f, wpAnimBtn1Y, aBtnW, aBtnH, "Walk", wp.animation == "Walk", false, wp.animation == "Walk");
                drawDarkButton(l.rightX + 152.0f, wpAnimBtn1Y, aBtnW, aBtnH, "Talk", wp.animation == "Talk", false, wp.animation == "Talk");
                drawDarkButton(l.rightX + 223.0f, wpAnimBtn1Y, aBtnW, aBtnH, "Attack", wp.animation == "Attack", false, wp.animation == "Attack");

                // Animation Quick Select: Row 2 [Inspect] [Crouch] [Salute] [Death]
                drawDarkButton(l.rightX + 10.0f, wpAnimBtn2Y, aBtnW, aBtnH, "Inspect", wp.animation == "Inspect", false, wp.animation == "Inspect");
                drawDarkButton(l.rightX + 81.0f, wpAnimBtn2Y, aBtnW, aBtnH, "Crouch", wp.animation == "Crouch", false, wp.animation == "Crouch");
                drawDarkButton(l.rightX + 152.0f, wpAnimBtn2Y, aBtnW, aBtnH, "Salute", wp.animation == "Salute", false, wp.animation == "Salute");
                drawDarkButton(l.rightX + 223.0f, wpAnimBtn2Y, aBtnW, aBtnH, "Death", wp.animation == "Death", false, wp.animation == "Death");

                // Section 2: Facing Rotation (Yaw in degrees)
                char yawBuf[64];
                std::snprintf(yawBuf, sizeof(yawBuf), "Facing Rotation: %d deg", (int)wp.facingYaw);
                LabFont::drawText(l.rightX + 12.0f, wpYawBtnY - 18.0f, yawBuf, 1.6f, orangeGlow, LabFontType::System);

                // Yaw Quick Select: [-15] [0] [90] [180] [270] [+15]
                float yBtnW = 44.0f, yBtnH = 22.0f;
                drawDarkButton(l.rightX + 10.0f, wpYawBtnY, yBtnW, yBtnH, "-15");
                drawDarkButton(l.rightX + 57.0f, wpYawBtnY, yBtnW, yBtnH, "0", std::abs(wp.facingYaw - 0.0f) < 1.0f, false, std::abs(wp.facingYaw - 0.0f) < 1.0f);
                drawDarkButton(l.rightX + 104.0f, wpYawBtnY, yBtnW, yBtnH, "90", std::abs(wp.facingYaw - 90.0f) < 1.0f, false, std::abs(wp.facingYaw - 90.0f) < 1.0f);
                drawDarkButton(l.rightX + 151.0f, wpYawBtnY, yBtnW, yBtnH, "180", std::abs(wp.facingYaw - 180.0f) < 1.0f, false, std::abs(wp.facingYaw - 180.0f) < 1.0f);
                drawDarkButton(l.rightX + 198.0f, wpYawBtnY, yBtnW, yBtnH, "270", std::abs(wp.facingYaw - 270.0f) < 1.0f, false, std::abs(wp.facingYaw - 270.0f) < 1.0f);
                drawDarkButton(l.rightX + 245.0f, wpYawBtnY, yBtnW, yBtnH, "+15");

                // Section 3: Action Duration / Wait
                char durBuf[64];
                std::snprintf(durBuf, sizeof(durBuf), "Action Duration: %.1fs", wp.waitDuration);
                LabFont::drawText(l.rightX + 12.0f, wpDurBtnY - 18.0f, durBuf, 1.6f, textLight, LabFontType::System);

                // Duration Buttons: [0s] [1s] [2s (Def)] [5s] [10s]
                float wBtnW = 52.0f, wBtnH = 22.0f;
                drawDarkButton(l.rightX + 10.0f, wpDurBtnY, wBtnW, wBtnH, "0s", wp.waitDuration == 0.0f, false, wp.waitDuration == 0.0f);
                drawDarkButton(l.rightX + 67.0f, wpDurBtnY, wBtnW, wBtnH, "1s", wp.waitDuration == 1.0f, false, wp.waitDuration == 1.0f);
                drawDarkButton(l.rightX + 124.0f, wpDurBtnY, wBtnW, wBtnH, "2s (Def)", wp.waitDuration == 2.0f, false, wp.waitDuration == 2.0f);
                drawDarkButton(l.rightX + 181.0f, wpDurBtnY, wBtnW, wBtnH, "5s", wp.waitDuration == 5.0f, false, wp.waitDuration == 5.0f);
                drawDarkButton(l.rightX + 238.0f, wpDurBtnY, wBtnW, wBtnH, "10s", wp.waitDuration == 10.0f, false, wp.waitDuration == 10.0f);

                // Section 4: Target Bot Designation (Archetype / Specific Bot)
                float wpBotLabelY = wpDurBtnY + 30.0f;
                float wpBotBtn1Y = wpBotLabelY + 16.0f;
                float wpBotBtn2Y = wpBotBtn1Y + 22.0f;

                std::string botHeader = "Target Bot: " + (wp.botName.empty() ? "(any bot)" : wp.botName);
                LabFont::drawText(l.rightX + 12.0f, wpBotLabelY, botHeader, 1.55f, greenAccent, LabFontType::System);

                // Row 1: [Any Bot] [Guard] [Scientist]
                float bBtnW1 = 88.0f, bBtnW2 = 88.0f, bBtnW3 = 94.0f, bBtnH = 20.0f;
                float bx1 = l.rightX + 10.0f, bx2 = l.rightX + 103.0f, bx3 = l.rightX + 196.0f;
                bool isAny = wp.botName.empty() || wp.botName == "(any bot)" || wp.botName == "Any";
                bool isGuard = wp.botName == "Guard" || wp.botName == "Bot_Guard";
                bool isScientist = wp.botName == "Scientist";
                bool isWorker = wp.botName == "Worker";
                bool isSynth1 = wp.botName == "Combat Synth #1" || wp.botName == "Synth #1";
                bool isSynth2 = wp.botName == "Combat Synth #2" || wp.botName == "Synth #2";

                drawDarkButton(bx1, wpBotBtn1Y, bBtnW1, bBtnH, "Any Bot", isAny, false, isAny);
                drawDarkButton(bx2, wpBotBtn1Y, bBtnW2, bBtnH, "Guard", isGuard, false, isGuard);
                drawDarkButton(bx3, wpBotBtn1Y, bBtnW3, bBtnH, "Scientist", isScientist, false, isScientist);

                // Row 2: [Worker] [Synth #1] [Synth #2]
                drawDarkButton(bx1, wpBotBtn2Y, bBtnW1, bBtnH, "Worker", isWorker, false, isWorker);
                drawDarkButton(bx2, wpBotBtn2Y, bBtnW2, bBtnH, "Synth #1", isSynth1, false, isSynth1);
                drawDarkButton(bx3, wpBotBtn2Y, bBtnW3, bBtnH, "Synth #2", isSynth2, false, isSynth2);

                // Section 5: Move Waypoint Position
                LabFont::drawText(l.rightX + 12.0f, l.posBtnsY - 18.0f, "Move Waypoint Position (X / Y / Z):", 1.7f, cyanGlow, LabFontType::System);
                drawDragRow(l, 0);

                // Quick Elevation Action Buttons
                drawDarkButton(l.elevUpX, l.elevUpY, l.elevUpW, l.elevUpH, "^ Up", false, false, true);
                drawDarkButton(l.elevDwnX, l.elevDwnY, l.elevDwnW, l.elevDwnH, "v Down");
                drawDarkButton(l.elevGndX, l.elevGndY, l.elevGndW, l.elevGndH, "Floor");

                // Deselect & Delete buttons
                drawDarkButton(l.deselX, l.deselY, l.deselW, l.deselH, "Deselect");
                drawDarkButton(l.delX, l.delY, l.delW, l.delH, "Delete", false, true);
            } else if (_selectionType == SelectionType::Trigger && _selectedIndex >= 0 && _selectedIndex < (int)_map->triggers.size()) {
                auto& tr = _map->triggers[_selectedIndex];
                float trPropY = l.rightY + 38.0f;

                std::string selHeader = "Selection: Trigger Volume #" + std::to_string(_selectedIndex);
                LabFont::drawText(l.rightX + 12.0f, trPropY, selHeader, 1.7f, orangeGlow, LabFontType::System);

                std::string roleStr = "Target Bot: " + (tr.targetBot.empty() ? "(any bot)" : tr.targetBot) + " | Type: " + tr.eventType;
                LabFont::drawText(l.rightX + 12.0f, trPropY + 18.0f, roleStr, 1.45f, greenAccent, LabFontType::System);

                char trStatBuf[128];
                std::snprintf(trStatBuf, sizeof(trStatBuf), "Pos: (%d, %d, %d) | Size: (%d, %d, %d)", (int)tr.position.x, (int)tr.position.y, (int)tr.position.z, (int)tr.size.x, (int)tr.size.y, (int)tr.size.z);
                LabFont::drawText(l.rightX + 12.0f, trPropY + 36.0f, trStatBuf, 1.4f, textLight, LabFontType::System);

                float trEventY = trPropY + 76.0f;
                float trTargetY = trEventY + 50.0f;
                float trActionY = trTargetY + 26.0f;
                float trWpY = trActionY + 50.0f;
                float trBotY1 = trWpY + 48.0f;
                float trBotY2 = trBotY1 + 22.0f;

                // Section 1: Event Action Type
                LabFont::drawText(l.rightX + 12.0f, trEventY - 18.0f, "Trigger Event Action:", 1.55f, cyanGlow, LabFontType::System);
                float evW[5] = { 52.0f, 74.0f, 54.0f, 66.0f, 40.0f };
                float evX[5] = { l.rightX + 10.0f, l.rightX + 64.0f, l.rightX + 140.0f, l.rightX + 196.0f, l.rightX + 264.0f };
                const char* evNames[5] = { "Dialog", "Open Door", "Unlock", "Waypoint", "Script" };
                const char* evKeys[5] = { "story_dialog", "door_open", "door_unlock", "waypoint_advance", "script" };
                for (int k = 0; k < 5; ++k) {
                    bool sel = (tr.eventType == evKeys[k]);
                    drawDarkButton(evX[k], trEventY, evW[k], 22.0f, evNames[k], sel, false, sel);
                }

                // Section 2: Target Mechanism / Door
                std::string entLabel = "Target Entity: " + (tr.targetEntity.empty() ? "(none)" : tr.targetEntity) + " [" + tr.targetAction + "]";
                LabFont::drawText(l.rightX + 12.0f, trTargetY - 18.0f, entLabel, 1.55f, orangeGlow, LabFontType::System);
                drawDarkButton(l.rightX + 10.0f, trTargetY, 130.0f, 22.0f, "Cycle Door", !tr.targetEntity.empty(), false, !tr.targetEntity.empty());
                drawDarkButton(l.rightX + 144.0f, trTargetY, 70.0f, 22.0f, "Clear Target");
                drawDarkButton(l.rightX + 218.0f, trTargetY, 76.0f, 22.0f, tr.once ? "Once: ON" : "Once: OFF", tr.once, false, tr.once);

                // Section 2b: Target Action Buttons: [Open] [Close] [Toggle] [Unlock] [Lock]
                float actW[5] = { 54.0f, 54.0f, 54.0f, 60.0f, 54.0f };
                float actX[5] = { l.rightX + 10.0f, l.rightX + 66.0f, l.rightX + 122.0f, l.rightX + 178.0f, l.rightX + 240.0f };
                const char* actNames[5] = { "Open", "Close", "Toggle", "Unlock", "Lock" };
                const char* actKeys[5] = { "open", "close", "toggle", "unlock", "lock" };
                for (int k = 0; k < 5; ++k) {
                    bool sel = (tr.targetAction == actKeys[k]);
                    drawDarkButton(actX[k], trActionY, actW[k], 22.0f, actNames[k], sel, false, sel);
                }

                // Section 3: Target Waypoint Link
                std::string wpLabel = "Target Waypoint: " + (tr.targetWaypoint.empty() ? "(none)" : tr.targetWaypoint);
                LabFont::drawText(l.rightX + 12.0f, trWpY - 18.0f, wpLabel, 1.55f, cyanGlow, LabFontType::System);
                drawDarkButton(l.rightX + 10.0f, trWpY, 150.0f, 22.0f, "Cycle Waypoint", !tr.targetWaypoint.empty(), false, !tr.targetWaypoint.empty());
                drawDarkButton(l.rightX + 165.0f, trWpY, 120.0f, 22.0f, "Clear WP");

                // Section 4: Target Bot Designation
                std::string botLabel = "Triggered By: " + (tr.targetBot.empty() ? "(Any Bot)" : tr.targetBot);
                LabFont::drawText(l.rightX + 12.0f, trBotY1 - 18.0f, botLabel, 1.55f, greenAccent, LabFontType::System);
                float bW1 = 88.0f, bW2 = 88.0f, bW3 = 94.0f, bH = 20.0f;
                float bx1 = l.rightX + 10.0f, bx2 = l.rightX + 103.0f, bx3 = l.rightX + 196.0f;
                bool isAnyB = tr.targetBot.empty();
                bool isGrdB = (tr.targetBot == "Guard" || tr.targetBot == "Bot_Guard");
                bool isSciB = (tr.targetBot == "Scientist");
                bool isWrkB = (tr.targetBot == "Worker");
                bool isSyn1B = (tr.targetBot == "Combat Synth #1");
                bool isSyn2B = (tr.targetBot == "Combat Synth #2");
                drawDarkButton(bx1, trBotY1, bW1, bH, "Any Bot", isAnyB, false, isAnyB);
                drawDarkButton(bx2, trBotY1, bW2, bH, "Guard", isGrdB, false, isGrdB);
                drawDarkButton(bx3, trBotY1, bW3, bH, "Scientist", isSciB, false, isSciB);
                drawDarkButton(bx1, trBotY2, bW1, bH, "Worker", isWrkB, false, isWrkB);
                drawDarkButton(bx2, trBotY2, bW2, bH, "Synth #1", isSyn1B, false, isSyn1B);
                drawDarkButton(bx3, trBotY2, bW3, bH, "Synth #2", isSyn2B, false, isSyn2B);

                // Position Translation [-] [+]
                LabFont::drawText(l.rightX + 12.0f, l.posBtnsY - 18.0f, "Move Trigger Position (X / Y / Z):", 1.7f, cyanGlow, LabFontType::System);
                drawDragRow(l, 0);

                // Quick Elevation Action Buttons
                drawDarkButton(l.elevUpX, l.elevUpY, l.elevUpW, l.elevUpH, "^ Up", false, false, true);
                drawDarkButton(l.elevDwnX, l.elevDwnY, l.elevDwnW, l.elevDwnH, "v Down");
                drawDarkButton(l.elevGndX, l.elevGndY, l.elevGndW, l.elevGndH, "Floor");

                // Dimension Adjusters [-] [+]
                LabFont::drawText(l.rightX + 12.0f, l.dimBtnsY - 18.0f, "Adjust Size (X / Y / Z):", 1.7f, textLight, LabFontType::System);
                drawDragRow(l, 1);

                // Deselect button
                drawDarkButton(l.deselX, l.deselY, l.deselW, l.deselH, "Deselect");

                // Delete Selected button
                drawDarkButton(l.delX, l.delY, l.delW, l.delH, "Delete", false, true);

            } else if (_selectionType == SelectionType::Prop && _selectedIndex >= 0 && _selectedIndex < (int)_map->props.size()) {
                auto& p = _map->props[_selectedIndex];

                std::string selHeader = "Selection: Prop #" + std::to_string(_selectedIndex);
                LabFont::drawText(l.rightX + 12.0f, propY, selHeader, 1.7f, orangeGlow, LabFontType::System);

                std::string nameDisplay = "Entity ID: " + (p.name.empty() ? "(unnamed prop)" : p.name);
                LabFont::drawText(l.rightX + 12.0f, propY + 18.0f, nameDisplay, 1.45f, greenAccent, LabFontType::System);

                std::string posStr = "Pos: (" + std::to_string((int)p.position.x) + ", " + std::to_string((int)p.position.y) + ", " + std::to_string((int)p.position.z) + ")";
                LabFont::drawText(l.rightX + 12.0f, propY + 34.0f, posStr, 1.45f, textLight, LabFontType::System);

                std::string scaleStr = "Scale: (" + std::to_string((int)p.scale.x) + ", " + std::to_string((int)p.scale.y) + ", " + std::to_string((int)p.scale.z) + ")";
                LabFont::drawText(l.rightX + 12.0f, propY + 50.0f, scaleStr, 1.45f, textLight, LabFontType::System);

                // 3D Model Picker
                float mBoxY = propY + 90.0f;
                LabFont::drawText(l.rightX + 12.0f, mBoxY - 18.0f, "Entity 3D Mesh Model:", 1.6f, textLight, LabFontType::System);
                Renderer::drawRect(l.rightX + 10.0f, mBoxY, 280.0f, 22.0f, wellBg);
                drawDarkBevel(l.rightX + 10.0f, mBoxY, 280.0f, 22.0f, true);
                LabFont::drawText(l.rightX + 16.0f, mBoxY + 4.0f, p.modelPath.empty() ? "(default prop)" : p.modelPath, 1.45f, textLight, LabFontType::System);
                drawDarkButton(l.rightX + 10.0f, mBoxY + 26.0f, 280.0f, 24.0f, "Browse");

                // Entity Components (ECS)
                float compY = mBoxY + 58.0f;
                LabFont::drawText(l.rightX + 12.0f, compY, "Entity Components (ECS):", 1.6f, cyanGlow, LabFontType::System);

                // Quick Add Component Row
                float addBtnY = compY + 18.0f;
                drawDarkButton(l.rightX + 10.0f, addBtnY, 65.0f, 22.0f, "+ Audio", p.hasAudio, false, p.hasAudio);
                drawDarkButton(l.rightX + 80.0f, addBtnY, 65.0f, 22.0f, "+ Phys", p.hasPhysics, false, p.hasPhysics);
                drawDarkButton(l.rightX + 150.0f, addBtnY, 65.0f, 22.0f, "+ Cloth", p.hasCloth, false, p.hasCloth);
                drawDarkButton(l.rightX + 220.0f, addBtnY, 70.0f, 22.0f, "+ Water", p.hasWaterVolume, false, p.hasWaterVolume);

                float addBtn2Y = addBtnY + 26.0f;
                drawDarkButton(l.rightX + 10.0f, addBtn2Y, 88.0f, 22.0f, "+ ForceField", p.hasForceField, false, p.hasForceField);
                drawDarkButton(l.rightX + 104.0f, addBtn2Y, 88.0f, 22.0f, "+ Decal", p.hasDecal, false, p.hasDecal);
                drawDarkButton(l.rightX + 198.0f, addBtn2Y, 92.0f, 22.0f, "+ PostProcess", p.hasPostProcess, false, p.hasPostProcess);

                // Active Component Mini-Panels
                float cardY = addBtn2Y + 28.0f;
                if (p.hasAudio) {
                    char aBuf[64];
                    std::snprintf(aBuf, sizeof(aBuf), "Audio: vol=%.1f, pit=%.1f, %s", p.audioVolume, p.audioPitch, p.audioLoop ? "Loop" : "Once");
                    LabFont::drawText(l.rightX + 12.0f, cardY, aBuf, 1.4f, orangeGlow, LabFontType::System);
                    float aBtnY = cardY + 16.0f;
                    drawDarkButton(l.rightX + 10.0f, aBtnY, 48.0f, 20.0f, "Vol -");
                    drawDarkButton(l.rightX + 62.0f, aBtnY, 48.0f, 20.0f, "Vol +");
                    drawDarkButton(l.rightX + 114.0f, aBtnY, 52.0f, 20.0f, p.audioLoop ? "Loop: ON" : "Loop: OFF", p.audioLoop);
                    drawDarkButton(l.rightX + 170.0f, aBtnY, 68.0f, 20.0f, "> 3D Aud", false, false, true);
                    drawDarkButton(l.rightX + 242.0f, aBtnY, 48.0f, 20.0f, "x Del", false, true);
                    cardY += 42.0f;
                }
                if (p.hasPhysics) {
                    char phBuf[64];
                    std::snprintf(phBuf, sizeof(phBuf), "RigidBody: %dkg, bounce=%.1f, %s", (int)p.physicsMass, p.physicsRestitution, p.physicsDynamic ? "Dynamic" : "Static");
                    LabFont::drawText(l.rightX + 12.0f, cardY, phBuf, 1.4f, greenAccent, LabFontType::System);
                    float phBtnY = cardY + 16.0f;
                    drawDarkButton(l.rightX + 10.0f, phBtnY, 54.0f, 20.0f, "-5kg");
                    drawDarkButton(l.rightX + 68.0f, phBtnY, 54.0f, 20.0f, "+5kg");
                    drawDarkButton(l.rightX + 126.0f, phBtnY, 60.0f, 20.0f, p.physicsDynamic ? "Dynamic" : "Static", p.physicsDynamic);
                    drawDarkButton(l.rightX + 190.0f, phBtnY, 46.0f, 20.0f, "Bnc");
                    drawDarkButton(l.rightX + 242.0f, phBtnY, 48.0f, 20.0f, "x Del", false, true);
                    cardY += 42.0f;
                }
                if (p.hasCloth) {
                    char clBuf[64];
                    std::snprintf(clBuf, sizeof(clBuf), "Cloth Sim: stiffness=%.1f (Verlet PBD)", p.clothStiffness);
                    LabFont::drawText(l.rightX + 12.0f, cardY, clBuf, 1.4f, cyanGlow, LabFontType::System);
                    float clBtnY = cardY + 16.0f;
                    drawDarkButton(l.rightX + 10.0f, clBtnY, 58.0f, 20.0f, "Stiff -");
                    drawDarkButton(l.rightX + 72.0f, clBtnY, 58.0f, 20.0f, "Stiff +");
                    drawDarkButton(l.rightX + 242.0f, clBtnY, 48.0f, 20.0f, "x Del", false, true);
                    cardY += 42.0f;
                }
                if (p.hasWaterVolume) {
                    char wBuf[64];
                    std::snprintf(wBuf, sizeof(wBuf), "Water: density=%.0f kg/m3 (Buoyancy)", p.waterDensity);
                    LabFont::drawText(l.rightX + 12.0f, cardY, wBuf, 1.4f, cyanGlow, LabFontType::System);
                    float wBtnY = cardY + 16.0f;
                    drawDarkButton(l.rightX + 10.0f, wBtnY, 90.0f, 20.0f, p.waterDensity > 1010.0f ? "Sea (1030)" : "Fresh (1000)");
                    drawDarkButton(l.rightX + 242.0f, wBtnY, 48.0f, 20.0f, "x Del", false, true);
                    cardY += 42.0f;
                }
                if (p.hasForceField) {
                    const char* fTypes[] = { "Radial", "Vortex", "Wind" };
                    char fBuf[64];
                    std::snprintf(fBuf, sizeof(fBuf), "ForceField: %s, %.0fN, r=%.0fm", fTypes[p.forceFieldType % 3], p.forceFieldStrength, p.forceFieldRadius);
                    LabFont::drawText(l.rightX + 12.0f, cardY, fBuf, 1.4f, orangeGlow, LabFontType::System);
                    float fBtnY = cardY + 16.0f;
                    drawDarkButton(l.rightX + 10.0f, fBtnY, 56.0f, 20.0f, fTypes[p.forceFieldType % 3]);
                    drawDarkButton(l.rightX + 70.0f, fBtnY, 50.0f, 20.0f, "Str +");
                    drawDarkButton(l.rightX + 124.0f, fBtnY, 50.0f, 20.0f, "Rad +");
                    drawDarkButton(l.rightX + 178.0f, fBtnY, 60.0f, 20.0f, "* Pulse", false, false, true);
                    drawDarkButton(l.rightX + 242.0f, fBtnY, 48.0f, 20.0f, "x Del", false, true);
                    cardY += 42.0f;
                }
                if (p.hasDecal) {
                    char decBuf[64];
                    std::snprintf(decBuf, sizeof(decBuf), "Decal: %s, opacity=%.1f", p.decalTexture.c_str(), p.decalOpacity);
                    LabFont::drawText(l.rightX + 12.0f, cardY, decBuf, 1.4f, textLight, LabFontType::System);
                    float decBtnY = cardY + 16.0f;
                    drawDarkButton(l.rightX + 10.0f, decBtnY, 110.0f, 20.0f, p.decalTexture == "blood_splatter.png" ? "Blood" : "BulletHole");
                    drawDarkButton(l.rightX + 124.0f, decBtnY, 60.0f, 20.0f, "Opac +");
                    drawDarkButton(l.rightX + 242.0f, decBtnY, 48.0f, 20.0f, "x Del", false, true);
                    cardY += 42.0f;
                }
                if (p.hasPostProcess) {
                    char ppBuf[64];
                    std::snprintf(ppBuf, sizeof(ppBuf), "PostProcess: bloom=%.1f, vig=%.1f", p.ppBloom, p.ppVignette);
                    LabFont::drawText(l.rightX + 12.0f, cardY, ppBuf, 1.4f, cyanGlow, LabFontType::System);
                    float ppBtnY = cardY + 16.0f;
                    drawDarkButton(l.rightX + 10.0f, ppBtnY, 62.0f, 20.0f, "Bloom +");
                    drawDarkButton(l.rightX + 76.0f, ppBtnY, 62.0f, 20.0f, "Vig +");
                    drawDarkButton(l.rightX + 242.0f, ppBtnY, 48.0f, 20.0f, "x Del", false, true);
                    cardY += 42.0f;
                }
            } else if (_selectionType == SelectionType::Door && _selectedIndex >= 0 && _selectedIndex < (int)_map->doors.size()) {
                auto& d = _map->doors[_selectedIndex];
                float doorPropY = l.rightY + 38.0f;

                std::string selHeader = "Selection: Door #" + std::to_string(_selectedIndex);
                LabFont::drawText(l.rightX + 12.0f, doorPropY, selHeader, 1.7f, orangeGlow, LabFontType::System);

                std::string nameDisplay = "Door Entity ID: " + (d.name.empty() ? "(unnamed door)" : d.name);
                LabFont::drawText(l.rightX + 12.0f, doorPropY + 18.0f, nameDisplay, 1.45f, greenAccent, LabFontType::System);

                char dStatBuf[128];
                std::snprintf(dStatBuf, sizeof(dStatBuf), "Pos: (%d, %d, %d) | Size: (%.1f, %.1f, %.1f)", (int)d.position.x, (int)d.position.y, (int)d.position.z, d.size.x, d.size.y, d.size.z);
                LabFont::drawText(l.rightX + 12.0f, doorPropY + 36.0f, dStatBuf, 1.4f, textLight, LabFontType::System);

                float dActY = doorPropY + 76.0f;
                float dKeyY = dActY + 50.0f;
                float dDepY = dKeyY + 50.0f;

                // Section 1: Activator Mode
                std::string actLabel = "Door Activator: [" + (d.activator.empty() ? "proximity" : d.activator) + "]";
                LabFont::drawText(l.rightX + 12.0f, dActY - 18.0f, actLabel, 1.55f, cyanGlow, LabFontType::System);
                bool isProx = (d.activator == "proximity" || d.activator.empty());
                bool isTrig = (d.activator == "trigger_only");
                bool isMan = (d.activator == "manual");
                drawDarkButton(l.rightX + 10.0f, dActY, 88.0f, 22.0f, "Proximity", isProx, false, isProx);
                drawDarkButton(l.rightX + 103.0f, dActY, 88.0f, 22.0f, "Trigger Only", isTrig, false, isTrig);
                drawDarkButton(l.rightX + 196.0f, dActY, 94.0f, 22.0f, "Manual/Use", isMan, false, isMan);

                // Section 2: Keycard & Lock Security
                std::string lockLabel = "Security: " + std::string(d.isLocked ? "[LOCKED] " : "[UNLOCKED] ") + (d.requiresKeycard ? d.keycardRequired : "Public Access");
                LabFont::drawText(l.rightX + 12.0f, dKeyY - 18.0f, lockLabel, 1.55f, orangeGlow, LabFontType::System);
                bool noKc = !d.requiresKeycard;
                bool isKc1 = (d.requiresKeycard && d.keycardRequired == "keycard_l1");
                bool isKc2 = (d.requiresKeycard && d.keycardRequired == "keycard_l2");
                drawDarkButton(l.rightX + 10.0f, dKeyY, 68.0f, 22.0f, "No Keycard", noKc, false, noKc);
                drawDarkButton(l.rightX + 80.0f, dKeyY, 72.0f, 22.0f, "Keycard L1", isKc1, false, isKc1);
                drawDarkButton(l.rightX + 155.0f, dKeyY, 72.0f, 22.0f, "Keycard L2", isKc2, false, isKc2);
                drawDarkButton(l.rightX + 230.0f, dKeyY, 60.0f, 22.0f, d.isLocked ? "Unlock" : "Lock", d.isLocked, false, d.isLocked);

                // Section 3: Dependency & Mechanism
                std::string depLabel = "Dependency: " + (d.dependency.empty() ? "(none / standalone)" : d.dependency);
                LabFont::drawText(l.rightX + 12.0f, dDepY - 18.0f, depLabel, 1.55f, greenAccent, LabFontType::System);
                drawDarkButton(l.rightX + 10.0f, dDepY, 120.0f, 22.0f, "Cycle Dep", !d.dependency.empty(), false, !d.dependency.empty());
                drawDarkButton(l.rightX + 134.0f, dDepY, 70.0f, 22.0f, "Clear Dep");
                drawDarkButton(l.rightX + 208.0f, dDepY, 82.0f, 22.0f, d.autoClose ? "AutoClose: ON" : "AutoClose: OFF", d.autoClose, false, d.autoClose);

                // Position Translation [-] [+]
                LabFont::drawText(l.rightX + 12.0f, l.posBtnsY - 18.0f, "Move Door Position (X / Y / Z):", 1.7f, cyanGlow, LabFontType::System);
                drawDragRow(l, 0);

                // Quick Elevation Action Buttons
                drawDarkButton(l.elevUpX, l.elevUpY, l.elevUpW, l.elevUpH, "^ Up", false, false, true);
                drawDarkButton(l.elevDwnX, l.elevDwnY, l.elevDwnW, l.elevDwnH, "v Down");
                drawDarkButton(l.elevGndX, l.elevGndY, l.elevGndW, l.elevGndH, "Floor");

                // Dimension Adjusters [-] [+]
                LabFont::drawText(l.rightX + 12.0f, l.dimBtnsY - 18.0f, "Adjust Door Size (X / Y / Z):", 1.7f, textLight, LabFontType::System);
                drawDragRow(l, 1);

                // Deselect button
                drawDarkButton(l.deselX, l.deselY, l.deselW, l.deselH, "Deselect");

                // Delete Selected button
                drawDarkButton(l.delX, l.delY, l.delW, l.delH, "Delete", false, true);
            } else {
                std::string selHeader = "Selection: None";
                if (_selectionType == SelectionType::Brush) selHeader = "Selection: Brush #" + std::to_string(_selectedIndex);

                LabFont::drawText(l.rightX + 12.0f, propY, selHeader, 1.7f, (_selectionType != SelectionType::None) ? orangeGlow : textLight, LabFontType::System);

                Vec3 pos = _cursorPos;
                Vec3 dims = _brushSize;
                std::string entName = "";

                if (_selectionType == SelectionType::Brush && _selectedIndex >= 0 && _selectedIndex < (int)_map->brushes.size()) {
                    pos = _map->brushes[_selectedIndex].position;
                    dims = _map->brushes[_selectedIndex].size;
                    entName = _map->brushes[_selectedIndex].name;
                }

                char pBuf[64];
                std::snprintf(pBuf, sizeof(pBuf), "Pos: (%.1f, %.1f, %.1f)", pos.x, pos.y, pos.z);
                LabFont::drawText(l.rightX + 12.0f, propY + 18.0f, pBuf, 1.45f, textLight, LabFontType::System);

                char sBuf[64];
                std::snprintf(sBuf, sizeof(sBuf), "Size: (%.1f, %.1f, %.1f)", dims.x, dims.y, dims.z);
                LabFont::drawText(l.rightX + 12.0f, propY + 34.0f, sBuf, 1.45f, textLight, LabFontType::System);

                if (!entName.empty()) {
                    std::string entDisplay = "Entity ID: " + entName;
                    LabFont::drawText(l.rightX + 12.0f, propY + 50.0f, entDisplay, 1.45f, orangeGlow, LabFontType::System);
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

                drawDarkButton(l.texBrowseX, l.texBrowseY, l.texBrowseW, l.texBrowseH, "Browse");
                drawDarkButton(l.texApplyX, l.texApplyY, l.texApplyW, l.texApplyH, "Apply", false, false, true);

                // 3D Entity Prop Model Selector
                LabFont::drawText(l.rightX + 12.0f, l.modelBoxY - 18.0f, "3D Entity Model (.stl):", 1.7f, textLight, LabFontType::System);
                Renderer::drawRect(l.modelBoxX, l.modelBoxY, l.modelBoxW, l.modelBoxH, wellBg);
                drawDarkBevel(l.modelBoxX, l.modelBoxY, l.modelBoxW, l.modelBoxH, true);
                LabFont::drawText(l.modelBoxX + 10.0f, l.modelBoxY + 4.0f, _selectedModel, 1.6f, textLight, LabFontType::System);

                drawDarkButton(l.modelBrowseX, l.modelBrowseY, l.modelBrowseW, l.modelBrowseH, "Browse");
            }

            // Position Translation [-] [+]
            LabFont::drawText(l.rightX + 12.0f, l.posBtnsY - 18.0f, "Move Position (X / Y / Z):", 1.7f, cyanGlow, LabFontType::System);

            // X Position
            drawDragRow(l, 0);

            // Quick Elevation Action Buttons
            drawDarkButton(l.elevUpX, l.elevUpY, l.elevUpW, l.elevUpH, "^ Up", false, false, true);
            drawDarkButton(l.elevDwnX, l.elevDwnY, l.elevDwnW, l.elevDwnH, "v Down");
            drawDarkButton(l.elevGndX, l.elevGndY, l.elevGndW, l.elevGndH, "Floor");

            // Dimension Adjusters [-] [+]
            LabFont::drawText(l.rightX + 12.0f, l.dimBtnsY - 18.0f, "Adjust Size (X / Y / Z):", 1.7f, textLight, LabFontType::System);

            // X
            drawDragRow(l, 1);

            // Rotation Adjusters (Pitch / Yaw / Roll)
            LabFont::drawText(l.rightX + 12.0f, l.rotBtnsY - 18.0f, "Rotate Selection (P / Y / R):", 1.7f, orangeGlow, LabFontType::System);
            drawDragRow(l, 2);

            // Deselect button
            drawDarkButton(l.deselX, l.deselY, l.deselW, l.deselH, "Deselect");

            // Delete Selected button
            drawDarkButton(l.delX, l.delY, l.delW, l.delH, "Delete", false, true);

            // Dedicated Action Buttons (Set "Klocek1" & Assign Script) cleanly placed below Delete button!
            float actBtnY = l.delY + l.delH + 10.0f;
            drawDarkButton(l.rightX + 10.0f, actBtnY, 135.0f, 24.0f, "Set \"Klocek1\"");
            drawDarkButton(l.rightX + 155.0f, actBtnY, 135.0f, 24.0f, "Assign");
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

        bool is3D = (_workspace == EditorWorkspace::World3D_Level || _workspace == EditorWorkspace::World3D_Animation);
        bool isScr = (_workspace == EditorWorkspace::Scripting_Editor || _workspace == EditorWorkspace::Scripting_Console);
        bool isMv = (_workspace == EditorWorkspace::World3D_ModelViewer);
        bool isMods = (_workspace == EditorWorkspace::Plugins_Manager);

        // Tab 1: [ 3D Level Viewport ]
        float tab1X = vpX + 6.0f;
        float tab1W = 160.0f;
        Renderer::drawRect(tab1X, vpY + 2.0f, tab1W, tabH - 4.0f, is3D ? Vec3(0.20f, 0.24f, 0.30f) : Vec3(0.12f, 0.13f, 0.16f));
        drawDarkBevel(tab1X, vpY + 2.0f, tab1W, tabH - 4.0f, is3D);
        if (is3D) Renderer::drawRect(tab1X, vpY + tabH - 2.0f, tab1W, 2.0f, orangeGlow);
        LabFont::drawText(tab1X + 16.0f, vpY + 6.0f, "3D Level Viewport", 1.35f, is3D ? Vec3(1, 1, 1) : textDim, LabFontType::System);

        // Tab 2: [ Prop / Model Viewer ]
        float tab2X = tab1X + tab1W + 6.0f;
        float tab2W = 170.0f;
        Renderer::drawRect(tab2X, vpY + 2.0f, tab2W, tabH - 4.0f, isMv ? Vec3(0.20f, 0.24f, 0.30f) : Vec3(0.12f, 0.13f, 0.16f));
        drawDarkBevel(tab2X, vpY + 2.0f, tab2W, tabH - 4.0f, isMv);
        if (isMv) Renderer::drawRect(tab2X, vpY + tabH - 2.0f, tab2W, 2.0f, cyanGlow);
        LabFont::drawText(tab2X + 14.0f, vpY + 6.0f, "Prop / Model Viewer", 1.35f, isMv ? Vec3(1, 1, 1) : textDim, LabFontType::System);

        // Tab 3: [ Lua Script Studio ]
        float tab3X = tab2X + tab2W + 6.0f;
        float tab3W = 150.0f;
        Renderer::drawRect(tab3X, vpY + 2.0f, tab3W, tabH - 4.0f, isScr ? Vec3(0.20f, 0.24f, 0.30f) : Vec3(0.12f, 0.13f, 0.16f));
        drawDarkBevel(tab3X, vpY + 2.0f, tab3W, tabH - 4.0f, isScr);
        if (isScr) Renderer::drawRect(tab3X, vpY + tabH - 2.0f, tab3W, 2.0f, cyanGlow);
        LabFont::drawText(tab3X + 14.0f, vpY + 6.0f, "Lua Script Studio", 1.35f, isScr ? Vec3(1, 1, 1) : textDim, LabFontType::System);

        // Tab 4: [ Mods ] (Dedicated Addons & Packages Explorer)
        float tabModsX = tab3X + tab3W + 6.0f;
        float tabModsW = 110.0f;
        Renderer::drawRect(tabModsX, vpY + 2.0f, tabModsW, tabH - 4.0f, isMods ? Vec3(0.20f, 0.24f, 0.30f) : Vec3(0.12f, 0.13f, 0.16f));
        drawDarkBevel(tabModsX, vpY + 2.0f, tabModsW, tabH - 4.0f, isMods);
        if (isMods) Renderer::drawRect(tabModsX, vpY + tabH - 2.0f, tabModsW, 2.0f, orangeGlow);
        LabFont::drawText(tabModsX + 26.0f, vpY + 6.0f, "Mods", 1.35f, isMods ? Vec3(1, 1, 1) : textDim, LabFontType::System);

        // Tab 5: [ Play (F9) ]
        float tabRunX = tabModsX + tabModsW + 6.0f;
        float tabRunW = 110.0f;
        Renderer::drawRect(tabRunX, vpY + 2.0f, tabRunW, tabH - 4.0f, Vec3(0.16f, 0.32f, 0.20f));
        drawDarkBevel(tabRunX, vpY + 2.0f, tabRunW, tabH - 4.0f, false);
        Renderer::drawRect(tabRunX, vpY + tabH - 2.0f, tabRunW, 2.0f, Vec3(0.25f, 0.85f, 0.40f));
        LabFont::drawText(tabRunX + 16.0f, vpY + 6.0f, "Play (F9)", 1.35f, Vec3(0.40f, 1.0f, 0.55f), LabFontType::System);

        float contentY = vpY + tabH;
        float contentH = vpH - tabH;

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
            drawDarkButton(inspX + 10.0f, curIY, 220.0f, 26.0f, "Reset");

            curIY += 34.0f;
            drawDarkButton(inspX + 10.0f, curIY, 220.0f, 26.0f, "Apply", false, false, true);

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
            drawDarkButton(listX + 6.0f, newBtnY, scriptListW - 12.0f, 22.0f, "+ Script");

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
            drawDarkButton(codeX, actY, 150.0f, 26.0f, "Run (Ctrl+R)", false, false, true);
            drawDarkButton(codeX + 156.0f, actY, 110.0f, 26.0f, "SAVE (Ctrl+S)", false, false, true);
            drawDarkButton(codeX + 272.0f, actY, 140.0f, 26.0f, "Klocek+Timer");
            drawDarkButton(codeX + 418.0f, actY, 130.0f, 26.0f, "Player HUD");
            drawDarkButton(codeX + 554.0f, actY, 120.0f, 26.0f, "Movement");
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
            Renderer::drawRect(vpX, contentY, vpW, contentH, Vec3(0.06f, 0.07f, 0.09f));
            drawDarkBevel(vpX, contentY, vpW, contentH, false);
            float barY = contentY + 4.0f;
            float barH = 34.0f;
            Renderer::drawRect(vpX + 4.0f, barY, vpW - 8.0f, barH, Vec3(0.10f, 0.12f, 0.16f));
            drawDarkBevel(vpX + 4.0f, barY, vpW - 8.0f, barH, false);
            LabFont::drawText(vpX + 16.0f, barY + 9.0f, "MODS & ADDONS EXPLORER (/addons/ VFS LAYER MANAGER)", 1.50f, orangeGlow, LabFontType::System);
            drawDarkButton(vpX + 420.0f, barY + 4.0f, 120.0f, 26.0f, "+ New Mod", false, false, true);
            drawDarkButton(vpX + 550.0f, barY + 4.0f, 100.0f, 26.0f, "Refresh");

            if (_editorDiscoveredAddons.empty()) {
                _editorDiscoveredAddons = VFS::discoverAddons("addons");
            }
            for (size_t i = 0; i < _editorDiscoveredAddons.size() && i < 6; ++i) {
                const auto& mod = _editorDiscoveredAddons[i];
                float cardY = barY + 46.0f + static_cast<float>(i) * 68.0f;
                Renderer::drawRect(vpX + 16.0f, cardY, 664.0f, 60.0f, mod.enabled ? Vec3(0.11f, 0.14f, 0.19f) : Vec3(0.08f, 0.09f, 0.11f));
                drawDarkBevel(vpX + 16.0f, cardY, 664.0f, 60.0f, mod.enabled);
                Renderer::drawRect(vpX + 16.0f, cardY, 4.0f, 60.0f, mod.enabled ? orangeGlow : textDim);
                LabFont::drawText(vpX + 28.0f, cardY + 8.0f, mod.title + " v" + mod.version + " (/addons/" + mod.folderName + ")", 1.45f, mod.enabled ? orangeGlow : textDim, LabFontType::System);
                LabFont::drawText(vpX + 28.0f, cardY + 26.0f, "Author: " + mod.author + " | Startup Hook: " + (mod.startupScript.empty() ? "None" : mod.startupScript), 1.25f, cyanGlow, LabFontType::System);
                LabFont::drawText(vpX + 28.0f, cardY + 42.0f, mod.description, 1.25f, textLight, LabFontType::System);
                drawDarkButton(vpX + 570.0f, cardY + 16.0f, 96.0f, 26.0f, mod.enabled ? "Mounted" : "Disabled", mod.enabled, false, mod.enabled);
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
            float botBtnY = mainY + mainH - 62.0f;
            bool isNew3D = (_hubNewProjectDimension == "3D");
            bool isNew2D = (_hubNewProjectDimension == "2D");
            LabFont::drawText(listX + 12.0f, botBtnY + 4.0f, "New Project Mode:", 1.18f, textDim, LabFontType::System);
            drawDarkButton(listX + 130.0f, botBtnY, 115.0f, 22.0f, "3D World", isNew3D);
            drawDarkButton(listX + 252.0f, botBtnY, 125.0f, 22.0f, "2D Canvas", isNew2D);

            float row2Y = botBtnY + 26.0f;
            drawDarkButton(listX + 10.0f, row2Y, 215.0f, 32.0f, (_hubNewProjectDimension == "2D" ? "+ Create 2D" : "+ Create 3D"), false, false, true);
            drawDarkButton(listX + 235.0f, row2Y, 145.0f, 32.0f, "Rescan Disk", false, false, true);

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

                std::string metaLine = "Dimension: [" + (proj.dimension.empty() ? "3D" : proj.dimension) + "] | Author: " + proj.author + " | Resolution: " + std::to_string(proj.display.width > 0 ? proj.display.width : 1280) + "x" + std::to_string(proj.display.height > 0 ? proj.display.height : 720) + " | " + (proj.isBuiltinShowcase ? "Flagship Engine Showcase" : "User Game Project");
                LabFont::drawText(detX + 20.0f, dy, metaLine, 1.25f, textDim, LabFontType::System);
                dy += 24.0f;

                // Action Buttons Bar
                // [ OPEN IN EDITOR (CONTINUE) ]
                drawDarkButton(detX + 20.0f, dy, 260.0f, 42.0f, "Open", false, false, true);
                Renderer::drawRect(detX + 20.0f, dy + 40.0f, 260.0f, 2.0f, greenAccent);

                // [ PLAY STANDALONE DEMO ]
                if (proj.hasStandaloneDemo) {
                    drawDarkButton(detX + 295.0f, dy, 250.0f, 42.0f, "Play Demo", false, false, true);
                    Renderer::drawRect(detX + 295.0f, dy + 40.0f, 250.0f, 2.0f, cyanGlow);
                }
                dy += 54.0f;

                // Secondary row of buttons
                drawDarkButton(detX + 20.0f, dy, 180.0f, 34.0f, "Open Folder", false, false, true);
                drawDarkButton(detX + 210.0f, dy, 180.0f, 34.0f, "Export", false, false, true);
                drawDarkButton(detX + 400.0f, dy, 170.0f, 34.0f, "Settings", false, false, true);
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
        } else if (_workspace == EditorWorkspace::Project_Settings) {
            // ==================== PROJECT SETTINGS & CONFIGURATION ====================
            Renderer::drawRect(vpX, contentY, vpW, contentH, Vec3(0.06f, 0.07f, 0.09f));
            drawDarkBevel(vpX, contentY, vpW, contentH, false);

            // 1. Top Header Banner
            float headY = contentY + 4.0f;
            float headH = 50.0f;
            Renderer::drawRect(vpX + 4.0f, headY, vpW - 8.0f, headH, Vec3(0.10f, 0.12f, 0.16f));
            drawDarkBevel(vpX + 4.0f, headY, vpW - 8.0f, headH, false);

            LabFont::drawText(vpX + 18.0f, headY + 10.0f, "PROJECT SETTINGS & ENGINE DISPLAY CONFIGURATION", 1.6f, cyanGlow, LabFontType::System);
            LabFont::drawText(vpX + 18.0f, headY + 28.0f, "Active Project: [" + ProjectManager::get().getActiveProjectName() + "] | Isolated Architecture | Saved to project.labproject & game.config", 1.25f, textDim, LabFontType::System);

            float pX = vpX + 24.0f;
            float pW = vpW - 48.0f;
            float curY = headY + headH + 16.0f;

            // Status Banner if any
            if (!_settingsStatusBanner.empty()) {
                Renderer::drawRect(pX, curY, pW, 32.0f, Vec3(0.12f, 0.32f, 0.20f));
                drawDarkBevel(pX, curY, pW, 32.0f, false);
                LabFont::drawText(pX + 14.0f, curY + 8.0f, _settingsStatusBanner, 1.4f, Vec3(0.6f, 1.0f, 0.7f), LabFontType::System);
                curY += 42.0f;
            }

            // Section 1: Dimension & Game Mode
            Renderer::drawRect(pX, curY, pW, 80.0f, Vec3(0.09f, 0.10f, 0.13f));
            drawDarkBevel(pX, curY, pW, 80.0f, false);
            LabFont::drawText(pX + 16.0f, curY + 10.0f, "PROJECT DIMENSION & RENDERING PIPELINE", 1.4f, orangeGlow, LabFontType::System);
            LabFont::drawText(pX + 16.0f, curY + 30.0f, "Select primary coordinate space and camera projection:", 1.2f, textDim, LabFontType::System);

            bool isDim3D = (_settingsDimension == "3D");
            bool isDim2D = (_settingsDimension == "2D");
            drawDarkButton(pX + 16.0f, curY + 48.0f, 220.0f, 26.0f, "3D World", isDim3D);
            drawDarkButton(pX + 246.0f, curY + 48.0f, 260.0f, 26.0f, "2D Canvas", isDim2D);
            curY += 92.0f;

            // Section 2: Display & Window Resolution
            Renderer::drawRect(pX, curY, pW, 140.0f, Vec3(0.09f, 0.10f, 0.13f));
            drawDarkBevel(pX, curY, pW, 140.0f, false);
            LabFont::drawText(pX + 16.0f, curY + 10.0f, "WINDOW & DISPLAY OUTPUT CONFIGURATION", 1.4f, cyanGlow, LabFontType::System);

            // Resolution presets
            LabFont::drawText(pX + 16.0f, curY + 30.0f, "Resolution Presets:", 1.2f, textDim, LabFontType::System);
            bool is720p = (_settingsWidth == 1280 && _settingsHeight == 720);
            bool is1080p = (_settingsWidth == 1920 && _settingsHeight == 1080);
            bool isRetro = (_settingsWidth == 800 && _settingsHeight == 600);
            bool is1440p = (_settingsWidth == 2560 && _settingsHeight == 1440);

            drawDarkButton(pX + 16.0f, curY + 48.0f, 130.0f, 26.0f, "1280x720 (HD)", is720p);
            drawDarkButton(pX + 154.0f, curY + 48.0f, 140.0f, 26.0f, "1920x1080 (FHD)", is1080p);
            drawDarkButton(pX + 302.0f, curY + 48.0f, 130.0f, 26.0f, "800x600 (Retro)", isRetro);
            drawDarkButton(pX + 440.0f, curY + 48.0f, 140.0f, 26.0f, "2560x1440 (2K)", is1440p);

            // Fine tuning: Width & Height
            LabFont::drawText(pX + 16.0f, curY + 84.0f, "Width: " + std::to_string(_settingsWidth) + " px", 1.3f, textLight, LabFontType::System);
            drawDarkButton(pX + 170.0f, curY + 80.0f, 28.0f, 24.0f, "-");
            drawDarkButton(pX + 204.0f, curY + 80.0f, 28.0f, 24.0f, "+");

            LabFont::drawText(pX + 260.0f, curY + 84.0f, "Height: " + std::to_string(_settingsHeight) + " px", 1.3f, textLight, LabFontType::System);
            drawDarkButton(pX + 410.0f, curY + 80.0f, 28.0f, 24.0f, "-");
            drawDarkButton(pX + 444.0f, curY + 80.0f, 28.0f, 24.0f, "+");

            // Fullscreen & VSync toggles
            drawDarkButton(pX + 16.0f, curY + 110.0f, 180.0f, 24.0f, _settingsFullscreen ? "Fullscreen: ON" : "Fullscreen: OFF", _settingsFullscreen);
            drawDarkButton(pX + 204.0f, curY + 110.0f, 160.0f, 24.0f, _settingsVSync ? "VSync: ON" : "VSync: OFF", _settingsVSync);
            curY += 152.0f;

            // Section 3: Entry Scene / Default Map
            Renderer::drawRect(pX, curY, pW, 75.0f, Vec3(0.09f, 0.10f, 0.13f));
            drawDarkBevel(pX, curY, pW, 75.0f, false);
            LabFont::drawText(pX + 16.0f, curY + 10.0f, "ENTRY SCENE / DEFAULT MAP", 1.4f, orangeGlow, LabFontType::System);
            LabFont::drawText(pX + 16.0f, curY + 30.0f, "Initial map loaded on project startup:", 1.2f, textDim, LabFontType::System);
            std::string dispScene = _settingsEntryScene.empty() ? "(default starter scene)" : _settingsEntryScene;
            LabFont::drawText(pX + 16.0f, curY + 50.0f, dispScene, 1.35f, cyanGlow, LabFontType::System);
            drawDarkButton(pX + pW - 220.0f, curY + 44.0f, 200.0f, 26.0f, "Browse", false, false, true);
            curY += 88.0f;

            // Section 4: Action Buttons (Save & Apply)
            drawDarkButton(pX, curY, 240.0f, 40.0f, "Save", false, false, true);
            Renderer::drawRect(pX, curY + 38.0f, 240.0f, 2.0f, greenAccent);

            drawDarkButton(pX + 255.0f, curY, 260.0f, 40.0f, "Save & Apply", false, false, true);
            Renderer::drawRect(pX + 255.0f, curY + 38.0f, 260.0f, 2.0f, cyanGlow);

            drawDarkButton(pX + 530.0f, curY, 180.0f, 40.0f, "Back", false, false, true);
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

        std::string sbText = "G: Move | R: Rotate | S: Scale | T: Omni | RMB: Fly | LMB: Gizmo/Select | Del: Delete | F: Focus | " + _cullingStats;
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
                { "New Map", "Ctrl+N" },
                { "Open Map...", "Ctrl+O" },
                { "Save Map", "Ctrl+S" },
                { "Save Map As...", "Ctrl+Shift+S" },
                { "Run Map in Frozen-Life", "F9" },
                { "Exit", "Alt+F4" }
            };

            for (int i = 0; i < 6; ++i) {
                float iy = menuY + 3.0f + i * 24.0f;
                bool isHov = (_mouseScreenX >= menuX && _mouseScreenX <= menuX + menuW && _mouseScreenY >= iy && _mouseScreenY <= iy + 22.0f);
                if (isHov) {
                    Renderer::drawRect(menuX + 2.0f, iy, menuW - 4.0f, 22.0f, Vec3(0.24f, 0.28f, 0.36f));
                }
                if (i == 4 || i == 5) Renderer::drawRect(menuX + 6.0f, iy - 2.0f, menuW - 12.0f, 1.0f, menuBorder);
                LabFont::drawText(menuX + 14.0f, iy + 4.0f, items[i].name, 1.6f, (i == 4) ? (isHov ? Vec3(0.4f, 1.0f, 0.5f) : Vec3(0.3f, 0.9f, 0.4f)) : (isHov ? orangeGlow : textLight), LabFontType::System);
                if (!items[i].shortcut.empty()) {
                    LabFont::drawText(menuX + menuW - 95.0f, iy + 4.0f, items[i].shortcut, 1.4f, textDim, LabFontType::System);
                }
            }
        }

        // ==================== 7.5. DROPDOWN EDIT MENU ====================
        if (_editMenuOpen) {
            float menuX = 52.0f;
            float menuY = 24.0f;
            float menuW = 260.0f;
            float menuH = 128.0f;
            Vec3 menuBg{ 0.16f, 0.17f, 0.20f };
            Vec3 menuBorder{ 0.35f, 0.38f, 0.44f };

            Renderer::drawRect(menuX, menuY, menuW, menuH, menuBg);
            drawDarkBevel(menuX, menuY, menuW, menuH, false);

            std::string undoLabel = "Undo";
            if (!_undoStack.empty()) {
                undoLabel += " (" + _undoStack.back().description + ")";
            }
            std::string redoLabel = "Redo";
            if (!_redoStack.empty()) {
                redoLabel += " (" + _redoStack.back().description + ")";
            }

            struct EditMenuItem { std::string name; std::string shortcut; bool enabled; };
            EditMenuItem eItems[] = {
                { undoLabel, "Ctrl+Z", !_undoStack.empty() },
                { redoLabel, "Ctrl+Y", !_redoStack.empty() },
                { "Delete Selection", "Del", _selectionType != SelectionType::None },
                { "Duplicate Selection", "Ctrl+D", _selectionType != SelectionType::None },
                { "Deselect", "Esc", _selectionType != SelectionType::None }
            };

            for (int i = 0; i < 5; ++i) {
                float iy = menuY + 3.0f + i * 24.0f;
                bool isHov = (_mouseScreenX >= menuX && _mouseScreenX <= menuX + menuW && _mouseScreenY >= iy && _mouseScreenY <= iy + 22.0f);
                if (isHov && eItems[i].enabled) {
                    Renderer::drawRect(menuX + 2.0f, iy, menuW - 4.0f, 22.0f, Vec3(0.24f, 0.28f, 0.36f));
                }
                if (i == 2) Renderer::drawRect(menuX + 6.0f, iy - 2.0f, menuW - 12.0f, 1.0f, menuBorder);

                Vec3 col = eItems[i].enabled ? (isHov ? orangeGlow : textLight) : Vec3(0.45f, 0.47f, 0.50f);
                LabFont::drawText(menuX + 14.0f, iy + 4.0f, eItems[i].name, 1.45f, col, LabFontType::System);
                if (!eItems[i].shortcut.empty()) {
                    LabFont::drawText(menuX + menuW - 75.0f, iy + 4.0f, eItems[i].shortcut, 1.35f, textDim, LabFontType::System);
                }
            }
        }

        // ==================== 8. DROPDOWN MAP MENU ====================
        if (_buildMenuOpen) {
            float menuX = 138.0f;
            float menuY = 24.0f;
            float menuW = 260.0f;
            float menuH = 125.0f;
            Vec3 menuBg{ 0.16f, 0.17f, 0.20f };
            Vec3 menuBorder{ 0.35f, 0.38f, 0.44f };

            Renderer::drawRect(menuX, menuY, menuW, menuH, menuBg);
            drawDarkBevel(menuX, menuY, menuW, menuH, false);

            struct MapMenuItem { std::string name; std::string shortcut; };
            MapMenuItem mItems[] = {
                { "Run Map in Frozen-Life", "F9" },
                { "Apply Darkness Preset", "" },
                { "Apply Sunlight Preset", "" },
                { "Clear All Map Geometry", "" },
                { "Entity Outliner / Spawns", "" }
            };

            for (int i = 0; i < 5; ++i) {
                float iy = menuY + 3.0f + i * 24.0f;
                bool isHov = (_mouseScreenX >= menuX && _mouseScreenX <= menuX + menuW && _mouseScreenY >= iy && _mouseScreenY <= iy + 22.0f);
                if (isHov) {
                    Renderer::drawRect(menuX + 2.0f, iy, menuW - 4.0f, 22.0f, Vec3(0.24f, 0.28f, 0.36f));
                }
                LabFont::drawText(menuX + 14.0f, iy + 4.0f, mItems[i].name, 1.5f, (i == 0) ? (isHov ? Vec3(0.4f, 1.0f, 0.5f) : Vec3(0.3f, 0.9f, 0.4f)) : (isHov ? orangeGlow : textLight), LabFontType::System);
                if (!mItems[i].shortcut.empty()) {
                    LabFont::drawText(menuX + menuW - 80.0f, iy + 4.0f, mItems[i].shortcut, 1.4f, textDim, LabFontType::System);
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
                "Audio Source",
                "RigidBody Dynamic",
                "Box Collider",
                "Verlet Cloth",
                "Water Volume",
                "Force Field",
                "Arcade Vehicle",
                "Decal Projected",
                "Post-Process",
                "Particle Emitter"
            };

            // Filter components
            std::vector<std::string> filtered;
            std::string q = _contextMenuSearch;
            std::transform(q.begin(), q.end(), q.begin(), [](unsigned char ch) { return (char)::tolower(ch); });
            for (const auto* c : allComponents) {
                std::string s(c);
                std::string sLow = s;
                std::transform(sLow.begin(), sLow.end(), sLow.begin(), [](unsigned char ch) { return (char)::tolower(ch); });
                if (q.empty() || sLow.find(q) != std::string::npos) {
                    filtered.push_back(s);
                }
            }

            size_t maxItems = std::min((size_t)12, filtered.size());
            float ctxH = 60.0f + (float)maxItems * 24.0f;
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
            for (size_t i = 0; i < maxItems; ++i) {
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
        drawDarkButton(bx + 20.0f, by + 45.0f, 300.0f, 34.0f, "Browse", false, false, true);

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
        LabFont::drawText(b1X + 20.0f, btnY + 10.0f, "Build", 1.45f, Vec3(1, 1, 1), LabFontType::System);

        // 2. [ BUILD & RUN ]
        float b2X = b1X + b1W + 14.0f;
        float b2W = 220.0f;
        bool b2Hov = (_mouseScreenX >= b2X && _mouseScreenX <= b2X + b2W && _mouseScreenY >= btnY && _mouseScreenY <= btnY + 36.0f);
        Renderer::drawRect(b2X, btnY, b2W, 36.0f, b2Hov ? Vec3(0.24f, 0.85f, 0.44f) : Vec3(0.18f, 0.68f, 0.34f));
        drawDarkBevel(b2X, btnY, b2W, 36.0f, false);
        LabFont::drawText(b2X + 22.0f, btnY + 10.0f, "Build & Run", 1.45f, Vec3(1, 1, 1), LabFontType::System);

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
        drawDarkButton(bx + 24.0f, btnY, 160.0f, 32.0f, "Save", false, false, true);
        drawDarkButton(bx + 194.0f, btnY, 160.0f, 32.0f, "Load", false, false, true);
        drawDarkButton(bx + 364.0f, btnY, 150.0f, 32.0f, "Reset", false, false, true);
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
        drawDarkButton(bx + 20.0f, btnY, 160.0f, 32.0f, "Slice", false, false, true);
        drawDarkButton(bx + 194.0f, btnY, 190.0f, 32.0f, "Place", false, false, true);
        drawDarkButton(bx + 398.0f, btnY, 170.0f, 32.0f, "Save", false, false, true);
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

    // Modal Session Crash Recovery Dialog (%TEMP% Session Restoration)
    void drawSessionRecoveryModal() {
        int fbW = 0, fbH = 0;
        getFramebufferSize(fbW, fbH);
        float w = (fbW > 0) ? (float)fbW : (float)getWidth();
        float h = (fbH > 0) ? (float)fbH : (float)getHeight();

        Renderer::beginUI((int)w, (int)h);

        // Dark modal overlay
        Renderer::drawRect(0, 0, w, h, Vec3(0.03f, 0.04f, 0.06f));

        float bw = 580.0f;
        float bh = 240.0f;
        float bx = (w - bw) * 0.5f;
        float by = (h - bh) * 0.5f;

        LabTheme::drawPanel(bx, by, bw, bh, "Przywracanie Poprzedniej Sesji (Crash Recovery)");
        drawDarkButton(bx + bw - 32.0f, by + 5.0f, 24.0f, 22.0f, "X", false, true);

        std::string fname = std::filesystem::path(_recoveryMapName).filename().string();
        if (fname.empty()) fname = "Niezapisana mapa (Untitled)";

        LabFont::drawText(bx + 24.0f, by + 46.0f, "Wykryto niezapisana poprzednia sesje programu Hammer:", 1.45f, Vec3(1.0f, 0.85f, 0.35f), LabFontType::System);
        LabFont::drawText(bx + 24.0f, by + 72.0f, "Mapa: " + fname + " | Data: " + _recoveryTimestamp, 1.45f, Vec3(0.4f, 0.85f, 1.0f), LabFontType::System);
        LabFont::drawText(bx + 24.0f, by + 98.0f, "Liczba zarejestrowanych operacji w sesji: " + std::to_string(_recoveryActionCount), 1.35f, Vec3(0.85f, 0.88f, 0.92f), LabFontType::System);
        LabFont::drawText(bx + 24.0f, by + 124.0f, "Czy chcesz przywrocic stan z pliku tymczasowego %TEMP%?", 1.35f, Vec3(0.95f, 0.95f, 0.98f), LabFontType::System);

        // Buttons
        float btnY = by + bh - 54.0f;
        float btnW = 200.0f;
        float btnH = 34.0f;

        // "Przywróć sesję (Y / F1)"
        float restX = bx + 45.0f;
        Renderer::drawRect(restX, btnY, btnW, btnH, Vec3(0.18f, 0.50f, 0.28f));
        drawDarkBevel(restX, btnY, btnW, btnH, false);
        LabFont::drawText(restX + 22.0f, btnY + 8.0f, "Przywroc sesje [Y]", 1.45f, Vec3(1, 1, 1), LabFontType::System);

        // "Odrzuć sesję (N / Esc)"
        float discX = bx + bw - 45.0f - btnW;
        Renderer::drawRect(discX, btnY, btnW, btnH, Vec3(0.48f, 0.18f, 0.18f));
        drawDarkBevel(discX, btnY, btnW, btnH, false);
        LabFont::drawText(discX + 28.0f, btnY + 8.0f, "Odrzuc sesje [N]", 1.45f, Vec3(1, 1, 1), LabFontType::System);

        Renderer::endUI();
    }

    void onShutdown() override {
        // Record clean exit in session journal & cleanup autosave if not dirty
        if (!_isDirty) {
            writeSessionJournal("clean_exit", _currentMapPath, _sessionActionCounter);
            auto autoPath = getAutosavePath();
            std::error_code ec;
            std::filesystem::remove(autoPath, ec);
        } else {
            // Keep autosave so next launch can offer to restore it
            saveSessionAutosave();
            writeSessionJournal("in_progress", _currentMapPath, _sessionActionCounter);
        }
        _decalSystem.shutdown();
        _particleSystem.shutdown();
        AudioEngine::shutdown();
        Renderer::shutdown();
    }

private:
    Camera _camera;
    std::unique_ptr<LabMap> _map;
    std::unique_ptr<BakedLightmap> _bakedLightmap;
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

    // Session Crash Recovery & %TEMP% Journal
    bool _showRecoveryPrompt = false;
    std::string _recoveryMapName = "";
    std::string _recoveryTimestamp = "";
    int _recoveryActionCount = 0;
    int _sessionActionCounter = 0;
    std::string _sessionStartTime = "";
    bool _isDirty = false;

    // Undo / Redo Stacks (C++20 Value Snapshotting)
    std::vector<HammerSnapshot> _undoStack;
    std::vector<HammerSnapshot> _redoStack;

    SpriteSheet2D _editorSpriteSheet;
    int _sliceCols = 4;
    int _sliceRows = 4;
    int _selectedSpriteFrame = 0;
    ExportSettings _exportSettings;
    std::string _lastBuildLog = "";
    bool _buildSuccess = false;
    bool _fileMenuOpen = false;
    bool _editMenuOpen = false;
    bool _buildMenuOpen = false;
    bool _viewMenuOpen = false;
    bool _wireframeMode = false;
    std::string _currentMapPath = "";

    // Selection State
    SelectionType _selectionType = SelectionType::None;
    int _selectedIndex = -1;
    std::string _defaultWaypointAnimation = "Idle";
    float _defaultWaypointYaw = 0.0f;
    float _defaultWaypointDuration = 2.0f;
    std::string _defaultWaypointBotName = "Guard";

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
    bool _retroScanlines = false;
    int _retroPixelScale = 2; // 2, 3, 4
    float _retroScanlineOpacity = 0.28f;
    bool _retroAberration = false;
    float _retroAberrationAmount = 0.0035f;
    bool _retroVignette = false;
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
    std::string _hubNewProjectDimension = "3D";

    // Project Settings Workspace State
    std::string _settingsProjectTitle = "";
    std::string _settingsDimension = "3D";
    int _settingsWidth = 1280;
    int _settingsHeight = 720;
    bool _settingsFullscreen = false;
    bool _settingsVSync = true;
    std::string _settingsEntryScene = "";
    std::string _settingsStatusBanner = "";
    float _settingsBannerTimer = 0.0f;

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
    // Blender-style drag fields in the properties sidebar
    int   _dragRow = -1, _dragAxis = -1;
    float _dragLastX = 0.0f, _dragPressX = 0.0f, _dragCenterX = 0.0f, _dragAccum = 0.0f;
    bool  _dragMoved = false;
    // Blender-style direct keyboard text entry in numeric fields
    int         _fieldEditRow = -1, _fieldEditAxis = -1;
    std::string _fieldEditBuffer = "";
    float       _fieldEditCursorBlink = 0.0f;
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
    bool _ctrlZPressed = false;
    bool _ctrlYPressed = false;
    bool _delPressed = false;
    bool _fPressed = false;
    bool _f9Pressed = false;
    bool _f5Pressed = false;
    bool _ctrlBPressed = false;
    bool _bPressed = false;
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
    bool _modelViewerAutoRotate = false;
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
    bool _quickFxMenuOpen = false;

    // Native Engine Subsystems Integration
    DecalSystem _decalSystem;
    ParticleSystem _particleSystem;
    std::vector<AddonManifest> _editorDiscoveredAddons;
};

int main(int argc, char** argv) {
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-game" || arg == "--game") {
            if (i + 1 < argc) {
                VFS::get().mountAddonLayer(argv[++i]);
            }
            continue;
        }
    }

    LabEditorApp editor;
    editor.run();
    return 0;
}
