#include "Lab.h"
#include "LabFilm.h"
#include "LabCable.h"
#include "LabTheme.h"
#include "LabFont.h"
#include "LabSkeletal.h"
#include "LabAudio.h"
#include "LabMap.h"
#include "LFM/LFMWorkspace.h"
#include <glad/gl.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <memory>
#include <vector>
#include <filesystem>
#include <fstream>
#include <cmath>
#include <cstring>

using namespace Lab;

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
        0, 0, 0, 0, 54, 0, 0, 0, 40, 0, 0, 0,
        (unsigned char)(width), (unsigned char)(width >> 8), (unsigned char)(width >> 16), (unsigned char)(width >> 24),
        (unsigned char)(height), (unsigned char)(height >> 8), (unsigned char)(height >> 16), (unsigned char)(height >> 24),
        1, 0, 24, 0, 0, 0, 0, 0,
        (unsigned char)(imageSize), (unsigned char)(imageSize >> 8), (unsigned char)(imageSize >> 16), (unsigned char)(imageSize >> 24),
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
    };
    std::filesystem::create_directories(std::filesystem::path(filename).parent_path());
    std::ofstream out(filename, std::ios::binary);
    if (!out.is_open()) return;
    out.write((char*)header, 54);
    std::vector<unsigned char> row(rowSize, 0);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            int srcIdx = (y * width + x) * 4;
            row[x * 3 + 0] = pixels[srcIdx + 2];
            row[x * 3 + 1] = pixels[srcIdx + 1];
            row[x * 3 + 2] = pixels[srcIdx + 0];
        }
        out.write((char*)row.data(), rowSize);
    }
}

struct OffscreenFBO {
    GLuint fbo = 0;
    GLuint colorTex = 0;
    GLuint depthRbo = 0;
    int width = 0;
    int height = 0;

