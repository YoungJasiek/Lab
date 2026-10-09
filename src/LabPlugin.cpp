#include "LabPlugin.h"
#include <algorithm>
#include <iostream>
#include <filesystem>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace Lab {

PluginManager& PluginManager::get() {
    static PluginManager s_instance;
    return s_instance;
}

PluginManager::~PluginManager() {
    unloadAll();
}

bool PluginManager::registerPlugin(std::shared_ptr<ILabPlugin> plugin) {
    if (!plugin) {
        LabLog::warn("PluginManager: Attempted to register null plugin pointer.");
        return false;
    }

    const std::string name = plugin->getName() ? plugin->getName() : "";
    if (name.empty()) {
        LabLog::warn("PluginManager: Plugin name cannot be empty.");
        return false;
    }

    // Check for duplicate plugin
    for (const auto& p : m_plugins) {
        if (p && name == p->getName()) {
            LabLog::warn("PluginManager: Plugin '" + name + "' is already registered.");
            return false;
        }
    }

    LabLog::info("PluginManager: Loading plugin '" + name + "' v" + (plugin->getVersion() ? plugin->getVersion() : "1.0"));
    if (!plugin->onLoad()) {
        LabLog::error("PluginManager: Plugin '" + name + "' failed onLoad() initialization.");
        return false;
    }

    m_plugins.push_back(plugin);
    LabLog::info("PluginManager: Plugin '" + name + "' successfully loaded (Total active: " + std::to_string(m_plugins.size()) + ").");
    return true;
}

bool PluginManager::loadDynamicPlugin(const std::string& libraryPath) {
    if (!std::filesystem::exists(libraryPath)) {
        LabLog::warn("PluginManager: Plugin library not found: " + libraryPath);
        return false;
    }

#if defined(_WIN32)
    HMODULE handle = LoadLibraryA(libraryPath.c_str());
    if (!handle) {
        LabLog::error("PluginManager: Failed to LoadLibraryA: " + libraryPath);
        return false;
    }
    CreatePluginFn createFn = (CreatePluginFn)GetProcAddress(handle, "CreatePlugin");
    if (!createFn) {
        LabLog::error("PluginManager: 'CreatePlugin' export not found in: " + libraryPath);
        FreeLibrary(handle);
        return false;
    }
#else
    void* handle = dlopen(libraryPath.c_str(), RTLD_NOW);
    if (!handle) {
        const char* err = dlerror();
        LabLog::error("PluginManager: Failed to dlopen " + libraryPath + ": " + (err ? err : "unknown error"));
        return false;
    }
    CreatePluginFn createFn = (CreatePluginFn)dlsym(handle, "CreatePlugin");
    if (!createFn) {
        LabLog::error("PluginManager: 'CreatePlugin' symbol not found in: " + libraryPath);
        dlclose(handle);
        return false;
    }
#endif

    ILabPlugin* rawPlugin = createFn();
    if (!rawPlugin) {
        LabLog::error("PluginManager: CreatePlugin() returned null from: " + libraryPath);
#if defined(_WIN32)
        FreeLibrary(handle);
#else
        dlclose(handle);
#endif
        return false;
    }

    if (rawPlugin->getApiVersion() != LAB_PLUGIN_API_VERSION) {
        LabLog::warn("PluginManager: Warning - Plugin '" + std::string(rawPlugin->getName()) + "' API version mismatch.");
    }

    std::shared_ptr<ILabPlugin> pluginPtr(rawPlugin, [handle](ILabPlugin* p) {
        delete p;
#if defined(_WIN32)
        if (handle) FreeLibrary(handle);
#else
        if (handle) dlclose(handle);
#endif
    });

    return registerPlugin(pluginPtr);
}

size_t PluginManager::loadPluginsFromDirectory(const std::string& directoryPath) {
    if (!std::filesystem::exists(directoryPath)) {
        return 0;
    }

    size_t count = 0;
    std::string expectedExt = 
#if defined(_WIN32)
        ".dll";
#elif defined(__APPLE__)
        ".dylib";
#else
        ".so";
#endif

    try {
        for (const auto& entry : std::filesystem::directory_iterator(directoryPath)) {
            if (entry.is_regular_file() && entry.path().extension() == expectedExt) {
                if (loadDynamicPlugin(entry.path().string())) {
                    count++;
                }
            }
        }
    } catch (const std::exception& e) {
        LabLog::warn("PluginManager: Error scanning directory " + directoryPath + ": " + e.what());
    }

    return count;
}

void PluginManager::unregisterPlugin(const std::string& name) {
    auto it = std::find_if(m_plugins.begin(), m_plugins.end(), [&](const std::shared_ptr<ILabPlugin>& p) {
        return p && name == p->getName();
    });

    if (it != m_plugins.end()) {
        LabLog::info("PluginManager: Unloading plugin '" + name + "'...");
        (*it)->onUnload();
        m_plugins.erase(it);
    }
}

void PluginManager::updateAll(float dt) {
    for (auto& plugin : m_plugins) {
        if (plugin) {
            plugin->onUpdate(dt);
        }
    }
}

void PluginManager::unloadAll() {
    if (m_plugins.empty()) return;

    LabLog::info("PluginManager: Unloading all " + std::to_string(m_plugins.size()) + " plugins in reverse registration order...");
    for (auto it = m_plugins.rbegin(); it != m_plugins.rend(); ++it) {
        if (*it) {
            LabLog::info("PluginManager: Unloading plugin '" + std::string((*it)->getName()) + "'...");
            (*it)->onUnload();
        }
    }
    m_plugins.clear();
}

size_t PluginManager::getPluginCount() const {
    return m_plugins.size();
}

std::shared_ptr<ILabPlugin> PluginManager::getPlugin(const std::string& name) {
    for (const auto& p : m_plugins) {
        if (p && name == p->getName()) {
            return p;
        }
    }
    return nullptr;
}

} // namespace Lab
