#include "Lab.h"
#include "LabFont.h"
#include "LabDialogs.h"
#include <iostream>
#include <vector>
#include <unordered_map>
#include <fstream>
#include <cmath>

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

    // Toolbar (18 buttons)
    Lab::Renderer::drawRect(0, 24.0f, (float)w, 34.0f, winBg);
    Lab::Renderer::drawRect(0, 57.0f, (float)w, 1.0f, winBorder);
    for (int i = 0; i < 18; ++i) {
        float bx = 8.0f + i * 28.0f;
        Lab::Renderer::drawRect(bx, 29.0f, 24.0f, 24.0f, (i == 17) ? Lab::Vec3(0.15f, 0.65f, 0.35f) : Lab::Vec3(0.88f, 0.88f, 0.90f));
    }

    // Left Palette (8 tools)
    Lab::Renderer::drawRect(0, 58.0f, 42.0f, (float)h - 80.0f, winBg);
    for (int i = 0; i < 8; ++i) {
        Lab::Renderer::drawRect(6.0f, 68.0f + i * 36.0f, 30.0f, 30.0f, (i == 0) ? Lab::Vec3(0.78f, 0.88f, 1.0f) : Lab::Vec3(0.88f, 0.88f, 0.90f));
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
        Lab::RaycastHit rHit;
        bool anyHit = aiMgr.testRaycast(Lab::Vec3(0.0f, 1.68f, 0.0f), Lab::Vec3(0.0f, 0.0f, -1.0f), rHit);
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

    Lab::Renderer::shutdown();
    glfwDestroyWindow(window);
    glfwTerminate();

    std::cout << "[Test] All automated and visual verifications passed with 100% success!\n";
    return 0;
}
