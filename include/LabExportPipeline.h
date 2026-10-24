#pragma once
#include <string>
#include <vector>
#include <filesystem>
#include <cstdint>
#include <sstream>
#include "LabExport.h"
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
        std::string entrySceneURI{"assets/maps/default_box.labmap"};
        std::string dimension{"3D"};
        int width{1280};
        int height{720};
        bool fullscreen{false};
        std::string rulesPath{""};
        std::string hudPath{""};
        bool retroFX{false};
        bool retroScanlines{false};
        bool retroVignette{false};
        bool retroAberration{false};
        int retroPixelScale{1};
        int retroColorTheme{0};
    };

    struct ExportResult {
        bool success{false};
        std::filesystem::path exportedExecutable;
        std::filesystem::path outputDirectory;
        size_t copiedFiles{0};
        uint64_t totalBytes{0};
        std::string logMessage;
    };

    class LAB_CORE_API ExportPipeline {
    public:
        static std::string platformToString(TargetPlatform p);
        static std::string configToString(BuildConfiguration c);
        static bool validateSettings(const ExportSettings& settings, std::string& outError);
        static std::string generateBuildManifest(const ExportSettings& settings, size_t assetCount, uint64_t totalBytes);
        static ExportResult exportProject(const ExportSettings& settings);
        static ExportResult buildAndPrepareTestGame(const std::string& gameTitle, const std::string& scenePath, const std::filesystem::path& customTestDir = "test_build/debug", const ExportSettings* optionalSettings = nullptr);
        static bool launchGame(const std::filesystem::path& executablePath, const std::string& args = "");

        // Production Script Protection
        static bool compileLuaToBytecode(const std::filesystem::path& srcPath, const std::filesystem::path& dstBytecodePath, bool stripDebug = true);
        static size_t compileAllScriptsInDirectory(const std::filesystem::path& scriptsDir, const std::filesystem::path& outputDir, bool stripDebug = true);
    };

} // namespace Lab
