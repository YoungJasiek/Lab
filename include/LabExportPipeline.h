#pragma once
#include <string>
#include <vector>
#include <filesystem>
#include <cstdint>
#include <sstream>
#include "LabAsset.h"

namespace Lab {

    enum class TargetPlatform {
        Windows_x64,
        Linux_x64,
        WebAssembly,
        macOS_ARM64
    };

    enum class BuildConfiguration {
        Debug,
        Release,
        Shipping
    };

    struct ExportSettings {
        TargetPlatform platform{TargetPlatform::Windows_x64};
        BuildConfiguration config{BuildConfiguration::Shipping};
        std::string gameTitle{"LabGame"};
        std::filesystem::path outputDirectory{"build/export"};
        bool embedAssets{true};
        bool compressAssets{true};
        std::string entrySceneURI{"assets://maps/cryo_complex.labmap"};
    };

    class ExportPipeline {
    public:
        static std::string platformToString(TargetPlatform p) {
            switch (p) {
                case TargetPlatform::Windows_x64: return "Windows_x64";
                case TargetPlatform::Linux_x64: return "Linux_x64";
                case TargetPlatform::WebAssembly: return "WebAssembly";
                case TargetPlatform::macOS_ARM64: return "macOS_ARM64";
            }
            return "Unknown";
        }

        static std::string configToString(BuildConfiguration c) {
            switch (c) {
                case BuildConfiguration::Debug: return "Debug";
                case BuildConfiguration::Release: return "Release";
                case BuildConfiguration::Shipping: return "Shipping";
            }
            return "Unknown";
        }

        static bool validateSettings(const ExportSettings& settings, std::string& outError) {
            if (settings.gameTitle.empty()) {
                outError = "Game title cannot be empty";
                return false;
            }
            if (settings.outputDirectory.empty()) {
                outError = "Output directory cannot be empty";
                return false;
            }
            if (settings.entrySceneURI.empty()) {
                outError = "Entry scene URI cannot be empty";
                return false;
            }
            return true;
        }

        static std::string generateBuildManifest(const ExportSettings& settings, size_t assetCount, uint64_t totalBytes) {
            std::stringstream ss;
            ss << "{\n"
               << "  \"title\": \"" << settings.gameTitle << "\",\n"
               << "  \"platform\": \"" << platformToString(settings.platform) << "\",\n"
               << "  \"configuration\": \"" << configToString(settings.config) << "\",\n"
               << "  \"entryScene\": \"" << settings.entrySceneURI << "\",\n"
               << "  \"embedAssets\": " << (settings.embedAssets ? "true" : "false") << ",\n"
               << "  \"compressAssets\": " << (settings.compressAssets ? "true" : "false") << ",\n"
               << "  \"assetCount\": " << assetCount << ",\n"
               << "  \"totalBytes\": " << totalBytes << "\n"
               << "}";
            return ss.str();
        }
    };

} // namespace Lab
