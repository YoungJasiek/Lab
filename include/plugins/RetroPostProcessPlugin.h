#pragma once

#include "LabPlugin.h"

namespace Lab {

    /// @brief 1 kod 1 plugin: Standalone Retro CRT & Post-Process Shaders FX Plugin
    class LAB_CORE_API RetroPostProcessPlugin : public ILabPlugin {
    public:
        RetroPostProcessPlugin();
        virtual ~RetroPostProcessPlugin() override = default;

        virtual const char* getName() const override { return "Retro CRT & Post-Process FX"; }
        virtual const char* getVersion() const override { return "2.0.1"; }
        virtual bool onLoad() override;
        virtual void onUpdate(float dt) override;
        virtual void onUnload() override;

        void toggleScanlines() { m_scanlines = !m_scanlines; }
        bool areScanlinesEnabled() const { return m_scanlines; }
        void setScanlines(bool enabled) { m_scanlines = enabled; }

        float getChromaticAberration() const { return m_chromaticAberration; }
        void setChromaticAberration(float val) { m_chromaticAberration = val; }

        int getPixelScale() const { return m_pixelScale; }
        void setPixelScale(int scale) { m_pixelScale = scale; }

        float getScanlinePhase() const { return m_phase; }

    private:
        bool m_scanlines{true};
        float m_chromaticAberration{0.0035f};
        int m_pixelScale{2};
        float m_phase{0.0f};
    };

} // namespace Lab
