#pragma once
#include <string>
#include <vector>
#include <filesystem>
#include <memory>
#include "LabExport.h"
#include "LabExportPipeline.h"
#include "LabVFS.h"

namespace Lab {

    struct ProjectDisplaySettings {
        int width{1280};
        int height{720};
        bool fullscreen{false};
        bool vsync{true};
    };

    struct ProjectInfo {
        std::string id;
        std::string displayName;
        std::string dimension{"3D"}; // "2D" or "3D"
        ProjectDisplaySettings display;
        std::filesystem::path rootPath;
        std::filesystem::path assetsPath;
        std::string defaultScene;
        std::string defaultHud;
        std::string version{"1.0.0"};
        std::string author{"Developer"};
        std::string description;
        bool isBuiltinShowcase{false};
        bool hasStandaloneDemo{false};
        std::filesystem::path demoExePath;
        std::string lastModified{"Recent"};
    };

    /**
     * @brief ProjectManager separates Engine Core Assets from User Game Projects
     * and powers the Engine Main Menu / Project Hub.
     */
    class LAB_CORE_API ProjectManager {
    public:
        static ProjectManager& get();

        /// @brief Physical root of the Lab engine repository (where CMakeLists.txt resides)
        std::filesystem::path getEngineRoot() const;

        /// @brief Physical directory of core engine assets (<EngineRoot>/engine/assets)
        std::filesystem::path getEngineAssetsPath() const;

        /// @brief Root folder for user game projects, defaulting to <Documents>/LabGames/
        std::filesystem::path getUserProjectsRoot() const;

        /// @brief Override custom projects directory (e.g. via config or environment)
        void setUserProjectsRoot(const std::filesystem::path& path);

        /// @brief Currently active project name
        std::string getActiveProjectName() const;

        /// @brief Path to the currently active project folder
        std::filesystem::path getActiveProjectPath() const;

        /// @brief Path to the currently active project's assets folder
        std::filesystem::path getActiveProjectAssetsPath() const;

        /// @brief Switch active game project
        bool setActiveProject(const std::string& projectName);

        /// @brief Create a brand new decoupled game project with 2D/3D dimension, display config, and clean starter scene
        bool createProject(const std::string& projectName, const std::string& dimension = "3D", int width = 1280, int height = 720, bool fullscreen = false);

        /// @brief Save updated project settings to project.labproject and game.config
        bool saveProjectSettings(const std::string& projectName, const ProjectInfo& info);

        /// @brief Retrieve parsed project info for a given project ID
        ProjectInfo getProjectInfo(const std::string& projectName) const;

        /// @brief Ensure project exists in Documents; creates folder structure and template if missing
        bool ensureProjectExists(const std::string& projectName);

        /// @brief List all project directory names found in User Documents/LabGames/
        std::vector<std::string> listUserProjects() const;

        /// @brief Discovers all projects across both games/ and Documents/LabGames/ with full metadata
        std::vector<ProjectInfo> getAllDiscoveredProjects() const;

        /// @brief Launches the standalone game or pre-built demo for the specified project
        bool launchProjectDemo(const std::string& projectId);

        /// @brief Automatically syncs / populates a project template to Documents without polluting CMake
        void syncGameToDocuments(const std::string& projectName);

        /// @brief Mounts VFS schemes ("engine" -> engine/assets, "game" -> active game assets)
        void setupVFS();

        /// @brief Resolves virtual URI ("engine://...", "game://...", "project://...") or relative path
        std::filesystem::path resolveAssetPath(const std::string& pathOrUri) const;

        /// @brief List all Lua script file names located strictly within active project's game://scripts (clean isolation)
        std::vector<std::string> getProjectScripts() const;

        /// @brief Build & export the given game project automatically to standalone output directory
        ExportResult exportGame(const std::string& projectName, const ExportSettings& settings);

    private:
        ProjectManager();
        ~ProjectManager() = default;

        std::filesystem::path m_engineRoot;
        std::filesystem::path m_userProjectsRoot;
        std::string m_activeProjectName{"frozen_life"};
        bool m_vfsConfigured{false};
    };

} // namespace Lab
