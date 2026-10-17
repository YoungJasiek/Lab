#pragma once
#include "LabRenderer.h"
#include "LabMath.h"
#include <string>
#include <vector>
#include <memory>
#include <filesystem>
#include <algorithm>
#include <fstream>
#include <iostream>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4505)
#endif
#include "stb_truetype.h"
#ifdef _MSC_VER
#pragma warning(pop)
#endif

namespace Lab {

    enum class LabFontType {
        GeoSans,    // Geo Sans Light (fonts/geo_sans_light/GeosansLight.ttf)
        System,     // System UI font (Segoe UI / Arial / DejaVuSans)
        DotMatrix,  // Classic 5x7 dot-matrix font
        Monospace   // Monospaced code font (Consolas / Courier New / DejaVuSansMono)
    };

    struct GlyphMetric {
        float x = 0.0f, y = 0.0f, w = 0.0f, h = 0.0f;
        float u0 = 0.0f, v0 = 0.0f, u1 = 0.0f, v1 = 0.0f;
        float xadvance = 0.0f;
    };

    struct FontAtlas {
        std::unique_ptr<Texture> texture;
        GlyphMetric glyphs[96]; // ASCII 32-126
        float fontHeight = 28.0f;
        bool valid = false;
    };

    class LabFont {
    public:
        // Returns 5 columns of 7-bit binary masks for ASCII characters 32-126 (fallback)
        static const unsigned char* getDotMatrixGlyph(char c) {
            static const unsigned char fontTable[96][5] = {
                {0x00, 0x00, 0x00, 0x00, 0x00}, // ' ' (32)
                {0x00, 0x00, 0x5F, 0x00, 0x00}, // '!'
                {0x00, 0x07, 0x00, 0x07, 0x00}, // '"'
                {0x14, 0x7F, 0x14, 0x7F, 0x14}, // '#'
                {0x24, 0x2A, 0x7F, 0x2A, 0x12}, // '$'
                {0x23, 0x13, 0x08, 0x64, 0x62}, // '%'
                {0x36, 0x49, 0x55, 0x22, 0x50}, // '&'
                {0x00, 0x05, 0x03, 0x00, 0x00}, // '''
                {0x00, 0x1C, 0x22, 0x41, 0x00}, // '('
                {0x00, 0x41, 0x22, 0x1C, 0x00}, // ')'
                {0x14, 0x08, 0x3E, 0x08, 0x14}, // '*'
                {0x08, 0x08, 0x3E, 0x08, 0x08}, // '+'
                {0x00, 0x50, 0x30, 0x00, 0x00}, // ','
                {0x08, 0x08, 0x08, 0x08, 0x08}, // '-'
                {0x00, 0x60, 0x60, 0x00, 0x00}, // '.'
                {0x20, 0x10, 0x08, 0x04, 0x02}, // '/'
                {0x3E, 0x51, 0x49, 0x45, 0x3E}, // '0'
                {0x00, 0x42, 0x7F, 0x40, 0x00}, // '1'
                {0x42, 0x61, 0x51, 0x49, 0x46}, // '2'
                {0x21, 0x41, 0x45, 0x4B, 0x31}, // '3'
                {0x18, 0x14, 0x12, 0x7F, 0x10}, // '4'
                {0x27, 0x45, 0x45, 0x45, 0x39}, // '5'
                {0x3C, 0x4A, 0x49, 0x49, 0x30}, // '6'
                {0x01, 0x71, 0x09, 0x05, 0x03}, // '7'
                {0x36, 0x49, 0x49, 0x49, 0x36}, // '8'
                {0x06, 0x49, 0x49, 0x29, 0x1E}, // '9'
                {0x00, 0x36, 0x36, 0x00, 0x00}, // ':'
                {0x00, 0x56, 0x36, 0x00, 0x00}, // ';'
                {0x08, 0x14, 0x22, 0x41, 0x00}, // '<'
                {0x14, 0x14, 0x14, 0x14, 0x14}, // '='
                {0x00, 0x41, 0x22, 0x14, 0x08}, // '>'
                {0x02, 0x01, 0x51, 0x09, 0x06}, // '?'
                {0x32, 0x49, 0x79, 0x41, 0x3E}, // '@'
                {0x7E, 0x11, 0x11, 0x11, 0x7E}, // 'A'
                {0x7F, 0x49, 0x49, 0x49, 0x36}, // 'B'
                {0x3E, 0x41, 0x41, 0x41, 0x22}, // 'C'
                {0x7F, 0x41, 0x41, 0x22, 0x1C}, // 'D'
                {0x7F, 0x49, 0x49, 0x49, 0x41}, // 'E'
                {0x7F, 0x09, 0x09, 0x09, 0x01}, // 'F'
                {0x3E, 0x41, 0x49, 0x49, 0x7A}, // 'G'
                {0x7F, 0x08, 0x08, 0x08, 0x7F}, // 'H'
                {0x00, 0x41, 0x7F, 0x41, 0x00}, // 'I'
                {0x20, 0x40, 0x41, 0x3F, 0x01}, // 'J'
                {0x7F, 0x08, 0x14, 0x22, 0x41}, // 'K'
                {0x7F, 0x40, 0x40, 0x40, 0x40}, // 'L'
                {0x7F, 0x02, 0x0C, 0x02, 0x7F}, // 'M'
                {0x7F, 0x04, 0x08, 0x10, 0x7F}, // 'N'
                {0x3E, 0x41, 0x41, 0x41, 0x3E}, // 'O'
                {0x7F, 0x09, 0x09, 0x09, 0x06}, // 'P'
                {0x3E, 0x41, 0x51, 0x21, 0x5E}, // 'Q'
                {0x7F, 0x09, 0x19, 0x29, 0x46}, // 'R'
                {0x46, 0x49, 0x49, 0x49, 0x31}, // 'S'
                {0x01, 0x01, 0x7F, 0x01, 0x01}, // 'T'
                {0x3F, 0x40, 0x40, 0x40, 0x3F}, // 'U'
                {0x1F, 0x20, 0x40, 0x20, 0x1F}, // 'V'
                {0x3F, 0x40, 0x38, 0x40, 0x3F}, // 'W'
                {0x63, 0x14, 0x08, 0x14, 0x63}, // 'X'
                {0x07, 0x08, 0x70, 0x08, 0x07}, // 'Y'
                {0x61, 0x51, 0x49, 0x45, 0x43}, // 'Z'
                {0x00, 0x7F, 0x41, 0x41, 0x00}, // '['
                {0x02, 0x04, 0x08, 0x10, 0x20}, // '\'
                {0x00, 0x41, 0x41, 0x7F, 0x00}, // ']'
                {0x04, 0x02, 0x01, 0x02, 0x04}, // '^'
                {0x40, 0x40, 0x40, 0x40, 0x40}, // '_'
                {0x00, 0x01, 0x02, 0x04, 0x00}, // '`'
                {0x20, 0x54, 0x54, 0x54, 0x78}, // 'a'
                {0x7F, 0x48, 0x44, 0x44, 0x38}, // 'b'
                {0x38, 0x44, 0x44, 0x44, 0x20}, // 'c'
                {0x38, 0x44, 0x44, 0x48, 0x7F}, // 'd'
                {0x38, 0x54, 0x54, 0x54, 0x18}, // 'e'
                {0x08, 0x7E, 0x09, 0x01, 0x02}, // 'f'
                {0x0C, 0x52, 0x52, 0x52, 0x3E}, // 'g'
                {0x7F, 0x08, 0x04, 0x04, 0x78}, // 'h'
                {0x00, 0x44, 0x7D, 0x40, 0x00}, // 'i'
                {0x20, 0x40, 0x44, 0x3D, 0x00}, // 'j'
                {0x7F, 0x10, 0x28, 0x44, 0x00}, // 'k'
                {0x00, 0x41, 0x7F, 0x40, 0x00}, // 'l'
                {0x7C, 0x04, 0x18, 0x04, 0x78}, // 'm'
                {0x7C, 0x08, 0x04, 0x04, 0x78}, // 'n'
                {0x38, 0x44, 0x44, 0x44, 0x38}, // 'o'
                {0x7C, 0x14, 0x14, 0x14, 0x08}, // 'p'
                {0x08, 0x14, 0x14, 0x18, 0x7C}, // 'q'
                {0x7C, 0x08, 0x04, 0x04, 0x08}, // 'r'
                {0x48, 0x54, 0x54, 0x54, 0x20}, // 's'
                {0x04, 0x3F, 0x44, 0x40, 0x20}, // 't'
                {0x3C, 0x40, 0x40, 0x20, 0x7C}, // 'u'
                {0x1C, 0x20, 0x40, 0x20, 0x1C}, // 'v'
                {0x3C, 0x40, 0x30, 0x40, 0x3C}, // 'w'
                {0x44, 0x28, 0x10, 0x28, 0x44}, // 'x'
                {0x0C, 0x50, 0x50, 0x50, 0x3C}, // 'y'
                {0x44, 0x64, 0x54, 0x4C, 0x44}, // 'z'
                {0x00, 0x08, 0x36, 0x41, 0x00}, // '{'
                {0x00, 0x00, 0x7F, 0x00, 0x00}, // '|'
                {0x00, 0x41, 0x36, 0x08, 0x00}, // '}'
                {0x08, 0x08, 0x2A, 0x1C, 0x08}  // '~'
            };

            int idx = (int)c - 32;
            if (idx < 0 || idx >= 96) idx = 0;
            return fontTable[idx];
        }

