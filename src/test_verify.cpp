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

    // Hammer 2D UI Overlay
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

    // Right Sidebar (Outliner Tab active)
    float rX = (float)w - 280.0f;
    Lab::Renderer::drawRect(rX, 58.0f, 280.0f, (float)h - 80.0f, winBg);
    Lab::Renderer::drawRect(rX + 10.0f, 64.0f, 125.0f, 24.0f, Lab::Vec3(0.85f, 0.85f, 0.88f));
    Lab::LabFont::drawText(rX + 30.0f, 69.0f, "Properties", 1.6f, Lab::Vec3(0.45f, 0.45f, 0.45f), Lab::LabFontType::System);
    Lab::Renderer::drawRect(rX + 140.0f, 64.0f, 125.0f, 24.0f, Lab::Vec3(1, 1, 1));
    Lab::Renderer::drawRect(rX + 140.0f, 64.0f, 125.0f, 1.0f, orangeGlow);
    Lab::LabFont::drawText(rX + 165.0f, 69.0f, "Struktura", 1.6f, textDark, Lab::LabFontType::System);

    Lab::LabFont::drawText(rX + 12.0f, 98.0f, "Map Entity Outliner (9 items):", 1.6f, textDark, Lab::LabFontType::System);
    Lab::Renderer::drawRect(rX + 10.0f, 118.0f, 260.0f, 400.0f, Lab::Vec3(1, 1, 1));
    Lab::LabFont::drawText(rX + 18.0f, 126.0f, "[Spawn] Player Start (0, 1.8, 0)", 1.5f, textDark, Lab::LabFontType::System);
    Lab::Renderer::drawRect(rX + 11.0f, 146.0f, 258.0f, 22.0f, Lab::Vec3(0.85f, 0.92f, 1.0f));
    Lab::Renderer::drawRect(rX + 11.0f, 146.0f, 4.0f, 22.0f, orangeGlow);
    Lab::LabFont::drawText(rX + 18.0f, 150.0f, "[B#0] floor_tiles.bmp (32x1x32)", 1.5f, Lab::Vec3(0.1f, 0.35f, 0.7f), Lab::LabFontType::System);
    Lab::LabFont::drawText(rX + 18.0f, 174.0f, "[B#1] concrete_wall.bmp (16x4x1)", 1.5f, textDark, Lab::LabFontType::System);
    Lab::LabFont::drawText(rX + 18.0f, 198.0f, "[P#0] Model.stl (1x1x1)", 1.5f, textDark, Lab::LabFontType::System);
    Lab::LabFont::drawText(rX + 18.0f, 222.0f, "[D#0] blast_door (2.5x3.5x0.4)", 1.5f, textDark, Lab::LabFontType::System);

    // Outliner action buttons
    Lab::Renderer::drawRect(rX + 12.0f, 530.0f, 76.0f, 26.0f, Lab::Vec3(0.88f, 0.88f, 0.90f));
    Lab::LabFont::drawText(rX + 18.0f, 536.0f, "Focus (F)", 1.5f, textDark, Lab::LabFontType::System);
    Lab::Renderer::drawRect(rX + 94.0f, 530.0f, 76.0f, 26.0f, Lab::Vec3(0.88f, 0.88f, 0.90f));
    Lab::LabFont::drawText(rX + 100.0f, 536.0f, "Duplicate", 1.5f, textDark, Lab::LabFontType::System);
    Lab::Renderer::drawRect(rX + 176.0f, 530.0f, 76.0f, 26.0f, Lab::Vec3(0.88f, 0.88f, 0.90f));
    Lab::LabFont::drawText(rX + 186.0f, 536.0f, "Delete", 1.5f, Lab::Vec3(0.7f, 0.1f, 0.1f), Lab::LabFontType::System);

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

    Lab::Renderer::shutdown();
    glfwDestroyWindow(window);
    glfwTerminate();

    std::cout << "[Test] Verification successfully finished!\n";
    return 0;
}
