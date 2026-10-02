#pragma once
#include <string>
#include <unordered_map>
#include <filesystem>
#include <fstream>
#include <vector>
#include <cstdint>

namespace Lab {

    class VFS {
    public:
        static VFS& get() {
            static VFS instance;
            return instance;
        }

        // Mount a virtual protocol/prefix, e.g. mount("assets", "assets/") or mount("engine", "engine/")
        void mount(const std::string& scheme, const std::filesystem::path& physicalPath) {
            _mounts[scheme] = physicalPath;
        }

        void unmount(const std::string& scheme) {
            _mounts.erase(scheme);
        }

        // Resolve virtual URI ("assets://textures/wall.png" or "assets:/textures/wall.png") to physical path
        std::filesystem::path resolve(const std::string& uri) const {
            size_t protocolEnd = uri.find("://");
            size_t prefixLen = 3;
            if (protocolEnd == std::string::npos) {
                protocolEnd = uri.find(":/");
                prefixLen = 2;
            }

            if (protocolEnd == std::string::npos) {
                // Raw physical path fallback
                return std::filesystem::path(uri);
            }

            std::string scheme = uri.substr(0, protocolEnd);
            std::string relative = uri.substr(protocolEnd + prefixLen);

            auto it = _mounts.find(scheme);
            if (it != _mounts.end()) {
                return it->second / relative;
            }

            // If scheme not mounted, return raw relative
            return std::filesystem::path(relative);
        }

        bool exists(const std::string& uri) const {
            std::filesystem::path p = resolve(uri);
            return std::filesystem::exists(p);
        }

        bool readBinary(const std::string& uri, std::vector<uint8_t>& outData) const {
            std::filesystem::path p = resolve(uri);
            std::ifstream file(p, std::ios::binary | std::ios::ate);
            if (!file.is_open()) return false;

            std::streamsize size = file.tellg();
            file.seekg(0, std::ios::beg);
            outData.resize((size_t)size);
            if (file.read(reinterpret_cast<char*>(outData.data()), size)) {
                return true;
            }
            return false;
        }

        bool readText(const std::string& uri, std::string& outText) const {
            std::filesystem::path p = resolve(uri);
            std::ifstream file(p);
            if (!file.is_open()) return false;
            outText.assign((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
            return true;
        }

        void clear() {
            _mounts.clear();
        }

    private:
        VFS() = default;
        std::unordered_map<std::string, std::filesystem::path> _mounts;
    };

} // namespace Lab