        static void drawDotMatrixChar(float x, float y, char c, float scale, const Vec3& color, float alpha = 1.0f) {
            const unsigned char* cols = getDotMatrixGlyph(c);
            for (int col = 0; col < 5; ++col) {
                unsigned char bits = cols[col];
                for (int row = 0; row < 7; ++row) {
                    if (bits & (1 << row)) {
                        Renderer::drawRect(x + col * scale, y + row * scale, scale, scale, color, alpha);
                    }
                }
            }
        }

        static void drawDotMatrixText(float x, float y, const std::string& text, float scale, const Vec3& color, float alpha = 1.0f) {
            float cursorX = x;
            float charWidth = 6.0f * scale;
            for (char c : text) {
                if (c == '\n') {
                    y += 9.0f * scale;
                    cursorX = x;
                    continue;
                }
                drawDotMatrixChar(cursorX, y, c, scale, color, alpha);
                cursorX += charWidth;
            }
        }

        // Resolves file paths across dev, debug, and release root environments
        static std::string resolveFontPath(const std::string& path) {
            if (path.empty()) return "";
            std::vector<std::string> candidates = {
                path,
                "fonts/geo_sans_light/" + path,
                "../fonts/geo_sans_light/" + path,
                "../../fonts/geo_sans_light/" + path,
                "engine/assets/fonts/geo_sans_light/" + path,
                "../engine/assets/fonts/geo_sans_light/" + path,
                "fonts/novecento_sans/" + path,
                "../fonts/novecento_sans/" + path,
                "fonts/" + path,
                "../fonts/" + path,
                "assets/fonts/" + path,
#if defined(_WIN32)
                "C:/Windows/Fonts/" + path,
                "C:/Windows/Fonts/segoeui.ttf",
                "C:/Windows/Fonts/arial.ttf",
                "C:/Windows/Fonts/consola.ttf",
#elif defined(__linux__) || defined(__unix__)
                "/usr/share/fonts/truetype/" + path,
                "/usr/share/fonts/truetype/dejavu/" + path,
                "/usr/share/fonts/truetype/liberation/" + path,
                "/usr/share/fonts/TTF/" + path,
                "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
                "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",
                "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf",
#endif
            };

            for (const auto& candidate : candidates) {
                if (!candidate.empty() && std::filesystem::exists(candidate)) {
                    return candidate;
                }
            }
            return path;
        }

