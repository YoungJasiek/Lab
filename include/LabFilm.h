#pragma once

#include "LabMath.h"
#include <string>
#include <vector>
#include <memory>
#include <functional>

namespace Lab {

    struct CameraKey {
        float time = 0.0f;
        Vec3 position{0.0f, 1.8f, 0.0f};
        float pitch = 0.0f;
        float yaw = 0.0f;
        float fov = 75.0f;
    };

    struct ActorKey {
        std::string entityName;
        std::string animationClip; // e.g. "guard_idle", "walk", "terminal_interact"
        float startTime = 0.0f;
        float duration = 5.0f;
        bool loop = true;
    };

    struct AudioKey {
        std::string soundPath;
        float time = 0.0f;
        float volume = 1.0f;
    };

    struct EventKey {
        std::string targetEntity;
        std::string inputAction;
        float time = 0.0f;
    };

    class FilmSequence {
    public:
        std::string name = "demo_act";
        float duration = 120.0f; // 2 minutes default

        std::vector<CameraKey> cameraKeys;
        std::vector<ActorKey> actorKeys;
        std::vector<AudioKey> audioKeys;
        std::vector<EventKey> eventKeys;

        void addCameraKey(float time, const Vec3& pos, float pitch, float yaw, float fov = 75.0f);
        void addActorClip(const std::string& entity, const std::string& clip, float start, float len, bool loop = true);
        void addAudioCue(const std::string& sound, float time, float volume = 1.0f);
        void addEvent(const std::string& entity, const std::string& action, float time);

        // Samples interpolated camera parameters at arbitrary time t using Catmull-Rom splines
        void sampleCamera(float t, Vec3& outPos, float& outPitch, float& outYaw, float& outFov) const;

        bool saveToJson(const std::string& filepath) const;
        bool loadFromJson(const std::string& filepath);
    };

    class FilmRenderer {
    public:
        // Renders sequence to an MP4 video file using ffmpeg.exe pipe
        static bool exportVideo(const FilmSequence& sequence,
                                int width = 1920,
                                int height = 1080,
                                int fps = 60,
                                const std::string& outputPath = "output/demo_showcase.mp4",
                                const std::string& ffmpegBinary = "C:\\ffmpeg\\bin\\ffmpeg.exe",
                                std::function<void(int currentFrame, int totalFrames)> progressCallback = nullptr);
    };

    // Non-blocking frame-by-frame export session for interactive DCC viewports
    class FilmExportSession {
    public:
        FilmExportSession();
        ~FilmExportSession();

        FilmExportSession(const FilmExportSession&) = delete;
        FilmExportSession& operator=(const FilmExportSession&) = delete;

        bool begin(int width, int height, int fps, int totalFrames,
                   const std::string& outputPath,
                   const std::string& audioWavPath = "",
                   const std::string& ffmpegBinary = "C:\\ffmpeg\\bin\\ffmpeg.exe");
        bool writeFrameRGB(const uint8_t* rgbPixels);
        void finish();
        void cancel();

        bool isActive() const { return _active; }
        int getCurrentFrame() const { return _currentFrame; }
        int getTotalFrames() const { return _totalFrames; }
        float getProgress() const { return _totalFrames > 0 ? static_cast<float>(_currentFrame) / static_cast<float>(_totalFrames) : 0.0f; }
        const std::string& getOutputPath() const { return _outputPath; }

    private:
        FILE* _pipe = nullptr;
        bool _active = false;
        int _width = 0;
        int _height = 0;
        int _fps = 60;
        int _currentFrame = 0;
        int _totalFrames = 0;
        std::string _outputPath;
    };

} // namespace Lab
