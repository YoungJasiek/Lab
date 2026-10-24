#include "LFM/LFMWorkspace.h"
#include "LabTheme.h"
#include "LabFont.h"
#include "LabRenderer.h"
#include "LabAudio.h"
#include <glad/gl.h>
#include <algorithm>
#include <cmath>
#include <sstream>
#include <iomanip>
#include <filesystem>

namespace Lab::LFM {

    Workspace::Workspace() {
        refreshAudioLibrary();
    }

    void Workspace::refreshAudioLibrary() {
        _availableAudio.clear();
        std::string audioDir = "assets/audio";
        if (!std::filesystem::exists(audioDir)) return;

        for (const auto& entry : std::filesystem::directory_iterator(audioDir)) {
            if (!entry.is_regular_file()) continue;
            if (entry.path().extension() == ".wav") {
                AudioAssetInfo info;
                info.name = entry.path().filename().string();
                info.path = entry.path().string();
                std::replace(info.path.begin(), info.path.end(), '\\', '/');
                if (info.name.find("dialog") != std::string::npos) {
                    info.channel = "dialogue";
                    info.duration = 6.0f;
                } else if (info.name.find("footstep") != std::string::npos) {
                    info.channel = "sfx";
                    info.duration = 1.5f;
                } else if (info.name.find("weapon") != std::string::npos || info.name.find("fire") != std::string::npos) {
                    info.channel = "sfx";
                    info.duration = 2.0f;
                } else {
                    info.channel = "sfx";
                    info.duration = 3.0f;
                }
                _availableAudio.push_back(std::move(info));
            }
        }

        std::sort(_availableAudio.begin(), _availableAudio.end(), [](const AudioAssetInfo& a, const AudioAssetInfo& b) {
            return a.name < b.name;
        });
    }

    void Workspace::update(float dt, double mouseX, double mouseY, bool lmb, bool rmb) {
        _mouseX = mouseX;
        _mouseY = mouseY;
        _lmb = lmb;
        _rmb = rmb;

        // Check if map selection was confirmed in MapBrowserModal
        if (_mapModal.hasConfirmed()) {
            std::string selectedPath = _mapModal.getSelectedMapPath();
            _mapModal.clearConfirmed();
            if (_onLoadMap && !selectedPath.empty()) {
                _onLoadMap(selectedPath);
            }
        }

        // Playback time update
        if (_sequencer.isPlaying() && !_isScrubbingTimeline) {
            float t = _sequencer.getCurrentTime() + dt;
            if (t >= _sequencer.getTotalDuration()) {
                t = 0.0f; // loop
            }
            _sequencer.setCurrentTime(t);
        }

        // Work camera navigation when RMB is inside secondary viewport
        const auto& secRect = _dualViewport.getSecondaryRect();
        bool inSec = (mouseX >= secRect.x && mouseX <= (secRect.x + secRect.w) &&
                      mouseY >= secRect.y && mouseY <= (secRect.y + secRect.h));
        _dualViewport.updateWorkCameraFlight(dt, mouseX, mouseY, rmb && inSec);

        if (!lmb) {
            _lmbHandled = false;
            _isScrubbingTimeline = false;
        }
    }

    void Workspace::render(int windowWidth, int windowHeight) {
        float winW = static_cast<float>(windowWidth);
        float winH = static_cast<float>(windowHeight);

        // Layout dimensions
        float menuH = 30.0f;
        float mainAreaY = menuH;
        float mainAreaH = winH - menuH - _sequencerHeight;

        float viewportX = _animSetWidth;
        float viewportW = winW - _animSetWidth;
        float viewportH = mainAreaH - 34.0f; // 34px for toolbar under viewports

        _dualViewport.updateLayout(viewportX, mainAreaY, viewportW, viewportH);

        // 1. Render Top Menu Bar
        renderTopMenuBar(winW);

        // 2. Render Left Panel (Scene Hierarchy, Animation Library, Audio Library)
        renderLeftPanel(0.0f, mainAreaY, _animSetWidth, mainAreaH + _sequencerHeight);

        // 3. Render Viewport Toolbars
        renderViewportToolbar(_dualViewport.getPrimaryRect(), "camera1");
        renderViewportToolbar(_dualViewport.getSecondaryRect(), "Work Camera");

        // 4. Render Interactive Sequencer (Bottom Multi-mode Timeline)
        renderSequencer(viewportX, winH - _sequencerHeight, viewportW, _sequencerHeight);

        // 5. Render Modal Overlays (if active)
        if (_mapModal.isOpen()) {
            _mapModal.render(winW, winH, _mouseX, _mouseY, _lmb, _lmbHandled);
        }
        if (_exportModal.isOpen()) {
            _exportModal.render(winW, winH, _mouseX, _mouseY, _lmb, _lmbHandled);
        }
    }