    bool init(int w, int h) {
        destroy();
        width = w;
        height = h;

        glCreateFramebuffers(1, &fbo);

        glCreateTextures(GL_TEXTURE_2D, 1, &colorTex);
        glTextureStorage2D(colorTex, 1, GL_RGB8, w, h);
        glTextureParameteri(colorTex, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTextureParameteri(colorTex, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glNamedFramebufferTexture(fbo, GL_COLOR_ATTACHMENT0, colorTex, 0);

        glCreateRenderbuffers(1, &depthRbo);
        glNamedRenderbufferStorage(depthRbo, GL_DEPTH24_STENCIL8, w, h);
        glNamedFramebufferRenderbuffer(fbo, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, depthRbo);

        GLenum drawBuf = GL_COLOR_ATTACHMENT0;
        glNamedFramebufferDrawBuffers(fbo, 1, &drawBuf);

        GLenum status = glCheckNamedFramebufferStatus(fbo, GL_FRAMEBUFFER);
        if (status != GL_FRAMEBUFFER_COMPLETE) {
            std::cerr << "[LFM] Offscreen FBO incomplete: 0x" << std::hex << status << std::dec << "\n";
            return false;
        }
        return true;
    }

    void destroy() {
        if (fbo) { glDeleteFramebuffers(1, &fbo); fbo = 0; }
        if (colorTex) { glDeleteTextures(1, &colorTex); colorTex = 0; }
        if (depthRbo) { glDeleteRenderbuffers(1, &depthRbo); depthRbo = 0; }
        width = 0;
        height = 0;
    }

    ~OffscreenFBO() {
        destroy();
    }
};

class LfmApp : public Engine {
public:
    LfmApp(const std::string& sceneFile = "")
        : Engine("Lab FilmMaker [lfm] - 2026", 1600, 900),
          _scenePath(sceneFile) {
    }

    void enableScreenshotMode() { _takeScreenshot = true; }
    void enableRenderTest(const std::string& outPath, int w = 1280, int h = 720, int fps = 30, int frames = 90) {
        _renderTestMode = true;
        _renderTestOut = outPath;
        _exportW = w;
        _exportH = h;
        _exportFps = fps;
        _exportTotalFrames = frames;
        _exportDuration = static_cast<float>(frames) / static_cast<float>(fps);
    }

    void onInit() override {
        LabLog::info("Initializing Lab FilmMaker [lfm] Professional DCC Workspace...");
        Renderer::init();
        AudioEngine::init();
        LabTheme::loadThemeFromJson("assets/theme/theme.json");
        setCursorCaptured(false);

        // 1. Load real guard character mesh, skeleton, and animations
        bool loadedBot = GLTFLoader::load("assets/models/guard.glb", _botSkeleton, _botAnimations, _botMesh);
        if (loadedBot && _botSkeleton && _botMesh) {
            LabLog::info("Loaded guard.glb (" + std::to_string(_botSkeleton->getBoneCount()) + " bones) successfully into LFM!");
            FBXLoader::loadAllFromDirectory("assets/animations", _botAnimations, _botSkeleton.get());

            _botAnimator.setSkeleton(_botSkeleton);
            for (const auto& clip : _botAnimations) {
                _botAnimator.addClip(clip);
            }
            _botAnimator.playAnimation(_botAnimator.hasClip("Pistol Idle") ? "Pistol Idle" : "Idle", true);

            // Populate the Animation Set Editor with real bone rig hierarchy
            _workspace.getAnimationSet().buildFromSkeleton("guard_01", *_botSkeleton, "guard.glb");

        } else {
            LabLog::warn("guard.glb could not be loaded; falling back to default rig.");
        }

        // Supply available animation clips to workspace library
        std::vector<std::string> clipNames;
        for (const auto& clip : _botAnimations) {
            clipNames.push_back(clip.name);
        }
        _workspace.setAvailableAnimations(clipNames);

        // Preview animation callback from library
        _workspace.setPreviewAnimationCallback([this](const std::string& animName) {
            if (_botAnimator.hasClip(animName)) {
                _botAnimator.playAnimation(animName, true, 0.15f);
            }
        });

        // Dynamic Map Loader callback from MapBrowserModal
        _workspace.setLoadMapCallback([this](const std::string& path) {
            loadSceneOrMap(path);
        });

        // 2. Load guard high-res texture
        if (std::filesystem::exists("assets/textures/guard.bmp")) {
            _guardTexture = std::make_unique<Texture>("assets/textures/guard.bmp");
        } else if (std::filesystem::exists("assets/textures/guard_body.bmp")) {
            _guardTexture = std::make_unique<Texture>("assets/textures/guard_body.bmp");
        }

        // 3. Build procedural hanging catenary cable mesh
        CableMesh cMesh = CableGenerator::generate(Vec3(-6.0f, 6.0f, 0.0f), Vec3(6.0f, 5.5f, 0.0f), 0.85f, 0.035f, 16, 4);
        std::vector<Vertex> cableVerts;
        cableVerts.reserve(cMesh.vertices.size());
        for (const auto& v : cMesh.vertices) {
            cableVerts.emplace_back(v.position, v.normal, v.texCoords, Vec3(0.18f, 0.18f, 0.20f));
        }
        _cableMesh = std::make_unique<Mesh>(cableVerts, cMesh.indices);

        // 4. Build sample camera motion path trajectory
        _cameraMotionPath.clear();
        for (float t = 0.0f; t <= 60.0f; t += 2.0f) {
            float phase = (t / 60.0f) * 3.14159f * 1.5f;
            Vec3 p(-10.0f + std::sin(phase) * 12.0f, 2.5f + std::sin(phase * 2.0f) * 0.8f, 6.0f - t * 0.15f);
            _cameraMotionPath.push_back(p);
        }

        // 5. Load external map or scene if specified
        if (!_scenePath.empty() && std::filesystem::exists(_scenePath)) {
            loadSceneOrMap(_scenePath);
        }

        if (_renderTestMode) {
            _offscreenFbo.init(_exportW, _exportH);
            std::string tempAudio = "output/temp_cinematic_audio.wav";
            _workspace.getSequencer().mixAudioTrackToWav(tempAudio, _exportDuration);
            _isExportActive = _exportSession.begin(_exportW, _exportH, _exportFps, _exportTotalFrames, _renderTestOut, tempAudio);
            _exportCurFrame = 0;
        }
    }

    void loadSceneOrMap(const std::string& path) {
        if (path.empty() || !std::filesystem::exists(path)) return;
        _map = LabMap::loadFromFile(path);
        if (_map) {
            LabLog::info("Loaded scene into LFM: " + path + " (" +
                         std::to_string(_map->brushes.size()) + " brushes, " +
                         std::to_string(_map->lights.size()) + " lights, " +
                         std::to_string(_map->cameras.size()) + " cameras)");
            _workspace.getAnimationSet().clearCameras();
            _workspace.getAnimationSet().clearLights();
            for (const auto& l : _map->lights) {
                _workspace.getAnimationSet().addLight(l.name, l.position, l.color);
            }
            for (const auto& c : _map->cameras) {
                _workspace.getAnimationSet().addCamera(c.name, c.position);
            }
        }
    }

    void onFixedUpdate(float fixedDelta) override {
        (void)fixedDelta;
    }

    void onUpdate(const Time& time) override {
        float dt = time.delta;

        double mx = 0, my = 0;
        getCursorPos(mx, my);

        bool lmb = Input::isMouseButtonPressed(0);
        bool rmb = Input::isMouseButtonPressed(1);

        _workspace.update(dt, mx, my, lmb, rmb);
        AudioEngine::update(dt);

        // Keyboard shortcuts for timeline scrubbing, playback & frame stepping
        if (Input::isKeyPressed(GLFW_KEY_SPACE)) {
            _workspace.getSequencer().togglePlay();
        }
        if (Input::isKeyPressed(GLFW_KEY_HOME)) {
            _workspace.getSequencer().setCurrentTime(0.0f);
        }
        if (Input::isKeyPressed(GLFW_KEY_LEFT)) {
            float cur = _workspace.getSequencer().getCurrentTime();
            _workspace.getSequencer().setCurrentTime(std::max(0.0f, cur - (1.0f / 30.0f)));
        }
        if (Input::isKeyPressed(GLFW_KEY_RIGHT)) {
            float cur = _workspace.getSequencer().getCurrentTime();
            _workspace.getSequencer().setCurrentTime(std::min(_workspace.getSequencer().getTotalDuration(), cur + (1.0f / 30.0f)));
        }
        if (Input::isKeyPressed(GLFW_KEY_DELETE)) {
            _workspace.getSequencer().removeSelected();
        }

        // Update secondary work camera flight navigation
        _workspace.getDualViewport().updateWorkCameraFlight(dt, mx, my, rmb);

        // Evaluate character animation and cinematic cameras
        float curT = _workspace.getSequencer().getCurrentTime();
        evaluateTimelinePose(curT, dt);

        // Audio scrubbing & playback trigger
        if (_workspace.getSequencer().isPlaying()) {
            auto triggeredClips = _workspace.getSequencer().getTriggeredAudioClips(_prevSequencerTime, curT);
            for (const auto* clip : triggeredClips) {
                if (clip && std::filesystem::exists(clip->soundPath)) {
                    AudioEngine::playCustomSound(clip->soundPath, clip->volume);
                }
            }
        }
        _prevSequencerTime = curT;

        // Export dialog requests handling
        if (_workspace.getExportModal().isStartRequested()) {
            _workspace.getExportModal().clearRequests();

            _exportW = _workspace.getExportModal().getSelectedWidth();
            _exportH = _workspace.getExportModal().getSelectedHeight();
            _exportFps = _workspace.getExportModal().getSelectedFps();
            float totalDur = _workspace.getSequencer().getTotalDuration();
            _exportDuration = _workspace.getExportModal().getSelectedDuration(totalDur, 15.0f);
            _exportTotalFrames = static_cast<int>(_exportDuration * static_cast<float>(_exportFps));
            if (_exportTotalFrames <= 0) _exportTotalFrames = 60;
            _exportCurFrame = 0;

            _offscreenFbo.init(_exportW, _exportH);

            std::string outPath = _workspace.getExportModal().getOutputPath();
            std::string tempAudio = "output/temp_cinematic_audio.wav";
            _workspace.getSequencer().mixAudioTrackToWav(tempAudio, _exportDuration);
            bool ok = _exportSession.begin(_exportW, _exportH, _exportFps, _exportTotalFrames, outPath, tempAudio);
            _isExportActive = ok;
            if (!ok) {
                _offscreenFbo.destroy();
                _workspace.getExportModal().close();
            }
        }

        if (_workspace.getExportModal().isCancelRequested()) {
            _workspace.getExportModal().clearRequests();
            _exportSession.cancel();
            _isExportActive = false;
            _offscreenFbo.destroy();
        }
    }

    void evaluateTimelinePose(float curT, float dt = 0.0f) {
        (void)dt;
        // 1. Determine active actor block
        std::string targetClip = "Idle";
        float blockLocalTime = 0.0f;
        for (const auto& block : _workspace.getSequencer().getActorBlocks()) {
            if (block.actorName == "guard_01") {
                if (curT >= block.startTime && curT < (block.startTime + block.duration)) {
                    targetClip = block.clipName;
                    blockLocalTime = curT - block.startTime;
                    break;
                }
            }
        }

        // Map animation block to loaded skeletal clips
        std::string actualAnim = "Idle";
        if (targetClip == "guard_idle_ready" || targetClip == "Idle") {
            actualAnim = _botAnimator.hasClip("Pistol Idle") ? "Pistol Idle" : "Idle";
            _characterStagePos = Vec3(0.0f, 0.0f, -4.0f);
            _characterFacingYaw = 0.0f;
        } else if (targetClip == "guard_walk_patrol" || targetClip == "Walk") {
            actualAnim = _botAnimator.hasClip("Walk") ? "Walk" : "Walking";
            float walkAlpha = std::clamp(blockLocalTime / 20.0f, 0.0f, 1.0f);
            _characterStagePos = Vec3(0.0f, 0.0f, -4.0f + walkAlpha * 4.0f);
            _characterFacingYaw = 0.0f;
        } else if (targetClip == "operate_terminal" || targetClip == "Talk") {
            actualAnim = _botAnimator.hasClip("Talk") ? "Talk" : "Attack";
            _characterStagePos = Vec3(0.0f, 0.0f, 0.0f);
            _characterFacingYaw = 40.0f;
        } else if (targetClip == "combat_alert_aim" || targetClip == "Firing Rifle") {
            actualAnim = _botAnimator.hasClip("Firing Rifle") ? "Firing Rifle" : "Idle";
            _characterStagePos = Vec3(0.0f, 0.0f, 0.0f);
            _characterFacingYaw = -15.0f;
        } else if (_botAnimator.hasClip(targetClip)) {
            actualAnim = targetClip;
        }

        if (_botAnimator.getCurrentAnimationName() != actualAnim) {
            _botAnimator.playAnimation(actualAnim, true, 0.15f);
        }

        _botAnimator.setCurrentTime(blockLocalTime);

        // 2. Multi-shot camera cuts and director trajectories
        const LFM::FilmShot* activeShot = nullptr;
        for (const auto& s : _workspace.getSequencer().getShots()) {
            if (curT >= s.startTime && curT < (s.startTime + s.duration)) {
                activeShot = &s;
                break;
            }
        }
        if (!activeShot && !_workspace.getSequencer().getShots().empty()) {
            activeShot = &_workspace.getSequencer().getShots().back();
        }

        if (activeShot && activeShot->id == "shot003") {
            float shotLocalT = curT - activeShot->startTime;
            Vec3 camPos(2.2f - shotLocalT * 0.04f, 1.7f, 1.8f);
            _workspace.getDualViewport().setCinematicCameraTransform(camPos, -6.0f, -125.0f, 55.0f);
        } else if (activeShot && activeShot->id == "shot002") {
            float shotLocalT = curT - activeShot->startTime;
            float phase = (shotLocalT / 20.0f) * 3.14159f;
            Vec3 camPos(-6.0f + std::sin(phase) * 2.0f, 2.5f, 3.5f - shotLocalT * 0.1f);
            _workspace.getDualViewport().setCinematicCameraTransform(camPos, -10.0f, -42.0f, 65.0f);
        } else {
            float phase = (curT / 15.0f) * 3.14159f * 0.8f;
            Vec3 camPos(-10.0f + std::sin(phase) * 6.0f, 2.8f, 6.0f - curT * 0.12f);
            _workspace.getDualViewport().setCinematicCameraTransform(camPos, -10.0f, -38.0f, 72.0f);
        }
    }

    void onRender() override {
        int fbW = 0, fbH = 0;
        getFramebufferSize(fbW, fbH);
        int winW = (fbW > 0) ? fbW : getWidth();
        int winH = (fbH > 0) ? fbH : getHeight();

        // Check if asynchronous / step-by-step video export is in progress
        if (_isExportActive) {
            float t = static_cast<float>(_exportCurFrame) / static_cast<float>(_exportFps);
            evaluateTimelinePose(t, 1.0f / static_cast<float>(_exportFps));

            // Set camera aspect ratio matching exact target export resolution
            float targetAspect = static_cast<float>(_exportW) / static_cast<float>(_exportH);
            auto& cam = _workspace.getDualViewport().getCinematicCamera();
            cam.setPerspective(cam.fieldOfView, targetAspect, 0.1f, 1000.0f);

            // Bind dedicated offscreen FBO at EXACT target resolution
            glBindFramebuffer(GL_FRAMEBUFFER, _offscreenFbo.fbo);
            glViewport(0, 0, _exportW, _exportH);
            glDisable(GL_SCISSOR_TEST);
            glClearColor(0.08f, 0.09f, 0.12f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            Renderer::setSunLight(Vec3(-0.4f, -0.8f, -0.4f), Vec3(1.0f, 0.95f, 0.90f), Vec3(0.22f, 0.25f, 0.32f));
            Renderer::beginFrame(_workspace.getDualViewport().getCinematicCamera());
            render3DScene();
            Renderer::endFrame();

            // Read frame pixels with 1-byte alignment matching exact target width & height
            std::vector<uint8_t> rawPixels(static_cast<size_t>(_exportW * _exportH * 3));
            glReadBuffer(GL_COLOR_ATTACHMENT0);
            glPixelStorei(GL_PACK_ALIGNMENT, 1);
            glReadPixels(0, 0, _exportW, _exportH, GL_RGB, GL_UNSIGNED_BYTE, rawPixels.data());
            glPixelStorei(GL_PACK_ALIGNMENT, 4);

            // Unbind offscreen FBO back to default backbuffer
            glBindFramebuffer(GL_FRAMEBUFFER, 0);

            // Vertical flip rows of EXACT _exportW * 3 stride
            std::vector<uint8_t> flippedPixels(static_cast<size_t>(_exportW * _exportH * 3));
            size_t rowStride = static_cast<size_t>(_exportW * 3);
            for (int y = 0; y < _exportH; ++y) {
                std::memcpy(flippedPixels.data() + static_cast<size_t>(_exportH - 1 - y) * rowStride,
                            rawPixels.data() + static_cast<size_t>(y) * rowStride,
                            rowStride);
            }

            _exportSession.writeFrameRGB(flippedPixels.data());
            _exportCurFrame++;

            // Update modal state
            _workspace.getExportModal().setRenderingProgress(_exportCurFrame, _exportTotalFrames, t, _exportDuration);

            if (_exportCurFrame >= _exportTotalFrames) {
                _exportSession.finish();
                _isExportActive = false;
                _offscreenFbo.destroy();
                if (std::filesystem::exists("output/temp_cinematic_audio.wav")) {
                    std::error_code ec;
                    std::filesystem::remove("output/temp_cinematic_audio.wav", ec);
                }
                _workspace.getExportModal().setFinished(_exportSession.getOutputPath());
                if (_renderTestMode) {
                    stop();
                }
            }

            // Render live progress overlay modal in window viewport
            glViewport(0, 0, winW, winH);
            glClearColor(0.06f, 0.07f, 0.09f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            Renderer::beginUI(winW, winH);
            bool handled = false;
            double mx = 0, my = 0;
            getCursorPos(mx, my);
            bool lmb = Input::isMouseButtonPressed(0);
            _workspace.getExportModal().render(static_cast<float>(winW), static_cast<float>(winH), mx, my, lmb, handled);
            Renderer::endUI();
            return;
        }

        // 1. DUAL VIEWPORT 3D RENDERING PASS
        glEnable(GL_SCISSOR_TEST);

        // --- A. PRIMARY VIEWPORT PASS (Cinematic Render Camera) ---
        const auto& priR = _workspace.getDualViewport().getPrimaryRect();
        glViewport(static_cast<GLint>(priR.x), static_cast<GLint>(winH - (priR.y + priR.h)),
                   static_cast<GLsizei>(priR.w), static_cast<GLsizei>(priR.h));
        glScissor(static_cast<GLint>(priR.x), static_cast<GLint>(winH - (priR.y + priR.h)),
                  static_cast<GLsizei>(priR.w), static_cast<GLsizei>(priR.h));

        glClearColor(0.08f, 0.09f, 0.12f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        Renderer::setSunLight(Vec3(-0.4f, -0.8f, -0.4f), Vec3(1.0f, 0.95f, 0.90f), Vec3(0.22f, 0.25f, 0.32f));
        Renderer::beginFrame(_workspace.getDualViewport().getCinematicCamera());
        render3DScene();
        Renderer::endFrame();

        // --- B. SECONDARY VIEWPORT PASS (Work Camera with Frustum, Skeleton & Gizmo) ---
        const auto& secR = _workspace.getDualViewport().getSecondaryRect();
        glViewport(static_cast<GLint>(secR.x), static_cast<GLint>(winH - (secR.y + secR.h)),
                   static_cast<GLsizei>(secR.w), static_cast<GLsizei>(secR.h));
        glScissor(static_cast<GLint>(secR.x), static_cast<GLint>(winH - (secR.y + secR.h)),
                  static_cast<GLsizei>(secR.w), static_cast<GLsizei>(secR.h));

        glClearColor(0.10f, 0.11f, 0.14f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        Renderer::beginFrame(_workspace.getDualViewport().getWorkCamera());
        render3DScene();

        // Draw 3D Frustum of Cinematic Camera
        LFM::DualViewport::drawCameraFrustum(_workspace.getDualViewport().getCinematicCamera(), Vec3(0.95f, 0.85f, 0.20f));

        // Draw 3D Motion Path with yellow trajectory nodes
        LFM::DualViewport::drawMotionPath(_cameraMotionPath, Vec3(0.25f, 0.85f, 0.45f));

        // Draw 3D Bone Hierarchy & Manipulator Gizmo
        if (_botSkeleton && _botMesh) {
            float botScale = _botMesh->getBaseScale(1.85f);
            float yOffset = -_botMesh->getMinBounds().y * botScale;
            Mat4 botModel = Mat4::translate(_characterStagePos + Vec3(0.0f, yOffset, 0.0f)) *
                            Mat4::rotate(_characterFacingYaw * (3.14159265f / 180.0f), Vec3(0.0f, 1.0f, 0.0f)) *
                            Mat4::scale(Vec3(botScale, botScale, botScale));

            int selectedBone = _workspace.getAnimationSet().getSelectedBoneIndex();
            LFM::DualViewport::drawSkeleton(*_botSkeleton, _botAnimator.getGlobalTransforms(), botModel, selectedBone);
        }

        // Draw light sources wireframes if map is loaded
        if (_map) {
            for (const auto& l : _map->lights) {
                Renderer::drawCube(l.position, Vec3(0.25f, 0.25f, 0.25f), l.color, nullptr, false);
                Renderer::drawWireCube(l.position, Vec3(l.radius * 0.2f, l.radius * 0.2f, l.radius * 0.2f), l.color * 0.5f);
            }
        }

        Renderer::endFrame();

        // Disable scissor test for 2D UI overlay pass
        glDisable(GL_SCISSOR_TEST);
        glViewport(0, 0, winW, winH);

        // 2. 2D WORKSPACE & TIMELINE UI PASS
        Renderer::beginUI(winW, winH);

        // Draw Viewport Header Labels
        renderViewportHeaders(priR, secR);

        // Draw Complete LFM Workspace (Left Panel, Viewport Toolbars, Sequencer, Modals)
        _workspace.render(winW, winH);

        Renderer::endUI();

        // Save screenshot if headless verification requested
        if (_takeScreenshot) {
            saveFrameToBMP("output/lfm_workspace_verify.bmp", winW, winH);
            _takeScreenshot = false;
            stop();
        }
    }

private:
    std::unordered_map<std::string, std::unique_ptr<Texture>> _sceneTextures;
    std::unordered_map<std::string, std::unique_ptr<Mesh>> _propMeshes;

    Texture* getSceneTexture(const std::string& path) {
        if (path.empty()) return nullptr;
        auto it = _sceneTextures.find(path);
        if (it != _sceneTextures.end()) return it->second.get();

        std::string fullPath = path;
        if (!std::filesystem::exists(fullPath)) {
            if (std::filesystem::exists("assets/textures/" + path)) {
                fullPath = "assets/textures/" + path;
            } else if (std::filesystem::exists("assets/textures/" + std::filesystem::path(path).filename().string())) {
                fullPath = "assets/textures/" + std::filesystem::path(path).filename().string();
            } else if (std::filesystem::exists("../assets/textures/" + path)) {
                fullPath = "../assets/textures/" + path;
            }
        }
        if (std::filesystem::exists(fullPath)) {
            auto tex = std::make_unique<Texture>(fullPath);
            Texture* ptr = tex.get();
            _sceneTextures[path] = std::move(tex);
            return ptr;
        }
        return nullptr;
    }

    Mesh* getPropMesh(const std::string& path) {
        if (path.empty()) return nullptr;
        auto it = _propMeshes.find(path);
        if (it != _propMeshes.end()) return it->second.get();

        std::string fullPath = path;
        if (!std::filesystem::exists(fullPath)) {
            if (std::filesystem::exists("assets/models/" + path)) {
                fullPath = "assets/models/" + path;
            } else if (std::filesystem::exists("assets/models/" + std::filesystem::path(path).filename().string())) {
                fullPath = "assets/models/" + std::filesystem::path(path).filename().string();
            }
        }
        if (std::filesystem::exists(fullPath)) {
            Mesh* raw = Mesh::loadSTL(fullPath);
            if (raw) {
                _propMeshes[path] = std::unique_ptr<Mesh>(raw);
                return raw;
            }
        }
        return nullptr;
    }

    void render3DScene() {
        if (_map) {
            for (const auto& b : _map->brushes) {
                Texture* tex = getSceneTexture(b.texturePath);
                if (!tex) tex = getSceneTexture("concrete_wall.bmp");
                if (b.rotation.x != 0.0f || b.rotation.y != 0.0f || b.rotation.z != 0.0f) {
                    Renderer::drawCube(b.position, b.rotation, b.size, b.color, tex, true, b.uvScale, b.uvMode);
                } else {
                    Renderer::drawCube(b.position, b.size, b.color, tex, true, b.uvScale, b.uvMode);
                }
            }
            for (const auto& d : _map->doors) {
                Texture* tex = getSceneTexture("blast_door.bmp");
                Renderer::drawCube(d.position, d.size, d.color, tex, true);
            }
            for (const auto& p : _map->props) {
                Texture* tex = getSceneTexture(p.texturePath.empty() ? "metal_hull.bmp" : p.texturePath);
                Mesh* m = getPropMesh(p.modelPath);
                if (m) {
                    Renderer::drawMesh(*m, p.position, p.rotation, p.scale, p.color, tex);
                } else {
                    Renderer::drawCube(p.position, p.scale, p.color, tex, true);
                }
            }
        } else {
            // Default Industrial Corridor with authentic high-res textures
            Texture* floorTex = getSceneTexture("floor_tiles.bmp");
            Texture* wallTex = getSceneTexture("concrete_wall.bmp");
            Texture* dirtyWallTex = getSceneTexture("corridor_dirty_wall.bmp");
            Texture* hazardTex = getSceneTexture("hazard_stripes.bmp");
            Texture* crateTex1 = getSceneTexture("dev_orange.bmp");
            Texture* crateTex2 = getSceneTexture("server_panel.bmp");

            Renderer::drawCube(Vec3(0.0f, -0.5f, 0.0f), Vec3(36.0f, 1.0f, 36.0f), Vec3(1.0f, 1.0f, 1.0f), floorTex, true, Vec2(0.2f, 0.2f));
            Renderer::drawCube(Vec3(0.0f, 4.0f, -16.0f), Vec3(36.0f, 8.0f, 1.0f), Vec3(1.0f, 1.0f, 1.0f), wallTex, true, Vec2(0.2f, 0.2f));
            Renderer::drawCube(Vec3(-18.0f, 4.0f, 0.0f), Vec3(1.0f, 8.0f, 36.0f), Vec3(1.0f, 1.0f, 1.0f), dirtyWallTex, true, Vec2(0.2f, 0.2f));
            Renderer::drawCube(Vec3(18.0f, 4.0f, 0.0f), Vec3(1.0f, 8.0f, 36.0f), Vec3(1.0f, 1.0f, 1.0f), dirtyWallTex, true, Vec2(0.2f, 0.2f));

            Renderer::drawCube(Vec3(-6.0f, 3.5f, 0.0f), Vec3(1.4f, 7.0f, 1.4f), Vec3(1.0f, 1.0f, 1.0f), hazardTex, true, Vec2(0.5f, 0.5f));
            Renderer::drawCube(Vec3(6.0f, 3.5f, 0.0f), Vec3(1.4f, 7.0f, 1.4f), Vec3(1.0f, 1.0f, 1.0f), hazardTex, true, Vec2(0.5f, 0.5f));

            Renderer::drawCube(Vec3(-2.5f, 0.75f, -3.0f), Vec3(1.5f, 1.5f, 1.5f), Vec3(1.0f, 1.0f, 1.0f), crateTex1, true);
            Renderer::drawCube(Vec3(-2.0f, 2.0f, -3.0f), Vec3(1.0f, 1.0f, 1.0f), Vec3(1.0f, 1.0f, 1.0f), crateTex2, true);
            Renderer::drawCube(Vec3(3.5f, 0.6f, -1.5f), Vec3(2.0f, 1.2f, 1.2f), Vec3(1.0f, 1.0f, 1.0f), crateTex2, true);
        }

        // Render Animated Skinned Character (guard.glb)
        if (_botMesh && _botAnimator.getSkeleton()) {
            float botScale = _botMesh->getBaseScale(1.85f);
            float yOffset = -_botMesh->getMinBounds().y * botScale;
            Mat4 botModel = Mat4::translate(_characterStagePos + Vec3(0.0f, yOffset, 0.0f)) *
                            Mat4::rotate(_characterFacingYaw * (3.14159265f / 180.0f), Vec3(0.0f, 1.0f, 0.0f)) *
                            Mat4::scale(Vec3(botScale, botScale, botScale));

            Renderer::drawSkinnedMesh(*_botMesh, botModel, _botAnimator.getSkinMatrices(),
                                      Vec3(1.0f, 1.0f, 1.0f), _guardTexture.get(), true);
        }

        // Hanging Catenary Cable between pillars
        if (_cableMesh) {
            Renderer::drawMesh(*_cableMesh, Vec3(0.0f, 0.0f, 0.0f), Vec3(0.0f, 0.0f, 0.0f), Vec3(1.0f, 1.0f, 1.0f), Vec3(0.18f, 0.18f, 0.20f));
        }
    }

    void renderViewportHeaders(const LFM::ViewportRect& priR, const LFM::ViewportRect& secR) {
        float curT = _workspace.getSequencer().getCurrentTime();
        const LFM::FilmShot* activeShot = nullptr;
        for (const auto& s : _workspace.getSequencer().getShots()) {
            if (curT >= s.startTime && curT < (s.startTime + s.duration)) {
                activeShot = &s;
                break;
            }
        }

        // Primary Viewport Header
        float hdrH = 26.0f;
        LabTheme::drawBevel(priR.x, priR.y, priR.w, hdrH, false);
        std::string priTc = LFM::Sequencer::formatTimecode(curT);
        LabFont::drawText(priR.x + 10.0f, priR.y + 5.0f, "PRIMARY VIEWPORT  [" + priTc + "]", 1.45f, Vec3(1.0f, 0.85f, 0.22f));
        std::string shotTag = activeShot ? (activeShot->id + " | " + activeShot->name) : "shot001 | Cryo Incident";
        LabFont::drawText(priR.x + priR.w - 280.0f, priR.y + 5.0f, shotTag, 1.35f, Vec3(0.90f, 0.92f, 0.95f));

        // Secondary Viewport Header
        LabTheme::drawBevel(secR.x, secR.y, secR.w, hdrH, false);
        LabFont::drawText(secR.x + 10.0f, secR.y + 5.0f, "SECONDARY VIEWPORT (Work Camera)", 1.45f, Vec3(0.30f, 0.88f, 1.0f));
        LabFont::drawText(secR.x + secR.w - 220.0f, secR.y + 5.0f, "RMB+WASD: Freecam Orbit", 1.35f, Vec3(0.85f, 0.88f, 0.92f));
    }

    std::string _scenePath;
    std::unique_ptr<LabMap> _map;
    LFM::Workspace _workspace;
    std::shared_ptr<Skeleton> _botSkeleton;
    std::vector<AnimationClip> _botAnimations;
    std::unique_ptr<SkinnedMesh> _botMesh;
    Animator _botAnimator;
    std::unique_ptr<Texture> _guardTexture;
    Vec3 _characterStagePos{0.0f, 0.0f, -4.0f};
    float _characterFacingYaw = 0.0f;

    std::unique_ptr<Mesh> _cableMesh;
    std::vector<Vec3> _cameraMotionPath;
    float _prevSequencerTime = 0.0f;
    bool _takeScreenshot = false;
    bool _renderTestMode = false;
    std::string _renderTestOut = "output/lfm_cinematic.mp4";

    // Interactive video export session
    OffscreenFBO _offscreenFbo;
    FilmExportSession _exportSession;
    bool _isExportActive = false;
    int _exportW = 1920;
    int _exportH = 1080;
    int _exportFps = 60;
    int _exportCurFrame = 0;
    int _exportTotalFrames = 0;
    float _exportDuration = 60.0f;
};

int main(int argc, char** argv) {
    std::string sceneFile = "";
    bool screenshot = false;
    bool renderTest = false;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--screenshot") screenshot = true;
        else if (arg == "--render-test") renderTest = true;
        else if (arg.rfind("--", 0) != 0 && sceneFile.empty()) sceneFile = arg;
    }
    LfmApp app(sceneFile);
    if (screenshot) app.enableScreenshotMode();
    if (renderTest) app.enableRenderTest("output/lfm_cinematic.mp4", 1280, 720, 30, 90);
    app.run();
    return 0;
}
