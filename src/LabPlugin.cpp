#include "LabPlugin.h"
#include <algorithm>
#include <iostream>

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
