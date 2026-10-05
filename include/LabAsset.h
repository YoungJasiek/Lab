#pragma once
#include <string>
#include <cstdint>
#include <unordered_map>
#include <filesystem>
#include "LabVFS.h"

namespace Lab {

    enum class AssetType {
        Unknown,
        Texture,
        Mesh,
        Audio,
        Shader,
        Prefab,
        Map,
        Script
    };

    struct AssetID {
        uint64_t high{0};
        uint64_t low{0};

        bool isValid() const { return high != 0 || low != 0; }

        bool operator==(const AssetID& other) const {
            return high == other.high && low == other.low;
        }

        bool operator!=(const AssetID& other) const {
            return !(*this == other);
        }

        static AssetID generate();
        static AssetID fromString(const std::string& str);
        std::string toString() const;
    };

} // namespace Lab

namespace std {
    template<>
    struct hash<Lab::AssetID> {
        size_t operator()(const Lab::AssetID& id) const noexcept {
            return static_cast<size_t>(id.high ^ (id.low + 0x9e3779b97f4a7c15ULL + (id.high << 6) + (id.high >> 2)));
        }
    };
}

namespace Lab {

    struct AssetMetadata {
        AssetID id;
        AssetType type{AssetType::Unknown};
        std::string virtualPath; // e.g. "assets://textures/player.png"
        std::string name;
        uint64_t fileSize{0};
    };

    class AssetRegistry {
    public:
        static AssetRegistry& get();

        void registerAsset(const AssetMetadata& meta);
        bool unregisterAsset(const AssetID& id);
        const AssetMetadata* getMetadata(const AssetID& id) const;
        AssetID findByPath(const std::string& virtualPath) const;
        std::filesystem::path resolveAssetPath(const AssetID& id) const;

        size_t getAssetCount() const { return _byID.size(); }
        void clear() { _byID.clear(); _byPath.clear(); }

    private:
        AssetRegistry() = default;
        std::unordered_map<AssetID, AssetMetadata> _byID;
        std::unordered_map<std::string, AssetID> _byPath;
    };

} // namespace Lab
