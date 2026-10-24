#include "LFM/LFMModals.h"
#include "LabTheme.h"
#include "LabFont.h"
#include "LabRenderer.h"
#include <filesystem>
#include <algorithm>
#include <sstream>
#include <iomanip>

namespace Lab::LFM {

    // =========================================================================
    // MAP BROWSER MODAL
    // =========================================================================

    MapBrowserModal::MapBrowserModal() {
        refreshMaps();
    }

    void MapBrowserModal::open() {
        refreshMaps();
        _open = true;
        _confirmed = false;
    }

    void MapBrowserModal::close() {
        _open = false;
    }

    std::string MapBrowserModal::getSelectedMapPath() const {
        if (_selectedIndex >= 0 && _selectedIndex < static_cast<int>(_maps.size())) {
            return _maps[_selectedIndex].filePath;
        }
        return "";
    }

    void MapBrowserModal::refreshMaps() {
        _maps.clear();
        std::string mapsDir = "assets/maps";
        if (!std::filesystem::exists(mapsDir)) return;

        for (const auto& entry : std::filesystem::directory_iterator(mapsDir)) {
            if (!entry.is_regular_file()) continue;
            std::string ext = entry.path().extension().string();
            // Skip backups
            if (entry.path().string().find(".backup") != std::string::npos) continue;

            if (ext == ".labmap" || ext == ".labscene" || ext == ".lmap") {
                MapFileInfo info;
                info.fileName = entry.path().filename().string();
                info.filePath = entry.path().string();
                // Replace backslashes with forward slashes for clean consistency
                std::replace(info.filePath.begin(), info.filePath.end(), '\\', '/');
                info.isScene = (ext == ".labscene");
                info.sizeBytes = entry.file_size();
                _maps.push_back(std::move(info));
            }
        }

        std::sort(_maps.begin(), _maps.end(), [](const MapFileInfo& a, const MapFileInfo& b) {
            return a.fileName < b.fileName;
        });

        if (_selectedIndex >= static_cast<int>(_maps.size())) {
            _selectedIndex = _maps.empty() ? 0 : static_cast<int>(_maps.size()) - 1;
        }
    }

    void MapBrowserModal::render(float screenW, float screenH, double mouseX, double mouseY, bool lmb, bool& lmbHandled) {
        if (!_open) return;

        // 1. Dim background overlay
        Renderer::drawRect(0.0f, 0.0f, screenW, screenH, Vec3(0.05f, 0.06f, 0.08f));

        // 2. Centered Modal Box
        float modalW = 660.0f;
        float modalH = 460.0f;
        float modalX = (screenW - modalW) * 0.5f;
        float modalY = (screenH - modalH) * 0.5f;

        LabTheme::drawPanel(modalX, modalY, modalW, modalH, "LAB DIRECTOR STUDIO - WYBOR MAPY / SCENY");

        // Subtitle / instructions
        LabFont::drawText(modalX + 16.0f, modalY + 32.0f, "Wybierz plik poziomu (.labmap) lub sceny (.labscene) z assets/maps:", 1.35f, Vec3(0.85f, 0.88f, 0.92f));

        // List frame
        float listX = modalX + 16.0f;
        float listY = modalY + 56.0f;
        float listW = modalW - 32.0f;
        float listH = modalH - 146.0f;

        LabTheme::drawBevel(listX, listY, listW, listH, true);

        float itemH = 28.0f;
        float curY = listY + 4.0f;

        for (size_t i = 0; i < _maps.size(); ++i) {
            const auto& map = _maps[i];
            bool isSelected = (_selectedIndex == static_cast<int>(i));

            if (curY + itemH <= listY + listH) {
                // Item background
                if (isSelected) {
                    Renderer::drawRect(listX + 2.0f, curY, listW - 4.0f, itemH, Vec3(0.20f, 0.38f, 0.58f));
                    Renderer::drawRect(listX + 2.0f, curY, listW - 4.0f, 1.0f, Vec3(0.35f, 0.80f, 1.0f));
                    Renderer::drawRect(listX + 2.0f, curY + itemH - 1.0f, listW - 4.0f, 1.0f, Vec3(0.35f, 0.80f, 1.0f));
                } else {
                    bool hovered = (mouseX >= listX + 2.0f && mouseX <= (listX + listW - 2.0f) &&
                                    mouseY >= curY && mouseY <= (curY + itemH));
                    if (hovered) {
                        Renderer::drawRect(listX + 2.0f, curY, listW - 4.0f, itemH, Vec3(0.28f, 0.30f, 0.34f));
                    }
                }

                // Click selection
                if (lmb && !lmbHandled &&
                    mouseX >= listX + 2.0f && mouseX <= (listX + listW - 2.0f) &&
                    mouseY >= curY && mouseY <= (curY + itemH)) {
                    _selectedIndex = static_cast<int>(i);
                    lmbHandled = true;
                }

                // Type badge & file name
                Vec3 tagCol = map.isScene ? Vec3(0.95f, 0.75f, 0.20f) : Vec3(0.30f, 0.85f, 0.45f);
                std::string tag = map.isScene ? "[SCENE]" : "[MAP]  ";
                LabFont::drawText(listX + 12.0f, curY + 5.0f, tag, 1.35f, tagCol);
                LabFont::drawText(listX + 85.0f, curY + 5.0f, map.fileName, 1.40f, isSelected ? Vec3(1.0f, 1.0f, 1.0f) : Vec3(0.90f, 0.92f, 0.95f));

                // File size
                std::ostringstream ss;
                ss << (map.sizeBytes / 1024) << " KB";
                LabFont::drawText(listX + listW - 80.0f, curY + 5.0f, ss.str(), 1.30f, Vec3(0.70f, 0.72f, 0.75f));
            }

            curY += itemH + 2.0f;
        }

        // Details bar
        float detY = modalY + modalH - 78.0f;
        std::string selPath = getSelectedMapPath();
        LabFont::drawText(modalX + 18.0f, detY, "Wybrany zasob: " + (selPath.empty() ? std::string("(brak)") : selPath), 1.35f, Vec3(1.0f, 0.85f, 0.25f));

        // Action Buttons: [ Load ] & [ Cancel ]
        float btnW = 120.0f;
        float btnH = 28.0f;
        float btnY = modalY + modalH - 42.0f;

        if (LabTheme::drawButton(modalX + modalW - btnW * 2.0f - 24.0f, btnY, btnW, btnH, "Load", false, true,
                                static_cast<float>(mouseX), static_cast<float>(mouseY), lmb, false, true)) {
            if (!lmbHandled && !selPath.empty()) {
                _confirmed = true;
                _open = false;
                lmbHandled = true;
            }
        }

        if (LabTheme::drawButton(modalX + modalW - btnW - 14.0f, btnY, btnW, btnH, "Cancel", false, false,
                                static_cast<float>(mouseX), static_cast<float>(mouseY), lmb)) {
            if (!lmbHandled) {
                _open = false;
                lmbHandled = true;
            }
        }
    }

