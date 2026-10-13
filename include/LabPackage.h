#pragma once

#include "LabPlugin.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <algorithm>
#include <filesystem>

namespace Lab {

    enum class PackageState {
        Discovered,
        Enabled,
        Disabled,
        Error
    };

    struct PackageManifest {
        std::string id;             // e.g. "com.lab.dialogue-system"
        std::string displayName;    // e.g. "Dialogue & Quest System"
        std::string version;        // e.g. "1.2.0"
        std::string author;         // e.g. "Lab Community"
        std::string description;
        std::string category{"General"}; // e.g. "3D & Geometry", "Rendering & VFX"
        std::vector<std::string> dependencies; // package IDs required
        PackageState state{PackageState::Discovered};
    };

    class LAB_CORE_API PackageManager {
    public:
        static PackageManager& instance() {
            static PackageManager inst;
            return inst;
        }

        bool registerPackage(const PackageManifest& manifest, std::shared_ptr<ILabPlugin> pluginImpl = nullptr) {
            if (m_packages.find(manifest.id) != m_packages.end()) {
                return false; // Already registered
            }
            m_packages[manifest.id] = manifest;
            if (pluginImpl) {
                m_pluginImplementations[manifest.id] = pluginImpl;
            }
            return true;
        }

        bool enablePackage(const std::string& packageId) {
            auto it = m_packages.find(packageId);
            if (it == m_packages.end()) return false;
            if (it->second.state == PackageState::Enabled) return true; // idempotent: already running

            // Check dependencies
            for (const auto& depId : it->second.dependencies) {
                auto depIt = m_packages.find(depId);
                if (depIt == m_packages.end() || depIt->second.state != PackageState::Enabled) {
                    it->second.state = PackageState::Error;
                    return false; // Missing dependency
                }
            }

            // If plugin implementation exists, load via PluginManager
            auto plugIt = m_pluginImplementations.find(packageId);
            if (plugIt != m_pluginImplementations.end() && plugIt->second) {
                if (!PluginManager::get().registerPlugin(plugIt->second)) {
                    it->second.state = PackageState::Error;
                    return false;
                }
            }

            it->second.state = PackageState::Enabled;
            return true;
        }

        bool disablePackage(const std::string& packageId) {
            auto it = m_packages.find(packageId);
            if (it == m_packages.end()) return false;

            // Check if any active package depends on this one
            for (const auto& [id, pkg] : m_packages) {
                if (pkg.state == PackageState::Enabled) {
                    for (const auto& dep : pkg.dependencies) {
                        if (dep == packageId) {
                            return false; // Cannot disable: required by an active package
                        }
                    }
                }
            }

            auto plugIt = m_pluginImplementations.find(packageId);
            if (plugIt != m_pluginImplementations.end() && plugIt->second) {
                PluginManager::get().unregisterPlugin(plugIt->second->getName());
            }

            it->second.state = PackageState::Disabled;
            return true;
        }

        bool isPackageEnabled(const std::string& packageId) const {
            auto it = m_packages.find(packageId);
            return (it != m_packages.end() && it->second.state == PackageState::Enabled);
        }

        bool togglePackage(const std::string& packageId) {
            if (isPackageEnabled(packageId)) {
                return disablePackage(packageId);
            } else {
                return enablePackage(packageId);
            }
        }

        std::shared_ptr<ILabPlugin> getPluginImplementation(const std::string& packageId) const {
            auto it = m_pluginImplementations.find(packageId);
            if (it != m_pluginImplementations.end()) return it->second;
            return nullptr;
        }

        void registerBuiltinPackages();
        void scanPluginDirectory(const std::string& directoryPath = "plugins");

        /// @brief Drop-in API: copy external .lua file into plugins dir, scan + auto-enable.
        /// @return plugin id on success, empty string on failure.
        std::string importDropinFile(const std::string& sourcePath,
                                     const std::string& directoryPath = "plugins");

        /// @brief Full removal: disable, unregister, delete file from disk.
        /// Hardcoded (non-file) plugins cannot be deleted this way and return false.
        bool deleteDropinPlugin(const std::string& packageId);

        /// @brief Unregister without touching disk (used when file was deleted externally).
        bool removePackage(const std::string& packageId);

        /// @brief File path backing a package, empty when builtin/hardcoded.
        std::string getPackageFile(const std::string& packageId) const;

        /// @brief Cheap poll: rescan only when directory mtime changed. Call every ~1s.
        void pollDropinChanges(const std::string& directoryPath = "plugins");

        std::vector<PackageManifest> filterPackages(const std::string& query, const std::string& cat = "All", const std::string& stateFilter = "All") const {
            std::vector<PackageManifest> result;
            std::string q = query;
            std::transform(q.begin(), q.end(), q.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

            for (const auto& [id, pkg] : m_packages) {
                // Category filter
                if (cat != "All" && pkg.category != cat) {
                    continue;
                }
                // State filter: All, Enabled, Disabled
                if (stateFilter == "Enabled" && pkg.state != PackageState::Enabled) {
                    continue;
                }
                if (stateFilter == "Disabled" && pkg.state == PackageState::Enabled) {
                    continue;
                }
                // Text search query in title, id, author, description
                if (!q.empty()) {
                    std::string title = pkg.displayName;
                    std::string pid = pkg.id;
                    std::string auth = pkg.author;
                    std::string desc = pkg.description;
                    auto toLowerSafe = [](std::string& s) {
                        std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
                    };
                    toLowerSafe(title);
                    toLowerSafe(pid);
                    toLowerSafe(auth);
                    toLowerSafe(desc);

                    if (title.find(q) == std::string::npos &&
                        pid.find(q) == std::string::npos &&
                        auth.find(q) == std::string::npos &&
                        desc.find(q) == std::string::npos) {
                        continue;
                    }
                }
                result.push_back(pkg);
            }

            // Sort alphabetically by displayName
            std::sort(result.begin(), result.end(), [](const PackageManifest& a, const PackageManifest& b) {
                return a.displayName < b.displayName;
            });
            return result;
        }

        const PackageManifest* getPackage(const std::string& packageId) const {
            auto it = m_packages.find(packageId);
            if (it != m_packages.end()) {
                return &it->second;
            }
            return nullptr;
        }

        std::vector<PackageManifest> getAllPackages() const {
            std::vector<PackageManifest> list;
            for (const auto& [id, pkg] : m_packages) {
                list.push_back(pkg);
            }
            return list;
        }

        void clear() {
            m_packages.clear();
            m_pluginImplementations.clear();
            m_packageFiles.clear();
            m_hasDirScan = false;
        }

    private:
        PackageManager() = default;
        std::unordered_map<std::string, PackageManifest> m_packages;
        std::unordered_map<std::string, std::shared_ptr<ILabPlugin>> m_pluginImplementations;
        // Drop-in index: package id -> source .lua file.
        std::unordered_map<std::string, std::string> m_packageFiles;
        std::filesystem::file_time_type m_lastDirScan{};
        bool m_hasDirScan{false};
    };

} // namespace Lab