    void Workspace::renderTopMenuBar(float w) {
        LabTheme::drawPanel(0.0f, 0.0f, w, 30.0f, "");

        float curX = 14.0f;
        const char* menus[] = { "Plik", "Edycja", "Widok", "Pomoc" };
        for (const char* m : menus) {
            LabFont::drawText(curX, 6.0f, m, 1.45f, Vec3(0.92f, 0.93f, 0.96f));
            curX += 62.0f;
        }

        // Quick Map Open Action Button on Menu Bar
        float openBtnW = 75.0f;
        if (LabTheme::drawButton(curX + 8.0f, 3.0f, openBtnW, 24.0f, "Open", false, false,
                                static_cast<float>(_mouseX), static_cast<float>(_mouseY), _lmb)) {
            if (!_lmbHandled && !_exportModal.isOpen()) {
                _mapModal.open();
                _lmbHandled = true;
            }
        }

        // Project Title
        LabFont::drawText(w * 0.5f - 145.0f, 6.0f, "Lab FilmMaker [lfm] - 2026", 1.60f, Vec3(1.0f, 0.82f, 0.22f));

        // Render Export Video Button in Top-Right
        float btnW = 90.0f;
        float btnX = w - btnW - 10.0f;
        if (LabTheme::drawButton(btnX, 3.0f, btnW, 24.0f, "Render", false, true,
                                static_cast<float>(_mouseX), static_cast<float>(_mouseY), _lmb)) {
            if (!_lmbHandled && !_mapModal.isOpen()) {
                _exportModal.open();
                _lmbHandled = true;
            }
        }
    }

    void Workspace::renderLeftPanel(float x, float y, float w, float h) {
        // Main panel frame
        LabTheme::drawPanel(x, y, w, h, "");

        // 3 Modern Distinct Tabs at Top of Left Panel
        float tabW = (w - 12.0f) / 3.0f;
        float tabH = 26.0f;
        float tabY = y + 2.0f;

        bool tab0 = (_activeLeftTab == LeftPanelTab::SceneHierarchy);
        bool tab1 = (_activeLeftTab == LeftPanelTab::AnimationLibrary);
        bool tab2 = (_activeLeftTab == LeftPanelTab::AudioLibrary);

        if (LabTheme::drawButton(x + 4.0f, tabY, tabW, tabH, "Scena", tab0, tab0,
                                static_cast<float>(_mouseX), static_cast<float>(_mouseY), _lmb)) {
            if (!_lmbHandled) { _activeLeftTab = LeftPanelTab::SceneHierarchy; _lmbHandled = true; }
        }
        if (LabTheme::drawButton(x + 4.0f + tabW + 2.0f, tabY, tabW, tabH, "Animacje", tab1, tab1,
                                static_cast<float>(_mouseX), static_cast<float>(_mouseY), _lmb)) {
            if (!_lmbHandled) { _activeLeftTab = LeftPanelTab::AnimationLibrary; _lmbHandled = true; }
        }
        if (LabTheme::drawButton(x + 4.0f + (tabW + 2.0f) * 2.0f, tabY, tabW, tabH, "Audio", tab2, tab2,
                                static_cast<float>(_mouseX), static_cast<float>(_mouseY), _lmb)) {
            if (!_lmbHandled) { _activeLeftTab = LeftPanelTab::AudioLibrary; _lmbHandled = true; }
        }

        float contentY = y + tabH + 6.0f;
        float contentH = h - tabH - 8.0f;

        if (_activeLeftTab == LeftPanelTab::SceneHierarchy) {
            renderSceneTab(x, contentY, w, contentH);
        } else if (_activeLeftTab == LeftPanelTab::AnimationLibrary) {
            renderAnimationsTab(x, contentY, w, contentH);
        } else if (_activeLeftTab == LeftPanelTab::AudioLibrary) {
            renderAudioTab(x, contentY, w, contentH);
        }
    }

