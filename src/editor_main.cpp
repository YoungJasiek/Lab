#include "Lab.h"
#include "LabFont.h"
#include "LabDialogs.h"
#include <iostream>
#include <filesystem>
#include <unordered_map>
#include <vector>
#include <algorithm>
#include <GLFW/glfw3.h>

using namespace Lab;

struct TextureEntry {
    std::string name;
    std::string filename;
};

class LabHammerStandalone : public Engine {
public:
    LabHammerStandalone()
        : Engine("Hammer - [Lab Map Editor 2026]", 1600, 900),
          _camera(70.0f, 16.0f / 9.0f, 0.01f, 3000.0f) {
    }

    void onInit() override {
        LabLog::info("Launching Full Valve Hammer UI Editor...");
        Renderer::init();

        // Scan textures in assets/textures
        discoverTextures();

        // Load or create map
        std::string targetMap = "assets/maps/facility_alpha.labmap";
        if (std::filesystem::exists(targetMap)) {
            _map = LabMap::loadFromFile(targetMap);
            if (_map) _currentMapPath = targetMap;
        }
        if (!_map) {
            _map = std::make_unique<LabMap>();
            _map->metadata.name = "new_map";
            _map->metadata.author = "Mapper";
            _map->spawn.position = Vec3(0, 1.8f, 0);

            // Default ground plate
            MapBrush floor;
            floor.position = Vec3(0, -0.5f, 0);
            floor.size = Vec3(32.0f, 1.0f, 32.0f);
            floor.color = Vec3(1.0f, 1.0f, 1.0f);
            floor.texturePath = "floor_tiles.bmp";
            _map->brushes.push_back(floor);
            _currentMapPath = "";
        }

        // Camera initial pose
        _camera.setPosition(Vec3(0, 6.0f, 14.0f));

        // Unlock mouse cursor for full desktop UI interaction
        glfwSetInputMode(getWindow(), GLFW_CURSOR, GLFW_CURSOR_NORMAL);

        logMessage("Search Path (GAME) : assets/textures/");
        logMessage("Search Path (GAME) : assets/models/");
        logMessage("Search Path (GAME) : assets/maps/");
        logMessage("Loaded " + std::to_string(_availableTextures.size()) + " textures into Hammer browser.");
        logMessage("Hammer initialized. Ready.");
    }

