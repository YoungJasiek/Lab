#include "Lab.h"
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
    Sprite
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
    Studio_ReloadTimeline = 4, // LabStudio: Reload Animation Choreography Timeline
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
    Plugins_Manager = 13,      // Full-viewport Package & Plugin Manager (1 kod 1 plugin)
    // Aliases for backwards compatibility
    World3D = 0,
    Level2D = 8,
    HUD = 10,
    Animation = 1,
    Scripting = 11,
    Plugins = 13
};

enum class HUDWidgetType {
    TextLabel,
    ProgressBar,
    Button,
    Icon,
    Panel,
    MinimapFrame,
    DialogueBox
};

struct HUDDesignWidget {
    std::string id;
    std::string label;
    HUDWidgetType type{ HUDWidgetType::TextLabel };
    float x{ 100.0f };
    float y{ 100.0f };
    float w{ 200.0f };
    float h{ 40.0f };
    Vec3 color{ 1.0f, 1.0f, 1.0f };
    Vec3 bgColor{ 0.15f, 0.18f, 0.22f };
    float currentValue{ 100.0f };
    float maxValue{ 100.0f };
    std::string bindingKey{ "" };
    std::string genreTag{ "General" }; // RPG, Platformer, RTS, Racing, Story, General
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

        // Initialize embedded LabStudio suite
        _characterStudio.init();
        _characterStudio.setWindow(getWindow());

        // Initialize Multi-Genre HUD Widgets (RPG, Platformer, RTS, Racing, Story)
        initDefaultHUDWidgets();

        // Initialize Package & Plugin System (drop-in: 1 plik .lua = 1 plugin)
        PackageManager::instance().registerBuiltinPackages();
        logMessage("[Plugins] Drop-in plugin system ready: wrzuc .lua do plugins/ lub przeciagniej na okno.");

