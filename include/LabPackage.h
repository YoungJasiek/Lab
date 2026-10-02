#pragma once

#include "LabPlugin.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <algorithm>

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
        std::vector<std::string> dependencies; // package IDs required
        PackageState state{PackageState::Discovered};
    };

    class PackageManager {
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
        }

    private:
        PackageManager() = default;
        std::unordered_map<std::string, PackageManifest> m_packages;
        std::unordered_map<std::string, std::shared_ptr<ILabPlugin>> m_pluginImplementations;
    };

} // namespace Lab
