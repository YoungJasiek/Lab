#pragma once
#include <vector>
#include <string>
#include <memory>
#include <algorithm>
#include "LabMath.h"
#include "LabMap.h"
#include "LabRenderer.h"
#include "LabTheme.h"

namespace Lab {

    enum class SidebarTab {
        Properties,
        Hierarchy,
        Prebuilts
    };

    enum class EditorTool {
        Select,
        CreateBrush,
        CreateLight,
        CreateDoor,
        CreateSpawn
    };

    struct EditorCursor3D {
        Vec3 position{ 0.0f, 0.0f, -5.0f };
        Vec3 brushSize{ 2.0f, 2.0f, 2.0f };
        Vec3 brushColor{ 0.45f, 0.5f, 0.55f };
        float gridSnap = 1.0f;
    };

    class LabHammerEditor {
    public:
        bool active = false;
        bool darknessMode = false;
        EditorTool currentTool = EditorTool::CreateBrush;
        EditorCursor3D cursor;
        int selectedBrushIndex = -1;
        std::string currentMapName = "facility_hammer.labmap";
        std::string statusMessage = "Hammer: Ready";

        // Light Tool parameters
        Vec3 lightColor{ 1.0f, 0.95f, 0.85f }; // Warm 2700K incandescent
        float lightRadius = 14.0f;
        float lightIntensity = 2.5f;

        void toggle(Camera& cam) {
            active = !active;
            if (active) {
                statusMessage = "Hammer Editor Active [Noclip + Grid Snap]";
                // place cursor in front of camera
                cursor.position = snapToGrid(cam.getPosition() + cam.getForward() * 6.0f, cursor.gridSnap);
            }
        }

        void toggleDarkness() {
            darknessMode = !darknessMode;
            statusMessage = darknessMode ? "Hammer: Darkness Mode ON [Pitch Black Ambient/Sun]" 
                                         : "Hammer: Darkness Mode OFF [Standard Daylight]";
        }

        static Vec3 snapToGrid(const Vec3& v, float snap) {
            if (snap <= 0.001f) return v;
            return Vec3(
                std::round(v.x / snap) * snap,
                std::round(v.y / snap) * snap,
                std::round(v.z / snap) * snap
            );
        }

        void update(float /*dt*/, Camera& cam, LabMap& /*currentMap*/) {
            if (!active) return;

            // Move cursor with arrow keys / numpad or cam forward
            cursor.position = snapToGrid(cam.getPosition() + cam.getForward() * 6.0f, cursor.gridSnap);
        }

        void placeBrush(LabMap& map) {
            MapBrush b;
            b.position = cursor.position;
            b.size = cursor.brushSize;
            b.color = cursor.brushColor;
            b.texturePath = "concrete_wall.bmp";
            map.brushes.push_back(b);
            statusMessage = "Placed Brush at (" + std::to_string((int)b.position.x) + ", " + std::to_string((int)b.position.y) + ", " + std::to_string((int)b.position.z) + ")";
        }

        void placeLight(LabMap& map) {
            MapLight lt;
            lt.name = "light_omni_" + std::to_string(map.lights.size() + 1);
            lt.position = cursor.position;
            lt.color = lightColor;
            lt.intensity = lightIntensity;
            lt.radius = lightRadius;
            lt.type = MapLightType::Point;
            map.lights.push_back(lt);
            statusMessage = "Placed Point Light #" + std::to_string(map.lights.size()) + " at (" + 
                            std::to_string((int)lt.position.x) + ", " + 
                            std::to_string((int)lt.position.y) + ", " + 
                            std::to_string((int)lt.position.z) + ") [Radius: " + 
                            std::to_string((int)lt.radius) + "m]";
        }

        void placeDoor(LabMap& map) {
            MapDoor d;
            d.position = cursor.position;
            d.size = Vec3(2.0f, 3.5f, 0.4f);
            d.color = Vec3(0.25f, 0.4f, 0.6f);
            d.openOffset = Vec3(0.0f, 3.5f, 0.0f);
            d.triggerRadius = 4.5f;
            d.openSpeed = 2.0f;
            map.doors.push_back(d);
            statusMessage = "Placed Dynamic Door at (" + std::to_string((int)d.position.x) + ")";
        }

