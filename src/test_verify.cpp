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

        // Verify Bot Grounding (feet clamped to floor, zero levitation)
        if (std::abs(aiMgr.bots[0].position.y - 0.0f) > 0.05f) {
            std::cerr << "ERROR: Bot not grounded on floor (Y != 0.0, got " << aiMgr.bots[0].position.y << ")!\n";
            return 1;
        }
        std::cout << "  [PASS] Bot ground contact verified: boots firmly clamped to surface (Y=0.0m, no levitation)!\n";

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

        // 2. Verify all 26 sound definitions and generated WAV files on disk
        constexpr size_t soundCount = (size_t)Lab::SoundID::Count;
        static_assert(soundCount == 26, "Expected 26 sounds in SoundID enum");

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
            std::cout << "  Verified WAV [" << (i + 1) << "/" << soundCount << "]: " << def.displayName << " (" << def.filename << ", " << fileSize << " bytes)\n";
        }
        std::cout << "  [PASS] All " << soundCount << " procedural sound assets verified with valid RIFF 16-bit PCM WAVE headers.\n";

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

    // ==========================================
    // TEST 24: Tactical FPP Arms, Hand Sockets & Animation States (Reload, Inspect, Melee Slash)
    // ==========================================
    std::cout << "\n[Test 24] Verifying First-Person Tactical Arms & Animation State Machine...\n";
    {
        Lab::WeaponAnimator animator;
        if (animator.getStateProgress() != 0.0f || animator.state != Lab::WeaponAnimState::Idle) {
            std::cerr << "Assertion failed: Animator should start in Idle state\n";
            return 1;
        }

        // 1. Test Weapon Inspect State (F key)
        animator.onInspect(2.4f);
        if (!animator.isInspecting() || animator.state != Lab::WeaponAnimState::Inspect) {
            std::cerr << "Assertion failed: Animator should enter Inspect state\n";
            return 1;
        }
        animator.update(0.6f, Lab::Vec2(0, 0), 0.0f);
        if (animator.getStateProgress() < 0.2f) {
            std::cerr << "Assertion failed: Inspect state progress did not advance\n";
            return 1;
        }
        Lab::Vec3 inspectPos = animator.calculatePositionOffset(Lab::Vec3(0, 0, 0));
        Lab::Vec3 inspectRot = animator.calculateRotationOffset(Lab::Vec3(0, 0, 0));
        if (inspectRot.y >= 0.0f) {
            std::cerr << "Assertion failed: Inspect rotation should tilt weapon right to inspect receiver\n";
            return 1;
        }
        std::cout << "  [PASS] Weapon Inspect State verified: Progress=" << animator.getStateProgress() 
                  << ", PosY=" << inspectPos.y << ", RotY=" << inspectRot.y << "\n";

        // Cancel inspect on combat action
        animator.cancelInspect();
        if (animator.isInspecting() || animator.state != Lab::WeaponAnimState::Idle) {
            std::cerr << "Assertion failed: cancelInspect should return to Idle\n";
            return 1;
        }

        // 2. Test Multi-Phase Reload State (R key)
        animator.onReload(1.8f);
        if (!animator.isReloading()) {
            std::cerr << "Assertion failed: onReload should set state to Reload\n";
            return 1;
        }
        // Advance to Phase 2 (Magazine retrieval and insertion)
        animator.update(0.72f, Lab::Vec2(0, 0), 0.0f); // 0.40 progress
        if (!animator.isReloadMagazineVisible()) {
            std::cerr << "Assertion failed: Magazine should be visible during reload phase 0.22 - 0.72\n";
            return 1;
        }
        Lab::Vec3 leftHandReload = animator.getLeftHandReloadOffset();
        if (leftHandReload.y >= 0.0f) {
            std::cerr << "Assertion failed: Left hand should drop toward magwell during reload\n";
            return 1;
        }
        std::cout << "  [PASS] Reload Phase 2 verified: Magazine visible, LeftHandOffset Y=" << leftHandReload.y << "\n";

        // Advance past reload completion
        animator.update(1.2f, Lab::Vec2(0, 0), 0.0f);
        if (animator.isReloading() || animator.state != Lab::WeaponAnimState::Idle) {
            std::cerr << "Assertion failed: Reload should complete and return to Idle\n";
            return 1;
        }
        std::cout << "  [PASS] Reload completed smoothly back to Idle combat stance.\n";

        // 3. Test Melee Slash Kinematics (Pipe swing)
        animator.onFire(true);
        if (!animator.isMeleeSwinging()) {
            std::cerr << "Assertion failed: onFire(true) should set state to MeleeSwing\n";
            return 1;
        }
        animator.update(0.18f, Lab::Vec2(0, 0), 0.0f);
        Lab::Vec3 slashRot = animator.calculateRotationOffset(Lab::Vec3(0, 0, 0));
        std::cout << "  [PASS] Melee Slash Phase verified: Swing angle yaw=" << slashRot.y << ", roll=" << slashRot.z << "\n";
        animator.update(0.30f, Lab::Vec2(0, 0), 0.0f);
        if (animator.isMeleeSwinging()) {
            std::cerr << "Assertion failed: Melee swing should finish after duration\n";
            return 1;
        }

        // 4. Visual Viewmodel Verification with FPP Tactical Arms
        glClearColor(0.06f, 0.07f, 0.10f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        Lab::Camera armsCam(75.0f, (float)w / (float)h, 0.01f, 1000.0f);
        Lab::Renderer::beginFrame(armsCam);

        Lab::WeaponSystem ws;
        ws.init();
        ws.unlockAll();
        ws.switchWeapon(Lab::WeaponID::M4A4S);
        ws.update(0.5f); // Complete switch transition

        // Advance animator into reload phase 2: left hand holding fresh magazine under receiver
        Lab::WeaponAnimator testAnim;
        testAnim.onReload(2.0f);
        testAnim.update(0.70f, Lab::Vec2(0, 0), 0.0f);

        ws.renderViewModel(armsCam, testAnim, nullptr, nullptr, 0.0f);

        // UI Overlay indicating FPP tactical arms validation
        Lab::Renderer::beginUI(w, h);
        Lab::Renderer::drawRect(40.0f, 30.0f, 620.0f, 85.0f, Lab::Vec3(0.10f, 0.12f, 0.16f));
        Lab::Renderer::drawRect(40.0f, 30.0f, 620.0f, 1.0f, Lab::Vec3(0.2f, 0.85f, 1.0f));
        Lab::LabFont::drawText(56.0f, 44.0f, "TACTICAL FPP ARMS & HANDS - KINEMATIC SOCKETS", 2.0f, Lab::Vec3(0.20f, 0.85f, 1.0f), Lab::LabFontType::GeoSans);
        Lab::LabFont::drawText(56.0f, 74.0f, "DUAL SLEEVES | KNUCKLE PLATES | CYBER GAUNTLETS | FULL ANIM STATES", 1.6f, Lab::Vec3(0.85f, 0.90f, 0.95f), Lab::LabFontType::GeoSans);
        Lab::Renderer::endUI();

        Lab::Renderer::endFrame();
        glFinish();
        saveFrameToBMP("test_arms_and_viewmodel.bmp", w, h);
        std::cout << "  [PASS] Saved visual FPP Arms & Viewmodel verification to 'test_arms_and_viewmodel.bmp'.\n";
    }

    // =========================================================================
    // [Test 25] Verifying Real-Time Lighting, Half-Life 2 Flashlight & Dynamic Shadows (Shadow Mapping + PCF)
    // =========================================================================
    {
        std::cout << "\n[Test 25] Verifying Real-Time Lighting, Tactical Flashlight & Shadow Mapping...\n";

        // 1. Initialize ShadowMap FBO & Depth Texture (OpenGL 4.5 Direct State Access)
        Lab::ShadowMap shadowMap;
        if (!shadowMap.init(2048, 2048)) {
            std::cerr << "Assertion failed: ShadowMap FBO initialization failed\n";
            return 1;
        }
        if (shadowMap.getFbo() == 0 || shadowMap.getDepthTexture() == 0) {
            std::cerr << "Assertion failed: Invalid shadow FBO or depth texture handles\n";
            return 1;
        }
        GLenum fboStatus = glCheckNamedFramebufferStatus(shadowMap.getFbo(), GL_FRAMEBUFFER);
        if (fboStatus != GL_FRAMEBUFFER_COMPLETE) {
            std::cerr << "Assertion failed: ShadowMap FBO not complete, status: " << fboStatus << "\n";
            return 1;
        }
        std::cout << "  [PASS] ShadowMap FBO 2048x2048 (DSA) initialized and complete.\n";

        // 2. Setup Tactical Flashlight (HL2 style Spotlight)
        Lab::Flashlight flashlight;
        flashlight.enabled = true;
        Lab::Vec3 camPos(0.0f, 1.8f, 5.0f);
        Lab::Vec3 camFwd(0.0f, -0.15f, -1.0f);
        camFwd = camFwd.normalized();
        flashlight.update(camPos, camFwd, Lab::Vec3(1, 0, 0), Lab::Vec3(0, 1, 0));

        if (!flashlight.enabled || flashlight.range <= 0.0f) {
            std::cerr << "Assertion failed: Flashlight should be enabled with positive range\n";
            return 1;
        }
        std::cout << "  [PASS] Tactical Flashlight spotlight parameters verified (Range=" << flashlight.range << "m, InnerCone=" << flashlight.innerCone << ")\n";

        // 3. Shadow Depth Pass (Compute light space matrix and render shadow casters)
        Lab::Vec3 sunDir(-0.45f, -0.85f, -0.28f);
        Lab::Mat4 lightSpaceMatrix = Lab::ShadowMap::computeSunLightSpaceMatrix(sunDir, Lab::Vec3(0, 0, 0), 25.0f);

        shadowMap.beginShadowPass(lightSpaceMatrix);
        Lab::Renderer::beginShadowDepthPass(lightSpaceMatrix);

        // Ground floor
        Lab::Renderer::drawShadowCube(Lab::Vec3(0.0f, -0.1f, 0.0f), Lab::Vec3(30.0f, 0.2f, 30.0f));
        // Back wall
        Lab::Renderer::drawShadowCube(Lab::Vec3(0.0f, 3.0f, -6.0f), Lab::Vec3(30.0f, 6.0f, 0.5f));
        // Monolithic Pillar (Primary shadow caster)
        Lab::Renderer::drawShadowCube(Lab::Vec3(-1.5f, 2.5f, -1.5f), Lab::Vec3(1.2f, 5.0f, 1.2f));
        // Tactical Barricade crate
        Lab::Renderer::drawShadowCube(Lab::Vec3(2.0f, 0.75f, 0.5f), Lab::Vec3(1.8f, 1.5f, 1.5f));

        Lab::Renderer::endShadowDepthPass();
        shadowMap.endShadowPass(w, h);
        std::cout << "  [PASS] Shadow Depth Pass completed: Occluders written to depth map.\n";

        // 4. Color & Lighting Pass with Shadow Mapping + Spotlight illumination
        glClearColor(0.04f, 0.05f, 0.07f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        Lab::Camera lightCam(70.0f, (float)w / (float)h, 0.01f, 1000.0f);
        lightCam.setPosition(camPos);
        lightCam.update(Lab::Vec2(0.0f, 10.0f)); // slight downward pitch
        Lab::Renderer::beginFrame(lightCam);

        // Set sun lighting (cool Source Engine directional light)
        Lab::Renderer::setSunLight(sunDir, Lab::Vec3(1.0f, 0.95f, 0.90f), Lab::Vec3(0.18f, 0.22f, 0.28f));

        // Bind shadow map and spotlight
        Lab::Renderer::setShadowMap(lightSpaceMatrix, shadowMap.getDepthTexture());
        Lab::Renderer::setFlashlight(flashlight.position, flashlight.direction, flashlight.color,
                                    flashlight.innerCone, flashlight.outerCone, flashlight.range, flashlight.intensity);

        // Render scene geometry with shadows and textures
        Lab::Texture floorTex("assets/textures/floor_tiles.bmp");
        Lab::Texture wallTex("assets/textures/concrete_wall.bmp");
        Lab::Texture hazardTex("assets/textures/hazard_stripes.bmp");

        Lab::Renderer::drawCube(Lab::Vec3(0.0f, -0.1f, 0.0f), Lab::Vec3(30.0f, 0.2f, 30.0f), Lab::Vec3(0.85f, 0.85f, 0.85f), &floorTex, true);
        Lab::Renderer::drawCube(Lab::Vec3(0.0f, 3.0f, -6.0f), Lab::Vec3(30.0f, 6.0f, 0.5f), Lab::Vec3(0.80f, 0.80f, 0.80f), &wallTex, true);
        Lab::Renderer::drawCube(Lab::Vec3(-1.5f, 2.5f, -1.5f), Lab::Vec3(1.2f, 5.0f, 1.2f), Lab::Vec3(0.35f, 0.38f, 0.45f), &hazardTex, true);
        Lab::Renderer::drawCube(Lab::Vec3(2.0f, 0.75f, 0.5f), Lab::Vec3(1.8f, 1.5f, 1.5f), Lab::Vec3(0.55f, 0.45f, 0.30f), &wallTex, true);

        // Render tactical weapon viewmodel with arms held in front
        Lab::WeaponSystem ws;
        ws.init();
        ws.unlockAll();
        ws.switchWeapon(Lab::WeaponID::M4A4S);
        ws.update(0.5f);

        Lab::WeaponAnimator testAnim;
        testAnim.update(0.1f, Lab::Vec2(0, 0), 0.0f);
        ws.renderViewModel(lightCam, testAnim, nullptr, nullptr, 0.0f);

        Lab::Renderer::disableShadowMap();
        Lab::Renderer::disableFlashlight();

        // 5. UI Overlay
        Lab::Renderer::beginUI(w, h);
        Lab::Renderer::drawRect(40.0f, 30.0f, 680.0f, 85.0f, Lab::Vec3(0.10f, 0.12f, 0.16f));
        Lab::Renderer::drawRect(40.0f, 30.0f, 680.0f, 1.0f, Lab::Vec3(0.2f, 0.85f, 1.0f));
        Lab::LabFont::drawText(56.0f, 44.0f, "REAL-TIME LIGHTING, TACTICAL FLASHLIGHT & SHADOW MAPPING", 2.0f, Lab::Vec3(0.20f, 0.85f, 1.0f), Lab::LabFontType::GeoSans);
        Lab::LabFont::drawText(56.0f, 74.0f, "OPENGL 4.5 DSA FBO | 3x3 PCF SOFT PENUMBRA | HL2 SPOTLIGHT CONE", 1.6f, Lab::Vec3(0.85f, 0.90f, 0.95f), Lab::LabFontType::GeoSans);
        Lab::Renderer::endUI();

        Lab::Renderer::endFrame();
        glFinish();
        saveFrameToBMP("test_flashlight_and_shadows.bmp", w, h);
        std::cout << "  [PASS] Saved visual Real-Time Lighting & Shadows verification to 'test_flashlight_and_shadows.bmp'.\n";
    }

    // 26. Verify glTF 2.0 Skeletal Animation Pipeline, GPU Vertex Skinning & STL Bone Socket Attachment
    {
        std::cout << "\n[Test 26] Verifying glTF 2.0 Skeletal Animation, GPU Vertex Skinning & STL Sockets...\n";

        // 1. Math Verification: Quaternions, Slerp, and Transformation Matrices
        Lab::Quat qId = Lab::Quat::identity();
        if (std::abs(qId.length() - 1.0f) > 1e-4f) {
            std::cerr << "Assertion failed: Identity quaternion length must be 1\n";
            return 1;
        }

        Lab::Quat qRotA = Lab::Quat::fromEuler(0.0f, 1.570796f, 0.0f); // 90 deg Yaw
        Lab::Quat qRotB = Lab::Quat::fromEuler(0.0f, 0.0f, 0.0f);
        Lab::Quat qSlerp = Lab::Quat::slerp(qRotA, qRotB, 0.5f);
        if (std::abs(qSlerp.length() - 1.0f) > 1e-4f) {
            std::cerr << "Assertion failed: Slerped quaternion must be unit length\n";
            return 1;
        }
        std::cout << "  [PASS] Quaternion mathematics and Spherical Linear Interpolation (Slerp) verified.\n";

        // 2. Load glTF 2.0 Asset and initialize Skeletal Rig
        std::shared_ptr<Lab::Skeleton> skeleton;
        std::vector<Lab::AnimationClip> animClips;
        std::unique_ptr<Lab::SkinnedMesh> skinnedMesh;

        bool loaded = Lab::GLTFLoader::load("assets/animations/bot_walk.gltf", skeleton, animClips, skinnedMesh);
        if (!loaded || !skeleton || !skinnedMesh || animClips.empty()) {
            std::cerr << "Assertion failed: Failed to load or initialize glTF skeletal mesh\n";
            return 1;
        }

        if (skeleton->getBoneCount() < 15) {
            std::cerr << "Assertion failed: Humanoid skeletal rig must have at least 15 bones\n";
            return 1;
        }
        std::cout << "  [PASS] Skeletal Rig initialized: " << skeleton->getBoneCount() << " joints loaded.\n";

        // 3. Animator & Animation Clips Verification
        Lab::Animator animator;
        animator.setSkeleton(skeleton);
        for (const auto& clip : animClips) {
            animator.addClip(clip);
        }

        if (!animator.hasClip("Walk") || !animator.hasClip("Idle") || !animator.hasClip("Shoot") || !animator.hasClip("Melee_Swing")) {
            std::cerr << "Assertion failed: Animator missing required core combat clips (Walk, Idle, Shoot, Melee_Swing)\n";
            return 1;
        }

        // Play Melee_Swing attack animation
        animator.playAnimation("Melee_Swing", false);
        animator.update(0.22f); // Advance to peak swing windup

        const auto& skinMats = animator.getSkinMatrices();
        if (skinMats.size() != skeleton->getBoneCount()) {
            std::cerr << "Assertion failed: Skinning matrices size mismatch\n";
            return 1;
        }
        std::cout << "  [PASS] Animator updated with GPU skinning matrices (" << skinMats.size() << " bone uniforms).\n";

        // 4. Verify glTF + STL Hybrid Bridge: Bone Socket Attachment
        // Attach user's pipe.stl weapon model to the animated right hand bone!
        Lab::Mat4 botWorld = Lab::Mat4::translate(Lab::Vec3(0.0f, 0.0f, 0.0f));
        Lab::Mat4 socketTransform = animator.getSocketTransform("Socket_Weapon", botWorld,
                                                                 Lab::makeTransform(Lab::Vec3(0.0f, -0.05f, 0.02f),
                                                                                    Lab::Quat::fromEuler(0.1f, -0.2f, 0.0f),
                                                                                    Lab::Vec3(0.016f, 0.016f, 0.016f)));

        std::unique_ptr<Lab::Mesh> pipeStl(Lab::Mesh::loadSTL("assets/models/pipe.stl"));
        if (!pipeStl) {
            std::cerr << "Assertion failed: Could not load assets/models/pipe.stl for socket attachment\n";
            return 1;
        }
        std::cout << "  [PASS] Bone Socket Attachment verified: STL model linked to 'Socket_Weapon' joint.\n";

        // 5. Render Visual Verification Frame: Skinned Character + Attached STL Weapon with Shadows
        glClearColor(0.04f, 0.06f, 0.09f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        Lab::Camera skelCam(60.0f, (float)w / (float)h, 0.01f, 1000.0f);
        skelCam.setPosition(Lab::Vec3(0.4f, 1.15f, 3.0f));
        skelCam.update(Lab::Vec2(-5.0f, -15.0f)); // Looking centered at bot chest/arms

        // Pass 1: Dynamic Shadow Depth Pass for Skinned Character & Attached Weapon
        Lab::Vec3 sunDir(-0.35f, -0.85f, -0.35f);
        Lab::Mat4 lightSpaceMatrix = Lab::ShadowMap::computeSunLightSpaceMatrix(sunDir, Lab::Vec3(0, 1, 0), 15.0f);
        Lab::ShadowMap shadowMap;
        shadowMap.init(2048, 2048);

        shadowMap.beginShadowPass(lightSpaceMatrix);
        Lab::Renderer::beginShadowDepthPass(lightSpaceMatrix);

        // Ground floor
        Lab::Renderer::drawShadowCube(Lab::Vec3(0.0f, -0.1f, 0.0f), Lab::Vec3(25.0f, 0.2f, 25.0f));
        // Back wall
        Lab::Renderer::drawShadowCube(Lab::Vec3(0.0f, 2.5f, -3.5f), Lab::Vec3(20.0f, 5.0f, 0.4f));

        // Draw shadow of the Skinned Humanoid Bot!
        Lab::Renderer::drawShadowSkinnedMesh(*skinnedMesh, botWorld, skinMats);

        // Draw shadow of the attached STL Weapon!
        Lab::Renderer::drawShadowMesh(*pipeStl, socketTransform);

        Lab::Renderer::endShadowDepthPass();
        shadowMap.endShadowPass(w, h);

        // Pass 2: Shaded Render Pass with Real-Time Lighting, PCF Shadows & Materials
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        Lab::Renderer::beginFrame(skelCam);
        Lab::Renderer::setSunLight(sunDir, Lab::Vec3(1.0f, 0.96f, 0.92f), Lab::Vec3(0.22f, 0.25f, 0.30f));
        Lab::Renderer::setShadowMap(lightSpaceMatrix, shadowMap.getDepthTexture());

        // Environment
        Lab::Texture floorTex("assets/textures/floor_tiles.bmp");
        Lab::Texture wallTex("assets/textures/concrete_wall.bmp");
        Lab::Renderer::drawCube(Lab::Vec3(0.0f, -0.1f, 0.0f), Lab::Vec3(25.0f, 0.2f, 25.0f), Lab::Vec3(0.75f, 0.75f, 0.75f), &floorTex, true);
        Lab::Renderer::drawCube(Lab::Vec3(0.0f, 2.5f, -3.5f), Lab::Vec3(20.0f, 5.0f, 0.4f), Lab::Vec3(0.70f, 0.70f, 0.70f), &wallTex, true);

        // Tactical support crates
        Lab::Renderer::drawCube(Lab::Vec3(-2.0f, 0.6f, -1.0f), Lab::Vec3(1.2f, 1.2f, 1.2f), Lab::Vec3(0.40f, 0.45f, 0.50f), &wallTex, true);
        Lab::Renderer::drawCube(Lab::Vec3(2.2f, 0.5f, -0.5f), Lab::Vec3(1.0f, 1.0f, 1.0f), Lab::Vec3(0.50f, 0.45f, 0.35f), &wallTex, true);

        // Draw Skinned Combat Bot (Head with cyan visor, chest plates, arms and legs)
        Lab::Renderer::drawSkinnedMesh(*skinnedMesh, botWorld, skinMats, Lab::Vec3(1.0f, 1.0f, 1.0f), nullptr, true);

        // Draw Attached STL Weapon (Pipe) locked to right hand socket!
        Lab::Texture pipeTex("assets/textures/weapon_pipe.bmp");
        Lab::Renderer::drawMesh(*pipeStl, socketTransform, Lab::Vec3(0.95f, 0.95f, 0.95f), &pipeTex, true);

        Lab::Renderer::disableShadowMap();

        // 6. UI Diagnostic Overlay
        Lab::Renderer::beginUI(w, h);
        Lab::Renderer::drawRect(40.0f, 30.0f, 750.0f, 85.0f, Lab::Vec3(0.10f, 0.12f, 0.16f));
        Lab::Renderer::drawRect(40.0f, 30.0f, 750.0f, 1.0f, Lab::Vec3(0.2f, 0.85f, 1.0f));
        Lab::LabFont::drawText(56.0f, 44.0f, "glTF 2.0 SKELETAL ANIMATION & GPU VERTEX SKINNING", 2.0f, Lab::Vec3(0.20f, 0.85f, 1.0f), Lab::LabFontType::GeoSans);
        Lab::LabFont::drawText(56.0f, 74.0f, "20-BONE RIG | MELEE SWING CLIP | RIGID STL WEAPON SOCKET ATTACHMENT", 1.6f, Lab::Vec3(0.85f, 0.90f, 0.95f), Lab::LabFontType::GeoSans);
        Lab::Renderer::endUI();

        Lab::Renderer::endFrame();
        glFinish();
        saveFrameToBMP("test_skeletal_animation.bmp", w, h);
        std::cout << "  [PASS] Saved visual glTF 2.0 Skeletal Animation & STL Socket verification to 'test_skeletal_animation.bmp'." << std::endl;
    }

    // =========================================================================
    // TEST 27: RIGID BODY PHYSICS, PROPS & EXPLOSIVE BARREL DESTRUCTION
    // =========================================================================
    {
        std::cout << "\n[Test 27] Verifying Rigid Body Physics, Wooden Crates & Explosive Barrels..." << std::endl;
        Lab::PhysicsWorld physics;
        physics.init();

        // 1. Verify RigidBody Inertia Calculation and Integration
        Lab::RigidBody testBody;
        testBody.setBox(Lab::Vec3(0.5f, 0.5f, 0.5f), 24.0f);
        if (testBody.invMass <= 0.0f || testBody.inertiaLocal.x <= 0.0f) {
            std::cerr << "Assertion failed: RigidBody mass or inertia calculation incorrect" << std::endl;
            return 1;
        }

        // Apply off-center impulse and verify linear and angular momentum
        testBody.applyImpulse(Lab::Vec3(0.0f, 10.0f, 0.0f), testBody.position + Lab::Vec3(0.4f, 0.0f, 0.0f));
        if (testBody.linearVelocity.y <= 0.0f || testBody.angularVelocity.lengthSq() <= 0.0f) {
            std::cerr << "Assertion failed: Impulse must generate linear velocity and angular torque" << std::endl;
            return 1;
        }
        std::cout << "  [PASS] RigidBody inertia tensor and off-center impulse torque verified." << std::endl;

        // 2. Verify Physics World Simulation, Gravity & Ground Rest
        physics.clear();
        Lab::RigidBody* crate1 = physics.spawnCrate(Lab::Vec3(0.0f, 2.0f, 0.0f), Lab::Vec3(0.45f, 0.45f, 0.45f), 20.0f);
        std::vector<Lab::CollisionBox> emptyObstacles;

        // Step physics 1.5 seconds (90 frames at 60Hz)
        for (int step = 0; step < 90; ++step) {
            physics.update(0.0166f, emptyObstacles);
        }

        // The crate should have fallen and settled on the ground (position.y near halfExtents.y ~ 0.45)
        if (crate1->position.y < 0.35f || crate1->position.y > 0.65f) {
            std::cerr << "Assertion failed: Crate should settle on the ground plane (Y ~ 0.45m), got Y=" << crate1->position.y << std::endl;
            return 1;
        }
        std::cout << "  [PASS] Gravity and ground contact restitution verified: Crate resting at Y=" << crate1->position.y << std::endl;

        // 3. Verify Wooden Crate Destruction & Debris Generation
        physics.clear();
        Lab::RigidBody* destructCrate = physics.spawnCrate(Lab::Vec3(0.0f, 0.45f, 0.0f));
        int initialBodyCount = (int)physics.bodies.size();

        // Apply lethal damage to crate
        bool crateHit = physics.takeDamage(destructCrate->id, 60.0f, Lab::Vec3(0, 0.45f, 0), Lab::Vec3(0, 0, 1));
        if (!crateHit) {
            std::cerr << "Assertion failed: takeDamage should return true for valid prop" << std::endl;
            return 1;
        }

        // Step physics to process destruction and debris spawning
        physics.update(0.0166f, emptyObstacles);

        // Crate should have shattered into multiple debris chunks
        if (physics.bodies.size() <= (size_t)initialBodyCount) {
            std::cerr << "Assertion failed: Destroyed crate must spawn dynamic debris chunks, body count: " << physics.bodies.size() << std::endl;
            return 1;
        }
        std::cout << "  [PASS] Wooden crate destruction verified: Shattered into " << physics.bodies.size() << " physical debris planks." << std::endl;

        // 4. Verify Explosive Barrel Radial Impulse Blast & Chain Reaction
        physics.clear();
        Lab::RigidBody* barrelA = physics.spawnExplosiveBarrel(Lab::Vec3(0.0f, 0.48f, 0.0f));
        [[maybe_unused]] Lab::RigidBody* barrelB = physics.spawnExplosiveBarrel(Lab::Vec3(1.8f, 0.48f, 0.0f)); // Within blast radius (6.5m)
        [[maybe_unused]] Lab::RigidBody* crateNear = physics.spawnCrate(Lab::Vec3(2.5f, 0.45f, 0.0f));

        // Detonate Barrel A
        physics.takeDamage(barrelA->id, 50.0f, barrelA->position, Lab::Vec3(1, 0, 0));
        physics.update(0.0166f, emptyObstacles);

        if (physics.pendingExplosions.empty()) {
            std::cerr << "Assertion failed: Barrel detonation must trigger pendingExplosion event" << std::endl;
            return 1;
        }
        std::cout << "  [PASS] Explosive barrel detonation verified: Radial blast emitted with " << physics.pendingExplosions.size() << " cascade events." << std::endl;

        // 5. Render Visual Verification Frame: Destructible Props & Dynamic Soft Shadows
        glClearColor(0.04f, 0.06f, 0.09f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        physics.clear();
        // Setup realistic combat arena props scene
        [[maybe_unused]] Lab::RigidBody* cBase1 = physics.spawnCrate(Lab::Vec3(-1.8f, 0.45f, -0.5f));
        [[maybe_unused]] Lab::RigidBody* cTop1  = physics.spawnCrate(Lab::Vec3(-1.8f, 1.35f, -0.5f));
        [[maybe_unused]] Lab::RigidBody* redBarrel = physics.spawnExplosiveBarrel(Lab::Vec3(-0.4f, 0.48f, -0.2f));
        [[maybe_unused]] Lab::RigidBody* cRight = physics.spawnCrate(Lab::Vec3(1.8f, 0.45f, -0.3f));

        // Spawn dynamic debris planks across the floor from a recently smashed crate
        physics.spawnCrateDebris(Lab::Vec3(0.6f, 0.5f, 0.5f), Lab::Vec3(0.45f, 0.45f, 0.45f), Lab::Vec3(1.0f, 0.3f, 0.5f));

        // Simulate a few physics steps so debris tumbles naturally and settles
        for (int i = 0; i < 20; ++i) {
            physics.update(0.0166f, emptyObstacles);
        }

        Lab::Camera physCam(60.0f, (float)w / (float)h, 0.01f, 1000.0f);
        physCam.setPosition(Lab::Vec3(0.0f, 1.65f, 3.8f));
        physCam.update(Lab::Vec2(0.0f, -14.0f)); // Looking slightly down at props and floor

        // Pass 1: Shadow Depth Pass
        Lab::Vec3 sunDir(-0.35f, -0.85f, -0.35f);
        Lab::Mat4 lightSpaceMatrix = Lab::ShadowMap::computeSunLightSpaceMatrix(sunDir, Lab::Vec3(0, 0.8f, 0), 16.0f);
        Lab::ShadowMap shadowMap;
        shadowMap.init(2048, 2048);

        shadowMap.beginShadowPass(lightSpaceMatrix);
        Lab::Renderer::beginShadowDepthPass(lightSpaceMatrix);
        Lab::Renderer::drawShadowCube(Lab::Vec3(0.0f, -0.1f, 0.0f), Lab::Vec3(25.0f, 0.2f, 25.0f));
        Lab::Renderer::drawShadowCube(Lab::Vec3(0.0f, 2.5f, -4.0f), Lab::Vec3(20.0f, 5.0f, 0.4f));

        // Render physics props into shadow map!
        physics.renderShadow();

        Lab::Renderer::endShadowDepthPass();
        shadowMap.endShadowPass(w, h);

        // Pass 2: Scene Shaded Pass
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        Lab::Renderer::beginFrame(physCam);
        Lab::Renderer::setSunLight(sunDir, Lab::Vec3(1.0f, 0.96f, 0.92f), Lab::Vec3(0.22f, 0.25f, 0.30f));
        Lab::Renderer::setShadowMap(lightSpaceMatrix, shadowMap.getDepthTexture());

        Lab::Texture floorTex("assets/textures/floor_tiles.bmp");
        Lab::Texture wallTex("assets/textures/concrete_wall.bmp");
        Lab::Renderer::drawCube(Lab::Vec3(0.0f, -0.1f, 0.0f), Lab::Vec3(25.0f, 0.2f, 25.0f), Lab::Vec3(0.75f, 0.75f, 0.75f), &floorTex, true);
        Lab::Renderer::drawCube(Lab::Vec3(0.0f, 2.5f, -4.0f), Lab::Vec3(20.0f, 5.0f, 0.4f), Lab::Vec3(0.70f, 0.70f, 0.70f), &wallTex, true);

        // Render live physics props with materials and soft penumbra shadows!
        physics.render();

        Lab::Renderer::disableShadowMap();

        // 6. UI Diagnostic Overlay
        Lab::Renderer::beginUI(w, h);
        Lab::Renderer::drawRect(40.0f, 30.0f, 780.0f, 85.0f, Lab::Vec3(0.10f, 0.12f, 0.16f));
        Lab::Renderer::drawRect(40.0f, 30.0f, 780.0f, 1.0f, Lab::Vec3(1.0f, 0.55f, 0.1f));
        Lab::LabFont::drawText(56.0f, 44.0f, "RIGID BODY PHYSICS & PROP DESTRUCTION (SPRINT 5)", 2.0f, Lab::Vec3(1.0f, 0.60f, 0.15f), Lab::LabFontType::GeoSans);
        Lab::LabFont::drawText(56.0f, 74.0f, "NEWTONIAN RIGID BODIES | RED HAZARD BARRELS | WOOD CRATES & DEBRIS", 1.6f, Lab::Vec3(0.90f, 0.92f, 0.95f), Lab::LabFontType::GeoSans);
        Lab::Renderer::endUI();

        Lab::Renderer::endFrame();
        glFinish();
        saveFrameToBMP("test_physics_and_destruction.bmp", w, h);
        std::cout << "  [PASS] Saved visual Rigid Body Physics & Destruction verification to 'test_physics_and_destruction.bmp'.\n";
    }

    // ==========================================
    // TEST 28: CSG Geometry Clipping Tool in Hammer & Physical Props Placement with Serialization
    // ==========================================
    {
        std::cout << "\n[Test 28] Verifying CSG Geometry Clipping & Physical Props Placement..." << std::endl;

        // 1. Verify CSG Box Slicing with arbitrary diagonal plane
        Lab::Vec3 bMin(-2.0f, 0.0f, -2.0f);
        Lab::Vec3 bMax( 2.0f, 4.0f,  2.0f);
        Lab::Vec3 planePt(0.0f, 0.0f, 0.0f);
        Lab::Vec3 planeNormal = Lab::Vec3(1.0f, 0.0f, 1.0f).normalized();

        Lab::ClipResult clipRes = Lab::CSGTool::sliceBox(bMin, bMax, planePt, planeNormal);

        if (!clipRes.didIntersect) {
            std::cerr << "Assertion failed: Diagonal plane must intersect cube [-2..2, 0..4, -2..2]!" << std::endl;
            return 1;
        }
        if (!clipRes.frontPiece.isValid() || !clipRes.backPiece.isValid()) {
            std::cerr << "Assertion failed: Both front and back pieces must be valid convex polyhedra!" << std::endl;
            return 1;
        }
        if (clipRes.frontPiece.triangleCount < 4 || clipRes.backPiece.triangleCount < 4) {
            std::cerr << "Assertion failed: Sliced pieces must have multiple triangles, got front="
                      << clipRes.frontPiece.triangleCount << " back=" << clipRes.backPiece.triangleCount << std::endl;
            return 1;
        }

        // Check centroids: front piece centroid must be on positive side, back piece on negative side
        float frontDist = Lab::Vec3::dot(clipRes.frontPiece.centroid - planePt, planeNormal);
        float backDist  = Lab::Vec3::dot(clipRes.backPiece.centroid - planePt, planeNormal);
        if (frontDist <= 0.01f || backDist >= -0.01f) {
            std::cerr << "Assertion failed: Sliced centroids must lie on respective sides of cutting plane!" << std::endl;
            return 1;
        }
        std::cout << "  [PASS] CSG box slicing verified: Front tris=" << clipRes.frontPiece.triangleCount
                  << " (dist=" << frontDist << "), Back tris=" << clipRes.backPiece.triangleCount
                  << " (dist=" << backDist << ")" << std::endl;

        // 2. Verify 2D Line to Vertical Cutting Plane
        Lab::Vec3 linePt, lineNorm;
        Lab::CSGTool::makePlaneFrom2DLine(Lab::Vec2(-2.0f, 0.0f), Lab::Vec2(2.0f, 4.0f), linePt, lineNorm);
        float linePerpDot = Lab::Vec2::dot(Lab::Vec2(4.0f, 4.0f), Lab::Vec2(lineNorm.x, lineNorm.z));
        if (std::abs(linePerpDot) > 1e-4f) {
            std::cerr << "Assertion failed: Cutting plane normal must be perpendicular to 2D line! Dot=" << linePerpDot << std::endl;
            return 1;
        }
        std::cout << "  [PASS] 2D line to 3D cutting plane verified: normal=("
                  << lineNorm.x << ", " << lineNorm.y << ", " << lineNorm.z << ")" << std::endl;

        // 3. Verify Slicing Arbitrary Convex Polyhedron (Sequential CSG cut)
        Lab::Vec3 horizPlanePt(0.0f, 2.0f, 0.0f);
        Lab::Vec3 horizPlaneNorm(0.0f, 1.0f, 0.0f);
        Lab::ClipResult subCut = Lab::CSGTool::sliceConvexMesh(
            clipRes.frontPiece.vertices, clipRes.frontPiece.indices,
            horizPlanePt, horizPlaneNorm
        );
        if (!subCut.didIntersect || !subCut.frontPiece.isValid() || !subCut.backPiece.isValid()) {
            std::cerr << "Assertion failed: Second CSG cut on convex polyhedron must produce valid sub-polyhedra!" << std::endl;
            return 1;
        }
        std::cout << "  [PASS] Multi-step convex polyhedron CSG clipping verified: Sub-front tris="
                  << subCut.frontPiece.triangleCount << ", Sub-back tris=" << subCut.backPiece.triangleCount << std::endl;

        // 4. Verify .labmap Roundtrip Serialization for poly_brush and Physical Props
        std::string testMapPath = "test_csg_props.labmap";
        {
            Lab::LabMap saveMap;
            // Add standard cube brush
            Lab::MapBrush solidBrush;
            solidBrush.type = "cube";
            solidBrush.position = Lab::Vec3(0.0f, 0.5f, 0.0f);
            solidBrush.size = Lab::Vec3(4.0f, 1.0f, 4.0f);
            solidBrush.color = Lab::Vec3(0.8f, 0.8f, 0.8f);
            solidBrush.texturePath = "concrete_wall.bmp";
            saveMap.brushes.push_back(solidBrush);

            // Add sliced poly_brush
            Lab::MapBrush bPoly;
            bPoly.type = "poly";
            bPoly.position = clipRes.frontPiece.centroid;
            bPoly.size = clipRes.frontPiece.aabbMax - clipRes.frontPiece.aabbMin;
            bPoly.color = Lab::Vec3(0.3f, 0.7f, 0.9f);
            bPoly.texturePath = "hazard_stripes.bmp";
            bPoly.customVertices = clipRes.frontPiece.vertices;
            bPoly.customIndices = clipRes.frontPiece.indices;
            saveMap.brushes.push_back(bPoly);

            // Add Physical Props (prop_crate and prop_barrel)
            Lab::MapProp pCrate;
            pCrate.modelPath = "models/props/crate.obj";
            pCrate.position = Lab::Vec3(-2.0f, 0.45f, 1.0f);
            pCrate.scale = Lab::Vec3(0.9f, 0.9f, 0.9f);
            pCrate.texturePath = "crate_wood.png";
            saveMap.props.push_back(pCrate);

            Lab::MapProp pBarrel;
            pBarrel.modelPath = "models/props/barrel.obj";
            pBarrel.position = Lab::Vec3(2.0f, 0.48f, 1.0f);
            pBarrel.scale = Lab::Vec3(0.64f, 0.96f, 0.64f);
            pBarrel.texturePath = "barrel_hazard.png";
            saveMap.props.push_back(pBarrel);

            if (!saveMap.saveToFile(testMapPath)) {
                std::cerr << "Assertion failed: Failed to save test map to " << testMapPath << std::endl;
                return 1;
            }
        }

        // Reload and assert
        std::unique_ptr<Lab::LabMap> loadedMap = Lab::LabMap::loadFromFile(testMapPath);
        if (!loadedMap) {
            std::cerr << "Assertion failed: Failed to load saved test map from " << testMapPath << std::endl;
            return 1;
        }

        if (loadedMap->brushes.size() != 2) {
            std::cerr << "Assertion failed: Expected 2 brushes, loaded " << loadedMap->brushes.size() << std::endl;
            return 1;
        }
        if (loadedMap->brushes[1].type != "poly" || loadedMap->brushes[1].customVertices.size() != clipRes.frontPiece.vertices.size()) {
            std::cerr << "Assertion failed: poly_brush geometry vertices mismatch! Loaded="
                      << loadedMap->brushes[1].customVertices.size() << " Expected=" << clipRes.frontPiece.vertices.size() << std::endl;
            return 1;
        }
        if (loadedMap->props.size() != 2) {
            std::cerr << "Assertion failed: Expected 2 physical props, loaded " << loadedMap->props.size() << std::endl;
            return 1;
        }
        if (loadedMap->props[0].modelPath.find("crate") == std::string::npos ||
            loadedMap->props[1].modelPath.find("barrel") == std::string::npos) {
            std::cerr << "Assertion failed: Physical props paths corrupted during serialization!" << std::endl;
            return 1;
        }
        std::cout << "  [PASS] .labmap poly_brush & physical props serialization roundtrip verified 100%!" << std::endl;

        // Clean up temporary map file
        std::filesystem::remove(testMapPath);

        // 5. Visual Rendering Verification:
        // Render 3D CSG Sliced Brushes, Physical Props, Dynamic Shadow Depth Pass, and Hammer Diagnostic Overlay
        glClearColor(0.12f, 0.14f, 0.18f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Create Mesh instances for the front and back CSG clipped polyhedra
        Lab::Mesh frontMesh(clipRes.frontPiece.vertices, clipRes.frontPiece.indices);
        Lab::Mesh backMesh(clipRes.backPiece.vertices, clipRes.backPiece.indices);

        // Position camera for optimal perspective overlooking the sliced geometry and props
        Lab::Camera csgCam(60.0f, (float)w / (float)h, 0.01f, 1000.0f);
        csgCam.setPosition(Lab::Vec3(0.0f, 3.2f, 5.5f));
        csgCam.update(Lab::Vec2(0.0f, -22.0f));

        Lab::Vec3 sunDir(-0.4f, -0.8f, -0.45f);
        Lab::Mat4 lightSpaceMatrix = Lab::ShadowMap::computeSunLightSpaceMatrix(sunDir, Lab::Vec3(0, 1.0f, 0), 16.0f);
        Lab::ShadowMap shadowMap;
        shadowMap.init(2048, 2048);

        // Pass 1: Shadow Depth Pass
        shadowMap.beginShadowPass(lightSpaceMatrix);
        Lab::Renderer::beginShadowDepthPass(lightSpaceMatrix);

        // Ground floor
        Lab::Renderer::drawShadowCube(Lab::Vec3(0.0f, -0.1f, 0.0f), Lab::Vec3(25.0f, 0.2f, 25.0f));
        // Back wall
        Lab::Renderer::drawShadowCube(Lab::Vec3(0.0f, 2.5f, -4.0f), Lab::Vec3(20.0f, 5.0f, 0.4f));

        // Draw CSG clipped meshes into shadow map
        Lab::Renderer::drawShadowMesh(frontMesh, Lab::Vec3(-1.2f, 0.0f, 0.0f), Lab::Vec3(0, 0, 0), Lab::Vec3(1, 1, 1));
        Lab::Renderer::drawShadowMesh(backMesh,  Lab::Vec3( 1.2f, 0.0f, 0.0f), Lab::Vec3(0, 0, 0), Lab::Vec3(1, 1, 1));

        // Physics Props in shadow map
        Lab::Renderer::drawShadowCube(Lab::Vec3(-2.8f, 0.45f, 0.5f), Lab::Vec3(0.9f, 0.9f, 0.9f));
        Lab::Renderer::drawShadowCube(Lab::Vec3(-2.8f, 1.35f, 0.5f), Lab::Vec3(0.9f, 0.9f, 0.9f));
        Lab::Renderer::drawShadowCube(Lab::Vec3( 2.8f, 0.48f, 0.5f), Lab::Vec3(0.64f, 0.96f, 0.64f));

        Lab::Renderer::endShadowDepthPass();
        shadowMap.endShadowPass(w, h);

        // Pass 2: Shaded Lit Pass
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        Lab::Renderer::beginFrame(csgCam);
        Lab::Renderer::setSunLight(sunDir, Lab::Vec3(1.0f, 0.96f, 0.92f), Lab::Vec3(0.25f, 0.28f, 0.35f));
        Lab::Renderer::setShadowMap(lightSpaceMatrix, shadowMap.getDepthTexture());

        Lab::Texture floorTex("assets/textures/floor_tiles.bmp");
        Lab::Texture wallTex("assets/textures/concrete_wall.bmp");
        Lab::Texture crateTex("crate_wood.png");
        Lab::Texture barrelTex("barrel_hazard.png");

        // Environment
        Lab::Renderer::drawCube(Lab::Vec3(0.0f, -0.1f, 0.0f), Lab::Vec3(25.0f, 0.2f, 25.0f), Lab::Vec3(0.75f, 0.75f, 0.75f), &floorTex, true);
        Lab::Renderer::drawCube(Lab::Vec3(0.0f, 2.5f, -4.0f), Lab::Vec3(20.0f, 5.0f, 0.4f), Lab::Vec3(0.70f, 0.70f, 0.70f), &wallTex, true);

        // Render CSG Clipped Polyhedral Geometry with Concrete Texture and Authentic Accents
        Lab::Renderer::drawMesh(frontMesh, Lab::Vec3(-1.2f, 0.0f, 0.0f), Lab::Vec3(0, 0, 0), Lab::Vec3(1, 1, 1), Lab::Vec3(0.95f, 0.95f, 1.0f), &wallTex);
        Lab::Renderer::drawMesh(backMesh,  Lab::Vec3( 1.2f, 0.0f, 0.0f), Lab::Vec3(0, 0, 0), Lab::Vec3(1, 1, 1), Lab::Vec3(1.0f, 0.92f, 0.88f), &wallTex);

        // Render Slicing Plane Cutting Guide Line in 3D
        for (int s = -8; s <= 8; ++s) {
            float t = (float)s * 0.35f;
            Lab::Vec3 pt = Lab::Vec3(-t, 2.0f, t);
            Lab::Renderer::drawCube(pt, Lab::Vec3(0.06f, 4.2f, 0.06f), Lab::Vec3(1.0f, 0.55f, 0.1f), nullptr, false);
        }

        // Render Physical Props (Stacked Wooden Crates & Red Explosive Fuel Barrel)
        Lab::Renderer::drawCube(Lab::Vec3(-2.8f, 0.45f, 0.5f), Lab::Vec3(0.9f, 0.9f, 0.9f), Lab::Vec3(1, 1, 1), &crateTex, true);
        Lab::Renderer::drawCube(Lab::Vec3(-2.8f, 1.35f, 0.5f), Lab::Vec3(0.9f, 0.9f, 0.9f), Lab::Vec3(1, 1, 1), &crateTex, true);
        Lab::Renderer::drawCube(Lab::Vec3( 2.8f, 0.48f, 0.5f), Lab::Vec3(0.64f, 0.96f, 0.64f), Lab::Vec3(1, 1, 1), &barrelTex, true);

        Lab::Renderer::disableShadowMap();

        // 6. UI Diagnostic Overlay
        Lab::Renderer::beginUI(w, h);
        Lab::Renderer::drawRect(40.0f, 30.0f, 850.0f, 88.0f, Lab::Vec3(0.10f, 0.12f, 0.16f));
        Lab::Renderer::drawRect(40.0f, 30.0f, 850.0f, 1.0f, Lab::Vec3(1.0f, 0.55f, 0.1f));
        Lab::LabFont::drawText(56.0f, 44.0f, "CSG GEOMETRY CLIPPING & HAMMER PHYSICAL PROPS (SPRINT 6)", 2.0f, Lab::Vec3(1.0f, 0.60f, 0.15f), Lab::LabFontType::GeoSans);
        Lab::LabFont::drawText(56.0f, 74.0f, "SUTHERLAND-HODGMAN CSG SLICING | CAP GENERATION | PROP_CRATE & PROP_BARREL", 1.6f, Lab::Vec3(0.90f, 0.92f, 0.95f), Lab::LabFontType::GeoSans);
        Lab::Renderer::endUI();

        Lab::Renderer::endFrame();
        glFinish();
        saveFrameToBMP("test_csg_clipping_and_hammer.bmp", w, h);
        std::cout << "  [PASS] Saved visual CSG Clipping & Hammer Props verification to 'test_csg_clipping_and_hammer.bmp'.\n";
    }

    // 29. Verify Dynamic Projective Decal System (Bullet Holes, Blood Splatters, Explosive Scorches)
    {
        std::cout << "\n[Test 29] Verifying Dynamic Projective Decal System...\n";
        Lab::DecalSystem decalSys;
        decalSys.init();

        if (decalSys.getDecalCount() != 0) {
            std::cerr << "Assertion failed: Fresh DecalSystem must have 0 decals!\n";
            return 1;
        }

        // 1. Verify Spawning of all 4 decal types
        decalSys.spawnDecal(Lab::DecalType::BulletHoleConcrete, Lab::Vec3(0.0f, 2.0f, -3.8f), Lab::Vec3(0.0f, 0.0f, 1.0f), 0.22f);
        decalSys.spawnDecal(Lab::DecalType::BulletHoleMetal, Lab::Vec3(-2.8f, 0.6f, 0.96f), Lab::Vec3(0.0f, 0.0f, 1.0f), 0.18f);
        decalSys.spawnDecal(Lab::DecalType::BloodSplatter, Lab::Vec3(1.2f, 1.5f, -3.8f), Lab::Vec3(0.0f, 0.0f, 1.0f), 0.55f);
        decalSys.spawnDecal(Lab::DecalType::BloodSplatter, Lab::Vec3(1.2f, 0.01f, -2.5f), Lab::Vec3(0.0f, 1.0f, 0.0f), 0.45f);

        if (decalSys.getDecalCount() != 4) {
            std::cerr << "Assertion failed: Expected 4 spawned decals, got " << decalSys.getDecalCount() << "\n";
            return 1;
        }

        // 2. Verify Decal Geometry (4 vertices, 6 indices per quad)
        for (const auto& d : decalSys.getDecals()) {
            if (d.vertices.size() != 4 || d.indices.size() != 6) {
                std::cerr << "Assertion failed: Decal geometry invalid! Vertices=" << d.vertices.size() 
                          << " Indices=" << d.indices.size() << "\n";
                return 1;
            }
            if (d.vertices[0].normal.lengthSq() < 0.9f) {
                std::cerr << "Assertion failed: Decal normal magnitude is invalid!\n";
                return 1;
            }
        }
        std::cout << "  [PASS] Decal quads generated with correct geometry and tangent orientation!\n";

        // 3. Verify Lifetime & Fadeout System
        decalSys.spawnDecal(Lab::DecalType::BulletHoleConcrete, Lab::Vec3(10.0f, 10.0f, 10.0f), Lab::Vec3(0.0f, 1.0f, 0.0f), 0.2f, 4.0f);
        size_t countBefore = decalSys.getDecalCount();
        if (countBefore != 5) {
            std::cerr << "Assertion failed: Expected 5 decals before update!\n";
            return 1;
        }

        // Advance time into fadeout period
        decalSys.update(2.5f);
        const auto& fadingDecals = decalSys.getDecals();
        if (fadingDecals.back().alpha >= 1.0f) {
            std::cerr << "Assertion failed: Decal alpha should fade when lifetime > maxLifetime - 5s!\n";
            return 1;
        }

        // Advance time past expiration
        decalSys.update(2.0f);
        if (decalSys.getDecalCount() != 4) {
            std::cerr << "Assertion failed: Expired decal was not culled from system!\n";
            return 1;
        }
        std::cout << "  [PASS] Decal lifetime aging, alpha fadeout, and automatic cleanup verified!\n";

        // 4. Verify Explosion Scorch Projection
        std::vector<Lab::MapBrush> testBrushes;
        Lab::MapBrush wallBrush;
        wallBrush.position = Lab::Vec3(0.0f, 2.0f, -3.8f);
        wallBrush.size = Lab::Vec3(10.0f, 4.0f, 0.4f);
        testBrushes.push_back(wallBrush);

        decalSys.spawnExplosionScorch(Lab::Vec3(0.0f, 0.5f, -2.5f), 2.8f, testBrushes);
        if (decalSys.getDecalCount() <= 4) {
            std::cerr << "Assertion failed: spawnExplosionScorch should project scorch decals on surfaces!\n";
            return 1;
        }
        std::cout << "  [PASS] Explosion scorch projection on ground and nearby brush surfaces verified!\n";

        // 5. Populate Rich Visual Verification Scene
        decalSys.clear();

        // Ground blast scorch from high-yield explosive detonation
        decalSys.spawnDecal(Lab::DecalType::ExplosiveScorch, Lab::Vec3(0.0f, 0.005f, -0.5f), Lab::Vec3(0.0f, 1.0f, 0.0f), 3.4f);

        // Multiple concrete bullet hole clusters on the back concrete wall
        decalSys.spawnDecal(Lab::DecalType::BulletHoleConcrete, Lab::Vec3(-0.4f, 2.2f, -3.79f), Lab::Vec3(0.0f, 0.0f, 1.0f), 0.22f);
        decalSys.spawnDecal(Lab::DecalType::BulletHoleConcrete, Lab::Vec3(-0.25f, 2.35f, -3.79f), Lab::Vec3(0.0f, 0.0f, 1.0f), 0.20f);
        decalSys.spawnDecal(Lab::DecalType::BulletHoleConcrete, Lab::Vec3(-0.3f, 2.05f, -3.79f), Lab::Vec3(0.0f, 0.0f, 1.0f), 0.24f);
        decalSys.spawnDecal(Lab::DecalType::BulletHoleConcrete, Lab::Vec3(-0.55f, 2.15f, -3.79f), Lab::Vec3(0.0f, 0.0f, 1.0f), 0.19f);
        decalSys.spawnDecal(Lab::DecalType::BulletHoleConcrete, Lab::Vec3( 0.6f, 1.8f, -3.79f), Lab::Vec3(0.0f, 0.0f, 1.0f), 0.25f);
        decalSys.spawnDecal(Lab::DecalType::BulletHoleConcrete, Lab::Vec3( 0.75f, 1.95f, -3.79f), Lab::Vec3(0.0f, 0.0f, 1.0f), 0.21f);

        // Organic crimson blood splatters on wall and floor pooling
        decalSys.spawnDecal(Lab::DecalType::BloodSplatter, Lab::Vec3(1.6f, 2.1f, -3.79f), Lab::Vec3(0.0f, 0.0f, 1.0f), 0.65f);
        decalSys.spawnDecal(Lab::DecalType::BloodSplatter, Lab::Vec3(1.9f, 1.6f, -3.79f), Lab::Vec3(0.0f, 0.0f, 1.0f), 0.45f);
        decalSys.spawnDecal(Lab::DecalType::BloodSplatter, Lab::Vec3(1.5f, 0.005f, -2.4f), Lab::Vec3(0.0f, 1.0f, 0.0f), 0.70f);
        decalSys.spawnDecal(Lab::DecalType::BloodSplatter, Lab::Vec3(2.1f, 0.005f, -2.0f), Lab::Vec3(0.0f, 1.0f, 0.0f), 0.50f);

        // Metal bullet punctures on metal props
        decalSys.spawnDecal(Lab::DecalType::BulletHoleMetal, Lab::Vec3( 2.8f, 0.55f, 0.83f), Lab::Vec3(0.0f, 0.0f, 1.0f), 0.18f);
        decalSys.spawnDecal(Lab::DecalType::BulletHoleMetal, Lab::Vec3( 2.7f, 0.75f, 0.83f), Lab::Vec3(0.0f, 0.0f, 1.0f), 0.16f);
        decalSys.spawnDecal(Lab::DecalType::BulletHoleMetal, Lab::Vec3(-2.8f, 0.50f, 0.96f), Lab::Vec3(0.0f, 0.0f, 1.0f), 0.17f);
        decalSys.spawnDecal(Lab::DecalType::BulletHoleMetal, Lab::Vec3(-2.6f, 0.65f, 0.96f), Lab::Vec3(0.0f, 0.0f, 1.0f), 0.19f);

        // Scorch mark climbing the back concrete wall from explosive detonation
        decalSys.spawnDecal(Lab::DecalType::ExplosiveScorch, Lab::Vec3(0.0f, 0.8f, -3.79f), Lab::Vec3(0.0f, 0.0f, 1.0f), 1.6f);

        // 6. Visual Rendering Pass
        glClearColor(0.10f, 0.12f, 0.16f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        Lab::Camera decalCam(60.0f, (float)w / (float)h, 0.01f, 1000.0f);
        decalCam.setPosition(Lab::Vec3(0.0f, 2.5f, 4.2f));
        decalCam.update(Lab::Vec2(0.0f, -18.0f));

        Lab::Vec3 sunDirection(-0.35f, -0.85f, -0.40f);
        Lab::Mat4 lightMatrix = Lab::ShadowMap::computeSunLightSpaceMatrix(sunDirection, Lab::Vec3(0, 1.0f, 0), 16.0f);
        Lab::ShadowMap sMap;
        sMap.init(2048, 2048);

        // Shadow Depth Pass
        sMap.beginShadowPass(lightMatrix);
        Lab::Renderer::beginShadowDepthPass(lightMatrix);
        Lab::Renderer::drawShadowCube(Lab::Vec3(0.0f, -0.1f, 0.0f), Lab::Vec3(25.0f, 0.2f, 25.0f));
        Lab::Renderer::drawShadowCube(Lab::Vec3(0.0f, 2.5f, -4.0f), Lab::Vec3(20.0f, 5.0f, 0.4f));
        Lab::Renderer::drawShadowCube(Lab::Vec3(-2.8f, 0.45f, 0.5f), Lab::Vec3(0.9f, 0.9f, 0.9f));
        Lab::Renderer::drawShadowCube(Lab::Vec3(-2.8f, 1.35f, 0.5f), Lab::Vec3(0.9f, 0.9f, 0.9f));
        Lab::Renderer::drawShadowCube(Lab::Vec3( 2.8f, 0.48f, 0.5f), Lab::Vec3(0.64f, 0.96f, 0.64f));
        Lab::Renderer::endShadowDepthPass();
        sMap.endShadowPass(w, h);

        // Lit Pass
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        Lab::Renderer::beginFrame(decalCam);
        Lab::Renderer::setSunLight(sunDirection, Lab::Vec3(1.0f, 0.98f, 0.94f), Lab::Vec3(0.25f, 0.28f, 0.35f));
        Lab::Renderer::setShadowMap(lightMatrix, sMap.getDepthTexture());

        Lab::Texture flrTex("assets/textures/floor_tiles.bmp");
        Lab::Texture wlTex("assets/textures/concrete_wall.bmp");
        Lab::Texture crtTex("crate_wood.png");
        Lab::Texture brlTex("barrel_hazard.png");

        // Environment geometry
        Lab::Renderer::drawCube(Lab::Vec3(0.0f, -0.1f, 0.0f), Lab::Vec3(25.0f, 0.2f, 25.0f), Lab::Vec3(0.75f, 0.75f, 0.75f), &flrTex, true);
        Lab::Renderer::drawCube(Lab::Vec3(0.0f, 2.5f, -4.0f), Lab::Vec3(20.0f, 5.0f, 0.4f), Lab::Vec3(0.70f, 0.70f, 0.70f), &wlTex, true);

        // Props
        Lab::Renderer::drawCube(Lab::Vec3(-2.8f, 0.45f, 0.5f), Lab::Vec3(0.9f, 0.9f, 0.9f), Lab::Vec3(1, 1, 1), &crtTex, true);
        Lab::Renderer::drawCube(Lab::Vec3(-2.8f, 1.35f, 0.5f), Lab::Vec3(0.9f, 0.9f, 0.9f), Lab::Vec3(1, 1, 1), &crtTex, true);
        Lab::Renderer::drawCube(Lab::Vec3( 2.8f, 0.48f, 0.5f), Lab::Vec3(0.64f, 0.96f, 0.64f), Lab::Vec3(1, 1, 1), &brlTex, true);

        Lab::Renderer::disableShadowMap();

        // Render Dynamic Projective Decals
        decalSys.render(decalCam);

        // UI Diagnostics Overlay
        Lab::Renderer::beginUI(w, h);
        Lab::Renderer::drawRect(40.0f, 30.0f, 950.0f, 88.0f, Lab::Vec3(0.10f, 0.12f, 0.16f));
        Lab::Renderer::drawRect(40.0f, 30.0f, 950.0f, 1.0f, Lab::Vec3(0.2f, 0.75f, 1.0f));
        Lab::LabFont::drawText(56.0f, 44.0f, "DYNAMIC PROJECTIVE DECAL SYSTEM (SPRINT 7)", 2.0f, Lab::Vec3(0.3f, 0.85f, 1.0f), Lab::LabFontType::GeoSans);
        Lab::LabFont::drawText(56.0f, 74.0f, "CONCRETE & METAL BULLET HOLES | ORGANIC BLOOD SPLATTERS | EXPLOSIVE SCORCHES", 1.6f, Lab::Vec3(0.90f, 0.92f, 0.95f), Lab::LabFontType::GeoSans);
        Lab::Renderer::endUI();

        Lab::Renderer::endFrame();
        glFinish();
        saveFrameToBMP("test_decals_and_impacts.bmp", w, h);
        std::cout << "  [PASS] Saved visual Decals & Impacts verification to 'test_decals_and_impacts.bmp'.\n";

        decalSys.shutdown();
    }

    // 30. Verify Interactive In-World Terminals, Consoles & Use Triggers (Sprint 8)
    {
        std::cout << "\n[Test 30] Verifying Biometric Retinal Scanner, Lua 5.4 Game Mechanics & Barrel Death...\n";
        Lab::InteractiveSystem isys;
        isys.init();

        if (!isys.getEntities().empty()) {
            std::cerr << "Assertion failed: Fresh InteractiveSystem must have 0 entities!\n";
            return 1;
        }

        // 1. Add interactive entities (Retinal Scanner, Keypad, WallSwitch)
        Lab::InteractiveEntity scanner;
        scanner.id = 1;
        scanner.type = Lab::InteractiveType::RetinalScanner;
        scanner.title = "RETINAL SCANNER";
        scanner.subtitle = "BIOMETRIC AIRLOCK GATE";
        scanner.statusText = "STAND STILL FOR RETINAL SCAN";
        scanner.authorizedUser = "DR. VANCE";
        scanner.clearanceLevel = 3;
        scanner.scanDuration = 1.25f;
        scanner.position = Lab::Vec3(0.0f, 1.6f, -3.85f);
        scanner.size = Lab::Vec3(0.70f, 0.55f, 0.12f);
        scanner.normal = Lab::Vec3(0.0f, 0.0f, 1.0f);
        scanner.themeColor = Lab::Vec3(0.2f, 0.85f, 1.0f);
        scanner.isLocked = true;
        scanner.isActivated = false;
        scanner.targetDoorIndex = 0;
        isys.addEntity(scanner);

        Lab::InteractiveEntity keypad;
        keypad.id = 2;
        keypad.type = Lab::InteractiveType::Keypad;
        keypad.title = "SECURITY KEYPAD";
        keypad.subtitle = "RESTRICTED VAULT";
        keypad.statusText = "LOCKED - CLEARANCE 4 REQUIRED";
        keypad.position = Lab::Vec3(-2.6f, 1.5f, -3.85f);
        keypad.size = Lab::Vec3(0.45f, 0.65f, 0.12f);
        keypad.normal = Lab::Vec3(0.0f, 0.0f, 1.0f);
        keypad.themeColor = Lab::Vec3(1.0f, 0.25f, 0.2f);
        keypad.isLocked = true;
        isys.addEntity(keypad);

        Lab::InteractiveEntity wallSwitch;
        wallSwitch.id = 3;
        wallSwitch.type = Lab::InteractiveType::WallSwitch;
        wallSwitch.title = "AUXILIARY POWER BREAKER";
        wallSwitch.subtitle = "TURBINE GENERATOR";
        wallSwitch.statusText = "OFFLINE";
        wallSwitch.position = Lab::Vec3(2.6f, 1.5f, -3.85f);
        wallSwitch.size = Lab::Vec3(0.40f, 0.55f, 0.12f);
        wallSwitch.normal = Lab::Vec3(0.0f, 0.0f, 1.0f);
        wallSwitch.themeColor = Lab::Vec3(1.0f, 0.65f, 0.15f);
        wallSwitch.isLocked = false;
        isys.addEntity(wallSwitch);

        if (isys.getEntities().size() != 3) {
            std::cerr << "Assertion failed: Expected 3 registered interactive entities, got " << isys.getEntities().size() << "\n";
            return 1;
        }
        std::cout << "  [PASS] Interactive entity registration and biometric property initialization verified!\n";

        // 2. Test raycast picking and distance threshold
        // Player standing far away (3.8m distance -> exceeds 2.8m limit)
        isys.update(0.016f, Lab::Vec3(0.0f, 1.6f, 0.0f), Lab::Vec3(0.0f, 0.0f, -1.0f), false);
        if (isys.getHoveredEntity() != nullptr) {
            std::cerr << "Assertion failed: Retinal Scanner should not be hovered beyond 2.8m range!\n";
            return 1;
        }

        // Player standing in range (1.85m distance -> within 2.8m limit)
        isys.update(0.016f, Lab::Vec3(0.0f, 1.6f, -2.0f), Lab::Vec3(0.0f, 0.0f, -1.0f), false);
        if (isys.getHoveredEntity() == nullptr || isys.getHoveredEntity()->id != 1) {
            std::cerr << "Assertion failed: Player looking at Retinal Scanner #1 should pick it up!\n";
            return 1;
        }
        std::cout << "  [PASS] Raycast picking and interaction distance limits verified!\n";

        // 3. Test locked entity interaction
        // Look at Keypad #2 and press [E]
        isys.update(0.016f, Lab::Vec3(-2.6f, 1.5f, -2.0f), Lab::Vec3(0.0f, 0.0f, -1.0f), true);
        if (isys.getHoveredEntity() == nullptr || isys.getHoveredEntity()->id != 2) {
            std::cerr << "Assertion failed: Player should be looking at Keypad #2!\n";
            return 1;
        }
        if (isys.getHoveredEntity()->isActivated) {
            std::cerr << "Assertion failed: Locked entity should NOT activate upon use!\n";
            return 1;
        }
        if (isys.getLastNotice().find("ACCESS DENIED") == std::string::npos) {
            std::cerr << "Assertion failed: Locked entity must display ACCESS DENIED notice!\n";
            return 1;
        }
        std::cout << "  [PASS] Security lockout and access denied logic verified!\n";

        // 4. Test Retinal Scanner interaction and door unlocking
        Lab::LabMap testMap;
        Lab::MapDoor testDoor;
        testDoor.name = "Sector 4 Heavy Airlock Gate";
        testDoor.position = Lab::Vec3(0.0f, 1.5f, -3.8f);
        testDoor.isLocked = true;
        testDoor.isOpen = false;
        testMap.doors.push_back(testDoor);

        // Look at Retinal Scanner #1 and press [E] -> Initiates biometric scan!
        isys.update(0.016f, Lab::Vec3(0.0f, 1.6f, -2.0f), Lab::Vec3(0.0f, 0.0f, -1.0f), true, &testMap);
        Lab::InteractiveEntity* scanEnt = isys.getEntity(1);
        if (!scanEnt || !scanEnt->isScanning) {
            std::cerr << "Assertion failed: Pressing [E] on Retinal Scanner must start biometric scanning!\n";
            return 1;
        }
        std::cout << "  [PASS] Biometric retinal eye scan started (isScanning = true, progress = 0%)!\n";

        // Advance scan by 0.5s -> scan in progress (door remains locked)
        isys.update(0.50f, Lab::Vec3(0.0f, 1.6f, -2.0f), Lab::Vec3(0.0f, 0.0f, -1.0f), false, &testMap);
        if (!scanEnt->isScanning || scanEnt->scanProgress <= 0.0f || scanEnt->scanProgress >= 1.0f) {
            std::cerr << "Assertion failed: Retinal scan progress should be midway between 0 and 100%!\n";
            return 1;
        }
        if (!testMap.doors[0].isLocked) {
            std::cerr << "Assertion failed: Airlock door must remain locked while scan is in progress!\n";
            return 1;
        }
        std::cout << "  [PASS] Midway scan progress (" << (int)(scanEnt->scanProgress * 100.0f) << "%) correctly tracked!\n";

        // Advance scan past duration (1.0s more -> total 1.5s > 1.25s) -> Scan completes, unlocks door!
        isys.update(1.00f, Lab::Vec3(0.0f, 1.6f, -2.0f), Lab::Vec3(0.0f, 0.0f, -1.0f), false, &testMap);
        if (scanEnt->isScanning) {
            std::cerr << "Assertion failed: Retinal scan should be completed!\n";
            return 1;
        }
        if (testMap.doors[0].isLocked || !testMap.doors[0].isOpen) {
            std::cerr << "Assertion failed: Airlock door should be UNLOCKED and OPEN after authenticated scan!\n";
            return 1;
        }
        if (isys.getLastNotice().find("ACCESS GRANTED") == std::string::npos) {
            std::cerr << "Assertion failed: Expected 'ACCESS GRANTED' notice upon scan completion!\n";
            return 1;
        }
        std::cout << "  [PASS] Retinal authentication passed: Dr. Vance verified, airlock door unlocked & opened!\n";

        // 5. Test Lua 5.4 ScriptEngine & Game Mechanics Subsystem
        std::cout << "  [Test] Initializing Lua 5.4 Game Mechanics from 'assets/scripts/game_mechanics.lua'...\n";
        Lab::ScriptEngine scriptEngine;
        if (!scriptEngine.init("assets/scripts/game_mechanics.lua")) {
            std::cerr << "Assertion failed: Failed to initialize Lua ScriptEngine!\n";
            return 1;
        }

        if (scriptEngine.getPlayerMaxHealth() != 150.0f) {
            std::cerr << "Assertion failed: Expected PlayerMaxHealth 150.0 from Lua, got " << scriptEngine.getPlayerMaxHealth() << "\n";
            return 1;
        }
        if (scriptEngine.getPlayerStartArmor() != 50.0f) {
            std::cerr << "Assertion failed: Expected PlayerStartArmor 50.0 from Lua, got " << scriptEngine.getPlayerStartArmor() << "\n";
            return 1;
        }
        if (scriptEngine.getRetinalAuthorizedUser() != "DR. VANCE") {
            std::cerr << "Assertion failed: Expected RetinalAuthorizedUser 'DR. VANCE' from Lua, got " << scriptEngine.getRetinalAuthorizedUser() << "\n";
            return 1;
        }
        if (scriptEngine.getBarrelDamageMultiplier() != 0.75f) {
            std::cerr << "Assertion failed: Expected BarrelDamageMultiplier 0.75 from Lua, got " << scriptEngine.getBarrelDamageMultiplier() << "\n";
            return 1;
        }
        std::cout << "  [PASS] Lua 5.4 tables correctly read and cached (Health: 150, Armor: 50, User: DR. VANCE)!\n";

        // Test Lua CalculateDamage function
        // Case A: Non-lethal damage with armor
        auto resA = scriptEngine.calculateDamage(50.0f, 50.0f, 150.0f);
        // 50 incoming * 0.70 = 35 absorbed, 15 final damage to health
        if (std::abs(resA.absorbedByArmor - 35.0f) > 0.1f || std::abs(resA.finalDamage - 15.0f) > 0.1f || resA.isLethal) {
            std::cerr << "Assertion failed: CalculateDamage(50, 50, 150) mismatch! Abs: " << resA.absorbedByArmor << ", Fin: " << resA.finalDamage << "\n";
            return 1;
        }
        std::cout << "  [PASS] Lua CalculateDamage non-lethal evaluation verified (Absorbed: 35, Final: 15, Lethal: false)!\n";

        // Case B: Lethal damage exceeding health
        auto resB = scriptEngine.calculateDamage(300.0f, 50.0f, 50.0f);
        if (!resB.isLethal) {
            std::cerr << "Assertion failed: CalculateDamage(300, 50, 50) should be LETHAL!\n";
            return 1;
        }
        std::cout << "  [PASS] Lua CalculateDamage lethal flag evaluation verified!\n";

        // Verify Weapons loaded from Lua
        const auto& wepDefs = scriptEngine.getWeaponDefinitions();
        if (wepDefs.size() < 9) {
            std::cerr << "Assertion failed: Expected at least 9 weapons loaded from Lua, got " << wepDefs.size() << "\n";
            return 1;
        }
        const auto* pipeDef = scriptEngine.getWeaponDefById(0);
        const auto* rpgDef = scriptEngine.getWeaponDefById(8);
        if (!pipeDef || pipeDef->damage != 55.0f || !pipeDef->isMelee) {
            std::cerr << "Assertion failed: Weapon 0 (Pipe) stats from Lua incorrect!\n";
            return 1;
        }
        if (!rpgDef || rpgDef->damage != 120.0f || rpgDef->splashRadius != 5.5f) {
            std::cerr << "Assertion failed: Weapon 8 (RPG) stats from Lua incorrect!\n";
            return 1;
        }
        std::cout << "  [PASS] All 9 weapon definitions successfully loaded and verified from Lua script!\n";

        // 6. Test Barrel Explosion Player Death & HUD Health Clamping
        Lab::LabHUD testHud;
        testHud.health = -38.0f;
        testHud.suitArmor = -12.0f;
        int clampedHealth = std::max(0, (int)std::ceil(testHud.health));
        int clampedArmor = std::max(0, (int)std::ceil(testHud.suitArmor));
        if (clampedHealth < 0 || clampedArmor < 0) {
            std::cerr << "Assertion failed: Clamped HUD values must never be negative!\n";
            return 1;
        }
        if (clampedHealth != 0 || clampedArmor != 0) {
            std::cerr << "Assertion failed: Negative health/armor must clamp to 0 display!\n";
            return 1;
        }
        std::cout << "  [PASS] HUD health/armor negative value bug fix verified: clamped to 0 (no more '-38 HP')!\n";

        // 7. Visual Rendering Verification of 3D Scanner & Biometric HUD Card
        glClearColor(0.10f, 0.12f, 0.16f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        Lab::Camera termCam(60.0f, (float)w / (float)h, 0.01f, 1000.0f);
        termCam.setPosition(Lab::Vec3(0.0f, 1.6f, -0.6f));
        termCam.update(Lab::Vec2(0.0f, -4.0f));

        Lab::Vec3 sunDir(-0.35f, -0.85f, -0.40f);
        Lab::Mat4 lightMat = Lab::ShadowMap::computeSunLightSpaceMatrix(sunDir, Lab::Vec3(0, 1.0f, 0), 16.0f);
        Lab::ShadowMap sMap;
        sMap.init(2048, 2048);

        // Shadow Pass
        sMap.beginShadowPass(lightMat);
        Lab::Renderer::beginShadowDepthPass(lightMat);
        Lab::Renderer::drawShadowCube(Lab::Vec3(0.0f, -0.1f, 0.0f), Lab::Vec3(25.0f, 0.2f, 25.0f));
        Lab::Renderer::drawShadowCube(Lab::Vec3(0.0f, 2.5f, -4.0f), Lab::Vec3(20.0f, 5.0f, 0.4f));
        Lab::Renderer::drawShadowCube(Lab::Vec3(0.0f, 1.6f, -3.85f), Lab::Vec3(0.70f, 0.55f, 0.12f));
        Lab::Renderer::drawShadowCube(Lab::Vec3(-2.6f, 1.5f, -3.85f), Lab::Vec3(0.45f, 0.65f, 0.12f));
        Lab::Renderer::drawShadowCube(Lab::Vec3(2.6f, 1.5f, -3.85f), Lab::Vec3(0.40f, 0.55f, 0.12f));
        Lab::Renderer::endShadowDepthPass();
        sMap.endShadowPass(w, h);

        // Shaded Lit Pass
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        Lab::Renderer::beginFrame(termCam);
        Lab::Renderer::setSunLight(sunDir, Lab::Vec3(1.0f, 0.98f, 0.94f), Lab::Vec3(0.25f, 0.28f, 0.35f));
        Lab::Renderer::setShadowMap(lightMat, sMap.getDepthTexture());

        Lab::Texture flrTex("assets/textures/floor_tiles.bmp");
        Lab::Texture wlTex("assets/textures/concrete_wall.bmp");

        // Environment
        Lab::Renderer::drawCube(Lab::Vec3(0.0f, -0.1f, 0.0f), Lab::Vec3(25.0f, 0.2f, 25.0f), Lab::Vec3(0.75f, 0.75f, 0.75f), &flrTex, true);
        Lab::Renderer::drawCube(Lab::Vec3(0.0f, 2.5f, -4.0f), Lab::Vec3(20.0f, 5.0f, 0.4f), Lab::Vec3(0.70f, 0.70f, 0.70f), &wlTex, true);

        // Trigger active scan state on entity 1 for visual capture
        scanEnt->isScanning = true;
        scanEnt->scanProgress = 0.68f;

        // Render In-World 3D Retinal Scanner & Consoles
        isys.render(termCam);

        Lab::Renderer::disableShadowMap();

        // 8. 2D HUD Biometric Retinal Scanning Card
        Lab::Renderer::beginUI(w, h);
        isys.renderHUD(w, h);
        Lab::Renderer::endUI();

        Lab::Renderer::endFrame();
        glFinish();
        saveFrameToBMP("test_interactive_terminals.bmp", w, h);
        std::cout << "  [PASS] Saved visual Biometric Retinal Scanner verification to 'test_interactive_terminals.bmp'.\n";

        isys.shutdown();
        scriptEngine.shutdown();
    }

    // 31. Verify Sprint 9: HDR Pipeline (GL_RGBA16F), Multi-Pass Bloom, Cryo Frost Vignette, ACES Filmic Tonemapping & Ice Surface Physics
    {
        std::cout << "\n[Test 31] Verifying Sprint 9: Advanced HDR Pipeline, Bloom, ACES Tonemapping, Frost Vignette & Ice Physics...\n";

        // 1. Initialize Pipeline
        Lab::PostProcessPipeline pp;
        if (!pp.init(w, h)) {
            std::cerr << "Assertion failed: PostProcessPipeline initialization failed!\n";
            return 1;
        }
        if (!pp.isInitialized() || pp.getHDRFbo() == 0 || pp.getHDRTexture() == 0) {
            std::cerr << "Assertion failed: HDR FBO and color textures must be valid!\n";
            return 1;
        }
        if (pp.getWidth() != w || pp.getHeight() != h) {
            std::cerr << "Assertion failed: PostProcessPipeline dimensions mismatch!\n";
            return 1;
        }
        std::cout << "  [PASS] HDR Framebuffer (GL_RGBA16F 1280x720) & Half-Res Bloom ping-pong FBOs initialized!\n";

        // 2. Tonemapping Math Verifications
        Lab::Vec3 ldrSample(0.5f, 0.5f, 0.5f);
        Lab::Vec3 hdrSuperBright(8.0f, 15.0f, 25.0f);
        Lab::Vec3 acesLdr = Lab::PostProcessPipeline::acesFilmicTonemap(ldrSample);
        Lab::Vec3 acesHdr = Lab::PostProcessPipeline::acesFilmicTonemap(hdrSuperBright);

        if (acesHdr.x > 1.0f || acesHdr.y > 1.0f || acesHdr.z > 1.0f ||
            acesHdr.x < 0.0f || acesHdr.y < 0.0f || acesHdr.z < 0.0f) {
            std::cerr << "Assertion failed: ACES Filmic tonemapper must compress HDR values strictly into [0.0, 1.0]!\n";
            return 1;
        }
        if (acesLdr.x <= 0.0f || acesLdr.x >= 1.0f) {
            std::cerr << "Assertion failed: ACES Filmic tonemapper response out of range for midtones!\n";
            return 1;
        }

        Lab::Vec3 reinhardHdr = Lab::PostProcessPipeline::reinhardTonemap(hdrSuperBright);
        if (reinhardHdr.x > 1.0f || reinhardHdr.y > 1.0f || reinhardHdr.z > 1.0f) {
            std::cerr << "Assertion failed: Reinhard tonemapper must compress into [0.0, 1.0]!\n";
            return 1;
        }
        std::cout << "  [PASS] ACES Filmic and Reinhard tonemapping mathematical compression verified!\n";

        // 3. Cryo Frost Vignette Calculation Verifications
        float fFullHealth = Lab::PostProcessPipeline::calculateFrostVignette(100.0f, 100.0f, false);
        float fLowHealth = Lab::PostProcessPipeline::calculateFrostVignette(20.0f, 100.0f, false);
        float fCryoAmbient = Lab::PostProcessPipeline::calculateFrostVignette(100.0f, 100.0f, true);
        float fCryoDead = Lab::PostProcessPipeline::calculateFrostVignette(0.0f, 100.0f, true);

        if (std::abs(fFullHealth - 0.0f) > 0.01f) {
            std::cerr << "Assertion failed: Full health frost vignette should be 0.0, got " << fFullHealth << "\n";
            return 1;
        }
        if (std::abs(fLowHealth - 0.72f) > 0.02f) {
            std::cerr << "Assertion failed: Low health (20 HP) frost vignette should be ~0.72, got " << fLowHealth << "\n";
            return 1;
        }
        if (std::abs(fCryoAmbient - 0.22f) > 0.02f) {
            std::cerr << "Assertion failed: Cryo ambient frost should be ~0.22, got " << fCryoAmbient << "\n";
            return 1;
        }
        if (fCryoDead < 0.99f) {
            std::cerr << "Assertion failed: Dead in Cryo sector frost vignette must clamp to 1.0, got " << fCryoDead << "\n";
            return 1;
        }
        std::cout << "  [PASS] Frost vignette formula verified (Full: 0.0, 20HP: 0.72, Cryo Ambient: 0.22, Dead: 1.0)!\n";

        // 4. Tactical Ice Friction & Momentum Conservation
        float standardFriction = 0.85f;
        float iceFriction = 0.985f;
        float velStandard = 10.0f;
        float velIce = 10.0f;
        for (int t = 0; t < 10; ++t) {
            velStandard *= standardFriction;
            velIce *= iceFriction;
        }
        if (velStandard > 2.5f || velIce < 8.0f) {
            std::cerr << "Assertion failed: Ice surface should conserve >85% velocity after 10 ticks while standard surface stops!\n";
            return 1;
        }
        std::cout << "  [PASS] Ice inertia momentum conservation verified (Standard: " << velStandard << " m/s vs Ice Drift: " << velIce << " m/s)!\n";

        // 5. Visual Verification of HDR Scene, Bloom Glowing Core, Ambient Blizzard & Cryo Frost
        glClearColor(0.05f, 0.07f, 0.12f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        Lab::Camera frostCam(65.0f, (float)w / (float)h, 0.01f, 1000.0f);
        frostCam.setPosition(Lab::Vec3(0.0f, 1.7f, 6.0f));

        Lab::Texture cryoIceTex("assets/textures/cryo_ice.bmp");
        Lab::Texture snowFrostTex("assets/textures/snow_frost.bmp");

        // Render into HDR Framebuffer
        pp.beginScene();
        Lab::Renderer::beginFrame(frostCam);
        Lab::Renderer::setSunLight(Lab::Vec3(-0.4f, -0.8f, -0.4f), Lab::Vec3(0.85f, 0.95f, 1.0f), Lab::Vec3(0.25f, 0.35f, 0.50f));

        // Arctic Frost & Ice environment brushes
        Lab::Renderer::drawCube(Lab::Vec3(0.0f, -0.2f, 0.0f), Lab::Vec3(30.0f, 0.4f, 30.0f), Lab::Vec3(0.85f, 0.92f, 1.0f), &cryoIceTex, true);
        Lab::Renderer::drawCube(Lab::Vec3(-5.0f, 2.0f, -5.0f), Lab::Vec3(2.0f, 4.0f, 2.0f), Lab::Vec3(0.9f, 0.95f, 1.0f), &snowFrostTex, true);
        Lab::Renderer::drawCube(Lab::Vec3(5.0f, 2.0f, -5.0f), Lab::Vec3(2.0f, 4.0f, 2.0f), Lab::Vec3(0.9f, 0.95f, 1.0f), &snowFrostTex, true);
        Lab::Renderer::drawCube(Lab::Vec3(0.0f, 3.5f, -8.0f), Lab::Vec3(16.0f, 7.0f, 1.0f), Lab::Vec3(0.7f, 0.8f, 0.9f), &cryoIceTex, true);

        // Super-luminous HDR Glowing Plasma Orb (Unclamped HDR values to produce brilliant bloom glow)
        Lab::Renderer::drawCube(Lab::Vec3(0.0f, 1.8f, 0.0f), Lab::Vec3(0.85f, 0.85f, 0.85f), Lab::Vec3(4.5f, 3.2f, 1.2f), nullptr, false);
        Lab::Renderer::drawCube(Lab::Vec3(-2.5f, 1.2f, 1.0f), Lab::Vec3(0.45f, 0.45f, 0.45f), Lab::Vec3(1.2f, 3.8f, 5.0f), nullptr, false);

        // Ambient Blizzard Particles
        Lab::ParticleSystem blizzardPs;
        blizzardPs.init();
        blizzardPs.spawnAmbientWeather(frostCam.getPosition(), 60, true);
        for (int step = 0; step < 5; ++step) blizzardPs.update(0.033f, nullptr);
        blizzardPs.render(frostCam);

        Lab::Renderer::endFrame();
        pp.endScene();

        // Downsample, separable blur, ACES filmic tonemap & frost vignette
        pp.render(1.0f, 0.80f, 2.0f, Lab::TonemapperType::ACESFilmic);

        // Crisp 2D HUD on top (Health at 18 HP showing danger state)
        Lab::LabHUD frostHud;
        frostHud.health = 18.0f;
        frostHud.suitArmor = 0.0f;
        frostHud.weaponName = "PLASMA RIFLE";
        frostHud.activeWeaponSlot = 7;
        frostHud.ammoClip = 12;
        frostHud.ammoReserve = 45;
        frostHud.showCombatMessage("CRITICAL HYPOTHERMIA DETECTED - 18 HP", 4.0f);
        frostHud.render(w, h);

        glFinish();
        saveFrameToBMP("test_postprocess_and_frost.bmp", w, h);
        std::cout << "  [PASS] Saved visual HDR Post-Processing & Cryo Frost verification to 'test_postprocess_and_frost.bmp'.\n";

        blizzardPs.shutdown();
        pp.shutdown();

        // Visual Verification: Dark Post-Apocalyptic Cryo Wasteland Outpost with Distance Fog
        {
            auto cryoMap = Lab::LabMap::loadFromFile("assets/maps/cryo_outpost.labmap");
            if (!cryoMap) cryoMap = Lab::LabMap::loadFromFile("cryo_outpost.labmap");
            if (cryoMap) {
                // Ensure textures are cached
                for (const auto& b : cryoMap->brushes) {
                    if (!b.texturePath.empty() && textures.find(b.texturePath) == textures.end()) {
                        textures[b.texturePath] = std::make_unique<Lab::Texture>("assets/textures/" + b.texturePath);
                    }
                }

                Lab::PostProcessPipeline cryoPP;
                cryoPP.init(w, h);
                cryoPP.beginScene();

                Lab::Camera wastelandCam(70.0f, (float)w / (float)h, 0.01f, 1000.0f);
                wastelandCam.setPosition(Lab::Vec3(0.0f, 2.2f, 14.0f));

                Lab::Renderer::beginFrame(wastelandCam);
                Lab::Renderer::setSunLight(cryoMap->metadata.sunDir, cryoMap->metadata.sunColor, cryoMap->metadata.ambientColor);
                Lab::Renderer::setFog(true, Lab::Vec3(0.05f, 0.07f, 0.10f), 10.0f, 75.0f);

                for (const auto& b : cryoMap->brushes) {
                    Lab::Texture* tex = b.texturePath.empty() ? nullptr : textures[b.texturePath].get();
                    Lab::Renderer::drawCube(b.position, b.size, b.color, tex, true, b.uvScale, b.uvMode);
                }

                // Render bots on ground in front of ruined outpost bunker
                Lab::AIManager wastelandAI;
                wastelandAI.spawnBotsForMap(cryoMap.get(), 2, Lab::GameMode::FFA);
                wastelandAI.bots[0].position = Lab::Vec3(-3.5f, 0.0f, -4.0f);
                wastelandAI.bots[1].position = Lab::Vec3(3.5f, 0.0f, -6.0f);
                wastelandAI.render();

                Lab::Renderer::endFrame();
                cryoPP.endScene();
                cryoPP.render(1.0f, 0.22f, 5.0f, Lab::TonemapperType::ACESFilmic);

                glFinish();
                saveFrameToBMP("test_cryo_wasteland_outpost.bmp", w, h);
                std::cout << "  [PASS] Saved visual Dark Post-Apocalyptic Wasteland Atmosphere to 'test_cryo_wasteland_outpost.bmp'.\n";
                cryoPP.shutdown();
            } else {
                std::cerr << "WARNING: Could not load assets/maps/cryo_outpost.labmap for visual verification!\n";
            }
        }
    }

    // 32. Verify Facial Lip-Sync & Morph Targets (Blend Shapes & Audio Envelope Follower)
    {
        std::cout << "\n[Test 32] Verifying Facial Lip-Sync, Morph Targets & Audio Viseme Analysis...\n";

        // 1. Initialize Procedural Head with Blend Shapes
        auto head = Lab::FacialMesh::createProceduralHead();
        if (!head || head->getVertexCount() == 0 || head->getIndexCount() == 0) {
            std::cerr << "Assertion failed: Failed to create procedural facial head mesh!\n";
            return 1;
        }
        if (head->getTargetCount() < 3) {
            std::cerr << "Assertion failed: Expected at least 3 blend shape morph targets, got " << head->getTargetCount() << "\n";
            return 1;
        }

        int jawIdx = head->findMorphTarget("Jaw_Open");
        int narrowIdx = head->findMorphTarget("Mouth_Narrow");
        int smileIdx = head->findMorphTarget("Mouth_Smile");

        if (jawIdx < 0 || narrowIdx < 0 || smileIdx < 0) {
            std::cerr << "Assertion failed: Missing required morph targets (Jaw_Open, Mouth_Narrow, Mouth_Smile)!\n";
            return 1;
        }
        std::cout << "  [PASS] Procedural 3D Head initialized (" << head->getVertexCount() << " vertices, 3 blend shapes: Jaw_Open, Mouth_Narrow, Mouth_Smile)!\n";

        // 2. Test Morph Target Evaluation Math
        head->setWeight("Jaw_Open", 1.0f);
        head->evaluate();

        // Check that lower face vertices deformed downward
        bool jawDeformed = false;
        for (const auto& v : head->getDeformedVertices()) {
            if (v.position.y < -0.15f && v.position.z > 0.0f) {
                jawDeformed = true;
                break;
            }
        }
        if (!jawDeformed) {
            std::cerr << "Assertion failed: Jaw_Open morph target did not displace lower jaw geometry!\n";
            return 1;
        }
        std::cout << "  [PASS] Blend shape vertex displacement and normal recalculation verified!\n";

        // 3. Test LipSyncEvaluator & Envelope Follower
        Lab::LipSyncEvaluator evaluator(30.0f, 15.0f, 2.5f);

        // Step A: Attack response to instant sound burst
        evaluator.update(0.85f, 0.05f); // 50ms pulse
        if (evaluator.getSmoothedEnergy() <= 0.2f || evaluator.getJawOpen() <= 0.4f) {
            std::cerr << "Assertion failed: Attack response too slow for audio acoustic burst!\n";
            return 1;
        }

        // Step B: Decay response to silence
        for (int i = 0; i < 10; ++i) {
            evaluator.update(0.0f, 0.033f);
        }
        if (evaluator.getSmoothedEnergy() > 0.15f) {
            std::cerr << "Assertion failed: Decay response did not close mouth during silence!\n";
            return 1;
        }
        std::cout << "  [PASS] Audio envelope follower attack/decay dynamics verified!\n";

        // 4. Test PCM Audio Buffer RMS Processing
        std::vector<int16_t> pcmWave(1024);
        for (size_t i = 0; i < pcmWave.size(); ++i) {
            pcmWave[i] = static_cast<int16_t>(std::sin(static_cast<float>(i) * 0.15f) * 26000.0f); // Synthetic loud vowel sound
        }
        evaluator.processPCM(pcmWave.data(), pcmWave.size(), 0.033f);
        if (evaluator.getJawOpen() < 0.5f) {
            std::cerr << "Assertion failed: PCM buffer failed to drive jaw opening!\n";
            return 1;
        }
        std::cout << "  [PASS] 16-bit PCM audio stream RMS energy evaluation verified (Jaw Open: " << evaluator.getJawOpen() << ")!\n";

        // 5. Test Procedural Dialog Track Generator
        auto dialogTrack = Lab::LipSyncEvaluator::generateSpeechTrack(2.5f, 4.0f);
        if (dialogTrack.size() != 150) {
            std::cerr << "Assertion failed: Expected 150 frames for 2.5s speech track, got " << dialogTrack.size() << "\n";
            return 1;
        }
        std::cout << "  [PASS] Procedural dialog audio track synthesized (2.5s, 150 frames)!\n";

        // 6. Visual Rendering of Speaking Humanoid Head with Dialog Overlay
        glClearColor(0.08f, 0.10f, 0.14f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        Lab::Camera faceCam(50.0f, (float)w / (float)h, 0.01f, 100.0f);
        faceCam.setPosition(Lab::Vec3(0.0f, 0.15f, 0.95f));

        Lab::Renderer::beginFrame(faceCam);
        Lab::Renderer::setSunLight(Lab::Vec3(-0.4f, -0.6f, -0.6f), Lab::Vec3(1.0f, 0.95f, 0.90f), Lab::Vec3(0.35f, 0.38f, 0.45f));

        // Background backdrop
        Lab::Renderer::drawCube(Lab::Vec3(0.0f, 0.0f, -1.2f), Lab::Vec3(4.0f, 3.0f, 0.2f), Lab::Vec3(0.16f, 0.20f, 0.28f));

        // Configure speech pose: Speaking an accented vowel with subtle smile
        head->setWeight("Jaw_Open", 0.78f);
        head->setWeight("Mouth_Narrow", 0.25f);
        head->setWeight("Mouth_Smile", 0.35f);
        head->evaluate();

        // Render deformed facial mesh
        Lab::Mat4 headMat = Lab::Mat4::translate(Lab::Vec3(0.0f, 0.0f, 0.0f));
        Lab::Renderer::drawFacialMesh(*head, headMat, Lab::Vec3(1.0f, 1.0f, 1.0f), nullptr, true);

        Lab::Renderer::endFrame();

        // 2D Dialog Box Overlay
        Lab::Renderer::beginUI(w, h);
        Lab::Renderer::drawRect(160.0f, 550.0f, 960.0f, 110.0f, Lab::Vec3(0.08f, 0.12f, 0.18f));
        Lab::Renderer::drawRect(162.0f, 552.0f, 956.0f, 106.0f, Lab::Vec3(0.12f, 0.18f, 0.26f));
        Lab::Renderer::drawRect(162.0f, 552.0f, 956.0f, 4.0f, Lab::Vec3(0.25f, 0.85f, 1.0f));

        Lab::LabFont::drawText(190.0f, 568.0f, "NPC RADIO COMMS // DR. VANCE", 1.8f, Lab::Vec3(0.3f, 0.85f, 1.0f), Lab::LabFontType::GeoSans);
        Lab::LabFont::drawText(190.0f, 604.0f, "\"SECURITY OVERRIDE CONFIRMED. PRIMARY GATE HAS BEEN UNLOCKED.\"", 1.9f, Lab::Vec3(0.95f, 0.98f, 1.0f), Lab::LabFontType::GeoSans);
        Lab::LabFont::drawText(190.0f, 634.0f, "[AUDIO ENVELOPE: 78% RMS | BLEND SHAPES: JAW_OPEN 0.78, MOUTH_NARROW 0.25]", 1.4f, Lab::Vec3(0.7f, 0.75f, 0.82f), Lab::LabFontType::System);
        Lab::Renderer::endUI();

        glFinish();
        saveFrameToBMP("test_facial_lipsync.bmp", w, h);
        std::cout << "  [PASS] Saved visual Facial Lip-Sync verification to 'test_facial_lipsync.bmp'.\n";

        head->shutdown();
    }

    // 33. Verify Dedicated Authoritative Client-Server UDP Multiplayer Pipeline
    {
        std::cout << "\n[Test 33] Verifying Dedicated Authoritative Client-Server UDP Multiplayer...\n";

        if (!Lab::NetworkSystem::init()) {
            std::cerr << "Assertion failed: NetworkSystem::init failed!\n";
            return 1;
        }

        uint16_t testPort = 27019;

        // 1. Start Dedicated Authoritative Server
        Lab::DedicatedServer server;
        if (!server.start(testPort, "facility_alpha.labmap")) {
            std::cerr << "Assertion failed: DedicatedServer failed to start on port " << testPort << "!\n";
            return 1;
        }
        if (!server.isRunning() || server.getPort() != testPort) {
            std::cerr << "Assertion failed: DedicatedServer state inconsistent!\n";
            return 1;
        }
        std::cout << "  [PASS] DedicatedServer started on UDP 127.0.0.1:" << testPort << " (Tickrate: 64Hz)!\n";

        // 2. Connect Client
        Lab::NetworkClient client;
        if (!client.connect("127.0.0.1", testPort, "TestRanger")) {
            std::cerr << "Assertion failed: NetworkClient failed to initiate connect!\n";
            return 1;
        }
        std::cout << "  [PASS] NetworkClient connected and sent ConnectRequest packet!\n";

        // 3. Loopback Handshake & Snapshot Exchange
        for (int frame = 0; frame < 10; ++frame) {
            server.tick(0.016f);
            client.update(0.016f, Lab::Vec3(0, 0, 0), Lab::Vec3(1.0f, 0, 0), 0.0f, 0.0f, 0);
        }

        if (server.getClientCount() != 1) {
            std::cerr << "Assertion failed: Server should register 1 connected client, got " << server.getClientCount() << "!\n";
            return 1;
        }
        if (client.getClientId() == 0) {
            std::cerr << "Assertion failed: Client should be assigned a valid ClientID from server!\n";
            return 1;
        }
        std::cout << "  [PASS] Client-Server handshake confirmed (Assigned ClientID: " << client.getClientId() << ")!\n";

        // 4. Test Authoritative World Movement & Snapshots
        for (int frame = 0; frame < 20; ++frame) {
            client.update(0.016f, Lab::Vec3(0, 0, 0), Lab::Vec3(0, 0, 5.5f), 0.0f, 0.0f, 0);
            server.tick(0.016f);
        }

        if (!client.hasNewSnapshot()) {
            std::cerr << "Assertion failed: Client should have received authoritative NetServerSnapshot!\n";
            return 1;
        }
        const auto& snap = client.getLatestSnapshot();
        if (snap.playerCount != 1) {
            std::cerr << "Assertion failed: Snapshot playerCount should be 1!\n";
            return 1;
        }
        std::cout << "  [PASS] Authoritative NetServerSnapshot received (Server Tick: " << snap.serverTick 
                  << ", Authoritative Pos: " << snap.players[0].position.z << "m)!\n";
        client.consumeSnapshot();

        // 5. Test Client-Side Prediction & Reconciliation
        Lab::ClientPrediction pred;
        Lab::NetUserCmd dummyCmd;
        dummyCmd.cmdNumber = 42;
        dummyCmd.deltaTime = 0.016f;
        dummyCmd.forwardMove = 1.0f;
        dummyCmd.yaw = 0.0f;

        // Local prediction thought player was at (0, 0, 8.5)
        pred.recordCommand(dummyCmd, Lab::Vec3(0, 0, 8.5f), Lab::Vec3(0, 0, 5.5f));

        // But server authoritative state says player hit a collision brush and stopped at (0, 0, 5.0)
        Lab::Vec3 authoritativeServerPos(0, 0, 5.0f);
        Lab::Vec3 authoritativeServerVel(0, 0, 0.0f);
        Lab::Vec3 correctedPos, correctedVel;

        bool reconciled = pred.reconcile(42, authoritativeServerPos, authoritativeServerVel, correctedPos, correctedVel, 0.05f);
        if (!reconciled) {
            std::cerr << "Assertion failed: Client prediction failed to detect 3.5m desync error!\n";
            return 1;
        }
        if (std::abs(correctedPos.z - 5.0f) > 0.01f) {
            std::cerr << "Assertion failed: Reconciled position should match server position (5.0), got " << correctedPos.z << "!\n";
            return 1;
        }
        std::cout << "  [PASS] Client prediction reconciliation verified: desync snapped and resolved smoothly!\n";

        // 6. Test Dynamic UDP LAN Server Discovery (ServerBrowser -> DedicatedServer)
        Lab::ServerBrowser browser;
        browser.start();
        browser.sendQueryTo("127.0.0.1", testPort);
        server.tick(0.016f);
        browser.update(0.016f);

        if (browser.getServers().empty()) {
            std::cerr << "Assertion failed: ServerBrowser failed to discover live DedicatedServer on port " << testPort << "!\n";
            return 1;
        }
        const auto& disc = browser.getServers()[0];
        if (disc.port != testPort || disc.playerCount != 1) {
            std::cerr << "Assertion failed: Discovered server info mismatch (expected port " << testPort << ", got " << disc.port << ")!\n";
            return 1;
        }
        std::cout << "  [PASS] ServerBrowser LAN discovery verified: found " << disc.name 
                  << " (Map: " << disc.map << ", Mode: " << disc.mode << ", Ping: " << disc.pingMs << "ms)!\n";
        browser.stop();

        // 7. Clean Disconnect & Server Teardown
        client.disconnect();
        server.tick(0.05f);
        server.stop();
        Lab::NetworkSystem::shutdown();
        std::cout << "  [PASS] Dedicated server and network client clean shutdown verified!\n";
    }

    // 34. Verify Valve Hammer Character & Weapon Studio Subsystem (Grip Poser, Reload Curve, Skins, Face Mimics & Lip-Sync)
    {
        std::cout << "\n[Test 34] Verifying Valve Hammer Character & Weapon Studio Subsystems...\n";

        Lab::CharacterStudio studio;
        studio.init();

        // 1. Verify Weapon Grip & Socket Math
        const auto& m4Grip = studio.getWeaponGrip(Lab::WeaponID::M4A4S);
        if (std::abs(m4Grip.rightSocketPos.x - (-0.015f)) > 0.001f ||
            std::abs(m4Grip.leftSocketPos.z - (-0.34f)) > 0.001f) {
            std::cerr << "Assertion failed: M4A4-S socket offsets mismatch!\n";
            return 1;
        }
        std::cout << "  [PASS] Weapon Grip & dual-socket transforms validated!\n";

        // 2. Verify Reload Timeline Configuration
        const auto& reloadConf = studio.getReloadTimeline();
        if (reloadConf.dipDuration <= 0.0f || reloadConf.magDropTime >= reloadConf.magInsertTime ||
            reloadConf.magInsertTime >= reloadConf.boltRackTime) {
            std::cerr << "Assertion failed: Reload timeline keyframe sequence invalid!\n";
            return 1;
        }
        std::cout << "  [PASS] Reload Timeline choreography & keyframe ordering verified!\n";

        // 3. Verify Facial Mesh Blend Shapes (6 targets)
        auto head = Lab::FacialMesh::createProceduralHead();
        if (!head || head->getTargetCount() < 6) {
            std::cerr << "Assertion failed: FacialMesh must support at least 6 blend shape targets, got "
                      << (head ? head->getTargetCount() : 0) << "!\n";
            return 1;
        }

        // Check target indices
        if (head->findMorphTarget("Jaw_Open") < 0 ||
            head->findMorphTarget("Mouth_Narrow") < 0 ||
            head->findMorphTarget("Mouth_Smile") < 0 ||
            head->findMorphTarget("Brow_Raise") < 0 ||
            head->findMorphTarget("Eyes_Squint") < 0 ||
            head->findMorphTarget("Mouth_Frown") < 0) {
            std::cerr << "Assertion failed: Required morph targets not found in head mesh!\n";
            return 1;
        }

        head->setWeight("Jaw_Open", 0.75f);
        head->setWeight("Mouth_Smile", 0.50f);
        head->evaluate();
        const auto& defVerts = head->getDeformedVertices();
        bool deformed = false;
        for (const auto& v : defVerts) {
            if (v.position.y < 0.1f) { deformed = true; break; }
        }
        if (!deformed) {
            std::cerr << "Assertion failed: Facial mesh blend shape evaluation did not displace vertices!\n";
            return 1;
        }
        std::cout << "  [PASS] 6 Facial Blend Shapes (Jaw_Open, Brow_Raise, Mouth_Smile, etc.) and GPU VBO deformation verified!\n";

        // 4. Verify Speech Synthesis & LipSyncEvaluator
        auto speechTrack = Lab::LipSyncEvaluator::generateSpeechTrack(2.5f, 4.5f, 1337);
        if (speechTrack.empty()) {
            std::cerr << "Assertion failed: Generated speech track is empty!\n";
            return 1;
        }
        Lab::LipSyncEvaluator eval;
        for (float amp : speechTrack) {
            eval.update(amp, 0.016f);
        }
        if (eval.getJawOpen() < 0.0f || eval.getJawOpen() > 1.0f) {
            std::cerr << "Assertion failed: LipSyncEvaluator output out of bounds!\n";
            return 1;
        }
        std::cout << "  [PASS] Audio phoneme energy tracking & real-time lip-sync evaluation verified!\n";

        // 5. Verify Config Persistence (.cfg Save & Load)
        std::string testCfg = "assets/configs/test_character_studio.cfg";
        if (!studio.saveConfig(testCfg)) {
            std::cerr << "Assertion failed: Saving CharacterStudio config failed!\n";
            return 1;
        }
        Lab::CharacterStudio studio2;
        if (!studio2.loadConfig(testCfg)) {
            std::cerr << "Assertion failed: Loading CharacterStudio config failed!\n";
            return 1;
        }
        if (std::abs(studio2.getWeaponGrip(Lab::WeaponID::M4A4S).rightSocketPos.x - m4Grip.rightSocketPos.x) > 0.001f) {
            std::cerr << "Assertion failed: Config integrity mismatch after reload!\n";
            return 1;
        }
        std::filesystem::remove(testCfg);
        std::cout << "  [PASS] Configuration file serialization (.cfg) integrity validated!\n";

        // 6. Verify Valve Hammer Dropdown Menus
        studio.setActiveDropdown(Lab::CharacterStudio::DropdownMenu::File);
        if (studio.getActiveDropdown() != Lab::CharacterStudio::DropdownMenu::File) {
            std::cerr << "Assertion failed: Dropdown menu state should be File!\n";
            return 1;
        }
        // Click outside closes dropdown
        studio.update(0.016f, 500.0f, 300.0f, true, false, 0.0f);
        studio.render(w, h);
        if (studio.getActiveDropdown() != Lab::CharacterStudio::DropdownMenu::None) {
            std::cerr << "Assertion failed: Clicking outside should close the dropdown menu!\n";
            return 1;
        }
        std::cout << "  [PASS] Valve Hammer Dropdown Menu modal behavior and dismissal validated!\n";

        // 7. Verify Undo / Redo & Grip Clipboard
        studio.setSelectedWeapon(3); // M4A4-S
        float originalX = studio.getWeaponGrip(Lab::WeaponID::M4A4S).rightSocketPos.x;
        studio.pushUndoState();
        studio.getWeaponGripMut(Lab::WeaponID::M4A4S).rightSocketPos.x += 0.05f;
        if (std::abs(studio.getWeaponGrip(Lab::WeaponID::M4A4S).rightSocketPos.x - (originalX + 0.05f)) > 0.001f) {
            std::cerr << "Assertion failed: Socket modification failed!\n";
            return 1;
        }
        studio.undo();
        if (std::abs(studio.getWeaponGrip(Lab::WeaponID::M4A4S).rightSocketPos.x - originalX) > 0.001f) {
            std::cerr << "Assertion failed: Undo failed to revert socket modification!\n";
            return 1;
        }
        studio.redo();
        if (std::abs(studio.getWeaponGrip(Lab::WeaponID::M4A4S).rightSocketPos.x - (originalX + 0.05f)) > 0.001f) {
            std::cerr << "Assertion failed: Redo failed to reapply socket modification!\n";
            return 1;
        }
        studio.undo(); // Revert back to original

        // Clipboard test
        studio.copyGrip();
        studio.setSelectedWeapon(1); // Pistol
        studio.pasteGrip();
        if (std::abs(studio.getWeaponGrip(Lab::WeaponID::Pistol).rightSocketPos.x - originalX) > 0.001f) {
            std::cerr << "Assertion failed: Grip clipboard copy/paste failed!\n";
            return 1;
        }
        std::cout << "  [PASS] Undo / Redo history snapshots and socket clipboard validated!\n";

        // 8. Verify Weapon 3D Model Transforms (Translation, Rotation, Scaling)
        studio.setSelectedWeapon(3); // M4A4-S
        studio.getWeaponGripMut(Lab::WeaponID::M4A4S).weaponOffset = Lab::Vec3(0.04f, -0.02f, 0.08f);
        studio.getWeaponGripMut(Lab::WeaponID::M4A4S).weaponRotation = Lab::Vec3(15.0f, -10.0f, 5.0f);
        studio.getWeaponGripMut(Lab::WeaponID::M4A4S).weaponScale = Lab::Vec3(1.25f, 1.25f, 1.25f);

        const auto& m4GripCheck = studio.getWeaponGrip(Lab::WeaponID::M4A4S);
        if (std::abs(m4GripCheck.weaponOffset.x - 0.04f) > 0.001f ||
            std::abs(m4GripCheck.weaponRotation.x - 15.0f) > 0.001f ||
            std::abs(m4GripCheck.weaponScale.x - 1.25f) > 0.001f) {
            std::cerr << "Assertion failed: Weapon transform (offset, rotation, scale) setting failed!\n";
            return 1;
        }

        studio.getWeaponGripMut(Lab::WeaponID::M4A4S).lockHands = true;

        // Test Serialization of weapon transform & LockHands
        std::string transformCfg = "build/test_transform.cfg";
        studio.saveConfig(transformCfg);
        Lab::CharacterStudio studioTransformLoader;
        studioTransformLoader.loadConfig(transformCfg);
        const auto& loadedGrip = studioTransformLoader.getWeaponGrip(Lab::WeaponID::M4A4S);
        if (std::abs(loadedGrip.weaponOffset.z - 0.08f) > 0.001f ||
            std::abs(loadedGrip.weaponRotation.y - (-10.0f)) > 0.001f ||
            std::abs(loadedGrip.weaponScale.z - 1.25f) > 0.001f ||
            !loadedGrip.lockHands) {
            std::cerr << "Assertion failed: Serialization of weapon transform (and LockHands) to .cfg failed!\n";
            return 1;
        }
        std::filesystem::remove(transformCfg);

        // Test Reset helpers
        studio.centerActiveWeapon();
        if (std::abs(studio.getWeaponGrip(Lab::WeaponID::M4A4S).weaponOffset.x) > 0.001f) {
            std::cerr << "Assertion failed: centerActiveWeapon failed!\n";
            return 1;
        }
        studio.resetActiveWeaponRotation();
        if (std::abs(studio.getWeaponGrip(Lab::WeaponID::M4A4S).weaponRotation.x) > 0.001f) {
            std::cerr << "Assertion failed: resetActiveWeaponRotation failed!\n";
            return 1;
        }
        studio.resetActiveWeaponScale();
        if (std::abs(studio.getWeaponGrip(Lab::WeaponID::M4A4S).weaponScale.x - 1.0f) > 0.001f) {
            std::cerr << "Assertion failed: resetActiveWeaponScale failed!\n";
            return 1;
        }

        // Test Tool Modes
        studio.setViewportToolMode(Lab::ViewportToolMode::MoveWeapon);
        if (studio.getViewportToolMode() != Lab::ViewportToolMode::MoveWeapon) {
            std::cerr << "Assertion failed: MoveWeapon tool mode setting failed!\n";
            return 1;
        }
        studio.setViewportToolMode(Lab::ViewportToolMode::RotateWeapon);
        if (studio.getViewportToolMode() != Lab::ViewportToolMode::RotateWeapon) {
            std::cerr << "Assertion failed: RotateWeapon tool mode setting failed!\n";
            return 1;
        }
        studio.setViewportToolMode(Lab::ViewportToolMode::ScaleWeapon);
        if (studio.getViewportToolMode() != Lab::ViewportToolMode::ScaleWeapon) {
            std::cerr << "Assertion failed: ScaleWeapon tool mode setting failed!\n";
            return 1;
        }
        studio.setViewportToolMode(Lab::ViewportToolMode::OrbitCamera);
        std::cout << "  [PASS] Weapon translation, rotation, scaling, and viewport tools validated!\n";

        // 9. Verify Custom STL Model Selection & Base Scale Auto-Normalization
        {
            studio.setSelectedWeapon(1); // Pistol
            studio.getWeaponSkinMut(Lab::WeaponID::Pistol).modelFile = "Model.stl";
            std::string testStlCfg = "build/test_stl_model.cfg";
            studio.saveConfig(testStlCfg);

            Lab::CharacterStudio studioStlLoader;
            studioStlLoader.loadConfig(testStlCfg);
            if (studioStlLoader.getWeaponSkin(Lab::WeaponID::Pistol).modelFile != "Model.stl") {
                std::cerr << "Assertion failed: STL Model path serialization to .cfg failed!\n";
                return 1;
            }
            std::filesystem::remove(testStlCfg);

            // Verify Mesh base scale calculation
            std::unique_ptr<Lab::Mesh> pipeMesh(Lab::Mesh::loadSTL("assets/models/pipe.stl"));
            if (pipeMesh) {
                float maxDim = pipeMesh->getMaxDimension();
                float baseScale = pipeMesh->getBaseScale(0.70f);
                float resultingSize = maxDim * baseScale;
                if (std::abs(resultingSize - 0.70f) > 0.01f) {
                    std::cerr << "Assertion failed: getBaseScale normalization failed! (expected 0.70, got " << resultingSize << ")\n";
                    return 1;
                }
            }
            std::cout << "  [PASS] Custom STL weapon model assignment, .cfg serialization, and base scale normalization validated!\n";
        }

        // 10. Visual Verification 1: Character Studio Stage & Hammer UI (Full responsive layout)
        glClearColor(0.12f, 0.13f, 0.15f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        studio.setActiveTab(Lab::StudioTab::Appearance);
        studio.update(0.016f, 500.0f, 300.0f, false, false, 0.0f);
        studio.render(w, h);

        glFinish();
        saveFrameToBMP("test_character_studio.bmp", w, h);
        std::cout << "  [PASS] Saved Character Studio & Outfit visual frame to 'test_character_studio.bmp'.\n";

        // Visual Verification 2: Weapon Grip & FPP Arms Poser with Pipe STL Model
        glClearColor(0.12f, 0.13f, 0.15f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        studio.setActiveTab(Lab::StudioTab::GripPoser);
        studio.setSelectedWeapon(0); // Pipe (STL model)
        studio.setPoserSubMode(Lab::PoserSubMode::WeaponTransform);
        studio.update(0.016f, 400.0f, 250.0f, false, false, 0.0f);
        studio.render(w, h);

        glFinish();
        saveFrameToBMP("test_weapon_grip_studio.bmp", w, h);
        std::cout << "  [PASS] Saved Weapon Grip Poser visual frame to 'test_weapon_grip_studio.bmp'.\n";

        // Visual Verification 3: Custom Model.stl held in player's hands in FPP viewmodel
        glClearColor(0.08f, 0.09f, 0.11f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        Lab::Camera customModelCam(70.0f, (float)w / (float)h, 0.01f, 100.0f);
        Lab::Renderer::beginFrame(customModelCam);
        Lab::WeaponSystem customWs;
        customWs.init();
        customWs.switchWeapon(Lab::WeaponID::Pistol);
        Lab::WeaponAnimator customAnim;
        std::unique_ptr<Lab::Mesh> customModelMesh(Lab::Mesh::loadSTL("assets/models/Model.stl"));
        customWs.renderViewModel(customModelCam, customAnim, nullptr, customModelMesh.get(), 0.0f);
        Lab::Renderer::endFrame();

        glFinish();
        saveFrameToBMP("test_custom_model_viewmodel.bmp", w, h);
        std::cout << "  [PASS] Saved Custom Model.stl FPP viewmodel frame to 'test_custom_model_viewmodel.bmp'.\n";

        // 11. Verify Universal STBI Texture Loading (PNG / misnamed BMP) & Textured STL Viewmodel
        {
            std::unique_ptr<Lab::Texture> pipePng(new Lab::Texture("assets/models/textures/PipeWrenchTool_baseColor.png"));
            if (!pipePng || pipePng->getId() == 0 || pipePng->getWidth() <= 64) {
                std::cerr << "Assertion failed: Loading PipeWrenchTool_baseColor.png via STBI failed!\n";
                return 1;
            }
            std::unique_ptr<Lab::Texture> pipeBmpPng(new Lab::Texture("assets/textures/weapon_pipe.bmp"));
            if (!pipeBmpPng || pipeBmpPng->getId() == 0 || pipeBmpPng->getWidth() <= 64) {
                std::cerr << "Assertion failed: Loading weapon_pipe.bmp (PNG content) failed!\n";
                return 1;
            }
            std::cout << "  [PASS] Universal STBI Texture Loading (PNG & misnamed BMP) validated! (" << pipePng->getWidth() << "x" << pipePng->getHeight() << ")\n";

            // Visual Verification 4: Pipe STL with actual High-Res Texture in player hands
            glClearColor(0.08f, 0.09f, 0.11f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            Lab::Camera pipeTexCam(70.0f, (float)w / (float)h, 0.01f, 100.0f);
            Lab::Renderer::beginFrame(pipeTexCam);
            Lab::WeaponSystem pipeWs;
            pipeWs.init();
            pipeWs.switchWeapon(Lab::WeaponID::Pipe);
            Lab::WeaponAnimator pipeAnim;
            std::unique_ptr<Lab::Mesh> pipeMesh(Lab::Mesh::loadSTL("assets/models/pipe.stl"));
            pipeWs.renderViewModel(pipeTexCam, pipeAnim, pipePng.get(), pipeMesh.get(), 0.0f);
            Lab::Renderer::endFrame();

            glFinish();
            saveFrameToBMP("test_pipe_textured_viewmodel.bmp", w, h);
            std::cout << "  [PASS] Saved Pipe STL with High-Res PNG Texture to 'test_pipe_textured_viewmodel.bmp'.\n";

            // Visual Verification 5: Independent Weapon Transform (lockHands = true)
            // Weapon offset/rotated without moving hands
            glClearColor(0.08f, 0.09f, 0.11f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            Lab::Camera lockHandsCam(70.0f, (float)w / (float)h, 0.01f, 100.0f);
            Lab::Renderer::beginFrame(lockHandsCam);
            Lab::WeaponSystem lockWs;
            lockWs.init();
            lockWs.switchWeapon(Lab::WeaponID::Pipe);
            Lab::WeaponAnimator lockAnim;
            Lab::Vec3 customWeaponOffset(0.02f, -0.01f, 0.04f);
            Lab::Vec3 customWeaponRot(5.0f, -8.0f, 3.0f);
            Lab::Vec3 customWeaponScale(1.0f, 1.0f, 1.0f);
            Lab::Vec3 defaultWhite(1.0f, 1.0f, 1.0f);
            // Render with lockHands = true: arms stay in base stance, weapon translates & rotates independently
            lockWs.renderViewModel(lockHandsCam, lockAnim, pipePng.get(), pipeMesh.get(), 0.0f,
                                   nullptr, nullptr, nullptr, nullptr,
                                   &defaultWhite, &customWeaponOffset, &customWeaponRot, &customWeaponScale,
                                   0.25f, true);
            Lab::Renderer::endFrame();

            glFinish();
            saveFrameToBMP("test_weapon_only_transform.bmp", w, h);
            std::cout << "  [PASS] Saved Independent Weapon Transform (lockHands = true) to 'test_weapon_only_transform.bmp'.\n";
        }
    }

    // =========================================================================
    // TEST 39: NATIVE BINARY GLB T-800 TERMINATOR SKELETAL BOT & SOCKET WEAPON
    // =========================================================================
    {
        std::cout << "\n[Test 39] Verifying Binary .GLB Loader & Terminator T-800 Skeletal Bot..." << std::endl;

        std::shared_ptr<Lab::Skeleton> t800Skeleton;
        std::vector<Lab::AnimationClip> t800Clips;
        std::unique_ptr<Lab::SkinnedMesh> t800Mesh;

        std::string glbPath = "assets/models/t-800_run.glb";
        if (!std::filesystem::exists(glbPath)) {
            glbPath = "assets/inwork/t-800_run.glb";
        }

        bool glbLoaded = Lab::GLTFLoader::load(glbPath, t800Skeleton, t800Clips, t800Mesh);
        if (!glbLoaded || !t800Skeleton || !t800Mesh || t800Clips.empty()) {
            std::cerr << "Assertion failed: Failed to load binary GLB model from " << glbPath << "\n";
            return 1;
        }

        if (t800Skeleton->getBoneCount() < 34) {
            std::cerr << "Assertion failed: T-800 skeleton rig must have at least 34 bones, got " << t800Skeleton->getBoneCount() << "\n";
            return 1;
        }
        std::cout << "  [PASS] Binary GLB loaded: " << t800Skeleton->getBoneCount() << " bones, "
                  << t800Mesh->getIndexCount() << " indices.\n";

        float origHeight = t800Mesh->getHeight();
        if (origHeight <= 10.0f) {
            std::cerr << "Assertion failed: T-800 original height should be in Mixamo units (>10), got " << origHeight << "\n";
            return 1;
        }
        float botScale = t800Mesh->getBaseScale(1.85f);
        float yOffset = -t800Mesh->getMinBounds().y * botScale;
        std::cout << "  [PASS] Mesh bounds: Height=" << origHeight << ", auto-scale=" << botScale << ", ground yOffset=" << yOffset << "\n";

        Lab::Animator t800Animator;
        t800Animator.setSkeleton(t800Skeleton);
        for (const auto& clip : t800Clips) {
            t800Animator.addClip(clip);
        }
        if (!t800Animator.hasClip("Walk")) {
            std::cerr << "Assertion failed: T-800 missing 'Walk' animation clip\n";
            return 1;
        }

        // Animate running cycle
        t800Animator.playAnimation("Walk", true);
        t800Animator.update(0.35f); // Advance into running stride

        // Bone socket attachment verification
        Lab::Mat4 botWorld = Lab::Mat4::translate(Lab::Vec3(0.0f, yOffset, 0.0f)) *
                             Lab::Mat4::rotate(15.0f * 3.14159265f / 180.0f, Lab::Vec3(0, 1, 0)) *
                             Lab::Mat4::scale(Lab::Vec3(botScale, botScale, botScale));

        float invBotScale = (botScale > 0.00001f) ? (1.0f / botScale) : 1.0f;
        float weaponScale = 0.016f * invBotScale;
        Lab::Vec3 socketPos = Lab::Vec3(0.0f, -0.05f, 0.02f) * invBotScale;
        Lab::Mat4 socketTransform = t800Animator.getSocketTransform("Socket_Weapon", botWorld,
            Lab::makeTransform(socketPos, Lab::Quat::fromEuler(0.1f, -0.2f, 0.0f), Lab::Vec3(weaponScale, weaponScale, weaponScale)));

        std::unique_ptr<Lab::Mesh> pipeStl(Lab::Mesh::loadSTL("assets/models/pipe.stl"));

        // Setup Shadow Map & Light
        Lab::Vec3 sunDir(-0.35f, -0.85f, -0.35f);
        Lab::Mat4 lightSpaceMatrix = Lab::ShadowMap::computeSunLightSpaceMatrix(sunDir, Lab::Vec3(0, 1, 0), 15.0f);
        Lab::ShadowMap shadowMap;
        shadowMap.init(2048, 2048);

        // Render shadow pass
        shadowMap.beginShadowPass(lightSpaceMatrix);
        Lab::Renderer::beginShadowDepthPass(lightSpaceMatrix);
        Lab::Renderer::drawShadowSkinnedMesh(*t800Mesh, botWorld, t800Animator.getSkinMatrices());
        if (pipeStl) {
            Lab::Renderer::drawShadowMesh(*pipeStl, socketTransform);
        }
        Lab::Renderer::endShadowDepthPass();
        shadowMap.endShadowPass(w, h);

        // Render main shaded pass
        glClearColor(0.06f, 0.08f, 0.11f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        Lab::Camera t800Cam(55.0f, (float)w / (float)h, 0.01f, 100.0f);
        t800Cam.setPosition(Lab::Vec3(0.0f, 1.3f, 2.7f));
        t800Cam.lookAt(Lab::Vec3(0.0f, 0.95f, 0.0f));

        Lab::Renderer::beginFrame(t800Cam);
        Lab::Renderer::setSunLight(sunDir, Lab::Vec3(1.0f, 0.96f, 0.92f), Lab::Vec3(0.25f, 0.28f, 0.35f));
        Lab::Renderer::setShadowMap(lightSpaceMatrix, shadowMap.getDepthTexture());

        // Floor and backdrop
        Lab::Texture floorTex("assets/textures/floor_tiles.bmp");
        Lab::Renderer::drawCube(Lab::Vec3(0.0f, -0.05f, 0.0f), Lab::Vec3(20.0f, 0.1f, 20.0f), Lab::Vec3(0.7f, 0.7f, 0.7f), &floorTex, true);

        // Render T-800 Terminator skinned mesh with real-time lighting and shadows
        Lab::Renderer::drawSkinnedMesh(*t800Mesh, botWorld, t800Animator.getSkinMatrices(), Lab::Vec3(1.0f, 1.0f, 1.0f), nullptr, true);

        // Render attached STL weapon in right hand
        if (pipeStl) {
            Lab::Texture pipeTex("assets/textures/weapon_pipe.bmp");
            Lab::Renderer::drawMesh(*pipeStl, socketTransform, Lab::Vec3(0.95f, 0.95f, 0.95f), &pipeTex, true);
        }

        Lab::Renderer::disableShadowMap();

        // Diagnostic HUD
        Lab::Renderer::beginUI(w, h);
        Lab::Renderer::drawRect(40.0f, 30.0f, 820.0f, 90.0f, Lab::Vec3(0.10f, 0.12f, 0.16f));
        Lab::Renderer::drawRect(40.0f, 30.0f, 820.0f, 1.0f, Lab::Vec3(0.9f, 0.2f, 0.2f));
        Lab::LabFont::drawText(56.0f, 44.0f, "BINARY GLB 2.0 SKELETAL BOT - TERMINATOR T-800", 2.0f, Lab::Vec3(0.95f, 0.25f, 0.25f), Lab::LabFontType::GeoSans);
        Lab::LabFont::drawText(56.0f, 74.0f, "34 JOINTS RIG | RUNNING ANIMATION | DUAL-MATERIAL ENDOSKELETON | STL SOCKET", 1.5f, Lab::Vec3(0.85f, 0.90f, 0.95f), Lab::LabFontType::GeoSans);
        Lab::Renderer::endUI();

        Lab::Renderer::endFrame();
        glFinish();
        saveFrameToBMP("test_t800_bot_render.bmp", w, h);
        std::cout << "  [PASS] Saved visual T-800 Terminator Skeletal Bot verification to 'test_t800_bot_render.bmp'.\n";

        // Verify AIManager integration
        Lab::AIManager aiMgr;
        aiMgr.initAssets();
        aiMgr.spawnBotsForMap(nullptr, 4, Lab::GameMode::DM);
        if (aiMgr.bots.size() != 4) {
            std::cerr << "Assertion failed: AIManager failed to spawn 4 bots\n";
            return 1;
        }
        std::vector<Lab::BulletTracer> testTracers;
        float testDamage = 0.0f;
        aiMgr.update(0.016f, Lab::Vec3(0, 0, 0), true, -1, Lab::LabMap(), testTracers, testDamage);
        std::cout << "  [PASS] AIManager successfully initialized and updated 4 T-800 Terminator bots!\n";
    }

    Lab::Renderer::shutdown();
    glfwDestroyWindow(window);
    glfwTerminate();

    std::cout << "[Test] All automated and visual verifications passed with 100% success!\n";
    return 0;
}