    void Workspace::renderSceneTab(float x, float y, float w, float h) {
        // Toolbar: [ Search Input ] [ ^ ] [ v ]
        float tbY = y + 2.0f;
        float tbH = 24.0f;

        float searchW = w - 74.0f;
        LabTheme::drawBevel(x + 6.0f, tbY, searchW, tbH, true);
        LabFont::drawText(x + 14.0f, tbY + 4.0f, "Szukaj kosci / swiatel...", 1.30f, Vec3(0.68f, 0.70f, 0.75f));

        // Up/Down scroll buttons
        if (LabTheme::drawButton(x + w - 62.0f, tbY, 26.0f, tbH, "^", false, false,
                                static_cast<float>(_mouseX), static_cast<float>(_mouseY), _lmb)) {
            if (!_lmbHandled) { _treeScrollY = std::max(0.0f, _treeScrollY - 48.0f); _lmbHandled = true; }
        }
        if (LabTheme::drawButton(x + w - 32.0f, tbY, 26.0f, tbH, "v", false, false,
                                static_cast<float>(_mouseX), static_cast<float>(_mouseY), _lmb)) {
            if (!_lmbHandled) { _treeScrollY += 48.0f; _lmbHandled = true; }
        }

        // Details card height at bottom
        float detailsCardH = 120.0f;
        float treeY = tbY + tbH + 6.0f;
        float treeH = h - (treeY - y) - detailsCardH - 8.0f;

        // Tree frame
        LabTheme::drawBevel(x + 6.0f, treeY, w - 12.0f, treeH, true);

        // Render Tree Nodes
        float cursorY = treeY + 4.0f - _treeScrollY;
        float itemH = 22.0f;

        auto renderNode = [&](auto& self, ElementNode& node, int indent) -> void {
            float itemX = x + 10.0f + static_cast<float>(indent) * 14.0f;
            float itemW = w - 24.0f - static_cast<float>(indent) * 14.0f;

            if (cursorY >= treeY && (cursorY + itemH) <= (treeY + treeH)) {
                if (node.selected) {
                    Renderer::drawRect(x + 8.0f, cursorY - 1.0f, w - 16.0f, itemH, Vec3(0.18f, 0.35f, 0.55f));
                    Renderer::drawRect(x + 8.0f, cursorY - 1.0f, w - 16.0f, 1.0f, Vec3(0.30f, 0.75f, 1.0f));
                    Renderer::drawRect(x + 8.0f, cursorY + itemH - 2.0f, w - 16.0f, 1.0f, Vec3(0.30f, 0.75f, 1.0f));
                }

                // Click selection
                if (_lmb && !_lmbHandled &&
                    _mouseX >= itemX && _mouseX <= (itemX + itemW) &&
                    _mouseY >= cursorY && _mouseY <= (cursorY + itemH)) {
                    _animSet.selectElement(node.name);
                    if (!node.children.empty()) {
                        node.expanded = !node.expanded;
                    }
                    _lmbHandled = true;
                }

                Vec3 nodeCol(0.92f, 0.94f, 0.96f);
                if (node.type == ElementType::Actor) nodeCol = Vec3(0.25f, 0.88f, 1.0f);
                if (node.type == ElementType::Camera) nodeCol = Vec3(1.0f, 0.88f, 0.25f);
                if (node.type == ElementType::Light) nodeCol = Vec3(1.0f, 0.65f, 0.25f);

                std::string prefix = node.children.empty() ? "  • " : (node.expanded ? " v " : " > ");
                LabFont::drawText(itemX, cursorY + 2.0f, prefix + node.displayName, 1.35f, nodeCol);
            }

            cursorY += itemH;
            if (node.expanded) {
                for (auto& child : node.children) {
                    self(self, child, indent + 1);
                }
            }
        };

        for (auto& root : _animSet.getRoots()) {
            renderNode(renderNode, root, 0);
        }

        // Details card at bottom of Scene Tab
        float cardY = treeY + treeH + 6.0f;
        LabTheme::drawBevel(x + 6.0f, cardY, w - 12.0f, detailsCardH, false);

        ElementNode* sel = _animSet.getSelectedElement();
        if (sel) {
            LabFont::drawText(x + 14.0f, cardY + 8.0f, "Element: " + sel->displayName, 1.40f, Vec3(1.0f, 0.85f, 0.25f));
            std::ostringstream posSs;
            posSs << "Pozycja: X=" << std::fixed << std::setprecision(2) << sel->localPosition.x
                  << " Y=" << sel->localPosition.y << " Z=" << sel->localPosition.z;
            LabFont::drawText(x + 14.0f, cardY + 30.0f, posSs.str(), 1.30f, Vec3(0.85f, 0.88f, 0.92f));

            if (sel->boneIndex >= 0) {
                LabFont::drawText(x + 14.0f, cardY + 50.0f, "ID Kosci: " + std::to_string(sel->boneIndex) + " (Szkielet guard.glb)", 1.25f, Vec3(0.30f, 0.85f, 1.0f));

                float bY = cardY + 76.0f;
                LabTheme::drawButton(x + 14.0f, bY, 95.0f, 24.0f, "Translate", true, false,
                                    static_cast<float>(_mouseX), static_cast<float>(_mouseY), _lmb);
                LabTheme::drawButton(x + 116.0f, bY, 80.0f, 24.0f, "Rotate", false, false,
                                    static_cast<float>(_mouseX), static_cast<float>(_mouseY), _lmb);
                LabTheme::drawButton(x + 204.0f, bY, 95.0f, 24.0f, "Bind Pose", false, false,
                                    static_cast<float>(_mouseX), static_cast<float>(_mouseY), _lmb);
            }
        } else {
            LabFont::drawText(x + 14.0f, cardY + 12.0f, "Brak zaznaczonego elementu.", 1.35f, Vec3(0.65f, 0.68f, 0.72f));
            LabFont::drawText(x + 14.0f, cardY + 34.0f, "Kliknij kosc lub obiekt, aby go edytowac.", 1.25f, Vec3(0.55f, 0.58f, 0.62f));
        }
    }

