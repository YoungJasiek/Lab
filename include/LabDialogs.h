#pragma once
#include <glad/gl.h>
#include <string>
#include <filesystem>
#include <GLFW/glfw3.h>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <commdlg.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
#else
#include <cstdio>
#include <array>
#include <unistd.h>
#endif

namespace Lab {

    class LabDialogs {
    public:
        static bool isAllowedSceneFile(const std::string& path) {
            if (path.empty()) return false;
            std::filesystem::path p(path);
            std::string ext = p.extension().string();
            for (char& c : ext) c = static_cast<char>(::tolower(static_cast<unsigned char>(c)));
            if (ext != ".labscene" && ext != ".labmap") {
                return false;
            }
            if (std::filesystem::exists(p) && !std::filesystem::is_regular_file(p)) {
                return false;
            }
            return true;
        }

        // Native File Open Dialog (Windows GetOpenFileNameA, Linux zenity/kdialog)
        static std::string openFileDialog(GLFWwindow* window, const char* filter = "Lab Scene Files (*.labscene;*.labmap)\0*.labscene;*.labmap\0", const char* defaultDir = "assets/maps") {
#if defined(_WIN32)
            char filename[MAX_PATH] = "";

            OPENFILENAMEA ofn;
            ZeroMemory(&ofn, sizeof(ofn));
            ofn.lStructSize = sizeof(ofn);
            ofn.hwndOwner = window ? glfwGetWin32Window(window) : NULL;
            ofn.lpstrFilter = filter;
            ofn.lpstrFile = filename;
            ofn.nMaxFile = MAX_PATH;

            std::string initialDirStr = "";
            if (defaultDir && strlen(defaultDir) > 0) {
                if (std::filesystem::exists(defaultDir)) {
                    initialDirStr = std::filesystem::absolute(defaultDir).string();
                } else if (std::filesystem::exists("../" + std::string(defaultDir))) {
                    initialDirStr = std::filesystem::absolute("../" + std::string(defaultDir)).string();
                } else if (std::filesystem::exists("../../" + std::string(defaultDir))) {
                    initialDirStr = std::filesystem::absolute("../../" + std::string(defaultDir)).string();
                }
            }
            if (!initialDirStr.empty()) {
                ofn.lpstrInitialDir = initialDirStr.c_str();
            }

            ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR | OFN_NODEREFERENCELINKS;

            if (GetOpenFileNameA(&ofn)) {
                std::string picked(filename);
                if (defaultDir && std::string(defaultDir).find("maps") != std::string::npos) {
                    if (!isAllowedSceneFile(picked)) {
                        return "";
                    }
                }
                return picked;
            }
            return "";
#else
            (void)window; (void)filter; (void)defaultDir;
            std::string cmd = "zenity --file-selection --file-filter='Lab Scene (*.labscene *.labmap) | *.labscene *.labmap' 2>/dev/null";
            FILE* pipe = popen(cmd.c_str(), "r");
            if (!pipe) {
                cmd = "kdialog --getopenfilename . '*.labscene *.labmap' 2>/dev/null";
                pipe = popen(cmd.c_str(), "r");
            }
            if (pipe) {
                char buffer[1024];
                std::string result = "";
                while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
                    result += buffer;
                }
                pclose(pipe);
                while (!result.empty() && (result.back() == '\n' || result.back() == '\r')) {
                    result.pop_back();
                }
                if (!result.empty()) {
                    if (defaultDir && std::string(defaultDir).find("maps") != std::string::npos && !isAllowedSceneFile(result)) {
                        return "";
                    }
                    return result;
                }
            }
            return "";
#endif
        }

        // Native File Save Dialog (Windows GetSaveFileNameA, Linux zenity/kdialog)
        static std::string saveFileDialog(GLFWwindow* window, const char* filter = "Lab Scene Files (*.labscene)\0*.labscene\0Lab Map Files (*.labmap)\0*.labmap\0", const char* defaultName = "my_level.labscene", const char* defaultDir = "assets/maps") {
#if defined(_WIN32)
            char filename[MAX_PATH] = "";
            strncpy_s(filename, defaultName, MAX_PATH - 1);

            OPENFILENAMEA ofn;
            ZeroMemory(&ofn, sizeof(ofn));
            ofn.lStructSize = sizeof(ofn);
            ofn.hwndOwner = window ? glfwGetWin32Window(window) : NULL;
            ofn.lpstrFilter = filter;
            ofn.lpstrFile = filename;
            ofn.nMaxFile = MAX_PATH;

            std::string initialDirStr = "";
            if (defaultDir && strlen(defaultDir) > 0) {
                if (std::filesystem::exists(defaultDir)) {
                    initialDirStr = std::filesystem::absolute(defaultDir).string();
                } else if (std::filesystem::exists("../" + std::string(defaultDir))) {
                    initialDirStr = std::filesystem::absolute("../" + std::string(defaultDir)).string();
                } else if (std::filesystem::exists("../../" + std::string(defaultDir))) {
                    initialDirStr = std::filesystem::absolute("../../" + std::string(defaultDir)).string();
                }
            }
            if (!initialDirStr.empty()) {
                ofn.lpstrInitialDir = initialDirStr.c_str();
            }

            ofn.lpstrDefExt = "labscene";
            ofn.Flags = OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR | OFN_NODEREFERENCELINKS;

            if (GetSaveFileNameA(&ofn)) {
                return std::string(filename);
            }
            return "";
#else
            (void)window; (void)filter; (void)defaultName; (void)defaultDir;
            std::string cmd = "zenity --file-selection --save --confirm-overwrite 2>/dev/null";
            FILE* pipe = popen(cmd.c_str(), "r");
            if (!pipe) {
                cmd = "kdialog --getsavefilename 2>/dev/null";
                pipe = popen(cmd.c_str(), "r");
            }
            if (pipe) {
                char buffer[1024];
                std::string result = "";
                while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
                    result += buffer;
                }
                pclose(pipe);
                while (!result.empty() && (result.back() == '\n' || result.back() == '\r')) {
                    result.pop_back();
                }
                if (!result.empty()) return result;
            }
            return "";
#endif
        }
    };

}
