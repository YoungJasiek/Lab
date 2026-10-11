#include "LabProject.h"
#include "LabExportPipeline.h"
#include "LabVFS.h"
#include <iostream>
#include <fstream>
#include <cstdlib>
#include <unordered_set>
#include <algorithm>

#if defined(_WIN32)
#include <windows.h>
#include <shlobj.h>
#else
#include <unistd.h>
#endif

namespace Lab {

    static std::filesystem::path findEngineRoot() {
        std::vector<std::filesystem::path> checks;
        checks.push_back(std::filesystem::current_path());
#if defined(_WIN32)
        char exePath[MAX_PATH] = { 0 };
        GetModuleFileNameA(NULL, exePath, MAX_PATH);
        if (exePath[0] != '\0') {
            checks.push_back(std::filesystem::path(exePath).parent_path());
        }
#elif defined(__linux__) || defined(__unix__)
        char exePath[1024] = { 0 };
        ssize_t len = readlink("/proc/self/exe", exePath, sizeof(exePath) - 1);
        if (len != -1) {
            exePath[len] = '\0';
            checks.push_back(std::filesystem::path(exePath).parent_path());
        }
#endif
        for (auto cur : checks) {
            for (int i = 0; i < 5; ++i) {
                if (std::filesystem::exists(cur / "CMakeLists.txt") &&
                   (std::filesystem::exists(cur / "include") || std::filesystem::exists(cur / "src"))) {
                    return cur;
                }
                if (!cur.has_parent_path() || cur == cur.parent_path()) break;
                cur = cur.parent_path();
            }
        }
        return std::filesystem::current_path();
    }

    static std::filesystem::path resolveUserDocumentsRoot() {
        const char* customEnv = std::getenv("LAB_PROJECTS_DIR");
        if (customEnv && customEnv[0] != '\0') {
            return std::filesystem::path(customEnv);
        }

#if defined(_WIN32)
        char myDocs[MAX_PATH] = { 0 };
        if (SUCCEEDED(SHGetFolderPathA(NULL, CSIDL_MYDOCUMENTS, NULL, 0, myDocs)) && myDocs[0] != '\0') {
            return std::filesystem::path(myDocs) / "LabGames";
        }
        const char* userProfile = std::getenv("USERPROFILE");
        if (userProfile && userProfile[0] != '\0') {
            return std::filesystem::path(userProfile) / "Documents" / "LabGames";
        }
#else
        const char* home = std::getenv("HOME");
        if (home && home[0] != '\0') {
            return std::filesystem::path(home) / "Documents" / "LabGames";
        }
#endif
        return findEngineRoot() / "projects";
    }

    ProjectManager& ProjectManager::get() {
        static ProjectManager instance;
        return instance;
    }

    ProjectManager::ProjectManager() {
        m_engineRoot = findEngineRoot();
        m_userProjectsRoot = resolveUserDocumentsRoot();

        std::error_code ec;
        std::filesystem::create_directories(m_userProjectsRoot, ec);

        // Keep root workspace clean without force-populating legacy projects
        setupVFS();
    }

    std::filesystem::path ProjectManager::getEngineRoot() const {
        return m_engineRoot;
    }

    std::filesystem::path ProjectManager::getEngineAssetsPath() const {
        std::filesystem::path p = m_engineRoot / "engine" / "assets";
        if (std::filesystem::exists(p)) return p;
        return m_engineRoot / "assets"; // backward fallback
    }

    std::filesystem::path ProjectManager::getUserProjectsRoot() const {
        return m_userProjectsRoot;
    }

    void ProjectManager::setUserProjectsRoot(const std::filesystem::path& path) {
        m_userProjectsRoot = path;
        std::error_code ec;
        std::filesystem::create_directories(m_userProjectsRoot, ec);
        setupVFS();
    }

    std::string ProjectManager::getActiveProjectName() const {
        return m_activeProjectName;
    }