    void Workspace::renderAnimationsTab(float x, float y, float w, float h) {
        float titleY = y + 4.0f;
        std::ostringstream hdrSs;
        hdrSs << "BIBLIOTEKA ANIMACJI (" << _availableAnimations.size() << " klipow)";
        LabFont::drawText(x + 12.0f, titleY, hdrSs.str(), 1.40f, Vec3(1.0f, 0.85f, 0.25f));

        // Scroll buttons
        if (LabTheme::drawButton(x + w - 62.0f, titleY - 2.0f, 26.0f, 22.0f, "^", false, false,
                                static_cast<float>(_mouseX), static_cast<float>(_mouseY), _lmb)) {
            if (!_lmbHandled) { _animScrollY = std::max(0.0f, _animScrollY - 48.0f); _lmbHandled = true; }
        }
        if (LabTheme::drawButton(x + w - 32.0f, titleY - 2.0f, 26.0f, 22.0f, "v", false, false,
                                static_cast<float>(_mouseX), static_cast<float>(_mouseY), _lmb)) {
            if (!_lmbHandled) { _animScrollY += 48.0f; _lmbHandled = true; }
        }

        float listY = titleY + 26.0f;
        float listH = h - 34.0f;
        LabTheme::drawBevel(x + 6.0f, listY, w - 12.0f, listH, true);

        float curY = listY + 4.0f - _animScrollY;
        float itemH = 34.0f;

        for (size_t i = 0; i < _availableAnimations.size(); ++i) {
            const std::string& animName = _availableAnimations[i];

            if (curY >= listY && (curY + itemH) <= (listY + listH)) {
                // Item background
                Renderer::drawRect(x + 8.0f, curY, w - 16.0f, itemH - 2.0f, Vec3(0.20f, 0.22f, 0.25f));

                // Animation title
                LabFont::drawText(x + 14.0f, curY + 6.0f, animName, 1.35f, Vec3(0.92f, 0.94f, 0.96f));

                // Action buttons: [ Preview ] & [ + Track ]
                float pBtnW = 68.0f;
                float pBtnX = x + w - pBtnW * 2.0f - 24.0f;
                if (LabTheme::drawButton(pBtnX, curY + 4.0f, pBtnW, 24.0f, "Preview", false, false,
                                        static_cast<float>(_mouseX), static_cast<float>(_mouseY), _lmb)) {
                    if (!_lmbHandled && _onPreviewAnimation) {
                        _onPreviewAnimation(animName);
                        _lmbHandled = true;
                    }
                }

                float addBtnX = x + w - pBtnW - 14.0f;
                if (LabTheme::drawButton(addBtnX, curY + 4.0f, pBtnW, 24.0f, "+ Track", false, true,
                                        static_cast<float>(_mouseX), static_cast<float>(_mouseY), _lmb)) {
                    if (!_lmbHandled) {
                        float curT = _sequencer.getCurrentTime();
                        ActorClipBlock block;
                        block.actorName = "guard_01";
                        block.clipName = animName;
                        block.startTime = curT;
                        block.duration = 6.0f;
                        block.color = Vec3(0.30f, 0.55f, 0.80f);
                        _sequencer.addActorBlock(block);
                        _lmbHandled = true;
                    }
                }
            }

            curY += itemH;
        }
    }

