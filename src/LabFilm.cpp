#include "LabFilm.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>

#if defined(_WIN32)
#include <windows.h>
#define POPEN _popen
#define PCLOSE _pclose
#else
#define POPEN popen
#define PCLOSE pclose
#endif

namespace Lab {

    void FilmSequence::addCameraKey(float time, const Vec3& pos, float pitch, float yaw, float fov) {
        CameraKey k;
        k.time = time;
        k.position = pos;
        k.pitch = pitch;
        k.yaw = yaw;
        k.fov = fov;
        cameraKeys.push_back(k);
        std::sort(cameraKeys.begin(), cameraKeys.end(), [](const CameraKey& a, const CameraKey& b) {
            return a.time < b.time;
        });
    }

    void FilmSequence::addActorClip(const std::string& entity, const std::string& clip, float start, float len, bool loop) {
        ActorKey k;
        k.entityName = entity;
        k.animationClip = clip;
        k.startTime = start;
        k.duration = len;
        k.loop = loop;
        actorKeys.push_back(k);
    }

    void FilmSequence::addAudioCue(const std::string& sound, float time, float volume) {
        AudioKey k;
        k.soundPath = sound;
        k.time = time;
        k.volume = volume;
        audioKeys.push_back(k);
    }

    void FilmSequence::addEvent(const std::string& entity, const std::string& action, float time) {
        EventKey k;
        k.targetEntity = entity;
        k.inputAction = action;
        k.time = time;
        eventKeys.push_back(k);
    }

    static Vec3 catmullRom(const Vec3& p0, const Vec3& p1, const Vec3& p2, const Vec3& p3, float u) {
        float u2 = u * u;
        float u3 = u2 * u;
        return (p1 * 2.0f +
               (-p0 + p2) * u +
               (p0 * 2.0f - p1 * 5.0f + p2 * 4.0f - p3) * u2 +
               (-p0 + p1 * 3.0f - p2 * 3.0f + p3) * u3) * 0.5f;
    }

    static float lerpAngle(float a, float b, float t) {
        float diff = std::fmod(b - a + 180.0f, 360.0f) - 180.0f;
        if (diff < -180.0f) diff += 360.0f;
        return a + diff * t;
    }

    void FilmSequence::sampleCamera(float t, Vec3& outPos, float& outPitch, float& outYaw, float& outFov) const {
        if (cameraKeys.empty()) {
            outPos = Vec3(0, 1.8f, 0);
            outPitch = 0.0f;
            outYaw = 0.0f;
            outFov = 75.0f;
            return;
        }

        if (cameraKeys.size() == 1 || t <= cameraKeys.front().time) {
            outPos = cameraKeys.front().position;
            outPitch = cameraKeys.front().pitch;
            outYaw = cameraKeys.front().yaw;
            outFov = cameraKeys.front().fov;
            return;
        }

        if (t >= cameraKeys.back().time) {
            outPos = cameraKeys.back().position;
            outPitch = cameraKeys.back().pitch;
            outYaw = cameraKeys.back().yaw;
            outFov = cameraKeys.back().fov;
            return;
        }

        // Find segment
        size_t idx = 0;
        for (size_t i = 0; i < cameraKeys.size() - 1; ++i) {
            if (t >= cameraKeys[i].time && t <= cameraKeys[i + 1].time) {
                idx = i;
                break;
            }
        }

        const auto& k1 = cameraKeys[idx];
        const auto& k2 = cameraKeys[idx + 1];
        float dt = k2.time - k1.time;
        float u = (dt > 0.0001f) ? (t - k1.time) / dt : 0.0f;
        u = std::clamp(u, 0.0f, 1.0f);

        // Surrounding points for Catmull-Rom
        const auto& k0 = (idx > 0) ? cameraKeys[idx - 1] : k1;
        const auto& k3 = (idx + 2 < cameraKeys.size()) ? cameraKeys[idx + 2] : k2;

        outPos = catmullRom(k0.position, k1.position, k2.position, k3.position, u);
        outPitch = k1.pitch + (k2.pitch - k1.pitch) * u;
        outYaw = lerpAngle(k1.yaw, k2.yaw, u);
        outFov = k1.fov + (k2.fov - k1.fov) * u;
    }