        // Cross-platform TrueType rasterizer powered by stb_truetype.h
        static FontAtlas buildAtlasSTB(const std::string& ttfFile, float fontHeight = 26.0f) {
            FontAtlas atlas;
            atlas.fontHeight = fontHeight;
            atlas.valid = false;

            if (ttfFile.empty()) return atlas;
            std::string resolved = resolveFontPath(ttfFile);
            if (!std::filesystem::exists(resolved)) return atlas;

            std::ifstream file(resolved, std::ios::binary | std::ios::ate);
            if (!file.is_open()) return atlas;

            std::streamsize size = file.tellg();
            file.seekg(0, std::ios::beg);
            if (size <= 0) return atlas;

            std::vector<unsigned char> fontBuffer((size_t)size);
            if (!file.read(reinterpret_cast<char*>(fontBuffer.data()), size)) return atlas;
            file.close();

            const int ATLAS_SIZE = (fontHeight > 36.0f) ? 1024 : 512;
            std::vector<unsigned char> monoBitmap(ATLAS_SIZE * ATLAS_SIZE, 0);
            stbtt_bakedchar cdata[96];

            int res = stbtt_BakeFontBitmap(
                fontBuffer.data(), 0,
                atlas.fontHeight,
                monoBitmap.data(), ATLAS_SIZE, ATLAS_SIZE,
                32, 96,
                cdata
            );
            (void)res;

            std::vector<unsigned char> rgba(ATLAS_SIZE * ATLAS_SIZE * 4, 0);
            int nonZero = 0;
            for (int i = 0; i < ATLAS_SIZE * ATLAS_SIZE; ++i) {
                unsigned char a = monoBitmap[i];
                if (a > 0) nonZero++;
                rgba[i * 4 + 0] = 255;
                rgba[i * 4 + 1] = 255;
                rgba[i * 4 + 2] = 255;
                rgba[i * 4 + 3] = a;
            }

            if (nonZero < 50) return atlas;

            for (int i = 0; i < 96; ++i) {
                GlyphMetric& gm = atlas.glyphs[i];
                gm.x = cdata[i].xoff;
                gm.y = cdata[i].yoff;
                gm.w = (float)(cdata[i].x1 - cdata[i].x0);
                gm.h = (float)(cdata[i].y1 - cdata[i].y0);
                gm.xadvance = cdata[i].xadvance;
                gm.u0 = (float)cdata[i].x0 / (float)ATLAS_SIZE;
                gm.v0 = (float)cdata[i].y0 / (float)ATLAS_SIZE;
                gm.u1 = (float)cdata[i].x1 / (float)ATLAS_SIZE;
                gm.v1 = (float)cdata[i].y1 / (float)ATLAS_SIZE;
            }

            atlas.texture = std::make_unique<Texture>(rgba.data(), ATLAS_SIZE, ATLAS_SIZE, 4);
            atlas.valid = true;
            return atlas;
        }

#if defined(_WIN32)
        // Windows GDI fallback if TTF files are not present on disk
        static FontAtlas buildAtlasGDI(const std::string& faceName, const std::string& ttfFile = "") {
            FontAtlas atlas;
            atlas.fontHeight = 26.0f;

            if (!ttfFile.empty()) {
                std::string resolved = resolveFontPath(ttfFile);
                if (std::filesystem::exists(resolved)) {
                    AddFontResourceExA(resolved.c_str(), FR_PRIVATE, 0);
                }
            }

            HDC hdc = CreateCompatibleDC(NULL);
            if (!hdc) return atlas;

            HFONT hFont = CreateFontA(
                (int)atlas.fontHeight, 0, 0, 0,
                FW_NORMAL, FALSE, FALSE, FALSE,
                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                CLIP_DEFAULT_PRECIS,
                ANTIALIASED_QUALITY,
                DEFAULT_PITCH | FF_DONTCARE,
                faceName.c_str()
            );
            if (!hFont) {
                DeleteDC(hdc);
                return atlas;
            }

            const int ATLAS_SIZE = 512;
            BITMAPINFO bmi{};
            bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
            bmi.bmiHeader.biWidth = ATLAS_SIZE;
            bmi.bmiHeader.biHeight = -ATLAS_SIZE;
            bmi.bmiHeader.biPlanes = 1;
            bmi.bmiHeader.biBitCount = 32;
            bmi.bmiHeader.biCompression = BI_RGB;

            void* bits = nullptr;
            HBITMAP hbm = CreateDIBSection(hdc, &bmi, DIB_RGB_COLORS, &bits, NULL, 0);
            if (!hbm || !bits) {
                DeleteObject(hFont);
                DeleteDC(hdc);
                return atlas;
            }

            HGDIOBJ oldBmp = SelectObject(hdc, hbm);
            HGDIOBJ oldFont = SelectObject(hdc, hFont);

            memset(bits, 0, ATLAS_SIZE * ATLAS_SIZE * 4);
            SetBkMode(hdc, OPAQUE);
            SetBkColor(hdc, RGB(0, 0, 0));
            SetTextColor(hdc, RGB(255, 255, 255));

            TEXTMETRICA tm{};
            GetTextMetricsA(hdc, &tm);
            int cellH = tm.tmHeight + 4;

            int curX = 4;
            int curY = 4;

            for (int c = 32; c <= 126; ++c) {
                char ch = (char)c;
                SIZE sz{};
                GetTextExtentPoint32A(hdc, &ch, 1, &sz);
                int charW = sz.cx > 0 ? sz.cx : 8;
                int charH = tm.tmHeight > 0 ? tm.tmHeight : 24;

                if (curX + charW + 6 >= ATLAS_SIZE) {
                    curX = 4;
                    curY += cellH + 4;
                }

                if (curY + cellH + 4 >= ATLAS_SIZE) {
                    break;
                }

                TextOutA(hdc, curX, curY, &ch, 1);

                GlyphMetric& gm = atlas.glyphs[c - 32];
                gm.x = 0.0f;
                gm.y = -atlas.fontHeight * 0.78f;
                gm.w = (float)charW;
                gm.h = (float)charH;
                gm.xadvance = (float)charW;
                gm.u0 = (float)curX / (float)ATLAS_SIZE;
                gm.v0 = (float)curY / (float)ATLAS_SIZE;
                gm.u1 = (float)(curX + charW) / (float)ATLAS_SIZE;
                gm.v1 = (float)(curY + charH) / (float)ATLAS_SIZE;

                curX += charW + 6;
            }

            GdiFlush();

            SelectObject(hdc, oldBmp);
            SelectObject(hdc, oldFont);

            std::vector<unsigned char> rgba(ATLAS_SIZE * ATLAS_SIZE * 4);
            const unsigned char* src = (const unsigned char*)bits;
            int nonZero = 0;

            for (int i = 0; i < ATLAS_SIZE * ATLAS_SIZE; ++i) {
                unsigned char b = src[i * 4 + 0];
                unsigned char g = src[i * 4 + 1];
                unsigned char r = src[i * 4 + 2];
                unsigned char a = (std::max)({ r, g, b });
                if (a > 0) nonZero++;

                rgba[i * 4 + 0] = 255;
                rgba[i * 4 + 1] = 255;
                rgba[i * 4 + 2] = 255;
                rgba[i * 4 + 3] = a;
            }

            if (nonZero > 100) {
                atlas.texture = std::make_unique<Texture>(rgba.data(), ATLAS_SIZE, ATLAS_SIZE, 4);
                atlas.valid = true;
            } else {
                atlas.valid = false;
            }

            DeleteObject(hbm);
            DeleteObject(hFont);
            DeleteDC(hdc);

            return atlas;
        }
#endif

