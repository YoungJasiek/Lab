#include "LabVFS.h"
#include <fstream>

namespace Lab {

    VFS& VFS::get() {
        static VFS instance;
        return instance;
    }

    void VFS::mount(const std::string& scheme, const std::filesystem::path& physicalPath) {
        _mounts[scheme] = physicalPath;
    }

    void VFS::unmount(const std::string& scheme) {
        _mounts.erase(scheme);
    }

    std::filesystem::path VFS::resolve(const std::string& uri) const {
        size_t protocolEnd = uri.find("://");
        size_t prefixLen = 3;
        if (protocolEnd == std::string::npos) {
            protocolEnd = uri.find(":/");
            prefixLen = 2;
        }

        auto sanitizeRelative = [](const std::string& rawRel) -> std::filesystem::path {
            std::filesystem::path relPath = std::filesystem::path(rawRel).lexically_normal();
            if (relPath.has_root_name()) {
                return {};
            }
            relPath = relPath.relative_path().lexically_normal();
            for (const auto& part : relPath) {
                if (part == "..") {
                    return {};
                }
            }
            return relPath;
        };

        if (protocolEnd == std::string::npos) {
            return sanitizeRelative(uri);
        }

        std::string scheme = uri.substr(0, protocolEnd);
        std::string relative = uri.substr(protocolEnd + prefixLen);
        std::filesystem::path safeRel = sanitizeRelative(relative);
        if (safeRel.empty() && !relative.empty()) {
            return {};
        }

        auto it = _mounts.find(scheme);
        if (it != _mounts.end()) {
            return it->second / safeRel;
        }

        return safeRel;
    }

    bool VFS::exists(const std::string& uri) const {
        std::filesystem::path p = resolve(uri);
        if (p.empty()) return false;
        return std::filesystem::exists(p);
    }

    bool VFS::readBinary(const std::string& uri, std::vector<uint8_t>& outData) const {
        std::filesystem::path p = resolve(uri);
        if (p.empty()) return false;
        std::ifstream file(p, std::ios::binary | std::ios::ate);
        if (!file.is_open()) return false;

        std::streamsize size = file.tellg();
        if (size < 0) return false;
        file.seekg(0, std::ios::beg);
        outData.resize(static_cast<size_t>(size));
        if (file.read(reinterpret_cast<char*>(outData.data()), size)) {
            return true;
        }
        return false;
    }

    bool VFS::readText(const std::string& uri, std::string& outText) const {
        std::filesystem::path p = resolve(uri);
        if (p.empty()) return false;
        std::ifstream file(p);
        if (!file.is_open()) return false;
        outText.assign((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        return true;
    }

    void VFS::clear() {
        _mounts.clear();
    }

} // namespace Lab
