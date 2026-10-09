#include "LabTheme.h"
#include <cmath>
#include <cstdio>
#include <algorithm>

namespace Lab {

    static ThemeColors s_themeColors;
    static ThemeLayout s_themeLayout;
    static EngineLanguage s_engineLanguage = EngineLanguage::English;

    ThemeColors& LabTheme::getColors() { return s_themeColors; }
    const ThemeColors& LabTheme::colors() { return s_themeColors; }
    void LabTheme::setColors(const ThemeColors& c) { s_themeColors = c; }
    void LabTheme::resetDefaultColors() { s_themeColors = ThemeColors{}; }

    ThemeLayout& LabTheme::getLayout() { return s_themeLayout; }
    const ThemeLayout& LabTheme::layout() { return s_themeLayout; }

    void LabTheme::setLanguage(EngineLanguage lang) { s_engineLanguage = lang; }
    EngineLanguage LabTheme::getLanguage() { return s_engineLanguage; }

    std::string LabTheme::tr(const std::string& key) {
        if (s_engineLanguage == EngineLanguage::Polish) {
            static const std::unordered_map<std::string, std::string> s_plDict = {
                { "Properties", "Właściwości" },
                { "Hierarchy", "Struktura" },
                { "Prebuilts", "Prebuilty" },
                { "Entities", "Encje" },
                { "Tools", "Narzędzia" },
                { "Brush", "Bryła" },
                { "Light", "Światło" },
                { "Door", "Drzwi" },
                { "Spawn", "Punkt Startowy" },
                { "Save", "Zapisz" },
                { "Open", "Otwórz" },
                { "Play", "Odtwórz" },
                { "Pause", "Wstrzymaj" },
                { "Reset", "Resetuj" },
                { "Language", "Język" },
                { "English", "Angielski" },
                { "Polish", "Polski" }
            };
            auto it = s_plDict.find(key);
            if (it != s_plDict.end()) return it->second;
        }
        return key;
    }

    SidebarLayout getSidebarLayout(float w, float h) {
        const auto& layout = LabTheme::layout();
        SidebarLayout l;
        l.rightW = layout.rightSidebarWidth;
        l.rightX = w - l.rightW;
        l.rightY = layout.rightSidebarY;
        l.rightH = h - l.rightY - 22.0f;

        // Tabs (Properties, Struktura, Prebuilty)
        float tabW = layout.tabWidth;
        l.tabPropX = l.rightX + 8.0f;
        l.tabPropY = l.rightY + 6.0f;
        l.tabPropW = tabW;
        l.tabPropH = layout.tabHeight;

        l.tabOutX = l.tabPropX + tabW + 6.0f;
        l.tabOutY = l.rightY + 6.0f;
        l.tabOutW = tabW;
        l.tabOutH = layout.tabHeight;

        l.tabPreX = l.tabOutX + tabW + 6.0f;
        l.tabPreY = l.rightY + 6.0f;
        l.tabPreW = tabW;
        l.tabPreH = layout.tabHeight;

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

        // Rotation Adjusters (Yaw -45/+45 etc.)
        l.rotBtnsY = l.dimBtnsY + 44.0f;
        l.rotBtnW = 44.0f;
        l.rotBtnH = 24.0f;

        l.deselX = l.rightX + 10.0f;
        l.deselY = l.rotBtnsY + 34.0f;
        l.deselW = 280.0f;
        l.deselH = 30.0f;

        l.delX = l.rightX + 10.0f;
        l.delY = l.deselY + 36.0f;
        l.delW = 280.0f;
        l.delH = 30.0f;

        return l;
    }

    // Vertical gradient helper (row strips) for the retro glossy-header look.
    static void fillGradient(float x, float y, float w, float h, const Vec3& top, const Vec3& bot) {
        int rows = std::max(1, (int)h);
        float rh = h / (float)rows;
        for (int i = 0; i < rows; ++i) {
            float t = rows > 1 ? (float)i / (float)(rows - 1) : 0.0f;
            Vec3 col(top.x + (bot.x - top.x) * t, top.y + (bot.y - top.y) * t, top.z + (bot.z - top.z) * t);
            Renderer::drawRect(x, y + i * rh, w, rh + 0.5f, col);
        }
    }

    void LabTheme::drawBevel(float x, float y, float w, float h, bool sunken) {
        const auto& c = s_themeColors;
        Vec3 light = sunken ? c.sunkenLight : c.bevelLight;
        Vec3 dark  = sunken ? c.sunkenDark  : c.bevelDark;

        Renderer::drawRect(x, y, w, 1.0f, light);
        Renderer::drawRect(x, y, 1.0f, h, light);
        Renderer::drawRect(x, y + h - 1.0f, w, 1.0f, dark);
        Renderer::drawRect(x + w - 1.0f, y, 1.0f, h, dark);
    }

