#include "LabPackage.h"
#include "plugins/LuaDropinPlugin.h"
#include <iostream>
#include <filesystem>
#include <fstream>

namespace Lab {

void PackageManager::registerBuiltinPackages() {
    // Drop-in model: NO hardcoded plugins. Every plugin is exactly one .lua file
    // in assets/plugins/. Drop file = new plugin, delete file = plugin gone.
    // No engine recompile, no per-plugin C++ edits.
    scanPluginDirectory("assets/plugins");
    // First startup: enable everything discovered so editor works out of the box
    // (same as before: proc-gen, speedrun, retro, dialogue were auto-enabled).
    // Plain rescan/poll never auto-enables; only fresh files and importDropinFile do.
    for (const auto& [id, pkg] : m_packages) {
        if (pkg.state == PackageState::Discovered) enablePackage(id);
    }
}

void PackageManager::scanPluginDirectory(const std::string& directoryPath) {
    std::error_code ec;
    std::filesystem::create_directories(directoryPath, ec);
    if (!std::filesystem::exists(directoryPath)) return;

    // Index current files on disk.
    std::unordered_map<std::string, std::string> fileToId; // path -> id
    std::unordered_map<std::string, std::string> idToFile; // id -> path
    for (const auto& entry : std::filesystem::directory_iterator(directoryPath)) {
        if (!entry.is_regular_file()) continue;
        if (entry.path().extension() != ".lua") continue;
        std::string path = entry.path().string();

        std::string id, name, ver, author, cat, desc;
        if (!LuaDropinPlugin::parseManifest(path, id, name, ver, author, cat, desc)) {
            std::string stem = entry.path().stem().string();
            id = "com.lab.script." + stem;
            name = "Script: " + stem + " [ERROR: no Plugin table]";
            ver = "1.0.0";
            author = "Unknown";
            cat = "Scripting & Dev";
            desc = "Broken drop-in file (missing Plugin table): " + path;
        }
        // Last file wins on duplicate id; ids must be unique per file.
        fileToId[path] = id;
        idToFile[id] = path;

        auto it = m_packages.find(id);
        if (it == m_packages.end()) {
            PackageManifest p;
            p.id = id;
            p.displayName = name;
            p.version = ver;
            p.author = author;
            p.category = cat;
            p.description = desc;
            p.state = PackageState::Discovered;
            auto impl = std::make_shared<LuaDropinPlugin>(path);
            m_packages[id] = p;
            m_pluginImplementations[id] = impl;
            m_packageFiles[id] = path;
            LabLog::info("Drop-in plugin discovered: '" + name + "' (" + id + ") <- " + path);
        } else {
            // Refresh metadata + hot-reload when file changed.
            it->second.displayName = name;
            it->second.version = ver;
            it->second.author = author;
            it->second.category = cat;
            it->second.description = desc;
            auto implIt = m_pluginImplementations.find(id);
            if (implIt != m_pluginImplementations.end()) {
                if (auto dropin = std::dynamic_pointer_cast<LuaDropinPlugin>(implIt->second)) {
                    if (dropin->isOutOfDate()) {
                        bool wasEnabled = (it->second.state == PackageState::Enabled);
                        std::string oldName = dropin->getName(); // name BEFORE reload (may change)
                        if (wasEnabled) PluginManager::get().unregisterPlugin(oldName);
                        dropin->reloadFromDisk();
                        if (wasEnabled) PluginManager::get().registerPlugin(dropin);
                        LabLog::info("Drop-in plugin reloaded: '" + name + "' (" + id + ")");
                    }
                    m_packageFiles[id] = path;
                }
            }
        }
    }

    // Remove packages whose id is no longer parsed from any file on disk
    // (user deleted the single file, or changed its id = full control, no recompile).
    std::vector<std::string> toRemove;
    for (const auto& [id, file] : m_packageFiles) {
        if (idToFile.find(id) == idToFile.end()) {
            toRemove.push_back(id);
        }
    }
    for (const auto& id : toRemove) {
        LabLog::info("Drop-in plugin removed (file deleted): " + id);
        removePackage(id);
    }

    std::filesystem::file_time_type newest{};
    bool hasNewest = false;
    for (const auto& [path, id] : fileToId) {
        auto t = std::filesystem::last_write_time(path, ec);
        if (!ec && (!hasNewest || t > newest)) { newest = t; hasNewest = true; }
    }
    if (hasNewest) { m_lastDirScan = newest; m_hasDirScan = true; }
}

std::string PackageManager::importDropinFile(const std::string& sourcePath,
                                            const std::string& directoryPath) {
    std::error_code ec;
    std::filesystem::create_directories(directoryPath, ec);
    std::filesystem::path src(sourcePath);
    if (!std::filesystem::exists(src)) return "";
    if (src.extension() != ".lua") return "";

    std::filesystem::path dst = std::filesystem::path(directoryPath) / src.filename();
    // Drag&drop onto own folder: nothing to copy, just scan + enable.
    if (std::filesystem::equivalent(src, dst, ec) == false) {
        std::filesystem::copy_file(src, dst,
            std::filesystem::copy_options::overwrite_existing, ec);
        if (ec) {
            LabLog::error("importDropinFile: copy failed: " + ec.message());
            return "";
        }
    }
    scanPluginDirectory(directoryPath);

    // Find id by file and auto-enable so dropped file works instantly.
    for (const auto& [id, file] : m_packageFiles) {
        if (file == dst.string()) {
            enablePackage(id);
            return id;
        }
    }
    return "";
}

bool PackageManager::removePackage(const std::string& packageId) {
    auto it = m_packages.find(packageId);
    if (it == m_packages.end()) return false;
    auto implIt = m_pluginImplementations.find(packageId);
    if (implIt != m_pluginImplementations.end() && implIt->second) {
        PluginManager::get().unregisterPlugin(implIt->second->getName());
        m_pluginImplementations.erase(implIt);
    }
    m_packageFiles.erase(packageId);
    m_packages.erase(it);
    return true;
}

bool PackageManager::deleteDropinPlugin(const std::string& packageId) {
    auto fit = m_packageFiles.find(packageId);
    if (fit == m_packageFiles.end()) return false; // hardcoded or unknown: refuse
    std::string path = fit->second;
    removePackage(packageId);
    std::error_code ec;
    std::filesystem::remove(path, ec);
    if (ec) {
        LabLog::error("deleteDropinPlugin: cannot delete " + path + ": " + ec.message());
        return false;
    }
    LabLog::info("Drop-in plugin deleted: " + packageId + " (" + path + ")");
    return true;
}

std::string PackageManager::getPackageFile(const std::string& packageId) const {
    auto it = m_packageFiles.find(packageId);
    return it != m_packageFiles.end() ? it->second : std::string{};
}

void PackageManager::pollDropinChanges(const std::string& directoryPath) {
    std::error_code ec;
    if (!std::filesystem::exists(directoryPath)) return;
    std::filesystem::file_time_type newest{};
    bool found = false;
    size_t luaCount = 0;
    for (const auto& entry : std::filesystem::directory_iterator(directoryPath)) {
        if (!entry.is_regular_file() || entry.path().extension() != ".lua") continue;
        ++luaCount;
        auto t = std::filesystem::last_write_time(entry.path(), ec);
        if (!ec && (!found || t > newest)) { newest = t; found = true; }
    }
    if (!found) {
        if (!m_packageFiles.empty()) scanPluginDirectory(directoryPath); // all deleted
        return;
    }
    if (!m_hasDirScan || newest != m_lastDirScan || luaCount != m_packageFiles.size()) {
        scanPluginDirectory(directoryPath);
    } else {
        // Same count + same newest mtime: still check per-file reload (cheap, edit in place).
        for (const auto& [id, file] : m_packageFiles) {
            auto implIt = m_pluginImplementations.find(id);
            if (implIt == m_pluginImplementations.end()) continue;
            if (auto dropin = std::dynamic_pointer_cast<LuaDropinPlugin>(implIt->second)) {
                if (dropin->isOutOfDate()) { scanPluginDirectory(directoryPath); break; }
            }
        }
    }
}

} // namespace Lab
