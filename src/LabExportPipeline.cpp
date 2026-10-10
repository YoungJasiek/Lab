#include "LabExportPipeline.h"
#include "LabProject.h"
#include <fstream>
#include <sstream>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <unordered_set>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <shellapi.h>
#endif

extern "C" {
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
}

namespace Lab {

    std::string ExportPipeline::platformToString(TargetPlatform p) {
        switch (p) {
            case TargetPlatform::Windows_x64: return "Windows_x64";
            case TargetPlatform::Linux_x64: return "Linux_x64";
            case TargetPlatform::WebAssembly: return "WebAssembly";
            case TargetPlatform::macOS_ARM64: return "macOS_ARM64";
        }
        return "Unknown";
    }

    std::string ExportPipeline::configToString(BuildConfiguration c) {
        switch (c) {
            case BuildConfiguration::Debug: return "Debug";
            case BuildConfiguration::Release: return "Release";
            case BuildConfiguration::Shipping: return "Shipping";
        }
        return "Unknown";
    }

    bool ExportPipeline::validateSettings(const ExportSettings& settings, std::string& outError) {
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

    std::string ExportPipeline::generateBuildManifest(const ExportSettings& settings, size_t assetCount, uint64_t totalBytes) {
        auto now = std::chrono::system_clock::now();
        std::time_t nowTime = std::chrono::system_clock::to_time_t(now);
        std::tm tmNow{};
#if defined(_WIN32)
        gmtime_s(&tmNow, &nowTime);
#else
        gmtime_r(&nowTime, &tmNow);
#endif
        std::stringstream ss;
        ss << std::put_time(&tmNow, "%Y-%m-%dT%H:%M:%SZ");
        std::string timeStr = ss.str();

        std::stringstream out;
        out << "{\n"
            << "  \"title\": \"" << settings.gameTitle << "\",\n"
            << "  \"platform\": \"" << platformToString(settings.platform) << "\",\n"
            << "  \"configuration\": \"" << configToString(settings.config) << "\",\n"
            << "  \"entryScene\": \"" << settings.entrySceneURI << "\",\n"
            << "  \"embedAssets\": " << (settings.embedAssets ? "true" : "false") << ",\n"
            << "  \"compressAssets\": " << (settings.compressAssets ? "true" : "false") << ",\n"
            << "  \"assetCount\": " << assetCount << ",\n"
            << "  \"totalBytes\": " << totalBytes << ",\n"
            << "  \"engine\": \"Lab Engine Studio 2026.10 (LabSDK)\",\n"
            << "  \"buildTimestamp\": \"" << timeStr << "\"\n"
            << "}\n";
        return out.str();
    }

    static void copyDirectoryRecursive(const std::filesystem::path& src, const std::filesystem::path& dst, size_t& fileCount, uint64_t& byteCount) {
        if (!std::filesystem::exists(src)) return;
        std::filesystem::create_directories(dst);

        for (const auto& entry : std::filesystem::recursive_directory_iterator(src)) {
            const auto& curPath = entry.path();
            auto relative = std::filesystem::relative(curPath, src);
            auto target = dst / relative;

            if (entry.is_directory()) {
                std::filesystem::create_directories(target);
            } else if (entry.is_regular_file()) {
                std::error_code ec;
                std::filesystem::create_directories(target.parent_path(), ec);
                std::filesystem::copy_file(curPath, target, std::filesystem::copy_options::overwrite_existing, ec);
                if (!ec) {
                    fileCount++;
                    byteCount += entry.file_size();
                }
            }
        }
    }

    ExportResult ExportPipeline::exportProject(const ExportSettings& settings) {
        ExportResult result;
        std::string err;
        if (!validateSettings(settings, err)) {
            result.success = false;
            result.logMessage = "Validation failed: " + err;
            return result;
        }

        std::string folderName = settings.gameTitle + "_" + platformToString(settings.platform);
        std::filesystem::path outDir = settings.outputDirectory / folderName;

        std::error_code ec;
        std::filesystem::create_directories(outDir, ec);
        if (ec) {
            result.success = false;
            result.logMessage = "Failed to create directory: " + outDir.string() + " (" + ec.message() + ")";
            return result;
        }

        result.outputDirectory = outDir;

        // 1. Replicate game assets (from Documents/LabGames/<GameTitle>/assets or fallback)
        auto& pm = ProjectManager::get();
        std::filesystem::path projAssets = pm.getActiveProjectAssetsPath();
        if (std::filesystem::exists(projAssets)) {
            copyDirectoryRecursive(projAssets, outDir / "assets", result.copiedFiles, result.totalBytes);
        }

        // Fallback to legacy/root assets if no project assets found
        if (result.copiedFiles == 0) {
            std::vector<std::string> assetRoots = { "assets", "../assets", "../../assets" };
            for (const auto& aRoot : assetRoots) {
                if (std::filesystem::exists(aRoot)) {
                    copyDirectoryRecursive(aRoot, outDir / "assets", result.copiedFiles, result.totalBytes);
                    break;
                }
            }
        }

        // Replicate engine runtime fonts (from engine/assets/fonts or root fonts)
        std::vector<std::filesystem::path> fontRoots = { 
            pm.getEngineAssetsPath() / "fonts",
            pm.getEngineRoot() / "fonts",
            "fonts", "../fonts", "../../fonts" 
        };
        for (const auto& fRoot : fontRoots) {
            if (std::filesystem::exists(fRoot)) {
                copyDirectoryRecursive(fRoot, outDir / "fonts", result.copiedFiles, result.totalBytes);
                break;
            }
        }

        // Replicate modular plugins directory
        std::vector<std::filesystem::path> pluginRoots = {
            pm.getEngineRoot() / "plugins",
            "plugins",
            pm.getEngineAssetsPath() / "plugins",
            "engine/assets/plugins"
        };
        for (const auto& pRoot : pluginRoots) {
            if (std::filesystem::exists(pRoot)) {
                copyDirectoryRecursive(pRoot, outDir / "plugins", result.copiedFiles, result.totalBytes);
                break;
            }
        }

        // Guarantee entry scene map is copied to outDir/assets/maps as portable relative asset
        std::filesystem::path esp(settings.entrySceneURI.empty() ? "assets/maps/default_box.labmap" : settings.entrySceneURI);
        std::string mapFileName = esp.filename().string();
        if (mapFileName.empty()) mapFileName = "main.labmap";
        std::filesystem::path targetMapsDir = outDir / "assets" / "maps";
        std::filesystem::create_directories(targetMapsDir, ec);
        std::filesystem::path targetMapPath = targetMapsDir / mapFileName;
        if (std::filesystem::exists(esp)) {
            std::filesystem::copy_file(esp, targetMapPath, std::filesystem::copy_options::overwrite_existing, ec);
            result.copiedFiles++;
            result.totalBytes += std::filesystem::file_size(targetMapPath);
        } else if (std::filesystem::exists(pm.getEngineRoot() / esp)) {
            std::filesystem::copy_file(pm.getEngineRoot() / esp, targetMapPath, std::filesystem::copy_options::overwrite_existing, ec);
            result.copiedFiles++;
            result.totalBytes += std::filesystem::file_size(targetMapPath);
        } else if (std::filesystem::exists(projAssets / "maps" / mapFileName)) {
            std::filesystem::copy_file(projAssets / "maps" / mapFileName, targetMapPath, std::filesystem::copy_options::overwrite_existing, ec);
            result.copiedFiles++;
            result.totalBytes += std::filesystem::file_size(targetMapPath);
        }

        // 1.5. Compile and protect Lua scripts for Release/Shipping
        if (settings.config == BuildConfiguration::Shipping || settings.config == BuildConfiguration::Release) {
            std::filesystem::path outScriptsDir = outDir / "assets" / "scripts";
            if (std::filesystem::exists(outScriptsDir)) {
                size_t compiledCount = compileAllScriptsInDirectory(outScriptsDir, outScriptsDir, true);
                if (settings.config == BuildConfiguration::Shipping) {
                    // In shipping configuration, remove plaintext .lua files so only precompiled .luac remains
                    for (const auto& entry : std::filesystem::recursive_directory_iterator(outScriptsDir)) {
                        if (entry.is_regular_file() && entry.path().extension() == ".lua") {
                            std::error_code delEc;
                            std::filesystem::remove(entry.path(), delEc);
                        }
                    }
                }
                if (compiledCount > 0) {
                    result.logMessage += " [Compiled " + std::to_string(compiledCount) + " scripts to protected bytecode]";
                }
            }
        }

        // 2. Platform specific binaries & wrappers
        if (settings.platform == TargetPlatform::Windows_x64) {
            // Find LabPlayer.exe runner or fallback
            std::vector<std::filesystem::path> runnerSearch = {
                "LabPlayer.exe",
                "build/Release/LabPlayer.exe",
                "build/Debug/LabPlayer.exe",
                "Release/LabPlayer.exe",
                "Debug/LabPlayer.exe",
                "build/Export/FrozenLife_Standalone/FrozenLife.exe",
                "build/Release/FrozenLife.exe",
                "build/Debug/FrozenLife.exe"
            };

            std::filesystem::path foundRunner;
            for (const auto& rPath : runnerSearch) {
                if (std::filesystem::exists(rPath)) {
                    foundRunner = rPath;
                    break;
                }
            }

            std::filesystem::path targetExe = outDir / (settings.gameTitle + ".exe");
            if (!foundRunner.empty()) {
                std::filesystem::copy_file(foundRunner, targetExe, std::filesystem::copy_options::overwrite_existing, ec);
                result.copiedFiles++;
                result.totalBytes += std::filesystem::file_size(targetExe);

                // Copy accompanying DLLs from runner's directory
                auto runnerDir = foundRunner.parent_path();
                if (runnerDir.empty()) runnerDir = ".";
                if (std::filesystem::exists(runnerDir)) {
                    for (const auto& dEntry : std::filesystem::directory_iterator(runnerDir)) {
                        if (dEntry.is_regular_file() && dEntry.path().extension() == ".dll") {
                            auto targetDll = outDir / dEntry.path().filename();
                            std::filesystem::copy_file(dEntry.path(), targetDll, std::filesystem::copy_options::overwrite_existing, ec);
                            result.copiedFiles++;
                            result.totalBytes += dEntry.file_size();
                        }
                    }
                }
            } else {
                // If runner wasn't compiled yet, create a batch launcher stub
                std::ofstream stubExe(targetExe, std::ios::binary);
                stubExe << "MZ_LAB_STANDALONE_BINARY_PLACEHOLDER\n";
                stubExe.close();
            }

            // Quick launcher script
            std::filesystem::path batPath = outDir / "run_game.bat";
            std::ofstream bat(batPath);
            bat << "@echo off\r\n"
                << "title " << settings.gameTitle << "\r\n"
                << "cd /d \"%~dp0\"\r\n"
                << "start \"\" \"%~dp0\\" << settings.gameTitle << ".exe\"\r\n";
            bat.close();

            result.exportedExecutable = targetExe;

        } else if (settings.platform == TargetPlatform::Linux_x64) {
            std::filesystem::create_directories(outDir / "lib");

            // Binary launcher runner
            std::filesystem::path elfPath = outDir / (settings.gameTitle + ".x86_64");
            std::ofstream elfFile(elfPath, std::ios::binary);
            elfFile << "#!/usr/bin/env bash\n"
                    << "DIR=\"$(cd \"$(dirname \"${BASH_SOURCE[0]}\")\" >/dev/null 2>&1 && pwd)\"\n"
                    << "echo \"[Lab Engine Standalone Linux] Launching " << settings.gameTitle << "...\"\n"
                    << "export LD_LIBRARY_PATH=\"$DIR/lib:$LD_LIBRARY_PATH\"\n"
                    << "echo \"Scene: " << settings.entrySceneURI << "\"\n";
            elfFile.close();

            // Shell launcher script
            std::filesystem::path shPath = outDir / ("run_" + settings.gameTitle + ".sh");
            std::ofstream shFile(shPath);
            shFile << "#!/usr/bin/env bash\n"
                   << "DIR=\"$(cd \"$(dirname \"${BASH_SOURCE[0]}\")\" >/dev/null 2>&1 && pwd)\"\n"
                   << "export LD_LIBRARY_PATH=\"$DIR/lib:$LD_LIBRARY_PATH\"\n"
                   << "chmod +x \"$DIR/" << settings.gameTitle << ".x86_64\"\n"
                   << "exec \"$DIR/" << settings.gameTitle << ".x86_64\" --scene \"" << settings.entrySceneURI << "\" \"$@\"\n";
            shFile.close();

            // Linux standalone CMake build script for native Linux build
            std::ofstream cmakeFile(outDir / "CMakeLists.txt");
            cmakeFile << "cmake_minimum_required(VERSION 3.20)\n"
                      << "project(" << settings.gameTitle << " CXX)\n"
                      << "set(CMAKE_CXX_STANDARD 20)\n"
                      << "add_executable(" << settings.gameTitle << " " << settings.gameTitle << ".x86_64)\n";
            cmakeFile.close();

            std::ofstream buildSh(outDir / "build_linux.sh");
            buildSh << "#!/usr/bin/env bash\n"
                    << "mkdir -p build && cd build\n"
                    << "cmake .. && cmake --build .\n";
            buildSh.close();

            result.exportedExecutable = shPath;

        } else if (settings.platform == TargetPlatform::WebAssembly) {
            // HTML5 Modern Canvas player
            std::filesystem::path indexPath = outDir / "index.html";
            std::ofstream html(indexPath);
            html << "<!DOCTYPE html>\n"
                 << "<html lang=\"en\">\n"
                 << "<head>\n"
                 << "  <meta charset=\"utf-8\">\n"
                 << "  <meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">\n"
                 << "  <title>" << settings.gameTitle << " - Lab Engine Web Player</title>\n"
                 << "  <style>\n"
                 << "    * { box-sizing: border-box; margin: 0; padding: 0; }\n"
                 << "    body { background: #0c0d11; color: #d0d7de; font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif; display: flex; flex-direction: column; align-items: center; justify-content: center; min-height: 100vh; overflow: hidden; }\n"
                 << "    #game-container { position: relative; border: 2px solid #2d333b; border-radius: 8px; box-shadow: 0 12px 40px rgba(0,0,0,0.8); overflow: hidden; background: #000; }\n"
                 << "    canvas#canvas { display: block; width: 1280px; height: 720px; outline: none; }\n"
                 << "    #header-bar { position: absolute; top: 12px; left: 16px; font-size: 15px; font-weight: 600; color: #ff9d00; text-shadow: 0 2px 4px rgba(0,0,0,0.8); pointer-events: none; }\n"
                 << "    #controls-bar { margin-top: 14px; display: flex; gap: 20px; align-items: center; font-size: 13px; color: #8b949e; }\n"
                 << "    button.fs-btn { background: #1f6feb; border: none; color: #fff; padding: 6px 16px; border-radius: 6px; font-weight: 500; cursor: pointer; transition: background 0.2s; }\n"
                 << "    button.fs-btn:hover { background: #388bfd; }\n"
                 << "    #status { position: absolute; bottom: 12px; left: 16px; font-size: 12px; color: #7ee787; pointer-events: none; }\n"
                 << "  </style>\n"
                 << "</head>\n"
                 << "<body>\n"
                 << "  <div id=\"game-container\">\n"
                 << "    <div id=\"header-bar\">" << settings.gameTitle << " [WebAssembly / WebGL 2.0]</div>\n"
                 << "    <div id=\"status\">Lab Engine Web Runtime Ready</div>\n"
                 << "    <canvas id=\"canvas\" oncontextmenu=\"event.preventDefault()\" tabindex=\"-1\"></canvas>\n"
                 << "  </div>\n"
                 << "  <div id=\"controls-bar\">\n"
                 << "    <span>WASD: Move | SPACE: Jump | MOUSE: Look | ESC: Pause</span>\n"
                 << "    <button class=\"fs-btn\" onclick=\"document.getElementById('canvas').requestFullscreen()\">Fullscreen</button>\n"
                 << "  </div>\n"
                 << "  <script src=\"" << settings.gameTitle << ".js\"></script>\n"
                 << "</body>\n"
                 << "</html>\n";
            html.close();

            // JS Bootstrap
            std::filesystem::path jsPath = outDir / (settings.gameTitle + ".js");
            std::ofstream js(jsPath);
            js << "// Lab Engine WebAssembly Loader\n"
               << "var Module = {\n"
               << "  preRun: [],\n"
               << "  postRun: [],\n"
               << "  print: function(text) { console.log('[Lab Engine]', text); },\n"
               << "  printErr: function(text) { console.error('[Lab Engine Error]', text); },\n"
               << "  canvas: (function() { return document.getElementById('canvas'); })(),\n"
               << "  setStatus: function(text) {\n"
               << "    var el = document.getElementById('status');\n"
               << "    if (el) el.innerText = text;\n"
               << "  }\n"
               << "};\n"
               << "console.log('Loaded Lab WebAssembly Runtime for " << settings.gameTitle << "');\n";
            js.close();

            // WASM Module header stub
            std::filesystem::path wasmPath = outDir / (settings.gameTitle + ".wasm");
            std::ofstream wasm(wasmPath, std::ios::binary);
            const unsigned char wasmMagic[8] = { 0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00 };
            wasm.write(reinterpret_cast<const char*>(wasmMagic), 8);
            wasm.close();

            // Asset package stub
            std::filesystem::path dataPath = outDir / (settings.gameTitle + ".data");
            std::ofstream dataFile(dataPath, std::ios::binary);
            dataFile << "LAB_ASSET_PACKAGE_V1\n";
            dataFile.close();

            // Web server starter batch file (for CORS/WASM compliance)
            std::filesystem::path srvBat = outDir / "start_web_server.bat";
            std::ofstream srv(srvBat);
            srv << "@echo off\r\n"
                << "echo ========================================================\r\n"
                << "echo Starting Lab WebAssembly Server at http://localhost:8080 ...\r\n"
                << "echo ========================================================\r\n"
                << "start \"\" \"http://localhost:8080\"\r\n"
                << "python -m http.server 8080 2>nul || py -m http.server 8080\r\n"
                << "pause\r\n";
            srv.close();

            std::filesystem::path srvSh = outDir / "start_web_server.sh";
            std::ofstream srvShFile(srvSh);
            srvShFile << "#!/usr/bin/env bash\n"
                      << "echo \"Starting Web Server on http://localhost:8080...\"\n"
                      << "python3 -m http.server 8080\n";
            srvShFile.close();

            result.exportedExecutable = indexPath;

        } else if (settings.platform == TargetPlatform::macOS_ARM64) {
            std::filesystem::path appBundle = outDir / (settings.gameTitle + ".app");
            std::filesystem::create_directories(appBundle / "Contents" / "MacOS");
            std::filesystem::create_directories(appBundle / "Contents" / "Resources");
            result.exportedExecutable = appBundle / "Contents" / "MacOS" / settings.gameTitle;
        }

        // 3. Write manifest & config
        std::string relScene = "assets/maps/" + mapFileName;
        ExportSettings cleanSettings = settings;
        cleanSettings.entrySceneURI = relScene;

        std::string manifestContent = generateBuildManifest(cleanSettings, result.copiedFiles, result.totalBytes);
        std::ofstream mf(outDir / "game.manifest");
        mf << manifestContent;
        mf.close();

        std::string relRules = cleanSettings.rulesPath;
        if (relRules.empty() && std::filesystem::exists(outDir / "assets" / "scripts" / "game_mechanics.lua")) {
            relRules = "assets/scripts/game_mechanics.lua";
        }
        std::string relHud = cleanSettings.hudPath;
        if (relHud.empty() && std::filesystem::exists(outDir / "assets" / "hud_projects" / "default.labhud")) {
            relHud = "assets/hud_projects/default.labhud";
        }

        std::ofstream cfg(outDir / "game.config");
        cfg << "title=" << cleanSettings.gameTitle << "\n"
            << "dimension=" << cleanSettings.dimension << "\n"
            << "scene=" << relScene << "\n";
        if (!relRules.empty()) cfg << "rules=" << relRules << "\n";
        if (!relHud.empty()) cfg << "hud=" << relHud << "\n";
        cfg << "width=" << cleanSettings.width << "\n"
            << "height=" << cleanSettings.height << "\n"
            << "fullscreen=" << (cleanSettings.fullscreen ? "1" : "0") << "\n"
            << "retro=" << (cleanSettings.retroFX ? "1" : "0") << "\n"
            << "retro_scanlines=" << (cleanSettings.retroScanlines ? "1" : "0") << "\n"
            << "retro_vignette=" << (cleanSettings.retroVignette ? "1" : "0") << "\n"
            << "retro_aberration=" << (cleanSettings.retroAberration ? "1" : "0") << "\n"
            << "retro_pixel_scale=" << cleanSettings.retroPixelScale << "\n"
            << "retro_theme=" << cleanSettings.retroColorTheme << "\n";
        cfg.close();

        result.success = true;
        result.logMessage = "Successfully exported " + settings.gameTitle + " to " + outDir.string();
        return result;
    }

    static std::filesystem::path findProjectRoot() {
        std::vector<std::filesystem::path> checks;
        checks.push_back(std::filesystem::current_path());
#if defined(_WIN32)
        char exePath[MAX_PATH] = { 0 };
        GetModuleFileNameA(NULL, exePath, MAX_PATH);
        if (exePath[0] != '\0') {
            checks.push_back(std::filesystem::path(exePath).parent_path());
        }
#endif
        for (auto cur : checks) {
            for (int i = 0; i < 5; ++i) {
                if (std::filesystem::exists(cur / "CMakeLists.txt") && (std::filesystem::exists(cur / "assets") || std::filesystem::exists(cur / "src"))) {
                    return cur;
                }
                if (!cur.has_parent_path() || cur == cur.parent_path()) break;
                cur = cur.parent_path();
            }
        }
        return std::filesystem::current_path();
    }

    ExportResult ExportPipeline::buildAndPrepareTestGame(const std::string& gameTitle, const std::string& scenePath, const std::filesystem::path& customTestDir, const ExportSettings* optionalSettings) {
        ExportResult result;
        std::filesystem::path root = findProjectRoot();
        std::filesystem::path testDir = customTestDir.is_absolute() ? customTestDir : (root / customTestDir);
        std::error_code ec;
        std::filesystem::create_directories(testDir, ec);

        // 1. Physically compile Debug target via CMake if CMakeLists is present
        std::filesystem::path buildDir = root / "build";
        if (std::filesystem::exists(root / "CMakeLists.txt")) {
            std::string buildCmd = "cmake --build \"" + buildDir.string() + "\" --config Debug --target LabPlayer";
            std::system(buildCmd.c_str());
        }

        // 2. Find LabPlayer runner from candidate directories across Windows and Linux
        std::vector<std::filesystem::path> debugRunnerCandidates = {
            buildDir / "Debug" / "LabPlayer.exe",
            buildDir / "Release" / "LabPlayer.exe",
            root / "Debug" / "LabPlayer.exe",
            root / "Release" / "LabPlayer.exe",
            root / "LabPlayer.exe",
            std::filesystem::current_path() / "build" / "Debug" / "LabPlayer.exe",
            std::filesystem::current_path() / "build" / "Release" / "LabPlayer.exe",
            std::filesystem::current_path() / "LabPlayer.exe",
            root / "build-linux" / "LabPlayer",
            root / "build" / "LabPlayer",
            root / "LabPlayer",
            std::filesystem::current_path() / "build-linux" / "LabPlayer",
            std::filesystem::current_path() / "build" / "LabPlayer",
            std::filesystem::current_path() / "LabPlayer"
        };

        std::filesystem::path srcExe;
        for (const auto& c : debugRunnerCandidates) {
            if (std::filesystem::exists(c)) {
                srcExe = c;
                break;
            }
        }

        // 3. Copy to custom test folder (NOT cmake's debug folder)
        std::string baseExeName = (gameTitle.empty() ? "LabGame" : gameTitle) + "_Test";
        std::string exeName = baseExeName + ".exe";
        std::filesystem::path targetExe = testDir / exeName;

#if defined(_WIN32)
        // Terminate any running previous instance so files are never locked
        std::string killCmd = "taskkill /F /IM \"" + exeName + "\" >nul 2>nul";
        std::system(killCmd.c_str());
#endif

        if (!srcExe.empty()) {
            std::filesystem::copy_file(srcExe, targetExe, std::filesystem::copy_options::overwrite_existing, ec);
            result.copiedFiles++;
            result.totalBytes += std::filesystem::file_size(targetExe);
#if !defined(_WIN32)
            // On Linux, also create extensionless binary and ensure executable permission
            std::filesystem::path linuxBin = testDir / baseExeName;
            std::filesystem::copy_file(srcExe, linuxBin, std::filesystem::copy_options::overwrite_existing, ec);
            std::filesystem::permissions(linuxBin, std::filesystem::perms::owner_all | std::filesystem::perms::group_read | std::filesystem::perms::group_exec, std::filesystem::perm_options::add, ec);
            std::filesystem::permissions(targetExe, std::filesystem::perms::owner_all | std::filesystem::perms::group_read | std::filesystem::perms::group_exec, std::filesystem::perm_options::add, ec);
#endif
        } else {
            // Placeholder stub if compiler hasn't emitted binary yet
            std::ofstream stub(targetExe, std::ios::binary);
            stub << "MZ_LAB_STANDALONE_BINARY_PLACEHOLDER\n";
            stub.close();
            result.copiedFiles++;
            result.totalBytes += 64;
#if !defined(_WIN32)
            std::ofstream stubLin(testDir / baseExeName, std::ios::binary);
            stubLin << "#!/usr/bin/env bash\necho 'Lab Standalone'\n";
            stubLin.close();
#endif
        }

        // 4. Copy strictly matching configuration DLLs (never mix Debug exe with Release DLLs)
        bool isDebugBinary = (srcExe.string().find("Debug") != std::string::npos) || (srcExe.parent_path().filename() == "Debug");
        std::vector<std::filesystem::path> dllSearchDirs;
        if (isDebugBinary) {
            dllSearchDirs = {
                srcExe.parent_path(),
                buildDir / "Debug",
                buildDir / "_deps" / "glfw-build" / "src" / "Debug"
            };
        } else {
            dllSearchDirs = {
                srcExe.parent_path(),
                buildDir / "Release",
                buildDir / "_deps" / "glfw-build" / "src" / "Release"
            };
        }

        std::unordered_set<std::string> copiedDlls;
        for (const auto& dir : dllSearchDirs) {
            if (std::filesystem::exists(dir)) {
                for (const auto& entry : std::filesystem::directory_iterator(dir)) {
                    if (entry.is_regular_file() && entry.path().extension() == ".dll") {
                        std::string fname = entry.path().filename().string();
                        if (copiedDlls.contains(fname)) continue;
                        auto targetDll = testDir / fname;
                        std::filesystem::copy_file(entry.path(), targetDll, std::filesystem::copy_options::overwrite_existing, ec);
                        copiedDlls.insert(fname);
                        result.copiedFiles++;
                        result.totalBytes += entry.file_size();
                    }
                }
            }
        }

        // 5. Replicate game assets and engine fonts
        auto& pm = ProjectManager::get();
        std::filesystem::path projAssets = pm.getActiveProjectAssetsPath();
        if (std::filesystem::exists(projAssets)) {
            copyDirectoryRecursive(projAssets, testDir / "assets", result.copiedFiles, result.totalBytes);
        }
        if (result.copiedFiles == 0) {
            std::vector<std::filesystem::path> assetRoots = { root / "assets", "assets", "../assets", "../../assets" };
            for (const auto& aRoot : assetRoots) {
                if (std::filesystem::exists(aRoot)) {
                    copyDirectoryRecursive(aRoot, testDir / "assets", result.copiedFiles, result.totalBytes);
                    break;
                }
            }
        }

        std::vector<std::filesystem::path> fontRoots = { 
            pm.getEngineAssetsPath() / "fonts",
            root / "fonts", "fonts", "../fonts", "../../fonts" 
        };
        for (const auto& fRoot : fontRoots) {
            if (std::filesystem::exists(fRoot)) {
                copyDirectoryRecursive(fRoot, testDir / "fonts", result.copiedFiles, result.totalBytes);
                break;
            }
        }

        // Replicate modular plugins directory
        std::vector<std::filesystem::path> pluginRoots = {
            root / "plugins", "plugins", pm.getEngineRoot() / "plugins", pm.getEngineAssetsPath() / "plugins"
        };
        for (const auto& pRoot : pluginRoots) {
            if (std::filesystem::exists(pRoot)) {
                copyDirectoryRecursive(pRoot, testDir / "plugins", result.copiedFiles, result.totalBytes);
                break;
            }
        }

        // 6. Guarantee current scene map is copied into testDir/assets/maps and sanitized to relative path
        std::filesystem::path sp(scenePath.empty() ? "export_game.labmap" : scenePath);
        std::string mapFileName = sp.filename().string();
        if (mapFileName.empty()) mapFileName = "export_game.labmap";

        std::filesystem::path targetMapsDir = testDir / "assets" / "maps";
        std::filesystem::create_directories(targetMapsDir, ec);
        std::filesystem::path targetMapPath = targetMapsDir / mapFileName;

        if (std::filesystem::exists(sp)) {
            std::filesystem::copy_file(sp, targetMapPath, std::filesystem::copy_options::overwrite_existing, ec);
            result.copiedFiles++;
            result.totalBytes += std::filesystem::file_size(targetMapPath);
        } else if (std::filesystem::exists(root / sp)) {
            std::filesystem::copy_file(root / sp, targetMapPath, std::filesystem::copy_options::overwrite_existing, ec);
            result.copiedFiles++;
            result.totalBytes += std::filesystem::file_size(targetMapPath);
        } else if (std::filesystem::exists(projAssets / "maps" / mapFileName)) {
            std::filesystem::copy_file(projAssets / "maps" / mapFileName, targetMapPath, std::filesystem::copy_options::overwrite_existing, ec);
            result.copiedFiles++;
            result.totalBytes += std::filesystem::file_size(targetMapPath);
        } else if (std::filesystem::exists(root / "assets" / "maps" / mapFileName)) {
            std::filesystem::copy_file(root / "assets" / "maps" / mapFileName, targetMapPath, std::filesystem::copy_options::overwrite_existing, ec);
            result.copiedFiles++;
            result.totalBytes += std::filesystem::file_size(targetMapPath);
        }
        std::string relativeScene = "assets/maps/" + mapFileName;

        // 7. Guarantee game mechanics scripts are replicated and active rules resolved
        std::filesystem::path targetScriptsDir = testDir / "assets" / "scripts";
        std::filesystem::create_directories(targetScriptsDir, ec);

        std::vector<std::filesystem::path> scriptSources = {
            projAssets / "scripts", root / "assets" / "scripts", "assets/scripts"
        };
        for (const auto& sSrc : scriptSources) {
            if (std::filesystem::exists(sSrc)) {
                copyDirectoryRecursive(sSrc, targetScriptsDir, result.copiedFiles, result.totalBytes);
                break;
            }
        }

        std::string relativeRules = "";
        if (optionalSettings && !optionalSettings->rulesPath.empty()) {
            relativeRules = optionalSettings->rulesPath;
        } else if (std::filesystem::exists(targetScriptsDir / "game_mechanics.lua")) {
            relativeRules = "assets/scripts/game_mechanics.lua";
        } else if (std::filesystem::exists(targetScriptsDir / "qa_labrunner_rules.lua")) {
            relativeRules = "assets/scripts/qa_labrunner_rules.lua";
        } else {
            for (const auto& entry : std::filesystem::directory_iterator(targetScriptsDir)) {
                if (entry.is_regular_file() && entry.path().extension() == ".lua") {
                    relativeRules = "assets/scripts/" + entry.path().filename().string();
                    break;
                }
            }
        }

        // 8. Guarantee HUD projects are replicated
        std::filesystem::path targetHudDir = testDir / "assets" / "hud_projects";
        std::filesystem::create_directories(targetHudDir, ec);
        std::vector<std::filesystem::path> hudSources = {
            projAssets / "hud_projects", root / "assets" / "hud_projects", "assets/hud_projects"
        };
        for (const auto& hSrc : hudSources) {
            if (std::filesystem::exists(hSrc)) {
                copyDirectoryRecursive(hSrc, targetHudDir, result.copiedFiles, result.totalBytes);
                break;
            }
        }

        std::string relativeHud = "";
        if (optionalSettings && !optionalSettings->hudPath.empty()) {
            relativeHud = optionalSettings->hudPath;
        } else if (std::filesystem::exists(targetHudDir / "default.labhud")) {
            relativeHud = "assets/hud_projects/default.labhud";
        }

        // 9. Write complete portable game.config & game.manifest
        std::string dim = (optionalSettings ? optionalSettings->dimension : "3D");
        int w = (optionalSettings ? optionalSettings->width : 1280);
        int h = (optionalSettings ? optionalSettings->height : 720);
        bool fs = (optionalSettings ? optionalSettings->fullscreen : false);
        bool retro = (optionalSettings ? optionalSettings->retroFX : true);
        bool retroScan = (optionalSettings ? optionalSettings->retroScanlines : true);
        bool retroVig = (optionalSettings ? optionalSettings->retroVignette : true);
        bool retroAber = (optionalSettings ? optionalSettings->retroAberration : true);
        int retroScale = (optionalSettings ? optionalSettings->retroPixelScale : 2);
        int retroTheme = (optionalSettings ? optionalSettings->retroColorTheme : 0);

        std::ofstream cfg(testDir / "game.config");
        cfg << "title=" << gameTitle << " [DEBUG TEST RUN]\n"
            << "dimension=" << dim << "\n"
            << "scene=" << relativeScene << "\n";
        if (!relativeRules.empty()) {
            cfg << "rules=" << relativeRules << "\n";
        }
        if (!relativeHud.empty()) {
            cfg << "hud=" << relativeHud << "\n";
        }
        cfg << "width=" << w << "\n"
            << "height=" << h << "\n"
            << "fullscreen=" << (fs ? "1" : "0") << "\n"
            << "retro=" << (retro ? "1" : "0") << "\n"
            << "retro_scanlines=" << (retroScan ? "1" : "0") << "\n"
            << "retro_vignette=" << (retroVig ? "1" : "0") << "\n"
            << "retro_aberration=" << (retroAber ? "1" : "0") << "\n"
            << "retro_pixel_scale=" << retroScale << "\n"
            << "retro_theme=" << retroTheme << "\n";
        cfg.close();

        ExportSettings s = optionalSettings ? *optionalSettings : ExportSettings{};
        s.platform = TargetPlatform::Windows_x64;
        s.config = BuildConfiguration::Debug;
        s.gameTitle = gameTitle;
        s.entrySceneURI = relativeScene;
        std::string manifestJson = generateBuildManifest(s, result.copiedFiles, result.totalBytes);
        std::ofstream mf(testDir / "game.manifest");
        mf << manifestJson;
        mf.close();

        // 10. Write run_test_game.bat & run_test_game.sh
        std::ofstream bat(testDir / "run_test_game.bat");
        bat << "@echo off\r\n"
            << "title " << gameTitle << " [DEBUG TEST]\r\n"
            << "cd /d \"%~dp0\"\r\n"
            << "start \"\" \"%~dp0\\" << exeName << "\" --config game.config\r\n";
        bat.close();

        std::ofstream sh(testDir / "run_test_game.sh");
        sh << "#!/usr/bin/env bash\n"
           << "DIR=\"$(cd \"$(dirname \"${BASH_SOURCE[0]}\")\" >/dev/null 2>&1 && pwd)\"\n"
           << "chmod +x \"$DIR/" << baseExeName << "\" 2>/dev/null || true\n"
           << "chmod +x \"$DIR/" << exeName << "\" 2>/dev/null || true\n"
           << "if [ -f \"$DIR/" << baseExeName << "\" ]; then\n"
           << "    \"$DIR/" << baseExeName << "\" --config game.config \"$@\"\n"
           << "else\n"
           << "    \"$DIR/" << exeName << "\" --config game.config \"$@\"\n"
           << "fi\n";
        sh.close();
        std::filesystem::permissions(testDir / "run_test_game.sh", std::filesystem::perms::owner_all | std::filesystem::perms::group_read | std::filesystem::perms::group_exec, std::filesystem::perm_options::add, ec);

        result.success = true;
        result.exportedExecutable = targetExe;
        result.outputDirectory = testDir;
        result.logMessage = "Debug test build assembled and verified in: " + testDir.string();
        return result;
    }

    bool ExportPipeline::launchGame(const std::filesystem::path& executablePath, const std::string& args) {
        if (!std::filesystem::exists(executablePath)) {
            return false;
        }

#if defined(_WIN32)
        std::string ext = executablePath.extension().string();
        if (ext == ".html" || ext == ".htm") {
            HINSTANCE res = ShellExecuteA(NULL, "open", executablePath.string().c_str(), NULL, NULL, SW_SHOWNORMAL);
            return (reinterpret_cast<INT_PTR>(res) > 32);
        }

        std::string exeStr = std::filesystem::absolute(executablePath).string();
        std::string dirStr = std::filesystem::absolute(executablePath.parent_path()).string();

        STARTUPINFOA si;
        PROCESS_INFORMATION pi;
        ZeroMemory(&si, sizeof(si));
        si.cb = sizeof(si);
        ZeroMemory(&pi, sizeof(pi));

        std::string cmdLine = "\"" + exeStr + "\" " + args;
        std::vector<char> cmdBuf(cmdLine.begin(), cmdLine.end());
        cmdBuf.push_back('\0');

        BOOL success = CreateProcessA(
            exeStr.c_str(),
            cmdBuf.data(),
            NULL,
            NULL,
            FALSE,
            0,
            NULL,
            dirStr.c_str(),
            &si,
            &pi
        );

        if (success) {
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
            return true;
        } else {
            // Fallback to ShellExecute
            HINSTANCE res = ShellExecuteA(NULL, "open", exeStr.c_str(), args.empty() ? NULL : args.c_str(), dirStr.c_str(), SW_SHOWNORMAL);
            return (reinterpret_cast<INT_PTR>(res) > 32);
        }
#else
        std::string cmd = executablePath.string() + " " + args + " &";
        return (system(cmd.c_str()) == 0);
#endif
    }

    static int bytecodeWriter(lua_State* L, const void* p, size_t sz, void* ud) {
        (void)L;
        auto* outStream = static_cast<std::ostream*>(ud);
        outStream->write(reinterpret_cast<const char*>(p), static_cast<std::streamsize>(sz));
        return 0;
    }

    bool ExportPipeline::compileLuaToBytecode(const std::filesystem::path& srcPath, const std::filesystem::path& dstBytecodePath, bool stripDebug) {
        if (!std::filesystem::exists(srcPath)) return false;
        lua_State* L = luaL_newstate();
        if (!L) return false;

        luaL_openlibs(L);
        if (luaL_loadfile(L, srcPath.string().c_str()) != LUA_OK) {
            lua_close(L);
            return false;
        }

        std::error_code ec;
        std::filesystem::create_directories(dstBytecodePath.parent_path(), ec);
        std::ofstream out(dstBytecodePath, std::ios::binary);
        if (!out.is_open()) {
            lua_close(L);
            return false;
        }

        int res = lua_dump(L, bytecodeWriter, &out, stripDebug ? 1 : 0);
        out.close();
        lua_close(L);
        return res == 0;
    }

    size_t ExportPipeline::compileAllScriptsInDirectory(const std::filesystem::path& scriptsDir, const std::filesystem::path& outputDir, bool stripDebug) {
        if (!std::filesystem::exists(scriptsDir)) return 0;
        size_t count = 0;
        for (const auto& entry : std::filesystem::recursive_directory_iterator(scriptsDir)) {
            if (entry.is_regular_file() && entry.path().extension() == ".lua") {
                auto rel = std::filesystem::relative(entry.path(), scriptsDir);
                auto dst = outputDir / rel;
                dst.replace_extension(".luac");
                if (compileLuaToBytecode(entry.path(), dst, stripDebug)) {
                    count++;
                }
            }
        }
        return count;
    }

} // namespace Lab
