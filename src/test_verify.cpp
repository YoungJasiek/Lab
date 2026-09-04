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
    Lab::Camera cam(75.0f, 16.0f / 9.0f, 0.01f, 1000.0f);
    cam.setPosition(map->spawn.position);
    Lab::Renderer::beginFrame(cam);
    Lab::Renderer::setSunLight(map->metadata.sunDir, map->metadata.sunColor, map->metadata.ambientColor);

    for (const auto& b : map->brushes) {
        Lab::Texture* tex = b.texturePath.empty() ? nullptr : textures[b.texturePath].get();
        Lab::Renderer::drawCube(b.position, b.size, b.color, tex);
    }

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

    Lab::Renderer::shutdown();
    glfwDestroyWindow(window);
    glfwTerminate();

    std::cout << "[Test] Verification successfully finished!\n";
    return 0;
}
