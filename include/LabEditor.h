#pragma once
#include <vector>
#include <string>
#include <memory>
#include "LabMath.h"
#include "LabMap.h"
#include "LabRenderer.h"

namespace Lab {

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
        EditorTool currentTool = EditorTool::CreateBrush;
        EditorCursor3D cursor;
        int selectedBrushIndex = -1;
        std::string currentMapName = "facility_hammer.labmap";
        std::string statusMessage = "Hammer: Ready";

        void toggle(Camera& cam) {
            active = !active;
            if (active) {
                statusMessage = "Hammer Editor Active [Noclip + Grid Snap]";
                // place cursor in front of camera
                cursor.position = snapToGrid(cam.getPosition() + cam.getForward() * 6.0f, cursor.gridSnap);
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
            if (!map.brushes.empty()) {
                map.brushes.pop_back();
                statusMessage = "Deleted last brush";
            }
        }

        void draw3DOverlay() {
            if (!active) return;

            // Draw Wireframe/Hologram Ghost Brush at Cursor Position (Valve Orange / Cyan)
            Vec3 ghostColor = Vec3(1.0f, 0.55f, 0.1f); // Valve Hammer Orange
            Renderer::drawCube(cursor.position, cursor.brushSize, ghostColor, false);

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

            // Tool selector buttons (Brush, Door, Save)
            Renderer::drawRect(240.0f, 8.0f, 90.0f, 24.0f, currentTool == EditorTool::CreateBrush ? Vec3(0.2f, 0.6f, 0.9f) : Vec3(0.18f, 0.2f, 0.25f));
            Renderer::drawRect(340.0f, 8.0f, 90.0f, 24.0f, currentTool == EditorTool::CreateDoor ? Vec3(0.2f, 0.6f, 0.9f) : Vec3(0.18f, 0.2f, 0.25f));
            Renderer::drawRect(440.0f, 8.0f, 90.0f, 24.0f, Vec3(0.2f, 0.55f, 0.35f)); // Save button

            // Bottom Status Console
            Renderer::drawRect(0, (float)screenH - 32.0f, (float)screenW, 32.0f, Vec3(0.08f, 0.1f, 0.13f));
            Renderer::drawRect(10.0f, (float)screenH - 22.0f, 12.0f, 12.0f, Vec3(0.2f, 0.85f, 0.4f));
        }
    };

}
