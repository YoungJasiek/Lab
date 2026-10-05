#pragma once
#include <string>
#include <unordered_map>
#include <filesystem>
#include <vector>
#include <cstdint>

namespace Lab {

    class VFS {
    public:
        static VFS& get();

        // Mount a virtual protocol/prefix, e.g. mount("assets", "assets/") or mount("engine", "engine/")
        void mount(const std::string& scheme, const std::filesystem::path& physicalPath);
        void unmount(const std::string& scheme);

        // Resolve virtual URI ("assets://textures/wall.png" or "assets:/textures/wall.png") to physical path
        std::filesystem::path resolve(const std::string& uri) const;
        bool exists(const std::string& uri) const;
        bool readBinary(const std::string& uri, std::vector<uint8_t>& outData) const;
        bool readText(const std::string& uri, std::string& outText) const;
        void clear();

    private:
        VFS() = default;
        std::unordered_map<std::string, std::filesystem::path> _mounts;
    };

} // namespace Lab
