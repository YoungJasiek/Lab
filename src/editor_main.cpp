#include "Lab.h"
#include "LabFont.h"
#include "LabDialogs.h"
#include <glad/gl.h>
#include <GLFW/glfw3.h>
#include <windows.h>
#include <iostream>
#include <filesystem>
#include <unordered_map>
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
    Spawn
};

enum class SidebarTab {
    Properties,
    Hierarchy
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

        // Scan textures and models
        discoverTextures();
        discoverModels();

        // Load default or facility map
        std::string targetMap = "assets/maps/facility_alpha.labmap";
        if (std::filesystem::exists(targetMap)) {
            _map = LabMap::loadFromFile(targetMap);
            if (_map) _currentMapPath = targetMap;
        }
        if (!_map) {
            newMap();
        }

        // Camera initial pose
        _camera.setPosition(Vec3(0, 8.0f, 18.0f));

        // Unlock mouse cursor for UI desktop interaction
        glfwSetInputMode(getWindow(), GLFW_CURSOR, GLFW_CURSOR_NORMAL);

        logMessage("Hammer initialized. Ready.");
        logMessage("Textures loaded: " + std::to_string(_availableTextures.size()) + " | Models: " + std::to_string(_availableModels.size()));
        logMessage("Frustum Culling active: 'To czego oko nie widzi tego maszyna renderowac nie musi'");
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
        auto tex = std::make_unique<Texture>(path);
        if (tex && tex->getId() != 0) {
            Texture* ptr = tex.get();
            _textures[path] = std::move(tex);
            return ptr;
        }
        return nullptr;
    }

    void logMessage(const std::string& msg) {
        _consoleMessages.push_back(msg);
        if (_consoleMessages.size() > 8) {
            _consoleMessages.erase(_consoleMessages.begin());
        }
    }

    void onFixedUpdate(float fixedDelta) override {
        // Noclip camera flight (active when holding Right Mouse Button)
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
        floor.position = Vec3(0, -0.5f, 0);
        floor.size = Vec3(32.0f, 1.0f, 32.0f);
        floor.color = Vec3(1.0f, 1.0f, 1.0f);
        floor.texturePath = "floor_tiles.bmp";
        floor.uvScale = Vec2(0.25f, 0.25f);
        floor.uvMode = 1;
        _map->brushes.push_back(floor);

        _currentMapPath = "";
        _selectionType = SelectionType::None;
        _selectedIndex = -1;
        logMessage("Created New Map.");
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

    void runInEngine() {
        if (!_map) return;
        std::string runPath = _currentMapPath.empty() ? "assets/maps/hammer_run.labmap" : _currentMapPath;
        _map->saveToFile(runPath);
        logMessage("Saved map for engine: " + runPath);

        STARTUPINFOA si;
        PROCESS_INFORMATION pi;
        ZeroMemory(&si, sizeof(si));
        si.cb = sizeof(si);
        ZeroMemory(&pi, sizeof(pi));

        char cmdLine[256] = "Lab.exe";
        if (CreateProcessA("Lab.exe", cmdLine, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi) ||
            CreateProcessA("Release\\Lab.exe", cmdLine, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
            logMessage("Launched Lab.exe in Game Engine!");
        } else {
            system("start Lab.exe");
            logMessage("Launched Lab.exe via shell!");
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

        // Mouse look in 3D Viewport when holding Right Mouse Button
        if (Input::isMouseButtonPressed(1)) {
            _camera.update(Input::mouseDelta);
        }

        // Snap 3D cursor to grid
        _cursorPos = snapToGrid(_camera.getPosition() + _camera.getFront() * 10.0f, _gridSnap);

        // Handle Left-Click
        if (Input::isMouseButtonPressed(0)) {
            if (!_lmbPressed) {
                handleMouseClick(Input::mousePos.x, Input::mousePos.y, false);
                _lmbPressed = true;
            }
        } else {
            _lmbPressed = false;
        }

        // Handle Right-Click (for Tool 3 Pipette sample or camera)
        if (Input::isMouseButtonPressed(1)) {
            if (!_rmbPressed) {
                if (_activeTool == 3) {
                    handleMouseClick(Input::mousePos.x, Input::mousePos.y, true);
                }
                _rmbPressed = true;
            }
        } else {
            _rmbPressed = false;
        }

        // Keyboard Shortcuts
        bool ctrlDown = Input::isKeyPressed(341) || Input::isKeyPressed(345); // Left/Right Ctrl
        bool shiftDown = Input::isKeyPressed(340) || Input::isKeyPressed(344);

        // F9: Run in Engine
        if (Input::isKeyPressed(298)) { // GLFW_KEY_F9
            if (!_f9Pressed) {
                runInEngine();
                _f9Pressed = true;
            }
        } else {
            _f9Pressed = false;
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

        // F: Focus camera on selection
        if (!ctrlDown && (Input::isKeyPressed('F') || Input::isKeyPressed('f'))) {
            if (!_fPressed) {
                focusCamera();
                _fPressed = true;
            }
        } else {
            _fPressed = false;
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

            if (Input::isKeyPressed(266)) { // PageUp
                if (!_pageUpPressed) { moveSelection(0, _gridSnap, 0); _pageUpPressed = true; }
            } else _pageUpPressed = false;

            if (Input::isKeyPressed(267)) { // PageDown
                if (!_pageDownPressed) { moveSelection(0, -_gridSnap, 0); _pageDownPressed = true; }
            } else _pageDownPressed = false;
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
        float vpX = 42.0f;
        float vpY = 58.0f;
        float vpW = 1258.0f;
        float vpH = 820.0f;
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

        // Test Spawn
        {
            Vec3 half(0.5f, 0.9f, 0.5f);
            float t = 0;
            if (rayIntersectAABB(rayOrigin, rayDir, _map->spawn.position - half, _map->spawn.position + half, t)) {
                if (t < closestT) {
                    closestT = t;
                    hitType = SelectionType::Spawn;
                    hitIndex = 0;
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
                logMessage("Selected Player Spawn");
            } else {
                logMessage("Deselected all");
            }
        }
    }

    void moveSelection(float dx, float dy, float dz) {
        if (!_map) return;
        if (_selectionType == SelectionType::Brush && _selectedIndex >= 0 && _selectedIndex < (int)_map->brushes.size()) {
            _map->brushes[_selectedIndex].position += Vec3(dx, dy, dz);
        } else if (_selectionType == SelectionType::Prop && _selectedIndex >= 0 && _selectedIndex < (int)_map->props.size()) {
            _map->props[_selectedIndex].position += Vec3(dx, dy, dz);
        } else if (_selectionType == SelectionType::Door && _selectedIndex >= 0 && _selectedIndex < (int)_map->doors.size()) {
            _map->doors[_selectedIndex].position += Vec3(dx, dy, dz);
        } else if (_selectionType == SelectionType::Spawn) {
            _map->spawn.position += Vec3(dx, dy, dz);
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
        } else if (_selectionType == SelectionType::Spawn) {
            targetPos = _map->spawn.position;
        }
        _camera.setPosition(targetPos - _camera.getFront() * 10.0f);
        logMessage("Focused Camera on selection at (" + std::to_string((int)targetPos.x) + ", " + std::to_string((int)targetPos.y) + ", " + std::to_string((int)targetPos.z) + ")");
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
            _map->spawn.position = _cursorPos;
            _selectionType = SelectionType::Spawn;
            _selectedIndex = 0;
            logMessage("Set Player Spawn to (" + std::to_string((int)_cursorPos.x) + ", " + std::to_string((int)_cursorPos.y) + ", " + std::to_string((int)_cursorPos.z) + ")");
        }
    }

    void handleMouseClick(float mx, float my, bool isRmb = false) {
        // 1. Texture Browser Modal
        if (_browserOpen) {
            // Close button click
            if (mx >= 1180.0f && mx <= 1215.0f && my >= 95.0f && my <= 125.0f) {
                _browserOpen = false;
                return;
            }
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
            // Close button click
            if (mx >= 1120.0f && mx <= 1155.0f && my >= 195.0f && my <= 225.0f) {
                _modelBrowserOpen = false;
                return;
            }
            // Browse Disk STL button
            if (mx >= 470.0f && mx <= 770.0f && my >= 240.0f && my <= 275.0f) {
                openModelDialog();
                return;
            }
            // Model list items
            float startY = 290.0f;
            for (size_t i = 0; i < _availableModels.size(); ++i) {
                float iy = startY + i * 36.0f;
                if (mx >= 470.0f && mx <= 1130.0f && my >= iy && my <= iy + 30.0f) {
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
            if (mx >= 1090.0f && mx <= 1125.0f && my >= 220.0f && my <= 250.0f) {
                _helpModalOpen = false;
                return;
            }
            return;
        }

        // 4. Dropdown File Menu
        if (_fileMenuOpen) {
            float menuX = 10.0f;
            float menuY = 24.0f;
            float menuW = 200.0f;
            float menuH = 130.0f;
            if (mx >= menuX && mx <= menuX + menuW && my >= menuY && my <= menuY + menuH) {
                int itemIdx = (int)((my - menuY) / 25.0f);
                if (itemIdx == 0) newMap();
                else if (itemIdx == 1) openMapDialog();
                else if (itemIdx == 2) saveMapAction(false);
                else if (itemIdx == 3) saveMapAction(true);
                else if (itemIdx >= 4) glfwSetWindowShouldClose(getWindow(), GLFW_TRUE);
                _fileMenuOpen = false;
                return;
            } else {
                _fileMenuOpen = false;
            }
        }

        // 5. Top Menu Bar (y: 0..24)
        if (my >= 0.0f && my <= 24.0f) {
            if (mx >= 10.0f && mx <= 50.0f) {
                _fileMenuOpen = !_fileMenuOpen;
                return;
            } else if (mx >= 52.0f && mx <= 90.0f) { // Edit
                if (_selectionType != SelectionType::None) deleteSelection();
                else if (_map && !_map->brushes.empty()) { _map->brushes.pop_back(); logMessage("Undo: deleted last brush"); }
                return;
            } else if (mx >= 92.0f && mx <= 135.0f) { // View
                _wireframeMode = !_wireframeMode;
                glPolygonMode(GL_FRONT_AND_BACK, _wireframeMode ? GL_LINE : GL_FILL);
                logMessage("Wireframe Mode: " + std::string(_wireframeMode ? "ON" : "OFF"));
                return;
            } else if (mx >= 137.0f && mx <= 185.0f) { // Tools
                _browserOpen = !_browserOpen;
                return;
            } else if (mx >= 187.0f && mx <= 230.0f) { // Help
                _helpModalOpen = !_helpModalOpen;
                return;
            }
        }

        // 6. Top Toolbar Buttons (y: 24..58, 18 concrete functional buttons!)
        if (my >= 24.0f && my <= 58.0f) {
            for (int i = 0; i < 18; ++i) {
                float bx = 8.0f + i * 28.0f;
                if (mx >= bx && mx <= bx + 26.0f) {
                    switch (i) {
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
                        case 10: _activeTool = 0; logMessage("Tool: Pointer / Selection Tool"); break;
                        case 11: _activeTool = 1; logMessage("Tool: Brush / Block Tool"); break;
                        case 12: _activeTool = 2; logMessage("Tool: Entity / Prop Tool"); break;
                        case 13: _activeTool = 3; logMessage("Tool: Texture Pipette & Apply"); break;
                        case 14: _activeTool = 4; logMessage("Tool: Dynamic Door Tool"); break;
                        case 15: // Grid snap down
                            _gridSnap = std::max(0.125f, _gridSnap * 0.5f);
                            logMessage("Grid Snap: " + std::to_string(_gridSnap));
                            break;
                        case 16: // Grid snap up
                            _gridSnap = std::min(16.0f, _gridSnap * 2.0f);
                            logMessage("Grid Snap: " + std::to_string(_gridSnap));
                            break;
                        case 17: // Run in Engine (F9)
                            runInEngine();
                            break;
                    }
                    return;
                }
            }
        }

        // 7. Left Tools Palette (x: 0..42, y: 58..878)
        if (mx >= 0.0f && mx <= 42.0f) {
            float startY = 65.0f;
            for (int i = 0; i < 8; ++i) {
                float ty = startY + i * 36.0f;
                if (my >= ty && my <= ty + 32.0f) {
                    _activeTool = i;
                    switch (i) {
                        case 0: logMessage("Tool 0: Selection / Pointer Tool"); break;
                        case 1: logMessage("Tool 1: Brush / Block Tool (E to place)"); break;
                        case 2: logMessage("Tool 2: Entity / Prop Tool (E to place model)"); break;
                        case 3: logMessage("Tool 3: Texture Tool (LMB apply, RMB pipette)"); break;
                        case 4: logMessage("Tool 4: Door Tool (E to place dynamic door)"); break;
                        case 5: logMessage("Tool 5: Spawn Tool (E to place player spawn)"); break;
                        case 6: // Resize tool
                            resizeSelection(_gridSnap, _gridSnap, _gridSnap);
                            break;
                        case 7: // Lighting tool
                            _sunAngle += 30.0f;
                            if (_sunAngle >= 360.0f) _sunAngle = 0.0f;
                            {
                                float rad = _sunAngle * 3.14159f / 180.0f;
                                Renderer::setSunLight(Vec3(std::cos(rad), -0.8f, std::sin(rad)), Vec3(1.0f, 0.95f, 0.9f), Vec3(0.25f, 0.28f, 0.35f));
                            }
                            logMessage("Tool 7: Adjusted Sun Light Angle (" + std::to_string((int)_sunAngle) + " deg)");
                            break;
                    }
                    return;
                }
            }
        }

        // 8. Right Sidebar (x: 1300..1600, y: 58..878)
        if (mx >= 1300.0f) {
            float rightX = 1300.0f;
            float rightY = 58.0f;

            // Tab headers (Properties vs Outliner)
            if (my >= rightY + 4.0f && my <= rightY + 30.0f) {
                if (mx >= rightX + 10.0f && mx <= rightX + 145.0f) {
                    _sidebarTab = SidebarTab::Properties;
                    return;
                } else if (mx >= rightX + 150.0f && mx <= rightX + 285.0f) {
                    _sidebarTab = SidebarTab::Hierarchy;
                    return;
                }
            }

            // OUTLINER TAB INTERACTIONS
            if (_sidebarTab == SidebarTab::Hierarchy) {
                float listY = rightY + 65.0f;
                int totalEntities = (int)(1 + _map->brushes.size() + _map->props.size() + _map->doors.size());
                int maxItemsPerPage = 18;

                // Click on entity items
                for (int i = 0; i < maxItemsPerPage; ++i) {
                    int itemIdx = _outlinerScroll + i;
                    if (itemIdx >= totalEntities) break;

                    float iy = listY + i * 26.0f;
                    if (my >= iy && my <= iy + 24.0f && mx >= rightX + 10.0f && mx <= rightX + 285.0f) {
                        if (itemIdx == 0) {
                            _selectionType = SelectionType::Spawn;
                            _selectedIndex = 0;
                            logMessage("Outliner: Selected Player Spawn");
                        } else {
                            int bOffset = 1;
                            int pOffset = bOffset + (int)_map->brushes.size();
                            int dOffset = pOffset + (int)_map->props.size();

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
                            } else if (itemIdx >= dOffset) {
                                _selectionType = SelectionType::Door;
                                _selectedIndex = itemIdx - dOffset;
                                logMessage("Outliner: Selected Door #" + std::to_string(_selectedIndex));
                            }
                        }
                        return;
                    }
                }

                // Outliner bottom action buttons
                float actY = rightY + 540.0f;
                // Focus (F)
                if (my >= actY && my <= actY + 28.0f && mx >= rightX + 12.0f && mx <= rightX + 98.0f) {
                    focusCamera();
                    return;
                }
                // Duplicate (Ctrl+D)
                if (my >= actY && my <= actY + 28.0f && mx >= rightX + 104.0f && mx <= rightX + 190.0f) {
                    duplicateSelection();
                    return;
                }
                // Delete (Del)
                if (my >= actY && my <= actY + 28.0f && mx >= rightX + 196.0f && mx <= rightX + 282.0f) {
                    deleteSelection();
                    return;
                }
                // Prev / Next Page buttons
                if (my >= actY + 34.0f && my <= actY + 60.0f) {
                    if (mx >= rightX + 12.0f && mx <= rightX + 140.0f) {
                        _outlinerScroll = std::max(0, _outlinerScroll - maxItemsPerPage);
                        return;
                    } else if (mx >= rightX + 154.0f && mx <= rightX + 282.0f) {
                        if (_outlinerScroll + maxItemsPerPage < totalEntities) _outlinerScroll += maxItemsPerPage;
                        return;
                    }
                }
                return;
            }

            // PROPERTIES TAB INTERACTIONS
            if (_sidebarTab == SidebarTab::Properties) {
                // UV Scale buttons [0.125] [0.25] [0.5] [1.0]
                float uvY = rightY + 160.0f;
                if (my >= uvY && my <= uvY + 24.0f) {
                    if (mx >= rightX + 12.0f && mx <= rightX + 72.0f) {
                        _activeUvScale = Vec2(0.125f, 0.125f);
                        if (_selectionType == SelectionType::Brush && _selectedIndex >= 0 && _selectedIndex < (int)_map->brushes.size()) {
                            _map->brushes[_selectedIndex].uvScale = _activeUvScale;
                        }
                        logMessage("Set UV Scale: 0.125 (Fine Tiling)");
                        return;
                    } else if (mx >= rightX + 76.0f && mx <= rightX + 136.0f) {
                        _activeUvScale = Vec2(0.25f, 0.25f);
                        if (_selectionType == SelectionType::Brush && _selectedIndex >= 0 && _selectedIndex < (int)_map->brushes.size()) {
                            _map->brushes[_selectedIndex].uvScale = _activeUvScale;
                        }
                        logMessage("Set UV Scale: 0.25 (Source Engine Standard)");
                        return;
                    } else if (mx >= rightX + 140.0f && mx <= rightX + 200.0f) {
                        _activeUvScale = Vec2(0.5f, 0.5f);
                        if (_selectionType == SelectionType::Brush && _selectedIndex >= 0 && _selectedIndex < (int)_map->brushes.size()) {
                            _map->brushes[_selectedIndex].uvScale = _activeUvScale;
                        }
                        logMessage("Set UV Scale: 0.5");
                        return;
                    } else if (mx >= rightX + 204.0f && mx <= rightX + 264.0f) {
                        _activeUvScale = Vec2(1.0f, 1.0f);
                        if (_selectionType == SelectionType::Brush && _selectedIndex >= 0 && _selectedIndex < (int)_map->brushes.size()) {
                            _map->brushes[_selectedIndex].uvScale = _activeUvScale;
                        }
                        logMessage("Set UV Scale: 1.0 (Single Repeat)");
                        return;
                    }
                }

                // Texture Browse button
                if (my >= rightY + 280.0f && my <= rightY + 312.0f && mx >= rightX + 105.0f && mx <= rightX + 275.0f) {
                    _browserOpen = true;
                    return;
                }
                // Apply Texture button
                if (my >= rightY + 318.0f && my <= rightY + 350.0f && mx >= rightX + 105.0f && mx <= rightX + 275.0f) {
                    if (_selectionType == SelectionType::Brush && _selectedIndex >= 0 && _selectedIndex < (int)_map->brushes.size()) {
                        _map->brushes[_selectedIndex].texturePath = _selectedTexture;
                        _map->brushes[_selectedIndex].uvScale = _activeUvScale;
                        logMessage("Applied texture '" + _selectedTexture + "' to Brush #" + std::to_string(_selectedIndex));
                    } else {
                        placeCurrentObject();
                    }
                    return;
                }

                // Model Browse button
                if (my >= rightY + 410.0f && my <= rightY + 442.0f && mx >= rightX + 12.0f && mx <= rightX + 275.0f) {
                    _modelBrowserOpen = true;
                    return;
                }

                // Resize [-] [+] buttons for selected object or brush size
                float dimY = rightY + 480.0f;
                if (my >= dimY && my <= dimY + 26.0f) {
                    // Size X [-] [+]
                    if (mx >= rightX + 60.0f && mx <= rightX + 85.0f) { resizeSelection(-_gridSnap, 0, 0); _brushSize.x = std::max(0.5f, _brushSize.x - _gridSnap); return; }
                    if (mx >= rightX + 90.0f && mx <= rightX + 115.0f) { resizeSelection(_gridSnap, 0, 0); _brushSize.x += _gridSnap; return; }
                    // Size Y [-] [+]
                    if (mx >= rightX + 140.0f && mx <= rightX + 165.0f) { resizeSelection(0, -_gridSnap, 0); _brushSize.y = std::max(0.5f, _brushSize.y - _gridSnap); return; }
                    if (mx >= rightX + 170.0f && mx <= rightX + 195.0f) { resizeSelection(0, _gridSnap, 0); _brushSize.y += _gridSnap; return; }
                    // Size Z [-] [+]
                    if (mx >= rightX + 220.0f && mx <= rightX + 245.0f) { resizeSelection(0, 0, -_gridSnap); _brushSize.z = std::max(0.5f, _brushSize.z - _gridSnap); return; }
                    if (mx >= rightX + 250.0f && mx <= rightX + 275.0f) { resizeSelection(0, 0, _gridSnap); _brushSize.z += _gridSnap; return; }
                }

                // Deselect button
                if (my >= rightY + 540.0f && my <= rightY + 568.0f && mx >= rightX + 12.0f && mx <= rightX + 275.0f) {
                    _selectionType = SelectionType::None;
                    _selectedIndex = -1;
                    logMessage("Deselected all.");
                    return;
                }
            }
            return;
        }

        // 9. 3D Viewport Raycast Picking (Selection / Texture Tool / Object placement)
        pickObjectInViewport(mx, my, isRmb);
    }

    void drawGizmo(const Vec3& pos) {
        float len = 1.6f;
        float thick = 0.06f;
        // X Axis: Red
        Renderer::drawCube(pos + Vec3(len * 0.5f, 0, 0), Vec3(len, thick, thick), Vec3(1.0f, 0.15f, 0.15f), false);
        // Y Axis: Green
        Renderer::drawCube(pos + Vec3(0, len * 0.5f, 0), Vec3(thick, len, thick), Vec3(0.15f, 1.0f, 0.2f), false);
        // Z Axis: Blue
        Renderer::drawCube(pos + Vec3(0, 0, len * 0.5f), Vec3(thick, thick, len), Vec3(0.2f, 0.55f, 1.0f), false);
    }

    void onRender() override {
        // 1. Begin 3D Frame & Frustum Culling
        Renderer::beginFrame(_camera);

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
                Renderer::drawCube(b.position, b.size, b.color, tex, true, b.uvScale, b.uvMode);
            }

            // Render Props (STL models) with Frustum Culling
            for (const auto& p : _map->props) {
                Vec3 halfScale = p.scale * 0.5f;
                Vec3 pMin = p.position - halfScale;
                Vec3 pMax = p.position + halfScale;
                if (!_camera.isInFrustum(pMin, pMax)) continue; // Frustum culling

                renderedProps++;
                if (!_meshes.contains(p.modelPath)) {
                    Mesh* m = Mesh::loadSTL(p.modelPath);
                    if (m) _meshes[p.modelPath] = std::unique_ptr<Mesh>(m);
                }
                if (_meshes.contains(p.modelPath)) {
                    Texture* tex = p.texturePath.empty() ? nullptr : getTexture(p.texturePath);
                    Renderer::drawMesh(*_meshes[p.modelPath], p.position, p.rotation, p.scale, p.color, tex);
                }
            }

            // Render Doors with Frustum Culling
            for (const auto& d : _map->doors) {
                Vec3 halfSize = d.size * 0.5f;
                if (!_camera.isInFrustum(d.position - halfSize, d.position + halfSize)) continue;
                Renderer::drawCube(d.position, d.size, d.color);
            }

            // Render Player Spawn Marker
            Renderer::drawWireCube(_map->spawn.position, Vec3(1.0f, 1.8f, 1.0f), Vec3(0.2f, 0.85f, 1.0f));
            Renderer::drawCube(_map->spawn.position, Vec3(0.8f, 0.1f, 0.8f), Vec3(0.1f, 0.6f, 0.9f), false);
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
            } else if (_selectionType == SelectionType::Spawn) {
                Vec3 half(0.5f, 0.9f, 0.5f);
                Renderer::drawBoundingBox(_map->spawn.position - half, _map->spawn.position + half, Vec3(0.2f, 0.85f, 1.0f));
                drawGizmo(_map->spawn.position);
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
            Renderer::drawWireCube(_cursorPos, Vec3(1.0f, 1.8f, 1.0f), Vec3(0.2f, 0.9f, 0.5f));
            drawGizmo(_cursorPos);
        }

        // 2. Render 2D Valve Hammer Desktop Interface
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
            case 2: // Entity Prop / Lamp / 3D Model
                Renderer::drawRect(x + 8.0f, y + 4.0f, 8.0f, 8.0f, color);
                Renderer::drawRect(x + 10.0f, y + 12.0f, 4.0f, 5.0f, color);
                Renderer::drawRect(x + 6.0f, y + 8.0f, 12.0f, 2.0f, color);
                break;
            case 3: // Texture / Material Application Pipette
                Renderer::drawRect(x + 4.0f, y + 4.0f, 16.0f, 16.0f, color);
                Renderer::drawRect(x + 4.0f, y + 4.0f, 8.0f, 8.0f, Vec3(0.1f, 0.1f, 0.1f));
                Renderer::drawRect(x + 12.0f, y + 12.0f, 8.0f, 8.0f, Vec3(0.1f, 0.1f, 0.1f));
                break;
            case 4: // Dynamic Door
                Renderer::drawRect(x + 6.0f, y + 4.0f, 12.0f, 16.0f, color);
                Renderer::drawRect(x + 8.0f, y + 6.0f, 8.0f, 12.0f, bg);
                Renderer::drawRect(x + 13.0f, y + 11.0f, 2.0f, 2.0f, color);
                break;
            case 5: // Player Spawn
                Renderer::drawRect(x + 9.0f, y + 4.0f, 6.0f, 6.0f, color);
                Renderer::drawRect(x + 7.0f, y + 11.0f, 10.0f, 8.0f, color);
                Renderer::drawRect(x + 9.0f, y + 19.0f, 2.0f, 4.0f, color);
                Renderer::drawRect(x + 13.0f, y + 19.0f, 2.0f, 4.0f, color);
                break;
            case 6: // Resize / Clip Tool
                Renderer::drawRect(x + 5.0f, y + 5.0f, 14.0f, 2.0f, color);
                Renderer::drawRect(x + 7.0f, y + 7.0f, 10.0f, 2.0f, color);
                Renderer::drawRect(x + 9.0f, y + 9.0f, 6.0f, 2.0f, color);
                Renderer::drawRect(x + 11.0f, y + 11.0f, 2.0f, 8.0f, color);
                break;
            case 7: // Sun / Lighting Tool
                Renderer::drawRect(x + 9.0f, y + 9.0f, 6.0f, 6.0f, Vec3(1.0f, 0.85f, 0.2f));
                Renderer::drawRect(x + 11.0f, y + 4.0f, 2.0f, 4.0f, color);
                Renderer::drawRect(x + 11.0f, y + 16.0f, 2.0f, 4.0f, color);
                Renderer::drawRect(x + 4.0f, y + 11.0f, 4.0f, 2.0f, color);
                Renderer::drawRect(x + 16.0f, y + 11.0f, 4.0f, 2.0f, color);
                break;
            default:
                Renderer::drawRect(x + 6.0f, y + 6.0f, 12.0f, 12.0f, color);
                break;
        }
    }

    // Classic Hammer Top Toolbar Icons
    static void drawToolbarIcon(int iconId, float x, float y, const Vec3& color, const Vec3& bg) {
        Renderer::drawRect(x, y, 24.0f, 24.0f, bg);
        Renderer::drawRect(x, y, 24.0f, 1.0f, Vec3(0.7f, 0.7f, 0.7f));
        Renderer::drawRect(x, y + 23.0f, 24.0f, 1.0f, Vec3(0.5f, 0.5f, 0.5f));

        switch(iconId) {
            case 0: // New Document
                Renderer::drawRect(x + 6.0f, y + 4.0f, 11.0f, 15.0f, Vec3(1, 1, 1));
                Renderer::drawRect(x + 6.0f, y + 4.0f, 11.0f, 1.0f, color);
                Renderer::drawRect(x + 6.0f, y + 4.0f, 1.0f, 15.0f, color);
                Renderer::drawRect(x + 16.0f, y + 7.0f, 1.0f, 12.0f, color);
                Renderer::drawRect(x + 6.0f, y + 19.0f, 11.0f, 1.0f, color);
                break;
            case 1: // Open Folder
                Renderer::drawRect(x + 5.0f, y + 6.0f, 6.0f, 2.0f, Vec3(0.9f, 0.75f, 0.2f));
                Renderer::drawRect(x + 5.0f, y + 8.0f, 14.0f, 10.0f, Vec3(0.95f, 0.8f, 0.25f));
                break;
            case 2: // Save Floppy Disk
                Renderer::drawRect(x + 5.0f, y + 5.0f, 14.0f, 14.0f, Vec3(0.2f, 0.45f, 0.85f));
                Renderer::drawRect(x + 8.0f, y + 5.0f, 8.0f, 4.0f, Vec3(0.85f, 0.85f, 0.9f));
                Renderer::drawRect(x + 7.0f, y + 11.0f, 10.0f, 7.0f, Vec3(1, 1, 1));
                break;
            case 3: // Save As (Disk with pencil)
                Renderer::drawRect(x + 5.0f, y + 5.0f, 14.0f, 14.0f, Vec3(0.2f, 0.6f, 0.8f));
                Renderer::drawRect(x + 12.0f, y + 12.0f, 6.0f, 6.0f, Vec3(1.0f, 0.8f, 0.1f));
                break;
            case 4: // Undo Arrow
                Renderer::drawRect(x + 6.0f, y + 11.0f, 9.0f, 2.0f, color);
                Renderer::drawRect(x + 13.0f, y + 7.0f, 2.0f, 6.0f, color);
                Renderer::drawRect(x + 6.0f, y + 9.0f, 2.0f, 6.0f, color);
                break;
            case 5: // Delete Cross
                Renderer::drawRect(x + 6.0f, y + 6.0f, 12.0f, 12.0f, Vec3(0.85f, 0.2f, 0.2f));
                Renderer::drawRect(x + 9.0f, y + 9.0f, 6.0f, 6.0f, Vec3(1, 1, 1));
                break;
            case 6: // Duplicate Plus
                Renderer::drawRect(x + 6.0f, y + 6.0f, 12.0f, 12.0f, Vec3(0.2f, 0.7f, 0.4f));
                Renderer::drawRect(x + 11.0f, y + 8.0f, 2.0f, 8.0f, Vec3(1, 1, 1));
                Renderer::drawRect(x + 8.0f, y + 11.0f, 8.0f, 2.0f, Vec3(1, 1, 1));
                break;
            case 7: // Focus Camera (Target Eye)
                Renderer::drawRect(x + 5.0f, y + 5.0f, 14.0f, 14.0f, color);
                Renderer::drawRect(x + 7.0f, y + 7.0f, 10.0f, 10.0f, bg);
                Renderer::drawRect(x + 10.0f, y + 10.0f, 4.0f, 4.0f, color);
                break;
            case 8: // Texture Browser Grid
                Renderer::drawRect(x + 5.0f, y + 5.0f, 14.0f, 14.0f, Vec3(0.3f, 0.35f, 0.4f));
                Renderer::drawRect(x + 6.0f, y + 6.0f, 5.0f, 5.0f, Vec3(0.9f, 0.5f, 0.1f));
                Renderer::drawRect(x + 13.0f, y + 6.0f, 5.0f, 5.0f, Vec3(0.2f, 0.7f, 0.9f));
                break;
            case 9: // Model Browser (3D Mesh Icon)
                Renderer::drawRect(x + 5.0f, y + 5.0f, 14.0f, 14.0f, Vec3(0.15f, 0.45f, 0.75f));
                Renderer::drawRect(x + 7.0f, y + 7.0f, 10.0f, 10.0f, Vec3(0.85f, 0.95f, 1.0f));
                Renderer::drawRect(x + 9.0f, y + 9.0f, 6.0f, 6.0f, Vec3(0.15f, 0.45f, 0.75f));
                break;
            case 10: // Tool 0 (Selection)
                drawHammerIcon(0, x, y, color, bg);
                break;
            case 11: // Tool 1 (Brush)
                drawHammerIcon(1, x, y, color, bg);
                break;
            case 12: // Tool 2 (Prop)
                drawHammerIcon(2, x, y, color, bg);
                break;
            case 13: // Tool 3 (Pipette)
                drawHammerIcon(3, x, y, color, bg);
                break;
            case 14: // Tool 4 (Door)
                drawHammerIcon(4, x, y, color, bg);
                break;
            case 15: // Grid -
                Renderer::drawRect(x + 7.0f, y + 11.0f, 10.0f, 2.0f, color);
                break;
            case 16: // Grid +
                Renderer::drawRect(x + 7.0f, y + 11.0f, 10.0f, 2.0f, color);
                Renderer::drawRect(x + 11.0f, y + 7.0f, 2.0f, 10.0f, color);
                break;
            case 17: // RUN IN ENGINE (Green Play Button)
                Renderer::drawRect(x + 4.0f, y + 4.0f, 16.0f, 16.0f, Vec3(0.15f, 0.65f, 0.35f));
                Renderer::drawRect(x + 8.0f, y + 6.0f, 8.0f, 12.0f, Vec3(1, 1, 1));
                break;
            default:
                drawHammerIcon(iconId % 8, x, y, color, bg);
                break;
        }
    }

    void drawHammerInterface() {
        int w = 1600, h = 900;
        Renderer::beginUI(w, h);

        Vec3 winBg{ 0.93f, 0.93f, 0.94f };          // Win32 Editor Gray
        Vec3 winBorder{ 0.65f, 0.65f, 0.68f };      // Bevel Gray
        Vec3 textDark{ 0.12f, 0.12f, 0.12f };       // Dark Gray Text
        Vec3 textDim{ 0.45f, 0.45f, 0.45f };        // Dim Label
        Vec3 cyanGlow{ 0.2f, 0.75f, 0.95f };        // Cyan accent
        Vec3 orangeGlow{ 1.0f, 0.55f, 0.1f };       // Hammer Orange

        // ==================== 1. TOP TITLEBAR & MENUS ====================
        Renderer::drawRect(0, 0, (float)w, 24.0f, winBg);
        Renderer::drawRect(0, 23.0f, (float)w, 1.0f, winBorder);

        LabFont::drawText(14.0f, 5.0f, "File", 1.8f, textDark, LabFontType::System);
        LabFont::drawText(54.0f, 5.0f, "Edit", 1.8f, textDark, LabFontType::System);
        LabFont::drawText(94.0f, 5.0f, "View", 1.8f, textDark, LabFontType::System);
        LabFont::drawText(140.0f, 5.0f, "Tools", 1.8f, textDark, LabFontType::System);
        LabFont::drawText(190.0f, 5.0f, "Help", 1.8f, textDark, LabFontType::System);

        LabFont::drawText((float)w - 360.0f, 5.0f, "Valve Hammer 4.1 - Frozen-Life Engine", 1.8f, Vec3(0.15f, 0.45f, 0.75f), LabFontType::GeoSans);

        // ==================== 2. MAIN TOOLBAR (18 Buttons) ====================
        float tbY = 24.0f;
        float tbH = 34.0f;
        Renderer::drawRect(0, tbY, (float)w, tbH, winBg);
        Renderer::drawRect(0, tbY + tbH - 1.0f, (float)w, 1.0f, winBorder);

        for (int i = 0; i < 18; ++i) {
            float bx = 8.0f + i * 28.0f;
            drawToolbarIcon(i, bx, tbY + 5.0f, (i == 17) ? Vec3(1, 1, 1) : Vec3(0.25f, 0.3f, 0.35f), (i == 17) ? Vec3(0.15f, 0.65f, 0.35f) : Vec3(0.88f, 0.88f, 0.90f));
        }

        // ==================== 3. LEFT TOOLS PALETTE (Tools 0..7) ====================
        float leftW = 42.0f;
        float leftY = tbY + tbH;
        float leftH = (float)h - leftY - 24.0f;
        Renderer::drawRect(0, leftY, leftW, leftH, winBg);
        Renderer::drawRect(leftW - 1.0f, leftY, 1.0f, leftH, winBorder);

        for (int i = 0; i < 8; ++i) {
            float ty = leftY + 10.0f + i * 36.0f;
            bool isSel = (_activeTool == i);
            Vec3 bgCol = isSel ? Vec3(0.78f, 0.88f, 1.0f) : Vec3(0.88f, 0.88f, 0.90f);
            Vec3 iconCol = isSel ? orangeGlow : Vec3(0.25f, 0.28f, 0.32f);

            Renderer::drawRect(6.0f, ty, 30.0f, 30.0f, bgCol);
            Renderer::drawRect(6.0f, ty, 30.0f, 1.0f, isSel ? cyanGlow : winBorder);
            drawHammerIcon(i, 9.0f, ty + 3.0f, iconCol, bgCol);
        }

        // ==================== 4. RIGHT SIDEBAR (300px) ====================
        float rightW = 300.0f;
        float rightX = (float)w - rightW;
        float rightY = leftY;
        float rightH = leftH;
        Renderer::drawRect(rightX, rightY, rightW, rightH, winBg);
        Renderer::drawRect(rightX, rightY, 1.0f, rightH, winBorder);

        // Sidebar Tabs: [ Properties ] and [ Outliner / Struktura ]
        bool isPropTab = (_sidebarTab == SidebarTab::Properties);
        bool isOutTab = (_sidebarTab == SidebarTab::Hierarchy);

        Renderer::drawRect(rightX + 10.0f, rightY + 6.0f, 135.0f, 24.0f, isPropTab ? Vec3(1, 1, 1) : Vec3(0.85f, 0.85f, 0.88f));
        Renderer::drawRect(rightX + 10.0f, rightY + 6.0f, 135.0f, 1.0f, isPropTab ? orangeGlow : winBorder);
        LabFont::drawText(rightX + 35.0f, rightY + 11.0f, "Properties", 1.6f, isPropTab ? textDark : textDim, LabFontType::System);

        Renderer::drawRect(rightX + 150.0f, rightY + 6.0f, 135.0f, 24.0f, isOutTab ? Vec3(1, 1, 1) : Vec3(0.85f, 0.85f, 0.88f));
        Renderer::drawRect(rightX + 150.0f, rightY + 6.0f, 135.0f, 1.0f, isOutTab ? orangeGlow : winBorder);
        LabFont::drawText(rightX + 175.0f, rightY + 11.0f, "Struktura", 1.6f, isOutTab ? textDark : textDim, LabFontType::System);

        // ==================== TAB CONTENT: OUTLINER (STRUKTURA MAPY) ====================
        if (_sidebarTab == SidebarTab::Hierarchy) {
            float outY = rightY + 40.0f;
            LabFont::drawText(rightX + 12.0f, outY, "Map Entity Outliner:", 1.7f, textDark, LabFontType::System);

            Renderer::drawRect(rightX + 10.0f, outY + 18.0f, 280.0f, 470.0f, Vec3(1, 1, 1));
            Renderer::drawRect(rightX + 10.0f, outY + 18.0f, 280.0f, 1.0f, winBorder);

            int totalEntities = (int)(1 + _map->brushes.size() + _map->props.size() + _map->doors.size());
            int maxItemsPerPage = 18;

            for (int i = 0; i < maxItemsPerPage; ++i) {
                int itemIdx = _outlinerScroll + i;
                if (itemIdx >= totalEntities) break;

                float iy = outY + 24.0f + i * 25.0f;
                bool isSelected = false;
                std::string itemText = "";

                if (itemIdx == 0) {
                    isSelected = (_selectionType == SelectionType::Spawn);
                    itemText = "[Spawn] Player Start (" + std::to_string((int)_map->spawn.position.x) + "," + std::to_string((int)_map->spawn.position.z) + ")";
                } else {
                    int bOffset = 1;
                    int pOffset = bOffset + (int)_map->brushes.size();
                    int dOffset = pOffset + (int)_map->props.size();

                    if (itemIdx >= bOffset && itemIdx < pOffset) {
                        int bIdx = itemIdx - bOffset;
                        isSelected = (_selectionType == SelectionType::Brush && _selectedIndex == bIdx);
                        std::string tName = _map->brushes[bIdx].texturePath;
                        if (tName.size() > 14) tName = tName.substr(0, 12) + "..";
                        itemText = "[B#" + std::to_string(bIdx) + "] " + tName;
                    } else if (itemIdx >= pOffset && itemIdx < dOffset) {
                        int pIdx = itemIdx - pOffset;
                        isSelected = (_selectionType == SelectionType::Prop && _selectedIndex == pIdx);
                        std::string mName = _map->props[pIdx].modelPath;
                        if (mName.size() > 14) mName = mName.substr(0, 12) + "..";
                        itemText = "[P#" + std::to_string(pIdx) + "] " + mName;
                    } else if (itemIdx >= dOffset) {
                        int dIdx = itemIdx - dOffset;
                        isSelected = (_selectionType == SelectionType::Door && _selectedIndex == dIdx);
                        itemText = "[D#" + std::to_string(dIdx) + "] " + _map->doors[dIdx].name;
                    }
                }

                if (isSelected) {
                    Renderer::drawRect(rightX + 12.0f, iy, 276.0f, 22.0f, Vec3(0.85f, 0.92f, 1.0f));
                    Renderer::drawRect(rightX + 12.0f, iy, 4.0f, 22.0f, orangeGlow);
                }

                LabFont::drawText(rightX + 20.0f, iy + 4.0f, itemText, 1.5f, isSelected ? Vec3(0.1f, 0.35f, 0.7f) : textDark, LabFontType::System);
            }

            // Outliner bottom action buttons
            float actY = rightY + 540.0f;
            Renderer::drawRect(rightX + 12.0f, actY, 86.0f, 26.0f, Vec3(0.88f, 0.88f, 0.90f));
            Renderer::drawRect(rightX + 12.0f, actY, 86.0f, 1.0f, winBorder);
            LabFont::drawText(rightX + 22.0f, actY + 6.0f, "Focus (F)", 1.5f, textDark, LabFontType::System);

            Renderer::drawRect(rightX + 104.0f, actY, 86.0f, 26.0f, Vec3(0.88f, 0.88f, 0.90f));
            Renderer::drawRect(rightX + 104.0f, actY, 86.0f, 1.0f, winBorder);
            LabFont::drawText(rightX + 114.0f, actY + 6.0f, "Duplicate", 1.5f, textDark, LabFontType::System);

            Renderer::drawRect(rightX + 196.0f, actY, 86.0f, 26.0f, Vec3(0.88f, 0.88f, 0.90f));
            Renderer::drawRect(rightX + 196.0f, actY, 86.0f, 1.0f, winBorder);
            LabFont::drawText(rightX + 208.0f, actY + 6.0f, "Delete", 1.5f, Vec3(0.7f, 0.1f, 0.1f), LabFontType::System);

            // Pagination buttons
            Renderer::drawRect(rightX + 12.0f, actY + 34.0f, 128.0f, 24.0f, Vec3(0.88f, 0.88f, 0.90f));
            Renderer::drawRect(rightX + 12.0f, actY + 34.0f, 128.0f, 1.0f, winBorder);
            LabFont::drawText(rightX + 45.0f, actY + 39.0f, "< Prev Page", 1.5f, textDark, LabFontType::System);

            Renderer::drawRect(rightX + 154.0f, actY + 34.0f, 128.0f, 24.0f, Vec3(0.88f, 0.88f, 0.90f));
            Renderer::drawRect(rightX + 154.0f, actY + 34.0f, 128.0f, 1.0f, winBorder);
            LabFont::drawText(rightX + 185.0f, actY + 39.0f, "Next Page >", 1.5f, textDark, LabFontType::System);
        }

        // ==================== TAB CONTENT: PROPERTIES ====================
        if (_sidebarTab == SidebarTab::Properties) {
            float propY = rightY + 40.0f;

            // Header info on selection
            std::string selHeader = "Selection: None";
            if (_selectionType == SelectionType::Brush) selHeader = "Selection: Brush #" + std::to_string(_selectedIndex);
            else if (_selectionType == SelectionType::Prop) selHeader = "Selection: Prop #" + std::to_string(_selectedIndex);
            else if (_selectionType == SelectionType::Door) selHeader = "Selection: Door #" + std::to_string(_selectedIndex);
            else if (_selectionType == SelectionType::Spawn) selHeader = "Selection: Player Spawn";

            LabFont::drawText(rightX + 12.0f, propY, selHeader, 1.7f, (_selectionType != SelectionType::None) ? orangeGlow : textDark, LabFontType::System);

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
            } else if (_selectionType == SelectionType::Spawn) {
                pos = _map->spawn.position;
            }

            std::string posStr = "Pos: (" + std::to_string((int)pos.x) + ", " + std::to_string((int)pos.y) + ", " + std::to_string((int)pos.z) + ")";
            LabFont::drawText(rightX + 12.0f, propY + 22.0f, posStr, 1.6f, textDark, LabFontType::System);

            std::string dimStr = "Size: (" + std::to_string((int)dims.x) + " x " + std::to_string((int)dims.y) + " x " + std::to_string((int)dims.z) + ")";
            LabFont::drawText(rightX + 12.0f, propY + 42.0f, dimStr, 1.6f, textDark, LabFontType::System);

            // UV Scale buttons (Source Engine Real Tri-Planar Tiling)
            float uvY = rightY + 115.0f;
            LabFont::drawText(rightX + 12.0f, uvY, "Texture UV Tiling Scale:", 1.7f, textDark, LabFontType::System);

            float scales[4] = { 0.125f, 0.25f, 0.5f, 1.0f };
            const char* scaleLabels[4] = { "0.125", "0.25", "0.5", "1.0" };
            for (int i = 0; i < 4; ++i) {
                float sx = rightX + 12.0f + i * 64.0f;
                bool isCurScale = (std::abs(_activeUvScale.x - scales[i]) < 0.01f);
                Renderer::drawRect(sx, uvY + 18.0f, 60.0f, 24.0f, isCurScale ? Vec3(0.78f, 0.88f, 1.0f) : Vec3(0.88f, 0.88f, 0.90f));
                Renderer::drawRect(sx, uvY + 18.0f, 60.0f, 1.0f, isCurScale ? cyanGlow : winBorder);
                LabFont::drawText(sx + 14.0f, uvY + 23.0f, scaleLabels[i], 1.5f, isCurScale ? Vec3(0.1f, 0.4f, 0.8f) : textDark, LabFontType::System);
            }

            // Texture Preview & Picker
            float texSecY = rightY + 175.0f;
            LabFont::drawText(rightX + 12.0f, texSecY, "Active Texture:", 1.7f, textDark, LabFontType::System);
            Renderer::drawRect(rightX + 12.0f, texSecY + 18.0f, 276.0f, 22.0f, Vec3(1, 1, 1));
            Renderer::drawRect(rightX + 12.0f, texSecY + 18.0f, 276.0f, 1.0f, winBorder);
            LabFont::drawText(rightX + 20.0f, texSecY + 23.0f, _selectedTexture, 1.6f, textDark, LabFontType::System);

            float thumbX = rightX + 12.0f;
            float thumbY = texSecY + 48.0f;
            float thumbS = 80.0f;
            Renderer::drawRect(thumbX, thumbY, thumbS, thumbS, Vec3(0, 0, 0));
            if (_textures.contains(_selectedTexture)) {
                Renderer::drawTextureRect(thumbX + 2.0f, thumbY + 2.0f, thumbS - 4.0f, thumbS - 4.0f, *_textures[_selectedTexture]);
            }

            Renderer::drawRect(rightX + 105.0f, thumbY + 5.0f, 170.0f, 28.0f, Vec3(0.88f, 0.88f, 0.90f));
            Renderer::drawRect(rightX + 105.0f, thumbY + 5.0f, 170.0f, 1.0f, winBorder);
            LabFont::drawText(rightX + 120.0f, thumbY + 12.0f, "Browse Textures...", 1.6f, textDark, LabFontType::System);

            Renderer::drawRect(rightX + 105.0f, thumbY + 40.0f, 170.0f, 28.0f, Vec3(0.88f, 0.88f, 0.90f));
            Renderer::drawRect(rightX + 105.0f, thumbY + 40.0f, 170.0f, 1.0f, winBorder);
            LabFont::drawText(rightX + 125.0f, thumbY + 47.0f, "Apply to Brush", 1.6f, textDark, LabFontType::System);

            // 3D Entity Prop Model Selector
            float modelSecY = thumbY + thumbS + 18.0f;
            LabFont::drawText(rightX + 12.0f, modelSecY, "3D Entity Model (.stl):", 1.7f, textDark, LabFontType::System);
            Renderer::drawRect(rightX + 12.0f, modelSecY + 18.0f, 276.0f, 22.0f, Vec3(1, 1, 1));
            Renderer::drawRect(rightX + 12.0f, modelSecY + 18.0f, 276.0f, 1.0f, winBorder);
            LabFont::drawText(rightX + 20.0f, modelSecY + 23.0f, _selectedModel, 1.6f, textDark, LabFontType::System);

            Renderer::drawRect(rightX + 12.0f, modelSecY + 46.0f, 276.0f, 28.0f, Vec3(0.88f, 0.88f, 0.90f));
            Renderer::drawRect(rightX + 12.0f, modelSecY + 46.0f, 276.0f, 1.0f, winBorder);
            LabFont::drawText(rightX + 60.0f, modelSecY + 53.0f, "Browse 3D Models...", 1.6f, textDark, LabFontType::System);

            // Dimension Adjusters [-] [+]
            float dimSecY = modelSecY + 84.0f;
            LabFont::drawText(rightX + 12.0f, dimSecY, "Adjust Size (X / Y / Z):", 1.7f, textDark, LabFontType::System);

            // X
            LabFont::drawText(rightX + 16.0f, dimSecY + 25.0f, "X:", 1.6f, textDark, LabFontType::System);
            Renderer::drawRect(rightX + 35.0f, dimSecY + 20.0f, 24.0f, 24.0f, Vec3(0.88f, 0.88f, 0.90f));
            LabFont::drawText(rightX + 43.0f, dimSecY + 24.0f, "-", 1.8f, textDark, LabFontType::System);
            Renderer::drawRect(rightX + 65.0f, dimSecY + 20.0f, 24.0f, 24.0f, Vec3(0.88f, 0.88f, 0.90f));
            LabFont::drawText(rightX + 71.0f, dimSecY + 24.0f, "+", 1.8f, textDark, LabFontType::System);

            // Y
            LabFont::drawText(rightX + 105.0f, dimSecY + 25.0f, "Y:", 1.6f, textDark, LabFontType::System);
            Renderer::drawRect(rightX + 125.0f, dimSecY + 20.0f, 24.0f, 24.0f, Vec3(0.88f, 0.88f, 0.90f));
            LabFont::drawText(rightX + 133.0f, dimSecY + 24.0f, "-", 1.8f, textDark, LabFontType::System);
            Renderer::drawRect(rightX + 155.0f, dimSecY + 20.0f, 24.0f, 24.0f, Vec3(0.88f, 0.88f, 0.90f));
            LabFont::drawText(rightX + 161.0f, dimSecY + 24.0f, "+", 1.8f, textDark, LabFontType::System);

            // Z
            LabFont::drawText(rightX + 195.0f, dimSecY + 25.0f, "Z:", 1.6f, textDark, LabFontType::System);
            Renderer::drawRect(rightX + 215.0f, dimSecY + 20.0f, 24.0f, 24.0f, Vec3(0.88f, 0.88f, 0.90f));
            LabFont::drawText(rightX + 223.0f, dimSecY + 24.0f, "-", 1.8f, textDark, LabFontType::System);
            Renderer::drawRect(rightX + 245.0f, dimSecY + 20.0f, 24.0f, 24.0f, Vec3(0.88f, 0.88f, 0.90f));
            LabFont::drawText(rightX + 251.0f, dimSecY + 24.0f, "+", 1.8f, textDark, LabFontType::System);

            // Deselect button
            Renderer::drawRect(rightX + 12.0f, dimSecY + 55.0f, 276.0f, 28.0f, Vec3(0.88f, 0.88f, 0.90f));
            Renderer::drawRect(rightX + 12.0f, dimSecY + 55.0f, 276.0f, 1.0f, winBorder);
            LabFont::drawText(rightX + 90.0f, dimSecY + 62.0f, "Deselect All", 1.6f, textDark, LabFontType::System);
        }

        // ==================== 5. BOTTOM CONSOLE / "Messages" ====================
        float conW = 720.0f;
        float conH = 135.0f;
        float conX = leftW + 25.0f;
        float conY = (float)h - conH - 32.0f;

        Renderer::drawRect(conX, conY, conW, conH, Vec3(1, 1, 1));
        Renderer::drawRect(conX, conY, conW, 22.0f, Vec3(0.85f, 0.90f, 0.96f));
        Renderer::drawRect(conX, conY, conW, 1.0f, winBorder);
        Renderer::drawRect(conX, conY + conH - 1.0f, conW, 1.0f, winBorder);
        Renderer::drawRect(conX, conY, 1.0f, conH, winBorder);
        Renderer::drawRect(conX + conW - 1.0f, conY, 1.0f, conH, winBorder);

        LabFont::drawText(conX + 10.0f, conY + 5.0f, "Editor Messages & Optimization Log", 1.7f, textDark, LabFontType::System);

        for (int i = 0; i < (int)_consoleMessages.size(); ++i) {
            LabFont::drawText(conX + 12.0f, conY + 28.0f + i * 14.0f, _consoleMessages[i], 1.5f, Vec3(0.1f, 0.15f, 0.2f), LabFontType::System);
        }

        // ==================== 6. STATUS BAR ====================
        float sbY = (float)h - 22.0f;
        Renderer::drawRect(0, sbY, (float)w, 22.0f, winBg);
        Renderer::drawRect(0, sbY, (float)w, 1.0f, winBorder);

        std::string sbText = "RMB Fly | LMB Pick/Apply | E Place | F Focus | Ctrl+D Duplicate | Del Delete | " + _cullingStats;
        LabFont::drawText(10.0f, sbY + 5.0f, sbText, 1.5f, textDark, LabFontType::System);
        std::string gridStr = "Snap: " + std::to_string((int)_gridSnap) + " | F9: Run";
        LabFont::drawText((float)w - 260.0f, sbY + 5.0f, gridStr, 1.5f, textDark, LabFontType::System);

        // ==================== 7. DROPDOWN FILE MENU ====================
        if (_fileMenuOpen) {
            float menuX = 10.0f;
            float menuY = 24.0f;
            float menuW = 190.0f;
            float menuH = 125.0f;
            Vec3 menuBg{ 0.96f, 0.96f, 0.97f };
            Vec3 menuBorder{ 0.55f, 0.55f, 0.60f };
            Vec3 menuShadow{ 0.2f, 0.2f, 0.2f };

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
                if (i == 4) Renderer::drawRect(menuX + 6.0f, iy - 2.0f, menuW - 12.0f, 1.0f, menuBorder);
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

        Renderer::drawRect(0, 0, (float)w, (float)h, Vec3(0.05f, 0.06f, 0.08f));

        float bw = 820.0f;
        float bh = 600.0f;
        float bx = ((float)w - bw) * 0.5f;
        float by = ((float)h - bh) * 0.5f;

        Renderer::drawRect(bx, by, bw, bh, Vec3(0.92f, 0.92f, 0.94f));
        Renderer::drawRect(bx, by, bw, 28.0f, Vec3(0.2f, 0.35f, 0.55f));
        LabFont::drawText(bx + 14.0f, by + 8.0f, "Texture Browser - Choose Surface Material", 1.8f, Vec3(1, 1, 1), LabFontType::System);

        Renderer::drawRect(bx + bw - 32.0f, by + 4.0f, 24.0f, 20.0f, Vec3(0.85f, 0.25f, 0.25f));
        LabFont::drawText(bx + bw - 25.0f, by + 7.0f, "X", 1.8f, Vec3(1, 1, 1), LabFontType::System);

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

            Renderer::drawRect(tx - 3.0f, ty - 3.0f, thumbSize + 6.0f, thumbSize + 22.0f, isSelected ? Vec3(1.0f, 0.55f, 0.1f) : Vec3(0.7f, 0.72f, 0.75f));
            Renderer::drawRect(tx, ty, thumbSize, thumbSize, Vec3(0, 0, 0));

            if (_textures.contains(_availableTextures[i].filename)) {
                Renderer::drawTextureRect(tx, ty, thumbSize, thumbSize, *_textures[_availableTextures[i].filename]);
            }

            std::string label = _availableTextures[i].filename;
            if (label.size() > 12) label = label.substr(0, 10) + "..";
            LabFont::drawText(tx, ty + thumbSize + 4.0f, label, 1.4f, Vec3(0.1f, 0.1f, 0.1f), LabFontType::System);
        }

        Renderer::endUI();
    }

    // Modal Model Browser Gallery
    void drawModelBrowser() {
        int w = 1600, h = 900;
        Renderer::beginUI(w, h);

        Renderer::drawRect(0, 0, (float)w, (float)h, Vec3(0.05f, 0.06f, 0.08f));

        float bw = 700.0f;
        float bh = 480.0f;
        float bx = ((float)w - bw) * 0.5f;
        float by = ((float)h - bh) * 0.5f;

        Renderer::drawRect(bx, by, bw, bh, Vec3(0.92f, 0.92f, 0.94f));
        Renderer::drawRect(bx, by, bw, 28.0f, Vec3(0.15f, 0.45f, 0.75f));
        LabFont::drawText(bx + 14.0f, by + 8.0f, "3D Model Browser - Place Entity Props", 1.8f, Vec3(1, 1, 1), LabFontType::System);

        Renderer::drawRect(bx + bw - 32.0f, by + 4.0f, 24.0f, 20.0f, Vec3(0.85f, 0.25f, 0.25f));
        LabFont::drawText(bx + bw - 25.0f, by + 7.0f, "X", 1.8f, Vec3(1, 1, 1), LabFontType::System);

        // Browse STL from disk button
        Renderer::drawRect(bx + 20.0f, by + 45.0f, 300.0f, 34.0f, Vec3(0.2f, 0.65f, 0.4f));
        LabFont::drawText(bx + 35.0f, by + 54.0f, "Browse Disk for 3D Model (.stl)...", 1.6f, Vec3(1, 1, 1), LabFontType::System);

        // List discovered models
        float startY = by + 95.0f;
        LabFont::drawText(bx + 20.0f, startY, "Available Models in assets/models/:", 1.7f, Vec3(0.1f, 0.1f, 0.1f), LabFontType::System);

        for (size_t i = 0; i < _availableModels.size(); ++i) {
            float iy = startY + 24.0f + i * 36.0f;
            bool isSel = (_availableModels[i] == _selectedModel);

            Renderer::drawRect(bx + 20.0f, iy, bw - 40.0f, 30.0f, isSel ? Vec3(0.78f, 0.88f, 1.0f) : Vec3(1, 1, 1));
            Renderer::drawRect(bx + 20.0f, iy, bw - 40.0f, 1.0f, isSel ? Vec3(0.2f, 0.75f, 0.95f) : Vec3(0.8f, 0.8f, 0.85f));
            if (isSel) Renderer::drawRect(bx + 20.0f, iy, 4.0f, 30.0f, Vec3(1.0f, 0.55f, 0.1f));

            LabFont::drawText(bx + 32.0f, iy + 7.0f, _availableModels[i], 1.6f, isSel ? Vec3(0.1f, 0.35f, 0.75f) : Vec3(0.15f, 0.15f, 0.15f), LabFontType::System);
        }

        Renderer::endUI();
    }

    // Modal Help
    void drawHelpModal() {
        int w = 1600, h = 900;
        Renderer::beginUI(w, h);

        Renderer::drawRect(0, 0, (float)w, (float)h, Vec3(0.05f, 0.06f, 0.08f));

        float bw = 650.0f;
        float bh = 420.0f;
        float bx = ((float)w - bw) * 0.5f;
        float by = ((float)h - bh) * 0.5f;

        Renderer::drawRect(bx, by, bw, bh, Vec3(0.92f, 0.92f, 0.94f));
        Renderer::drawRect(bx, by, bw, 28.0f, Vec3(0.2f, 0.35f, 0.55f));
        LabFont::drawText(bx + 14.0f, by + 8.0f, "Lab Hammer 2026 - Keyboard & Mouse Reference", 1.8f, Vec3(1, 1, 1), LabFontType::System);

        Renderer::drawRect(bx + bw - 32.0f, by + 4.0f, 24.0f, 20.0f, Vec3(0.85f, 0.25f, 0.25f));
        LabFont::drawText(bx + bw - 25.0f, by + 7.0f, "X", 1.8f, Vec3(1, 1, 1), LabFontType::System);

        const char* helpLines[] = {
            "Hold RMB + WASD: Free-cam flying (Shift = Boost, Space = Up, Ctrl = Down)",
            "LMB Click in 3D: Raycast selection of Brushes, Props, Doors, Spawns",
            "Tool 3 (Pipette): LMB applies active texture, RMB samples clicked brush texture",
            "E Key: Place object on grid at 3D cursor (Brush, Prop, Door, Spawn)",
            "F Key: Center & Focus Camera on selected entity",
            "Ctrl + D: Duplicate selected entity offset on grid",
            "Delete / Backspace: Delete selected entity",
            "Arrow Keys / PageUp / PageDn: Translate selected object along grid axes",
            "F9: Quick Save and Launch Map in Frozen-Life Engine (Lab.exe)",
            "Optimization: Frustum culling skips rendering off-screen brushes and props"
        };

        for (int i = 0; i < 10; ++i) {
            LabFont::drawText(bx + 25.0f, by + 45.0f + i * 34.0f, helpLines[i], 1.5f, Vec3(0.12f, 0.15f, 0.2f), LabFontType::System);
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

    std::vector<TextureEntry> _availableTextures;
    std::vector<std::string> _availableModels;
    std::string _selectedTexture = "concrete_wall.bmp";
    std::string _selectedModel = "Model.stl";
    Vec2 _activeUvScale{ 0.25f, 0.25f };

    bool _browserOpen = false;
    bool _modelBrowserOpen = false;
    bool _helpModalOpen = false;
    bool _fileMenuOpen = false;
    bool _wireframeMode = false;
    std::string _currentMapPath = "";

    // Selection State
    SelectionType _selectionType = SelectionType::None;
    int _selectedIndex = -1;

    // Sidebar State
    SidebarTab _sidebarTab = SidebarTab::Properties;
    int _outlinerScroll = 0;

    int _activeTool = 1; // 0=Select, 1=Brush, 2=Prop, 3=Texture, 4=Door, 5=Spawn, 6=Resize, 7=Sun
    Vec3 _cursorPos{ 0, 0, 0 };
    Vec3 _brushSize{ 2.0f, 2.0f, 2.0f };
    Vec3 _propScale{ 1.0f, 1.0f, 1.0f };
    float _gridSnap = 1.0f;
    float _sunAngle = 45.0f;

    std::vector<std::string> _consoleMessages;
    std::string _cullingStats = "";

    // Input States
    bool _lmbPressed = false;
    bool _rmbPressed = false;
    bool _ePressed = false;
    bool _ctrlSPressed = false;
    bool _ctrlOPressed = false;
    bool _ctrlNPressed = false;
    bool _ctrlDPressed = false;
    bool _delPressed = false;
    bool _fPressed = false;
    bool _f9Pressed = false;
    bool _arrowLeftPressed = false;
    bool _arrowRightPressed = false;
    bool _arrowUpPressed = false;
    bool _arrowDownPressed = false;
    bool _pageUpPressed = false;
    bool _pageDownPressed = false;
};

int main() {
    LabHammerStandalone hammer;
    hammer.run();
    return 0;
}
