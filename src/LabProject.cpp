#include "LabProject.h"
#include "LabExportPipeline.h"
#include "LabVFS.h"
#include <iostream>
#include <fstream>
#include <cstdlib>

#if defined(_WIN32)
#include <windows.h>
#include <shlobj.h>
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

        // Ensure default game exists in user documents
        ensureProjectExists("LabRunner_Escape");
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

    bool ProjectManager::createProject(const std::string& projectName, const std::string& templateName) {
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

        // Populate metadata project.labproject
        std::filesystem::path projectMeta = projDir / "project.labproject";
        if (!std::filesystem::exists(projectMeta)) {
            std::ofstream pm(projectMeta);
            pm << "{\n"
               << "  \"name\": \"" << projectName << "\",\n"
               << "  \"version\": \"1.0.0\",\n"
               << "  \"engine\": \"Lab Engine Studio 2026.10\",\n"
               << "  \"author\": \"Developer\",\n"
               << "  \"entryScene\": \"assets/maps/main.labmap\"\n"
               << "}\n";
            pm.close();
        }

        // Populate game.config
        std::filesystem::path configPath = projDir / "game.config";
        if (!std::filesystem::exists(configPath)) {
            std::ofstream cfg(configPath);
            cfg << "title=" << projectName << "\n"
                << "scene=assets/maps/main.labmap\n"
                << "width=1280\n"
                << "height=720\n"
                << "fullscreen=0\n";
            cfg.close();
        }

        // Copy template if requested
        if (templateName == "labrunner" || projectName == "LabRunner_Escape") {
            syncGameToDocuments(projectName);
        } else {
            // Generate basic default map if none exists
            std::filesystem::path defaultMap = assetsDir / "maps" / "main.labmap";
            if (!std::filesystem::exists(defaultMap)) {
                std::ofstream dm(defaultMap);
                dm << "MAP_VERSION 1\n"
                   << "MAP_NAME " << projectName << " Starter Map\n"
                   << "SPAWN 0.0 1.8 0.0 0.0\n"
                   << "LIGHT 0.0 6.0 0.0 1.0 1.0 1.0 20.0\n"
                   << "BRUSH cube 0.0 -0.25 0.0 16.0 0.5 16.0 dev_gray.bmp 0.25 0.25\n";
                dm.close();
            }
        }

        return true;
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
        std::filesystem::path projDir = m_userProjectsRoot / projectName;
        if (!std::filesystem::exists(projDir)) {
            return createProject(projectName);
        }
        return true;
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

    ExportResult ProjectManager::exportGame(const std::string& projectName, const ExportSettings& settings) {
        ensureProjectExists(projectName);
        ExportSettings s = settings;
        s.gameTitle = projectName;
        return ExportPipeline::exportProject(s);
    }

} // namespace Lab
