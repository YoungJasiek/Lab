#include "LabVFS.h"
#include "LabPak.h"
#include "LabCore.h"
#include <fstream>
#include <algorithm>

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

    void VFS::mountAddonLayer(const std::filesystem::path& addonDir) {
        if (addonDir.empty()) return;
        for (const auto& existing : _addonLayers) {
            if (existing == addonDir) return;
        }
        _addonLayers.push_back(addonDir);
        std::error_code ec;
        if (std::filesystem::exists(addonDir, ec) && std::filesystem::is_directory(addonDir, ec)) {
            for (const auto& entry : std::filesystem::directory_iterator(addonDir, ec)) {
                if (entry.is_regular_file() && entry.path().extension() == ".lpak") {
                    mountPak(entry.path());
                }
            }
        }
    }

    void VFS::unmountAddonLayer(const std::filesystem::path& addonDir) {
        _addonLayers.erase(std::remove(_addonLayers.begin(), _addonLayers.end(), addonDir), _addonLayers.end());
    }

    static std::string extractJsonStringField(const std::string& json, const std::string& key, const std::string& fallback) {
        std::string pattern = "\"" + key + "\"";
        size_t pos = json.find(pattern);
        if (pos == std::string::npos) return fallback;
        size_t colon = json.find(':', pos + pattern.size());
        if (colon == std::string::npos) return fallback;
        size_t q1 = json.find('"', colon + 1);
        if (q1 == std::string::npos) return fallback;
        size_t q2 = json.find('"', q1 + 1);
        if (q2 == std::string::npos) return fallback;
        return json.substr(q1 + 1, q2 - q1 - 1);
    }

    std::vector<AddonManifest> VFS::discoverAddons(const std::filesystem::path& addonsRoot) {
        std::vector<AddonManifest> list;
        std::error_code ec;
        if (!std::filesystem::exists(addonsRoot, ec) || !std::filesystem::is_directory(addonsRoot, ec)) {
            return list;
        }
        for (const auto& entry : std::filesystem::directory_iterator(addonsRoot, ec)) {
            if (!entry.is_directory(ec)) continue;
            AddonManifest item;
            item.folderName = entry.path().filename().string();
            item.title = item.folderName;
            std::filesystem::path manifestPath = entry.path() / "addon.json";
            if (std::filesystem::exists(manifestPath, ec)) {
                std::ifstream ifs(manifestPath);
                if (ifs.is_open()) {
                    std::string content((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
                    item.title = extractJsonStringField(content, "title", item.title);
                    item.version = extractJsonStringField(content, "version", item.version);
                    item.author = extractJsonStringField(content, "author", item.author);
                    item.description = extractJsonStringField(content, "description", item.description);
                    item.startupScript = extractJsonStringField(content, "startup_script", "");
                }
            }
            if (item.startupScript.empty() && std::filesystem::exists(entry.path() / "scripts" / "startup.lua", ec)) {
                item.startupScript = "scripts/startup.lua";
            }
            for (const auto& sub : std::filesystem::recursive_directory_iterator(entry.path(), ec)) {
                if (sub.is_regular_file(ec)) {
                    auto ext = sub.path().extension().string();
                    if (ext == ".labscene" || ext == ".labmap" || ext == ".lmap") {
                        item.mapCount++;
                    }
                }
            }
            list.push_back(item);
        }
        return list;
    }

    bool VFS::validateMediaBufferSecurity(const std::vector<uint8_t>& data, const std::string& extension, std::string& outReason) {
        std::string ext = extension;
        std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        if (!ext.empty() && ext[0] != '.') ext = "." + ext;

        if (ext == ".exe" || ext == ".dll" || ext == ".bat" || ext == ".cmd" || ext == ".ps1" || ext == ".vbs" || ext == ".scr" || ext == ".com") {
            outReason = "SECURITY VIOLATION: Executable file extension (" + ext + ") is forbidden in addons.";
            return false;
        }

        if (data.size() < 4) {
            outReason = "Invalid or truncated media buffer (< 4 bytes).";
            return false;
        }

        // Reject Windows PE/DOS executables ("MZ" = 0x4D, 0x5A) or Linux ELF (0x7F, 'E', 'L', 'F') disguised as media
        if (data[0] == 0x4D && data[1] == 0x5A) {
            outReason = "SECURITY VIOLATION: PE/DOS Executable binary (MZ header) disguised as media asset!";
            return false;
        }
        if (data[0] == 0x7F && data[1] == 'E' && data[2] == 'L' && data[3] == 'F') {
            outReason = "SECURITY VIOLATION: ELF Executable binary disguised as media asset!";
            return false;
        }

        if (ext == ".mp4" || ext == ".m4v" || ext == ".mov") {
            if (data.size() < 8 || !(data[4] == 'f' && data[5] == 't' && data[6] == 'y' && data[7] == 'p')) {
                outReason = "SECURITY VIOLATION: MP4 container missing ISO Base Media 'ftyp' Magic Bytes signature.";
                return false;
            }
        } else if (ext == ".mkv" || ext == ".webm") {
            if (!(data[0] == 0x1A && data[1] == 0x45 && data[2] == 0xDF && data[3] == 0xA3)) {
                outReason = "SECURITY VIOLATION: MKV/WebM container missing Matroska EBML (0x1A45DFA3) Magic Bytes signature.";
                return false;
            }
        } else if (ext == ".wav") {
            if (data.size() < 12 || !(data[0] == 'R' && data[1] == 'I' && data[2] == 'F' && data[3] == 'F' &&
                                      data[8] == 'W' && data[9] == 'A' && data[10] == 'V' && data[11] == 'E')) {
                outReason = "SECURITY VIOLATION: WAV audio missing RIFF/WAVE Magic Bytes signature.";
                return false;
            }
        } else if (ext == ".png") {
            if (data.size() < 8 || !(data[0] == 0x89 && data[1] == 'P' && data[2] == 'N' && data[3] == 'G')) {
                outReason = "SECURITY VIOLATION: PNG image missing 0x89 'PNG' Magic Bytes signature.";
                return false;
            }
        } else if (ext == ".ogg") {
            if (!(data[0] == 'O' && data[1] == 'g' && data[2] == 'g' && data[3] == 'S')) {
                outReason = "SECURITY VIOLATION: OGG audio missing 'OggS' Magic Bytes signature.";
                return false;
            }
        }

        outReason = "OK";
        return true;
    }

    bool VFS::validateMediaFileSecurity(const std::filesystem::path& path, std::string& outReason) {
        std::string rawStr = path.generic_string();
        if (rawStr.find("..") != std::string::npos) {
            outReason = "SECURITY VIOLATION: Directory traversal ('..') detected in media path.";
            return false;
        }
        std::error_code ec;
        if (!std::filesystem::exists(path, ec) || !std::filesystem::is_regular_file(path, ec)) {
            outReason = "File does not exist.";
            return false;
        }
        std::ifstream ifs(path, std::ios::binary);
        if (!ifs.is_open()) {
            outReason = "Unable to open media file for header inspection.";
            return false;
        }
        std::vector<uint8_t> header(16, 0);
        ifs.read(reinterpret_cast<char*>(header.data()), static_cast<std::streamsize>(header.size()));
        std::streamsize bytesRead = ifs.gcount();
        header.resize(static_cast<size_t>(std::max<std::streamsize>(0, bytesRead)));
        return validateMediaBufferSecurity(header, path.extension().string(), outReason);
    }

    bool VFS::mountPak(const std::filesystem::path& pakPath) {
        if (!std::filesystem::exists(pakPath)) {
            return false;
        }

        // Avoid mounting duplicate pak paths
        for (const auto& existing : _paks) {
            if (existing && existing->getPakPath() == pakPath) {
                return true;
            }
        }

        auto pak = std::make_shared<LabPak>();
        if (!pak->open(pakPath)) {
            return false;
        }

        _paks.push_back(pak);
        LabLog::info("[VFS] Mounted asset package: " + pakPath.string() + " (" + std::to_string(pak->getEntryCount()) + " files)");
        return true;
    }

    void VFS::unmountPak(const std::filesystem::path& pakPath) {
        _paks.erase(std::remove_if(_paks.begin(), _paks.end(),
            [&](const std::shared_ptr<LabPak>& pak) {
                return pak && pak->getPakPath() == pakPath;
            }), _paks.end());
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
            std::filesystem::path safeRel = sanitizeRelative(uri);
            if (!safeRel.empty() && !_addonLayers.empty()) {
                std::string genericStr = safeRel.generic_string();
                std::filesystem::path strippedRel = safeRel;
                if (genericStr.rfind("assets/", 0) == 0) {
                    strippedRel = sanitizeRelative(genericStr.substr(7));
                }
                std::error_code ec;
                for (auto it = _addonLayers.rbegin(); it != _addonLayers.rend(); ++it) {
                    if (!strippedRel.empty() && std::filesystem::exists(*it / strippedRel, ec)) {
                        return *it / strippedRel;
                    }
                    if (std::filesystem::exists(*it / safeRel, ec)) {
                        return *it / safeRel;
                    }
                }
            }
            return safeRel;
        }

        std::string scheme = uri.substr(0, protocolEnd);
        std::string relative = uri.substr(protocolEnd + prefixLen);
        std::filesystem::path safeRel = sanitizeRelative(relative);
        if (safeRel.empty() && !relative.empty()) {
            return {};
        }

        // Priority 1: Check mounted /addons/<name>/ layers (reverse order = highest priority first)
        if (!_addonLayers.empty() && (scheme == "assets" || scheme == "addon" || scheme == "mod")) {
            std::error_code ec;
            for (auto it = _addonLayers.rbegin(); it != _addonLayers.rend(); ++it) {
                if (std::filesystem::exists(*it / safeRel, ec)) {
                    return *it / safeRel;
                }
                if (std::filesystem::exists(*it / "assets" / safeRel, ec)) {
                    return *it / "assets" / safeRel;
                }
            }
        }

        auto it = _mounts.find(scheme);
        if (it != _mounts.end()) {
            return it->second / safeRel;
        }

        return safeRel;
    }

    static std::string extractVirtualLookupKey(const std::string& uri) {
        size_t protocolEnd = uri.find("://");
        size_t prefixLen = 3;
        if (protocolEnd == std::string::npos) {
            protocolEnd = uri.find(":/");
            prefixLen = 2;
        }

        if (protocolEnd != std::string::npos) {
            std::string scheme = uri.substr(0, protocolEnd);
            std::string relative = uri.substr(protocolEnd + prefixLen);
            return scheme + "/" + relative;
        }

        return uri;
    }

    bool VFS::exists(const std::string& uri) const {
        // 1. Check physical filesystem
        std::filesystem::path p = resolve(uri);
        if (!p.empty() && std::filesystem::exists(p)) {
            return true;
        }

        // 2. Check mounted .lpak packages
        std::string key = extractVirtualLookupKey(uri);
        for (const auto& pak : _paks) {
            if (pak && (pak->hasFile(key) || pak->hasFile(uri))) {
                return true;
            }
        }

        return false;
    }

    bool VFS::readBinary(const std::string& uri, std::vector<uint8_t>& outData) const {
        // 1. Check physical filesystem first
        std::filesystem::path p = resolve(uri);
        if (!p.empty()) {
            std::ifstream file(p, std::ios::binary | std::ios::ate);
            if (file.is_open()) {
                std::streamsize size = file.tellg();
                if (size >= 0) {
                    file.seekg(0, std::ios::beg);
                    outData.resize(static_cast<size_t>(size));
                    if (file.read(reinterpret_cast<char*>(outData.data()), size)) {
                        return true;
                    }
                }
            }
        }

        // 2. Check mounted .lpak packages
        std::string key = extractVirtualLookupKey(uri);
        for (const auto& pak : _paks) {
            if (!pak) continue;
            if (pak->hasFile(key) && pak->readBinary(key, outData)) {
                return true;
            }
            if (pak->hasFile(uri) && pak->readBinary(uri, outData)) {
                return true;
            }
        }

        return false;
    }

    bool VFS::readText(const std::string& uri, std::string& outText) const {
        // 1. Check physical filesystem first
        std::filesystem::path p = resolve(uri);
        if (!p.empty()) {
            std::ifstream file(p);
            if (file.is_open()) {
                outText.assign((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
                return true;
            }
        }

        // 2. Check mounted .lpak packages
        std::string key = extractVirtualLookupKey(uri);
        for (const auto& pak : _paks) {
            if (!pak) continue;
            if (pak->hasFile(key) && pak->readText(key, outText)) {
                return true;
            }
            if (pak->hasFile(uri) && pak->readText(uri, outText)) {
                return true;
            }
        }

        return false;
    }

    void VFS::clear() {
        _mounts.clear();
        _addonLayers.clear();
        _paks.clear();
    }

} // namespace Lab