    std::filesystem::path ProjectManager::getActiveProjectPath() const {
        auto inGames = m_engineRoot / "games" / m_activeProjectName;
        if (std::filesystem::exists(inGames)) {
            return inGames;
        }
        return m_userProjectsRoot / m_activeProjectName;
    }

    std::filesystem::path ProjectManager::getActiveProjectAssetsPath() const {
        return getActiveProjectPath() / "assets";
    }

    bool ProjectManager::setActiveProject(const std::string& projectName) {
        if (projectName.empty()) return false;
        if (!ensureProjectExists(projectName)) return false;
        m_activeProjectName = projectName;
        setupVFS();
        return true;
    }

    bool ProjectManager::createProject(const std::string& projectName, const std::string& dimension, int width, int height, bool fullscreen) {
        if (projectName.empty()) return false;

        std::filesystem::path projDir = m_userProjectsRoot / projectName;
        std::filesystem::path assetsDir = projDir / "assets";

        std::error_code ec;
        std::filesystem::create_directories(assetsDir / "maps", ec);
        std::filesystem::create_directories(assetsDir / "scripts", ec);
        std::filesystem::create_directories(assetsDir / "textures", ec);
        std::filesystem::create_directories(assetsDir / "models", ec);
        std::filesystem::create_directories(assetsDir / "audio", ec);
        std::filesystem::create_directories(assetsDir / "hud", ec);

        bool is2D = (dimension == "2D");
        std::string entryScene = is2D ? "assets/maps/main.labmap2d" : "assets/maps/main.labmap";

        // Populate metadata project.labproject
        std::filesystem::path projectMeta = projDir / "project.labproject";
        std::ofstream pm(projectMeta);
        if (pm.is_open()) {
            pm << "{\n"
               << "  \"name\": \"" << projectName << "\",\n"
               << "  \"dimension\": \"" << (is2D ? "2D" : "3D") << "\",\n"
               << "  \"version\": \"1.0.0\",\n"
               << "  \"engine\": \"Lab Engine Studio 2026.10\",\n"
               << "  \"author\": \"Developer\",\n"
               << "  \"description\": \"" << (is2D ? "2D Game Project" : "3D Game Project") << "\",\n"
               << "  \"display\": {\n"
               << "    \"width\": " << width << ",\n"
               << "    \"height\": " << height << ",\n"
               << "    \"fullscreen\": " << (fullscreen ? "true" : "false") << ",\n"
               << "    \"vsync\": true\n"
               << "  },\n"
               << "  \"entryScene\": \"" << entryScene << "\"\n"
               << "}\n";
            pm.close();
        }

        // Populate game.config
        std::filesystem::path configPath = projDir / "game.config";
        std::ofstream cfg(configPath);
        if (cfg.is_open()) {
            cfg << "title=" << projectName << "\n"
                << "dimension=" << (is2D ? "2D" : "3D") << "\n"
                << "scene=" << entryScene << "\n"
                << "width=" << width << "\n"
                << "height=" << height << "\n"
                << "fullscreen=" << (fullscreen ? "1" : "0") << "\n"
                << "vsync=1\n";
            cfg.close();
        }

        if (is2D) {
            // Clean Native 2D starter scene (Camera 2D + 1 Sprite Quad on 2D plane; ZERO 3D hack)
            std::filesystem::path defaultMap2D = assetsDir / "maps" / "main.labmap2d";
            if (!std::filesystem::exists(defaultMap2D)) {
                std::ofstream dm(defaultMap2D);
                dm << "MAP2D_VERSION 1\n"
                   << "MAP2D_NAME " << projectName << " 2D Starter Scene\n"
                   << "CAMERA2D 0.0 0.0 10.0\n"
                   << "SPRITE_QUAD 0.0 0.0 2.0 2.0 dev_gray.bmp 1.0 1.0 1.0\n";
                dm.close();
            }
        } else {
            // Clean 3D starter scene (1 Camera, 1 Cube, 1 Light; ZERO player/weapons/AI)
            std::filesystem::path defaultMap = assetsDir / "maps" / "main.labmap";
            if (!std::filesystem::exists(defaultMap)) {
                std::ofstream dm(defaultMap);
                dm << "MAP_VERSION 1\n"
                   << "MAP_NAME " << projectName << " Starter Scene\n"
                   << "CAMERA 0.0 2.0 5.0 0.0 -90.0 60.0\n"
                   << "LIGHT 0.0 5.0 3.0 1.0 1.0 1.0 25.0\n"
                   << "BRUSH cube 0.0 0.0 0.0 2.0 2.0 2.0 dev_gray.bmp 0.5 0.5\n";
                dm.close();
            }
        }

        return true;
    }