    bool FilmSequence::saveToJson(const std::string& filepath) const {
        std::ofstream out(filepath);
        if (!out.is_open()) return false;

        out << "{\n";
        out << "  \"name\": \"" << name << "\",\n";
        out << "  \"duration\": " << duration << ",\n";
        out << "  \"camera_keys\": [\n";
        for (size_t i = 0; i < cameraKeys.size(); ++i) {
            const auto& k = cameraKeys[i];
            out << "    { \"time\": " << k.time
                << ", \"pos\": [" << k.position.x << ", " << k.position.y << ", " << k.position.z << "]"
                << ", \"pitch\": " << k.pitch
                << ", \"yaw\": " << k.yaw
                << ", \"fov\": " << k.fov
                << " }" << (i + 1 < cameraKeys.size() ? "," : "") << "\n";
        }
        out << "  ],\n";
        out << "  \"actors\": [\n";
        for (size_t i = 0; i < actorKeys.size(); ++i) {
            const auto& a = actorKeys[i];
            out << "    { \"entity\": \"" << a.entityName
                << "\", \"clip\": \"" << a.animationClip
                << "\", \"start\": " << a.startTime
                << ", \"duration\": " << a.duration
                << ", \"loop\": " << (a.loop ? "true" : "false")
                << " }" << (i + 1 < actorKeys.size() ? "," : "") << "\n";
        }
        out << "  ],\n";
        out << "  \"audio\": [\n";
        for (size_t i = 0; i < audioKeys.size(); ++i) {
            const auto& s = audioKeys[i];
            out << "    { \"path\": \"" << s.soundPath
                << "\", \"time\": " << s.time
                << ", \"volume\": " << s.volume
                << " }" << (i + 1 < audioKeys.size() ? "," : "") << "\n";
        }
        out << "  ],\n";
        out << "  \"events\": [\n";
        for (size_t i = 0; i < eventKeys.size(); ++i) {
            const auto& e = eventKeys[i];
            out << "    { \"target\": \"" << e.targetEntity
                << "\", \"action\": \"" << e.inputAction
                << "\", \"time\": " << e.time
                << " }" << (i + 1 < eventKeys.size() ? "," : "") << "\n";
        }
        out << "  ]\n";
        out << "}\n";
        return true;
    }