    // =========================================================================
    // EXPORT SETTINGS MODAL
    // =========================================================================

    ExportSettingsModal::ExportSettingsModal() = default;

    void ExportSettingsModal::open() {
        _open = true;
        _state = ExportState::Settings;
        _startRequested = false;
        _cancelRequested = false;
    }

    void ExportSettingsModal::close() {
        _open = false;
        _state = ExportState::Settings;
    }

    int ExportSettingsModal::getSelectedWidth() const {
        if (_resolutionPreset == 0) return 1280;
        if (_resolutionPreset == 2) return 3840;
        return 1920; // 1080p
    }

    int ExportSettingsModal::getSelectedHeight() const {
        if (_resolutionPreset == 0) return 720;
        if (_resolutionPreset == 2) return 2160;
        return 1080; // 1080p
    }

    int ExportSettingsModal::getSelectedFps() const {
        if (_fpsPreset == 0) return 24;
        if (_fpsPreset == 1) return 30;
        return 60;
    }

    float ExportSettingsModal::getSelectedDuration(float totalDuration, float activeShotDuration) const {
        if (_rangePreset == 1) return std::max(1.0f, activeShotDuration);
        if (_rangePreset == 2) return 5.0f; // 5s preview
        return totalDuration;
    }

    void ExportSettingsModal::setRenderingProgress(int curFrame, int totalFrames, float curTime, float totalTime) {
        _currentFrame = curFrame;
        _totalFrames = totalFrames;
        _currentTime = curTime;
        _totalTime = totalTime;
        _state = ExportState::Rendering;
    }

    void ExportSettingsModal::setFinished(const std::string& path) {
        _outputPath = path;
        _state = ExportState::Finished;
    }

