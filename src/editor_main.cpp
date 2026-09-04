#include "Lab.h"
#include "LabFont.h"
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
            floor.color = Vec3(0.5f, 0.5f, 0.5f);
            floor.texturePath = "wall_concrete.bmp";
            _map->brushes.push_back(floor);
        }

        // Camera initial pose
        _camera.setPosition(Vec3(0, 6.0f, 14.0f));

        // Unlock mouse cursor for full desktop UI interaction
        glfwSetInputMode(getWindow(), GLFW_CURSOR, GLFW_CURSOR_NORMAL);

        logMessage("Search Path (GAME) : assets/textures/");
        logMessage("Search Path (GAME) : assets/models/");
        logMessage("Search Path (GAME) : assets/maps/");
        logMessage("Hammer initialized. Ready.");
    }

    void discoverTextures() {
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
            _availableTextures.push_back({ "Test.bmp", "Test.bmp" });
            _textures["Test.bmp"] = std::make_unique<Texture>("Test.bmp");
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
        // Noclip camera movement (active when holding Right Mouse Button or when hovering viewport)
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
        // E: Place Brush or Entity with active texture
        if (Input::isKeyPressed('E') || Input::isKeyPressed('e')) {
            if (!_ePressed && _map) {
                placeCurrentObject();
                _ePressed = true;
            }
        } else {
            _ePressed = false;
        }

        // Backspace / Delete: Undo last brush
        if (Input::isKeyPressed(259)) {
            if (!_delPressed && _map && !_map->brushes.empty()) {
                _map->brushes.pop_back();
                logMessage("Deleted last brush.");
                _delPressed = true;
            }
        } else {
            _delPressed = false;
        }

        // K or Ctrl+S: Save Map
        if (Input::isKeyPressed('K') || Input::isKeyPressed('k')) {
            if (!_kPressed && _map) {
                _map->saveToFile("assets/maps/hammer_export.labmap");
                logMessage("Map saved successfully to assets/maps/hammer_export.labmap");
                _kPressed = true;
            }
        } else {
            _kPressed = false;
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
        // Left Toolbar Tools (x: 5..35)
        if (mx >= 5.0f && mx <= 35.0f) {
            float startY = 70.0f;
            for (int i = 0; i < 8; ++i) {
                float ty = startY + i * 36.0f;
                if (my >= ty && my <= ty + 32.0f) {
                    _activeTool = i;
                    if (i == 0) logMessage("Tool: Selection Tool");
                    else if (i == 1) logMessage("Tool: Block / Brush Tool");
                    else if (i == 2) logMessage("Tool: Entity / Prop Tool");
                    else if (i == 3) logMessage("Tool: Texture Application Tool");
                    else if (i == 4) logMessage("Tool: Clipping Tool");
                    return;
                }
            }
        }

        // Right Sidebar Texture Picker Click (x: 1380..1580, y: 310..420)
        if (mx >= 1380.0f && mx <= 1580.0f && my >= 280.0f && my <= 430.0f) {
            // Next texture cycle
            _textureIndex = (_textureIndex + 1) % _availableTextures.size();
            _selectedTexture = _availableTextures[_textureIndex].filename;
            logMessage("Selected Texture: " + _selectedTexture);
            return;
        }

        // Top Menu Bar Clicks
        if (my >= 0.0f && my <= 24.0f) {
            if (mx >= 10.0f && mx <= 45.0f) {
                // File -> Save
                if (_map) {
                    _map->saveToFile("assets/maps/hammer_export.labmap");
                    logMessage("File -> Saved assets/maps/hammer_export.labmap");
                }
            } else if (mx >= 50.0f && mx <= 90.0f) {
                // Tools -> Place Brush
                placeCurrentObject();
            }
        }
    }

    void onRender() override {
        // 1. Render 3D World Viewport
        Renderer::beginFrame(_camera);

        if (_map) {
            // Brushes with their chosen textures
            for (const auto& b : _map->brushes) {
                Texture* tex = b.texturePath.empty() ? nullptr : _textures[b.texturePath].get();
                Renderer::drawCube(b.position, b.size, b.color, tex);
            }
            // Doors
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

        Renderer::endFrame();
    }

    void drawHammerInterface() {
        int w = 1600, h = 900;
        Renderer::beginUI(w, h);

        // Valve Hammer Desktop Gray Colors (Exact reference match)
        Vec3 winBg{ 0.94f, 0.94f, 0.94f };          // Classic Win32 Dialog Gray
        Vec3 winBorder{ 0.65f, 0.65f, 0.68f };      // Bevel Gray
        Vec3 textDark{ 0.12f, 0.12f, 0.12f };       // Black Text
        Vec3 textDim{ 0.45f, 0.45f, 0.45f };        // Dim Label
        Vec3 toolDark{ 0.35f, 0.37f, 0.40f };       // Dark Tool Button
        Vec3 cyanGlow{ 0.2f, 0.75f, 0.95f };        // Frozen-Life Palette accent

        // ==================== 1. TOP TITLEBAR & MENUS ====================
        // Menu Bar (File, Edit, View, Tools, Help)
        Renderer::drawRect(0, 0, (float)w, 24.0f, winBg);
        Renderer::drawRect(0, 23.0f, (float)w, 1.0f, winBorder);

        LabFont::drawText(10.0f, 6.0f, "File", 1.8f, textDark);
        LabFont::drawText(50.0f, 6.0f, "Edit", 1.8f, textDark);
        LabFont::drawText(90.0f, 6.0f, "View", 1.8f, textDark);
        LabFont::drawText(130.0f, 6.0f, "Tools", 1.8f, textDark);
        LabFont::drawText(180.0f, 6.0f, "Help", 1.8f, textDark);

        // Title text in corner
        LabFont::drawText((float)w - 240.0f, 6.0f, "Hammer - Frozen-Life", 1.7f, Vec3(0.2f, 0.4f, 0.6f));

        // ==================== 2. MAIN TOOLBAR (Row of Action Icons) ====================
        float tbY = 24.0f;
        float tbH = 34.0f;
        Renderer::drawRect(0, tbY, (float)w, tbH, winBg);
        Renderer::drawRect(0, tbY + tbH - 1.0f, (float)w, 1.0f, winBorder);

        // Grid icon boxes (24x24 icons)
        for (int i = 0; i < 18; ++i) {
            float bx = 8.0f + i * 28.0f;
            Renderer::drawRect(bx, tbY + 5.0f, 24.0f, 24.0f, Vec3(0.88f, 0.88f, 0.90f));
            Renderer::drawRect(bx, tbY + 5.0f, 24.0f, 1.0f, winBorder);
            // Inner icon symbol glyph
            Renderer::drawRect(bx + 6.0f, tbY + 11.0f, 12.0f, 12.0f, (i % 2 == 0) ? cyanGlow * 0.7f : Vec3(0.4f, 0.4f, 0.4f));
        }

        // ==================== 3. LEFT TOOLS PALETTE (Selection, Brush, Clip, etc.) ====================
        float leftW = 42.0f;
        float leftY = tbY + tbH;
        float leftH = (float)h - leftY - 24.0f;
        Renderer::drawRect(0, leftY, leftW, leftH, winBg);
        Renderer::drawRect(leftW - 1.0f, leftY, 1.0f, leftH, winBorder);

        // Vertical Tool Buttons
        for (int i = 0; i < 8; ++i) {
            float ty = leftY + 10.0f + i * 36.0f;
            bool isSel = (_activeTool == i);
            Renderer::drawRect(6.0f, ty, 30.0f, 30.0f, isSel ? Vec3(0.78f, 0.85f, 0.95f) : Vec3(0.85f, 0.85f, 0.87f));
            Renderer::drawRect(6.0f, ty, 30.0f, 1.0f, isSel ? cyanGlow : winBorder);
            // Icon inner square
            Renderer::drawRect(12.0f, ty + 6.0f, 18.0f, 18.0f, isSel ? Vec3(1.0f, 0.55f, 0.1f) : toolDark);
        }

        // ==================== 4. RIGHT SIDEBAR (Texture, Groups, VisGroups, Manifest) ====================
        float rightW = 260.0f;
        float rightX = (float)w - rightW;
        float rightY = leftY;
        float rightH = leftH;
        Renderer::drawRect(rightX, rightY, rightW, rightH, winBg);
        Renderer::drawRect(rightX, rightY, 1.0f, rightH, winBorder);

        // Section A: "Select:"
        LabFont::drawText(rightX + 12.0f, rightY + 10.0f, "Select:", 1.7f, textDark);
        Renderer::drawRect(rightX + 12.0f, rightY + 28.0f, 110.0f, 22.0f, Vec3(0.88f, 0.88f, 0.90f));
        LabFont::drawText(rightX + 22.0f, rightY + 34.0f, "Groups", 1.6f, textDim);
        Renderer::drawRect(rightX + 12.0f, rightY + 54.0f, 110.0f, 22.0f, Vec3(0.88f, 0.88f, 0.90f));
        LabFont::drawText(rightX + 22.0f, rightY + 60.0f, "Objects", 1.6f, textDim);
        Renderer::drawRect(rightX + 12.0f, rightY + 80.0f, 110.0f, 22.0f, Vec3(0.88f, 0.88f, 0.90f));
        LabFont::drawText(rightX + 22.0f, rightY + 86.0f, "Solids", 1.6f, textDim);

        // Section B: "Texture group:" & "Current texture:"
        float texSecY = rightY + 115.0f;
        LabFont::drawText(rightX + 12.0f, texSecY, "Texture group:", 1.7f, textDark);
        Renderer::drawRect(rightX + 12.0f, texSecY + 16.0f, 236.0f, 22.0f, Vec3(1, 1, 1));
        Renderer::drawRect(rightX + 12.0f, texSecY + 16.0f, 236.0f, 1.0f, winBorder);
        LabFont::drawText(rightX + 20.0f, texSecY + 22.0f, "All Textures", 1.6f, textDark);

        LabFont::drawText(rightX + 12.0f, texSecY + 45.0f, "Current texture:", 1.7f, textDark);
        Renderer::drawRect(rightX + 12.0f, texSecY + 62.0f, 236.0f, 22.0f, Vec3(1, 1, 1));
        Renderer::drawRect(rightX + 12.0f, texSecY + 62.0f, 236.0f, 1.0f, winBorder);
        LabFont::drawText(rightX + 20.0f, texSecY + 68.0f, _selectedTexture, 1.6f, textDark);

        // Texture Thumbnail Preview Box (Exact match to big black box in Hammer screenshot!)
        float thumbX = rightX + 12.0f;
        float thumbY = texSecY + 92.0f;
        float thumbS = 85.0f;
        Renderer::drawRect(thumbX, thumbY, thumbS, thumbS, Vec3(0, 0, 0)); // Black backdrop

        // Draw Actual 2D Texture Thumbnail Preview
        if (_textures.contains(_selectedTexture)) {
            Renderer::drawTextureRect(thumbX + 2.0f, thumbY + 2.0f, thumbS - 4.0f, thumbS - 4.0f, *_textures[_selectedTexture]);
        }

        // Browse... & Replace... Buttons
        Renderer::drawRect(rightX + 110.0f, thumbY + 12.0f, 138.0f, 26.0f, Vec3(0.88f, 0.88f, 0.90f));
        Renderer::drawRect(rightX + 110.0f, thumbY + 12.0f, 138.0f, 1.0f, winBorder);
        LabFont::drawText(rightX + 125.0f, thumbY + 20.0f, "Browse... [Click]", 1.6f, textDark);

        Renderer::drawRect(rightX + 110.0f, thumbY + 46.0f, 138.0f, 26.0f, Vec3(0.88f, 0.88f, 0.90f));
        Renderer::drawRect(rightX + 110.0f, thumbY + 46.0f, 138.0f, 1.0f, winBorder);
        LabFont::drawText(rightX + 125.0f, thumbY + 54.0f, "Apply Texture", 1.6f, textDark);

        // Section C: "VisGroups:" Box
        float visY = thumbY + thumbS + 18.0f;
        LabFont::drawText(rightX + 12.0f, visY, "VisGroups:", 1.7f, textDark);
        Renderer::drawRect(rightX + 12.0f, visY + 16.0f, 236.0f, 130.0f, Vec3(1, 1, 1));
        Renderer::drawRect(rightX + 12.0f, visY + 16.0f, 236.0f, 1.0f, winBorder);

        // Visgroup items
        LabFont::drawText(rightX + 20.0f, visY + 26.0f, "[x] World Geometry", 1.6f, textDark);
        LabFont::drawText(rightX + 20.0f, visY + 46.0f, "[x] Entities & Props", 1.6f, textDark);
        LabFont::drawText(rightX + 20.0f, visY + 66.0f, "[x] Dynamic Doors", 1.6f, textDark);
        LabFont::drawText(rightX + 20.0f, visY + 86.0f, "[x] Player Spawns", 1.6f, textDark);

        // ==================== 5. BOTTOM CONSOLE / "Messages" WINDOW ====================
        // Floating tool window matched to bottom of reference screenshot
        float conW = 750.0f;
        float conH = 140.0f;
        float conX = leftW + 30.0f;
        float conY = (float)h - conH - 35.0f;

        // Window Frame
        Renderer::drawRect(conX, conY, conW, conH, Vec3(1, 1, 1));
        // Windows Aero / Classic Titlebar
        Renderer::drawRect(conX, conY, conW, 22.0f, Vec3(0.85f, 0.90f, 0.96f));
        Renderer::drawRect(conX, conY, conW, 1.0f, winBorder);
        Renderer::drawRect(conX, conY + conH - 1.0f, conW, 1.0f, winBorder);
        Renderer::drawRect(conX, conY, 1.0f, conH, winBorder);
        Renderer::drawRect(conX + conW - 1.0f, conY, 1.0f, conH, winBorder);

        LabFont::drawText(conX + 10.0f, conY + 6.0f, "Messages", 1.7f, textDark);

        // Print Search Path / Log messages
        for (int i = 0; i < (int)_consoleMessages.size(); ++i) {
            LabFont::drawText(conX + 12.0f, conY + 30.0f + i * 14.0f, _consoleMessages[i], 1.5f, Vec3(0.1f, 0.15f, 0.2f));
        }

        // ==================== 6. STATUS BAR AT VERY BOTTOM ====================
        float sbY = (float)h - 22.0f;
        Renderer::drawRect(0, sbY, (float)w, 22.0f, winBg);
        Renderer::drawRect(0, sbY, (float)w, 1.0f, winBorder);

        LabFont::drawText(10.0f, sbY + 5.0f, "For Help, press F1 | Hold RMB: Fly & Look | E: Place Brush | K: Save Map", 1.6f, textDark);
        std::string gridStr = "Snap: " + std::to_string((int)_gridSnap);
        LabFont::drawText((float)w - 280.0f, sbY + 5.0f, gridStr, 1.6f, textDark);

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
    std::string _selectedTexture = "wall_concrete.bmp";
    int _textureIndex = 0;

    int _activeTool = 1; // 1 = Brush Tool
    Vec3 _cursorPos{ 0, 0, 0 };
    Vec3 _brushSize{ 2.0f, 2.0f, 2.0f };
    float _gridSnap = 1.0f;

    std::vector<std::string> _consoleMessages;

    bool _lmbPressed = false;
    bool _ePressed = false;
    bool _kPressed = false;
    bool _delPressed = false;
};

int main() {
    LabHammerStandalone hammer;
    hammer.run();
    return 0;
}
