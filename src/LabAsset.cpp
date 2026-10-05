#include "LabAsset.h"
#include <sstream>
#include <iomanip>
#include <random>
#include <algorithm>

namespace Lab {

    AssetID AssetID::generate() {
        static std::random_device rd;
        static std::mt19937_64 gen(rd());
        static std::uniform_int_distribution<uint64_t> dis;
        AssetID id;
        id.high = dis(gen);
        id.low = dis(gen);
        if (id.high == 0 && id.low == 0) id.high = 1;
        return id;
    }

    AssetID AssetID::fromString(const std::string& str) {
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

    std::string AssetID::toString() const {
        std::stringstream ss;
        ss << std::hex << std::setfill('0')
           << std::setw(16) << high
           << std::setw(16) << low;
        std::string s = ss.str();
        return s.substr(0, 8) + "-" + s.substr(8, 4) + "-" + s.substr(12, 4) + "-" + s.substr(16, 4) + "-" + s.substr(20, 12);
    }

    AssetRegistry& AssetRegistry::get() {
        static AssetRegistry instance;
        return instance;
    }

    void AssetRegistry::registerAsset(const AssetMetadata& meta) {
        _byID[meta.id] = meta;
        _byPath[meta.virtualPath] = meta.id;
    }

    bool AssetRegistry::unregisterAsset(const AssetID& id) {
        auto it = _byID.find(id);
        if (it == _byID.end()) return false;
        _byPath.erase(it->second.virtualPath);
        _byID.erase(it);
        return true;
    }

    const AssetMetadata* AssetRegistry::getMetadata(const AssetID& id) const {
        auto it = _byID.find(id);
        if (it != _byID.end()) return &it->second;
        return nullptr;
    }

    AssetID AssetRegistry::findByPath(const std::string& virtualPath) const {
        auto it = _byPath.find(virtualPath);
        if (it != _byPath.end()) return it->second;
        return AssetID{};
    }

    std::filesystem::path AssetRegistry::resolveAssetPath(const AssetID& id) const {
        const AssetMetadata* meta = getMetadata(id);
        if (!meta) return {};
        return VFS::get().resolve(meta->virtualPath);
    }

} // namespace Lab
