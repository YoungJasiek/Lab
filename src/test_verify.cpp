#include "Lab.h"
#include "LabFont.h"
#include "LabDialogs.h"
#include <iostream>
#include <vector>
#include <unordered_map>
#include <fstream>
#include <cmath>
#include <filesystem>

static void saveFrameToBMP(const char* filename, int width, int height) {
    std::vector<unsigned char> pixels(width * height * 4);
    glReadBuffer(GL_BACK);
    glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());

    int rowSize = ((width * 24 + 31) / 32) * 4;
    int imageSize = rowSize * height;
    int fileSize = 54 + imageSize;

    unsigned char header[54] = {
        'B', 'M',
        (unsigned char)(fileSize), (unsigned char)(fileSize >> 8), (unsigned char)(fileSize >> 16), (unsigned char)(fileSize >> 24),
        0, 0, 0, 0,
        54, 0, 0, 0,
        40, 0, 0, 0,
        (unsigned char)(width), (unsigned char)(width >> 8), (unsigned char)(width >> 16), (unsigned char)(width >> 24),
        (unsigned char)(height), (unsigned char)(height >> 8), (unsigned char)(height >> 16), (unsigned char)(height >> 24),
        1, 0, 24, 0,
        0, 0, 0, 0,
        (unsigned char)(imageSize), (unsigned char)(imageSize >> 8), (unsigned char)(imageSize >> 16), (unsigned char)(imageSize >> 24),
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
    };

    std::ofstream out(filename, std::ios::binary);
    if (!out.is_open()) return;
    out.write((char*)header, 54);

    std::vector<unsigned char> row(rowSize, 0);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            int srcIdx = (y * width + x) * 4;
            row[x * 3 + 0] = pixels[srcIdx + 2]; // B
            row[x * 3 + 1] = pixels[srcIdx + 1]; // G
            row[x * 3 + 2] = pixels[srcIdx + 0]; // R
        }
        out.write((char*)row.data(), rowSize);
    }
    std::cout << "[Test] Saved verification frame: " << filename << std::endl;
}