        logMessage("Lab Engine Studio initialized. Ready.");
        logMessage("Textures loaded: " + std::to_string(_availableTextures.size()) + " | Models: " + std::to_string(_availableModels.size()));
        logMessage("Lua 5.4 Script Engine active. Workspaces: 3D, 2D, HUD, Animation, Scripting, Plugins.");
        logMessage("Frustum Culling active: 'To czego oko nie widzi tego maszyna renderowac nie musi'");
    }

    void initDefaultHUDWidgets() {
        _hudWidgets.clear();
        // 1. RPG / Action Widgets
        _hudWidgets.push_back({ "HealthBar", "HEALTH", HUDWidgetType::ProgressBar, 60.0f, 960.0f, 260.0f, 26.0f, Vec3(0.2f, 0.85f, 0.35f), Vec3(0.12f, 0.14f, 0.18f), 100.0f, 100.0f, "Health", "RPG" });
        _hudWidgets.push_back({ "ManaBar", "MANA", HUDWidgetType::ProgressBar, 60.0f, 995.0f, 260.0f, 20.0f, Vec3(0.2f, 0.6f, 0.95f), Vec3(0.12f, 0.14f, 0.18f), 85.0f, 100.0f, "Mana", "RPG" });
        _hudWidgets.push_back({ "XPBar", "XP", HUDWidgetType::ProgressBar, 60.0f, 1025.0f, 260.0f, 12.0f, Vec3(0.95f, 0.75f, 0.15f), Vec3(0.12f, 0.14f, 0.18f), 450.0f, 1000.0f, "XP", "RPG" });

        // 2. Platformer / Arcade Widgets
        _hudWidgets.push_back({ "ScoreText", "SCORE: 014500", HUDWidgetType::TextLabel, 60.0f, 60.0f, 220.0f, 32.0f, Vec3(1.0f, 0.95f, 0.3f), Vec3(0.10f, 0.12f, 0.16f), 14500.0f, 99999.0f, "Score", "Platformer" });
        _hudWidgets.push_back({ "CoinCount", "COINS: x42", HUDWidgetType::TextLabel, 300.0f, 60.0f, 160.0f, 32.0f, Vec3(1.0f, 0.8f, 0.1f), Vec3(0.10f, 0.12f, 0.16f), 42.0f, 99.0f, "Coins", "Platformer" });
        _hudWidgets.push_back({ "LivesDisplay", "LIVES: <3 <3 <3", HUDWidgetType::TextLabel, 480.0f, 60.0f, 180.0f, 32.0f, Vec3(0.95f, 0.25f, 0.35f), Vec3(0.10f, 0.12f, 0.16f), 3.0f, 3.0f, "Lives", "Platformer" });

        // 3. Racing / Vehicle Widgets
        _hudWidgets.push_back({ "Speedometer", "142 KM/H", HUDWidgetType::TextLabel, 1620.0f, 940.0f, 240.0f, 44.0f, Vec3(0.2f, 0.9f, 0.95f), Vec3(0.08f, 0.10f, 0.14f), 142.0f, 280.0f, "Speedometer", "Racing" });
        _hudWidgets.push_back({ "LapTimer", "LAP 1 - 01:24.5", HUDWidgetType::TextLabel, 840.0f, 60.0f, 240.0f, 36.0f, Vec3(1.0f, 0.9f, 0.2f), Vec3(0.10f, 0.12f, 0.16f), 84.5f, 999.0f, "LapTimer", "Racing" });

        // 4. Strategy / RTS Widgets
        _hudWidgets.push_back({ "ResourceBar", "GOLD: 1200 | WOOD: 450 | FOOD: 80/100", HUDWidgetType::TextLabel, 1200.0f, 60.0f, 380.0f, 32.0f, Vec3(0.9f, 0.95f, 1.0f), Vec3(0.12f, 0.14f, 0.18f), 1200.0f, 5000.0f, "Resources", "RTS" });
        _hudWidgets.push_back({ "MinimapFrame", "[ MINIMAP RADAR ]", HUDWidgetType::MinimapFrame, 60.0f, 780.0f, 180.0f, 160.0f, Vec3(0.3f, 0.85f, 0.65f), Vec3(0.06f, 0.08f, 0.10f), 1.0f, 1.0f, "Minimap", "RTS" });

        // 5. Story / Adventure Widgets
        _hudWidgets.push_back({ "DialogueBox", "Scientist: The containment field is holding.", HUDWidgetType::DialogueBox, 460.0f, 880.0f, 1000.0f, 75.0f, Vec3(0.95f, 0.95f, 0.95f), Vec3(0.08f, 0.10f, 0.14f), 1.0f, 1.0f, "DialogueText", "Story" });
    }

    void discoverTextures() {
        _availableTextures.clear();
        std::vector<std::string> searchDirs = { "assets/textures", "../assets/textures", "../../assets/textures" };
        for (const auto& dir : searchDirs) {
            if (std::filesystem::exists(dir)) {
                for (const auto& entry : std::filesystem::directory_iterator(dir)) {
                    std::string ext = entry.path().extension().string();
                    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return (char)::tolower(c); });
                    if (ext == ".bmp" || ext == ".png" || ext == ".jpg" || ext == ".tga") {
                        std::string fname = entry.path().filename().string();
                        _availableTextures.push_back({ fname, fname });
                        if (!_textures.contains(fname)) {
                            _textures[fname] = std::make_unique<Texture>(fname);
                        }
                    }
                }
                if (!_availableTextures.empty()) break;
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
        std::vector<std::string> searchDirs = { "assets/models", "../assets/models", "../../assets/models" };
        for (const auto& dir : searchDirs) {
            if (std::filesystem::exists(dir)) {
                for (const auto& entry : std::filesystem::directory_iterator(dir)) {
                    std::string ext = entry.path().extension().string();
                    if (ext == ".stl" || ext == ".STL") {
                        std::string fname = entry.path().filename().string();
                        _availableModels.push_back(fname);
                        if (!_meshes.contains(fname)) {
                            Mesh* m = Mesh::loadSTL(fname);
                            if (m) _meshes[fname] = std::unique_ptr<Mesh>(m);
                        }
                    }
                }
                if (!_availableModels.empty()) break;
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
        std::string openPath = LabDialogs::openFileDialog(getWindow(), "Lab Map Files (*.labmap)\0*.labmap\0All Files (*.*)\0*.*\0", "assets\\maps");
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
                logMessage("Loaded Map: " + openPath);
            }
        }
    }

    bool isStudioWorkspace(EditorWorkspace ws) const {
        return ws == EditorWorkspace::Studio_GripPoser ||
               ws == EditorWorkspace::Studio_ReloadTimeline ||
               ws == EditorWorkspace::Studio_WeaponSkins ||
               ws == EditorWorkspace::Studio_Appearance ||
               ws == EditorWorkspace::Studio_FaceDialogue;
    }

    std::string getWorkspaceName(EditorWorkspace ws) const {
        switch (ws) {
            case EditorWorkspace::World3D_Level: return "3D > Level & Geometry Editor";
            case EditorWorkspace::World3D_Animation: return "3D > Animation 3D Timeline";
            case EditorWorkspace::World3D_ModelViewer: return "3D > 3D Model Viewer & Inspector";
            case EditorWorkspace::Studio_GripPoser: return "3D > Character & Weapon Studio > Weapon Grip Poser";
            case EditorWorkspace::Studio_ReloadTimeline: return "3D > Character & Weapon Studio > Reload Timeline";
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
            case EditorWorkspace::World3D_Level: return "3D > Level";
            case EditorWorkspace::World3D_Animation: return "3D > Anim 3D";
            case EditorWorkspace::World3D_ModelViewer: return "3D > Model Viewer";
            case EditorWorkspace::Studio_GripPoser: return "Studio > Grip Poser";
            case EditorWorkspace::Studio_ReloadTimeline: return "Studio > Reload";
            case EditorWorkspace::Studio_WeaponSkins: return "Studio > Skins";
            case EditorWorkspace::Studio_Appearance: return "Studio > Appearance";
            case EditorWorkspace::Studio_FaceDialogue: return "Studio > Dialogue";
            case EditorWorkspace::Level2D_Tilemap: return "2D > Level";
            case EditorWorkspace::Level2D_Animation: return "2D > Anim 2D";
            case EditorWorkspace::Level2D_HUD: return "2D > HUD Canvas";
            case EditorWorkspace::Scripting_Editor: return "Script > Lua IDE";
            case EditorWorkspace::Scripting_Console: return "Script > Console";
            default: return "3D > Level";
        }
    }

    void setWorkspace(EditorWorkspace ws) {
        _workspace = ws;
        if (ws == EditorWorkspace::World3D_Level || ws == EditorWorkspace::World3D_Animation || ws == EditorWorkspace::World3D_ModelViewer) {
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
        } else if (ws == EditorWorkspace::Studio_ReloadTimeline) {
            _characterStudio.setActiveTab(StudioTab::ReloadTimeline);
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
        }

        logMessage("Workspace switched: [ " + getWorkspaceName(ws) + " ]");
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
            std::filesystem::create_directories(std::filesystem::path(path).parent_path());
            std::ofstream out(path);
            if (out.is_open()) {
                out << _currentScript;
                out.close();
                logMessage("Script saved to " + path);
            }
        } catch (const std::exception& e) {
            logMessage("Failed to save script: " + std::string(e.what()));
        }
    }

    void loadScriptFromFile(const std::string& path) {
        if (!std::filesystem::exists(path)) return;
        std::ifstream in(path);
        if (in.is_open()) {
            std::stringstream ss;
            ss << in.rdbuf();
            _currentScript = ss.str();
            syncStringToScriptLines();
            logMessage("Loaded script from " + path);
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
            std::string savePath = LabDialogs::saveFileDialog(getWindow(), "Lab Map Files (*.labmap)\0*.labmap\0All Files (*.*)\0*.*\0", "my_level.labmap", "assets\\maps");
            if (!savePath.empty()) {
                _currentMapPath = savePath;
                _map->saveToFile(_currentMapPath);
                logMessage("Map saved to: " + _currentMapPath);
            }
        } else {
            _map->saveToFile(_currentMapPath);
            logMessage("Map saved to: " + _currentMapPath);
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

        // Handle Right-Click (for Tool 3 Pipette sample or Hierarchy Context Menu)
        if (Input::isMouseButtonJustPressed(1) || (Input::isMouseButtonPressed(1) && !_rmbPressed)) {
            if (_activeTool == 3) {
                handleMouseClick(mx, my, true);
            } else if (!in3DViewport || _contextMenuOpen || mx >= vpX + vpW) {
                handleMouseClick(mx, my, true);
            }
            _rmbPressed = true;
        } else if (!Input::isMouseButtonPressed(1)) {
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

        // Simulation Tick (Lua OnUpdate & in-editor game mechanics)
        if (_simState == EngineSimState::Playing) {
            if (_map) {
                _scriptEngine.setActiveMap(_map.get());
            }
            _scriptEngine.update(time.delta);
        }

        // Timeline playback in Animation Workspace
        if (_timelinePlaying) {
            static float animTimer = 0.0f;
            animTimer += time.delta;
            if (animTimer >= 1.0f / 30.0f) {
                animTimer = 0.0f;
                _timelineFrame = (_timelineFrame + 1) % 60;
            }
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

            bool upElevateKey = Input::isKeyPressed(266) || Input::isKeyPressed(82) || Input::isKeyPressed(334) || Input::isKeyPressed(93); // PageUp, 'R', Keypad '+', ']'
            bool dwnElevateKey = Input::isKeyPressed(267) || Input::isKeyPressed(67) || Input::isKeyPressed(333) || Input::isKeyPressed(91); // PageDown, 'C', Keypad '-', '['

            if (upElevateKey) {
                if (!_pageUpPressed) { moveSelection(0, _gridSnap, 0); _pageUpPressed = true; }
            } else _pageUpPressed = false;

            if (dwnElevateKey) {
                if (!_pageDownPressed) { moveSelection(0, -_gridSnap, 0); _pageDownPressed = true; }
            } else _pageDownPressed = false;
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

        // Interactive Gizmo Y-Handle Hit Test (Elevate selected object by clicking the vertical green arrow)
        if (_selectionType != SelectionType::None && !isRmb) {
            Vec3 selPos = getSelectedPosition();
            Vec3 yHandleMin = selPos + Vec3(-0.35f, 0.2f, -0.35f);
            Vec3 yHandleMax = selPos + Vec3(0.35f, 2.3f, 0.35f);
            float tGizmo = 0.0f;
            if (rayIntersectAABB(rayOrigin, rayDir, yHandleMin, yHandleMax, tGizmo)) {
                if (tGizmo > 0.01f && tGizmo < 300.0f) {
                    moveSelection(0.0f, _gridSnap, 0.0f);
                    return;
                }
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
        }
        return Vec3(0, 0, 0);
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
                { "Script (Lua)", "Script" },
                { "Point Light", "Light" },
                { "2D Sprite", "Sprite" },
                { "Box Collider", "Physics" },
                { "Audio Source", "Sound" },
                { "Timer Trigger", "Trigger" },
                { "Particle Emitter", "FX" },
                { "Collectible Item (Score/Coins)", "Gameplay" },
                { "Interactive Trigger / Switch", "Gameplay" },
                { "Camera Rail / Anchor", "Camera" }
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

            int itemCount = std::min((int)filtered.size(), 7);
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
                        if (picked.find("Script") != std::string::npos) {
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

        // 4. Dropdown File Menu
        if (_fileMenuOpen) {
            float menuX = 10.0f;
            float menuY = 24.0f;
            float menuW = 220.0f;
            float menuH = 175.0f;
            if (mx >= menuX && mx <= menuX + menuW && my >= menuY && my <= menuY + menuH) {
                int itemIdx = (int)((my - menuY) / 24.0f);
                if (itemIdx == 0) newMap();
                else if (itemIdx == 1) openMapDialog();
                else if (itemIdx == 2) saveMapAction(false);
                else if (itemIdx == 3) saveMapAction(true);
                else if (itemIdx == 4) buildGameAction(true);
                else if (itemIdx == 5) _buildModalOpen = true;
                else if (itemIdx >= 6) stop();
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
                        else if (subIdx == 4) setWorkspace(EditorWorkspace::Studio_ReloadTimeline);
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
                int totalEntities = (int)(totalSpawns + totalWepSpawns + _map->brushes.size() + _map->props.size() + _map->doors.size() + _map->lights.size());
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
                            } else if (itemIdx >= lOffset) {
                                _selectionType = SelectionType::Light;
                                _selectedIndex = itemIdx - lOffset;
                                logMessage("Outliner: Selected Lamp Light #" + std::to_string(_selectedIndex) + " (" + _map->lights[_selectedIndex].name + ")");
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

        // Viewport Tab Bar Click Handling (Seamless one-click switching between 3D Scene, Script Studio, HUD Canvas, and Plugins)
        float vpTabX = leftW;
        float vpTabY = 58.0f;
        float tabH = 26.0f;
        if (my >= vpTabY && my <= vpTabY + tabH && mx >= vpTabX && mx <= l.rightX) {
            float tab1X = vpTabX + 6.0f;
            float tab1W = 120.0f;
            float tab2X = tab1X + tab1W + 6.0f;
            float tab2W = 145.0f;
            float tab3X = tab2X + tab2W + 6.0f;
            float tab3W = 145.0f;
            float tab4X = tab3X + tab3W + 6.0f;
            float tab4W = 145.0f;

            if (mx >= tab1X && mx <= tab1X + tab1W) {
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

        // 8.5. Native Multi-Genre HUD Canvas Workspace Interactions
        if (_workspace == EditorWorkspace::Level2D_HUD) {
            float vpX = leftW;
            float vpW = l.rightX - vpX;
            float vpH = (h - 22.0f) - 58.0f;
            float cvW = std::min(vpW - 40.0f, 960.0f);
            float cvH = std::min(vpH - 40.0f, 540.0f);
            float cvX = vpX + (vpW - cvW) * 0.5f;
            float cvY = 58.0f + (vpH - cvH) * 0.5f;

            // Template Bar Clicks
            float tbY = cvY + 30.0f;
            if (my >= tbY && my <= tbY + 24.0f) {
                if (mx >= cvX + 10.0f && mx <= cvX + 100.0f) {
                    _hudWidgets.push_back({ "HealthBar" + std::to_string(_hudWidgets.size()), "HEALTH", HUDWidgetType::ProgressBar, 60.0f, 960.0f, 260.0f, 26.0f, Vec3(0.2f, 0.85f, 0.35f), Vec3(0.12f, 0.14f, 0.18f), 100.0f, 100.0f, "Health", "RPG" });
                    logMessage("HUD: Added Health/Mana Widget Template");
                    return;
                } else if (mx >= cvX + 105.0f && mx <= cvX + 185.0f) {
                    _hudWidgets.push_back({ "ScoreText" + std::to_string(_hudWidgets.size()), "SCORE: 0000", HUDWidgetType::TextLabel, 60.0f, 60.0f, 220.0f, 32.0f, Vec3(1.0f, 0.95f, 0.3f), Vec3(0.10f, 0.12f, 0.16f), 0.0f, 99999.0f, "Score", "Platformer" });
                    logMessage("HUD: Added Score/Coin Widget Template");
                    return;
                } else if (mx >= cvX + 190.0f && mx <= cvX + 270.0f) {
                    _hudWidgets.push_back({ "Resources" + std::to_string(_hudWidgets.size()), "GOLD: 500 | WOOD: 200", HUDWidgetType::TextLabel, 1200.0f, 60.0f, 320.0f, 30.0f, Vec3(0.9f, 0.95f, 1.0f), Vec3(0.12f, 0.14f, 0.18f), 500.0f, 9999.0f, "Resources", "RTS" });
                    logMessage("HUD: Added Strategy Resource Bar Template");
                    return;
                } else if (mx >= cvX + 275.0f && mx <= cvX + 360.0f) {
                    _hudWidgets.push_back({ "Speedometer" + std::to_string(_hudWidgets.size()), "0 KM/H", HUDWidgetType::TextLabel, 1620.0f, 940.0f, 220.0f, 40.0f, Vec3(0.2f, 0.9f, 0.95f), Vec3(0.08f, 0.10f, 0.14f), 0.0f, 300.0f, "Speedometer", "Racing" });
                    logMessage("HUD: Added Racing Speedometer Template");
                    return;
                } else if (mx >= cvX + 365.0f && mx <= cvX + 450.0f) {
                    _hudWidgets.push_back({ "Dialogue" + std::to_string(_hudWidgets.size()), "Narrator: Once upon a time in the Cryo Complex...", HUDWidgetType::DialogueBox, 460.0f, 880.0f, 1000.0f, 75.0f, Vec3(0.95f, 0.95f, 0.95f), Vec3(0.08f, 0.10f, 0.14f), 1.0f, 1.0f, "DialogueText", "Story" });
                    logMessage("HUD: Added Adventure Dialogue Box Template");
                    return;
                } else if (mx >= cvX + 455.0f && mx <= cvX + 535.0f) {
                    _hudWidgets.push_back({ "Timer" + std::to_string(_hudWidgets.size()), "TIME: 00:00", HUDWidgetType::TextLabel, 860.0f, 60.0f, 200.0f, 36.0f, Vec3(1.0f, 0.85f, 0.2f), Vec3(0.10f, 0.12f, 0.16f), 0.0f, 9999.0f, "TimerLabel", "General" });
                    logMessage("HUD: Added Digital Timer Clock Template");
                    return;
                } else if (mx >= cvX + 540.0f && mx <= cvX + 615.0f) {
                    _hudWidgets.push_back({ "Crosshair" + std::to_string(_hudWidgets.size()), "[ + ]", HUDWidgetType::Icon, 940.0f, 520.0f, 40.0f, 40.0f, Vec3(0.2f, 0.9f, 0.4f), Vec3(0.0f, 0.0f, 0.0f), 1.0f, 1.0f, "Crosshair", "General" });
                    logMessage("HUD: Added Crosshair Template");
                    return;
                } else if (mx >= cvX + 620.0f && mx <= cvX + 690.0f) {
                    _hudWidgets.push_back({ "ActionButton" + std::to_string(_hudWidgets.size()), "ACTION", HUDWidgetType::Button, 860.0f, 500.0f, 200.0f, 44.0f, Vec3(1.0f, 1.0f, 1.0f), Vec3(0.20f, 0.24f, 0.32f), 1.0f, 1.0f, "BtnAction", "General" });
                    logMessage("HUD: Added Action Button Template");
                    return;
                }
            }

            // Bottom test controls
            float botY = cvY + cvH - 38.0f;
            if (my >= botY + 2.0f && my <= botY + 28.0f) {
                if (mx >= cvX + 240.0f && mx <= cvX + 300.0f) {
                    _hudTestSliderValue = std::max(0.0f, _hudTestSliderValue - 10.0f);
                    ScriptEngine::setHUDValue("Health", _hudTestSliderValue);
                    ScriptEngine::setHUDValue("Mana", _hudTestSliderValue * 0.85f);
                    for (auto& w : _hudWidgets) {
                        if (w.type == HUDWidgetType::ProgressBar) w.currentValue = std::min(w.maxValue, _hudTestSliderValue);
                    }
                    logMessage("HUD Live Test: Decreased value to " + std::to_string((int)_hudTestSliderValue));
                    return;
                } else if (mx >= cvX + 305.0f && mx <= cvX + 365.0f) {
                    _hudTestSliderValue = std::min(100.0f, _hudTestSliderValue + 10.0f);
                    ScriptEngine::setHUDValue("Health", _hudTestSliderValue);
                    ScriptEngine::setHUDValue("Mana", _hudTestSliderValue * 0.85f);
                    for (auto& w : _hudWidgets) {
                        if (w.type == HUDWidgetType::ProgressBar) w.currentValue = std::min(w.maxValue, _hudTestSliderValue);
                    }
                    logMessage("HUD Live Test: Increased value to " + std::to_string((int)_hudTestSliderValue));
                    return;
                } else if (mx >= cvX + 370.0f && mx <= cvX + 470.0f) {
                    _hudTestSliderValue = 100.0f;
                    ScriptEngine::setHUDValue("Health", 100.0f);
                    ScriptEngine::setHUDValue("Mana", 100.0f);
                    for (auto& w : _hudWidgets) w.currentValue = w.maxValue;
                    logMessage("HUD Live Test: Reset gauges to maximum.");
                    return;
                } else if (mx >= cvX + 475.0f && mx <= cvX + 605.0f) {
                    toggleSimulation();
                    return;
                }
            }
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
            float actY = contentY + 3.0f;

            if (my >= actY && my <= actY + 28.0f) {
                if (mx >= vpX + 6.0f && mx <= vpX + 166.0f) {
                    runCurrentScript();
                    return;
                } else if (mx >= vpX + 172.0f && mx <= vpX + 292.0f) {
                    saveScriptToFile(_activeScriptPath);
                    return;
                } else if (mx >= vpX + 298.0f && mx <= vpX + 448.0f) {
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
                } else if (mx >= vpX + 454.0f && mx <= vpX + 594.0f) {
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
                } else if (mx >= vpX + 600.0f && mx <= vpX + 730.0f) {
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
                } else if (mx >= vpX + 736.0f && mx <= vpX + 886.0f) {
                    if (_selectionType == SelectionType::Brush && _selectedIndex >= 0 && _selectedIndex < (int)_map->brushes.size()) {
                        _map->brushes[_selectedIndex].name = "Klocek1";
                        _map->brushes[_selectedIndex].script = "assets/scripts/gameplay.lua";
                    } else if (_selectionType == SelectionType::Prop && _selectedIndex >= 0 && _selectedIndex < (int)_map->props.size()) {
                        _map->props[_selectedIndex].name = "Klocek1";
                        _map->props[_selectedIndex].script = "assets/scripts/gameplay.lua";
                    } else if (_selectionType == SelectionType::Sprite && _selectedIndex >= 0 && _selectedIndex < (int)_map->sprites.size()) {
                        _map->sprites[_selectedIndex].name = "Klocek1";
                        _map->sprites[_selectedIndex].script = "assets/scripts/gameplay.lua";
                    }
                    logMessage("Named selected entity 'Klocek1' (Controllable in Lua via World.getEntity('Klocek1'))");
                    return;
                }
            }

            // Click inside the Code Area to place cursor & focus
            float codeX = vpX + 4.0f;
            float codeY = contentY + 34.0f;
            float codeW = vpW - 8.0f;
            float codeH = contentH - 34.0f - 24.0f;
            if (mx >= codeX && mx <= codeX + codeW && my >= codeY && my <= codeY + codeH) {
                _scriptEditorFocused = true;
                float relY = my - (codeY + 6.0f);
                int lineIdx = _scriptScrollLine + (int)(relY / 18.0f);
                if (lineIdx >= 0 && lineIdx < (int)_scriptLines.size()) {
                    _scriptCursorLine = lineIdx;
                    float gutterW = 46.0f;
                    float textStartX = codeX + gutterW + 6.0f;
                    float relX = std::max(0.0f, mx - textStartX);
                    int colIdx = (int)std::round(relX / 8.5f);
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

            // 1. Top Filter Buttons: All, Enabled, Disabled, Import, Rescan
            if (my >= barY + 4.0f && my <= barY + 30.0f) {
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
                    // Import / Create new plugin template
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
            }

            // 2. Category Buttons
            if (my >= catY && my <= catY + catH) {
                const char* cats[] = { "All", "3D & Geometry", "Gameplay & HUD", "Rendering & VFX", "Scripting & Dev", "Physics & Vehicles", "Tools & Utilities" };
                float cx = vpX + 14.0f;
                for (int i = 0; i < 7; ++i) {
                    float cw = (i == 0) ? 45.0f : 120.0f;
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

            // 4. Right Pane Action Buttons
            const auto* selPkg = PackageManager::instance().getPackage(_pluginSelectedId);
            if (selPkg) {
                float dy = mainY + 16.0f + 34.0f + 28.0f + 14.0f;
                if (my >= dy && my <= dy + 30.0f) {
                    // Toggle Enable/Disable
                    if (mx >= detX + 20.0f && mx <= detX + 160.0f) {
                        PackageManager::instance().togglePackage(selPkg->id);
                        bool nowEn = PackageManager::instance().isPackageEnabled(selPkg->id);
                        _pluginStatusBanner = "Plugin '" + selPkg->displayName + "' is now " + (nowEn ? "ACTIVE" : "DISABLED");
                        _pluginBannerTimer = 3.0f;
                        return;
                    }

                    // Procedural Level Generator Action (real CSG brushes generated into level!)
                    if (_pluginSelectedId == "com.lab.proc-gen" && mx >= detX + 170.0f && mx <= detX + 410.0f) {
                        auto proc = std::dynamic_pointer_cast<ProceduralLevelPlugin>(PackageManager::instance().getPluginImplementation("com.lab.proc-gen"));
                        if (proc && _map) {
                            int added = proc->generateRoom(_map->brushes, Vec3(0, 0, 0));
                            _pluginStatusBanner = "Generated procedural room (" + std::to_string(added) + " CSG brushes)! Switch to 3D Level Scene tab to view.";
                            _pluginBannerTimer = 5.0f;
                            logMessage("Procedural Level Generator: Added " + std::to_string(added) + " CSG room brushes to map.");
                        } else if (_map) {
                            ProceduralLevelPlugin fallbackProc;
                            int added = fallbackProc.generateRoom(_map->brushes, Vec3(0, 0, 0));
                            _pluginStatusBanner = "Generated procedural room (" + std::to_string(added) + " CSG brushes)! Switch to 3D Level Scene tab to view.";
                            _pluginBannerTimer = 5.0f;
                        }
                        return;
                    }

                    // Speedrun Timer Action (legacy C++ fallback when drop-in Lua has no timer impl)
                    if (_pluginSelectedId == "com.lab.speedrun-timer") {
                        static SpeedrunTelemetryPlugin s_legacyTimer;
                        if (mx >= detX + 170.0f && mx <= detX + 390.0f) {
                            auto timerPlug = std::dynamic_pointer_cast<SpeedrunTelemetryPlugin>(PackageManager::instance().getPluginImplementation("com.lab.speedrun-timer"));
                            if (timerPlug) {
                                if (timerPlug->isRunning()) timerPlug->pauseTimer();
                                else timerPlug->startTimer();
                                _pluginStatusBanner = timerPlug->isRunning() ? "Speedrun timer RUNNING." : "Speedrun timer PAUSED.";
                            } else {
                                if (s_legacyTimer.isRunning()) s_legacyTimer.pauseTimer();
                                else s_legacyTimer.startTimer();
                                _pluginStatusBanner = s_legacyTimer.isRunning() ? "Speedrun timer RUNNING (local)." : "Speedrun timer PAUSED (local).";
                            }
                            _pluginBannerTimer = 3.0f;
                            return;
                        } else if (mx >= detX + 400.0f && mx <= detX + 520.0f) {
                            auto timerPlug = std::dynamic_pointer_cast<SpeedrunTelemetryPlugin>(PackageManager::instance().getPluginImplementation("com.lab.speedrun-timer"));
                            if (timerPlug) timerPlug->resetTimer();
                            else s_legacyTimer.resetTimer();
                            _pluginStatusBanner = "Speedrun timer RESET to 00:00.00.";
                            _pluginBannerTimer = 3.0f;
                            return;
                        }
                    }

                    // Open in Lua Studio Button (opens the drop-in single file!)
                    if (mx >= detX + detW - 200.0f && mx <= detX + detW - 20.0f) {
                        std::string luaPath = PackageManager::instance().getPackageFile(selPkg->id);
                        if (luaPath.empty()) luaPath = "plugins/" + selPkg->id.substr(selPkg->id.rfind('.') + 1) + ".lua";
                        if (!std::filesystem::exists(luaPath) && std::filesystem::exists("plugins")) {
                            // Find corresponding script in plugins
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
            }
            return; // Consume click so it never falls through to 3D actions
        }

        // 8.8. 3D Animation Workspace Timeline Interactions
        if (_workspace == EditorWorkspace::World3D_Animation) {
            float vpX = leftW;
            float vpW = l.rightX - vpX;
            float tlW = std::min(vpW - 40.0f, 860.0f);
            float tlH = 75.0f;
            float tlX = vpX + (vpW - tlW) * 0.5f;
            float tlY = (h - 22.0f) - tlH - 15.0f;

            if (my >= tlY + 10.0f && my <= tlY + 36.0f) {
                if (mx >= tlX + 10.0f && mx <= tlX + 90.0f) {
                    _timelineFrame = 0;
                    return;
                } else if (mx >= tlX + 96.0f && mx <= tlX + 196.0f) {
                    _timelinePlaying = !_timelinePlaying;
                    return;
                }
            }
            // Scrubber bar click
            if (my >= tlY + 44.0f && my <= tlY + 62.0f) {
                float barX = tlX + 10.0f;
                float barW = tlW - 20.0f;
                if (mx >= barX && mx <= barX + barW) {
                    _timelineFrame = std::clamp((int)((mx - barX) / barW * 60.0f), 0, 59);
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
        // X Axis: Red
        Renderer::drawCube(pos + Vec3(len * 0.5f, 0, 0), Vec3(len, thick, thick), Vec3(1.0f, 0.18f, 0.18f), false);
        Renderer::drawCube(pos + Vec3(len, 0, 0), Vec3(cap, cap, cap), Vec3(1.0f, 0.35f, 0.35f), false);
        // Y Axis: Green (vertical elevation handle - glowing green top cap arrow)
        Renderer::drawCube(pos + Vec3(0, len * 0.5f, 0), Vec3(thick, len, thick), Vec3(0.2f, 1.0f, 0.25f), false);
        Renderer::drawCube(pos + Vec3(0, len, 0), Vec3(cap * 1.3f, cap * 1.5f, cap * 1.3f), Vec3(0.35f, 1.0f, 0.45f), false);
        // Z Axis: Blue
        Renderer::drawCube(pos + Vec3(0, 0, len * 0.5f), Vec3(thick, thick, len), Vec3(0.25f, 0.55f, 1.0f), false);
        Renderer::drawCube(pos + Vec3(0, 0, len), Vec3(cap, cap, cap), Vec3(0.45f, 0.7f, 1.0f), false);
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
                    Renderer::drawCube(b.position, b.size, b.color, tex, true, b.uvScale, b.uvMode);
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

        Renderer::endFrame();
        Profiler::instance().endFrame();
    }

    // Studio Dark Industrial Styling Helpers
    static void drawDarkBevel(float x, float y, float w, float h, bool sunken) {
        Vec3 light = sunken ? Vec3(0.08f, 0.09f, 0.11f) : Vec3(0.38f, 0.40f, 0.45f);
        Vec3 dark  = sunken ? Vec3(0.38f, 0.40f, 0.45f) : Vec3(0.08f, 0.09f, 0.11f);

        // Top line
        Renderer::drawRect(x, y, w, 1.0f, light);
        // Left line
        Renderer::drawRect(x, y, 1.0f, h, light);
        // Bottom line
        Renderer::drawRect(x, y + h - 1.0f, w, 1.0f, dark);
        // Right line
        Renderer::drawRect(x + w - 1.0f, y, 1.0f, h, dark);
    }

    static void drawDarkPanel(float x, float y, float w, float h, const std::string& title = "") {
        Vec3 bgCol(0.18f, 0.19f, 0.22f); // Studio dark slate
        Renderer::drawRect(x, y, w, h, bgCol);
        drawDarkBevel(x, y, w, h, false);

        if (!title.empty()) {
            Vec3 titleBg(0.14f, 0.22f, 0.32f); // Studio dark blue header
            Renderer::drawRect(x + 2.0f, y + 2.0f, w - 4.0f, 20.0f, titleBg);
            LabFont::drawText(x + 8.0f, y + 5.0f, title, 1.5f, Vec3(1.0f, 1.0f, 1.0f), LabFontType::System);
        }
    }

    bool drawDarkButton(float x, float y, float w, float h, const std::string& label, bool active = false, bool isRed = false, bool isGreen = false) {
        bool hovered = (_mouseScreenX >= x && _mouseScreenX <= x + w && _mouseScreenY >= y && _mouseScreenY <= y + h);

        Vec3 bgCol = active ? Vec3(0.85f, 0.48f, 0.10f) :
                     isRed ? (hovered ? Vec3(0.75f, 0.22f, 0.22f) : Vec3(0.55f, 0.16f, 0.16f)) :
                     isGreen ? (hovered ? Vec3(0.22f, 0.65f, 0.32f) : Vec3(0.16f, 0.50f, 0.25f)) :
                     hovered ? Vec3(0.32f, 0.35f, 0.40f) : Vec3(0.24f, 0.25f, 0.28f);

        Renderer::drawRect(x, y, w, h, bgCol);
        drawDarkBevel(x, y, w, h, active);

        Vec3 txtCol = active ? Vec3(1.0f, 1.0f, 1.0f) :
                      hovered ? Vec3(1.0f, 0.95f, 0.70f) :
                      isRed ? Vec3(1.0f, 0.85f, 0.85f) :
                      isGreen ? Vec3(0.90f, 1.0f, 0.90f) :
                      Vec3(0.90f, 0.92f, 0.95f);

        float textX = x + 8.0f;
        float textY = y + (h - 14.0f) * 0.5f;
        LabFont::drawText(textX, textY, label, 1.4f, txtCol, LabFontType::System);
        return hovered;
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
            int totalEntities = (int)(totalSpawns + totalWepSpawns + _map->brushes.size() + _map->props.size() + _map->doors.size() + _map->lights.size());
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
                    } else if (itemIdx >= lOffset) {
                        int lIdx = itemIdx - lOffset;
                        isSelected = (_selectionType == SelectionType::Light && _selectedIndex == lIdx);
                        itemText = "[Light #" + std::to_string(lIdx) + "] " + _map->lights[lIdx].name;
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

            LabFont::drawText(l.rightX + 12.0f, propY, selHeader, 1.7f, (_selectionType != SelectionType::None) ? orangeGlow : textLight, LabFontType::System);

            // Coordinates & Dimensions
            Vec3 pos = _cursorPos;
            Vec3 dims = _brushSize;
            if (_selectionType == SelectionType::Brush && _selectedIndex >= 0 && _selectedIndex < (int)_map->brushes.size()) {
                pos = _map->brushes[_selectedIndex].position;
                dims = _map->brushes[_selectedIndex].size;
            } else if (_selectionType == SelectionType::Prop && _selectedIndex >= 0 && _selectedIndex < (int)_map->props.size()) {
                pos = _map->props[_selectedIndex].position;
                dims = _map->props[_selectedIndex].scale;
            } else if (_selectionType == SelectionType::Door && _selectedIndex >= 0 && _selectedIndex < (int)_map->doors.size()) {
                pos = _map->doors[_selectedIndex].position;
                dims = _map->doors[_selectedIndex].size;
            } else if (_selectionType == SelectionType::Spawn && _selectedIndex >= 0 && _selectedIndex < (int)_map->spawnPoints.size()) {
                pos = _map->spawnPoints[_selectedIndex].position;
            } else if (_selectionType == SelectionType::WeaponSpawner && _selectedIndex >= 0 && _selectedIndex < (int)_map->weaponSpawners.size()) {
                pos = _map->weaponSpawners[_selectedIndex].position;
            }

            std::string posStr = "Pos: (" + std::to_string((int)pos.x) + ", " + std::to_string((int)pos.y) + ", " + std::to_string((int)pos.z) + ")";
            LabFont::drawText(l.rightX + 12.0f, propY + 22.0f, posStr, 1.6f, textLight, LabFontType::System);

            std::string dimStr = "Size: (" + std::to_string((int)dims.x) + " x " + std::to_string((int)dims.y) + " x " + std::to_string((int)dims.z) + ")";
            LabFont::drawText(l.rightX + 12.0f, propY + 40.0f, dimStr, 1.5f, textLight, LabFontType::System);

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
            }

            if (_selectionType != SelectionType::None) {
                std::string nameDisplay = "Entity ID: " + (entName.empty() ? "(unnamed)" : entName);
                LabFont::drawText(l.rightX + 12.0f, propY + 58.0f, nameDisplay, 1.45f, cyanGlow, LabFontType::System);
                drawDarkButton(l.rightX + 10.0f, propY + 74.0f, 135.0f, 22.0f, "Set \"Klocek1\"", false, false, true);
                drawDarkButton(l.rightX + 155.0f, propY + 74.0f, 140.0f, 22.0f, "Assign Script");
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

        // Viewport Tab Bar (Intuitive Workspace Switcher: 3D Scene / Lua Script Studio / HUD Canvas)
        float tabH = 26.0f;
        Renderer::drawRect(vpX, vpY, vpW, tabH, Vec3(0.10f, 0.11f, 0.14f));
        drawDarkBevel(vpX, vpY, vpW, tabH, false);

        bool is3D = (_workspace == EditorWorkspace::World3D_Level || _workspace == EditorWorkspace::World3D_Animation);
        bool isScr = (_workspace == EditorWorkspace::Scripting_Editor || _workspace == EditorWorkspace::Scripting_Console);
        bool isHud = (_workspace == EditorWorkspace::Level2D_HUD);
        bool isMv = (_workspace == EditorWorkspace::World3D_ModelViewer);
        bool isAnim2D = (_workspace == EditorWorkspace::Level2D_Animation);
        bool isStud = isStudioWorkspace(_workspace);

        // Tab 1: [ 3D Level Scene ]
        float tab1X = vpX + 6.0f;
        float tab1W = 120.0f;
        Renderer::drawRect(tab1X, vpY + 2.0f, tab1W, tabH - 4.0f, is3D ? Vec3(0.18f, 0.22f, 0.28f) : Vec3(0.12f, 0.13f, 0.16f));
        drawDarkBevel(tab1X, vpY + 2.0f, tab1W, tabH - 4.0f, is3D);
        if (is3D) Renderer::drawRect(tab1X, vpY + tabH - 2.0f, tab1W, 2.0f, orangeGlow);
        LabFont::drawText(tab1X + 12.0f, vpY + 6.0f, "3D Level Scene", 1.35f, is3D ? Vec3(1, 1, 1) : textDim, LabFontType::System);

        // Tab 2: [ Lua Script Studio ]
        float tab2X = tab1X + tab1W + 6.0f;
        float tab2W = 145.0f;
        Renderer::drawRect(tab2X, vpY + 2.0f, tab2W, tabH - 4.0f, isScr ? Vec3(0.18f, 0.22f, 0.28f) : Vec3(0.12f, 0.13f, 0.16f));
        drawDarkBevel(tab2X, vpY + 2.0f, tab2W, tabH - 4.0f, isScr);
        if (isScr) Renderer::drawRect(tab2X, vpY + tabH - 2.0f, tab2W, 2.0f, cyanGlow);
        LabFont::drawText(tab2X + 10.0f, vpY + 6.0f, "Lua Script Studio", 1.35f, isScr ? Vec3(1, 1, 1) : textDim, LabFontType::System);

        // Tab 3: [ HUD Canvas Designer ]
        float tab3X = tab2X + tab2W + 6.0f;
        float tab3W = 145.0f;
        Renderer::drawRect(tab3X, vpY + 2.0f, tab3W, tabH - 4.0f, isHud ? Vec3(0.18f, 0.22f, 0.28f) : Vec3(0.12f, 0.13f, 0.16f));
        drawDarkBevel(tab3X, vpY + 2.0f, tab3W, tabH - 4.0f, isHud);
        if (isHud) Renderer::drawRect(tab3X, vpY + tabH - 2.0f, tab3W, 2.0f, cyanGlow);
        LabFont::drawText(tab3X + 10.0f, vpY + 6.0f, "HUD Canvas Designer", 1.35f, isHud ? Vec3(1, 1, 1) : textDim, LabFontType::System);

        // Tab 4: [ Plugins & Add-ons ]
        bool isPlug = (_workspace == EditorWorkspace::Plugins_Manager);
        float tab4X = tab3X + tab3W + 6.0f;
        float tab4W = 145.0f;
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
            // Interactive Multi-Genre HUD Canvas Designer - full viewport stage
            Renderer::drawRect(vpX, contentY, vpW, contentH, Vec3(0.05f, 0.06f, 0.08f));
            drawDarkBevel(vpX, contentY, vpW, contentH, false);

            // Top Genre Template Toolbar
            float htbY = contentY + 2.0f;
            Renderer::drawRect(vpX, htbY, vpW, 28.0f, Vec3(0.09f, 0.10f, 0.13f));
            drawDarkButton(vpX + 8.0f, htbY + 2.0f, 90.0f, 24.0f, "+ RPG Bars");
            drawDarkButton(vpX + 104.0f, htbY + 2.0f, 85.0f, 24.0f, "+ Platform");
            drawDarkButton(vpX + 195.0f, htbY + 2.0f, 85.0f, 24.0f, "+ RTS Bars");
            drawDarkButton(vpX + 286.0f, htbY + 2.0f, 85.0f, 24.0f, "+ Racing");
            drawDarkButton(vpX + 377.0f, htbY + 2.0f, 85.0f, 24.0f, "+ Story Box");
            drawDarkButton(vpX + 468.0f, htbY + 2.0f, 80.0f, 24.0f, "+ Timer");
            drawDarkButton(vpX + 554.0f, htbY + 2.0f, 80.0f, 24.0f, "+ Crosshair");
            drawDarkButton(vpX + 640.0f, htbY + 2.0f, 75.0f, 24.0f, "+ Button");

            // Calculate centered 16:9 Virtual Artboard (1920x1080)
            float botH = 34.0f;
            float availW = vpW - 20.0f;
            float availH = contentH - 32.0f - botH - 10.0f;
            float s = std::min(availW / 1920.0f, availH / 1080.0f);
            float cvW = 1920.0f * s;
            float cvH = 1080.0f * s;
            float cvX = vpX + (vpW - cvW) * 0.5f;
            float cvY = contentY + 32.0f + (availH - cvH) * 0.5f;

            Renderer::drawRect(cvX, cvY, cvW, cvH, Vec3(0.07f, 0.08f, 0.11f));
            drawDarkBevel(cvX, cvY, cvW, cvH, true);
            LabFont::drawText(cvX + 8.0f, cvY + 4.0f, "1920x1080 Screen Artboard", 1.25f, cyanGlow, LabFontType::System);

            // Render HUD Widgets onto Virtual Artboard
            float scaleX = cvW / 1920.0f;
            float scaleY = cvH / 1080.0f;

            for (size_t i = 0; i < _hudWidgets.size(); ++i) {
                const auto& w = _hudWidgets[i];
                float wx = cvX + w.x * scaleX;
                float wy = cvY + w.y * scaleY;
                float ww = std::max(20.0f, w.w * scaleX);
                float wh = std::max(12.0f, w.h * scaleY);

                bool isSel = ((int)i == _selectedHUDWidgetIndex);
                if (w.type == HUDWidgetType::ProgressBar) {
                    Renderer::drawRect(wx, wy, ww, wh, w.bgColor);
                    drawDarkBevel(wx, wy, ww, wh, true);
                    float fillRatio = std::clamp(w.currentValue / std::max(1.0f, w.maxValue), 0.0f, 1.0f);
                    Renderer::drawRect(wx + 1.0f, wy + 1.0f, (ww - 2.0f) * fillRatio, wh - 2.0f, w.color);
                    LabFont::drawText(wx + 4.0f, wy - 14.0f, w.label + " [" + w.bindingKey + "]", 1.25f, w.color, LabFontType::System);
                } else if (w.type == HUDWidgetType::TextLabel) {
                    Renderer::drawRect(wx, wy, ww, wh, w.bgColor);
                    drawDarkBevel(wx, wy, ww, wh, false);
                    LabFont::drawText(wx + 6.0f, wy + (wh - 14.0f) * 0.5f, w.label, 1.4f, w.color, LabFontType::System);
                } else if (w.type == HUDWidgetType::DialogueBox) {
                    Renderer::drawRect(wx, wy, ww, wh, w.bgColor);
                    drawDarkBevel(wx, wy, ww, wh, false);
                    Renderer::drawRect(wx, wy, ww, 2.0f, cyanGlow);
                    LabFont::drawText(wx + 12.0f, wy + 8.0f, w.label, 1.4f, w.color, LabFontType::System);
                } else if (w.type == HUDWidgetType::Button) {
                    Renderer::drawRect(wx, wy, ww, wh, w.bgColor);
                    drawDarkBevel(wx, wy, ww, wh, false);
                    LabFont::drawText(wx + 10.0f, wy + (wh - 14.0f) * 0.5f, w.label, 1.35f, w.color, LabFontType::System);
                } else if (w.type == HUDWidgetType::Icon) {
                    Renderer::drawRect(wx, wy, ww, wh, w.color);
                    drawDarkBevel(wx, wy, ww, wh, false);
                    LabFont::drawText(wx + 4.0f, wy + 2.0f, w.label, 1.3f, Vec3(1, 1, 1), LabFontType::System);
                }

                if (isSel) {
                    Renderer::drawRect(wx - 2.0f, wy - 2.0f, ww + 4.0f, 2.0f, orangeGlow);
                    Renderer::drawRect(wx - 2.0f, wy + wh, ww + 4.0f, 2.0f, orangeGlow);
                    Renderer::drawRect(wx - 2.0f, wy - 2.0f, 2.0f, wh + 4.0f, orangeGlow);
                    Renderer::drawRect(wx + ww, wy - 2.0f, 2.0f, wh + 4.0f, orangeGlow);
                }
            }

            // Canvas Quick-Control Buttons (at bottom of canvas)
            bool isSim = (_simState == EngineSimState::Playing);
            float btnY = contentY + contentH - 30.0f;
            drawDarkButton(vpX + 8.0f, btnY, 140.0f, 24.0f, isSim ? "Stop Sim (F9)" : "> Play Sim (F9)", isSim, false, !isSim);
            drawDarkButton(vpX + 154.0f, btnY, 110.0f, 24.0f, "+1s Clock", false, false, true);
            drawDarkButton(vpX + 270.0f, btnY, 100.0f, 24.0f, "Reset Timer");
            drawDarkButton(vpX + 376.0f, btnY, 100.0f, 24.0f, "-15 Health", false, true);
            drawDarkButton(vpX + 482.0f, btnY, 100.0f, 24.0f, "+25 Health", false, false, true);
            drawDarkButton(vpX + 588.0f, btnY, 100.0f, 24.0f, "+50 Score", false, false, true);
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
            float tlH = 75.0f;
            float tlX = vpX + (vpW - tlW) * 0.5f;
            float tlY = (h - 22.0f) - tlH - 15.0f;

            Renderer::drawRect(tlX, tlY, tlW, tlH, Vec3(0.12f, 0.14f, 0.18f));
            drawDarkBevel(tlX, tlY, tlW, tlH, false);

            drawDarkButton(tlX + 10.0f, tlY + 10.0f, 80.0f, 26.0f, "|< Reset");
            drawDarkButton(tlX + 96.0f, tlY + 10.0f, 100.0f, 26.0f, _timelinePlaying ? "Pause" : "> Play", _timelinePlaying, false, !_timelinePlaying);

            std::string fInfo = "3D Frame: " + std::to_string(_timelineFrame) + " / 60  (30 FPS)";
            LabFont::drawText(tlX + 210.0f, tlY + 15.0f, fInfo, 1.5f, cyanGlow, LabFontType::System);

            // Scrubber Bar
            float barX = tlX + 10.0f;
            float barY = tlY + 44.0f;
            float barW = tlW - 20.0f;
            float barH = 18.0f;
            Renderer::drawRect(barX, barY, barW, barH, Vec3(0.08f, 0.09f, 0.11f));
            drawDarkBevel(barX, barY, barW, barH, true);

            float playheadX = barX + (_timelineFrame / 60.0f) * barW;
            Renderer::drawRect(playheadX - 2.0f, barY, 5.0f, barH, orangeGlow);
        } else if (_workspace == EditorWorkspace::Scripting_Editor || _workspace == EditorWorkspace::Scripting_Console) {
            // Full In-Engine Lua Script Studio IDE - occupies full central viewport
            Renderer::drawRect(vpX, contentY, vpW, contentH, Vec3(0.065f, 0.075f, 0.095f));
            drawDarkBevel(vpX, contentY, vpW, contentH, false);

            // Actions toolbar
            float actY = contentY + 4.0f;
            drawDarkButton(vpX + 6.0f, actY, 160.0f, 26.0f, "> RUN SCRIPT (Ctrl+R)", false, false, true);
            drawDarkButton(vpX + 172.0f, actY, 120.0f, 26.0f, "SAVE (Ctrl+S)", false, false, true);
            drawDarkButton(vpX + 298.0f, actY, 150.0f, 26.0f, "Preset: Klocek+Timer");
            drawDarkButton(vpX + 454.0f, actY, 140.0f, 26.0f, "Preset: Player HUD");
            drawDarkButton(vpX + 600.0f, actY, 130.0f, 26.0f, "Preset: Movement");
            drawDarkButton(vpX + 736.0f, actY, 150.0f, 26.0f, "Name Sel 'Klocek1'", false, false, true);

            // Interactive Code Editor Area
            float codeX = vpX + 4.0f;
            float codeY = contentY + 34.0f;
            float codeW = vpW - 8.0f;
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
                LabFont::drawText(codeX + gutterW + 8.0f, ly, lText, 1.35f, lColor, LabFontType::System);
            }

            // Blinking caret
            if (_scriptEditorFocused && ((int)(_scriptBlinkTimer * 2.0f) % 2 == 0)) {
                if (curRel >= 0 && curRel < maxVisible) {
                    float caretY = codeY + 6.0f + curRel * 18.0f;
                    float caretX = codeX + gutterW + 8.0f + _scriptCursorCol * 8.5f;
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
            // Full-Viewport Package & Plugin Manager (Unity & Blender Add-ons Standard)
            // 1 kod 1 plugin: Standalone decoupled modules
            Renderer::drawRect(vpX, contentY, vpW, contentH, Vec3(0.06f, 0.07f, 0.09f));
            drawDarkBevel(vpX, contentY, vpW, contentH, false);

            // 1. Top Header & Filter Toolbar (height 40px)
            float barY = contentY + 4.0f;
            float barH = 34.0f;
            Renderer::drawRect(vpX + 4.0f, barY, vpW - 8.0f, barH, Vec3(0.10f, 0.12f, 0.16f));
            drawDarkBevel(vpX + 4.0f, barY, vpW - 8.0f, barH, false);

            LabFont::drawText(vpX + 16.0f, barY + 9.0f, "EXTENSIONS: PACKAGE & PLUGIN MANAGER", 1.55f, orangeGlow, LabFontType::System);
            LabFont::drawText(vpX + 380.0f, barY + 11.0f, "[Unity Package Manager & Blender Add-ons Standard - 1 kod 1 plugin]", 1.25f, textDim, LabFontType::System);

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

            const char* cats[] = { "All", "3D & Geometry", "Gameplay & HUD", "Rendering & VFX", "Scripting & Dev", "Physics & Vehicles", "Tools & Utilities" };
            float cx = vpX + 14.0f;
            for (int i = 0; i < 7; ++i) {
                bool isCurCat = (_pluginCategoryFilter == cats[i]);
                float cw = (i == 0) ? 45.0f : 120.0f;
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

            // Split Layout
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

            // --- RIGHT PANEL: Inspector & Real Actions ---
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
                dy += 14.0f;

                // Actions Bar
                bool isEn = (selPkg->state == PackageState::Enabled);
                float btnW = 140.0f;
                drawDarkButton(detX + 20.0f, dy, btnW, 30.0f, isEn ? "Disable Plugin" : "Enable Plugin", false, false, true);

                if (_pluginSelectedId == "com.lab.proc-gen") {
                    drawDarkButton(detX + 170.0f, dy, 240.0f, 30.0f, "+ Generate Procedural Room", false, false, true);
                } else if (_pluginSelectedId == "com.lab.speedrun-timer") {
                    drawDarkButton(detX + 170.0f, dy, 220.0f, 30.0f, "Start / Pause Split Timer", false, false, true);
                    drawDarkButton(detX + 400.0f, dy, 120.0f, 30.0f, "Reset Timer", false, false, true);
                } else if (_pluginSelectedId == "com.lab.retro-fx") {
                    drawDarkButton(detX + 170.0f, dy, 200.0f, 30.0f, "Toggle CRT Scanlines", false, false, true);
                } else if (_pluginSelectedId == "com.lab.dialogue-quest") {
                    drawDarkButton(detX + 170.0f, dy, 200.0f, 30.0f, "Test Dialogue NPC", false, false, true);
                } else if (_pluginSelectedId == "com.lab.voxel-terrain") {
                    drawDarkButton(detX + 170.0f, dy, 200.0f, 30.0f, "Stamp Voxel Terrain", false, false, true);
                }

                drawDarkButton(detX + detW - 200.0f, dy, 180.0f, 30.0f, "Open in Lua Studio", false, false, true);
                dy += 42.0f;

                // Status Banner
                if (!_pluginStatusBanner.empty()) {
                    Renderer::drawRect(detX + 20.0f, dy, detW - 40.0f, 26.0f, Vec3(0.12f, 0.32f, 0.20f));
                    drawDarkBevel(detX + 20.0f, dy, detW - 40.0f, 26.0f, false);
                    LabFont::drawText(detX + 30.0f, dy + 5.0f, _pluginStatusBanner, 1.35f, Vec3(0.6f, 1.0f, 0.7f), LabFontType::System);
                    dy += 34.0f;
                }

                // Description Box
                LabFont::drawText(detX + 20.0f, dy, "Description:", 1.45f, textLight, LabFontType::System);
                dy += 22.0f;

                Renderer::drawRect(detX + 20.0f, dy, detW - 40.0f, 85.0f, Vec3(0.06f, 0.07f, 0.09f));
                drawDarkBevel(detX + 20.0f, dy, detW - 40.0f, 85.0f, true);
                LabFont::drawText(detX + 30.0f, dy + 12.0f, selPkg->description, 1.38f, Vec3(0.9f, 0.92f, 0.95f), LabFontType::System);
                std::string fileLocation = "Source Code: plugins/" + selPkg->id.substr(selPkg->id.rfind('.') + 1) + ".lua  (1 kod 1 plugin)";
                LabFont::drawText(detX + 30.0f, dy + 45.0f, fileLocation, 1.25f, orangeGlow, LabFontType::System);
                dy += 100.0f;

                // Telemetry & Architecture Box
                LabFont::drawText(detX + 20.0f, dy, "Engine Architecture & Hooks:", 1.45f, textLight, LabFontType::System);
                dy += 22.0f;

                Renderer::drawRect(detX + 20.0f, dy, detW - 40.0f, 100.0f, Vec3(0.06f, 0.07f, 0.09f));
                drawDarkBevel(detX + 20.0f, dy, detW - 40.0f, 100.0f, true);

                LabFont::drawText(detX + 30.0f, dy + 12.0f, "• Standalone Isolation: Each plugin runs in its own decoupled translation unit.", 1.32f, textDim, LabFontType::System);
                LabFont::drawText(detX + 30.0f, dy + 34.0f, "• Lifecycle Events: onLoad(), onUpdate(dt), onUnload() registered with Lab::PluginManager.", 1.32f, textDim, LabFontType::System);
                LabFont::drawText(detX + 30.0f, dy + 56.0f, "• State: " + std::string(isEn ? "RUNNING (Listening to frame ticks)" : "STOPPED (Zero runtime overhead)"), 1.32f, isEn ? Vec3(0.4f, 1.0f, 0.5f) : textDim, LabFontType::System);
                LabFont::drawText(detX + 30.0f, dy + 76.0f, "• Memory & Dependencies: None (Self-contained C++ / Lua module).", 1.32f, textDim, LabFontType::System);
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
        LabFont::drawText(10.0f, sbY + 5.0f, sbText, 1.5f, textLight, LabFontType::System);
        std::string gridStr = "Snap: " + std::to_string((int)_gridSnap) + " | F9: Run";
        LabFont::drawText(w - 240.0f, sbY + 5.0f, gridStr, 1.5f, orangeGlow, LabFontType::System);

        // ==================== 7. DROPDOWN FILE MENU ====================
        if (_fileMenuOpen) {
            float menuX = 10.0f;
            float menuY = 24.0f;
            float menuW = 230.0f;
            float menuH = 175.0f;
            Vec3 menuBg{ 0.16f, 0.17f, 0.20f };
            Vec3 menuBorder{ 0.35f, 0.38f, 0.44f };

            Renderer::drawRect(menuX, menuY, menuW, menuH, menuBg);
            drawDarkBevel(menuX, menuY, menuW, menuH, false);

            struct MenuItem { std::string name; std::string shortcut; };
            MenuItem items[] = {
                { "New Map", "Ctrl+N" },
                { "Open Map...", "Ctrl+O" },
                { "Save Map", "Ctrl+S" },
                { "Save Map As...", "" },
                { "Build & Run Game", "F5 / Ctrl+B" },
                { "Build Settings...", "Ctrl+Shift+B" },
                { "Exit", "Alt+F4" }
            };

            for (int i = 0; i < 7; ++i) {
                float iy = menuY + 3.0f + i * 24.0f;
                bool isHov = (_mouseScreenX >= menuX && _mouseScreenX <= menuX + menuW && _mouseScreenY >= iy && _mouseScreenY <= iy + 22.0f);
                if (isHov) {
                    Renderer::drawRect(menuX + 2.0f, iy, menuW - 4.0f, 22.0f, Vec3(0.24f, 0.28f, 0.36f));
                }
                if (i == 4 || i == 6) Renderer::drawRect(menuX + 6.0f, iy - 2.0f, menuW - 12.0f, 1.0f, menuBorder);
                LabFont::drawText(menuX + 14.0f, iy + 4.0f, items[i].name, 1.6f, (i == 4) ? (isHov ? Vec3(0.4f, 1.0f, 0.5f) : Vec3(0.3f, 0.9f, 0.4f)) : (isHov ? orangeGlow : textLight), LabFontType::System);
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
                        { "Weapon Reload Timeline", EditorWorkspace::Studio_ReloadTimeline },
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
            LabFont::drawText(ctxX + 8.0f, ctxY + 5.0f, "Add Component (Entity #" + std::to_string(_contextMenuTargetIndex) + ")", 1.35f, cyanGlow, LabFontType::System);

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
    int _timelineFrame = 0;
    bool _timelinePlaying = false;

    // Interactive Lua Script Studio Text Editor State
    std::vector<std::string> _scriptLines;
    int _scriptCursorLine = 0;
    int _scriptCursorCol = 0;
    int _scriptScrollLine = 0;
    bool _scriptEditorFocused = true;
    float _scriptBlinkTimer = 0.0f;
    std::string _activeScriptPath = "assets/scripts/gameplay.lua";
    bool _scriptModified = false;

    // Unity / Blender Plugin & Package Manager State
    std::string _pluginSearchQuery = "";
    std::string _pluginCategoryFilter = "All";
    std::string _pluginStateFilter = "All"; // "All", "Enabled", "Disabled"
    std::string _pluginSelectedId = "com.lab.proc-gen";
    std::string _pluginStatusBanner = "";
    float _pluginBannerTimer = 0.0f;
    float _pluginPollAccum = 0.0f;
    int _pluginListScroll = 0;

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

    // Native Multi-Genre HUD Canvas Designer State
    std::vector<HUDDesignWidget> _hudWidgets;
    int _selectedHUDWidgetIndex = -1;
    float _hudTestSliderValue = 100.0f;

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