    bool ProjectManager::saveProjectSettings(const std::string& projectName, const ProjectInfo& info) {
        if (projectName.empty()) return false;
        std::filesystem::path projDir = m_userProjectsRoot / projectName;
        if (!std::filesystem::exists(projDir)) {
            projDir = m_engineRoot / "games" / projectName;
            if (!std::filesystem::exists(projDir)) return false;
        }

        bool is2D = (info.dimension == "2D");

        // Save project.labproject
        std::ofstream pm(projDir / "project.labproject");
        if (pm.is_open()) {
            pm << "{\n"
               << "  \"name\": \"" << info.displayName << "\",\n"
               << "  \"dimension\": \"" << (is2D ? "2D" : "3D") << "\",\n"
               << "  \"version\": \"" << info.version << "\",\n"
               << "  \"engine\": \"Lab Engine Studio 2026.10\",\n"
               << "  \"author\": \"" << info.author << "\",\n"
               << "  \"description\": \"" << info.description << "\",\n"
               << "  \"display\": {\n"
               << "    \"width\": " << info.display.width << ",\n"
               << "    \"height\": " << info.display.height << ",\n"
               << "    \"fullscreen\": " << (info.display.fullscreen ? "true" : "false") << ",\n"
               << "    \"vsync\": " << (info.display.vsync ? "true" : "false") << "\n"
               << "  },\n"
               << "  \"entryScene\": \"" << info.defaultScene << "\",\n"
               << "  \"defaultHud\": \"" << info.defaultHud << "\"\n"
               << "}\n";
            pm.close();
        }

        // Save game.config
        std::ofstream cfg(projDir / "game.config");
        if (cfg.is_open()) {
            cfg << "title=" << info.displayName << "\n"
                << "dimension=" << (is2D ? "2D" : "3D") << "\n"
                << "scene=" << info.defaultScene << "\n"
                << "width=" << info.display.width << "\n"
                << "height=" << info.display.height << "\n"
                << "fullscreen=" << (info.display.fullscreen ? "1" : "0") << "\n"
                << "vsync=" << (info.display.vsync ? "1" : "0") << "\n";
            if (!info.defaultHud.empty()) cfg << "hud=" << info.defaultHud << "\n";
            cfg.close();
        }

        return true;
    }

    ProjectInfo ProjectManager::getProjectInfo(const std::string& projectName) const {
        auto all = getAllDiscoveredProjects();
        for (const auto& p : all) {
            if (p.id == projectName) return p;
        }
        ProjectInfo def;
        def.id = projectName;
        def.displayName = projectName;
        def.dimension = "3D";
        def.rootPath = m_userProjectsRoot / projectName;
        def.assetsPath = def.rootPath / "assets";
        def.defaultScene = "assets/maps/main.labmap";
        return def;
    }

