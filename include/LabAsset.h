#pragma once
#include <string>
#include <cstdint>
#include <unordered_map>
#include <sstream>
#include <iomanip>
#include <random>
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

        static AssetID generate() {
            static std::random_device rd;
            static std::mt19937_64 gen(rd());
            static std::uniform_int_distribution<uint64_t> dis;
            AssetID id;
            id.high = dis(gen);
            id.low = dis(gen);
            // Ensure non-zero
            if (id.high == 0 && id.low == 0) id.high = 1;
            return id;
        }

        static AssetID fromString(const std::string& str) {
            if (str.length() < 32) return AssetID{};
            AssetID id;
            std::string clean = str;
            clean.erase(std::remove(clean.begin(), clean.end(), '-'), clean.end());
            if (clean.length() != 32) return AssetID{};

            std::stringstream ssHigh(clean.substr(0, 16));
            std::stringstream ssLow(clean.substr(16, 16));
            ssHigh >> std::hex >> id.high;
            ssLow >> std::hex >> id.low;
            return id;
        }

        std::string toString() const {
            std::stringstream ss;
            ss << std::hex << std::setfill('0')
               << std::setw(16) << high
               << std::setw(16) << low;
            std::string s = ss.str();
            // Format 8-4-4-4-12 standard GUID style
            return s.substr(0, 8) + "-" + s.substr(8, 4) + "-" + s.substr(12, 4) + "-" + s.substr(16, 4) + "-" + s.substr(20, 12);
        }
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
        static AssetRegistry& get() {
            static AssetRegistry instance;
            return instance;
        }

        void registerAsset(const AssetMetadata& meta) {
            _byID[meta.id] = meta;
            _byPath[meta.virtualPath] = meta.id;
        }

        bool unregisterAsset(const AssetID& id) {
            auto it = _byID.find(id);
            if (it == _byID.end()) return false;
            _byPath.erase(it->second.virtualPath);
            _byID.erase(it);
            return true;
        }

        const AssetMetadata* getMetadata(const AssetID& id) const {
            auto it = _byID.find(id);
            if (it != _byID.end()) return &it->second;
            return nullptr;
        }

        AssetID findByPath(const std::string& virtualPath) const {
            auto it = _byPath.find(virtualPath);
            if (it != _byPath.end()) return it->second;
            return AssetID{};
        }

        std::filesystem::path resolveAssetPath(const AssetID& id) const {
            const AssetMetadata* meta = getMetadata(id);
            if (!meta) return {};
            return VFS::get().resolve(meta->virtualPath);
        }

        size_t getAssetCount() const { return _byID.size(); }
        void clear() { _byID.clear(); _byPath.clear(); }

    private:
        AssetRegistry() = default;
        std::unordered_map<AssetID, AssetMetadata> _byID;
        std::unordered_map<std::string, AssetID> _byPath;
    };

} // namespace Lab
