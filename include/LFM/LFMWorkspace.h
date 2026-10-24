#pragma once

#include "LFM/LFMAnimationSet.h"
#include "LFM/LFMDualViewport.h"
#include "LFM/LFMSequencer.h"
#include "LFM/LFMModals.h"
#include <string>
#include <vector>
#include <functional>

namespace Lab::LFM {

    enum class LeftPanelTab {
        SceneHierarchy = 0,
        AnimationLibrary = 1,
        AudioLibrary = 2
    };

    struct AudioAssetInfo {
        std::string name;
        std::string path;
        std::string channel;
        float duration = 3.0f;
    };

    class Workspace {
    public:
        Workspace();

        void update(float dt, double mouseX, double mouseY, bool lmb, bool rmb);
        void render(int windowWidth, int windowHeight);

        AnimationSet& getAnimationSet() { return _animSet; }
        const AnimationSet& getAnimationSet() const { return _animSet; }

        DualViewport& getDualViewport() { return _dualViewport; }
        const DualViewport& getDualViewport() const { return _dualViewport; }

        Sequencer& getSequencer() { return _sequencer; }
        const Sequencer& getSequencer() const { return _sequencer; }

        MapBrowserModal& getMapModal() { return _mapModal; }
        const MapBrowserModal& getMapModal() const { return _mapModal; }

        ExportSettingsModal& getExportModal() { return _exportModal; }
        const ExportSettingsModal& getExportModal() const { return _exportModal; }

        void setAvailableAnimations(const std::vector<std::string>& anims) { _availableAnimations = anims; }
        void setPreviewAnimationCallback(std::function<void(const std::string&)> cb) { _onPreviewAnimation = std::move(cb); }
        void setLoadMapCallback(std::function<void(const std::string&)> cb) { _onLoadMap = std::move(cb); }

        void scrollTree(float delta) { _treeScrollY = std::max(0.0f, _treeScrollY + delta); }

    private:
        void refreshAudioLibrary();
        void renderTopMenuBar(float w);
        void renderLeftPanel(float x, float y, float w, float h);
        void renderSceneTab(float x, float y, float w, float h);
        void renderAnimationsTab(float x, float y, float w, float h);
        void renderAudioTab(float x, float y, float w, float h);
        void renderViewportToolbar(const ViewportRect& r, const std::string& camName);
        void renderSequencer(float x, float y, float w, float h);

        AnimationSet _animSet;
        DualViewport _dualViewport;
        Sequencer _sequencer;

        MapBrowserModal _mapModal;
        ExportSettingsModal _exportModal;

        LeftPanelTab _activeLeftTab = LeftPanelTab::SceneHierarchy;
        std::vector<std::string> _availableAnimations;
        std::vector<AudioAssetInfo> _availableAudio;

        std::function<void(const std::string&)> _onPreviewAnimation;
        std::function<void(const std::string&)> _onLoadMap;

        double _mouseX = 0.0;
        double _mouseY = 0.0;
        bool _lmb = false;
        bool _rmb = false;
        bool _lmbHandled = false;
        bool _isScrubbingTimeline = false;

        float _animSetWidth = 380.0f;
        float _sequencerHeight = 300.0f;
        float _treeScrollY = 0.0f;
        float _animScrollY = 0.0f;
        float _audioScrollY = 0.0f;
    };

} // namespace Lab::LFM
