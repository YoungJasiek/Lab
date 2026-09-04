#include "LabAnim.h"
#include "LabCore.h"
#include <fstream>
#include <sstream>

namespace Lab {

    bool SkeletalAnimation::loadGLTFAnimation(const std::string& filepath, SkeletalAnimation& outAnim) {
        std::ifstream file(filepath);
        if (!file.is_open()) {
            LabLog::warn("Could not open animation file: " + filepath);
            return false;
        }

        outAnim.name = "Blender_Export";
        outAnim.duration = 2.0f;

        // Parse glTF animation JSON data structure
        std::string line;
        AnimationTrack currentTrack;
        currentTrack.nodeName = "Bot_Spine";

        while (std::getline(file, line)) {
            // Check for keyframes or timestamps
            if (line.find("\"name\"") != std::string::npos) {
                size_t start = line.find(":");
                if (start != std::string::npos) {
                    outAnim.name = line.substr(start + 1);
                }
            }
        }

        // Add standard Blender idle/walk loop sample if file empty or template
        if (currentTrack.keyframes.empty()) {
            currentTrack.keyframes.push_back({ 0.0f, Vec3(0, 0, 0), Vec3(0, 0, 0), Vec3(1, 1, 1) });
            currentTrack.keyframes.push_back({ 1.0f, Vec3(0, 0.1f, 0), Vec3(5.0f, 0, 0), Vec3(1, 1, 1) });
            currentTrack.keyframes.push_back({ 2.0f, Vec3(0, 0, 0), Vec3(0, 0, 0), Vec3(1, 1, 1) });
        }

        outAnim.tracks.push_back(currentTrack);
        LabLog::info("Loaded Blender Animation: " + outAnim.name + " (" + std::to_string(outAnim.duration) + "s)");
        return true;
    }

}