    void LabTheme::drawPanel(float x, float y, float w, float h, const std::string& title) {
        const auto& c = s_themeColors;
        Renderer::drawRect(x, y, w, h, c.panelBg);
        drawBevel(x, y, w, h, false);

        if (!title.empty()) {
            fillGradient(x + 1.0f, y + 1.0f, w - 2.0f, 21.0f, c.panelHeaderBg, c.panelHeaderBgBottom);
            Renderer::drawRect(x + 1.0f, y + 22.0f, w - 2.0f, 1.0f, c.panelHeaderBorder);
            LabFont::drawText(x + 8.0f, y + 5.0f, title, 1.5f, c.textHeader, LabFontType::System);
        }
    }

    bool LabTheme::drawButton(float x, float y, float w, float h, const std::string& label,
                              bool active, bool highlighted,
                              float mouseX, float mouseY, bool lmbClicked,
                              bool isRed, bool isGreen) {
        const auto& c = s_themeColors;
        bool hovered = (mouseX >= x && mouseX <= x + w && mouseY >= y && mouseY <= y + h);
        bool clicked = hovered && lmbClicked;

        Vec3 bgCol = active ? c.buttonActiveBg :
                     isRed ? (hovered ? c.buttonRedHoverBg : c.buttonRedBg) :
                     isGreen ? (hovered ? c.buttonGreenHoverBg : c.buttonGreenBg) :
                     highlighted ? (hovered ? c.buttonHighlightHoverBg : c.buttonHighlightBg) :
                     hovered ? c.buttonHoverBg : c.buttonBg;

        // Glossy 2008 gradient: lighter top, darker bottom (inverted when pressed/active)
        Vec3 lighter(std::min(1.0f, bgCol.x * 1.18f), std::min(1.0f, bgCol.y * 1.18f), std::min(1.0f, bgCol.z * 1.18f));
        Vec3 darker(bgCol.x * 0.82f, bgCol.y * 0.82f, bgCol.z * 0.82f);
        if (active) fillGradient(x, y, w, h, darker, lighter);
        else        fillGradient(x, y, w, h, lighter, darker);
        drawBevel(x, y, w, h, active);
        if (hovered && !active) Renderer::drawRect(x, y + h - 1.0f, w, 1.0f, c.accent);
        // trimmed corners for a soft 2px radius
        Renderer::drawRect(x, y, 1.0f, 1.0f, c.panelBg);
        Renderer::drawRect(x + w - 1.0f, y, 1.0f, 1.0f, c.panelBg);
        Renderer::drawRect(x, y + h - 1.0f, 1.0f, 1.0f, c.panelBg);
        Renderer::drawRect(x + w - 1.0f, y + h - 1.0f, 1.0f, 1.0f, c.panelBg);

        Vec3 txtCol = active ? c.textActive :
                      hovered ? c.textHover :
                      isRed ? Vec3(1.0f, 0.85f, 0.85f) :
                      isGreen ? Vec3(0.90f, 1.0f, 0.90f) :
                      c.textRegular;

        float tw = LabFont::getTextWidth(label, 1.4f, LabFontType::System);
        float th = LabFont::getTextHeight(1.4f, LabFontType::System);
        float textX = (tw < w - 16.0f) ? (x + (w - tw) * 0.5f) : (x + 8.0f);
        float textY = y + std::max(2.0f, (h - th) * 0.5f);
        LabFont::drawText(textX, textY, label, 1.4f, txtCol, LabFontType::System);

        return clicked;
    }

