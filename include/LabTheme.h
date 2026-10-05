#pragma once
#include <string>
#include <vector>
#include <algorithm>
#include <unordered_map>
#include "LabMath.h"
#include "LabRenderer.h"
#include "LabFont.h"

namespace Lab {

    enum class EngineLanguage {
        English = 0,
        Polish = 1
    };

    struct ThemeColors {
        // Window & Panels
        Vec3 windowBg{ 0.12f, 0.14f, 0.18f };
        Vec3 panelBg{ 0.18f, 0.19f, 0.22f };
        Vec3 panelHeaderBg{ 0.14f, 0.22f, 0.32f };
        Vec3 panelHeaderBorder{ 0.20f, 0.50f, 0.80f };
        Vec3 viewportBg{ 0.08f, 0.09f, 0.11f };

        // Accents & Highlights
        Vec3 accent{ 1.0f, 0.55f, 0.10f };       // Valve Hammer Orange
        Vec3 accentHover{ 1.0f, 0.68f, 0.20f };
        Vec3 accentGlow{ 0.20f, 0.85f, 1.0f };    // Cyber Cyan

        // Buttons
        Vec3 buttonBg{ 0.24f, 0.25f, 0.28f };
        Vec3 buttonHoverBg{ 0.32f, 0.35f, 0.40f };
        Vec3 buttonActiveBg{ 0.85f, 0.48f, 0.10f };
        Vec3 buttonHighlightBg{ 0.24f, 0.42f, 0.26f };
        Vec3 buttonHighlightHoverBg{ 0.32f, 0.55f, 0.35f };
        Vec3 buttonGreenBg{ 0.16f, 0.50f, 0.25f };
        Vec3 buttonGreenHoverBg{ 0.22f, 0.65f, 0.32f };
        Vec3 buttonRedBg{ 0.55f, 0.16f, 0.16f };
        Vec3 buttonRedHoverBg{ 0.75f, 0.22f, 0.22f };

        // Bevel borders
        Vec3 bevelLight{ 0.38f, 0.40f, 0.45f };
        Vec3 bevelDark{ 0.08f, 0.09f, 0.11f };
        Vec3 sunkenLight{ 0.08f, 0.09f, 0.11f };
        Vec3 sunkenDark{ 0.38f, 0.40f, 0.45f };

        // Text
        Vec3 textRegular{ 0.90f, 0.92f, 0.95f };
        Vec3 textMuted{ 0.60f, 0.62f, 0.66f };
        Vec3 textHover{ 1.0f, 0.95f, 0.70f };
        Vec3 textActive{ 1.0f, 1.0f, 1.0f };
        Vec3 textHeader{ 1.0f, 1.0f, 1.0f };
        Vec3 textAccent{ 0.98f, 0.78f, 0.08f };

        // Sliders & Progress
        Vec3 sliderTrackBg{ 0.14f, 0.15f, 0.17f };
        Vec3 sliderFillBg{ 0.22f, 0.52f, 0.72f };
        Vec3 sliderThumbBg{ 0.80f, 0.82f, 0.85f };
        Vec3 sliderText{ 0.95f, 0.80f, 0.25f };

        // Input fields & Tabs
        Vec3 inputBg{ 0.10f, 0.11f, 0.13f };
        Vec3 inputBorder{ 0.30f, 0.32f, 0.36f };
        Vec3 inputBorderFocused{ 0.20f, 0.70f, 1.0f };
        Vec3 inputCursor{ 0.20f, 0.85f, 1.0f };
        Vec3 tabActiveBg{ 0.28f, 0.30f, 0.35f };
        Vec3 tabInactiveBg{ 0.16f, 0.17f, 0.20f };
    };

    struct ThemeLayout {
        float toolbarHeight = 40.0f;
        float menuBarHeight = 32.0f;
        float statusBarHeight = 28.0f;
        float rightSidebarWidth = 320.0f;
        float rightSidebarY = 58.0f;
        float leftPaletteWidth = 48.0f;
        float buttonHeight = 24.0f;
        float sliderHeight = 18.0f;
        float tabWidth = 96.0f;
        float tabHeight = 26.0f;
        float bevelWidth = 1.0f;
        float iconSize = 24.0f;
    };

    // Sidebar Layout parameters moved from LabEditor.h to Theme constants
    struct SidebarLayout {
        float rightX = 0.0f;
        float rightY = 0.0f;
        float rightW = 0.0f;
        float rightH = 0.0f;

