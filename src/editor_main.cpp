#include "Lab.h"
#include <iostream>
#include <filesystem>
#include <unordered_map>
#include <GLFW/glfw3.h>

using namespace Lab;

class LabHammerStandalone : public Engine {
public:
    LabHammerStandalone()
        : Engine("Lab Hammer Map Editor (Source 2 / HL2 Style)", 1600, 900),
          _camera(75.0f, 16.0f / 9.0f, 0.01f, 2000.0f) {
    }

    void onInit() override {
        LabLog::info("Launching Standalone Lab Hammer Editor...");
        Renderer::init();

        // Check if facility_alpha exists or create new empty map
        std::string targetMap = "assets/maps/facility_alpha.labmap";
        if (std::filesystem::exists(targetMap)) {
            _map = LabMap::loadFromFile(targetMap);
        }
        if (!_map) {
            _map = std::make_unique<LabMap>();
            _map->metadata.name = "Custom Hammer Level";
            _map->metadata.author = "Mapper";
            _map->spawn.position = Vec3(0, 2.0f, 0);

            // Add standard floor baseplate
            MapBrush floor;
            floor.position = Vec3(0, -0.5f, 0);
            floor.size = Vec3(40.0f, 1.0f, 40.0f);
            floor.color = Vec3(0.3f, 0.35f, 0.4f);
            floor.texturePath = "Test.bmp";
            _map->brushes.push_back(floor);
        }

        _textures["Test.bmp"] = std::make_unique<Texture>("Test.bmp");

        _editor.active = true;
        _camera.setPosition(Vec3(0, 5.0f, 15.0f));

        // Unlock mouse cursor for UI editor controls
        glfwSetInputMode(getWindow(), GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    }

    void onFixedUpdate(float fixedDelta) override {
        // Freecam fly navigation in editor
        bool isFast = Input::isKeyPressed(340); // Shift
        float flySpeed = isFast ? 30.0f : 15.0f;

        Vec3 camPos = _camera.getPosition();
        if (Input::isKeyPressed('W') || Input::isKeyPressed('w')) camPos += _camera.getFront() * flySpeed * fixedDelta;
        if (Input::isKeyPressed('S') || Input::isKeyPressed('s')) camPos -= _camera.getFront() * flySpeed * fixedDelta;
        if (Input::isKeyPressed('A') || Input::isKeyPressed('a')) camPos -= _camera.getRight() * flySpeed * fixedDelta;
        if (Input::isKeyPressed('D') || Input::isKeyPressed('d')) camPos += _camera.getRight() * flySpeed * fixedDelta;
        if (Input::isKeyPressed(32)) camPos.y += flySpeed * fixedDelta; // Space
        if (Input::isKeyPressed(341)) camPos.y -= flySpeed * fixedDelta; // Ctrl

        _camera.setPosition(camPos);
    }

    void onUpdate(const Time& time) override {
        // Right Mouse Button = Look around
        if (Input::isMouseButtonPressed(1)) {
            _camera.update(Input::mouseDelta);
        }

        if (_map) {
            _editor.update(time.delta, _camera, *_map);

            // E key = Place object
            if (Input::isKeyPressed('E') || Input::isKeyPressed('e')) {
                if (!_ePressed) {
                    if (_editor.currentTool == EditorTool::CreateBrush) _editor.placeBrush(*_map);
                    else if (_editor.currentTool == EditorTool::CreateDoor) _editor.placeDoor(*_map);
                    _ePressed = true;
                }
            } else {
                _ePressed = false;
            }

            // 1 key = Select Brush Tool
            if (Input::isKeyPressed('1')) _editor.currentTool = EditorTool::CreateBrush;
            // 2 key = Select Dynamic Door Tool
            if (Input::isKeyPressed('2')) _editor.currentTool = EditorTool::CreateDoor;

            // K key or Ctrl+S = Quick Save Map
            if (Input::isKeyPressed('K') || Input::isKeyPressed('k')) {
                if (!_kPressed) {
                    _editor.saveMap(*_map, "assets/maps/hammer_export.labmap");
                    LabLog::info("Map exported to assets/maps/hammer_export.labmap");
                    _kPressed = true;
                }
            } else {
                _kPressed = false;
            }

            // Backspace / Delete = Undo last brush
            if (Input::isKeyPressed(259)) {
                if (!_delPressed) {
                    _editor.deleteLast(*_map);
                    _delPressed = true;
                }
            } else {
                _delPressed = false;
            }

            // [ and ] keys = Resize brush cursor
            if (Input::isKeyPressed(93)) { // ']'
                _editor.cursor.brushSize += Vec3(0.5f, 0.5f, 0.5f) * time.delta * 4.0f;
            }
            if (Input::isKeyPressed(91)) { // '['
                _editor.cursor.brushSize -= Vec3(0.5f, 0.5f, 0.5f) * time.delta * 4.0f;
                if (_editor.cursor.brushSize.x < 0.5f) _editor.cursor.brushSize = Vec3(0.5f, 0.5f, 0.5f);
            }
        }
    }

    void onRender() override {
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

        // Draw 3D Hammer Wireframe/Axes & Cursor
        _editor.draw3DOverlay();

        // Draw Hammer Editor Toolbar, Menus & Help overlay
        drawEditorChrome();

        Renderer::endFrame();
    }

    void drawEditorChrome() {
        int w = 1600, h = 900;
        _editor.drawUI(w, h);

        Renderer::beginUI(w, h);

        // Sidebar Tools Panel (Hammer Palette)
        Renderer::drawRect(0, 42.0f, 60.0f, (float)h - 74.0f, Vec3(0.14f, 0.16f, 0.2f));
        Renderer::drawRect(58.0f, 42.0f, 2.0f, (float)h - 74.0f, Vec3(0.2f, 0.25f, 0.32f));

        // Tool Icon 1: Brush (Orange active)
        Vec3 brushCol = (_editor.currentTool == EditorTool::CreateBrush) ? Vec3(1.0f, 0.55f, 0.1f) : Vec3(0.3f, 0.35f, 0.4f);
        Renderer::drawRect(14.0f, 60.0f, 32.0f, 32.0f, brushCol);

        // Tool Icon 2: Door (Cyan active)
        Vec3 doorCol = (_editor.currentTool == EditorTool::CreateDoor) ? Vec3(0.2f, 0.8f, 1.0f) : Vec3(0.3f, 0.35f, 0.4f);
        Renderer::drawRect(14.0f, 105.0f, 32.0f, 32.0f, doorCol);

        // Help Legend in Top Right
        Renderer::drawRect((float)w - 380.0f, 48.0f, 360.0f, 120.0f, Vec3(0.08f, 0.1f, 0.14f));
        Renderer::drawRect((float)w - 380.0f, 48.0f, 360.0f, 2.0f, Vec3(1.0f, 0.55f, 0.1f));
        
        // Status indicator in help box
        Renderer::drawRect((float)w - 365.0f, 62.0f, 10.0f, 10.0f, Vec3(0.2f, 0.9f, 0.4f));

        Renderer::endUI();
    }

    void onShutdown() override {
        Renderer::shutdown();
    }

private:
    Camera _camera;
    std::unique_ptr<LabMap> _map;
    std::unordered_map<std::string, std::unique_ptr<Texture>> _textures;
    LabHammerEditor _editor;

    bool _ePressed = false;
    bool _kPressed = false;
    bool _delPressed = false;
};

int main() {
    LabHammerStandalone hammer;
    hammer.run();
    return 0;
}