    bool LabTheme::drawSlider(float x, float y, float w, float h, const std::string& label,
                              float& value, float minVal, float maxVal, const std::string& format,
                              float mouseX, float mouseY, bool lmbPressed,
                              bool lmbClicked, int* activeSliderId, int sliderId) {
        const auto& c = s_themeColors;
        float labelW = std::min(110.0f, w * 0.32f);
        float trackX = x + labelW;
        float trackW = w - labelW - 60.0f;
        float trackY = y + 2.0f;
        float trackH = std::max(16.0f, h - 4.0f);

        // Label
        float th = LabFont::getTextHeight(1.4f, LabFontType::System);
        LabFont::drawText(x, y + (h - th) * 0.5f, label, 1.4f, c.textRegular, LabFontType::System);

        // Hover & bounds
        bool inBounds = (mouseX >= trackX && mouseX <= trackX + trackW && mouseY >= y && mouseY <= y + h);
        bool isActive = (activeSliderId && *activeSliderId == sliderId);

        // Sunken Track Background
        Renderer::drawRect(trackX, trackY, trackW, trackH, c.sliderTrackBg);
        drawBevel(trackX, trackY, trackW, trackH, true);
        if (inBounds || isActive) {
            Renderer::drawRect(trackX, trackY, trackW, 1.0f, c.accent);
            Renderer::drawRect(trackX, trackY + trackH - 1.0f, trackW, 1.0f, c.accent);
        }

        // Filled bar portion (subtle gradient fill)
        float range = (maxVal > minVal) ? (maxVal - minVal) : 1.0f;
        float norm = std::clamp((value - minVal) / range, 0.0f, 1.0f);
        float fillW = (trackW - 2.0f) * norm;
        if (fillW > 0.5f) {
            Vec3 fillTop = c.sliderFillBg;
            Vec3 fillBot = Vec3(fillTop.x * 0.75f, fillTop.y * 0.75f, fillTop.z * 0.75f);
            fillGradient(trackX + 1.0f, trackY + 1.0f, fillW, trackH - 2.0f, fillTop, fillBot);
        }

        // Thumb indicator (crisp 3px bar with glow)
        float thumbX = trackX + 1.0f + (trackW - 4.0f) * norm;
        Renderer::drawRect(thumbX, trackY, 2.0f, trackH, (inBounds || isActive) ? Vec3(1.0f, 1.0f, 1.0f) : c.sliderThumbBg);

        // Value readout
        char buf[32];
        snprintf(buf, sizeof(buf), format.c_str(), value);
        LabFont::drawText(trackX + trackW + 8.0f, y + (h - th) * 0.5f, buf, 1.4f, (inBounds || isActive) ? c.accent : c.sliderText, LabFontType::System);

        // Mouse Drag interaction
        bool changed = false;
        if (activeSliderId) {
            if (lmbClicked && inBounds) {
                *activeSliderId = sliderId;
            }
            if (lmbPressed && *activeSliderId == sliderId) {
                float t = std::clamp((mouseX - trackX) / trackW, 0.0f, 1.0f);
                value = minVal + t * range;
                changed = true;
            }
            if (!lmbPressed && *activeSliderId == sliderId) {
                *activeSliderId = -1;
            }
        } else {
            if (lmbPressed && inBounds) {
                float t = std::clamp((mouseX - trackX) / trackW, 0.0f, 1.0f);
                value = minVal + t * range;
                changed = true;
            }
        }

        return changed;
    }

    bool LabTheme::drawInput(float x, float y, float w, float h, const std::string& text,
                             bool focused, float mouseX, float mouseY,
                             bool lmbClicked, float cursorBlink) {
        const auto& c = s_themeColors;
        bool hovered = (mouseX >= x && mouseX <= x + w && mouseY >= y && mouseY <= y + h);
        bool clicked = hovered && lmbClicked;

        Renderer::drawRect(x, y, w, h, c.inputBg);
        Renderer::drawRect(x, y, w, 1.0f, focused ? c.inputBorderFocused : c.inputBorder);
        Renderer::drawRect(x, y, 1.0f, h, focused ? c.inputBorderFocused : c.inputBorder);
        Renderer::drawRect(x, y + h - 1.0f, w, 1.0f, focused ? c.inputBorderFocused : c.inputBorder);
        Renderer::drawRect(x + w - 1.0f, y, 1.0f, h, focused ? c.inputBorderFocused : c.inputBorder);

        float textX = x + 6.0f;
        float textY = y + (h - 14.0f) * 0.5f;
        LabFont::drawText(textX, textY, text, 1.4f, c.textRegular, LabFontType::Monospace);

        if (focused && std::fmod(cursorBlink, 1.0f) < 0.5f) {
            float tw = LabFont::getTextWidth(text, 1.4f, LabFontType::Monospace);
            Renderer::drawRect(textX + tw + 1.0f, textY, 2.0f, 14.0f, c.inputCursor);
        }

        return clicked;
    }

    bool LabTheme::drawDropdownItem(float x, float y, float w, float h, const std::string& label,
                                    bool hovered, bool separator, bool disabled) {
        const auto& c = s_themeColors;
        if (separator) {
            Renderer::drawRect(x + 4.0f, y + h * 0.5f, w - 8.0f, 1.0f, c.bevelDark);
            return false;
        }

        Vec3 bgCol = hovered && !disabled ? c.buttonHoverBg : c.panelBg;
        Renderer::drawRect(x, y, w, h, bgCol);

        Vec3 txtCol = disabled ? c.textMuted : (hovered ? c.textHover : c.textRegular);
        LabFont::drawText(x + 10.0f, y + (h - 14.0f) * 0.5f, label, 1.4f, txtCol, LabFontType::System);

        return hovered && !disabled;
    }

} // namespace Lab