        // Tabs
        float tabPropX = 0.0f, tabPropY = 0.0f, tabPropW = 0.0f, tabPropH = 0.0f;
        float tabOutX = 0.0f,  tabOutY = 0.0f,  tabOutW = 0.0f,  tabOutH = 0.0f;
        float tabPreX = 0.0f,  tabPreY = 0.0f,  tabPreW = 0.0f,  tabPreH = 0.0f;

        // Outliner
        float outListX = 0.0f, outListY = 0.0f, outListW = 0.0f, outListH = 0.0f;
        float outItemH = 0.0f, outItemSpacing = 0.0f;
        float outFocusX = 0.0f, outFocusY = 0.0f, outFocusW = 0.0f, outFocusH = 0.0f;
        float outDupX = 0.0f,   outDupY = 0.0f,   outDupW = 0.0f,   outDupH = 0.0f;
        float outDelX = 0.0f,   outDelY = 0.0f,   outDelW = 0.0f,   outDelH = 0.0f;
        float outPrevX = 0.0f,  outPrevY = 0.0f,  outPrevW = 0.0f,  outPrevH = 0.0f;
        float outNextX = 0.0f,  outNextY = 0.0f,  outNextW = 0.0f,  outNextH = 0.0f;

        // Properties
        float uvBtnY = 0.0f, uvBtnW = 0.0f, uvBtnH = 0.0f;
        float texBoxX = 0.0f, texBoxY = 0.0f, texBoxW = 0.0f, texBoxH = 0.0f;
        float thumbX = 0.0f, thumbY = 0.0f, thumbS = 0.0f;
        float texBrowseX = 0.0f, texBrowseY = 0.0f, texBrowseW = 0.0f, texBrowseH = 0.0f;
        float texApplyX = 0.0f,  texApplyY = 0.0f,  texApplyW = 0.0f,  texApplyH = 0.0f;
        float modelBoxX = 0.0f, modelBoxY = 0.0f, modelBoxW = 0.0f, modelBoxH = 0.0f;
        float modelBrowseX = 0.0f, modelBrowseY = 0.0f, modelBrowseW = 0.0f, modelBrowseH = 0.0f;

        // Position translation
        float posBtnsY = 0.0f, posBtnW = 0.0f, posBtnH = 0.0f;
        float elevUpX = 0.0f, elevUpY = 0.0f, elevUpW = 0.0f, elevUpH = 0.0f;
        float elevDwnX = 0.0f, elevDwnY = 0.0f, elevDwnW = 0.0f, elevDwnH = 0.0f;
        float elevGndX = 0.0f, elevGndY = 0.0f, elevGndW = 0.0f, elevGndH = 0.0f;

        // Size scaling
        float dimBtnsY = 0.0f, dimBtnW = 0.0f, dimBtnH = 0.0f;

        // Rotation adjustment
        float rotBtnsY = 0.0f, rotBtnW = 0.0f, rotBtnH = 0.0f;
        float deselX = 0.0f, deselY = 0.0f, deselW = 0.0f, deselH = 0.0f;
        float delX = 0.0f,   delY = 0.0f,   delW = 0.0f,   delH = 0.0f;
    };

    SidebarLayout getSidebarLayout(float w, float h);

    class LabTheme {
    public:
        static ThemeColors& getColors();
        static const ThemeColors& colors();
        static void setColors(const ThemeColors& c);
        static void resetDefaultColors();

        static ThemeLayout& getLayout();
        static const ThemeLayout& layout();

        static void setLanguage(EngineLanguage lang);
        static EngineLanguage getLanguage();
        static std::string tr(const std::string& key);

        // Standard UI Widgets
        static void drawBevel(float x, float y, float w, float h, bool sunken = false);
        static void drawPanel(float x, float y, float w, float h, const std::string& title = "");
        static bool drawButton(float x, float y, float w, float h, const std::string& label,
                               bool active = false, bool highlighted = false,
                               float mouseX = 0.0f, float mouseY = 0.0f, bool lmbClicked = false,
                               bool isRed = false, bool isGreen = false);
        static bool drawSlider(float x, float y, float w, float h, const std::string& label,
                               float& value, float minVal, float maxVal, const std::string& format = "%.2f",
                               float mouseX = 0.0f, float mouseY = 0.0f, bool lmbPressed = false,
                               bool lmbClicked = false, int* activeSliderId = nullptr, int sliderId = 0);
        static bool drawInput(float x, float y, float w, float h, const std::string& text,
                              bool focused, float mouseX = 0.0f, float mouseY = 0.0f,
                              bool lmbClicked = false, float cursorBlink = 0.0f);
        static bool drawDropdownItem(float x, float y, float w, float h, const std::string& label,
                                     bool hovered, bool separator = false, bool disabled = false);
    };

} // namespace Lab