    void Workspace::renderAudioTab(float x, float y, float w, float h) {
        float titleY = y + 4.0f;
        std::ostringstream hdrSs;
        hdrSs << "BIBLIOTEKA DZWIEKU (" << _availableAudio.size() << " plikow WAV)";
        LabFont::drawText(x + 12.0f, titleY, hdrSs.str(), 1.40f, Vec3(0.35f, 0.95f, 0.45f));

        // Scroll buttons
        if (LabTheme::drawButton(x + w - 62.0f, titleY - 2.0f, 26.0f, 22.0f, "^", false, false,
                                static_cast<float>(_mouseX), static_cast<float>(_mouseY), _lmb)) {
            if (!_lmbHandled) { _audioScrollY = std::max(0.0f, _audioScrollY - 48.0f); _lmbHandled = true; }
        }
        if (LabTheme::drawButton(x + w - 32.0f, titleY - 2.0f, 26.0f, 22.0f, "v", false, false,
                                static_cast<float>(_mouseX), static_cast<float>(_mouseY), _lmb)) {
            if (!_lmbHandled) { _audioScrollY += 48.0f; _lmbHandled = true; }
        }

        float listY = titleY + 26.0f;
        float listH = h - 34.0f;
        LabTheme::drawBevel(x + 6.0f, listY, w - 12.0f, listH, true);

        float curY = listY + 4.0f - _audioScrollY;
        float itemH = 34.0f;

        for (size_t i = 0; i < _availableAudio.size(); ++i) {
            const auto& snd = _availableAudio[i];

            if (curY >= listY && (curY + itemH) <= (listY + listH)) {
                // Item background
                Renderer::drawRect(x + 8.0f, curY, w - 16.0f, itemH - 2.0f, Vec3(0.18f, 0.22f, 0.20f));

                // Audio title & channel
                LabFont::drawText(x + 14.0f, curY + 6.0f, snd.name, 1.30f, Vec3(0.85f, 0.95f, 0.88f));

                // Action buttons: [ Play ] & [ + Audio ]
                float pBtnW = 68.0f;
                float pBtnX = x + w - pBtnW * 2.0f - 24.0f;
                if (LabTheme::drawButton(pBtnX, curY + 4.0f, pBtnW, 24.0f, "Play", false, false,
                                        static_cast<float>(_mouseX), static_cast<float>(_mouseY), _lmb)) {
                    if (!_lmbHandled) {
                        AudioEngine::playCustomSound(snd.path, 1.0f);
                        _lmbHandled = true;
                    }
                }

                float addBtnX = x + w - pBtnW - 14.0f;
                if (LabTheme::drawButton(addBtnX, curY + 4.0f, pBtnW, 24.0f, "+ Audio", false, true,
                                        static_cast<float>(_mouseX), static_cast<float>(_mouseY), _lmb)) {
                    if (!_lmbHandled) {
                        float curT = _sequencer.getCurrentTime();
                        AudioTrackClip clip;
                        clip.name = snd.name;
                        clip.soundPath = snd.path;
                        clip.channel = snd.channel;
                        clip.startTime = curT;
                        clip.duration = snd.duration;
                        clip.volume = 1.0f;
                        clip.waveform = Sequencer::extractWavWaveform(snd.path, 32);
                        _sequencer.addAudioClip(clip);
                        _lmbHandled = true;
                    }
                }
            }

            curY += itemH;
        }
    }

    void Workspace::renderViewportToolbar(const ViewportRect& r, const std::string& camName) {
        float barY = r.y + r.h + 2.0f;
        float barH = 30.0f;
        LabTheme::drawPanel(r.x, barY, r.w, barH, "");

        // Transport controls: [|<] [ ▶ / || ] [>|]
        float bX = r.x + 8.0f;
        if (LabTheme::drawButton(bX, barY + 3.0f, 28.0f, 24.0f, "|<", false, false,
                                static_cast<float>(_mouseX), static_cast<float>(_mouseY), _lmb)) {
            if (!_lmbHandled) { _sequencer.setCurrentTime(0.0f); _lmbHandled = true; }
        }
        bX += 32.0f;
        std::string playLabel = _sequencer.isPlaying() ? "||" : ">";
        if (LabTheme::drawButton(bX, barY + 3.0f, 36.0f, 24.0f, playLabel.c_str(), _sequencer.isPlaying(), false,
                                static_cast<float>(_mouseX), static_cast<float>(_mouseY), _lmb)) {
            if (!_lmbHandled) { _sequencer.togglePlay(); _lmbHandled = true; }
        }
        bX += 40.0f;
        if (LabTheme::drawButton(bX, barY + 3.0f, 28.0f, 24.0f, ">|", false, false,
                                static_cast<float>(_mouseX), static_cast<float>(_mouseY), _lmb)) {
            if (!_lmbHandled) { _sequencer.setCurrentTime(_sequencer.getTotalDuration()); _lmbHandled = true; }
        }

        // Camera name tag dropdown on the right of viewport toolbar
        float tagW = 140.0f;
        float tagX = r.x + r.w - tagW - 8.0f;
        LabTheme::drawBevel(tagX, barY + 3.0f, tagW, 24.0f, false);
        LabFont::drawText(tagX + 10.0f, barY + 5.0f, camName, 1.40f, Vec3(1.0f, 0.88f, 0.30f));
    }

