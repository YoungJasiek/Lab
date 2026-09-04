#pragma once
#include <glad/gl.h>
#include <string>
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <commdlg.h>
#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>

namespace Lab {

    class LabDialogs {
    public:
        // Native Windows File Open Dialog (presety Windows GetOpenFileName)
        static std::string openFileDialog(GLFWwindow* window, const char* filter = "Lab Map Files (*.labmap)\0*.labmap\0All Files (*.*)\0*.*\0", const char* defaultDir = "assets\\maps") {
            char filename[MAX_PATH] = "";

            OPENFILENAMEA ofn;
            ZeroMemory(&ofn, sizeof(ofn));
            ofn.lStructSize = sizeof(ofn);
            ofn.hwndOwner = window ? glfwGetWin32Window(window) : NULL;
            ofn.lpstrFilter = filter;
            ofn.lpstrFile = filename;
            ofn.nMaxFile = MAX_PATH;
            ofn.lpstrInitialDir = defaultDir;
            ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;

            if (GetOpenFileNameA(&ofn)) {
                return std::string(filename);
            }
            return "";
        }

        // Native Windows File Save Dialog (presety Windows GetSaveFileName)
        static std::string saveFileDialog(GLFWwindow* window, const char* filter = "Lab Map Files (*.labmap)\0*.labmap\0All Files (*.*)\0*.*\0", const char* defaultName = "my_level.labmap", const char* defaultDir = "assets\\maps") {
            char filename[MAX_PATH] = "";
            strncpy_s(filename, defaultName, MAX_PATH - 1);

            OPENFILENAMEA ofn;
            ZeroMemory(&ofn, sizeof(ofn));
            ofn.lStructSize = sizeof(ofn);
            ofn.hwndOwner = window ? glfwGetWin32Window(window) : NULL;
            ofn.lpstrFilter = filter;
            ofn.lpstrFile = filename;
            ofn.nMaxFile = MAX_PATH;
            ofn.lpstrInitialDir = defaultDir;
            ofn.lpstrDefExt = "labmap";
            ofn.Flags = OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR;

            if (GetSaveFileNameA(&ofn)) {
                return std::string(filename);
            }
            return "";
        }
    };

}