    void ProjectManager::syncGameToDocuments(const std::string& projectName) {
        std::filesystem::path projDir = m_userProjectsRoot / projectName;
        std::filesystem::path assetsDir = projDir / "assets";
        std::error_code ec;
        std::filesystem::create_directories(assetsDir / "maps", ec);
        std::filesystem::create_directories(assetsDir / "scripts", ec);
        std::filesystem::create_directories(assetsDir / "textures", ec);
        std::filesystem::create_directories(assetsDir / "hud", ec);

        // Sources to copy from engine tree templates (games/<projectName> or legacy assets)
        std::vector<std::filesystem::path> templateRoots = {
            m_engineRoot / "games" / "labrunner_escape" / "assets",
            m_engineRoot / "assets"
        };

        for (const auto& tRoot : templateRoots) {
            if (!std::filesystem::exists(tRoot)) continue;

            // Map
            auto srcMap1 = tRoot / "maps" / "qa_labrunner_escape.labmap";
            auto dstMap = assetsDir / "maps" / "qa_labrunner_escape.labmap";
            if (std::filesystem::exists(srcMap1) && !std::filesystem::exists(dstMap)) {
                std::filesystem::copy_file(srcMap1, dstMap, std::filesystem::copy_options::overwrite_existing, ec);
            }

            // Rules script
            auto srcScript1 = tRoot / "scripts" / "qa_labrunner_rules.lua";
            auto dstScript = assetsDir / "scripts" / "qa_labrunner_rules.lua";
            if (std::filesystem::exists(srcScript1) && !std::filesystem::exists(dstScript)) {
                std::filesystem::copy_file(srcScript1, dstScript, std::filesystem::copy_options::overwrite_existing, ec);
            }

            // HUD
            auto srcHud1 = tRoot / "hud" / "labrunner_escape.labhud";
            auto srcHud2 = tRoot / "hud_projects" / "labrunner_escape.labhud";
            auto dstHud = assetsDir / "hud" / "labrunner_escape.labhud";
            if (std::filesystem::exists(srcHud1) && !std::filesystem::exists(dstHud)) {
                std::filesystem::copy_file(srcHud1, dstHud, std::filesystem::copy_options::overwrite_existing, ec);
            } else if (std::filesystem::exists(srcHud2) && !std::filesystem::exists(dstHud)) {
                std::filesystem::copy_file(srcHud2, dstHud, std::filesystem::copy_options::overwrite_existing, ec);
            }

            // Textures
            auto srcTexDir = tRoot / "textures";
            if (std::filesystem::exists(srcTexDir)) {
                std::vector<std::string> wantedTex = { "floor_lab.bmp", "blast_door.bmp", "cryo_ice.bmp", "concrete_wall.bmp" };
                for (const auto& tName : wantedTex) {
                    auto sTex = srcTexDir / tName;
                    auto dTex = assetsDir / "textures" / tName;
                    if (std::filesystem::exists(sTex) && !std::filesystem::exists(dTex)) {
                        std::filesystem::copy_file(sTex, dTex, std::filesystem::copy_options::overwrite_existing, ec);
                    }
                }
            }
        }

        // Configure game.config for this project
        std::ofstream cfg(projDir / "game.config");
        cfg << "title=QA LabRunner: Kryo-Ucieczka\n"
            << "scene=assets/maps/qa_labrunner_escape.labmap\n"
            << "hud=assets/hud/labrunner_escape.labhud\n"
            << "rules=assets/scripts/qa_labrunner_rules.lua\n"
            << "width=1280\n"
            << "height=720\n"
            << "fullscreen=0\n";
        cfg.close();
    }

    bool ProjectManager::ensureProjectExists(const std::string& projectName) {
        if (projectName.empty()) return false;
        auto inGames = m_engineRoot / "games" / projectName;
        if (std::filesystem::exists(inGames)) return true;
        std::filesystem::path projDir = m_userProjectsRoot / projectName;
        if (!std::filesystem::exists(projDir)) {
            return createProject(projectName);
        }
        return true;
    }

    static std::string extractJsonValue(const std::string& content, const std::string& key) {
        std::string pattern = "\"" + key + "\"";
        size_t p = content.find(pattern);
        if (p == std::string::npos) return "";
        p = content.find(':', p);
        if (p == std::string::npos) return "";
        p = content.find('"', p);
        if (p == std::string::npos) return "";
        size_t endP = content.find('"', p + 1);
        if (endP == std::string::npos) return "";
        return content.substr(p + 1, endP - p - 1);
    }