        void saveMap(const LabMap& map, const std::string& filepath) {
            if (map.saveToFile(filepath)) {
                statusMessage = "Saved to " + filepath;
            } else {
                statusMessage = "Error saving map!";
            }
        }

        void deleteLast(LabMap& map) {
            if (currentTool == EditorTool::CreateLight && !map.lights.empty()) {
                map.lights.pop_back();
                statusMessage = "Deleted last point light";
            } else if (!map.brushes.empty()) {
                map.brushes.pop_back();
                statusMessage = "Deleted last brush";
            }
        }

        void draw3DOverlay(LabMap* map = nullptr) {
            if (!active) return;

            // Draw Wireframe/Hologram Ghost Brush or Light Bulb at Cursor Position
            if (currentTool == EditorTool::CreateLight) {
                // Glowing light bulb indicator
                Renderer::drawCube(cursor.position, Vec3(0.5f, 0.5f, 0.5f), lightColor, false);
                // Wireframe volume illustrating light influence radius
                Renderer::drawWireCube(cursor.position, Vec3(lightRadius * 0.5f, lightRadius * 0.5f, lightRadius * 0.5f), lightColor);
            } else {
                Vec3 ghostColor = Vec3(1.0f, 0.55f, 0.1f); // Valve Hammer Orange
                Renderer::drawCube(cursor.position, cursor.brushSize, ghostColor, false);
            }

            // Draw existing map dynamic point lights as glowing markers
            if (map) {
                for (const auto& l : map->lights) {
                    Renderer::drawCube(l.position, Vec3(0.35f, 0.35f, 0.35f), l.color, false);
                    Renderer::drawWireCube(l.position, Vec3(l.radius * 0.35f, l.radius * 0.35f, l.radius * 0.35f), l.color * 0.7f);
                }
            }

            // Draw coordinate axis markers around cursor
            Renderer::drawCube(cursor.position + Vec3(1.5f, 0, 0), Vec3(1.0f, 0.06f, 0.06f), Vec3(1.0f, 0.2f, 0.2f), false); // X
            Renderer::drawCube(cursor.position + Vec3(0, 1.5f, 0), Vec3(0.06f, 1.0f, 0.06f), Vec3(0.2f, 1.0f, 0.2f), false); // Y
            Renderer::drawCube(cursor.position + Vec3(0, 0, 1.5f), Vec3(0.06f, 0.06f, 1.0f), Vec3(0.2f, 0.5f, 1.0f), false); // Z
        }

        void drawUI(int screenW, int screenH) {
            if (!active) return;

            // Hammer Editor Top Toolbar (Valve Source Style dark industrial gray)
            Renderer::drawRect(0, 0, (float)screenW, 40.0f, Vec3(0.12f, 0.14f, 0.18f));
            Renderer::drawRect(0, 38.0f, (float)screenW, 2.0f, Vec3(1.0f, 0.55f, 0.1f)); // Orange Hammer accent

            // Editor Status & Controls Bar
            Renderer::drawRect(10.0f, 8.0f, 24.0f, 24.0f, Vec3(1.0f, 0.55f, 0.1f)); // Hammer Logo Icon
            Renderer::drawRect(45.0f, 12.0f, 160.0f, 16.0f, Vec3(0.2f, 0.25f, 0.32f));

            // Tool selector buttons (Brush, Point Light, Door, Darkness Mode, Save)
            Renderer::drawRect(240.0f, 8.0f, 85.0f, 24.0f, currentTool == EditorTool::CreateBrush ? Vec3(0.2f, 0.6f, 0.9f) : Vec3(0.18f, 0.2f, 0.25f));
            Renderer::drawRect(332.0f, 8.0f, 95.0f, 24.0f, currentTool == EditorTool::CreateLight ? Vec3(0.95f, 0.75f, 0.15f) : Vec3(0.18f, 0.2f, 0.25f));
            Renderer::drawRect(434.0f, 8.0f, 85.0f, 24.0f, currentTool == EditorTool::CreateDoor ? Vec3(0.2f, 0.6f, 0.9f) : Vec3(0.18f, 0.2f, 0.25f));
            // Darkness mode button
            Renderer::drawRect(526.0f, 8.0f, 115.0f, 24.0f, darknessMode ? Vec3(0.6f, 0.15f, 0.85f) : Vec3(0.18f, 0.2f, 0.25f));
            Renderer::drawRect(648.0f, 8.0f, 85.0f, 24.0f, Vec3(0.2f, 0.55f, 0.35f)); // Save button

            // Bottom Status Console
            Renderer::drawRect(0, (float)screenH - 32.0f, (float)screenW, 32.0f, Vec3(0.08f, 0.1f, 0.13f));
            Renderer::drawRect(10.0f, (float)screenH - 22.0f, 12.0f, 12.0f, darknessMode ? Vec3(0.8f, 0.2f, 0.9f) : Vec3(0.2f, 0.85f, 0.4f));
        }
    };