    void Workspace::renderSequencer(float x, float y, float w, float h) {
        // Mode Tabs & Action Buttons Header
        LabTheme::drawPanel(x, y, w, h, "");

        float tabW = 115.0f;
        float tabH = 26.0f;
        bool isClip = (_sequencer.getMode() == SequencerMode::ClipEditor);
        bool isMotion = (_sequencer.getMode() == SequencerMode::MotionEditor);
        bool isGraph = (_sequencer.getMode() == SequencerMode::GraphEditor);

        if (LabTheme::drawButton(x + 8.0f, y + 2.0f, tabW, tabH, "Clip Editor", isClip, false,
                                static_cast<float>(_mouseX), static_cast<float>(_mouseY), _lmb)) {
            if (!_lmbHandled) { _sequencer.setMode(SequencerMode::ClipEditor); _lmbHandled = true; }
        }
        if (LabTheme::drawButton(x + 128.0f, y + 2.0f, tabW, tabH, "Motion Editor", isMotion, false,
                                static_cast<float>(_mouseX), static_cast<float>(_mouseY), _lmb)) {
            if (!_lmbHandled) { _sequencer.setMode(SequencerMode::MotionEditor); _lmbHandled = true; }
        }
        if (LabTheme::drawButton(x + 248.0f, y + 2.0f, tabW, tabH, "Graph Editor", isGraph, false,
                                static_cast<float>(_mouseX), static_cast<float>(_mouseY), _lmb)) {
            if (!_lmbHandled) { _sequencer.setMode(SequencerMode::GraphEditor); _lmbHandled = true; }
        }

        // Action Buttons on Sequencer Bar: [ + Shot ] [ Delete ]
        float actX = x + 375.0f;
        if (LabTheme::drawButton(actX, y + 2.0f, 75.0f, tabH, "+ Shot", false, false,
                                static_cast<float>(_mouseX), static_cast<float>(_mouseY), _lmb)) {
            if (!_lmbHandled) {
                float curT = _sequencer.getCurrentTime();
                FilmShot s;
                s.id = "shot_" + std::to_string(_sequencer.getShots().size() + 1);
                s.name = "Shot " + std::to_string(_sequencer.getShots().size() + 1);
                s.startTime = curT;
                s.duration = 10.0f;
                s.cameraName = "camera1";
                s.color = Vec3(0.35f, 0.65f, 0.45f);
                _sequencer.addShot(s);
                _lmbHandled = true;
            }
        }

        if (LabTheme::drawButton(actX + 82.0f, y + 2.0f, 65.0f, tabH, "Delete", false, false,
                                static_cast<float>(_mouseX), static_cast<float>(_mouseY), _lmb, true, false)) {
            if (!_lmbHandled) {
                _sequencer.removeSelected();
                _lmbHandled = true;
            }
        }

        // Current timecode in sequencer header
        std::string tc = Sequencer::formatTimecode(_sequencer.getCurrentTime()) + "  /  " + Sequencer::formatTimecode(_sequencer.getTotalDuration());
        LabFont::drawText(x + w - 240.0f, y + 5.0f, tc, 1.45f, Vec3(1.0f, 1.0f, 1.0f));

        // Time Ruler Geometry
        float rulerY = y + 30.0f;
        float headerW = 105.0f;
        float trackAreaX = x + headerW;
        float trackAreaW = w - headerW - 12.0f;
        LabTheme::drawBevel(trackAreaX, rulerY, trackAreaW, 22.0f, true);

        float totalDur = _sequencer.getTotalDuration();
        for (float t = 0.0f; t <= totalDur; t += 5.0f) {
            float tx = trackAreaX + (t / totalDur) * trackAreaW;
            std::ostringstream ss;
            ss << static_cast<int>(t) << "s";
            LabFont::drawText(tx - 8.0f, rulerY + 3.0f, ss.str(), 1.30f, Vec3(0.85f, 0.88f, 0.92f));
            LabTheme::drawBevel(tx, rulerY + 14.0f, 1.0f, 8.0f, false);
        }

        // --- TRACKS AREA ---
        float tracksY = rulerY + 24.0f;

        // 1. SHOTS TRACK
        float shotH = 44.0f;
        LabTheme::drawBevel(x + 6.0f, tracksY, headerW - 8.0f, shotH, false);
        LabFont::drawText(x + 12.0f, tracksY + 14.0f, "SHOTS", 1.45f, Vec3(1.0f, 0.88f, 0.25f));
        LabTheme::drawBevel(trackAreaX, tracksY, trackAreaW, shotH, true);

        for (size_t i = 0; i < _sequencer.getShots().size(); ++i) {
            const auto& shot = _sequencer.getShots()[i];
            float sx = trackAreaX + (shot.startTime / totalDur) * trackAreaW;
            float sw = (shot.duration / totalDur) * trackAreaW;
            bool isSel = (_sequencer.getSelectedShotIndex() == static_cast<int>(i));

            // Selection & Block Fill
            if (isSel) {
                Renderer::drawRect(sx + 1.0f, tracksY + 2.0f, sw - 2.0f, shotH - 4.0f, Vec3(0.20f, 0.45f, 0.30f));
                // Bold gold outline for selected shot
                Renderer::drawRect(sx + 1.0f, tracksY + 2.0f, sw - 2.0f, 2.0f, Vec3(1.0f, 0.85f, 0.20f));
                Renderer::drawRect(sx + 1.0f, tracksY + shotH - 4.0f, sw - 2.0f, 2.0f, Vec3(1.0f, 0.85f, 0.20f));
                Renderer::drawRect(sx + 1.0f, tracksY + 2.0f, 2.0f, shotH - 4.0f, Vec3(1.0f, 0.85f, 0.20f));
                Renderer::drawRect(sx + sw - 3.0f, tracksY + 2.0f, 2.0f, shotH - 4.0f, Vec3(1.0f, 0.85f, 0.20f));
            } else {
                LabTheme::drawBevel(sx + 1.0f, tracksY + 2.0f, sw - 2.0f, shotH - 4.0f, false);
            }

            // Click hit testing for Shot selection
            if (_lmb && !_lmbHandled &&
                _mouseX >= sx && _mouseX <= (sx + sw) &&
                _mouseY >= tracksY && _mouseY <= (tracksY + shotH)) {
                _sequencer.setSelectedShotIndex(static_cast<int>(i));
                _lmbHandled = true;
            }

            LabFont::drawText(sx + 8.0f, tracksY + 14.0f, shot.name, 1.35f, Vec3(1.0f, 1.0f, 1.0f));
        }

        // 2. ACTORS ANIMATION TRACK
        float actorY = tracksY + shotH + 4.0f;
        float actorH = 40.0f;
        LabTheme::drawBevel(x + 6.0f, actorY, headerW - 8.0f, actorH, false);
        LabFont::drawText(x + 12.0f, actorY + 12.0f, "ACTORS", 1.45f, Vec3(0.25f, 0.88f, 1.0f));
        LabTheme::drawBevel(trackAreaX, actorY, trackAreaW, actorH, true);

        for (size_t i = 0; i < _sequencer.getActorBlocks().size(); ++i) {
            const auto& block = _sequencer.getActorBlocks()[i];
            float ax = trackAreaX + (block.startTime / totalDur) * trackAreaW;
            float aw = (block.duration / totalDur) * trackAreaW;
            bool isSel = (_sequencer.getSelectedActorBlockIndex() == static_cast<int>(i));

            if (isSel) {
                Renderer::drawRect(ax + 1.0f, actorY + 2.0f, aw - 2.0f, actorH - 4.0f, Vec3(0.20f, 0.35f, 0.60f));
                Renderer::drawRect(ax + 1.0f, actorY + 2.0f, aw - 2.0f, 2.0f, Vec3(0.35f, 0.85f, 1.0f));
                Renderer::drawRect(ax + 1.0f, actorY + actorH - 4.0f, aw - 2.0f, 2.0f, Vec3(0.35f, 0.85f, 1.0f));
                Renderer::drawRect(ax + 1.0f, actorY + 2.0f, 2.0f, actorH - 4.0f, Vec3(0.35f, 0.85f, 1.0f));
                Renderer::drawRect(ax + aw - 3.0f, actorY + 2.0f, 2.0f, actorH - 4.0f, Vec3(0.35f, 0.85f, 1.0f));
            } else {
                LabTheme::drawBevel(ax + 1.0f, actorY + 2.0f, aw - 2.0f, actorH - 4.0f, false);
            }

            // Click hit testing for Actor block selection
            if (_lmb && !_lmbHandled &&
                _mouseX >= ax && _mouseX <= (ax + aw) &&
                _mouseY >= actorY && _mouseY <= (actorY + actorH)) {
                _sequencer.setSelectedActorBlockIndex(static_cast<int>(i));
                _lmbHandled = true;
            }

            LabFont::drawText(ax + 8.0f, actorY + 12.0f, block.clipName, 1.35f, Vec3(1.0f, 1.0f, 1.0f));
        }

        // 3. AUDIO TRACK WITH AUTHENTIC WAVEFORM
        float audioY = actorY + actorH + 4.0f;
        float audioH = 70.0f;
        LabTheme::drawBevel(x + 6.0f, audioY, headerW - 8.0f, audioH, false);
        LabFont::drawText(x + 12.0f, audioY + 26.0f, "AUDIO", 1.45f, Vec3(0.35f, 0.95f, 0.45f));
        LabTheme::drawBevel(trackAreaX, audioY, trackAreaW, audioH, true);

        for (size_t i = 0; i < _sequencer.getAudioClips().size(); ++i) {
            const auto& clip = _sequencer.getAudioClips()[i];
            float cx = trackAreaX + (clip.startTime / totalDur) * trackAreaW;
            float cw = (clip.duration / totalDur) * trackAreaW;
            bool isSel = (_sequencer.getSelectedAudioClipIndex() == static_cast<int>(i));

            if (isSel) {
                Renderer::drawRect(cx + 1.0f, audioY + 2.0f, cw - 2.0f, audioH - 4.0f, Vec3(0.18f, 0.35f, 0.25f));
                Renderer::drawRect(cx + 1.0f, audioY + 2.0f, cw - 2.0f, 2.0f, Vec3(0.40f, 1.0f, 0.50f));
                Renderer::drawRect(cx + 1.0f, audioY + audioH - 4.0f, cw - 2.0f, 2.0f, Vec3(0.40f, 1.0f, 0.50f));
                Renderer::drawRect(cx + 1.0f, audioY + 2.0f, 2.0f, audioH - 4.0f, Vec3(0.40f, 1.0f, 0.50f));
                Renderer::drawRect(cx + cw - 3.0f, audioY + 2.0f, 2.0f, audioH - 4.0f, Vec3(0.40f, 1.0f, 0.50f));
            } else {
                LabTheme::drawBevel(cx + 1.0f, audioY + 2.0f, cw - 2.0f, audioH - 4.0f, false);
            }

            // Click hit testing for Audio clip selection
            if (_lmb && !_lmbHandled &&
                _mouseX >= cx && _mouseX <= (cx + cw) &&
                _mouseY >= audioY && _mouseY <= (audioY + audioH)) {
                _sequencer.setSelectedAudioClipIndex(static_cast<int>(i));
                _lmbHandled = true;
            }

            LabFont::drawText(cx + 8.0f, audioY + 6.0f, clip.name, 1.30f, Vec3(1.0f, 0.92f, 0.35f));

            // Draw audio waveforms (Valve SFM bright green audio bars)
            if (!clip.waveform.empty()) {
                float waveCenterY = audioY + audioH * 0.5f + 8.0f;
                float stepX = (cw - 8.0f) / static_cast<float>(clip.waveform.size());
                for (size_t wIdx = 0; wIdx < clip.waveform.size(); ++wIdx) {
                    float barAmp = clip.waveform[wIdx] * 20.0f;
                    float barX = cx + 4.0f + static_cast<float>(wIdx) * stepX;
                    float barW = std::max(1.0f, stepX - 1.0f);
                    Renderer::drawRect(barX, waveCenterY - barAmp, barW, barAmp * 2.0f, Vec3(0.30f, 0.92f, 0.45f));
                }
            }
        }

        // --- INTERACTIVE TIMELINE SCRUBBING & SEEKING ---
        // Clicking or dragging anywhere on the Ruler or Track area scrubs the timeline immediately!
        float totalTrackH = (audioY + audioH) - rulerY;
        bool inTimelineArea = (_mouseX >= trackAreaX && _mouseX <= (trackAreaX + trackAreaW) &&
                               _mouseY >= rulerY && _mouseY <= (audioY + audioH));

        if (_lmb && inTimelineArea && !_mapModal.isOpen() && !_exportModal.isOpen()) {
            // Drag or click to scrub
            float newT = ((static_cast<float>(_mouseX) - trackAreaX) / trackAreaW) * totalDur;
            _sequencer.setCurrentTime(std::clamp(newT, 0.0f, totalDur));
            _isScrubbingTimeline = true;
        }

        // MOTION EDITOR RANGE OVERLAY (Green Selection Band if in Motion Editor)
        if (_sequencer.getMode() == SequencerMode::MotionEditor) {
            float selX = trackAreaX + (_sequencer.getSelectionStart() / totalDur) * trackAreaW;
            float selW = ((_sequencer.getSelectionEnd() - _sequencer.getSelectionStart()) / totalDur) * trackAreaW;
            float bandH = audioY + audioH - tracksY;
            LabTheme::drawBevel(selX, tracksY, selW, bandH, false);
        }

        // PLAYHEAD NEEDLE (High-visibility red needle with top diamond cursor)
        float curT = _sequencer.getCurrentTime();
        float playheadX = trackAreaX + (curT / totalDur) * trackAreaW;

        // Draw needle line
        Renderer::drawRect(playheadX - 1.0f, rulerY, 2.0f, totalTrackH, Vec3(1.0f, 0.25f, 0.25f));

        // Draw needle head
        Renderer::drawRect(playheadX - 4.0f, rulerY - 4.0f, 8.0f, 8.0f, Vec3(1.0f, 0.85f, 0.20f));
    }

} // namespace Lab::LFM