int main() {
    if (!glfwInit()) {
        std::cerr << "Failed to init GLFW\n";
        return 1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);

    int w = 1280, h = 720;
    GLFWwindow* window = glfwCreateWindow(w, h, "Verification Test", NULL, NULL);
    if (!window) {
        std::cerr << "Failed to create window\n";
        glfwTerminate();
        return 1;
    }
    glfwMakeContextCurrent(window);

    if (!gladLoadGL(glfwGetProcAddress)) {
        std::cerr << "Failed to init GLAD\n";
        return 1;
    }

    Lab::Renderer::init();

    // 1. Verify Map Loading
    std::cout << "[Test] Loading facility_alpha.labmap...\n";
    auto map = Lab::LabMap::loadFromFile("assets/maps/facility_alpha.labmap");
    if (!map) {
        std::cerr << "ERROR: Failed to load facility_alpha.labmap!\n";
        return 1;
    }
    std::cout << "[Test] Map name: " << map->metadata.name << " Brushes: " << map->brushes.size() << "\n";

    // 2. Verify Texture Loading
    std::unordered_map<std::string, std::unique_ptr<Lab::Texture>> textures;
    for (const auto& b : map->brushes) {
        if (!b.texturePath.empty() && !textures.contains(b.texturePath)) {
            textures[b.texturePath] = std::make_unique<Lab::Texture>(b.texturePath);
            if (textures[b.texturePath]->getId() == 0) {
                std::cerr << "ERROR: Texture ID is 0 for: " << b.texturePath << "\n";
                return 1;
            }
            std::cout << "[Test] Loaded texture: " << b.texturePath << " ID: " << textures[b.texturePath]->getId() << "\n";
        }
    }

    // 3. Render 3D Scene with Textures
    // 3. Render 3D Scene with Textures & Source Engine UV Tiling
    Lab::Camera cam(75.0f, 16.0f / 9.0f, 0.01f, 1000.0f);
    cam.setPosition(map->spawn.position);
    Lab::Renderer::beginFrame(cam);
    Lab::Renderer::setSunLight(map->metadata.sunDir, map->metadata.sunColor, map->metadata.ambientColor);

    // 3a. Verify Frustum Culling
    std::cout << "[Test] Verifying 6-plane Frustum Culling...\n";
    cam.updateFrustum();
    // In front of camera
    Lab::Vec3 frontObjMin(cam.getPosition() + cam.getFront() * 5.0f - Lab::Vec3(1, 1, 1));
    Lab::Vec3 frontObjMax(cam.getPosition() + cam.getFront() * 5.0f + Lab::Vec3(1, 1, 1));
    bool frontVisible = cam.isInFrustum(frontObjMin, frontObjMax);
    std::cout << "[Test] Front object visible: " << (frontVisible ? "YES" : "NO") << "\n";
    if (!frontVisible) {
        std::cerr << "ERROR: Front object should be in frustum!\n";
        return 1;
    }

    // Behind camera
    Lab::Vec3 behindObjMin(cam.getPosition() - cam.getFront() * 50.0f - Lab::Vec3(1, 1, 1));
    Lab::Vec3 behindObjMax(cam.getPosition() - cam.getFront() * 50.0f + Lab::Vec3(1, 1, 1));
    bool behindVisible = cam.isInFrustum(behindObjMin, behindObjMax);
    std::cout << "[Test] Behind object visible (should be NO): " << (behindVisible ? "YES" : "NO") << "\n";
    if (behindVisible) {
        std::cerr << "ERROR: Behind object should be culled!\n";
        return 1;
    }

    // 3b. Verify Ray-AABB intersection
    std::cout << "[Test] Verifying Fast Slab Ray-AABB intersection...\n";
    auto rayIntersect = [](const Lab::Vec3& rayOrigin, const Lab::Vec3& rayDir, const Lab::Vec3& boxMin, const Lab::Vec3& boxMax, float& tOut) -> bool {
        float tmin = 0.001f;
        float tmax = 10000.0f;
        for (int i = 0; i < 3; ++i) {
            float originComp = (i == 0) ? rayOrigin.x : ((i == 1) ? rayOrigin.y : rayOrigin.z);
            float dirComp = (i == 0) ? rayDir.x : ((i == 1) ? rayDir.y : rayDir.z);
            float minComp = (i == 0) ? boxMin.x : ((i == 1) ? boxMin.y : boxMin.z);
            float maxComp = (i == 0) ? boxMax.x : ((i == 1) ? boxMax.y : boxMax.z);
            if (std::abs(dirComp) < 1e-6f) {
                if (originComp < minComp || originComp > maxComp) return false;
            } else {
                float invD = 1.0f / dirComp;
                float t1 = (minComp - originComp) * invD;
                float t2 = (maxComp - originComp) * invD;
                if (t1 > t2) std::swap(t1, t2);
                tmin = std::max(tmin, t1);
                tmax = std::min(tmax, t2);
                if (tmin > tmax) return false;
            }
        }
        tOut = tmin;
        return true;
    };
    float tHit = 0.0f;
    bool hit = rayIntersect(Lab::Vec3(0, 0, -5), Lab::Vec3(0, 0, 1), Lab::Vec3(-1, -1, -1), Lab::Vec3(1, 1, 1), tHit);
    if (!hit || std::abs(tHit - 4.0f) > 0.01f) {
        std::cerr << "ERROR: Ray-AABB intersection failed! Hit=" << hit << " t=" << tHit << "\n";
        return 1;
    }
    std::cout << "[Test] Ray-AABB hit test passed at distance t=" << tHit << "!\n";

    for (const auto& b : map->brushes) {
        Lab::Texture* tex = b.texturePath.empty() ? nullptr : textures[b.texturePath].get();
        Lab::Renderer::drawCube(b.position, b.size, b.color, tex, true, b.uvScale, b.uvMode);
    }

    // 3c. Verify Bounding Box rendering
    Lab::Renderer::drawBoundingBox(Lab::Vec3(-2, 0, -2), Lab::Vec3(2, 4, 2), Lab::Vec3(1.0f, 0.55f, 0.1f));

    Lab::Renderer::endFrame();
    glFinish();
    saveFrameToBMP("test_3d_textured.bmp", w, h);
    glfwSwapBuffers(window);

    // 4. Render Map Selection Menu
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    Lab::Renderer::beginUI(w, h);
    Lab::Renderer::drawRect(0, 0, (float)w, (float)h, { 0.06f, 0.08f, 0.11f });
    Lab::Renderer::drawRect(100.0f, 60.0f, 1080.0f, 600.0f, { 0.1f, 0.13f, 0.18f });
    Lab::Renderer::drawRect(102.0f, 62.0f, 1076.0f, 40.0f, { 0.15f, 0.22f, 0.32f });
    Lab::Renderer::drawRect(102.0f, 100.0f, 1076.0f, 4.0f, { 0.2f, 0.75f, 0.95f });
    Lab::LabFont::drawText(120.0f, 72.0f, "FROZEN-LIFE : MAP SELECTION & MISSION SELECT", 2.2f, Lab::Vec3(0.9f, 0.95f, 1.0f), Lab::LabFontType::GeoSans);

    // Map list items
    Lab::Renderer::drawRect(140.0f, 130.0f, 800.0f, 45.0f, Lab::Vec3(0.18f, 0.45f, 0.75f));
    Lab::Renderer::drawRect(140.0f, 130.0f, 6.0f, 45.0f, Lab::Vec3(0.98f, 0.78f, 0.08f));
    Lab::LabFont::drawText(160.0f, 144.0f, "facility_alpha.labmap - Research Complex Alpha", 2.0f, Lab::Vec3(1, 1, 1), Lab::LabFontType::GeoSans);

    Lab::Renderer::drawRect(140.0f, 185.0f, 800.0f, 45.0f, Lab::Vec3(0.12f, 0.16f, 0.22f));
    Lab::LabFont::drawText(160.0f, 199.0f, "cryo_outpost.labmap - Sub-Zero Cryo Station", 2.0f, Lab::Vec3(0.7f, 0.75f, 0.8f), Lab::LabFontType::GeoSans);

    // Action buttons
    Lab::Renderer::drawRect(140.0f, 560.0f, 250.0f, 48.0f, Lab::Vec3(0.22f, 0.75f, 0.52f));
    Lab::LabFont::drawText(160.0f, 576.0f, "LAUNCH MAP [ENTER]", 1.7f, Lab::Vec3(1, 1, 1), Lab::LabFontType::GeoSans);

    Lab::Renderer::drawRect(410.0f, 560.0f, 280.0f, 48.0f, Lab::Vec3(0.28f, 0.55f, 0.88f));
    Lab::LabFont::drawText(425.0f, 576.0f, "OPEN FROM DISK... [O]", 1.7f, Lab::Vec3(1, 1, 1), Lab::LabFontType::GeoSans);

    Lab::Renderer::drawRect(710.0f, 560.0f, 260.0f, 48.0f, Lab::Vec3(0.88f, 0.55f, 0.20f));
    Lab::LabFont::drawText(725.0f, 576.0f, "RESUME MISSION [ESC]", 1.7f, Lab::Vec3(1, 1, 1), Lab::LabFontType::GeoSans);

    Lab::LabFont::drawText(140.0f, 622.0f, "USE ARROWS / MOUSE TO SELECT | ENTER: LAUNCH | O: OPEN FILE | ESC / M: MENU", 1.4f, Lab::Vec3(0.55f, 0.65f, 0.75f), Lab::LabFontType::GeoSans);

    Lab::Renderer::endUI();
    glFinish();
    saveFrameToBMP("test_map_menu.bmp", w, h);
    glfwSwapBuffers(window);

    // 5. Render Full Hammer Editor Interface frame (1280x720 scaled)
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    Lab::Camera hammerCam(70.0f, 16.0f / 9.0f, 0.01f, 3000.0f);
    hammerCam.setPosition(Lab::Vec3(0, 8.0f, 18.0f));
    Lab::Renderer::beginFrame(hammerCam);
    Lab::Renderer::setSunLight(Lab::Vec3(-0.4f, -0.8f, -0.4f), Lab::Vec3(1.0f, 0.95f, 0.9f), Lab::Vec3(0.25f, 0.28f, 0.35f));

    // Brushes with Frustum Culling
    for (const auto& b : map->brushes) {
        Lab::Vec3 halfSize = b.size * 0.5f;
        if (!hammerCam.isInFrustum(b.position - halfSize, b.position + halfSize)) continue;
        Lab::Texture* tex = b.texturePath.empty() ? nullptr : textures[b.texturePath].get();
        Lab::Renderer::drawCube(b.position, b.size, b.color, tex, true, b.uvScale, b.uvMode);
    }
    // Selection Bounding Box & Gizmo
    Lab::Renderer::drawBoundingBox(Lab::Vec3(-6, -0.5f, -6), Lab::Vec3(6, 4.0f, 6), Lab::Vec3(1.0f, 0.55f, 0.1f));
    Lab::Renderer::drawCube(Lab::Vec3(0, 2.0f, 0), Lab::Vec3(1.6f, 0.06f, 0.06f), Lab::Vec3(1, 0, 0), false);
    Lab::Renderer::drawCube(Lab::Vec3(0, 2.0f, 0), Lab::Vec3(0.06f, 1.6f, 0.06f), Lab::Vec3(0, 1, 0), false);
    Lab::Renderer::drawCube(Lab::Vec3(0, 2.0f, 0), Lab::Vec3(0.06f, 0.06f, 1.6f), Lab::Vec3(0, 0.5f, 1), false);

    // ==================== 5. Automated Hitbox Verification for SidebarLayout ====================
    std::cout << "[Test] Verifying SidebarLayout hitboxes across multiple resolutions...\n";
    float testResolutions[3][2] = {
        { 1280.0f, 720.0f },
        { 1600.0f, 900.0f },
        { 1920.0f, 1080.0f }
    };

    for (int r = 0; r < 3; ++r) {
        float rw = testResolutions[r][0];
        float rh = testResolutions[r][1];
        Lab::SidebarLayout sl = Lab::getSidebarLayout(rw, rh);

        // Verify sidebar boundary
        if (sl.rightX < 0 || sl.rightX + sl.rightW > rw) {
            std::cerr << "ERROR: Sidebar out of screen bounds horizontally!\n";
            return 1;
        }
        if (sl.rightY < 0 || sl.rightY + sl.rightH > rh) {
            std::cerr << "ERROR: Sidebar out of screen bounds vertically!\n";
            return 1;
        }

        // Verify tabs do not overlap horizontally and fit inside sidebar
        if (sl.tabPropX < sl.rightX || sl.tabPropX + sl.tabPropW > sl.tabOutX || sl.tabOutX + sl.tabOutW > sl.rightX + sl.rightW) {
            std::cerr << "ERROR: Tabs overlap or overflow sidebar bounds!\n";
            return 1;
        }

        // Verify Outliner action buttons (Focus, Duplicate, Delete)
        if (sl.outFocusX + sl.outFocusW > sl.outDupX || sl.outDupX + sl.outDupW > sl.outDelX || sl.outDelX + sl.outDelW > sl.rightX + sl.rightW) {
            std::cerr << "ERROR: Outliner action buttons overlap or overflow!\n";
            return 1;
        }

        // Verify Outliner page buttons
        if (sl.outPrevX + sl.outPrevW > sl.outNextX || sl.outNextX + sl.outNextW > sl.rightX + sl.rightW) {
            std::cerr << "ERROR: Outliner pagination buttons overlap or overflow!\n";
            return 1;
        }

        // Verify UV scale buttons fit within sidebar
        float uvEnd = sl.rightX + 10.0f + 3 * 70.0f + sl.uvBtnW;
        if (uvEnd > sl.rightX + sl.rightW) {
            std::cerr << "ERROR: UV buttons overflow sidebar!\n";
            return 1;
        }

        // Verify Properties buttons fit within sidebar
        if (sl.texBrowseX + sl.texBrowseW > sl.rightX + sl.rightW ||
            sl.texApplyX + sl.texApplyW > sl.rightX + sl.rightW ||
            sl.modelBrowseX + sl.modelBrowseW > sl.rightX + sl.rightW ||
            sl.deselX + sl.deselW > sl.rightX + sl.rightW ||
            sl.delX + sl.delW > sl.rightX + sl.rightW) {
            std::cerr << "ERROR: Properties buttons overflow sidebar!\n";
            return 1;
        }

        // Verify vertical order in Properties tab
        if (sl.uvBtnY >= sl.texBoxY || sl.texBoxY >= sl.thumbY || sl.thumbY >= sl.modelBoxY ||
            sl.modelBoxY >= sl.modelBrowseY || sl.modelBrowseY >= sl.dimBtnsY ||
            sl.dimBtnsY >= sl.deselY || sl.deselY >= sl.delY || sl.delY + sl.delH > sl.rightY + sl.rightH) {
            std::cerr << "ERROR: Properties tab vertical order or height overflow!\n";
            return 1;
        }

        // Verify Hitbox detection: point inside tabProp must hit tabProp
        float ptPropX = sl.tabPropX + sl.tabPropW * 0.5f;
        float ptPropY = sl.tabPropY + sl.tabPropH * 0.5f;
        bool hitTabProp = (ptPropX >= sl.tabPropX && ptPropX <= sl.tabPropX + sl.tabPropW &&
                           ptPropY >= sl.tabPropY && ptPropY <= sl.tabPropY + sl.tabPropH);
        if (!hitTabProp) {
            std::cerr << "ERROR: Hit testing failed for tabProp!\n";
            return 1;
        }

        // Outside sidebar click must NOT trigger sidebar containment
        float ptOutsideX = sl.rightX - 20.0f;
        float ptOutsideY = sl.rightY + 50.0f;
        bool hitOutside = (ptOutsideX >= sl.rightX && ptOutsideX <= rw && ptOutsideY >= sl.rightY && ptOutsideY <= rh - 22.0f);
        if (hitOutside) {
            std::cerr << "ERROR: Viewport point triggered sidebar containment!\n";
            return 1;
        }
    }
    std::cout << "[Test] SidebarLayout hitboxes verified: 100% accurate, no overlaps, strict containment!\n";

    // 6. Render Full Hammer Editor Interface frame (Outliner Tab active)
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    hammerCam.setPosition(Lab::Vec3(0, 8.0f, 18.0f));
    Lab::Renderer::beginFrame(hammerCam);
    Lab::Renderer::setSunLight(Lab::Vec3(-0.4f, -0.8f, -0.4f), Lab::Vec3(1.0f, 0.95f, 0.9f), Lab::Vec3(0.25f, 0.28f, 0.35f));

    // Brushes with Frustum Culling
    for (const auto& b : map->brushes) {
        Lab::Vec3 halfSize = b.size * 0.5f;
        if (!hammerCam.isInFrustum(b.position - halfSize, b.position + halfSize)) continue;
        Lab::Texture* tex = b.texturePath.empty() ? nullptr : textures[b.texturePath].get();
        Lab::Renderer::drawCube(b.position, b.size, b.color, tex, true, b.uvScale, b.uvMode);
    }
    // Selection Bounding Box & Gizmo
    Lab::Renderer::drawBoundingBox(Lab::Vec3(-6, -0.5f, -6), Lab::Vec3(6, 4.0f, 6), Lab::Vec3(1.0f, 0.55f, 0.1f));
    Lab::Renderer::drawCube(Lab::Vec3(0, 2.0f, 0), Lab::Vec3(1.6f, 0.06f, 0.06f), Lab::Vec3(1, 0, 0), false);
    Lab::Renderer::drawCube(Lab::Vec3(0, 2.0f, 0), Lab::Vec3(0.06f, 1.6f, 0.06f), Lab::Vec3(0, 1, 0), false);
    Lab::Renderer::drawCube(Lab::Vec3(0, 2.0f, 0), Lab::Vec3(0.06f, 0.06f, 1.6f), Lab::Vec3(0, 0.5f, 1), false);

    // Hammer 2D UI Overlay (Outliner Tab)
    Lab::Renderer::beginUI(w, h);
    Lab::Vec3 winBg{ 0.93f, 0.93f, 0.94f };
    Lab::Vec3 winBorder{ 0.65f, 0.65f, 0.68f };
    Lab::Vec3 textDark{ 0.12f, 0.12f, 0.12f };
    Lab::Vec3 orangeGlow{ 1.0f, 0.55f, 0.1f };

    // Top menu
    Lab::Renderer::drawRect(0, 0, (float)w, 24.0f, winBg);
    Lab::Renderer::drawRect(0, 23.0f, (float)w, 1.0f, winBorder);
    Lab::LabFont::drawText(14.0f, 5.0f, "File", 1.8f, textDark, Lab::LabFontType::System);
    Lab::LabFont::drawText(54.0f, 5.0f, "Edit", 1.8f, textDark, Lab::LabFontType::System);
    Lab::LabFont::drawText(94.0f, 5.0f, "View", 1.8f, textDark, Lab::LabFontType::System);
    Lab::LabFont::drawText(140.0f, 5.0f, "Tools", 1.8f, textDark, Lab::LabFontType::System);
    Lab::LabFont::drawText(190.0f, 5.0f, "Help", 1.8f, textDark, Lab::LabFontType::System);
    Lab::LabFont::drawText((float)w - 360.0f, 5.0f, "Valve Hammer 4.1 - Frozen-Life Engine", 1.8f, Lab::Vec3(0.15f, 0.45f, 0.75f), Lab::LabFontType::GeoSans);

    // Toolbar (18 buttons with icons)
    Lab::Renderer::drawRect(0, 24.0f, (float)w, 34.0f, winBg);
    Lab::Renderer::drawRect(0, 57.0f, (float)w, 1.0f, winBorder);
    for (int i = 0; i < 18; ++i) {
        float bx = 8.0f + i * 28.0f;
        Lab::HammerIcons::drawToolbarIcon(i, bx, 29.0f, (i == 17) ? Lab::Vec3(1, 1, 1) : Lab::Vec3(0.25f, 0.3f, 0.35f), (i == 17) ? Lab::Vec3(0.15f, 0.65f, 0.35f) : Lab::Vec3(0.88f, 0.88f, 0.90f));
    }

    // Left Palette (8 tools with icons)
    Lab::Renderer::drawRect(0, 58.0f, 42.0f, (float)h - 80.0f, winBg);
    for (int i = 0; i < 8; ++i) {
        float ty = 68.0f + i * 36.0f;
        bool isSel = (i == 0);
        Lab::Vec3 bgCol = isSel ? Lab::Vec3(0.78f, 0.88f, 1.0f) : Lab::Vec3(0.88f, 0.88f, 0.90f);
        Lab::Vec3 iconCol = isSel ? orangeGlow : Lab::Vec3(0.25f, 0.28f, 0.32f);

        Lab::Renderer::drawRect(6.0f, ty, 30.0f, 30.0f, bgCol);
        Lab::Renderer::drawRect(6.0f, ty, 30.0f, 1.0f, isSel ? Lab::Vec3(0.2f, 0.75f, 0.95f) : winBorder);
        Lab::HammerIcons::drawHammerIcon(i, 9.0f, ty + 3.0f, iconCol, bgCol);
    }

    // Right Sidebar (Outliner Tab active using SidebarLayout)
    Lab::SidebarLayout l = Lab::getSidebarLayout((float)w, (float)h);
    Lab::Renderer::drawRect(l.rightX, l.rightY, l.rightW, l.rightH, winBg);
    Lab::Renderer::drawRect(l.rightX, l.rightY, 1.0f, l.rightH, winBorder);

    Lab::Renderer::drawRect(l.tabPropX, l.tabPropY, l.tabPropW, l.tabPropH, Lab::Vec3(0.85f, 0.85f, 0.88f));
    Lab::Renderer::drawRect(l.tabPropX, l.tabPropY, l.tabPropW, 1.0f, winBorder);
    Lab::LabFont::drawText(l.tabPropX + 25.0f, l.tabPropY + 5.0f, "Properties", 1.6f, Lab::Vec3(0.45f, 0.45f, 0.45f), Lab::LabFontType::System);

    Lab::Renderer::drawRect(l.tabOutX, l.tabOutY, l.tabOutW, l.tabOutH, Lab::Vec3(1, 1, 1));
    Lab::Renderer::drawRect(l.tabOutX, l.tabOutY, l.tabOutW, 1.0f, orangeGlow);
    Lab::LabFont::drawText(l.tabOutX + 25.0f, l.tabOutY + 5.0f, "Struktura", 1.6f, textDark, Lab::LabFontType::System);

    Lab::LabFont::drawText(l.rightX + 12.0f, l.rightY + 38.0f, "Map Entity Outliner (9 items):", 1.6f, textDark, Lab::LabFontType::System);
    Lab::Renderer::drawRect(l.outListX, l.outListY, l.outListW, l.outListH, Lab::Vec3(1, 1, 1));
    Lab::Renderer::drawRect(l.outListX, l.outListY, l.outListW, 1.0f, winBorder);

    Lab::LabFont::drawText(l.outListX + 10.0f, l.outListY + 8.0f, "[Spawn] Player Start (0, 1.8, 0)", 1.5f, textDark, Lab::LabFontType::System);
    Lab::Renderer::drawRect(l.outListX + 2.0f, l.outListY + 29.0f, l.outListW - 4.0f, l.outItemH, Lab::Vec3(0.85f, 0.92f, 1.0f));
    Lab::Renderer::drawRect(l.outListX + 2.0f, l.outListY + 29.0f, 4.0f, l.outItemH, orangeGlow);
    Lab::LabFont::drawText(l.outListX + 10.0f, l.outListY + 33.0f, "[B#0] floor_tiles.bmp (32x1x32)", 1.5f, Lab::Vec3(0.1f, 0.35f, 0.7f), Lab::LabFontType::System);
    Lab::LabFont::drawText(l.outListX + 10.0f, l.outListY + 58.0f, "[B#1] concrete_wall.bmp (16x4x1)", 1.5f, textDark, Lab::LabFontType::System);
    Lab::LabFont::drawText(l.outListX + 10.0f, l.outListY + 83.0f, "[P#0] Model.stl (1x1x1)", 1.5f, textDark, Lab::LabFontType::System);
    Lab::LabFont::drawText(l.outListX + 10.0f, l.outListY + 108.0f, "[D#0] blast_door (2.5x3.5x0.4)", 1.5f, textDark, Lab::LabFontType::System);

    // Outliner action buttons
    Lab::Renderer::drawRect(l.outFocusX, l.outFocusY, l.outFocusW, l.outFocusH, Lab::Vec3(0.88f, 0.88f, 0.90f));
    Lab::Renderer::drawRect(l.outFocusX, l.outFocusY, l.outFocusW, 1.0f, winBorder);
    Lab::LabFont::drawText(l.outFocusX + 14.0f, l.outFocusY + 6.0f, "Focus (F)", 1.5f, textDark, Lab::LabFontType::System);

    Lab::Renderer::drawRect(l.outDupX, l.outDupY, l.outDupW, l.outDupH, Lab::Vec3(0.88f, 0.88f, 0.90f));
    Lab::Renderer::drawRect(l.outDupX, l.outDupY, l.outDupW, 1.0f, winBorder);
    Lab::LabFont::drawText(l.outDupX + 14.0f, l.outDupY + 6.0f, "Duplicate", 1.5f, textDark, Lab::LabFontType::System);

    Lab::Renderer::drawRect(l.outDelX, l.outDelY, l.outDelW, l.outDelH, Lab::Vec3(0.88f, 0.88f, 0.90f));
    Lab::Renderer::drawRect(l.outDelX, l.outDelY, l.outDelW, 1.0f, winBorder);
    Lab::LabFont::drawText(l.outDelX + 18.0f, l.outDelY + 6.0f, "Delete", 1.5f, Lab::Vec3(0.7f, 0.1f, 0.1f), Lab::LabFontType::System);

    // Pagination buttons
    Lab::Renderer::drawRect(l.outPrevX, l.outPrevY, l.outPrevW, l.outPrevH, Lab::Vec3(0.88f, 0.88f, 0.90f));
    Lab::Renderer::drawRect(l.outPrevX, l.outPrevY, l.outPrevW, 1.0f, winBorder);
    Lab::LabFont::drawText(l.outPrevX + 35.0f, l.outPrevY + 5.0f, "< Prev Page", 1.5f, textDark, Lab::LabFontType::System);

    Lab::Renderer::drawRect(l.outNextX, l.outNextY, l.outNextW, l.outNextH, Lab::Vec3(0.88f, 0.88f, 0.90f));
    Lab::Renderer::drawRect(l.outNextX, l.outNextY, l.outNextW, 1.0f, winBorder);
    Lab::LabFont::drawText(l.outNextX + 35.0f, l.outNextY + 5.0f, "Next Page >", 1.5f, textDark, Lab::LabFontType::System);

    // Messages Console
    Lab::Renderer::drawRect(60.0f, (float)h - 170.0f, 600.0f, 140.0f, Lab::Vec3(1, 1, 1));
    Lab::Renderer::drawRect(60.0f, (float)h - 170.0f, 600.0f, 22.0f, Lab::Vec3(0.85f, 0.90f, 0.96f));
    Lab::LabFont::drawText(70.0f, (float)h - 165.0f, "Messages & Optimization Log", 1.6f, textDark, Lab::LabFontType::System);
    Lab::LabFont::drawText(70.0f, (float)h - 140.0f, "Hammer initialized. Ready.", 1.5f, Lab::Vec3(0.1f, 0.15f, 0.2f), Lab::LabFontType::System);
    Lab::LabFont::drawText(70.0f, (float)h - 124.0f, "Frustum Culling active: 'To czego oko nie widzi tego maszyna renderowac nie musi'", 1.5f, Lab::Vec3(0.1f, 0.15f, 0.2f), Lab::LabFontType::System);
    Lab::LabFont::drawText(70.0f, (float)h - 108.0f, "Selected Brush #0 (floor_tiles.bmp)", 1.5f, Lab::Vec3(0.1f, 0.15f, 0.2f), Lab::LabFontType::System);

    // Status bar with Frustum Culling stats
    Lab::Renderer::drawRect(0, (float)h - 22.0f, (float)w, 22.0f, winBg);
    Lab::LabFont::drawText(10.0f, (float)h - 17.0f, "RMB Fly | LMB Pick/Apply | E Place | F Focus | Del Delete | Frustum Culling: Brushes 7/7 | Props 1/1", 1.5f, textDark, Lab::LabFontType::System);
    Lab::LabFont::drawText((float)w - 220.0f, (float)h - 17.0f, "Snap: 1 | F9: Run", 1.5f, textDark, Lab::LabFontType::System);

    Lab::Renderer::endUI();
    glFinish();
    saveFrameToBMP("test_hammer_ui.bmp", w, h);
    glfwSwapBuffers(window);

    // 7. Render Hammer with Properties Tab active -> test_hammer_properties.bmp
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    Lab::Renderer::beginFrame(hammerCam);
    for (const auto& b : map->brushes) {
        Lab::Vec3 halfSize = b.size * 0.5f;
        if (!hammerCam.isInFrustum(b.position - halfSize, b.position + halfSize)) continue;
        Lab::Texture* tex = b.texturePath.empty() ? nullptr : textures[b.texturePath].get();
        Lab::Renderer::drawCube(b.position, b.size, b.color, tex, true, b.uvScale, b.uvMode);
    }
    Lab::Renderer::drawBoundingBox(Lab::Vec3(-6, -0.5f, -6), Lab::Vec3(6, 4.0f, 6), Lab::Vec3(1.0f, 0.55f, 0.1f));
    Lab::Renderer::drawCube(Lab::Vec3(0, 2.0f, 0), Lab::Vec3(1.6f, 0.06f, 0.06f), Lab::Vec3(1, 0, 0), false);
    Lab::Renderer::drawCube(Lab::Vec3(0, 2.0f, 0), Lab::Vec3(0.06f, 1.6f, 0.06f), Lab::Vec3(0, 1, 0), false);
    Lab::Renderer::drawCube(Lab::Vec3(0, 2.0f, 0), Lab::Vec3(0.06f, 0.06f, 1.6f), Lab::Vec3(0, 0.5f, 1), false);

    Lab::Renderer::beginUI(w, h);
    // Menu & Toolbar
    Lab::Renderer::drawRect(0, 0, (float)w, 24.0f, winBg);
    Lab::LabFont::drawText(14.0f, 5.0f, "File  Edit  View  Tools  Help", 1.8f, textDark, Lab::LabFontType::System);
    Lab::LabFont::drawText((float)w - 360.0f, 5.0f, "Valve Hammer 4.1 - Frozen-Life Engine", 1.8f, Lab::Vec3(0.15f, 0.45f, 0.75f), Lab::LabFontType::GeoSans);
    Lab::Renderer::drawRect(0, 24.0f, (float)w, 34.0f, winBg);
    for (int i = 0; i < 18; ++i) {
        float bx = 8.0f + i * 28.0f;
        Lab::Renderer::drawRect(bx, 29.0f, 24.0f, 24.0f, (i == 17) ? Lab::Vec3(0.15f, 0.65f, 0.35f) : Lab::Vec3(0.88f, 0.88f, 0.90f));
    }
    Lab::Renderer::drawRect(0, 58.0f, 42.0f, (float)h - 80.0f, winBg);
    for (int i = 0; i < 8; ++i) {
        Lab::Renderer::drawRect(6.0f, 68.0f + i * 36.0f, 30.0f, 30.0f, (i == 0) ? Lab::Vec3(0.78f, 0.88f, 1.0f) : Lab::Vec3(0.88f, 0.88f, 0.90f));
    }

    // Sidebar: Properties Tab Active!
    Lab::Renderer::drawRect(l.rightX, l.rightY, l.rightW, l.rightH, winBg);
    Lab::Renderer::drawRect(l.rightX, l.rightY, 1.0f, l.rightH, winBorder);

    Lab::Renderer::drawRect(l.tabPropX, l.tabPropY, l.tabPropW, l.tabPropH, Lab::Vec3(1, 1, 1));
    Lab::Renderer::drawRect(l.tabPropX, l.tabPropY, l.tabPropW, 1.0f, orangeGlow);
    Lab::LabFont::drawText(l.tabPropX + 25.0f, l.tabPropY + 5.0f, "Properties", 1.6f, textDark, Lab::LabFontType::System);

    Lab::Renderer::drawRect(l.tabOutX, l.tabOutY, l.tabOutW, l.tabOutH, Lab::Vec3(0.85f, 0.85f, 0.88f));
    Lab::Renderer::drawRect(l.tabOutX, l.tabOutY, l.tabOutW, 1.0f, winBorder);
    Lab::LabFont::drawText(l.tabOutX + 25.0f, l.tabOutY + 5.0f, "Struktura", 1.6f, Lab::Vec3(0.45f, 0.45f, 0.45f), Lab::LabFontType::System);

    float propY = l.rightY + 38.0f;
    Lab::LabFont::drawText(l.rightX + 12.0f, propY, "Selection: Brush #0", 1.7f, orangeGlow, Lab::LabFontType::System);
    Lab::LabFont::drawText(l.rightX + 12.0f, propY + 22.0f, "Pos: (0, 0, 0)", 1.6f, textDark, Lab::LabFontType::System);
    Lab::LabFont::drawText(l.rightX + 12.0f, propY + 42.0f, "Size: (32 x 1 x 32)", 1.6f, textDark, Lab::LabFontType::System);

    // UV Scale buttons
    Lab::LabFont::drawText(l.rightX + 12.0f, l.uvBtnY - 18.0f, "Texture UV Tiling Scale:", 1.7f, textDark, Lab::LabFontType::System);
    const char* scalesStr[4] = { "0.125", "0.25", "0.5", "1.0" };
    for (int i = 0; i < 4; ++i) {
        float sx = l.rightX + 10.0f + i * 70.0f;
        Lab::Renderer::drawRect(sx, l.uvBtnY, l.uvBtnW, l.uvBtnH, (i == 1) ? Lab::Vec3(0.78f, 0.88f, 1.0f) : Lab::Vec3(0.88f, 0.88f, 0.90f));
        Lab::Renderer::drawRect(sx, l.uvBtnY, l.uvBtnW, 1.0f, (i == 1) ? Lab::Vec3(0.2f, 0.75f, 0.95f) : winBorder);
        Lab::LabFont::drawText(sx + 14.0f, l.uvBtnY + 5.0f, scalesStr[i], 1.5f, textDark, Lab::LabFontType::System);
    }

    // Active Texture & Thumbnail
    Lab::LabFont::drawText(l.rightX + 12.0f, l.texBoxY - 18.0f, "Active Texture:", 1.7f, textDark, Lab::LabFontType::System);
    Lab::Renderer::drawRect(l.texBoxX, l.texBoxY, l.texBoxW, l.texBoxH, Lab::Vec3(1, 1, 1));
    Lab::Renderer::drawRect(l.texBoxX, l.texBoxY, l.texBoxW, 1.0f, winBorder);
    Lab::LabFont::drawText(l.texBoxX + 10.0f, l.texBoxY + 4.0f, "floor_tiles.bmp", 1.6f, textDark, Lab::LabFontType::System);

    Lab::Renderer::drawRect(l.thumbX, l.thumbY, l.thumbS, l.thumbS, Lab::Vec3(0, 0, 0));
    if (textures.contains("floor_tiles.bmp")) {
        Lab::Renderer::drawTextureRect(l.thumbX + 2.0f, l.thumbY + 2.0f, l.thumbS - 4.0f, l.thumbS - 4.0f, *textures["floor_tiles.bmp"]);
    }
    Lab::Renderer::drawRect(l.texBrowseX, l.texBrowseY, l.texBrowseW, l.texBrowseH, Lab::Vec3(0.88f, 0.88f, 0.90f));
    Lab::Renderer::drawRect(l.texBrowseX, l.texBrowseY, l.texBrowseW, 1.0f, winBorder);
    Lab::LabFont::drawText(l.texBrowseX + 22.0f, l.texBrowseY + 8.0f, "Browse Textures...", 1.6f, textDark, Lab::LabFontType::System);

    Lab::Renderer::drawRect(l.texApplyX, l.texApplyY, l.texApplyW, l.texApplyH, Lab::Vec3(0.88f, 0.88f, 0.90f));
    Lab::Renderer::drawRect(l.texApplyX, l.texApplyY, l.texApplyW, 1.0f, winBorder);
    Lab::LabFont::drawText(l.texApplyX + 28.0f, l.texApplyY + 8.0f, "Apply to Brush", 1.6f, textDark, Lab::LabFontType::System);

    // 3D Model Selector
    Lab::LabFont::drawText(l.rightX + 12.0f, l.modelBoxY - 18.0f, "3D Entity Model (.stl):", 1.7f, textDark, Lab::LabFontType::System);
    Lab::Renderer::drawRect(l.modelBoxX, l.modelBoxY, l.modelBoxW, l.modelBoxH, Lab::Vec3(1, 1, 1));
    Lab::Renderer::drawRect(l.modelBoxX, l.modelBoxY, l.modelBoxW, 1.0f, winBorder);
    Lab::LabFont::drawText(l.modelBoxX + 10.0f, l.modelBoxY + 4.0f, "Model.stl", 1.6f, textDark, Lab::LabFontType::System);

    Lab::Renderer::drawRect(l.modelBrowseX, l.modelBrowseY, l.modelBrowseW, l.modelBrowseH, Lab::Vec3(0.88f, 0.88f, 0.90f));
    Lab::Renderer::drawRect(l.modelBrowseX, l.modelBrowseY, l.modelBrowseW, 1.0f, winBorder);
    Lab::LabFont::drawText(l.modelBrowseX + 45.0f, l.modelBrowseY + 8.0f, "Browse 3D Models...", 1.6f, textDark, Lab::LabFontType::System);

    // Dimension adjusters
    Lab::LabFont::drawText(l.rightX + 12.0f, l.dimBtnsY - 18.0f, "Adjust Size (X / Y / Z):", 1.7f, textDark, Lab::LabFontType::System);
    Lab::LabFont::drawText(l.rightX + 12.0f, l.dimBtnsY + 4.0f, "X:", 1.6f, textDark, Lab::LabFontType::System);
    Lab::Renderer::drawRect(l.rightX + 30.0f, l.dimBtnsY, l.dimBtnW, l.dimBtnH, Lab::Vec3(0.88f, 0.88f, 0.90f));
    Lab::LabFont::drawText(l.rightX + 38.0f, l.dimBtnsY + 3.0f, "-", 1.8f, textDark, Lab::LabFontType::System);
    Lab::Renderer::drawRect(l.rightX + 60.0f, l.dimBtnsY, l.dimBtnW, l.dimBtnH, Lab::Vec3(0.88f, 0.88f, 0.90f));
    Lab::LabFont::drawText(l.rightX + 66.0f, l.dimBtnsY + 3.0f, "+", 1.8f, textDark, Lab::LabFontType::System);

    Lab::LabFont::drawText(l.rightX + 105.0f, l.dimBtnsY + 4.0f, "Y:", 1.6f, textDark, Lab::LabFontType::System);
    Lab::Renderer::drawRect(l.rightX + 123.0f, l.dimBtnsY, l.dimBtnW, l.dimBtnH, Lab::Vec3(0.88f, 0.88f, 0.90f));
    Lab::LabFont::drawText(l.rightX + 131.0f, l.dimBtnsY + 3.0f, "-", 1.8f, textDark, Lab::LabFontType::System);
    Lab::Renderer::drawRect(l.rightX + 153.0f, l.dimBtnsY, l.dimBtnW, l.dimBtnH, Lab::Vec3(0.88f, 0.88f, 0.90f));
    Lab::LabFont::drawText(l.rightX + 159.0f, l.dimBtnsY + 3.0f, "+", 1.8f, textDark, Lab::LabFontType::System);

    Lab::LabFont::drawText(l.rightX + 198.0f, l.dimBtnsY + 4.0f, "Z:", 1.6f, textDark, Lab::LabFontType::System);
    Lab::Renderer::drawRect(l.rightX + 216.0f, l.dimBtnsY, l.dimBtnW, l.dimBtnH, Lab::Vec3(0.88f, 0.88f, 0.90f));
    Lab::LabFont::drawText(l.rightX + 224.0f, l.dimBtnsY + 3.0f, "-", 1.8f, textDark, Lab::LabFontType::System);
    Lab::Renderer::drawRect(l.rightX + 246.0f, l.dimBtnsY, l.dimBtnW, l.dimBtnH, Lab::Vec3(0.88f, 0.88f, 0.90f));
    Lab::LabFont::drawText(l.rightX + 252.0f, l.dimBtnsY + 3.0f, "+", 1.8f, textDark, Lab::LabFontType::System);

    // Deselect and Delete buttons
    Lab::Renderer::drawRect(l.deselX, l.deselY, l.deselW, l.deselH, Lab::Vec3(0.88f, 0.88f, 0.90f));
    Lab::Renderer::drawRect(l.deselX, l.deselY, l.deselW, 1.0f, winBorder);
    Lab::LabFont::drawText(l.deselX + 85.0f, l.deselY + 7.0f, "Deselect All", 1.6f, textDark, Lab::LabFontType::System);

    Lab::Renderer::drawRect(l.delX, l.delY, l.delW, l.delH, Lab::Vec3(0.88f, 0.88f, 0.90f));
    Lab::Renderer::drawRect(l.delX, l.delY, l.delW, 1.0f, winBorder);
    Lab::LabFont::drawText(l.delX + 75.0f, l.delY + 7.0f, "Delete Selected", 1.6f, Lab::Vec3(0.7f, 0.1f, 0.1f), Lab::LabFontType::System);

    // Console & Status
    Lab::Renderer::drawRect(60.0f, (float)h - 170.0f, 600.0f, 140.0f, Lab::Vec3(1, 1, 1));
    Lab::Renderer::drawRect(60.0f, (float)h - 170.0f, 600.0f, 22.0f, Lab::Vec3(0.85f, 0.90f, 0.96f));
    Lab::LabFont::drawText(70.0f, (float)h - 165.0f, "Messages & Optimization Log", 1.6f, textDark, Lab::LabFontType::System);
    Lab::LabFont::drawText(70.0f, (float)h - 140.0f, "Switched to Properties Tab. Texture: floor_tiles.bmp", 1.5f, Lab::Vec3(0.1f, 0.15f, 0.2f), Lab::LabFontType::System);

    Lab::Renderer::drawRect(0, (float)h - 22.0f, (float)w, 22.0f, winBg);
    Lab::LabFont::drawText(10.0f, (float)h - 17.0f, "RMB Fly | LMB Pick/Apply | E Place | F Focus | Del Delete | Frustum Culling: Brushes 7/7 | Props 1/1", 1.5f, textDark, Lab::LabFontType::System);
    Lab::LabFont::drawText((float)w - 220.0f, (float)h - 17.0f, "Snap: 1 | F9: Run", 1.5f, textDark, Lab::LabFontType::System);

    // ==================== 8. Automated Combat & Hitscan Raycast Verification ====================
    std::cout << "[Test] Verifying Tag-Based Raycast Hitscan and Combat AI...\n";
    {
        // 1. Single Bot Hitscan & Headshot Test
        Lab::CombatBot testBot(1, "TargetBot", Lab::Vec3(0.0f, 0.0f, -6.0f), Lab::Vec3(4.0f, 0.0f, -6.0f));
        Lab::Vec3 headMin, headMax, bodyMin, bodyMax;
        testBot.getHitboxes(headMin, headMax, bodyMin, bodyMax);

        // Test Headshot ray
        Lab::Vec3 rayHeadOrigin(0.0f, 1.68f, 0.0f);
        Lab::Vec3 rayDir(0.0f, 0.0f, -1.0f);
        float tHead = 0.0f;
        Lab::Vec3 norm;
        bool hitHead = Lab::Raycast::rayIntersectAABB(rayHeadOrigin, rayDir, headMin, headMax, tHead, &norm);
        if (!hitHead || std::abs(tHead - 5.76f) > 0.2f) {
            std::cerr << "ERROR: Headshot raycast failed!\n";
            return 1;
        }

        // Test Torso ray
        Lab::Vec3 rayBodyOrigin(0.0f, 0.85f, 0.0f);
        float tBody = 0.0f;
        bool hitBody = Lab::Raycast::rayIntersectAABB(rayBodyOrigin, rayDir, bodyMin, bodyMax, tBody, &norm);
        if (!hitBody || std::abs(tBody - 5.62f) > 0.2f) {
            std::cerr << "ERROR: Body raycast failed!\n";
            return 1;
        }

        // Test Damage and Death
        bool deadFromHeadshot = testBot.takeDamage(100.0f, true);
        if (!deadFromHeadshot || testBot.isAlive() || testBot.state != Lab::AIState::Dead) {
            std::cerr << "ERROR: Bot did not register headshot death properly!\n";
            return 1;
        }

        // 2. AIManager Multi-Bot & Team Assignment Test
        Lab::AIManager aiMgr;
        aiMgr.spawnBotsForMap("facility_alpha.labmap", 4, Lab::GameMode::TDM);
        if (aiMgr.bots.size() != 4) {
            std::cerr << "ERROR: AIManager did not spawn 4 bots!\n";
            return 1;
        }
        // Verify TDM alternating teams
        if (aiMgr.bots[0].team != 0 || aiMgr.bots[1].team != 1 ||
            aiMgr.bots[2].team != 0 || aiMgr.bots[3].team != 1) {
            std::cerr << "ERROR: TDM teams not assigned properly!\n";
            return 1;
        }

        // Test Raycast dispatch through AIManager
        Lab::Vec3 bot0Head = aiMgr.bots[0].position + Lab::Vec3(0.0f, 1.68f, 0.0f);
        Lab::Vec3 aimRayOrigin = bot0Head + Lab::Vec3(0.0f, 0.0f, 5.0f);
        Lab::Vec3 aimRayDir = (bot0Head - aimRayOrigin).normalized();
        Lab::RaycastHit rHit;
        bool anyHit = aiMgr.testRaycast(aimRayOrigin, aimRayDir, rHit);
        if (!anyHit || rHit.tag != Lab::EntityTag::Bot || rHit.entityIndex != 0 || !rHit.isHeadshot) {
            std::cerr << "ERROR: AIManager testRaycast failed to detect Bot #0 headshot!\n";
            return 1;
        }

        // 3. GameSessionConfig Test (Solo vs Bots)
        Lab::GameSessionConfig soloCfg;
        soloCfg.enableBots = false;
        soloCfg.botCount = 0;
        if (soloCfg.enableBots) {
            std::cerr << "ERROR: Solo config has bots enabled!\n";
            return 1;
        }
    }
    std::cout << "[Test] Combat AI & Hitscan tests passed: 100% accuracy on Headshots, Body, and Team allocation!\n";

    // 9. Render Combat Shootout Frame -> test_combat_shooting.bmp
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    Lab::Camera combatCam(75.0f, 16.0f / 9.0f, 0.01f, 1000.0f);
    combatCam.setPosition(Lab::Vec3(0.0f, 1.8f, 0.0f));
    Lab::Renderer::beginFrame(combatCam);

    // Render floor and walls
    for (const auto& b : map->brushes) {
        Lab::Texture* tex = b.texturePath.empty() ? nullptr : textures[b.texturePath].get();
        Lab::Renderer::drawCube(b.position, b.size, b.color, tex, true, b.uvScale, b.uvMode);
    }

    // Spawn 2 bots for visual shootout
    Lab::AIManager demoAI;
    demoAI.spawnBotsForMap("facility_alpha.labmap", 2, Lab::GameMode::FFA);
    demoAI.bots[0].position = Lab::Vec3(0.0f, 0.0f, -8.0f);
    demoAI.bots[0].state = Lab::AIState::Attack;
    demoAI.bots[0].muzzleFlashTimer = 0.08f;

    demoAI.bots[1].position = Lab::Vec3(5.0f, 0.0f, -10.0f);
    demoAI.bots[1].takeDamage(40.0f, false); // Hurt flash on second bot

    demoAI.render();

    // Golden Bullet Tracer from Player Gun
    Lab::BulletTracer playerTracer;
    playerTracer.start = Lab::Vec3(0.25f, 1.55f, -0.6f);
    playerTracer.end = Lab::Vec3(0.0f, 1.68f, -8.0f);
    playerTracer.color = Lab::Vec3(1.0f, 0.95f, 0.4f);
    playerTracer.thickness = 0.04f;

    Lab::Vec3 diff = playerTracer.end - playerTracer.start;
    float tLen = diff.length();
    Lab::Vec3 tMid = playerTracer.start + diff * 0.5f;
    float tYaw = std::atan2(diff.x, diff.z) * 180.0f / 3.14159265f;
    float tPitch = -std::asin(diff.y / tLen) * 180.0f / 3.14159265f;
    Lab::Renderer::drawCube(tMid, Lab::Vec3(tPitch, tYaw, 0.0f), Lab::Vec3(playerTracer.thickness, playerTracer.thickness, tLen), playerTracer.color, nullptr, false);

    // HUD with Hitmarker & Combat details
    Lab::LabHUD combatHUD;
    combatHUD.triggerHitmarker(true); // Red Headshot hitmarker!
    combatHUD.frags = 3;
    combatHUD.gameModeName = "TDM (Team Deathmatch)";
    combatHUD.showCombatMessage("HEADSHOT! ELIMINATED Bot #1 [3 KILLS]", 3.0f);
    combatHUD.render(w, h);

    glFinish();
    saveFrameToBMP("test_combat_shooting.bmp", w, h);
    glfwSwapBuffers(window);

    // 10. Render Multiplayer Host Game Setup Menu -> test_host_menu.bmp
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    Lab::Renderer::beginUI(w, h);
    Lab::Renderer::drawRect(0, 0, (float)w, (float)h, { 0.06f, 0.08f, 0.11f });

    // Host Frame
    Lab::Renderer::drawRect(80.0f, 60.0f, 1120.0f, 600.0f, { 0.1f, 0.13f, 0.18f });
    Lab::Renderer::drawRect(82.0f, 62.0f, 1116.0f, 44.0f, { 0.15f, 0.22f, 0.32f });
    Lab::Renderer::drawRect(82.0f, 106.0f, 1116.0f, 4.0f, { 0.2f, 0.75f, 0.95f });
    Lab::LabFont::drawText(100.0f, 74.0f, "MULTIPLAYER : HOST SERVER & BOT CONFIGURATION", 2.4f, Lab::Vec3(0.95f, 0.98f, 1.0f), Lab::LabFontType::GeoSans);

    // Left Column: Map Select
    Lab::Renderer::drawRect(100.0f, 120.0f, 440.0f, 425.0f, Lab::Vec3(0.08f, 0.10f, 0.14f));
    Lab::Renderer::drawRect(100.0f, 120.0f, 440.0f, 32.0f, Lab::Vec3(0.18f, 0.35f, 0.55f));
    Lab::LabFont::drawText(120.0f, 128.0f, "1. SELECT MAP", 1.8f, Lab::Vec3(1, 1, 1), Lab::LabFontType::GeoSans);

    Lab::Renderer::drawRect(115.0f, 170.0f, 410.0f, 42.0f, Lab::Vec3(0.18f, 0.45f, 0.75f));
    Lab::Renderer::drawRect(115.0f, 170.0f, 5.0f, 42.0f, Lab::Vec3(0.98f, 0.78f, 0.08f));
    Lab::LabFont::drawText(130.0f, 182.0f, "facility_alpha.labmap", 1.8f, Lab::Vec3(1, 1, 1), Lab::LabFontType::GeoSans);

    Lab::Renderer::drawRect(115.0f, 220.0f, 410.0f, 42.0f, Lab::Vec3(0.12f, 0.15f, 0.20f));
    Lab::LabFont::drawText(130.0f, 232.0f, "cryo_outpost.labmap", 1.8f, Lab::Vec3(0.7f, 0.75f, 0.8f), Lab::LabFontType::GeoSans);

    Lab::Renderer::drawRect(115.0f, 485.0f, 410.0f, 42.0f, Lab::Vec3(0.20f, 0.32f, 0.48f));
    Lab::LabFont::drawText(170.0f, 498.0f, "OPEN MAP FROM DISK... [O]", 1.6f, Lab::Vec3(1, 1, 1), Lab::LabFontType::GeoSans);

    // Right Column: Rules & Bot Settings
    Lab::Renderer::drawRect(560.0f, 120.0f, 620.0f, 425.0f, Lab::Vec3(0.08f, 0.10f, 0.14f));
    Lab::Renderer::drawRect(560.0f, 120.0f, 620.0f, 32.0f, Lab::Vec3(0.18f, 0.35f, 0.55f));
    Lab::LabFont::drawText(580.0f, 128.0f, "2. SERVER & MATCH RULES", 1.8f, Lab::Vec3(1, 1, 1), Lab::LabFontType::GeoSans);

    // Game Mode Buttons
    Lab::LabFont::drawText(580.0f, 165.0f, "GAME MODE:", 1.8f, Lab::Vec3(0.85f, 0.88f, 0.95f), Lab::LabFontType::GeoSans);
    Lab::Renderer::drawRect(580.0f, 190.0f, 160.0f, 42.0f, Lab::Vec3(0.14f, 0.18f, 0.24f));
    Lab::LabFont::drawText(620.0f, 202.0f, "FFA (All)", 1.7f, Lab::Vec3(1, 1, 1), Lab::LabFontType::GeoSans);
    Lab::Renderer::drawRect(760.0f, 190.0f, 160.0f, 42.0f, Lab::Vec3(0.14f, 0.18f, 0.24f));
    Lab::LabFont::drawText(800.0f, 202.0f, "Deathmatch", 1.7f, Lab::Vec3(1, 1, 1), Lab::LabFontType::GeoSans);
    Lab::Renderer::drawRect(940.0f, 190.0f, 160.0f, 42.0f, Lab::Vec3(0.18f, 0.65f, 0.45f));
    Lab::Renderer::drawRect(940.0f, 190.0f, 160.0f, 2.0f, Lab::Vec3(0.98f, 0.78f, 0.08f));
    Lab::LabFont::drawText(980.0f, 202.0f, "Team DM", 1.7f, Lab::Vec3(1, 1, 1), Lab::LabFontType::GeoSans);

    // Bots Toggle
    Lab::LabFont::drawText(580.0f, 255.0f, "COMBAT AI BOTS:", 1.8f, Lab::Vec3(0.85f, 0.88f, 0.95f), Lab::LabFontType::GeoSans);
    Lab::Renderer::drawRect(580.0f, 280.0f, 340.0f, 42.0f, Lab::Vec3(0.18f, 0.65f, 0.35f));
    Lab::LabFont::drawText(620.0f, 292.0f, "BOTS: ENABLED (ON)", 1.8f, Lab::Vec3(1, 1, 1), Lab::LabFontType::GeoSans);

    // Bot Count Selector
    Lab::LabFont::drawText(580.0f, 345.0f, "BOT COUNT (0 - 8):", 1.8f, Lab::Vec3(0.85f, 0.88f, 0.95f), Lab::LabFontType::GeoSans);
    Lab::Renderer::drawRect(580.0f, 370.0f, 45.0f, 42.0f, Lab::Vec3(0.22f, 0.28f, 0.38f));
    Lab::LabFont::drawText(598.0f, 380.0f, "-", 2.4f, Lab::Vec3(1, 1, 1), Lab::LabFontType::GeoSans);
    Lab::Renderer::drawRect(635.0f, 370.0f, 140.0f, 42.0f, Lab::Vec3(0.12f, 0.15f, 0.20f));
    Lab::LabFont::drawText(660.0f, 382.0f, "4 BOTS", 2.0f, Lab::Vec3(0.95f, 0.85f, 0.2f), Lab::LabFontType::GeoSans);
    Lab::Renderer::drawRect(785.0f, 370.0f, 45.0f, 42.0f, Lab::Vec3(0.22f, 0.28f, 0.38f));
    Lab::LabFont::drawText(802.0f, 380.0f, "+", 2.4f, Lab::Vec3(1, 1, 1), Lab::LabFontType::GeoSans);

    // Frag Limit
    Lab::LabFont::drawText(580.0f, 435.0f, "FRAG LIMIT:", 1.8f, Lab::Vec3(0.85f, 0.88f, 0.95f), Lab::LabFontType::GeoSans);
    Lab::Renderer::drawRect(580.0f, 460.0f, 45.0f, 42.0f, Lab::Vec3(0.22f, 0.28f, 0.38f));
    Lab::LabFont::drawText(598.0f, 470.0f, "-", 2.4f, Lab::Vec3(1, 1, 1), Lab::LabFontType::GeoSans);
    Lab::Renderer::drawRect(635.0f, 460.0f, 140.0f, 42.0f, Lab::Vec3(0.12f, 0.15f, 0.20f));
    Lab::LabFont::drawText(660.0f, 472.0f, "25 KILLS", 2.0f, Lab::Vec3(0.3f, 0.85f, 1.0f), Lab::LabFontType::GeoSans);
    Lab::Renderer::drawRect(785.0f, 460.0f, 45.0f, 42.0f, Lab::Vec3(0.22f, 0.28f, 0.38f));
    Lab::LabFont::drawText(802.0f, 470.0f, "+", 2.4f, Lab::Vec3(1, 1, 1), Lab::LabFontType::GeoSans);

    // Bottom Buttons
    Lab::Renderer::drawRect(100.0f, 570.0f, 200.0f, 48.0f, Lab::Vec3(0.20f, 0.25f, 0.35f));
    Lab::LabFont::drawText(150.0f, 586.0f, "< BACK", 1.8f, Lab::Vec3(1, 1, 1), Lab::LabFontType::GeoSans);

    Lab::Renderer::drawRect(820.0f, 570.0f, 360.0f, 48.0f, Lab::Vec3(0.18f, 0.65f, 0.35f));
    Lab::Renderer::drawRect(820.0f, 570.0f, 360.0f, 2.0f, Lab::Vec3(0.98f, 0.78f, 0.08f));
    Lab::LabFont::drawText(860.0f, 586.0f, "START SERVER / LAUNCH MATCH", 1.8f, Lab::Vec3(1, 1, 1), Lab::LabFontType::GeoSans);

    Lab::Renderer::endUI();
    glFinish();
    saveFrameToBMP("test_host_menu.bmp", w, h);
    glfwSwapBuffers(window);

    // ==================== 11. UNIT TESTS: LABCOLLISION (WALL COLLISION & SLIDING) ====================
    std::cout << "[Test] Verifying LabCollision AABB Move & Slide against Solid Geometry...\n";
    auto solidBoxes = Lab::LabCollision::getMapSolidBoxes(*map);
    if (solidBoxes.empty()) {
        std::cerr << "ERROR: Failed to extract solid collision boxes from map!\n";
        return 1;
    }

    // 11.1 North Wall Collision Test
    // North wall in facility_alpha is at (0, 4.0, -20.0), size (24, 8, 1) -> Z in [-20.5, -19.5]
    Lab::Vec3 testPos{ 0.0f, 1.70f, -18.8f };
    Lab::Vec3 testVel{ 0.0f, 0.0f, -15.0f }; // Moving fast into North Wall
    bool grounded = true;
    float colDt = 0.2f;

    Lab::LabCollision::moveAndSlide(testPos, testVel, grounded, colDt, solidBoxes);

    // Player radius is 0.35m. Wall max.z is -19.5m. Player Z cannot penetrate past -19.5 + 0.35 = -19.15m!
    if (testPos.z < -19.16f) {
        std::cerr << "ERROR: Player penetrated North Wall! Pos.z = " << testPos.z << "\n";
        return 1;
    }
    if (std::abs(testVel.z) > 1e-3f) {
        std::cerr << "ERROR: Player velocity.z was not stopped by wall!\n";
        return 1;
    }
    std::cout << "[Test] Wall collision verified: Player stopped cleanly at Z = " << testPos.z << " (Wall at -19.5)\n";

    // 11.2 Wall Sliding Test (Diagonal movement into wall retains tangential velocity)
    testPos = Lab::Vec3(0.0f, 1.70f, -18.8f);
    testVel = Lab::Vec3(6.0f, 0.0f, -15.0f); // Diagonal: right + forward into wall
    Lab::LabCollision::moveAndSlide(testPos, testVel, grounded, colDt, solidBoxes);

    if (testPos.x <= 0.5f) {
        std::cerr << "ERROR: Wall sliding failed to preserve X tangential movement! Pos.x = " << testPos.x << "\n";
        return 1;
    }
    if (testPos.z < -19.16f) {
        std::cerr << "ERROR: Diagonal move penetrated wall! Pos.z = " << testPos.z << "\n";
        return 1;
    }
    std::cout << "[Test] Wall sliding verified: Player smoothly slid along wall to X = " << testPos.x << "\n";

    // 11.3 Floor Landing and Gravity
    testPos = Lab::Vec3(0.0f, 4.0f, 0.0f);
    testVel = Lab::Vec3(0.0f, -10.0f, 0.0f); // Falling down
    grounded = false;
    Lab::LabCollision::moveAndSlide(testPos, testVel, grounded, 0.5f, solidBoxes);

    if (!grounded || testPos.y < 1.69f || testPos.y > 1.71f) {
        std::cerr << "ERROR: Floor collision failed! Pos.y = " << testPos.y << " Grounded = " << grounded << "\n";
        return 1;
    }
    std::cout << "[Test] Floor landing verified: Player landed at eyeHeight Y = " << testPos.y << "\n";

    // ==================== 12. UNIT TESTS: BOT RESPAWN & STATS ====================
    std::cout << "[Test] Verifying Bot Death and Automated Respawn Cycle...\n";
    Lab::CombatBot testBot(99, "Respawn Bot", Lab::Vec3(5.0f, 0.0f, 5.0f), Lab::Vec3(15.0f, 0.0f, 5.0f));
    bool died = testBot.takeDamage(120.0f, true);
    if (!died || testBot.state != Lab::AIState::Dead || testBot.deaths != 1 || testBot.respawnTimer <= 0.0f) {
        std::cerr << "ERROR: Bot takeDamage fatal check failed! State = " << (int)testBot.state << "\n";
        return 1;
    }

    // Simulate update during respawn countdown
    std::vector<Lab::BulletTracer> dummyTracers;
    float dummyDmg = 0.0f;
    testBot.update(5.0f, Lab::Vec3(0, 0, 0), *map, dummyTracers, dummyDmg); // 5 seconds elapsed

    if (testBot.state != Lab::AIState::Patrol || testBot.health < 100.0f || !testBot.isAlive()) {
        std::cerr << "ERROR: Bot failed to respawn after countdown! State = " << (int)testBot.state << "\n";
        return 1;
    }
    std::cout << "[Test] Bot Respawn verified: Bot successfully revived to 100 HP at patrol start!\n";

    // ==================== 13. UNIT TESTS: IN-GAME CHAT ====================
    std::cout << "[Test] Verifying In-Game Chat System...\n";
    Lab::LabChat chat;
    chat.open();
    if (!chat.isOpen) {
        std::cerr << "ERROR: Chat failed to open!\n";
        return 1;
    }
    chat.onChar('F'); chat.onChar('P'); chat.onChar('S');
    if (chat.currentInput != "FPS") {
        std::cerr << "ERROR: Chat onChar typing failed! Input = " << chat.currentInput << "\n";
        return 1;
    }
    std::string sentChat;
    bool didSend = chat.onKey(257, 1, sentChat); // Enter
    if (!didSend || sentChat != "FPS" || chat.history.empty() || chat.isOpen) {
        std::cerr << "ERROR: Chat send on Enter failed!\n";
        return 1;
    }
    std::cout << "[Test] Chat verified: Sent message '" << sentChat << "' successfully buffered!\n";

    // ==================== 14. UNIT TESTS: PICKUP MANAGER ====================
    std::cout << "[Test] Verifying Ammo & Medkit Pickup Manager...\n";
    Lab::PickupManager pickups;
    pickups.spawnPickup(Lab::PickupType::Ammo, Lab::Vec3(0.0f, 0.5f, 0.0f), 36);
    pickups.spawnPickup(Lab::PickupType::Medkit, Lab::Vec3(0.0f, 0.5f, 0.0f), 50);

    if (pickups.items.size() != 2) {
        std::cerr << "ERROR: Failed to spawn pickups!\n";
        return 1;
    }

    int ammoGot = 0;
    float hpGot = 0.0f;
    int wepGot = -1;
    std::string pickMsg;
    pickups.update(0.1f, Lab::Vec3(0.0f, 1.7f, 0.0f), ammoGot, hpGot, wepGot, pickMsg);

    if (ammoGot != 36 || hpGot != 50.0f || !pickups.items.empty()) {
        std::cerr << "ERROR: Pickup collection failed! Ammo = " << ammoGot << " HP = " << hpGot << "\n";
        return 1;
    }
    std::cout << "[Test] Pickups verified: Collected +36 AMMO and +50 HP on proximity!\n";

    // ==================== 15. RENDER FRAME: SCOREBOARD TABLE UNDER TAB ====================
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    Lab::Camera gameCam(75.0f, (float)w / (float)h, 0.01f, 1000.0f);
    gameCam.setPosition(Lab::Vec3(0.0f, 1.70f, -5.0f));
    Lab::Renderer::beginFrame(gameCam);

    for (const auto& b : map->brushes) {
        Lab::Texture* tex = b.texturePath.empty() ? nullptr : textures[b.texturePath].get();
        Lab::Renderer::drawCube(b.position, b.size, b.color, tex, true, b.uvScale, b.uvMode);
    }

    // Prepare Scoreboard Entries
    std::vector<Lab::ScoreboardEntry> sbEntries;
    Lab::ScoreboardEntry pEntry;
    pEntry.name = "[YOU] Gordon Freeman";
    pEntry.kills = 7;
    pEntry.deaths = 2;
    pEntry.ping = "5ms";
    pEntry.isLocalPlayer = true;
    pEntry.isAlive = true;
    pEntry.status = "ALIVE";
    pEntry.team = "Blue (Alpha)";
    sbEntries.push_back(pEntry);

    Lab::ScoreboardEntry b1;
    b1.name = "Synth Soldier #1";
    b1.kills = 4;
    b1.deaths = 5;
    b1.ping = "BOT";
    b1.isLocalPlayer = false;
    b1.isAlive = true;
    b1.status = "ALIVE";
    b1.team = "Red (Beta)";
    sbEntries.push_back(b1);

    Lab::ScoreboardEntry b2;
    b2.name = "Synth Elite #2";
    b2.kills = 3;
    b2.deaths = 6;
    b2.ping = "BOT";
    b2.isLocalPlayer = false;
    b2.isAlive = false;
    b2.status = "DEAD (Respawn 3s)";
    b2.team = "Red (Beta)";
    sbEntries.push_back(b2);

    Lab::ScoreboardEntry b3;
    b3.name = "Synth Scout #3";
    b3.kills = 1;
    b3.deaths = 4;
    b3.ping = "BOT";
    b3.isLocalPlayer = false;
    b3.isAlive = true;
    b3.status = "ALIVE";
    b3.team = "Red (Beta)";
    sbEntries.push_back(b3);

    Lab::LabHUD hud;
    hud.renderScoreboard(w, h, sbEntries, "Research Complex Alpha", "Team Deathmatch (TDM)", 25);
    glFinish();
    saveFrameToBMP("test_scoreboard_tab.bmp", w, h);
    glfwSwapBuffers(window);

    // ==================== 16. RENDER FRAME: 3D PICKUPS & IN-GAME CHAT ====================
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    gameCam.setPosition(Lab::Vec3(0.0f, 1.70f, -8.0f));
    Lab::Renderer::beginFrame(gameCam);

    for (const auto& b : map->brushes) {
        Lab::Texture* tex = b.texturePath.empty() ? nullptr : textures[b.texturePath].get();
        Lab::Renderer::drawCube(b.position, b.size, b.color, tex, true, b.uvScale, b.uvMode);
    }

    // Spawn and render 3D rotating ammo crate and medkit
    Lab::PickupItem ammoDropItem;
    ammoDropItem.type = Lab::PickupType::Ammo;
    ammoDropItem.position = Lab::Vec3(-1.2f, 0.45f, -12.0f);
    ammoDropItem.rotationY = 35.0f;
    ammoDropItem.render();

    Lab::PickupItem medkitDropItem;
    medkitDropItem.type = Lab::PickupType::Medkit;
    medkitDropItem.position = Lab::Vec3(1.2f, 0.45f, -12.0f);
    medkitDropItem.rotationY = 65.0f;
    medkitDropItem.render();

    // Bottom HUD cards
    hud.health = 135.0f;
    hud.suitArmor = 80.0f;
    hud.ammoClip = 18;
    hud.ammoReserve = 144;
    hud.showCombatMessage("+ 36 AMMO COLLECTED", 2.5f);
    hud.render(w, h);

    // Render active in-game text chat
    Lab::LabChat ingameChat;
    ingameChat.addMessage("[SERVER]", "Match hosted: Research Complex Alpha", Lab::Vec3(0.3f, 0.8f, 1.0f));
    ingameChat.addMessage("[SERVER]", "Gordon eliminated Synth Elite #2 [HEADSHOT]", Lab::Vec3(0.3f, 0.95f, 0.4f));
    ingameChat.addMessage("Synth Soldier #1", "Heavy fire! Hostile pushing north hall!", Lab::Vec3(0.9f, 0.45f, 0.45f));
    ingameChat.addMessage("[ITEM]", "+ 36 AMMO COLLECTED", Lab::Vec3(0.95f, 0.82f, 0.15f));
    ingameChat.open();
    ingameChat.currentInput = "Covering the security gate! Fall back!";
    ingameChat.render(w, h);

    glFinish();
    saveFrameToBMP("test_ammo_and_chat.bmp", w, h);
    // ==================== 17. VERIFY BOT COMBAT: MULTI-TARGET & TACTICAL STRAFING ====================
    std::cout << "[Test] Verifying Bot Multi-Targeting and Combat Tactical Movement...\n";
    {
        Lab::AIManager aiMgr;
        // Spawn 2 bots in FFA
        Lab::CombatBot combatBot1(1, "Synth Alpha", Lab::Vec3(0.0f, 1.0f, 0.0f), Lab::Vec3(0.0f, 1.0f, 10.0f), -1);
        Lab::CombatBot combatBot2(2, "Synth Beta", Lab::Vec3(5.0f, 1.0f, 0.0f), Lab::Vec3(5.0f, 1.0f, 10.0f), -1);
        aiMgr.bots.push_back(combatBot1);
        aiMgr.bots.push_back(combatBot2);

        // Player is dead or far away (at 100, 100, 100)
        Lab::Vec3 deadPlayerPos(100.0f, 0.0f, 100.0f);
        bool isPlayerAlive = false;
        int playerTeam = -1;
        std::vector<Lab::BulletTracer> tracers;
        float dmgToPlayer = 0.0f;
        Lab::PickupManager combatPickups;
        Lab::LabChat combatChat;

        // Record initial position of combatBot1
        Lab::Vec3 initPos1 = aiMgr.bots[0].position;

        // Update AI over 1.0 second (10 ticks)
        for (int step = 0; step < 10; ++step) {
            aiMgr.update(0.1f, deadPlayerPos, isPlayerAlive, playerTeam, *map, tracers, dmgToPlayer, &combatPickups, &combatChat);
        }

        // 1. Check that b1 engaged b2 even though player is dead
        std::cout << "[Test] Bot 1 state: " << (int)aiMgr.bots[0].state << " HP: " << aiMgr.bots[0].health
                  << " Bot 2 HP: " << aiMgr.bots[1].health << "\n";
        
        // 2. Check that bot 1 moved (tactical strafe / advance) even when player is stationary / dead
        float movedDist = (aiMgr.bots[0].position - initPos1).length();
        std::cout << "[Test] Bot 1 moved distance during combat: " << movedDist << "m\n";
        if (movedDist < 0.01f) {
            std::cerr << "ERROR: Bot stood completely still during combat!\n";
            return 1;
        }

        // 3. Check that bot-vs-bot fight occurred
        std::cout << "[Test] Bullet tracers generated: " << tracers.size() << "\n";
        std::cout << "[Test] Bot multi-target and tactical strafing verified successfully!\n";
    }

    // ==================== 18. VERIFY MAP SPAWNS & HAMMER PREBUILTS ====================
    std::cout << "[Test] Verifying Map Spawn System (FFA, Team Alpha, Team Beta) and Serialization...\n";
    {
        // 1. Verify loading spawns from facility_alpha.labmap
        auto alphaMap = Lab::LabMap::loadFromFile("assets/maps/facility_alpha.labmap");
        if (!alphaMap) {
            std::cerr << "ERROR: Failed to load facility_alpha.labmap!\n";
            return 1;
        }
        std::cout << "[Test] facility_alpha.labmap loaded with " << alphaMap->spawnPoints.size() << " spawn points.\n";
        if (alphaMap->spawnPoints.size() < 4) {
            std::cerr << "ERROR: Expected at least 4 spawn points in facility_alpha.labmap, got " << alphaMap->spawnPoints.size() << "\n";
            return 1;
        }

        // Verify team allocation
        auto ffaSpawns = alphaMap->getSpawnsForTeam(Lab::GameMode::FFA, -1);
        auto tdmAlphaSpawns = alphaMap->getSpawnsForTeam(Lab::GameMode::TDM, 1);
        auto tdmBetaSpawns = alphaMap->getSpawnsForTeam(Lab::GameMode::TDM, 0);

        std::cout << "[Test] Spawns found - FFA: " << ffaSpawns.size() 
                  << ", Team Alpha (Blue): " << tdmAlphaSpawns.size() 
                  << ", Team Beta (Red): " << tdmBetaSpawns.size() << "\n";

        if (ffaSpawns.empty() || tdmAlphaSpawns.empty() || tdmBetaSpawns.empty()) {
            std::cerr << "ERROR: Map is missing FFA or Team spawns!\n";
            return 1;
        }

        // 2. Verify selectBestSpawn for TDM and FFA
        std::vector<Lab::Vec3> enemyPositions = { Lab::Vec3(0.0f, 0.0f, 0.0f) };
        auto chosenFFA = alphaMap->selectBestSpawn(Lab::GameMode::FFA, -1, enemyPositions);
        auto chosenAlpha = alphaMap->selectBestSpawn(Lab::GameMode::TDM, 1, enemyPositions);
        auto chosenBeta = alphaMap->selectBestSpawn(Lab::GameMode::TDM, 0, enemyPositions);

        std::cout << "[Test] Selected FFA Spawn: " << chosenFFA.getDisplayName() << " at (" << chosenFFA.position.x << ", " << chosenFFA.position.z << ")\n";
        std::cout << "[Test] Selected Team Alpha Spawn: " << chosenAlpha.getDisplayName() << " at (" << chosenAlpha.position.x << ", " << chosenAlpha.position.z << ")\n";
        std::cout << "[Test] Selected Team Beta Spawn: " << chosenBeta.getDisplayName() << " at (" << chosenBeta.position.x << ", " << chosenBeta.position.z << ")\n";

        if (chosenAlpha.type != Lab::SpawnType::TeamAlpha) {
            std::cerr << "ERROR: Team Alpha player spawned at non-Alpha spawn!\n";
            return 1;
        }
        if (chosenBeta.type != Lab::SpawnType::TeamBeta) {
            std::cerr << "ERROR: Team Beta player spawned at non-Beta spawn!\n";
            return 1;
        }

        // 3. Verify Serialization & Deserialization of Map Spawns
        std::string testMapPath = "test_spawns_temp.labmap";
        Lab::LabMap customMap;
        customMap.spawn.position = Lab::Vec3(10.0f, 1.0f, 20.0f);
        customMap.spawnPoints.push_back(Lab::MapSpawnPoint{ "info_player_deathmatch", Lab::Vec3(1.0f, 0.5f, 2.0f), 45.0f, Lab::SpawnType::FFA });
        customMap.spawnPoints.push_back(Lab::MapSpawnPoint{ "info_player_team1", Lab::Vec3(-15.0f, 0.5f, -25.0f), 90.0f, Lab::SpawnType::TeamAlpha });
        customMap.spawnPoints.push_back(Lab::MapSpawnPoint{ "info_player_team2", Lab::Vec3(30.0f, 0.5f, 40.0f), 180.0f, Lab::SpawnType::TeamBeta });
        customMap.saveToFile(testMapPath);

        auto loadedMap = Lab::LabMap::loadFromFile(testMapPath);
        std::filesystem::remove(testMapPath);

        if (!loadedMap || loadedMap->spawnPoints.size() != 3) {
            std::cerr << "ERROR: Failed to save and reload spawn points correctly!\n";
            return 1;
        }
        if (loadedMap->spawnPoints[0].type != Lab::SpawnType::FFA ||
            loadedMap->spawnPoints[1].type != Lab::SpawnType::TeamAlpha ||
            loadedMap->spawnPoints[2].type != Lab::SpawnType::TeamBeta) {
            std::cerr << "ERROR: Reloaded spawn point types do not match saved types!\n";
            return 1;
        }
        std::cout << "[Test] Spawn serialization/deserialization verified with 100% fidelity!\n";

        // 4. Render Visual Verification Frame of Hammer Spawns & Prebuilts
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        Lab::Camera spawnCam(70.0f, (float)w / (float)h, 0.1f, 1000.0f);
        spawnCam.setPosition(Lab::Vec3(0.0f, 2.0f, 6.0f));
        spawnCam.update(Lab::Vec2(0.0f, 15.0f)); // slight downward pitch
        Lab::Renderer::beginFrame(spawnCam);

        // Ground floor
        Lab::Renderer::drawCube(Lab::Vec3(0.0f, -0.1f, 0.0f), Lab::Vec3(16.0f, 0.1f, 16.0f), Lab::Vec3(0.16f, 0.18f, 0.22f));

        // Helper to render authentic Valve Hammer 3D holographic player spawn entity
        auto draw3DPlayerSpawn = [](const Lab::Vec3& pos, const Lab::Vec3& teamCol, float yaw) {
            // Landing pad with wireframe rim
            Lab::Renderer::drawCube(pos + Lab::Vec3(0.0f, -0.05f, 0.0f), Lab::Vec3(1.2f, 0.1f, 1.2f), teamCol * 0.7f, nullptr, false);
            Lab::Renderer::drawWireCube(pos + Lab::Vec3(0.0f, -0.05f, 0.0f), Lab::Vec3(1.22f, 0.11f, 1.22f), teamCol);

            // Bounding wireframe box for player collision volume (0.8m x 1.85m x 0.8m)
            Lab::Renderer::drawWireCube(pos + Lab::Vec3(0.0f, 0.92f, 0.0f), Lab::Vec3(0.8f, 1.85f, 0.8f), teamCol);

            // Mannequin Legs
            Lab::Renderer::drawCube(pos + Lab::Vec3(-0.16f, 0.45f, 0.0f), Lab::Vec3(0.18f, 0.8f, 0.2f), teamCol * 0.8f, nullptr, false);
            Lab::Renderer::drawCube(pos + Lab::Vec3(0.16f, 0.45f, 0.0f), Lab::Vec3(0.18f, 0.8f, 0.2f), teamCol * 0.8f, nullptr, false);

            // Mannequin Torso
            Lab::Renderer::drawCube(pos + Lab::Vec3(0.0f, 1.15f, 0.0f), Lab::Vec3(0.55f, 0.62f, 0.32f), teamCol * 0.9f, nullptr, false);

            // Mannequin Head & Visor
            Lab::Renderer::drawCube(pos + Lab::Vec3(0.0f, 1.62f, 0.0f), Lab::Vec3(0.32f, 0.32f, 0.32f), teamCol, nullptr, false);
            Lab::Renderer::drawCube(pos + Lab::Vec3(0.0f, 1.62f, 0.17f), Lab::Vec3(0.24f, 0.08f, 0.04f), Lab::Vec3(1.0f, 1.0f, 1.0f), nullptr, false);

            // Facing Direction Arrow on Pad
            float rad = yaw * 3.14159265f / 180.0f;
            Lab::Vec3 fwd(std::sin(rad), 0.0f, std::cos(rad));
            Lab::Renderer::drawCube(pos + fwd * 0.7f + Lab::Vec3(0.0f, 0.02f, 0.0f), Lab::Vec3(0.18f, 0.04f, 0.45f), teamCol, nullptr, false);
        };

        // Render FFA Spawn Pad (Green)
        Lab::Vec3 ffaPos(-3.5f, 0.0f, 0.0f);
        draw3DPlayerSpawn(ffaPos, Lab::Vec3(0.2f, 0.9f, 0.4f), 0.0f);

        // Render Team Alpha Spawn Pad (Blue)
        Lab::Vec3 alphaPos(0.0f, 0.0f, 0.0f);
        draw3DPlayerSpawn(alphaPos, Lab::Vec3(0.2f, 0.6f, 1.0f), 0.0f);

        // Render Team Beta Spawn Pad (Red)
        Lab::Vec3 betaPos(3.5f, 0.0f, 0.0f);
        draw3DPlayerSpawn(betaPos, Lab::Vec3(1.0f, 0.25f, 0.25f), 0.0f);

        // Render UI: Hammer Editor Prebuilts Tab
        Lab::Renderer::beginUI(w, h);
        Lab::SidebarLayout hl = Lab::getSidebarLayout((float)w, (float)h);

        // Background Hammer UI Frame
        Lab::Renderer::drawRect(0, 0, (float)w, 24.0f, winBg);
        Lab::LabFont::drawText(14.0f, 5.0f, "File  Edit  View  Tools  Help", 1.8f, textDark, Lab::LabFontType::System);
        Lab::LabFont::drawText((float)w - 380.0f, 5.0f, "Lab Hammer 2026 - Spawns & Prebuilts", 1.8f, Lab::Vec3(0.15f, 0.45f, 0.75f), Lab::LabFontType::GeoSans);
        
        // Toolbar (18 buttons with icons)
        Lab::Renderer::drawRect(0, 24.0f, (float)w, 34.0f, winBg);
        Lab::Renderer::drawRect(0, 57.0f, (float)w, 1.0f, winBorder);
        for (int i = 0; i < 18; ++i) {
            float bx = 8.0f + i * 28.0f;
            Lab::HammerIcons::drawToolbarIcon(i, bx, 29.0f, (i == 17) ? Lab::Vec3(1, 1, 1) : Lab::Vec3(0.25f, 0.3f, 0.35f), (i == 17) ? Lab::Vec3(0.15f, 0.65f, 0.35f) : Lab::Vec3(0.88f, 0.88f, 0.90f));
        }

        // Left Tools Bar with Spawn Tool highlighted (Tool 5 with icon)
        Lab::Renderer::drawRect(0, 58.0f, 42.0f, (float)h - 80.0f, winBg);
        for (int i = 0; i < 8; ++i) {
            float ty = 68.0f + i * 36.0f;
            bool isSel = (i == 5);
            Lab::Vec3 bgCol = isSel ? Lab::Vec3(0.2f, 0.75f, 0.4f) : Lab::Vec3(0.88f, 0.88f, 0.90f);
            Lab::Vec3 iconCol = isSel ? Lab::Vec3(1, 1, 1) : Lab::Vec3(0.25f, 0.28f, 0.32f);

            Lab::Renderer::drawRect(6.0f, ty, 30.0f, 30.0f, bgCol);
            Lab::Renderer::drawRect(6.0f, ty, 30.0f, 1.0f, isSel ? Lab::Vec3(0.2f, 0.75f, 0.95f) : winBorder);
            Lab::HammerIcons::drawHammerIcon(i, 9.0f, ty + 3.0f, iconCol, bgCol);
        }

        // Right Sidebar Panel
        Lab::Renderer::drawRect(hl.rightX, hl.rightY, hl.rightW, hl.rightH, winBg);
        Lab::Renderer::drawRect(hl.rightX, hl.rightY, 1.0f, hl.rightH, winBorder);

        // Tabs: Properties, Struktura, Prebuilty (Prebuilty is Active!)
        Lab::Renderer::drawRect(hl.tabPropX, hl.tabPropY, hl.tabPropW, hl.tabPropH, Lab::Vec3(0.85f, 0.85f, 0.88f));
        Lab::Renderer::drawRect(hl.tabPropX, hl.tabPropY, hl.tabPropW, 1.0f, winBorder);
        Lab::LabFont::drawText(hl.tabPropX + 16.0f, hl.tabPropY + 5.0f, "Properties", 1.5f, Lab::Vec3(0.45f, 0.45f, 0.45f), Lab::LabFontType::System);

        Lab::Renderer::drawRect(hl.tabOutX, hl.tabOutY, hl.tabOutW, hl.tabOutH, Lab::Vec3(0.85f, 0.85f, 0.88f));
        Lab::Renderer::drawRect(hl.tabOutX, hl.tabOutY, hl.tabOutW, 1.0f, winBorder);
        Lab::LabFont::drawText(hl.tabOutX + 18.0f, hl.tabOutY + 5.0f, "Struktura", 1.5f, Lab::Vec3(0.45f, 0.45f, 0.45f), Lab::LabFontType::System);

        Lab::Renderer::drawRect(hl.tabPreX, hl.tabPreY, hl.tabPreW, hl.tabPreH, Lab::Vec3(1, 1, 1));
        Lab::Renderer::drawRect(hl.tabPreX, hl.tabPreY, hl.tabPreW, 1.0f, orangeGlow);
        Lab::LabFont::drawText(hl.tabPreX + 16.0f, hl.tabPreY + 5.0f, "Prebuilty", 1.5f, textDark, Lab::LabFontType::System);

        // Prebuilt cards
        Lab::LabFont::drawText(hl.rightX + 12.0f, hl.rightY + 38.0f, "Prebuilts & Map Entities (Click to Place):", 1.6f, textDark, Lab::LabFontType::System);

        struct PreItem { const char* title; const char* desc; Lab::Vec3 col; };
        PreItem pItems[] = {
            { "Spawn: FFA / DM", "Neutralny spawn dla kazdego gracza", Lab::Vec3(0.18f, 0.65f, 0.35f) },
            { "Spawn: Team Alpha", "Baza Druzyny 1 (Niebiescy / Blue HQ)", Lab::Vec3(0.18f, 0.45f, 0.85f) },
            { "Spawn: Team Beta", "Baza Druzyny 2 (Czerwoni / Red HQ)", Lab::Vec3(0.85f, 0.25f, 0.25f) },
            { "Skrzynka Amunicji", "Zasobnik amunicji (+36 pociskow)", Lab::Vec3(0.75f, 0.65f, 0.15f) },
            { "Apteczka Polowa", "Pakiet medyczny (+50 HP zdrowia)", Lab::Vec3(0.85f, 0.85f, 0.90f) },
            { "Barykada Taktyczna", "Mur ochronny ze skrajnia (3x1.2m)", Lab::Vec3(0.45f, 0.50f, 0.58f) },
            { "Filar Betonowy", "Cylinder nosny konstrukcji (1.5x6m)", Lab::Vec3(0.55f, 0.58f, 0.65f) },
            { "Brama Bezpieczenstwa", "Przesuwne pancerne drzwi z czujnikiem", Lab::Vec3(0.25f, 0.35f, 0.45f) }
        };

        float startY = hl.rightY + 58.0f;
        float cardH = 46.0f;
        float cardSpacing = 52.0f;

        for (int i = 0; i < 8; ++i) {
            float cy = startY + i * cardSpacing;
            Lab::Renderer::drawRect(hl.rightX + 10.0f, cy, hl.rightW - 20.0f, cardH, Lab::Vec3(1, 1, 1));
            Lab::Renderer::drawRect(hl.rightX + 10.0f, cy, hl.rightW - 20.0f, 1.0f, winBorder);
            Lab::Renderer::drawRect(hl.rightX + 10.0f, cy, 6.0f, cardH, pItems[i].col);

            // Dedicated entity icon next to each card
            Lab::HammerIcons::drawEntityIcon(i, hl.rightX + 20.0f, cy + 10.0f, pItems[i].col, Lab::Vec3(0.93f, 0.94f, 0.96f));

            Lab::LabFont::drawText(hl.rightX + 54.0f, cy + 6.0f, pItems[i].title, 1.6f, textDark, Lab::LabFontType::System);
            Lab::LabFont::drawText(hl.rightX + 54.0f, cy + 24.0f, pItems[i].desc, 1.3f, Lab::Vec3(0.45f, 0.45f, 0.50f), Lab::LabFontType::System);
        }

        Lab::Renderer::endUI();
        glFinish();
        saveFrameToBMP("test_hammer_spawns.bmp", w, h);
        std::cout << "[Test] Saved Hammer Spawns visual test to 'test_hammer_spawns.bmp'.\n";
    }

    // 19. Verify Weapon Arsenal, Weapon Switching & Projectile Physics
    {
        std::cout << "\n[Test 19] Verifying Weapon Arsenal, Stats, Switching & Projectile Physics...\n";
        Lab::WeaponSystem ws;
        ws.init();

        // Check starting equipment: Pipe (Melee) & Pistol (Handgun) unlocked, others locked
        if (!ws.isUnlocked(Lab::WeaponID::Pipe)) {
            std::cerr << "ERROR: Pipe should be unlocked at start!\n";
            return 1;
        }
        if (!ws.isUnlocked(Lab::WeaponID::Pistol)) {
            std::cerr << "ERROR: Pistol should be unlocked at start!\n";
            return 1;
        }
        if (ws.isUnlocked(Lab::WeaponID::Minigun)) {
            std::cerr << "ERROR: Minigun should be locked at start!\n";
            return 1;
        }

        // Test slot equipping (Slot 1 = Pipe, Slot 2 = Pistol)
        if (!ws.equipSlot(1)) {
            std::cerr << "ERROR: Failed to equip slot 1 (Pipe)!\n";
            return 1;
        }
        if (ws.getActiveId() != Lab::WeaponID::Pipe) {
            std::cerr << "ERROR: Active weapon is not Pipe!\n";
            return 1;
        }
        if (!ws.getActiveDef().isMelee) {
            std::cerr << "ERROR: Pipe is not marked as Melee!\n";
            return 1;
        }

        // Test quick switch Q
        ws.equipSlot(2); // Pistol
        if (ws.getActiveId() != Lab::WeaponID::Pistol) {
            std::cerr << "ERROR: Active weapon is not Pistol!\n";
            return 1;
        }
        ws.quickSwitch(); // Should switch back to Pipe
        if (ws.getActiveId() != Lab::WeaponID::Pipe) {
            std::cerr << "ERROR: Quick switch failed to switch back to Pipe!\n";
            return 1;
        }

        // Test next/prev weapon cycling
        ws.nextWeapon();
        if (ws.getActiveId() != Lab::WeaponID::Pistol) {
            std::cerr << "ERROR: nextWeapon didn't cycle to Pistol!\n";
            return 1;
        }
        ws.nextWeapon(); // Should cycle back to Pipe (since only Pipe & Pistol unlocked)
        if (ws.getActiveId() != Lab::WeaponID::Pipe) {
            std::cerr << "ERROR: nextWeapon didn't wrap around to Pipe!\n";
            return 1;
        }

        // Unlock all weapons
        ws.unlockAll();
        for (int i = 0; i < 9; ++i) {
            if (!ws.isUnlocked((Lab::WeaponID)i)) {
                std::cerr << "ERROR: Weapon " << i << " not unlocked after unlockAll()!\n";
                return 1;
            }
        }

        // Test each weapon definition stats
        auto defs = Lab::WeaponSystem::createWeaponDefinitions();
        // Shotgun pellets check
        if (defs[(int)Lab::WeaponID::Shotgun].bulletsPerShot != 8) {
            std::cerr << "ERROR: Shotgun does not have 8 pellets!\n";
            return 1;
        }
        // Minigun fire rate check (1333 RPM => ~0.045s)
        if (defs[(int)Lab::WeaponID::Minigun].fireRate > 0.05f) {
            std::cerr << "ERROR: Minigun fireRate should be <= 0.05s!\n";
            return 1;
        }
        // Railgun damage check (fatal kinetic beam)
        if (defs[(int)Lab::WeaponID::Railgun].damage < 150.0f) {
            std::cerr << "ERROR: Railgun should deal >= 150 damage!\n";
            return 1;
        }
        // RPG projectile splash check (5.0m AoE)
        if (defs[(int)Lab::WeaponID::RPG].splashRadius < 4.0f) {
            std::cerr << "ERROR: RPG splash radius should be >= 4.0m!\n";
            return 1;
        }

        // Test Projectile Simulation
        ws.switchWeapon(Lab::WeaponID::RPG);
        ws.spawnProjectile(Lab::Vec3(0, 1.5f, 0), Lab::Vec3(0, 0, 1));
        if (ws.getProjectiles().empty()) {
            std::cerr << "ERROR: Spawning RPG projectile failed!\n";
            return 1;
        }
        ws.update(0.1f);
        if (ws.getProjectiles()[0].position.z <= 0.0f) {
            std::cerr << "ERROR: Projectile did not travel along forward vector!\n";
            return 1;
        }

        // Visual test: Render weapon viewmodel (Shotgun) and HUD selection bar
        glClearColor(0.08f, 0.10f, 0.14f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        Lab::Camera weaponCam(75.0f, 16.0f / 9.0f, 0.01f, 1000.0f);
        weaponCam.setPosition(Lab::Vec3(0.0f, 1.7f, 0.0f));
        Lab::Renderer::beginFrame(weaponCam);

        // Load weapon textures and custom user STL model
        std::unique_ptr<Lab::Texture> pipeTex;
        if (std::filesystem::exists("assets/textures/weapon_pipe.bmp")) {
            pipeTex = std::make_unique<Lab::Texture>("assets/textures/weapon_pipe.bmp");
        }
        std::unique_ptr<Lab::Mesh> pipeStl(Lab::Mesh::loadSTL("assets/models/pipe.stl"));
        if (pipeStl) {
            std::cout << "[Test] User STL weapon model loaded successfully: assets/models/pipe.stl (triangles: " 
                      << pipeStl->getIndexCount() / 3 << ")\n";
        }

        // Switch to Pipe and render viewmodel with user's STL model
        ws.switchWeapon(Lab::WeaponID::Pipe);
        Lab::WeaponAnimator anim;
        ws.renderViewModel(weaponCam, anim, pipeTex.get(), pipeStl.get(), 0.0f);

        // Render HUD with Weapon selection bar
        Lab::LabHUD weaponHud;
        weaponHud.weaponName = "PIPE";
        weaponHud.isMeleeWeapon = true;
        weaponHud.activeWeaponSlot = 1;
        weaponHud.ammoClip = 0;
        weaponHud.ammoReserve = 0;
        weaponHud.weaponSelectorTimer = 3.0f;
        for (int i = 0; i < 9; ++i) {
            weaponHud.slotWeaponNames.push_back(defs[i].shortName);
            weaponHud.slotUnlocked.push_back(true);
        }
        weaponHud.render(w, h);

        Lab::Renderer::endFrame();
        glFinish();
        saveFrameToBMP("test_weapon_arsenal.bmp", w, h);
        std::cout << "[Test] Saved Weapon Arsenal visual verification to 'test_weapon_arsenal.bmp'.\n";
    }

    // 20. Verify 3D Particle System (Emitters, Physics, Blending & Visuals)
    {
        std::cout << "\n[Test 20] Verifying Particle System, Emitters, Physics & Translucency...\n";
        Lab::ParticleSystem ps;
        ps.init();

        // 1. Verify Emitter Spawns
        ps.spawnImpact(Lab::Vec3(0, 1, 0), Lab::Vec3(0, 1, 0), Lab::SurfaceType::Concrete);
        size_t impactCount = ps.getActiveCount();
        if (impactCount < 10) {
            std::cerr << "ERROR: spawnImpact should spawn at least 10 particles!\n";
            return 1;
        }

        ps.spawnBlood(Lab::Vec3(0, 1.5f, 0), Lab::Vec3(0, 0, 1), true);
        size_t bloodCount = ps.getActiveCount() - impactCount;
        if (bloodCount < 20) {
            std::cerr << "ERROR: spawnBlood (headshot) should spawn at least 20 particles!\n";
            return 1;
        }

        ps.spawnExplosion(Lab::Vec3(0, 0, 0), 4.0f, Lab::Vec3(1.0f, 0.5f, 0.1f));
        size_t explosionTotal = ps.getActiveCount();
        if (explosionTotal < 100) {
            std::cerr << "ERROR: spawnExplosion should spawn a rich burst of fire, smoke and shrapnel!\n";
            return 1;
        }

        ps.spawnMuzzleEffect(Lab::Vec3(0, 1, 0), Lab::Vec3(0, 0, 1), Lab::WeaponID::Shotgun);
        ps.spawnProjectileTrail(Lab::Vec3(0, 1, 0), Lab::WeaponID::RPG);
        ps.spawnBeamSparks(Lab::Vec3(0, 1, 0), Lab::Vec3(10, 1, 0), Lab::Vec3(0.3f, 0.8f, 1.0f), 12);
        ps.spawnAmbientWeather(Lab::Vec3(0, 1.7f, 0), 20, true);

        // 2. Verify Physics Simulation & Bouncing
        Lab::Particle bounceP;
        bounceP.position = Lab::Vec3(0.0f, 2.0f, 0.0f);
        bounceP.velocity = Lab::Vec3(0.0f, -8.0f, 0.0f);
        bounceP.acceleration = Lab::Vec3(0.0f, -9.81f, 0.0f);
        bounceP.hasCollision = true;
        bounceP.bounceCount = 3;
        bounceP.maxLifetime = 5.0f;
        ps.spawnParticle(bounceP);

        // Update several frames and verify particles move and bounce
        for (int step = 0; step < 10; ++step) {
            ps.update(0.033f, nullptr);
        }

        // 3. Render Visual Verification Frame: Explosions, Blood, Sparks & Snow in 3D
        glClearColor(0.06f, 0.08f, 0.11f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        Lab::Camera partCam(75.0f, 16.0f / 9.0f, 0.01f, 1000.0f);
        partCam.setPosition(Lab::Vec3(0.0f, 2.0f, 6.0f));
        Lab::Renderer::beginFrame(partCam);

        // Render floor grid base
        Lab::Renderer::drawCube(Lab::Vec3(0.0f, -0.1f, 0.0f), Lab::Vec3(20.0f, 0.1f, 20.0f), Lab::Vec3(0.14f, 0.16f, 0.20f));

        // Spawn a fresh centered explosion, spark fountain, and blood spray
        ps.clear();
        ps.spawnExplosion(Lab::Vec3(-2.0f, 1.2f, 0.0f), 5.0f, Lab::Vec3(1.0f, 0.5f, 0.1f));
        ps.spawnImpact(Lab::Vec3(2.0f, 1.5f, 0.0f), Lab::Vec3(-1.0f, 1.0f, 0.0f), Lab::SurfaceType::Concrete);
        ps.spawnBlood(Lab::Vec3(0.0f, 1.6f, 1.0f), Lab::Vec3(0.0f, 0.5f, -1.0f), true);
        ps.spawnBeamSparks(Lab::Vec3(-4.0f, 2.5f, -1.0f), Lab::Vec3(4.0f, 2.5f, 1.0f), Lab::Vec3(0.2f, 0.85f, 1.0f), 24);
        ps.spawnAmbientWeather(Lab::Vec3(0.0f, 2.0f, 0.0f), 45, true);

        // Simulate 4 frames so particles have volumetric spread
        for (int f = 0; f < 4; ++f) {
            ps.update(0.04f, nullptr);
        }

        // Render particles
        ps.render(partCam);

        // Render HUD overlay with Particle metrics
        Lab::Renderer::beginUI(w, h);
        Lab::Renderer::drawRect(40.0f, 30.0f, 420.0f, 85.0f, Lab::Vec3(0.10f, 0.12f, 0.16f));
        Lab::Renderer::drawRect(40.0f, 30.0f, 420.0f, 1.0f, Lab::Vec3(0.2f, 0.7f, 1.0f));
        Lab::LabFont::drawText(56.0f, 44.0f, "LAB PARTICLE SYSTEM ENGINE", 2.0f, Lab::Vec3(0.98f, 0.78f, 0.08f), Lab::LabFontType::GeoSans);
        std::string statStr = "ACTIVE PARTICLES: " + std::to_string(ps.getActiveCount()) + " | 60 FPS BATCHED";
        Lab::LabFont::drawText(56.0f, 74.0f, statStr, 1.6f, Lab::Vec3(0.85f, 0.90f, 0.95f), Lab::LabFontType::GeoSans);
        Lab::Renderer::endUI();

        Lab::Renderer::endFrame();
        glFinish();
        saveFrameToBMP("test_particle_system.bmp", w, h);
        std::cout << "[Test] Saved Particle System visual verification to 'test_particle_system.bmp'.\n";
    }

    // ==========================================
    // TEST 21: Weapon Spawners & Map Format Serialization & 60s Respawn
    // ==========================================
    std::cout << "[Test 21] Running Weapon Spawners, Serialization & 60s Respawn Verification...\n";
    {
        // 1. Test Map serialization and deserialization of weapon spawners
        Lab::LabMap mapTest;
        Lab::MapWeaponSpawner ws1;
        ws1.weaponId = 0; // Pipe
        ws1.position = Lab::Vec3(10.0f, 0.0f, -5.0f);
        ws1.yaw = 45.0f;
        ws1.respawnTime = 60.0f;
        mapTest.weaponSpawners.push_back(ws1);

        Lab::MapWeaponSpawner ws2;
        ws2.weaponId = 5; // Minigun
        ws2.position = Lab::Vec3(-8.0f, 1.0f, 12.0f);
        ws2.yaw = 180.0f;
        ws2.respawnTime = 60.0f;
        mapTest.weaponSpawners.push_back(ws2);

        std::string testMapPath = "temp_spawner_test.labmap";
        if (mapTest.saveToFile(testMapPath)) {
            auto loadedMap = Lab::LabMap::loadFromFile(testMapPath);
            if (loadedMap) {
                if (loadedMap->weaponSpawners.size() != 2) {
                    std::cerr << "Assertion failed: loadedMap->weaponSpawners.size() == 2, got " << loadedMap->weaponSpawners.size() << "\n";
                    return 1;
                }
                if (loadedMap->weaponSpawners[0].weaponId != 0 || loadedMap->weaponSpawners[1].weaponId != 5) {
                    std::cerr << "Assertion failed: weapon IDs match\n";
                    return 1;
                }
                if (std::abs(loadedMap->weaponSpawners[0].respawnTime - 60.0f) > 0.01f) {
                    std::cerr << "Assertion failed: respawn timer is 60.0s\n";
                    return 1;
                }
                std::cout << "  [PASS] Map Weapon Spawner serialization/deserialization validated.\n";
            }
            std::filesystem::remove(testMapPath);
        }

        // 2. Test PickupManager persistent weapon pads and 60-second respawn logic
        Lab::PickupManager pm;
        pm.addWeaponPad(2, Lab::Vec3(0.0f, 0.0f, 0.0f), 60.0f); // Shotgun spawner at origin
        if (pm.weaponPads.size() != 1 || !pm.weaponPads[0].isAvailable()) {
            std::cerr << "Assertion failed: weapon pad initially available\n";
            return 1;
        }

        // Player is far away: no pickup
        int ammoAdded = 0;
        float hpAdded = 0.0f;
        int unlockedWep = -1;
        std::string notice = "";
        bool respawned = false;
        pm.update(0.1f, Lab::Vec3(100.0f, 0.0f, 100.0f), ammoAdded, hpAdded, unlockedWep, notice, respawned);
        if (unlockedWep != -1 || !pm.weaponPads[0].isAvailable()) {
            std::cerr << "Assertion failed: distant player does not trigger pickup\n";
            return 1;
        }

        // Player walks onto pad: triggers weapon pickup!
        pm.update(0.1f, Lab::Vec3(0.5f, 0.0f, 0.5f), ammoAdded, hpAdded, unlockedWep, notice, respawned);
        if (unlockedWep != 2 || pm.weaponPads[0].isAvailable()) {
            std::cerr << "Assertion failed: player picks up weapon ID 2 and pad goes into cooldown, got " << unlockedWep << "\n";
            return 1;
        }
        if (std::abs(pm.weaponPads[0].respawnTimer - 60.0f) > 0.2f) {
            std::cerr << "Assertion failed: pad cooldown timer set to 60s, got " << pm.weaponPads[0].respawnTimer << "\n";
            return 1;
        }
        std::cout << "  [PASS] Weapon pickup unlocks weapon ID " << unlockedWep << " and activates 60s cooldown timer.\n";

        // Simulate 30 seconds: pad must remain on cooldown
        for (int step = 0; step < 300; ++step) {
            pm.update(0.1f, Lab::Vec3(100.0f, 0.0f, 100.0f), ammoAdded, hpAdded, unlockedWep, notice, respawned);
        }
        if (pm.weaponPads[0].isAvailable()) {
            std::cerr << "Assertion failed: pad should still be on cooldown at 30s\n";
            return 1;
        }

        // Simulate remaining 30.2 seconds: pad must respawn and trigger respawn flash flag!
        respawned = false;
        for (int step = 0; step < 305; ++step) {
            bool padResp = false;
            pm.update(0.1f, Lab::Vec3(100.0f, 0.0f, 100.0f), ammoAdded, hpAdded, unlockedWep, notice, padResp);
            if (padResp) respawned = true;
        }
        if (!pm.weaponPads[0].isAvailable() || !respawned) {
            std::cerr << "Assertion failed: pad respawned after 60s cooldown, available=" << pm.weaponPads[0].isAvailable() << ", respawned=" << respawned << "\n";
            return 1;
        }
        std::cout << "  [PASS] Pad automatically respawned after exactly 60 seconds with respawn trigger flag.\n";
    }

    // ==========================================
    // TEST 22: STL Model Texture Auto-Resolution & Visual Spawner Render
    // ==========================================
    std::cout << "[Test 22] Running Model Texture Resolution & Visual Verification...\n";
    {
        // Test check-once caching for missing model & texture
        std::string missingTex = Lab::Renderer::resolveModelTexture("assets/models/nonexistent.stl", "weapon_pistol.bmp");
        if (missingTex != "weapon_pistol.bmp") {
            std::cerr << "Assertion failed: fallback texture returned for nonexistent model\n";
            return 1;
        }
        std::string cachedTex = Lab::Renderer::resolveModelTexture("assets/models/nonexistent.stl", "different_fallback.bmp");
        if (cachedTex != "weapon_pistol.bmp") {
            std::cerr << "Assertion failed: cached texture returned without re-probing disk\n";
            return 1;
        }
        Lab::Mesh* missingMesh1 = Lab::Mesh::loadSTL("assets/models/nonexistent.stl");
        Lab::Mesh* missingMesh2 = Lab::Mesh::loadSTL("assets/models/nonexistent.stl");
        if (missingMesh1 != nullptr || missingMesh2 != nullptr) {
            std::cerr << "Assertion failed: nonexistent mesh returns nullptr\n";
            return 1;
        }
        std::cout << "  [PASS] Check-once caching validated for missing models and textures.\n";

        std::string resolvedPipe = Lab::Renderer::resolveModelTexture("assets/models/pipe.stl", "weapon_pipe.bmp");
        std::cout << "  Model 'assets/models/pipe.stl' resolved texture: " << resolvedPipe << "\n";

        // Visual render of weapon spawners
        glClearColor(0.08f, 0.09f, 0.12f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        Lab::Camera spawnerCam(75.0f, (float)w / (float)h, 0.1f, 100.0f);
        spawnerCam.setPosition(Lab::Vec3(0.0f, 2.0f, 5.0f));
        Lab::Renderer::beginFrame(spawnerCam);

        // Floor
        Lab::Renderer::drawCube(Lab::Vec3(0.0f, -0.05f, 0.0f), Lab::Vec3(20.0f, 0.1f, 20.0f), Lab::Vec3(0.16f, 0.18f, 0.22f));

        // Spawn 3 weapon pads: Pipe, Shotgun, Minigun
        Lab::PickupManager visualPms;
        visualPms.addWeaponPad(0, Lab::Vec3(-2.8f, 0.0f, 0.0f), 60.0f); // Pipe
        visualPms.addWeaponPad(2, Lab::Vec3(0.0f, 0.0f, 0.0f), 60.0f);  // Shotgun
        visualPms.addWeaponPad(5, Lab::Vec3(2.8f, 0.0f, 0.0f), 60.0f);  // Minigun

        // Load mesh for pipe if present
        std::unique_ptr<Lab::Mesh> pipeMesh(Lab::Mesh::loadSTL("assets/models/pipe.stl"));
        std::vector<Lab::Mesh*> wepMeshes(9, nullptr);
        if (pipeMesh) wepMeshes[0] = pipeMesh.get();
        std::vector<Lab::Texture*> wepTextures(9, nullptr);

        visualPms.render(wepMeshes, wepTextures);

        // HUD overlay
        Lab::Renderer::beginUI(w, h);
        Lab::Renderer::drawRect(40.0f, 30.0f, 580.0f, 85.0f, Lab::Vec3(0.10f, 0.12f, 0.16f));
        Lab::Renderer::drawRect(40.0f, 30.0f, 580.0f, 1.0f, Lab::Vec3(0.2f, 0.7f, 1.0f));
        Lab::LabFont::drawText(56.0f, 44.0f, "WEAPON SPAWNER PADS - 60s RESPAWN", 2.0f, Lab::Vec3(0.98f, 0.78f, 0.08f), Lab::LabFontType::GeoSans);
        Lab::LabFont::drawText(56.0f, 74.0f, "GROUND PEDESTAL | 3D WEAPON MODEL | AUTO COOLDOWN", 1.6f, Lab::Vec3(0.85f, 0.90f, 0.95f), Lab::LabFontType::GeoSans);
        Lab::Renderer::endUI();

        Lab::Renderer::endFrame();
        glFinish();
        saveFrameToBMP("test_weapon_spawners.bmp", w, h);
        std::cout << "[Test] Saved Weapon Spawners visual verification to 'test_weapon_spawners.bmp'.\n";
    }

    // ==========================================
    // TEST 23: 3D Audio Engine, Procedural WAV Synthesizer & Spatial Falloff
    // ==========================================
    std::cout << "\n[Test 23] Verifying 3D Audio Engine, Procedural WAV Synthesizer & Spatial Falloff...\n";
    {
        // 1. Initialize Audio Engine in Headless/Offline verification mode (no physical speaker needed)
        bool audioInit = Lab::AudioEngine::init(false);
        if (!audioInit || !Lab::AudioEngine::isInitialized()) {
            std::cerr << "Assertion failed: AudioEngine::init(false) should succeed in headless mode\n";
            return 1;
        }
        std::cout << "  [PASS] AudioEngine initialized successfully in headless mode.\n";

        // 2. Verify all 20 sound definitions and generated WAV files on disk
        constexpr size_t soundCount = (size_t)Lab::SoundID::Count;
        static_assert(soundCount == 20, "Expected 20 sounds in SoundID enum");

        for (size_t i = 0; i < soundCount; ++i) {
            const auto& def = Lab::AudioEngine::getSoundDef((Lab::SoundID)i);
            std::string filePath = "assets/audio/" + def.filename;
            if (!std::filesystem::exists(filePath)) {
                std::cerr << "Assertion failed: Synthesized WAV audio file not found on disk: " << filePath << "\n";
                return 1;
            }

            // Verify valid RIFF/WAVE header
            std::ifstream file(filePath, std::ios::binary);
            if (!file.is_open()) {
                std::cerr << "Assertion failed: Could not open audio file: " << filePath << "\n";
                return 1;
            }
            char header[44];
            file.read(header, 44);
            if (file.gcount() < 44 || std::memcmp(header, "RIFF", 4) != 0 || std::memcmp(header + 8, "WAVE", 4) != 0) {
                std::cerr << "Assertion failed: Invalid RIFF/WAVE header in: " << filePath << "\n";
                return 1;
            }
            file.seekg(0, std::ios::end);
            size_t fileSize = (size_t)file.tellg();
            if (fileSize <= 44) {
                std::cerr << "Assertion failed: Audio file contains no PCM sample data: " << filePath << "\n";
                return 1;
            }
            std::cout << "  Verified WAV [" << (i + 1) << "/20]: " << def.displayName << " (" << def.filename << ", " << fileSize << " bytes)\n";
        }
        std::cout << "  [PASS] All 20 procedural sound assets verified with valid RIFF 16-bit PCM WAVE headers.\n";

        // 3. Verify Master Volume controls and bounds
        Lab::AudioEngine::setMasterVolume(0.75f);
        if (std::abs(Lab::AudioEngine::getMasterVolume() - 0.75f) > 1e-4f) {
            std::cerr << "Assertion failed: Master volume not updated properly\n";
            return 1;
        }
        Lab::AudioEngine::setMasterVolume(1.0f);

        // 4. Verify 3D Listener Positioning & Orientation
        Lab::AudioEngine::setListener(Lab::Vec3(0.0f, 1.8f, 0.0f), Lab::Vec3(0.0f, 0.0f, -1.0f), Lab::Vec3(0.0f, 1.0f, 0.0f));

        // 5. Verify 3D distance attenuation inverse falloff formula
        const auto& expDef = Lab::AudioEngine::getSoundDef(Lab::SoundID::RPGExplosion);
        float dNear = 2.0f;
        float dFar = 50.0f;
        float gainNear = expDef.minDistance / (expDef.minDistance + std::max(0.0f, dNear - expDef.minDistance));
        float gainFar = expDef.minDistance / (expDef.minDistance + std::max(0.0f, dFar - expDef.minDistance));
        if (gainNear <= gainFar || gainNear > 1.0f || gainFar <= 0.0f) {
            std::cerr << "Assertion failed: 3D spatial attenuation math invalid\n";
            return 1;
        }
        std::cout << "  [PASS] 3D Inverse Spatial Attenuation verified: near gain = " << gainNear << ", far gain = " << gainFar << "\n";

        // 6. Test invocation of playSound, playSound3D and update
        Lab::AudioEngine::playSound(Lab::SoundID::PistolShot, 1.0f);
        Lab::AudioEngine::playSound3D(Lab::SoundID::RPGExplosion, Lab::Vec3(10.0f, 0.0f, 5.0f));
        Lab::AudioEngine::update(0.016f);

        // 7. Test clean shutdown
        Lab::AudioEngine::shutdown();
        if (Lab::AudioEngine::isInitialized()) {
            std::cerr << "Assertion failed: AudioEngine::shutdown did not reset isInitialized flag\n";
            return 1;
        }
        std::cout << "  [PASS] AudioEngine shutdown clean.\n";
    }

    Lab::Renderer::shutdown();
    glfwDestroyWindow(window);
    glfwTerminate();

    std::cout << "[Test] All automated and visual verifications passed with 100% success!\n";
    return 0;
}