    static int extractJsonInt(const std::string& content, const std::string& key, int defaultVal) {
        std::string pattern = "\"" + key + "\"";
        size_t p = content.find(pattern);
        if (p == std::string::npos) return defaultVal;
        p = content.find(':', p);
        if (p == std::string::npos) return defaultVal;
        p = content.find_first_of("0123456789-", p);
        if (p == std::string::npos) return defaultVal;
        return std::atoi(content.c_str() + p);
    }

    static bool extractJsonBool(const std::string& content, const std::string& key, bool defaultVal) {
        std::string pattern = "\"" + key + "\"";
        size_t p = content.find(pattern);
        if (p == std::string::npos) return defaultVal;
        p = content.find(':', p);
        if (p == std::string::npos) return defaultVal;
        size_t t = content.find("true", p);
        size_t f = content.find("false", p);
        if (t != std::string::npos && (f == std::string::npos || t < f)) return true;
        if (f != std::string::npos && (t == std::string::npos || f < t)) return false;
        return defaultVal;
    }

    static std::string extractConfigValue(const std::string& content, const std::string& key) {
        std::string pattern = key + "=";
        size_t p = content.find(pattern);
        if (p == std::string::npos) return "";
        p += pattern.length();
        size_t endP = content.find_first_of("\r\n", p);
        if (endP == std::string::npos) return content.substr(p);
        return content.substr(p, endP - p);
    }

