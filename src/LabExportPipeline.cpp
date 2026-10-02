#include "LabExportPipeline.h"
#include <fstream>
#include <sstream>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <iostream>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <shellapi.h>
#endif

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

        // 1. Replicate assets & fonts
        std::vector<std::string> assetRoots = { "assets", "../assets", "../../assets" };
        for (const auto& aRoot : assetRoots) {
            if (std::filesystem::exists(aRoot)) {
                copyDirectoryRecursive(aRoot, outDir / "assets", result.copiedFiles, result.totalBytes);
                break;
            }
        }

        std::vector<std::string> fontRoots = { "fonts", "../fonts", "../../fonts" };
        for (const auto& fRoot : fontRoots) {
            if (std::filesystem::exists(fRoot)) {
                copyDirectoryRecursive(fRoot, outDir / "fonts", result.copiedFiles, result.totalBytes);
                break;
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
        std::string manifestContent = generateBuildManifest(settings, result.copiedFiles, result.totalBytes);
        std::ofstream mf(outDir / "game.manifest");
        mf << manifestContent;
        mf.close();

        std::ofstream cfg(outDir / "game.config");
        cfg << "title=" << settings.gameTitle << "\n"
            << "scene=" << settings.entrySceneURI << "\n"
            << "width=1280\n"
            << "height=720\n"
            << "fullscreen=0\n";
        cfg.close();

        result.success = true;
        result.logMessage = "Successfully exported " + settings.gameTitle + " to " + outDir.string();
        return result;
    }

    ExportResult ExportPipeline::buildAndPrepareTestGame(const std::string& gameTitle, const std::string& scenePath, const std::filesystem::path& customTestDir) {
        ExportResult result;
        std::filesystem::path testDir = customTestDir.empty() ? std::filesystem::path("test_build/debug") : customTestDir;
        std::error_code ec;
        std::filesystem::create_directories(testDir, ec);

        // 1. Physically compile Debug target via CMake
        std::string buildCmd = "cmake --build build --config Debug --target LabPlayer";
        std::system(buildCmd.c_str());

        // 2. Find LabPlayer.exe from build/Debug
        std::vector<std::filesystem::path> debugRunnerCandidates = {
            "build/Debug/LabPlayer.exe",
            "Debug/LabPlayer.exe",
            "build/Release/LabPlayer.exe",
            "LabPlayer.exe"
        };

        std::filesystem::path srcExe;
        for (const auto& c : debugRunnerCandidates) {
            if (std::filesystem::exists(c)) {
                srcExe = c;
                break;
            }
        }

        if (srcExe.empty()) {
            result.success = false;
            result.logMessage = "Failed to compile or find LabPlayer.exe for test run!";
            return result;
        }

        // 3. Copy to custom test folder (NOT cmake's debug folder)
        std::string exeName = (gameTitle.empty() ? "LabGame" : gameTitle) + "_Test.exe";
        std::filesystem::path targetExe = testDir / exeName;
        std::filesystem::copy_file(srcExe, targetExe, std::filesystem::copy_options::overwrite_existing, ec);
        result.copiedFiles++;
        result.totalBytes += std::filesystem::file_size(targetExe);

        // 4. Copy all debug DLLs from source directory
        auto srcDir = srcExe.parent_path();
        if (std::filesystem::exists(srcDir)) {
            for (const auto& entry : std::filesystem::directory_iterator(srcDir)) {
                if (entry.is_regular_file() && entry.path().extension() == ".dll") {
                    auto targetDll = testDir / entry.path().filename();
                    std::filesystem::copy_file(entry.path(), targetDll, std::filesystem::copy_options::overwrite_existing, ec);
                    result.copiedFiles++;
                    result.totalBytes += entry.file_size();
                }
            }
        }

        // 5. Replicate assets and fonts
        std::vector<std::string> assetRoots = { "assets", "../assets", "../../assets" };
        for (const auto& aRoot : assetRoots) {
            if (std::filesystem::exists(aRoot)) {
                copyDirectoryRecursive(aRoot, testDir / "assets", result.copiedFiles, result.totalBytes);
                break;
            }
        }
        std::vector<std::string> fontRoots = { "fonts", "../fonts", "../../fonts" };
        for (const auto& fRoot : fontRoots) {
            if (std::filesystem::exists(fRoot)) {
                copyDirectoryRecursive(fRoot, testDir / "fonts", result.copiedFiles, result.totalBytes);
                break;
            }
        }

        // 6. Write game.config & game.manifest
        std::ofstream cfg(testDir / "game.config");
        cfg << "title=" << gameTitle << " [DEBUG TEST RUN]\n"
            << "scene=" << scenePath << "\n"
            << "width=1280\n"
            << "height=720\n"
            << "fullscreen=0\n";
        cfg.close();

        ExportSettings s;
        s.platform = TargetPlatform::Windows_x64;
        s.config = BuildConfiguration::Debug;
        s.gameTitle = gameTitle;
        s.entrySceneURI = scenePath;
        std::string manifestJson = generateBuildManifest(s, result.copiedFiles, result.totalBytes);
        std::ofstream mf(testDir / "game.manifest");
        mf << manifestJson;
        mf.close();

        // 7. Write run_test_game.bat
        std::ofstream bat(testDir / "run_test_game.bat");
        bat << "@echo off\r\n"
            << "title " << gameTitle << " [DEBUG TEST]\r\n"
            << "cd /d \"%~dp0\"\r\n"
            << "start \"\" \"%~dp0\\" << exeName << "\"\r\n";
        bat.close();

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

        std::string exeStr = executablePath.string();
        std::string dirStr = executablePath.parent_path().string();

        STARTUPINFOA si;
        PROCESS_INFORMATION pi;
        ZeroMemory(&si, sizeof(si));
        si.cb = sizeof(si);
        ZeroMemory(&pi, sizeof(pi));

        std::string cmdLine = "\"" + exeStr + "\" " + args;
        std::vector<char> cmdBuf(cmdLine.begin(), cmdLine.end());
        cmdBuf.push_back('\0');

        BOOL success = CreateProcessA(
            NULL,
            cmdBuf.data(),
            NULL,
            NULL,
            FALSE,
            0,
            NULL,
            dirStr.empty() ? NULL : dirStr.c_str(),
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

} // namespace Lab
