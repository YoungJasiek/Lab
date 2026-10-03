#pragma once
#include <string>
#include <vector>
#include <filesystem>
#include <memory>
#include "LabExport.h"
#include "LabExportPipeline.h"
#include "LabVFS.h"

namespace Lab {

    struct ProjectInfo {
        std::string name;
        std::filesystem::path rootPath;
        std::filesystem::path assetsPath;
        std::string defaultScene;
        std::string version{"1.0.0"};
        std::string author{"Developer"};
    };

    /**
     * @brief ProjectManager separates Engine Core Assets from User Game Projects.
     * 
     * In Development / Production:
     * - Engine Core Assets live in <EngineRoot>/engine/assets/ (shaders, editor fonts, fallback textures, gizmos).
     * - User Games & Assets live in User Documents (<Documents>/LabGames/<GameTitle>/assets/),
     *   keeping CMake & the engine repository completely clean of game assets.
     * 
     * On One-Click Build & Export:
     * - The pipeline automatically assembles a standalone, self-contained game structure in the target directory:
     *   <outputDirectory>/<GameTitle>_<Platform>/
     *   ├── <GameTitle>.exe (or linux runner / WASM canvas)
     *   ├── *.dll (engine dynamic libraries)
     *   ├── game.config
     *   ├── game.manifest
     *   ├── run_game.bat / .sh
     *   └── assets/ (only game assets + compiled .luac scripts)
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

        /// @brief Path to the currently active project folder in Documents
        std::filesystem::path getActiveProjectPath() const;

        /// @brief Path to the currently active project's assets folder in Documents
        std::filesystem::path getActiveProjectAssetsPath() const;

        /// @brief Switch active game project
        bool setActiveProject(const std::string& projectName);

        /// @brief Create a brand new decoupled game project in Documents/LabGames/<projectName>
        bool createProject(const std::string& projectName, const std::string& templateName = "default");

        /// @brief Ensure project exists in Documents; creates folder structure and template if missing
        bool ensureProjectExists(const std::string& projectName);

        /// @brief List all projects found in User Documents/LabGames/
        std::vector<std::string> listUserProjects() const;

        /// @brief Automatically syncs / populates a project template to Documents without polluting CMake
        void syncGameToDocuments(const std::string& projectName);

        /// @brief Mounts VFS schemes ("engine" -> engine/assets, "game" -> active game assets)
        void setupVFS();

        /// @brief Resolves virtual URI ("engine://...", "game://...", "project://...") or relative path
        std::filesystem::path resolveAssetPath(const std::string& pathOrUri) const;

        /// @brief Build & export the given game project automatically to standalone output directory
        ExportResult exportGame(const std::string& projectName, const ExportSettings& settings);

    private:
        ProjectManager();
        ~ProjectManager() = default;

        std::filesystem::path m_engineRoot;
        std::filesystem::path m_userProjectsRoot;
        std::string m_activeProjectName{"LabRunner_Escape"};
        bool m_vfsConfigured{false};
    };

} // namespace Lab