    namespace HammerIcons {
        inline void drawHammerIcon(int iconId, float x, float y, const Vec3& color, const Vec3& bg) {
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

        inline void drawToolbarIcon(int iconId, float x, float y, const Vec3& color, const Vec3& bg) {
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

        // Entity / Prebuilt Item Icon (Player Spawns, Ammo, Medkit, Barricades, Weapons)
        inline void drawEntityIcon(int entityType, float x, float y, const Vec3& color, const Vec3& bg) {
            Renderer::drawRect(x, y, 26.0f, 26.0f, bg);
            Renderer::drawRect(x, y, 26.0f, 1.0f, Vec3(0.7f, 0.7f, 0.7f));
            Renderer::drawRect(x, y + 25.0f, 26.0f, 1.0f, Vec3(0.5f, 0.5f, 0.5f));

            switch (entityType) {
                case 0: // Player Spawn (FFA / Alpha / Beta)
                case 1:
                case 2:
                    drawHammerIcon(5, x + 1.0f, y + 1.0f, color, bg);
                    break;
                case 3: // Ammo Crate (Olive/Gold container with brass bullet lines)
                    Renderer::drawRect(x + 4.0f, y + 6.0f, 18.0f, 14.0f, color);
                    Renderer::drawRect(x + 7.0f, y + 8.0f, 3.0f, 9.0f, Vec3(1.0f, 0.85f, 0.2f));
                    Renderer::drawRect(x + 12.0f, y + 8.0f, 3.0f, 9.0f, Vec3(1.0f, 0.85f, 0.2f));
                    Renderer::drawRect(x + 17.0f, y + 8.0f, 3.0f, 9.0f, Vec3(1.0f, 0.85f, 0.2f));
                    break;
                case 4: // Medkit (Clean White Case with Red Cross)
                    Renderer::drawRect(x + 4.0f, y + 5.0f, 18.0f, 16.0f, Vec3(0.95f, 0.95f, 0.95f));
                    Renderer::drawRect(x + 11.0f, y + 8.0f, 4.0f, 10.0f, Vec3(0.9f, 0.15f, 0.15f));
                    Renderer::drawRect(x + 8.0f, y + 11.0f, 10.0f, 4.0f, Vec3(0.9f, 0.15f, 0.15f));
                    break;
                case 5: // Tactical Barricade
                    Renderer::drawRect(x + 5.0f, y + 8.0f, 16.0f, 11.0f, color);
                    Renderer::drawRect(x + 7.0f, y + 10.0f, 12.0f, 2.0f, Vec3(0.9f, 0.75f, 0.1f));
                    Renderer::drawRect(x + 7.0f, y + 14.0f, 12.0f, 2.0f, Vec3(0.9f, 0.75f, 0.1f));
                    break;
                case 6: // Concrete Pillar
                    Renderer::drawRect(x + 9.0f, y + 4.0f, 8.0f, 18.0f, color);
                    Renderer::drawRect(x + 7.0f, y + 4.0f, 12.0f, 3.0f, Vec3(0.35f, 0.38f, 0.42f));
                    Renderer::drawRect(x + 7.0f, y + 19.0f, 12.0f, 3.0f, Vec3(0.35f, 0.38f, 0.42f));
                    break;
                case 7: // Security Door
                    drawHammerIcon(4, x + 1.0f, y + 1.0f, color, bg);
                    break;
                case 8: // Weapon Spawn Silhouette (Gun barrel + grip + reservoir)
                    Renderer::drawRect(x + 5.0f, y + 10.0f, 16.0f, 4.0f, color);
                    Renderer::drawRect(x + 7.0f, y + 14.0f, 4.0f, 7.0f, color);
                    Renderer::drawRect(x + 13.0f, y + 14.0f, 3.0f, 5.0f, Vec3(0.2f, 0.2f, 0.2f));
                    break;
                case 9: // Wooden Crate (Source Engine cedar box with cross brace)
                    Renderer::drawRect(x + 4.0f, y + 4.0f, 18.0f, 18.0f, Vec3(0.60f, 0.42f, 0.25f));
                    Renderer::drawRect(x + 4.0f, y + 4.0f, 18.0f, 2.0f, Vec3(0.25f, 0.26f, 0.28f));
                    Renderer::drawRect(x + 4.0f, y + 20.0f, 18.0f, 2.0f, Vec3(0.25f, 0.26f, 0.28f));
                    Renderer::drawRect(x + 4.0f, y + 4.0f, 2.0f, 18.0f, Vec3(0.25f, 0.26f, 0.28f));
                    Renderer::drawRect(x + 20.0f, y + 4.0f, 2.0f, 18.0f, Vec3(0.25f, 0.26f, 0.28f));
                    Renderer::drawRect(x + 6.0f, y + 6.0f, 14.0f, 2.0f, Vec3(0.48f, 0.32f, 0.18f));
                    Renderer::drawRect(x + 8.0f, y + 10.0f, 10.0f, 2.0f, Vec3(0.48f, 0.32f, 0.18f));
                    Renderer::drawRect(x + 10.0f, y + 14.0f, 6.0f, 2.0f, Vec3(0.48f, 0.32f, 0.18f));
                    break;
                case 10: // Red Hazard Fuel Barrel (Crimson with yellow/black hazard band)
                    Renderer::drawRect(x + 6.0f, y + 4.0f, 14.0f, 18.0f, Vec3(0.85f, 0.15f, 0.15f));
                    Renderer::drawRect(x + 5.0f, y + 3.0f, 16.0f, 2.0f, Vec3(0.25f, 0.26f, 0.28f));
                    Renderer::drawRect(x + 5.0f, y + 21.0f, 16.0f, 2.0f, Vec3(0.25f, 0.26f, 0.28f));
                    // Yellow/black hazard stripe in center
                    Renderer::drawRect(x + 6.0f, y + 10.0f, 14.0f, 5.0f, Vec3(0.95f, 0.82f, 0.1f));
                    Renderer::drawRect(x + 8.0f, y + 10.0f, 3.0f, 5.0f, Vec3(0.12f, 0.12f, 0.14f));
                    Renderer::drawRect(x + 14.0f, y + 10.0f, 3.0f, 5.0f, Vec3(0.12f, 0.12f, 0.14f));
                    break;
                case 11: // Lamp / Light Source (Glowing warm bulb with radial rays)
                    Renderer::drawRect(x + 10.0f, y + 8.0f, 6.0f, 7.0f, Vec3(1.0f, 0.95f, 0.5f));
                    Renderer::drawRect(x + 11.0f, y + 15.0f, 4.0f, 3.0f, Vec3(0.6f, 0.6f, 0.65f));
                    Renderer::drawRect(x + 6.0f, y + 10.0f, 2.0f, 3.0f, color);
                    Renderer::drawRect(x + 18.0f, y + 10.0f, 2.0f, 3.0f, color);
                    Renderer::drawRect(x + 12.0f, y + 4.0f, 2.0f, 2.0f, color);
                    break;
                default:
                    Renderer::drawRect(x + 6.0f, y + 6.0f, 14.0f, 14.0f, color);
                    break;
            }
        }
    }

}
