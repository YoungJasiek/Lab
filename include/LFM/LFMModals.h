#pragma once

#include "LabMath.h"
#include <string>
#include <vector>
#include <cstdint>

namespace Lab::LFM {

    struct MapFileInfo {
        std::string fileName;
        std::string filePath;
        bool isScene = false;
        uint64_t sizeBytes = 0;
    };

    class MapBrowserModal {
    public:
        MapBrowserModal();

        void open();
        void close();
        bool isOpen() const { return _open; }

        bool hasConfirmed() const { return _confirmed; }
        void clearConfirmed() { _confirmed = false; }
        std::string getSelectedMapPath() const;

        void render(float screenW, float screenH, double mouseX, double mouseY, bool lmb, bool& lmbHandled);

    private:
        void refreshMaps();

        bool _open = false;
        bool _confirmed = false;
        std::vector<MapFileInfo> _maps;
        int _selectedIndex = 0;
        float _scrollY = 0.0f;
    };

    enum class ExportState {
        Settings,
        Rendering,
        Finished
    };

    class ExportSettingsModal {
    public:
        ExportSettingsModal();

        void open();
        void close();
        bool isOpen() const { return _open; }

        ExportState getState() const { return _state; }
        void setState(ExportState state) { _state = state; }

        bool isStartRequested() const { return _startRequested; }
        bool isCancelRequested() const { return _cancelRequested; }
        void clearRequests() { _startRequested = false; _cancelRequested = false; }

        int getSelectedWidth() const;
        int getSelectedHeight() const;
        int getSelectedFps() const;
        float getSelectedDuration(float totalDuration, float activeShotDuration) const;
        std::string getOutputPath() const { return _outputPath; }

        void setRenderingProgress(int curFrame, int totalFrames, float curTime, float totalTime);
        void setFinished(const std::string& path);

        void render(float screenW, float screenH, double mouseX, double mouseY, bool lmb, bool& lmbHandled);

    private:
        bool _open = false;
        ExportState _state = ExportState::Settings;
        bool _startRequested = false;
        bool _cancelRequested = false;

        // Settings choices
        int _resolutionPreset = 1; // 0=720p, 1=1080p, 2=4K
        int _fpsPreset = 2;        // 0=24fps, 1=30fps, 2=60fps
        int _rangePreset = 0;      // 0=Full (60s), 1=Active Shot, 2=Quick Test (5s)
        int _formatPreset = 0;     // 0=MP4 (H.264), 1=PNG Frame Sequence
        std::string _outputPath = "output/lfm_cinematic.mp4";

        // Live progress state
        int _currentFrame = 0;
        int _totalFrames = 0;
        float _currentTime = 0.0f;
        float _totalTime = 0.0f;
        std::string _statusMessage = "Ready";
    };

} // namespace Lab::LFM