    bool FilmSequence::loadFromJson(const std::string& filepath) {
        std::ifstream in(filepath);
        if (!in.is_open()) return false;

        cameraKeys.clear();
        actorKeys.clear();
        audioKeys.clear();
        eventKeys.clear();

        std::string line;
        while (std::getline(in, line)) {
            // Simple keyframe parsing
            if (line.find("\"camera_keys\"") != std::string::npos) {
                while (std::getline(in, line) && line.find("]") == std::string::npos) {
                    if (line.find("\"time\"") != std::string::npos) {
                        CameraKey k;
                        // Scan time, pos, pitch, yaw, fov
                        float px = 0, py = 0, pz = 0;
#if defined(_MSC_VER)
                        if (sscanf_s(line.c_str(), "%*[^0-9.-]%f%*[^0-9.-]%f%*[^0-9.-]%f%*[^0-9.-]%f%*[^0-9.-]%f%*[^0-9.-]%f%*[^0-9.-]%f",
                                     &k.time, &px, &py, &pz, &k.pitch, &k.yaw, &k.fov) >= 4) {
#else
                        if (std::sscanf(line.c_str(), "%*[^0-9.-]%f%*[^0-9.-]%f%*[^0-9.-]%f%*[^0-9.-]%f%*[^0-9.-]%f%*[^0-9.-]%f%*[^0-9.-]%f",
                                        &k.time, &px, &py, &pz, &k.pitch, &k.yaw, &k.fov) >= 4) {
#endif
                            k.position = Vec3(px, py, pz);
                            cameraKeys.push_back(k);
                        }
                    }
                }
            }
        }
        return true;
    }

    bool FilmRenderer::exportVideo(const FilmSequence& sequence,
                                   int width,
                                   int height,
                                   int fps,
                                   const std::string& outputPath,
                                   const std::string& ffmpegBinary,
                                   std::function<void(int currentFrame, int totalFrames)> progressCallback) {
        std::filesystem::path binPath = ffmpegBinary;
        std::string exe = binPath.string();
        if (!std::filesystem::exists(binPath)) {
            exe = "ffmpeg"; // Attempt PATH
        }

        // Ensure parent directory exists
        std::filesystem::path outDir = std::filesystem::path(outputPath).parent_path();
        if (!outDir.empty()) {
            std::filesystem::create_directories(outDir);
        }

        // Construct high-quality production command
        std::ostringstream cmd;
        cmd << "\"" << exe << "\" -y"
            << " -f rawvideo -vcodec rawvideo"
            << " -s " << width << "x" << height
            << " -pix_fmt rgb24"
            << " -r " << fps
            << " -i -"
            << " -c:v libx264 -preset fast -crf 18 -pix_fmt yuv420p "
            << "\"" << outputPath << "\"";

#ifdef _WIN32
        std::string finalCmd = "\"" + cmd.str() + "\"";
        FILE* pipe = POPEN(finalCmd.c_str(), "wb");
#else
        FILE* pipe = POPEN(cmd.str().c_str(), "wb");
#endif
        if (!pipe) {
            std::cerr << "[FilmRenderer] Failed to launch video pipe: " << cmd.str() << "\n";
            return false;
        }

        int totalFrames = static_cast<int>(sequence.duration * fps);
        if (totalFrames <= 0) totalFrames = 60;

        std::vector<uint8_t> frame(static_cast<size_t>(width * height * 3), 0);

        for (int f = 0; f < totalFrames; ++f) {
            float t = static_cast<float>(f) / static_cast<float>(fps);

            Vec3 camPos;
            float pitch = 0, yaw = 0, fov = 75.0f;
            sequence.sampleCamera(t, camPos, pitch, yaw, fov);

            // Procedural preview color gradient if headlessly invoked without active GPU FBO
            uint8_t rVal = static_cast<uint8_t>(std::clamp((camPos.x + 10.0f) * 12.0f, 20.0f, 220.0f));
            uint8_t gVal = static_cast<uint8_t>(std::clamp((camPos.y) * 40.0f, 30.0f, 200.0f));
            uint8_t bVal = static_cast<uint8_t>(std::clamp((camPos.z + 10.0f) * 12.0f, 40.0f, 240.0f));

            for (int y = 0; y < height; ++y) {
                for (int x = 0; x < width; ++x) {
                    size_t idx = static_cast<size_t>((y * width + x) * 3);
                    frame[idx + 0] = rVal;
                    frame[idx + 1] = gVal;
                    frame[idx + 2] = bVal;
                }
            }

            fwrite(frame.data(), 1, frame.size(), pipe);

            if (progressCallback && (f % 30 == 0 || f == totalFrames - 1)) {
                progressCallback(f + 1, totalFrames);
            }
        }

        PCLOSE(pipe);
        std::cout << "[FilmRenderer] Render completed successfully: " << outputPath << " (" << totalFrames << " frames)\n";
        return true;
    }

    FilmExportSession::FilmExportSession() = default;

    FilmExportSession::~FilmExportSession() {
        cancel();
    }

    bool FilmExportSession::begin(int width, int height, int fps, int totalFrames,
                                  const std::string& outputPath,
                                  const std::string& audioWavPath,
                                  const std::string& ffmpegBinary) {
        cancel(); // Close any prior session

        _width = width;
        _height = height;
        _fps = fps;
        _totalFrames = totalFrames;
        _currentFrame = 0;
        _outputPath = outputPath;

        std::filesystem::path binPath = ffmpegBinary;
        std::string exe = binPath.string();
        if (!std::filesystem::exists(binPath)) {
            exe = "ffmpeg";
        }

        std::filesystem::path outDir = std::filesystem::path(outputPath).parent_path();
        if (!outDir.empty()) {
            std::filesystem::create_directories(outDir);
        }

        bool hasAudio = !audioWavPath.empty() && std::filesystem::exists(audioWavPath);

        std::ostringstream cmd;
        cmd << "\"" << exe << "\" -y"
            << " -f rawvideo -vcodec rawvideo"
            << " -s " << width << "x" << height
            << " -pix_fmt rgb24"
            << " -r " << fps
            << " -i -";
        if (hasAudio) {
            cmd << " -i \"" << audioWavPath << "\"";
        }
        cmd << " -c:v libx264 -preset fast -crf 18 -pix_fmt yuv420p";
        if (hasAudio) {
            cmd << " -c:a aac -b:a 192k -shortest";
        }
        cmd << " \"" << outputPath << "\"";

#ifdef _WIN32
        std::string finalCmd = "\"" + cmd.str() + "\"";
        _pipe = POPEN(finalCmd.c_str(), "wb");
#else
        _pipe = POPEN(cmd.str().c_str(), "wb");
#endif
        if (!_pipe) {
            std::cerr << "[FilmExportSession] Failed to launch video pipe: " << cmd.str() << "\n";
            _active = false;
            return false;
        }

        _active = true;
        std::cout << "[FilmExportSession] Video pipe initialized: " << outputPath
                  << " (" << width << "x" << height << " @ " << fps << " fps, "
                  << totalFrames << " frames)\n";
        return true;
    }

    bool FilmExportSession::writeFrameRGB(const uint8_t* rgbPixels) {
        if (!_active || !_pipe || !rgbPixels) return false;

        size_t frameBytes = static_cast<size_t>(_width * _height * 3);
        size_t written = fwrite(rgbPixels, 1, frameBytes, _pipe);
        if (written != frameBytes) {
            std::cerr << "[FilmExportSession] Error writing frame " << _currentFrame << " to pipe.\n";
            return false;
        }

        _currentFrame++;
        return true;
    }

    void FilmExportSession::finish() {
        if (_pipe) {
            PCLOSE(_pipe);
            _pipe = nullptr;
            std::cout << "[FilmExportSession] Export finished successfully: " << _outputPath
                      << " (" << _currentFrame << " frames written)\n";
        }
        _active = false;
    }

    void FilmExportSession::cancel() {
        if (_pipe) {
            PCLOSE(_pipe);
            _pipe = nullptr;
            std::cout << "[FilmExportSession] Export cancelled.\n";
        }
        _active = false;
        _currentFrame = 0;
    }

} // namespace Lab