        static FontAtlas& getAtlas(LabFontType type, float scale = 1.0f) {
            static FontAtlas geoSansAtlas;
            static FontAtlas geoSansLargeAtlas;
            static FontAtlas systemAtlas;
            static FontAtlas monospaceAtlas;
            static bool initialized = false;

            if (!initialized) {
                // 1. Primary: TrueType files via cross-platform stb_truetype
                geoSansAtlas = buildAtlasSTB("GeosansLight.ttf", 28.0f);
                if (!geoSansAtlas.valid) {
                    geoSansAtlas = buildAtlasSTB("Novecentosanswide-Normal.otf", 28.0f);
                }

                // High-DPI 64px atlas for crisp large HUD numbers (HEV / Health / Ammo)
                geoSansLargeAtlas = buildAtlasSTB("GeosansLight.ttf", 64.0f);
                if (!geoSansLargeAtlas.valid) {
                    geoSansLargeAtlas = buildAtlasSTB("Novecentosanswide-Normal.otf", 64.0f);
                }

#if defined(_WIN32)
                systemAtlas = buildAtlasSTB("segoeui.ttf", 28.0f);
                if (!systemAtlas.valid) systemAtlas = buildAtlasSTB("arial.ttf", 28.0f);
                if (!systemAtlas.valid) systemAtlas = buildAtlasGDI("Segoe UI", "");
                if (!systemAtlas.valid) systemAtlas = buildAtlasGDI("Arial", "");

                if (!geoSansLargeAtlas.valid) {
                    geoSansLargeAtlas = buildAtlasSTB("segoeui.ttf", 64.0f);
                    if (!geoSansLargeAtlas.valid) geoSansLargeAtlas = buildAtlasSTB("arial.ttf", 64.0f);
                }

                monospaceAtlas = buildAtlasSTB("consola.ttf", 26.0f);
                if (!monospaceAtlas.valid) monospaceAtlas = buildAtlasSTB("cour.ttf", 26.0f);
                if (!monospaceAtlas.valid) monospaceAtlas = buildAtlasGDI("Consolas", "");
                if (!monospaceAtlas.valid) monospaceAtlas = buildAtlasGDI("Courier New", "");
#else
                systemAtlas = buildAtlasSTB("DejaVuSans.ttf", 28.0f);
                if (!systemAtlas.valid) systemAtlas = buildAtlasSTB("LiberationSans-Regular.ttf", 28.0f);
                if (!systemAtlas.valid) systemAtlas = buildAtlasSTB("GeosansLight.ttf", 28.0f);

                if (!geoSansLargeAtlas.valid) {
                    geoSansLargeAtlas = buildAtlasSTB("DejaVuSans.ttf", 64.0f);
                }

                monospaceAtlas = buildAtlasSTB("DejaVuSansMono.ttf", 26.0f);
                if (!monospaceAtlas.valid) monospaceAtlas = buildAtlasSTB("LiberationMono-Regular.ttf", 26.0f);
                if (!monospaceAtlas.valid) monospaceAtlas = buildAtlasSTB("GeosansLight.ttf", 26.0f);
#endif

                if (!systemAtlas.valid && geoSansAtlas.valid) {
                    systemAtlas = buildAtlasSTB("GeosansLight.ttf", 28.0f);
                }
                if (!monospaceAtlas.valid && systemAtlas.valid) {
                    monospaceAtlas = buildAtlasSTB("GeosansLight.ttf", 26.0f);
                }

                initialized = true;
            }

            if (type == LabFontType::GeoSans) {
                if (scale >= 2.2f && geoSansLargeAtlas.valid) return geoSansLargeAtlas;
                return geoSansAtlas.valid ? geoSansAtlas : systemAtlas;
            }
            if (type == LabFontType::Monospace) return monospaceAtlas.valid ? monospaceAtlas : systemAtlas;
            if (scale >= 2.4f && geoSansLargeAtlas.valid) return geoSansLargeAtlas;
            return systemAtlas;
        }

