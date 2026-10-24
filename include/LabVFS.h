#pragma once
#include <string>
#include <unordered_map>
#include <filesystem>
#include <vector>
#include <cstdint>
#include <memory>

namespace Lab {

    class LabPak;

    struct AddonManifest {
        std::string folderName;
        std::string title = "Custom Addon";
        std::string version = "1.0";
        std::string author = "Community";
        std::string description = "Custom addon package for Lab Engine.";
        std::string startupScript = "";
        bool enabled = true;
        int mapCount = 0;
    };

    class VFS {
    public:
        static VFS& get();

        // Mount a virtual protocol/prefix, e.g. mount("assets", "assets/") or mount("engine", "engine/")
        void mount(const std::string& scheme, const std::filesystem::path& physicalPath);
        void unmount(const std::string& scheme);

        // Layered /addons/<name>/ mounting (highest priority overrides over base assets/)
        void mountAddonLayer(const std::filesystem::path& addonDir);
        void unmountAddonLayer(const std::filesystem::path& addonDir);
        void clearAddonLayers() { _addonLayers.clear(); }
        const std::vector<std::filesystem::path>& getAddonLayers() const { return _addonLayers; }
        static std::vector<AddonManifest> discoverAddons(const std::filesystem::path& addonsRoot = "addons");

        // Security & Malware Sanitizer for Media/Addon Files (Magic Bytes verification)
        static bool validateMediaBufferSecurity(const std::vector<uint8_t>& data, const std::string& extension, std::string& outReason);
        static bool validateMediaFileSecurity(const std::filesystem::path& path, std::string& outReason);

        // Mount / unmount .lpak virtual asset packages
        bool mountPak(const std::filesystem::path& pakPath);
        void unmountPak(const std::filesystem::path& pakPath);
        const std::vector<std::shared_ptr<LabPak>>& getMountedPaks() const { return _paks; }

        // Resolve virtual URI ("assets://textures/wall.png" or "assets:/textures/wall.png") to physical path
        std::filesystem::path resolve(const std::string& uri) const;
        bool exists(const std::string& uri) const;
        bool readBinary(const std::string& uri, std::vector<uint8_t>& outData) const;
        bool readText(const std::string& uri, std::string& outText) const;
        void clear();

    private:
        VFS() = default;
        std::unordered_map<std::string, std::filesystem::path> _mounts;
        std::vector<std::filesystem::path> _addonLayers;
        std::vector<std::shared_ptr<LabPak>> _paks;
    };

} // namespace Lab
