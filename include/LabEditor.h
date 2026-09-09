#pragma once
#include <vector>
#include <string>
#include <memory>
#include <algorithm>
#include "LabMath.h"
#include "LabMap.h"
#include "LabRenderer.h"

namespace Lab {

    enum class SidebarTab {
        Properties,
        Hierarchy,
        Prebuilts
    };

    struct SidebarLayout {
        float rightX;
        float rightY;
        float rightW;
        float rightH;

        // Tabs
        float tabPropX, tabPropY, tabPropW, tabPropH;
        float tabOutX, tabOutY, tabOutW, tabOutH;
        float tabPreX, tabPreY, tabPreW, tabPreH;

        // Outliner
        float outListX, outListY, outListW, outListH;
        float outItemH, outItemSpacing;
        float outFocusX, outFocusY, outFocusW, outFocusH;
        float outDupX, outDupY, outDupW, outDupH;
        float outDelX, outDelY, outDelW, outDelH;
        float outPrevX, outPrevY, outPrevW, outPrevH;
        float outNextX, outNextY, outNextW, outNextH;

        // Properties
        float uvBtnY, uvBtnW, uvBtnH;
        float texBoxX, texBoxY, texBoxW, texBoxH;
        float thumbX, thumbY, thumbS;
        float texBrowseX, texBrowseY, texBrowseW, texBrowseH;
        float texApplyX, texApplyY, texApplyW, texApplyH;
        float modelBoxX, modelBoxY, modelBoxW, modelBoxH;
        float modelBrowseX, modelBrowseY, modelBrowseW, modelBrowseH;
        // Position translation
        float posBtnsY, posBtnW, posBtnH;
        float elevUpX, elevUpY, elevUpW, elevUpH;
        float elevDwnX, elevDwnY, elevDwnW, elevDwnH;
        float elevGndX, elevGndY, elevGndW, elevGndH;
        // Size scaling
        float dimBtnsY, dimBtnW, dimBtnH;
        float deselX, deselY, deselW, deselH;
        float delX, delY, delW, delH;
    };

    inline SidebarLayout getSidebarLayout(float w, float h) {
        SidebarLayout l;
        l.rightW = 320.0f;
        l.rightX = w - l.rightW;
        l.rightY = 58.0f;
        l.rightH = h - l.rightY - 22.0f;

        // Tabs (Properties, Struktura, Prebuilty)
        float tabW = 96.0f;
        l.tabPropX = l.rightX + 8.0f;
        l.tabPropY = l.rightY + 6.0f;
        l.tabPropW = tabW;
        l.tabPropH = 26.0f;

        l.tabOutX = l.tabPropX + tabW + 6.0f;
        l.tabOutY = l.rightY + 6.0f;
        l.tabOutW = tabW;
        l.tabOutH = 26.0f;

        l.tabPreX = l.tabOutX + tabW + 6.0f;
        l.tabPreY = l.rightY + 6.0f;
        l.tabPreW = tabW;
        l.tabPreH = 26.0f;

        // Outliner
        l.outListX = l.rightX + 10.0f;
        l.outListY = l.rightY + 60.0f;
        l.outListW = 280.0f;
        l.outListH = std::max(200.0f, l.rightH - 180.0f);
        l.outItemH = 23.0f;
        l.outItemSpacing = 25.0f;

        float actY = l.outListY + l.outListH + 10.0f;
        l.outFocusX = l.rightX + 10.0f;  l.outFocusY = actY; l.outFocusW = 88.0f; l.outFocusH = 28.0f;
        l.outDupX   = l.rightX + 104.0f; l.outDupY   = actY; l.outDupW   = 88.0f; l.outDupH   = 28.0f;
        l.outDelX   = l.rightX + 198.0f; l.outDelY   = actY; l.outDelW   = 88.0f; l.outDelH   = 28.0f;

        float pageY = actY + 34.0f;
        l.outPrevX = l.rightX + 10.0f;  l.outPrevY = pageY; l.outPrevW = 135.0f; l.outPrevH = 26.0f;
        l.outNextX = l.rightX + 150.0f; l.outNextY = pageY; l.outNextW = 135.0f; l.outNextH = 26.0f;

        // Properties
        l.uvBtnY = l.rightY + 125.0f;
        l.uvBtnW = 66.0f;
        l.uvBtnH = 24.0f;

        l.texBoxX = l.rightX + 10.0f;
        l.texBoxY = l.uvBtnY + 46.0f;
        l.texBoxW = 280.0f;
        l.texBoxH = 22.0f;

        l.thumbX = l.rightX + 10.0f;
        l.thumbY = l.texBoxY + 28.0f;
        l.thumbS = 80.0f;

        l.texBrowseX = l.rightX + 100.0f;
        l.texBrowseY = l.thumbY + 4.0f;
        l.texBrowseW = 186.0f;
        l.texBrowseH = 32.0f;

        l.texApplyX = l.rightX + 100.0f;
        l.texApplyY = l.thumbY + 44.0f;
        l.texApplyW = 186.0f;
        l.texApplyH = 32.0f;

        l.modelBoxX = l.rightX + 10.0f;
        l.modelBoxY = l.thumbY + l.thumbS + 28.0f;
        l.modelBoxW = 280.0f;
        l.modelBoxH = 22.0f;

        l.modelBrowseX = l.rightX + 10.0f;
        l.modelBrowseY = l.modelBoxY + 28.0f;
        l.modelBrowseW = 280.0f;
        l.modelBrowseH = 32.0f;

        // Position Translation (Move X / Y / Z on grid)
        l.posBtnsY = l.modelBrowseY + 52.0f;
        l.posBtnW = 26.0f;
        l.posBtnH = 24.0f;

        // Quick Elevation Buttons (Up / Down / Ground)
        l.elevUpX = l.rightX + 10.0f;
        l.elevUpY = l.posBtnsY + 28.0f;
        l.elevUpW = 88.0f;
        l.elevUpH = 26.0f;

        l.elevDwnX = l.rightX + 104.0f;
        l.elevDwnY = l.posBtnsY + 28.0f;
        l.elevDwnW = 88.0f;
        l.elevDwnH = 26.0f;

        l.elevGndX = l.rightX + 198.0f;
        l.elevGndY = l.posBtnsY + 28.0f;
        l.elevGndW = 92.0f;
        l.elevGndH = 26.0f;

        // Dimension Adjusters (Scale X / Y / Z)
        l.dimBtnsY = l.elevUpY + 48.0f;
        l.dimBtnW = 26.0f;
        l.dimBtnH = 24.0f;

        l.deselX = l.rightX + 10.0f;
        l.deselY = l.dimBtnsY + 34.0f;
        l.deselW = 280.0f;
        l.deselH = 30.0f;

        l.delX = l.rightX + 10.0f;
        l.delY = l.deselY + 36.0f;
        l.delW = 280.0f;
        l.delH = 30.0f;

        return l;
    }

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
                case 8: // Weapon Spawn Silhouette (Gun barrel + grip + magazine)
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
                default:
                    Renderer::drawRect(x + 6.0f, y + 6.0f, 14.0f, 14.0f, color);
                    break;
            }
        }
    }

}