        static float getMonospaceAdvance(float scale = 1.0f) {
            FontAtlas& atlas = getAtlas(LabFontType::Monospace, scale);
            if (atlas.valid) {
                float visualScale = scale * (8.5f / atlas.fontHeight);
                float adv = atlas.glyphs['0' - 32].xadvance;
                if (adv <= 0.0f) adv = atlas.glyphs[0].xadvance;
                if (adv <= 0.0f) adv = atlas.fontHeight * 0.55f;
                return adv * visualScale;
            }
            return 6.0f * scale;
        }

        static void drawText(float x, float y, const std::string& text, float scale = 1.0f, const Vec3& color = { 1, 1, 1 }, LabFontType fontType = LabFontType::System, float alpha = 1.0f) {
            if (fontType != LabFontType::DotMatrix) {
                FontAtlas& atlas = getAtlas(fontType, scale);
                if (atlas.valid && atlas.texture) {
                    float visualScale = scale * (8.5f / atlas.fontHeight);
                    float cursorX = x;
                    float cursorY = y;
                    float lineH = (atlas.fontHeight + 4.0f) * visualScale;
                    float monoAdvance = (fontType == LabFontType::Monospace) ? getMonospaceAdvance(scale) : 0.0f;

                    for (char c : text) {
                        if (c == '\n') {
                            cursorY += lineH;
                            cursorX = x;
                            continue;
                        }
                        if (c == '\t') {
                            float tabAdv = (fontType == LabFontType::Monospace) ? (monoAdvance * 4.0f) : (24.0f * visualScale);
                            cursorX += tabAdv;
                            continue;
                        }
                        int idx = (int)c - 32;
                        if (idx < 0 || idx >= 95) {
                            cursorX += (fontType == LabFontType::Monospace) ? monoAdvance : (6.0f * visualScale);
                            continue;
                        }
                        const GlyphMetric& gm = atlas.glyphs[idx];
                        if (c != ' ') {
                            float drawX = cursorX + gm.x * visualScale;
                            float drawY = cursorY + (gm.y + atlas.fontHeight * 0.78f) * visualScale;
                            Renderer::drawTextureRect(
                                drawX, drawY,
                                gm.w * visualScale, gm.h * visualScale,
                                *atlas.texture,
                                gm.u0, gm.v0, gm.u1, gm.v1,
                                color, alpha
                            );
                        }
                        cursorX += (fontType == LabFontType::Monospace) ? monoAdvance : ((gm.xadvance > 0.0f ? gm.xadvance : gm.w + 1.0f) * visualScale);
                    }
                    return;
                }
            }

            drawDotMatrixText(x, y, text, scale, color, alpha);
        }