    void discoverTextures() {
        _availableTextures.clear();
        std::vector<std::string> searchDirs = { "assets/textures", "../assets/textures", "../../assets/textures" };
        for (const auto& dir : searchDirs) {
            if (std::filesystem::exists(dir)) {
                for (const auto& entry : std::filesystem::directory_iterator(dir)) {
                    if (entry.path().extension() == ".bmp") {
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
        if (_availableTextures.empty()) {
            _availableTextures.push_back({ "concrete_wall.bmp", "concrete_wall.bmp" });
            _textures["concrete_wall.bmp"] = std::make_unique<Texture>("concrete_wall.bmp");
        }
        _selectedTexture = _availableTextures[0].filename;
    }

    void logMessage(const std::string& msg) {
        _consoleMessages.push_back(msg);
        if (_consoleMessages.size() > 8) {
            _consoleMessages.erase(_consoleMessages.begin());
        }
    }

    void onFixedUpdate(float fixedDelta) override {
        // Noclip camera movement (active when holding Right Mouse Button)
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

        MapBrush floor;
        floor.position = Vec3(0, -0.5f, 0);
        floor.size = Vec3(32.0f, 1.0f, 32.0f);
        floor.color = Vec3(1.0f, 1.0f, 1.0f);
        floor.texturePath = "floor_tiles.bmp";
        _map->brushes.push_back(floor);

        _currentMapPath = "";
        logMessage("Created New Map.");
    }

    void openMapDialog() {
        std::string openPath = LabDialogs::openFileDialog(getWindow(), "Lab Map Files (*.labmap)\0*.labmap\0All Files (*.*)\0*.*\0", "assets\\maps");
        if (!openPath.empty()) {
            auto loaded = LabMap::loadFromFile(openPath);
            if (loaded) {
                _map = std::move(loaded);
                _currentMapPath = openPath;
                logMessage("Loaded Map: " + openPath);
            }
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

    void onUpdate(const Time& time) override {
        (void)time;

        // Mouse look in 3D Viewport when holding Right Mouse Button
        if (Input::isMouseButtonPressed(1)) {
            _camera.update(Input::mouseDelta);
        }

        // Snap 3D cursor to grid
        _cursorPos = snapToGrid(_camera.getPosition() + _camera.getFront() * 8.0f, _gridSnap);

        // Handle Left-Click on UI buttons & texture picker
        if (Input::isMouseButtonPressed(0)) {
            if (!_lmbPressed) {
                handleMouseClick(Input::mousePos.x, Input::mousePos.y);
                _lmbPressed = true;
            }
        } else {
            _lmbPressed = false;
        }

        // Keyboard Shortcuts
        bool ctrlDown = Input::isKeyPressed(341) || Input::isKeyPressed(345); // Left/Right Ctrl

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

        // Ctrl+S or K: Save Map
        if ((ctrlDown && (Input::isKeyPressed('S') || Input::isKeyPressed('s'))) || 
            (!ctrlDown && (Input::isKeyPressed('K') || Input::isKeyPressed('k')))) {
            if (!_ctrlSPressed) {
                saveMapAction(false);
                _ctrlSPressed = true;
            }
        } else {
            _ctrlSPressed = false;
        }

        // E: Place Brush or Entity with active texture
        if (!ctrlDown && (Input::isKeyPressed('E') || Input::isKeyPressed('e'))) {
            if (!_ePressed && _map) {
                placeCurrentObject();
                _ePressed = true;
            }
        } else {
            _ePressed = false;
        }

        // Backspace / Delete: Undo last brush
        if (Input::isKeyPressed(259) || Input::isKeyPressed(261)) {
            if (!_delPressed && _map && !_map->brushes.empty()) {
                _map->brushes.pop_back();
                logMessage("Undo: Deleted last brush.");
                _delPressed = true;
            }
        } else {
            _delPressed = false;
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

    void placeCurrentObject() {
        if (!_map) return;
        MapBrush b;
        b.position = _cursorPos;
        b.size = _brushSize;
        b.color = Vec3(1.0f, 1.0f, 1.0f);
        b.texturePath = _selectedTexture;
        _map->brushes.push_back(b);
        logMessage("Placed Brush (" + _selectedTexture + ") at (" + 
                   std::to_string((int)b.position.x) + ", " + 
                   std::to_string((int)b.position.y) + ", " + 
                   std::to_string((int)b.position.z) + ")");
    }

    void handleMouseClick(float mx, float my) {
        // Dropdown File Menu Clicks (when open)
        if (_fileMenuOpen) {
            float menuX = 10.0f;
            float menuY = 24.0f;
            float menuW = 190.0f;
            float menuH = 125.0f;
            if (mx >= menuX && mx <= menuX + menuW && my >= menuY && my <= menuY + menuH) {
                int itemIdx = (int)((my - menuY) / 24.0f);
                if (itemIdx == 0) { // New Map
                    newMap();
                } else if (itemIdx == 1) { // Open Map...
                    openMapDialog();
                } else if (itemIdx == 2) { // Save Map
                    saveMapAction(false);
                } else if (itemIdx == 3) { // Save Map As...
                    saveMapAction(true);
                } else if (itemIdx >= 4) { // Exit
                    glfwSetWindowShouldClose(getWindow(), GLFW_TRUE);
                }
                _fileMenuOpen = false;
                return;
            } else {
                _fileMenuOpen = false;
            }
        }

        // Texture Browser Grid Clicks (Inside Browse Window if open)
        if (_browserOpen) {
            // Close button click
            if (mx >= 1170.0f && mx <= 1200.0f && my >= 100.0f && my <= 125.0f) {
                _browserOpen = false;
                return;
            }
            // Grid of textures (x: 420..1180, y: 150..700)
            int cols = 6;
            float thumbSize = 110.0f;
            float gap = 15.0f;
            float startX = 425.0f;
            float startY = 150.0f;

            for (size_t i = 0; i < _availableTextures.size(); ++i) {
                int col = (int)(i % cols);
                int row = (int)(i / cols);
                float tx = startX + col * (thumbSize + gap);
                float ty = startY + row * (thumbSize + gap);

                if (mx >= tx && mx <= tx + thumbSize && my >= ty && my <= ty + thumbSize) {
                    _selectedTexture = _availableTextures[i].filename;
                    _textureIndex = (int)i;
                    logMessage("Selected Texture: " + _selectedTexture);
                    _browserOpen = false;
                    return;
                }
            }
            return;
        }

        // Left Toolbar Tools (x: 5..45)
        if (mx >= 5.0f && mx <= 45.0f) {
            float startY = 70.0f;
            for (int i = 0; i < 8; ++i) {
                float ty = startY + i * 36.0f;
                if (my >= ty && my <= ty + 32.0f) {
                    _activeTool = i;
                    if (i == 0) logMessage("Tool: Pointer / Selection Tool");
                    else if (i == 1) logMessage("Tool: Block / Brush Tool");
                    else if (i == 2) logMessage("Tool: Entity / Prop Tool");
                    else if (i == 3) logMessage("Tool: Texture Application Tool");
                    else if (i == 4) logMessage("Tool: Face Edit Tool");
                    else if (i == 5) logMessage("Tool: Decal Tool");
                    else if (i == 6) logMessage("Tool: Clipping Tool");
                    return;
                }
            }
        }

        // Right Sidebar Texture Thumbnail Click or Browse... Button
        if (mx >= 1320.0f && mx <= 1580.0f) {
            // Browse... button
            if (my >= 370.0f && my <= 405.0f) {
                _browserOpen = true;
                logMessage("Opened Texture Browser (" + std::to_string(_availableTextures.size()) + " available)");
                return;
            }
            // Texture thumbnail click (cycles textures)
            if (my >= 280.0f && my <= 365.0f && mx <= 1420.0f) {
                _textureIndex = (int)((_textureIndex + 1) % _availableTextures.size());
                _selectedTexture = _availableTextures[_textureIndex].filename;
                logMessage("Selected Texture: " + _selectedTexture);
                return;
            }
            // Apply Texture button
            if (my >= 410.0f && my <= 445.0f) {
                placeCurrentObject();
                return;
            }
        }

        // Top Menu Bar Clicks (y: 0..24)
        if (my >= 0.0f && my <= 24.0f) {
            // File dropdown toggle (x: 10..48)
            if (mx >= 10.0f && mx <= 48.0f) {
                _fileMenuOpen = !_fileMenuOpen;
                return;
            }
            // Edit (x: 50..85)
            else if (mx >= 50.0f && mx <= 85.0f) {
                if (_map && !_map->brushes.empty()) {
                    _map->brushes.pop_back();
                    logMessage("Undo: Deleted last brush.");
                }
                return;
            }
            // View (x: 88..130)
            else if (mx >= 88.0f && mx <= 130.0f) {
                logMessage("View: 3D Textured Viewport Active");
                return;
            }
            // Tools (x: 132..180)
            else if (mx >= 132.0f && mx <= 180.0f) {
                _browserOpen = !_browserOpen;
                logMessage("Tools: Toggled Texture Browser");
                return;
            }
            // Help (x: 182..225)
            else if (mx >= 182.0f && mx <= 225.0f) {
                logMessage("Lab Hammer 1.0 - Controls: RMB to Fly, E to Place, Ctrl+O Open, Ctrl+S Save");
                return;
            }
        }

        // Main Toolbar Clicks (y: 24..58)
        if (my >= 24.0f && my <= 58.0f) {
            // Button 0: New Map (x: 8..34)
            if (mx >= 8.0f && mx <= 34.0f) {
                newMap();
                return;
            }
            // Button 1: Open Map (x: 36..62)
            else if (mx >= 36.0f && mx <= 62.0f) {
                openMapDialog();
                return;
            }
            // Button 2: Save Map (x: 64..90)
            else if (mx >= 64.0f && mx <= 90.0f) {
                saveMapAction(false);
                return;
            }
            // Button 3: Undo (x: 92..118)
            else if (mx >= 92.0f && mx <= 118.0f) {
                if (_map && !_map->brushes.empty()) {
                    _map->brushes.pop_back();
                    logMessage("Undo: Deleted last brush.");
                }
                return;
            }
            // Button 4: Texture Browser (x: 120..146)
            else if (mx >= 120.0f && mx <= 146.0f) {
                _browserOpen = !_browserOpen;
                return;
            }
        }
    }


    void onRender() override {
        // 1. Render 3D World Viewport
        Renderer::beginFrame(_camera);

        if (_map) {
            for (const auto& b : _map->brushes) {
                Texture* tex = b.texturePath.empty() ? nullptr : _textures[b.texturePath].get();
                Renderer::drawCube(b.position, b.size, b.color, tex);
            }
            for (const auto& d : _map->doors) {
                Renderer::drawCube(d.position, d.size, d.color);
            }
        }

        // Draw 3D Hammer Wireframe Box & Axis indicators at cursor
        Vec3 hammerOrange{ 1.0f, 0.55f, 0.1f };
        Renderer::drawCube(_cursorPos, _brushSize, hammerOrange, false);
        Renderer::drawCube(_cursorPos + Vec3(1.5f, 0, 0), Vec3(1.0f, 0.05f, 0.05f), Vec3(1, 0, 0), false);
        Renderer::drawCube(_cursorPos + Vec3(0, 1.5f, 0), Vec3(0.05f, 1.0f, 0.05f), Vec3(0, 1, 0), false);
        Renderer::drawCube(_cursorPos + Vec3(0, 0, 1.5f), Vec3(0.05f, 0.05f, 1.0f), Vec3(0, 0.5f, 1), false);

        // 2. Render 2D Valve Hammer Desktop Interface
        drawHammerInterface();

        // 3. Render Modal Texture Browser if open
        if (_browserOpen) {
            drawTextureBrowser();
        }

        Renderer::endFrame();
    }

    // Custom Icon Renderers for classic Hammer Toolbar
    static void drawHammerIcon(int iconId, float x, float y, const Vec3& color, const Vec3& bg) {
        Renderer::drawRect(x, y, 24.0f, 24.0f, bg);
        Renderer::drawRect(x, y, 24.0f, 1.0f, Vec3(0.7f, 0.7f, 0.7f));
        Renderer::drawRect(x, y + 23.0f, 24.0f, 1.0f, Vec3(0.5f, 0.5f, 0.5f));

        switch(iconId) {
            case 0: // Pointer / Selection Arrow
                Renderer::drawRect(x + 5.0f, y + 5.0f, 3.0f, 13.0f, color);
                Renderer::drawRect(x + 8.0f, y + 8.0f, 3.0f, 8.0f, color);
                Renderer::drawRect(x + 11.0f, y + 11.0f, 3.0f, 4.0f, color);
                Renderer::drawRect(x + 8.0f, y + 14.0f, 5.0f, 3.0f, color);
                break;
            case 1: // 3D Block / Cube Brush
                Renderer::drawRect(x + 5.0f, y + 5.0f, 14.0f, 14.0f, color);
                Renderer::drawRect(x + 7.0f, y + 7.0f, 10.0f, 10.0f, bg);
                Renderer::drawRect(x + 9.0f, y + 9.0f, 6.0f, 6.0f, color);
                break;
            case 2: // Entity Lightbulb / Lamp
                Renderer::drawRect(x + 8.0f, y + 4.0f, 8.0f, 8.0f, color);
                Renderer::drawRect(x + 10.0f, y + 12.0f, 4.0f, 5.0f, color);
                Renderer::drawRect(x + 6.0f, y + 8.0f, 12.0f, 2.0f, color);
                break;
            case 3: // Texture / Material Application
                Renderer::drawRect(x + 4.0f, y + 4.0f, 16.0f, 16.0f, color);
                Renderer::drawRect(x + 4.0f, y + 4.0f, 8.0f, 8.0f, Vec3(0.1f, 0.1f, 0.1f));
                Renderer::drawRect(x + 12.0f, y + 12.0f, 8.0f, 8.0f, Vec3(0.1f, 0.1f, 0.1f));
                break;
            case 4: // Face Edit
                Renderer::drawRect(x + 5.0f, y + 5.0f, 14.0f, 14.0f, color);
                Renderer::drawRect(x + 7.0f, y + 7.0f, 10.0f, 10.0f, Vec3(0.9f, 0.2f, 0.2f));
                break;
            case 5: // Decal tool
                Renderer::drawRect(x + 7.0f, y + 5.0f, 10.0f, 14.0f, color);
                Renderer::drawRect(x + 9.0f, y + 7.0f, 6.0f, 4.0f, bg);
                break;
            case 6: // Clipping / Knife Tool
                Renderer::drawRect(x + 5.0f, y + 5.0f, 14.0f, 2.0f, color);
                Renderer::drawRect(x + 7.0f, y + 7.0f, 10.0f, 2.0f, color);
                Renderer::drawRect(x + 9.0f, y + 9.0f, 6.0f, 2.0f, color);
                Renderer::drawRect(x + 11.0f, y + 11.0f, 2.0f, 8.0f, color);
                break;
            default: // Generic tool
                Renderer::drawRect(x + 6.0f, y + 6.0f, 12.0f, 12.0f, color);
                break;
        }
    }

    // Classic Hammer Top Toolbar Icons (New, Open, Save, Undo, Browser)
    static void drawToolbarIcon(int iconId, float x, float y, const Vec3& color, const Vec3& bg) {
        Renderer::drawRect(x, y, 24.0f, 24.0f, bg);
        Renderer::drawRect(x, y, 24.0f, 1.0f, Vec3(0.7f, 0.7f, 0.7f));
        Renderer::drawRect(x, y + 23.0f, 24.0f, 1.0f, Vec3(0.5f, 0.5f, 0.5f));

        switch(iconId) {
            case 0: // New Document (white page with folded corner)
                Renderer::drawRect(x + 6.0f, y + 4.0f, 11.0f, 15.0f, Vec3(1, 1, 1));
                Renderer::drawRect(x + 6.0f, y + 4.0f, 11.0f, 1.0f, color);
                Renderer::drawRect(x + 6.0f, y + 4.0f, 1.0f, 15.0f, color);
                Renderer::drawRect(x + 16.0f, y + 7.0f, 1.0f, 12.0f, color);
                Renderer::drawRect(x + 6.0f, y + 19.0f, 11.0f, 1.0f, color);
                Renderer::drawRect(x + 9.0f, y + 8.0f, 5.0f, 1.0f, color);
                Renderer::drawRect(x + 9.0f, y + 11.0f, 5.0f, 1.0f, color);
                Renderer::drawRect(x + 9.0f, y + 14.0f, 5.0f, 1.0f, color);
                break;
            case 1: // Open Folder
                Renderer::drawRect(x + 5.0f, y + 6.0f, 6.0f, 2.0f, Vec3(0.9f, 0.75f, 0.2f));
                Renderer::drawRect(x + 5.0f, y + 8.0f, 14.0f, 10.0f, Vec3(0.95f, 0.8f, 0.25f));
                Renderer::drawRect(x + 5.0f, y + 8.0f, 14.0f, 1.0f, Vec3(0.7f, 0.55f, 0.1f));
                Renderer::drawRect(x + 5.0f, y + 17.0f, 14.0f, 1.0f, Vec3(0.7f, 0.55f, 0.1f));
                break;
            case 2: // Save Floppy Disk
                Renderer::drawRect(x + 5.0f, y + 5.0f, 14.0f, 14.0f, Vec3(0.2f, 0.45f, 0.85f));
                Renderer::drawRect(x + 8.0f, y + 5.0f, 8.0f, 4.0f, Vec3(0.85f, 0.85f, 0.9f));
                Renderer::drawRect(x + 7.0f, y + 11.0f, 10.0f, 7.0f, Vec3(1, 1, 1));
                Renderer::drawRect(x + 8.0f, y + 13.0f, 8.0f, 1.0f, Vec3(0.3f, 0.4f, 0.5f));
                break;
            case 3: // Undo Arrow
                Renderer::drawRect(x + 6.0f, y + 11.0f, 9.0f, 2.0f, color);
                Renderer::drawRect(x + 13.0f, y + 7.0f, 2.0f, 6.0f, color);
                Renderer::drawRect(x + 6.0f, y + 9.0f, 2.0f, 6.0f, color);
                Renderer::drawRect(x + 8.0f, y + 10.0f, 2.0f, 4.0f, color);
                break;
            case 4: // Texture Browser Grid
                Renderer::drawRect(x + 5.0f, y + 5.0f, 14.0f, 14.0f, Vec3(0.3f, 0.35f, 0.4f));
                Renderer::drawRect(x + 6.0f, y + 6.0f, 5.0f, 5.0f, Vec3(0.9f, 0.5f, 0.1f));
                Renderer::drawRect(x + 13.0f, y + 6.0f, 5.0f, 5.0f, Vec3(0.2f, 0.7f, 0.9f));
                Renderer::drawRect(x + 6.0f, y + 13.0f, 5.0f, 5.0f, Vec3(0.8f, 0.8f, 0.85f));
                Renderer::drawRect(x + 13.0f, y + 13.0f, 5.0f, 5.0f, Vec3(0.4f, 0.45f, 0.5f));
                break;
            default:
                drawHammerIcon(iconId % 7, x, y, color, bg);
                break;
        }
    }

    void drawHammerInterface() {
        int w = 1600, h = 900;
        Renderer::beginUI(w, h);

        Vec3 winBg{ 0.94f, 0.94f, 0.94f };          // Classic Win32 Dialog Gray
        Vec3 winBorder{ 0.65f, 0.65f, 0.68f };      // Bevel Gray
        Vec3 textDark{ 0.12f, 0.12f, 0.12f };       // Black Text
        Vec3 textDim{ 0.45f, 0.45f, 0.45f };        // Dim Label
        Vec3 cyanGlow{ 0.2f, 0.75f, 0.95f };        // Frozen-Life Palette accent

        // ==================== 1. TOP TITLEBAR & MENUS ====================
        Renderer::drawRect(0, 0, (float)w, 24.0f, winBg);
        Renderer::drawRect(0, 23.0f, (float)w, 1.0f, winBorder);

        LabFont::drawText(14.0f, 5.0f, "File", 1.8f, textDark, LabFontType::System);
        LabFont::drawText(54.0f, 5.0f, "Edit", 1.8f, textDark, LabFontType::System);
        LabFont::drawText(94.0f, 5.0f, "View", 1.8f, textDark, LabFontType::System);
        LabFont::drawText(140.0f, 5.0f, "Tools", 1.8f, textDark, LabFontType::System);
        LabFont::drawText(190.0f, 5.0f, "Help", 1.8f, textDark, LabFontType::System);

        LabFont::drawText((float)w - 240.0f, 5.0f, "Hammer - Frozen-Life", 1.8f, Vec3(0.15f, 0.45f, 0.75f), LabFontType::GeoSans);

        // ==================== 2. MAIN TOOLBAR ====================
        float tbY = 24.0f;
        float tbH = 34.0f;
        Renderer::drawRect(0, tbY, (float)w, tbH, winBg);
        Renderer::drawRect(0, tbY + tbH - 1.0f, (float)w, 1.0f, winBorder);

        // Render actual icons for the top toolbar (New, Open, Save, Undo, Browser, tools...)
        for (int i = 0; i < 18; ++i) {
            float bx = 8.0f + i * 28.0f;
            drawToolbarIcon(i, bx, tbY + 5.0f, (i % 2 == 0) ? cyanGlow * 0.7f : Vec3(0.3f, 0.35f, 0.4f), Vec3(0.88f, 0.88f, 0.90f));
        }

        // ==================== 3. LEFT TOOLS PALETTE ====================
        float leftW = 42.0f;
        float leftY = tbY + tbH;
        float leftH = (float)h - leftY - 24.0f;
        Renderer::drawRect(0, leftY, leftW, leftH, winBg);
        Renderer::drawRect(leftW - 1.0f, leftY, 1.0f, leftH, winBorder);

        // Render actual individual icons for each tool on the left palette
        for (int i = 0; i < 7; ++i) {
            float ty = leftY + 10.0f + i * 36.0f;
            bool isSel = (_activeTool == i);
            Vec3 bgCol = isSel ? Vec3(0.78f, 0.88f, 1.0f) : Vec3(0.88f, 0.88f, 0.90f);
            Vec3 iconCol = isSel ? Vec3(1.0f, 0.55f, 0.1f) : Vec3(0.25f, 0.28f, 0.32f);

            Renderer::drawRect(6.0f, ty, 30.0f, 30.0f, bgCol);
            Renderer::drawRect(6.0f, ty, 30.0f, 1.0f, isSel ? cyanGlow : winBorder);
            drawHammerIcon(i, 9.0f, ty + 3.0f, iconCol, bgCol);
        }

        // ==================== 4. RIGHT SIDEBAR ====================
        float rightW = 280.0f;
        float rightX = (float)w - rightW;
        float rightY = leftY;
        float rightH = leftH;
        Renderer::drawRect(rightX, rightY, rightW, rightH, winBg);
        Renderer::drawRect(rightX, rightY, 1.0f, rightH, winBorder);

        // Section A: "Select:"
        LabFont::drawText(rightX + 12.0f, rightY + 10.0f, "Select:", 1.7f, textDark, LabFontType::System);
        Renderer::drawRect(rightX + 12.0f, rightY + 28.0f, 120.0f, 22.0f, Vec3(0.88f, 0.88f, 0.90f));
        LabFont::drawText(rightX + 22.0f, rightY + 34.0f, "Groups", 1.6f, textDim, LabFontType::System);
        Renderer::drawRect(rightX + 12.0f, rightY + 54.0f, 120.0f, 22.0f, Vec3(0.88f, 0.88f, 0.90f));
        LabFont::drawText(rightX + 22.0f, rightY + 60.0f, "Objects", 1.6f, textDim, LabFontType::System);
        Renderer::drawRect(rightX + 12.0f, rightY + 80.0f, 120.0f, 22.0f, Vec3(0.88f, 0.88f, 0.90f));
        LabFont::drawText(rightX + 22.0f, rightY + 86.0f, "Solids", 1.6f, textDim, LabFontType::System);

        // Section B: "Texture group:" & "Current texture:"
        float texSecY = rightY + 115.0f;
        LabFont::drawText(rightX + 12.0f, texSecY, "Texture group:", 1.7f, textDark, LabFontType::System);
        Renderer::drawRect(rightX + 12.0f, texSecY + 16.0f, 256.0f, 22.0f, Vec3(1, 1, 1));
        Renderer::drawRect(rightX + 12.0f, texSecY + 16.0f, 256.0f, 1.0f, winBorder);
        LabFont::drawText(rightX + 20.0f, texSecY + 22.0f, "All Textures (" + std::to_string(_availableTextures.size()) + ")", 1.6f, textDark, LabFontType::System);

        LabFont::drawText(rightX + 12.0f, texSecY + 45.0f, "Current texture:", 1.7f, textDark, LabFontType::System);
        Renderer::drawRect(rightX + 12.0f, texSecY + 62.0f, 256.0f, 22.0f, Vec3(1, 1, 1));
        Renderer::drawRect(rightX + 12.0f, texSecY + 62.0f, 256.0f, 1.0f, winBorder);
        LabFont::drawText(rightX + 20.0f, texSecY + 68.0f, _selectedTexture, 1.6f, textDark, LabFontType::System);

        // Texture Thumbnail Preview Box (Exact match to Valve Hammer)
        float thumbX = rightX + 12.0f;
        float thumbY = texSecY + 92.0f;
        float thumbS = 85.0f;
        Renderer::drawRect(thumbX, thumbY, thumbS, thumbS, Vec3(0, 0, 0));

        // Draw Actual 2D Texture Thumbnail Preview
        if (_textures.contains(_selectedTexture)) {
            Renderer::drawTextureRect(thumbX + 2.0f, thumbY + 2.0f, thumbS - 4.0f, thumbS - 4.0f, *_textures[_selectedTexture]);
        }

        // Browse... & Apply Texture Buttons
        Renderer::drawRect(rightX + 105.0f, thumbY + 12.0f, 160.0f, 28.0f, Vec3(0.88f, 0.88f, 0.90f));
        Renderer::drawRect(rightX + 105.0f, thumbY + 12.0f, 160.0f, 1.0f, winBorder);
        LabFont::drawText(rightX + 115.0f, thumbY + 20.0f, "Browse Textures...", 1.6f, textDark, LabFontType::System);

        Renderer::drawRect(rightX + 105.0f, thumbY + 48.0f, 160.0f, 28.0f, Vec3(0.88f, 0.88f, 0.90f));
        Renderer::drawRect(rightX + 105.0f, thumbY + 48.0f, 160.0f, 1.0f, winBorder);
        LabFont::drawText(rightX + 120.0f, thumbY + 56.0f, "Apply to Brush", 1.6f, textDark, LabFontType::System);

        // Section C: "VisGroups:" Box
        float visY = thumbY + thumbS + 18.0f;
        LabFont::drawText(rightX + 12.0f, visY, "VisGroups:", 1.7f, textDark, LabFontType::System);
        Renderer::drawRect(rightX + 12.0f, visY + 16.0f, 256.0f, 130.0f, Vec3(1, 1, 1));
        Renderer::drawRect(rightX + 12.0f, visY + 16.0f, 256.0f, 1.0f, winBorder);

        LabFont::drawText(rightX + 20.0f, visY + 26.0f, "[x] World Geometry", 1.6f, textDark, LabFontType::System);
        LabFont::drawText(rightX + 20.0f, visY + 46.0f, "[x] Entities & Props", 1.6f, textDark, LabFontType::System);
        LabFont::drawText(rightX + 20.0f, visY + 66.0f, "[x] Dynamic Doors", 1.6f, textDark, LabFontType::System);
        LabFont::drawText(rightX + 20.0f, visY + 86.0f, "[x] Player Spawns", 1.6f, textDark, LabFontType::System);

        // ==================== 5. BOTTOM CONSOLE / "Messages" WINDOW ====================
        float conW = 750.0f;
        float conH = 140.0f;
        float conX = leftW + 30.0f;
        float conY = (float)h - conH - 35.0f;

        Renderer::drawRect(conX, conY, conW, conH, Vec3(1, 1, 1));
        Renderer::drawRect(conX, conY, conW, 22.0f, Vec3(0.85f, 0.90f, 0.96f));
        Renderer::drawRect(conX, conY, conW, 1.0f, winBorder);
        Renderer::drawRect(conX, conY + conH - 1.0f, conW, 1.0f, winBorder);
        Renderer::drawRect(conX, conY, 1.0f, conH, winBorder);
        Renderer::drawRect(conX + conW - 1.0f, conY, 1.0f, conH, winBorder);

        LabFont::drawText(conX + 10.0f, conY + 6.0f, "Messages", 1.7f, textDark, LabFontType::System);

        for (int i = 0; i < (int)_consoleMessages.size(); ++i) {
            LabFont::drawText(conX + 12.0f, conY + 30.0f + i * 14.0f, _consoleMessages[i], 1.5f, Vec3(0.1f, 0.15f, 0.2f), LabFontType::System);
        }

        // ==================== 6. STATUS BAR ====================
        float sbY = (float)h - 22.0f;
        Renderer::drawRect(0, sbY, (float)w, 22.0f, winBg);
        Renderer::drawRect(0, sbY, (float)w, 1.0f, winBorder);

        std::string sbText = "Hold RMB: Fly & Look | E: Place | Ctrl+O: Open | Ctrl+S: Save | Map: " + 
                             (_currentMapPath.empty() ? "Untitled" : _currentMapPath);
        LabFont::drawText(10.0f, sbY + 5.0f, sbText, 1.6f, textDark, LabFontType::System);
        std::string gridStr = "Snap: " + std::to_string((int)_gridSnap);
        LabFont::drawText((float)w - 280.0f, sbY + 5.0f, gridStr, 1.6f, textDark, LabFontType::System);

        // ==================== 7. DROPDOWN FILE MENU ====================
        if (_fileMenuOpen) {
            float menuX = 10.0f;
            float menuY = 24.0f;
            float menuW = 190.0f;
            float menuH = 125.0f;
            Vec3 menuBg{ 0.96f, 0.96f, 0.97f };
            Vec3 menuBorder{ 0.55f, 0.55f, 0.60f };
            Vec3 menuShadow{ 0.2f, 0.2f, 0.2f };

            // Drop shadow & Menu frame
            Renderer::drawRect(menuX + 3.0f, menuY + 3.0f, menuW, menuH, menuShadow * 0.35f);
            Renderer::drawRect(menuX, menuY, menuW, menuH, menuBg);
            Renderer::drawRect(menuX, menuY, menuW, 1.0f, menuBorder);
            Renderer::drawRect(menuX, menuY, 1.0f, menuH, menuBorder);
            Renderer::drawRect(menuX + menuW - 1.0f, menuY, 1.0f, menuH, menuBorder);
            Renderer::drawRect(menuX, menuY + menuH - 1.0f, menuW, 1.0f, menuBorder);

            struct MenuItem { std::string name; std::string shortcut; };
            MenuItem items[] = {
                { "New Map", "Ctrl+N" },
                { "Open Map...", "Ctrl+O" },
                { "Save Map", "Ctrl+S" },
                { "Save Map As...", "" },
                { "Exit", "Alt+F4" }
            };

            for (int i = 0; i < 5; ++i) {
                float iy = menuY + 3.0f + i * 24.0f;
                if (i == 4) {
                    Renderer::drawRect(menuX + 6.0f, iy - 2.0f, menuW - 12.0f, 1.0f, menuBorder);
                }
                LabFont::drawText(menuX + 14.0f, iy + 4.0f, items[i].name, 1.6f, textDark, LabFontType::System);
                if (!items[i].shortcut.empty()) {
                    LabFont::drawText(menuX + menuW - 65.0f, iy + 4.0f, items[i].shortcut, 1.5f, textDim, LabFontType::System);
                }
            }
        }

        Renderer::endUI();
    }


    // Modal Texture Browser Gallery
    void drawTextureBrowser() {
        int w = 1600, h = 900;
        Renderer::beginUI(w, h);

        // Dim background overlay
        Renderer::drawRect(0, 0, (float)w, (float)h, Vec3(0.05f, 0.06f, 0.08f));

        // Window Frame
        float bw = 820.0f;
        float bh = 600.0f;
        float bx = ((float)w - bw) * 0.5f;
        float by = ((float)h - bh) * 0.5f;

        Renderer::drawRect(bx, by, bw, bh, Vec3(0.92f, 0.92f, 0.94f));
        Renderer::drawRect(bx, by, bw, 28.0f, Vec3(0.2f, 0.35f, 0.55f)); // Titlebar
        LabFont::drawText(bx + 14.0f, by + 8.0f, "Texture Browser - Choose Surface Material", 1.8f, Vec3(1, 1, 1), LabFontType::System);

        // Close 'X' Button
        Renderer::drawRect(bx + bw - 32.0f, by + 4.0f, 24.0f, 20.0f, Vec3(0.85f, 0.25f, 0.25f));
        LabFont::drawText(bx + bw - 25.0f, by + 7.0f, "X", 1.8f, Vec3(1, 1, 1), LabFontType::System);

        // Texture Gallery Grid (6 columns)
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

            // Thumbnail Border / Highlight
            Renderer::drawRect(tx - 3.0f, ty - 3.0f, thumbSize + 6.0f, thumbSize + 22.0f, isSelected ? Vec3(1.0f, 0.55f, 0.1f) : Vec3(0.7f, 0.72f, 0.75f));
            Renderer::drawRect(tx, ty, thumbSize, thumbSize, Vec3(0, 0, 0));

            // Render Actual 2D Texture Image
            if (_textures.contains(_availableTextures[i].filename)) {
                Renderer::drawTextureRect(tx, ty, thumbSize, thumbSize, *_textures[_availableTextures[i].filename]);
            }

            // Label underneath
            std::string label = _availableTextures[i].filename;
            if (label.size() > 12) label = label.substr(0, 10) + "..";
            LabFont::drawText(tx, ty + thumbSize + 4.0f, label, 1.4f, Vec3(0.1f, 0.1f, 0.1f), LabFontType::System);
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
    std::vector<TextureEntry> _availableTextures;
    std::string _selectedTexture = "concrete_wall.bmp";
    int _textureIndex = 0;
    bool _browserOpen = false;
    bool _fileMenuOpen = false;
    std::string _currentMapPath = "";

    int _activeTool = 1; // 1 = Brush Tool
    Vec3 _cursorPos{ 0, 0, 0 };
    Vec3 _brushSize{ 2.0f, 2.0f, 2.0f };
    float _gridSnap = 1.0f;

    std::vector<std::string> _consoleMessages;

    bool _lmbPressed = false;
    bool _ePressed = false;
    bool _ctrlSPressed = false;
    bool _ctrlOPressed = false;
    bool _ctrlNPressed = false;
    bool _delPressed = false;
};

int main() {
    LabHammerStandalone hammer;
    hammer.run();
    return 0;
}