    void ExportSettingsModal::render(float screenW, float screenH, double mouseX, double mouseY, bool lmb, bool& lmbHandled) {
        if (!_open) return;

        // Dim background overlay
        Renderer::drawRect(0.0f, 0.0f, screenW, screenH, Vec3(0.05f, 0.06f, 0.08f));

        float modalW = 640.0f;
        float modalH = 480.0f;
        float modalX = (screenW - modalW) * 0.5f;
        float modalY = (screenH - modalH) * 0.5f;

        if (_state == ExportState::Settings) {
            LabTheme::drawPanel(modalX, modalY, modalW, modalH, "EKSPORT WIDEO I SEKWENCJI [LFM RENDER]");

            float curY = modalY + 36.0f;
            float leftX = modalX + 22.0f;

            // 1. Resolution Presets
            LabFont::drawText(leftX, curY, "1. Rozdzielczosc wideo:", 1.40f, Vec3(1.0f, 0.88f, 0.30f));
            curY += 24.0f;
            const char* resLabels[] = { "720p HD (1280x720)", "1080p FHD (1920x1080)", "4K UHD (3840x2160)" };
            float optW = 188.0f;
            for (int r = 0; r < 3; ++r) {
                bool isSel = (_resolutionPreset == r);
                if (LabTheme::drawButton(leftX + static_cast<float>(r) * (optW + 8.0f), curY, optW, 26.0f, resLabels[r], isSel, isSel,
                                        static_cast<float>(mouseX), static_cast<float>(mouseY), lmb)) {
                    if (!lmbHandled) { _resolutionPreset = r; lmbHandled = true; }
                }
            }

            // 2. Framerate Presets
            curY += 38.0f;
            LabFont::drawText(leftX, curY, "2. Liczba klatek na sekunde (FPS):", 1.40f, Vec3(1.0f, 0.88f, 0.30f));
            curY += 24.0f;
            const char* fpsLabels[] = { "24 FPS (Kinowy)", "30 FPS (Standard)", "60 FPS (Plynny)" };
            for (int f = 0; f < 3; ++f) {
                bool isSel = (_fpsPreset == f);
                if (LabTheme::drawButton(leftX + static_cast<float>(f) * (optW + 8.0f), curY, optW, 26.0f, fpsLabels[f], isSel, isSel,
                                        static_cast<float>(mouseX), static_cast<float>(mouseY), lmb)) {
                    if (!lmbHandled) { _fpsPreset = f; lmbHandled = true; }
                }
            }

            // 3. Range Presets
            curY += 38.0f;
            LabFont::drawText(leftX, curY, "3. Zakres osi czasu do wyrenderowania:", 1.40f, Vec3(1.0f, 0.88f, 0.30f));
            curY += 24.0f;
            const char* rangeLabels[] = { "Calosc (0s - 60s)", "Aktywne ujecie", "Szybki test (5s)" };
            for (int rg = 0; rg < 3; ++rg) {
                bool isSel = (_rangePreset == rg);
                if (LabTheme::drawButton(leftX + static_cast<float>(rg) * (optW + 8.0f), curY, optW, 26.0f, rangeLabels[rg], isSel, isSel,
                                        static_cast<float>(mouseX), static_cast<float>(mouseY), lmb)) {
                    if (!lmbHandled) { _rangePreset = rg; lmbHandled = true; }
                }
            }

            // 4. Format Presets
            curY += 38.0f;
            LabFont::drawText(leftX, curY, "4. Format wyjsciowy:", 1.40f, Vec3(1.0f, 0.88f, 0.30f));
            curY += 24.0f;
            const char* fmtLabels[] = { "Wideo MP4 (H.264 FFmpeg)", "Sekwencja PNG + WAV" };
            float fmtW = 286.0f;
            for (int fm = 0; fm < 2; ++fm) {
                bool isSel = (_formatPreset == fm);
                if (LabTheme::drawButton(leftX + static_cast<float>(fm) * (fmtW + 12.0f), curY, fmtW, 26.0f, fmtLabels[fm], isSel, isSel,
                                        static_cast<float>(mouseX), static_cast<float>(mouseY), lmb)) {
                    if (!lmbHandled) { _formatPreset = fm; lmbHandled = true; }
                }
            }

            // 5. Output Target Info Box
            curY += 40.0f;
            LabTheme::drawBevel(leftX, curY, modalW - 44.0f, 38.0f, true);
            LabFont::drawText(leftX + 10.0f, curY + 4.0f, "Sciezka docelowa:", 1.30f, Vec3(0.70f, 0.72f, 0.78f));
            LabFont::drawText(leftX + 10.0f, curY + 18.0f, _outputPath, 1.40f, Vec3(0.35f, 0.90f, 1.0f));

            // Bottom Actions: [ Render ] & [ Cancel ]
            float btnW = 120.0f;
            float btnH = 30.0f;
            float btnY = modalY + modalH - 46.0f;

            if (LabTheme::drawButton(modalX + modalW - btnW * 2.0f - 24.0f, btnY, btnW, btnH, "Render", false, true,
                                    static_cast<float>(mouseX), static_cast<float>(mouseY), lmb, false, true)) {
                if (!lmbHandled) {
                    _startRequested = true;
                    _state = ExportState::Rendering;
                    lmbHandled = true;
                }
            }

            if (LabTheme::drawButton(modalX + modalW - btnW - 14.0f, btnY, btnW, btnH, "Cancel", false, false,
                                    static_cast<float>(mouseX), static_cast<float>(mouseY), lmb)) {
                if (!lmbHandled) {
                    _open = false;
                    lmbHandled = true;
                }
            }
        }
        else if (_state == ExportState::Rendering) {
            LabTheme::drawPanel(modalX, modalY, modalW, modalH, "RENDEROWANIE WIDEO W TOKU...");

            float midX = modalX + modalW * 0.5f;
            float curY = modalY + 60.0f;

            LabFont::drawText(midX - 170.0f, curY, "Trwa generowanie klatek i kodowanie wideo...", 1.45f, Vec3(1.0f, 0.88f, 0.30f));

            // Configuration Info
            curY += 34.0f;
            std::ostringstream cfgSs;
            cfgSs << getSelectedWidth() << "x" << getSelectedHeight() << " @ " << getSelectedFps() << " FPS";
            LabFont::drawText(midX - 85.0f, curY, cfgSs.str(), 1.40f, Vec3(0.85f, 0.88f, 0.92f));

            // Progress Bar
            curY += 46.0f;
            float barW = modalW - 64.0f;
            float barH = 36.0f;
            float barX = modalX + 32.0f;
            LabTheme::drawBevel(barX, curY, barW, barH, true);

            float pct = (_totalFrames > 0) ? std::clamp(static_cast<float>(_currentFrame) / static_cast<float>(_totalFrames), 0.0f, 1.0f) : 0.0f;
            Renderer::drawRect(barX + 2.0f, curY + 2.0f, (barW - 4.0f) * pct, barH - 4.0f, Vec3(0.96f, 0.51f, 0.12f));

            // Centered Percentage Text
            std::ostringstream pctSs;
            pctSs << std::fixed << std::setprecision(1) << (pct * 100.0f) << "%";
            LabFont::drawText(barX + barW * 0.5f - 24.0f, curY + 8.0f, pctSs.str(), 1.55f, Vec3(1.0f, 1.0f, 1.0f));

            // Frame counter details
            curY += 56.0f;
            std::ostringstream frameSs;
            frameSs << "Klatka: " << _currentFrame << " / " << _totalFrames
                    << "   |   Czas: " << std::fixed << std::setprecision(2) << _currentTime << "s / " << _totalTime << "s";
            LabFont::drawText(midX - 150.0f, curY, frameSs.str(), 1.40f, Vec3(0.90f, 0.92f, 0.95f));

            // Cancel Button
            float btnW = 120.0f;
            float btnH = 30.0f;
            float btnY = modalY + modalH - 52.0f;
            if (LabTheme::drawButton(midX - btnW * 0.5f, btnY, btnW, btnH, "Cancel", false, false,
                                    static_cast<float>(mouseX), static_cast<float>(mouseY), lmb, true, false)) {
                if (!lmbHandled) {
                    _cancelRequested = true;
                    _open = false;
                    _state = ExportState::Settings;
                    lmbHandled = true;
                }
            }
        }
        else if (_state == ExportState::Finished) {
            LabTheme::drawPanel(modalX, modalY, modalW, modalH, "EKSPORT ZAKONCZONY SUKCESEM!");

            float midX = modalX + modalW * 0.5f;
            float curY = modalY + 70.0f;

            LabFont::drawText(midX - 160.0f, curY, "Plik wideo zostal pomyslnie wyrenderowany!", 1.50f, Vec3(0.35f, 0.95f, 0.45f));

            curY += 40.0f;
            LabTheme::drawBevel(modalX + 32.0f, curY, modalW - 64.0f, 44.0f, true);
            LabFont::drawText(modalX + 44.0f, curY + 12.0f, "Zapisano do: " + _outputPath, 1.40f, Vec3(1.0f, 0.88f, 0.30f));

            curY += 60.0f;
            std::ostringstream finSs;
            finSs << "Laczna liczba wyrenderowanych klatek: " << _totalFrames;
            LabFont::drawText(midX - 130.0f, curY, finSs.str(), 1.35f, Vec3(0.85f, 0.88f, 0.92f));

            // Close button
            float btnW = 120.0f;
            float btnH = 30.0f;
            float btnY = modalY + modalH - 52.0f;
            if (LabTheme::drawButton(midX - btnW * 0.5f, btnY, btnW, btnH, "Close", false, true,
                                    static_cast<float>(mouseX), static_cast<float>(mouseY), lmb)) {
                if (!lmbHandled) {
                    _open = false;
                    _state = ExportState::Settings;
                    lmbHandled = true;
                }
            }
        }
    }

} // namespace Lab::LFM