        static float getTextWidth(const std::string& text, float scale = 1.0f, LabFontType fontType = LabFontType::System) {
            if (fontType == LabFontType::Monospace) {
                float maxW = 0.0f;
                float curW = 0.0f;
                float adv = getMonospaceAdvance(scale);
                for (char c : text) {
                    if (c == '\n') {
                        if (curW > maxW) maxW = curW;
                        curW = 0.0f;
                    } else if (c == '\t') {
                        curW += adv * 4.0f;
                    } else {
                        curW += adv;
                    }
                }
                return (std::max)(maxW, curW);
            }
            if (fontType != LabFontType::DotMatrix) {
                FontAtlas& atlas = getAtlas(fontType, scale);
                if (atlas.valid) {
                    float visualScale = scale * (8.5f / atlas.fontHeight);
                    float maxW = 0.0f;
                    float curW = 0.0f;
                    for (char c : text) {
                        if (c == '\n') {
                            if (curW > maxW) maxW = curW;
                            curW = 0.0f;
                            continue;
                        }
                        int idx = (int)c - 32;
                        if (idx >= 0 && idx < 95) {
                            curW += (atlas.glyphs[idx].xadvance > 0.0f ? atlas.glyphs[idx].xadvance : atlas.glyphs[idx].w + 1.0f) * visualScale;
                        } else {
                            curW += 6.0f * visualScale;
                        }
                    }
                    return (std::max)(maxW, curW);
                }
            }
            return (float)text.length() * 6.0f * scale;
        }

        static float getTextHeight(float scale = 1.0f, LabFontType fontType = LabFontType::System) {
            if (fontType != LabFontType::DotMatrix) {
                FontAtlas& atlas = getAtlas(fontType, scale);
                if (atlas.valid) {
                    float visualScale = scale * (8.5f / atlas.fontHeight);
                    return atlas.fontHeight * visualScale;
                }
            }
            return 7.0f * scale;
        }

    };

}