    std::vector<ProjectInfo> ProjectManager::getAllDiscoveredProjects() const {
        std::vector<ProjectInfo> list;
        std::unordered_set<std::string> seenIds;

        auto parseProject = [&](const std::filesystem::path& dir, bool isBuiltin) {
            std::string id = dir.filename().string();
            if (id.empty() || seenIds.contains(id)) return;

            ProjectInfo info;
            info.id = id;
            info.displayName = id;
            info.rootPath = dir;
            info.assetsPath = dir / "assets";
            info.isBuiltinShowcase = isBuiltin;
            info.version = "1.0.0";
            info.dimension = "3D";
            info.display.width = 1280;
            info.display.height = 720;
            info.display.fullscreen = false;
            info.display.vsync = true;
            info.author = isBuiltin ? "Lab Engine Core Team" : "User";
            info.description = isBuiltin ? "Built-in Showcase Game" : "User Game Project in Documents";
            info.lastModified = "Recent";

            // Check project.labproject
            std::filesystem::path metaPath = dir / "project.labproject";
            if (std::filesystem::exists(metaPath)) {
                std::ifstream f(metaPath);
                std::string content((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
                std::string disp = extractJsonValue(content, "displayName");
                if (disp.empty()) disp = extractJsonValue(content, "name");
                if (!disp.empty()) info.displayName = disp;
                std::string dim = extractJsonValue(content, "dimension");
                if (!dim.empty()) info.dimension = dim;
                std::string scn = extractJsonValue(content, "entryScene");
                if (!scn.empty()) info.defaultScene = scn;
                std::string dhud = extractJsonValue(content, "defaultHud");
                if (!dhud.empty()) info.defaultHud = dhud;
                std::string desc = extractJsonValue(content, "description");
                if (!desc.empty()) info.description = desc;
                std::string auth = extractJsonValue(content, "author");
                if (!auth.empty()) info.author = auth;
                std::string ver = extractJsonValue(content, "version");
                if (!ver.empty()) info.version = ver;

                info.display.width = extractJsonInt(content, "width", info.display.width);
                info.display.height = extractJsonInt(content, "height", info.display.height);
                info.display.fullscreen = extractJsonBool(content, "fullscreen", info.display.fullscreen);
                info.display.vsync = extractJsonBool(content, "vsync", info.display.vsync);
            }

            // Check game.config
            std::filesystem::path cfgPath = dir / "game.config";
            if (std::filesystem::exists(cfgPath)) {
                std::ifstream f(cfgPath);
                std::string content((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
                if (info.displayName == id) {
                    std::string t = extractConfigValue(content, "title");
                    if (!t.empty()) info.displayName = t;
                }
                std::string dim = extractConfigValue(content, "dimension");
                if (!dim.empty()) info.dimension = dim;
                if (info.defaultScene.empty()) {
                    std::string scn = extractConfigValue(content, "scene");
                    if (!scn.empty()) info.defaultScene = scn;
                }
                if (info.defaultHud.empty()) {
                    std::string hud = extractConfigValue(content, "hud");
                    if (!hud.empty()) info.defaultHud = hud;
                }
                std::string wStr = extractConfigValue(content, "width");
                if (!wStr.empty()) info.display.width = std::atoi(wStr.c_str());
                std::string hStr = extractConfigValue(content, "height");
                if (!hStr.empty()) info.display.height = std::atoi(hStr.c_str());
                std::string fsStr = extractConfigValue(content, "fullscreen");
                if (!fsStr.empty()) info.display.fullscreen = (fsStr == "1" || fsStr == "true");
                std::string vsStr = extractConfigValue(content, "vsync");
                if (!vsStr.empty()) info.display.vsync = (vsStr == "1" || vsStr == "true");
            }

            // Default fallbacks for known built-in games
            if (id == "frozen_life") {
                if (info.displayName == id) info.displayName = "Frozen-Life (3D Showcase)";
                if (info.defaultScene.empty()) info.defaultScene = "assets/maps/cryo_complex.labmap";
                if (info.defaultHud.empty() || info.defaultHud.find("labrunner") != std::string::npos) {
                    info.defaultHud = "assets/hud/frozen_life_hud.labhud";
                }
                if (info.description.empty() || info.description == "Built-in Showcase Game") {
                    info.description = "Flagship 3D Showcase Game for Lab Engine with Cryo Outpost, volumetric lighting, physics, T-800 character animation and weapon mechanics.";
                }
            } else if (id == "labrunner_escape") {
                if (info.displayName == id) info.displayName = "QA LabRunner: Kryo-Ucieczka";
                if (info.defaultScene.empty()) info.defaultScene = "assets/maps/qa_labrunner_escape.labmap";
                if (info.defaultHud.empty()) info.defaultHud = "assets/hud_projects/labrunner_escape.labhud";
                if (info.description.empty() || info.description == "Built-in Showcase Game") {
                    info.description = "Laboratory runner showcase testing dynamic collision, speed mechanics, and Lua rules.";
                }
            }

            // Check demo availability
            std::filesystem::path demoDir = dir / "demo";
            std::filesystem::path frozenDemo = demoDir / "FrozenLife_Demo.exe";
            std::filesystem::path namedDemo = demoDir / (id + ".exe");
            std::filesystem::path batDemo = demoDir / "run_demo.bat";
            std::filesystem::path rootBat = dir / "run_game.bat";

            if (std::filesystem::exists(frozenDemo)) {
                info.hasStandaloneDemo = true;
                info.demoExePath = frozenDemo;
            } else if (std::filesystem::exists(namedDemo)) {
                info.hasStandaloneDemo = true;
                info.demoExePath = namedDemo;
            } else if (std::filesystem::exists(batDemo)) {
                info.hasStandaloneDemo = true;
                info.demoExePath = batDemo;
            } else if (std::filesystem::exists(rootBat)) {
                info.hasStandaloneDemo = true;
                info.demoExePath = rootBat;
            }

            seenIds.insert(id);
            list.push_back(info);
        };

        // 1. Scan engine built-in games/
        std::filesystem::path gamesDir = m_engineRoot / "games";
        if (std::filesystem::exists(gamesDir)) {
            // Guarantee frozen_life is processed first if present
            if (std::filesystem::exists(gamesDir / "frozen_life")) {
                parseProject(gamesDir / "frozen_life", true);
            }
            for (const auto& entry : std::filesystem::directory_iterator(gamesDir)) {
                if (entry.is_directory()) {
                    parseProject(entry.path(), true);
                }
            }
        }

        // Guarantee frozen_life is in the list even if directory scan missed it
        if (!seenIds.contains("frozen_life")) {
            ProjectInfo fl;
            fl.id = "frozen_life";
            fl.displayName = "Frozen-Life (3D Showcase)";
            fl.rootPath = m_engineRoot / "games" / "frozen_life";
            fl.assetsPath = fl.rootPath / "assets";
            fl.defaultScene = "assets/maps/cryo_complex.labmap";
            fl.defaultHud = "assets/hud/frozen_life_hud.labhud";
            fl.description = "Flagship 3D Showcase Game for Lab Engine with Cryo Outpost, volumetric lighting, physics, T-800 character animation and weapon mechanics.";
            fl.isBuiltinShowcase = true;
            fl.hasStandaloneDemo = std::filesystem::exists(fl.rootPath / "demo" / "FrozenLife_Demo.exe") ||
                                   std::filesystem::exists(fl.rootPath / "demo" / "run_demo.bat");
            fl.demoExePath = fl.rootPath / "demo" / "FrozenLife_Demo.exe";
            list.insert(list.begin(), fl);
            seenIds.insert("frozen_life");
        }

        // 2. Scan user projects directory Documents/LabGames/
        if (std::filesystem::exists(m_userProjectsRoot)) {
            for (const auto& entry : std::filesystem::directory_iterator(m_userProjectsRoot)) {
                if (entry.is_directory()) {
                    parseProject(entry.path(), false);
                }
            }
        }

        return list;
    }

    bool ProjectManager::launchProjectDemo(const std::string& projectId) {
        auto projects = getAllDiscoveredProjects();
        const ProjectInfo* target = nullptr;
        for (const auto& p : projects) {
            if (p.id == projectId) {
                target = &p;
                break;
            }
        }

        if (!target) return false;

        // 1. If pre-built standalone demo exists, launch it directly!
        if (target->hasStandaloneDemo && !target->demoExePath.empty() && std::filesystem::exists(target->demoExePath)) {
            std::string ext = target->demoExePath.extension().string();
            if (ext == ".bat") {
#if defined(_WIN32)
                STARTUPINFOA si;
                PROCESS_INFORMATION pi;
                ZeroMemory(&si, sizeof(si));
                si.cb = sizeof(si);
                ZeroMemory(&pi, sizeof(pi));
                std::string dirStr = std::filesystem::absolute(target->demoExePath.parent_path()).string();
                std::string cmd = "cmd.exe /c \"" + std::filesystem::absolute(target->demoExePath).string() + "\"";
                std::vector<char> buf(cmd.begin(), cmd.end());
                buf.push_back('\0');
                if (CreateProcessA(NULL, buf.data(), NULL, NULL, FALSE, 0, NULL, dirStr.c_str(), &si, &pi)) {
                    CloseHandle(pi.hProcess);
                    CloseHandle(pi.hThread);
                    return true;
                }
#endif
            } else {
                return ExportPipeline::launchGame(target->demoExePath);
            }
        }

        // 2. Check for LabPlayer runner with game.config
        std::filesystem::path cfgPath = target->rootPath / "game.config";
        std::vector<std::filesystem::path> runners = {
            m_engineRoot / "build" / "Release" / "LabPlayer.exe",
            m_engineRoot / "build" / "Debug" / "LabPlayer.exe",
            m_engineRoot / "LabPlayer.exe"
        };
        for (const auto& runner : runners) {
            if (std::filesystem::exists(runner)) {
                std::string args = "--config \"" + cfgPath.string() + "\"";
                return ExportPipeline::launchGame(runner, args);
            }
        }

        // 3. Fallback: Run quick test build via ExportPipeline
        std::string scn = target->defaultScene.empty() ? "assets/maps/main.labmap" : target->defaultScene;
        return ExportPipeline::buildAndPrepareTestGame(target->id, scn).success;
    }

    std::vector<std::string> ProjectManager::listUserProjects() const {
        std::vector<std::string> projects;
        if (!std::filesystem::exists(m_userProjectsRoot)) return projects;

        for (const auto& entry : std::filesystem::directory_iterator(m_userProjectsRoot)) {
            if (entry.is_directory()) {
                std::string name = entry.path().filename().string();
                if (std::filesystem::exists(entry.path() / "game.config") ||
                    std::filesystem::exists(entry.path() / "project.labproject") ||
                    std::filesystem::exists(entry.path() / "assets")) {
                    projects.push_back(name);
                }
            }
        }
        return projects;
    }

    void ProjectManager::setupVFS() {
        auto& vfs = VFS::get();
        vfs.mount("engine", getEngineAssetsPath());
        vfs.mount("game", getActiveProjectAssetsPath());
        vfs.mount("project", getActiveProjectPath());
        m_vfsConfigured = true;
    }

    std::filesystem::path ProjectManager::resolveAssetPath(const std::string& pathOrUri) const {
        if (pathOrUri.empty()) return {};

        // 1. Virtual URI schemes
        if (pathOrUri.rfind("engine://", 0) == 0) {
            return getEngineAssetsPath() / pathOrUri.substr(9);
        }
        if (pathOrUri.rfind("game://", 0) == 0) {
            return getActiveProjectAssetsPath() / pathOrUri.substr(7);
        }
        if (pathOrUri.rfind("project://", 0) == 0) {
            return getActiveProjectPath() / pathOrUri.substr(10);
        }

        // 2. Direct absolute path
        std::filesystem::path p(pathOrUri);
        if (p.is_absolute() && std::filesystem::exists(p)) {
            return p;
        }

        // 3. Check active game project assets in Documents
        auto inGameAssets = getActiveProjectAssetsPath() / p;
        if (std::filesystem::exists(inGameAssets)) return inGameAssets;

        auto inGameRoot = getActiveProjectPath() / p;
        if (std::filesystem::exists(inGameRoot)) return inGameRoot;

        // 4. Check core engine assets (engine/assets)
        auto inEngineAssets = getEngineAssetsPath() / p;
        if (std::filesystem::exists(inEngineAssets)) return inEngineAssets;

        // Strip "assets/" prefix if present to check subfolders
        if (pathOrUri.rfind("assets/", 0) == 0 || pathOrUri.rfind("assets\\", 0) == 0) {
            std::string sub = pathOrUri.substr(7);
            auto inGameSub = getActiveProjectAssetsPath() / sub;
            if (std::filesystem::exists(inGameSub)) return inGameSub;

            auto inEngineSub = getEngineAssetsPath() / sub;
            if (std::filesystem::exists(inEngineSub)) return inEngineSub;
        }

        // 5. Legacy engine root fallback
        auto inLegacy = m_engineRoot / p;
        if (std::filesystem::exists(inLegacy)) return inLegacy;

        return p;
    }

    std::vector<std::string> ProjectManager::getProjectScripts() const {
        std::vector<std::string> scripts;
        auto scriptsDir = getActiveProjectAssetsPath() / "scripts";
        if (std::filesystem::exists(scriptsDir)) {
            for (const auto& entry : std::filesystem::directory_iterator(scriptsDir)) {
                if (entry.is_regular_file()) {
                    std::string ext = entry.path().extension().string();
                    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return (char)::tolower(c); });
                    if (ext == ".lua") {
                        scripts.push_back(entry.path().filename().string());
                    }
                }
            }
        }
        std::sort(scripts.begin(), scripts.end());
        return scripts;
    }

    ExportResult ProjectManager::exportGame(const std::string& projectName, const ExportSettings& settings) {
        ensureProjectExists(projectName);
        ExportSettings s = settings;
        s.gameTitle = projectName;
        return ExportPipeline::exportProject(s);
    }

} // namespace Lab
