#include "LabPak.h"
#include "LabCore.h"
#include <fstream>
#include <sstream>
#include <cstring>
#include <algorithm>
#include <array>

namespace Lab {

    namespace {
        // Standard IEEE 802.3 32-bit CRC table
        constexpr auto generateCrc32Table() {
            std::array<uint32_t, 256> table{};
            for (uint32_t i = 0; i < 256; ++i) {
                uint32_t rem = i;
                for (int j = 0; j < 8; ++j) {
                    if (rem & 1) {
                        rem = (rem >> 1) ^ 0xEDB88320u;
                    } else {
                        rem >>= 1;
                    }
                }
                table[i] = rem;
            }
            return table;
        }

        constexpr auto s_crc32Table = generateCrc32Table();

        // Engine Master Cryptographic Key (32 bytes)
        constexpr uint8_t s_engineMasterKey[32] = {
            0x4C, 0x61, 0x62, 0x45, 0x6E, 0x67, 0x69, 0x6E, // "LabEngin"
            0x65, 0x32, 0x30, 0x32, 0x36, 0x5F, 0x41, 0x73, // "e2026_As"
            0x73, 0x65, 0x74, 0x53, 0x65, 0x63, 0x75, 0x72, // "setSecur"
            0x65, 0x4B, 0x65, 0x79, 0x5F, 0x46, 0x4C, 0x21  // "eKey_FL!"
        };
    }

    uint32_t LabPak::calculateCRC32(std::span<const uint8_t> data) {
        uint32_t crc = 0xFFFFFFFFu;
        for (uint8_t byte : data) {
            uint8_t tableIndex = static_cast<uint8_t>((crc ^ byte) & 0xFFu);
            crc = (crc >> 8) ^ s_crc32Table[tableIndex];
        }
        return crc ^ 0xFFFFFFFFu;
    }

    void LabPak::cryptBuffer(std::span<uint8_t> buffer, uint64_t seedOffset) {
        for (size_t i = 0; i < buffer.size(); ++i) {
            uint64_t totalIdx = seedOffset + i;
            uint8_t keyByte = s_engineMasterKey[totalIdx % sizeof(s_engineMasterKey)];
            uint8_t posMix = static_cast<uint8_t>((totalIdx * 109u + 89u) & 0xFFu);
            buffer[i] ^= (keyByte ^ posMix);
        }
    }

    std::vector<uint8_t> LabPak::compressBuffer(std::span<const uint8_t> data) {
        std::vector<uint8_t> out;
        out.reserve(data.size() / 2 + 16);
        out.push_back('L');
        out.push_back('Z');
        out.push_back('L');
        out.push_back('B');
        uint32_t origSize = static_cast<uint32_t>(data.size());
        out.push_back(static_cast<uint8_t>(origSize & 0xFFu));
        out.push_back(static_cast<uint8_t>((origSize >> 8) & 0xFFu));
        out.push_back(static_cast<uint8_t>((origSize >> 16) & 0xFFu));
        out.push_back(static_cast<uint8_t>((origSize >> 24) & 0xFFu));

        if (data.empty()) {
            return out;
        }

        std::array<uint32_t, 4096> hashTable{};
        size_t anchor = 0;
        size_t ip = 0;
        const size_t matchLimit = data.size() >= 12 ? data.size() - 12 : 0;

        while (ip < matchLimit) {
            uint32_t seq = 0;
            std::memcpy(&seq, &data[ip], sizeof(uint32_t));
            uint32_t h = (seq * 2654435761u) >> 20;
            size_t ref = hashTable[h];
            hashTable[h] = static_cast<uint32_t>(ip);

            size_t offset = ip - ref;
            if (offset > 0 && offset <= 65535 && std::memcmp(&data[ref], &data[ip], 4) == 0) {
                size_t matchLen = 4;
                while (ip + matchLen < data.size() && data[ref + matchLen] == data[ip + matchLen]) {
                    ++matchLen;
                }

                size_t litLen = ip - anchor;
                uint8_t litCode = (litLen >= 15) ? 15 : static_cast<uint8_t>(litLen);
                size_t mlCodeVal = matchLen - 4;
                uint8_t mlCode = (mlCodeVal >= 15) ? 15 : static_cast<uint8_t>(mlCodeVal);
                out.push_back(static_cast<uint8_t>((litCode << 4) | mlCode));

                if (litLen >= 15) {
                    size_t rem = litLen - 15;
                    while (rem >= 255) {
                        out.push_back(255);
                        rem -= 255;
                    }
                    out.push_back(static_cast<uint8_t>(rem));
                }

                out.insert(out.end(), data.begin() + static_cast<std::ptrdiff_t>(anchor),
                           data.begin() + static_cast<std::ptrdiff_t>(anchor + litLen));

                uint16_t off16 = static_cast<uint16_t>(offset);
                out.push_back(static_cast<uint8_t>(off16 & 0xFFu));
                out.push_back(static_cast<uint8_t>((off16 >> 8) & 0xFFu));

                if (mlCodeVal >= 15) {
                    size_t mrem = mlCodeVal - 15;
                    while (mrem >= 255) {
                        out.push_back(255);
                        mrem -= 255;
                    }
                    out.push_back(static_cast<uint8_t>(mrem));
                }

                ip += matchLen;
                anchor = ip;
            } else {
                ++ip;
            }
        }

        size_t litLen = data.size() - anchor;
        uint8_t litCode = (litLen >= 15) ? 15 : static_cast<uint8_t>(litLen);
        out.push_back(static_cast<uint8_t>(litCode << 4));
        if (litLen >= 15) {
            size_t rem = litLen - 15;
            while (rem >= 255) {
                out.push_back(255);
                rem -= 255;
            }
            out.push_back(static_cast<uint8_t>(rem));
        }
        out.insert(out.end(), data.begin() + static_cast<std::ptrdiff_t>(anchor), data.end());
        return out;
    }

    bool LabPak::decompressBuffer(std::span<const uint8_t> compressed, std::vector<uint8_t>& outDecompressed) {
        if (compressed.size() < 8 ||
            compressed[0] != 'L' || compressed[1] != 'Z' || compressed[2] != 'L' || compressed[3] != 'B') {
            return false;
        }

        uint32_t origSize = static_cast<uint32_t>(compressed[4]) |
                            (static_cast<uint32_t>(compressed[5]) << 8) |
                            (static_cast<uint32_t>(compressed[6]) << 16) |
                            (static_cast<uint32_t>(compressed[7]) << 24);

        outDecompressed.clear();
        if (origSize == 0) {
            return true;
        }
        outDecompressed.reserve(origSize);

        size_t ip = 8;
        while (ip < compressed.size()) {
            uint8_t token = compressed[ip++];
            size_t litLen = (token >> 4) & 0x0Fu;
            if (litLen == 15) {
                while (ip < compressed.size()) {
                    uint8_t b = compressed[ip++];
                    litLen += b;
                    if (b < 255) break;
                }
            }

            if (ip + litLen > compressed.size() || outDecompressed.size() + litLen > origSize) {
                return false;
            }

            outDecompressed.insert(outDecompressed.end(),
                                   compressed.begin() + static_cast<std::ptrdiff_t>(ip),
                                   compressed.begin() + static_cast<std::ptrdiff_t>(ip + litLen));
            ip += litLen;

            if (ip >= compressed.size() || outDecompressed.size() == origSize) {
                break;
            }

            if (ip + 2 > compressed.size()) {
                return false;
            }

            uint16_t offset = static_cast<uint16_t>(compressed[ip]) |
                              static_cast<uint16_t>(static_cast<uint16_t>(compressed[ip + 1]) << 8);
            ip += 2;

            if (offset == 0 || offset > outDecompressed.size()) {
                return false;
            }

            size_t matchLen = (token & 0x0Fu) + 4;
            if ((token & 0x0Fu) == 15) {
                while (ip < compressed.size()) {
                    uint8_t b = compressed[ip++];
                    matchLen += b;
                    if (b < 255) break;
                }
            }

            if (outDecompressed.size() + matchLen > origSize) {
                return false;
            }

            size_t matchPos = outDecompressed.size() - offset;
            for (size_t i = 0; i < matchLen; ++i) {
                outDecompressed.push_back(outDecompressed[matchPos + i]);
            }
        }

        return outDecompressed.size() == origSize;
    }

    std::string LabPak::normalizeVirtualPath(const std::string& path) {
        std::string norm = path;
        std::replace(norm.begin(), norm.end(), '\\', '/');
        while (!norm.empty() && norm.front() == '/') {
            norm.erase(norm.begin());
        }

        // Security check: reject paths attempting path traversal
        if (norm.find("..") != std::string::npos || norm.find(':') != std::string::npos) {
            return {};
        }

        // Lowercase for case-insensitive lookup
        std::string lowerNorm = norm;
        for (char& c : lowerNorm) {
            c = static_cast<char>(::tolower(static_cast<unsigned char>(c)));
        }
        return lowerNorm;
    }

    LabPak::LabPak() {
        _header = PakHeader{};
    }

    LabPak::~LabPak() {
        close();
    }

    LabPak::LabPak(LabPak&& other) noexcept
        : _pakPath(std::move(other._pakPath))
        , _fileStream(std::move(other._fileStream))
        , _header(other._header)
        , _entries(std::move(other._entries)) {
        other._header = PakHeader{};
    }

    LabPak& LabPak::operator=(LabPak&& other) noexcept {
        if (this != &other) {
            close();
            _pakPath = std::move(other._pakPath);
            _fileStream = std::move(other._fileStream);
            _header = other._header;
            _entries = std::move(other._entries);
            other._header = PakHeader{};
        }
        return *this;
    }

    void LabPak::close() {
        if (_fileStream.is_open()) {
            _fileStream.close();
        }
        _entries.clear();
        _header = PakHeader{};
        _pakPath.clear();
    }

    bool LabPak::isOpen() const {
        return _fileStream.is_open();
    }

    bool LabPak::open(const std::filesystem::path& pakPath) {
        close();

        _fileStream.open(pakPath, std::ios::binary);
        if (!_fileStream.is_open()) {
            return false;
        }

        _pakPath = pakPath;

        // 1. Read header
        if (!_fileStream.read(reinterpret_cast<char*>(&_header), sizeof(PakHeader))) {
            close();
            return false;
        }

        // 2. Validate magic ('L', 'P', 'A', 'K')
        if (_header.magic != 0x4B41504C) {
            LabLog::error("[LabPak] Invalid magic in: " + pakPath.string());
            close();
            return false;
        }

        if (_header.version != 1) {
            LabLog::error("[LabPak] Unsupported package version: " + std::to_string(_header.version));
            close();
            return false;
        }

        // 3. Read index table
        if (_header.entryCount > 0 && _header.indexOffset > 0) {
            _fileStream.seekg(static_cast<std::streamoff>(_header.indexOffset), std::ios::beg);
            if (!_fileStream.good()) {
                close();
                return false;
            }

            std::vector<uint8_t> indexBytes(_header.indexLength);
            if (!_fileStream.read(reinterpret_cast<char*>(indexBytes.data()), static_cast<std::streamsize>(_header.indexLength))) {
                close();
                return false;
            }

            // If index is encrypted, decrypt it
            if (_header.flags & PAK_FLAG_ENCRYPTED) {
                cryptBuffer(indexBytes, _header.indexOffset);
            }

            // Verify index checksum
            uint32_t computedCrc = calculateCRC32(indexBytes);
            if (_header.checksum != 0 && computedCrc != _header.checksum) {
                LabLog::error("[LabPak] Package index checksum mismatch in: " + pakPath.string());
                close();
                return false;
            }

            // Parse entries from indexBytes
            size_t cursor = 0;
            for (uint32_t i = 0; i < _header.entryCount; ++i) {
                if (cursor + sizeof(uint16_t) > indexBytes.size()) break;

                uint16_t pathLen = 0;
                std::memcpy(&pathLen, &indexBytes[cursor], sizeof(uint16_t));
                cursor += sizeof(uint16_t);

                if (cursor + pathLen > indexBytes.size()) break;
                std::string vpath(reinterpret_cast<char*>(&indexBytes[cursor]), pathLen);
                cursor += pathLen;

                if (cursor + sizeof(uint64_t) * 3 + sizeof(uint32_t) * 2 > indexBytes.size()) break;

                PakEntry entry;
                entry.virtualPath = vpath;

                std::memcpy(&entry.offset, &indexBytes[cursor], sizeof(uint64_t)); cursor += sizeof(uint64_t);
                std::memcpy(&entry.compressedSize, &indexBytes[cursor], sizeof(uint64_t)); cursor += sizeof(uint64_t);
                std::memcpy(&entry.uncompressedSize, &indexBytes[cursor], sizeof(uint64_t)); cursor += sizeof(uint64_t);
                std::memcpy(&entry.crc32, &indexBytes[cursor], sizeof(uint32_t)); cursor += sizeof(uint32_t);
                std::memcpy(&entry.flags, &indexBytes[cursor], sizeof(uint32_t)); cursor += sizeof(uint32_t);

                std::string key = normalizeVirtualPath(vpath);
                if (!key.empty()) {
                    _entries[key] = entry;
                }
            }
        }

        return true;
    }

    bool LabPak::hasFile(const std::string& virtualPath) const {
        std::string key = normalizeVirtualPath(virtualPath);
        return _entries.find(key) != _entries.end();
    }

    const PakEntry* LabPak::getEntry(const std::string& virtualPath) const {
        std::string key = normalizeVirtualPath(virtualPath);
        auto it = _entries.find(key);
        if (it != _entries.end()) {
            return &it->second;
        }
        return nullptr;
    }

    std::vector<std::string> LabPak::getFileList() const {
        std::vector<std::string> result;
        result.reserve(_entries.size());
        for (const auto& [_, entry] : _entries) {
            result.push_back(entry.virtualPath);
        }
        return result;
    }

    bool LabPak::readBinary(const std::string& virtualPath, std::vector<uint8_t>& outData) const {
        const PakEntry* entry = getEntry(virtualPath);
        if (!entry || !_fileStream.is_open()) {
            return false;
        }

        _fileStream.seekg(static_cast<std::streamoff>(entry->offset), std::ios::beg);
        if (!_fileStream.good()) {
            return false;
        }

        size_t sizeToRead = static_cast<size_t>(entry->compressedSize > 0 ? entry->compressedSize : entry->uncompressedSize);
        outData.resize(sizeToRead);

        if (!_fileStream.read(reinterpret_cast<char*>(outData.data()), static_cast<std::streamsize>(sizeToRead))) {
            outData.clear();
            return false;
        }

        // Decrypt if encrypted
        if ((_header.flags & PAK_FLAG_ENCRYPTED) || (entry->flags & PAK_FLAG_ENCRYPTED)) {
            cryptBuffer(outData, entry->offset);
        }

        // Verify CRC32
        if (entry->crc32 != 0) {
            uint32_t computed = calculateCRC32(outData);
            if (computed != entry->crc32) {
                LabLog::error("[LabPak] Checksum error reading: " + virtualPath);
                outData.clear();
                return false;
            }
        }

        return true;
    }

    bool LabPak::readText(const std::string& virtualPath, std::string& outText) const {
        std::vector<uint8_t> binData;
        if (!readBinary(virtualPath, binData)) {
            return false;
        }
        outText.assign(reinterpret_cast<char*>(binData.data()), binData.size());
        return true;
    }

    bool LabPak::createPackage(const std::filesystem::path& outPakPath,
                              const std::vector<std::pair<std::string, std::filesystem::path>>& filesToPack,
                              bool encrypt) {
        if (filesToPack.empty()) {
            return false;
        }

        std::filesystem::create_directories(outPakPath.parent_path());
        std::ofstream out(outPakPath, std::ios::binary);
        if (!out.is_open()) {
            return false;
        }

        PakHeader header;
        header.magic = 0x4B41504C; // "LPAK"
        header.version = 1;
        header.flags = encrypt ? (PAK_FLAG_ENCRYPTED | PAK_FLAG_VERIFIED) : PAK_FLAG_VERIFIED;
        header.entryCount = static_cast<uint32_t>(filesToPack.size());

        // Write placeholder header
        out.write(reinterpret_cast<const char*>(&header), sizeof(PakHeader));

        std::vector<PakEntry> entries;
        entries.reserve(filesToPack.size());

        // Write payload data
        for (const auto& [vpath, diskPath] : filesToPack) {
            std::ifstream file(diskPath, std::ios::binary | std::ios::ate);
            if (!file.is_open()) {
                LabLog::warn("[LabPak] Could not open file for packing: " + diskPath.string());
                continue;
            }

            std::streamsize fileSize = file.tellg();
            if (fileSize < 0) continue;
            file.seekg(0, std::ios::beg);

            std::vector<uint8_t> buffer(static_cast<size_t>(fileSize));
            if (!file.read(reinterpret_cast<char*>(buffer.data()), fileSize)) {
                continue;
            }

            PakEntry entry;
            entry.virtualPath = vpath;
            entry.uncompressedSize = static_cast<uint64_t>(fileSize);
            entry.compressedSize = entry.uncompressedSize;
            entry.crc32 = calculateCRC32(buffer);
            entry.offset = static_cast<uint64_t>(out.tellp());
            entry.flags = encrypt ? PAK_FLAG_ENCRYPTED : PAK_FLAG_NONE;

            // Encrypt if requested
            if (encrypt) {
                cryptBuffer(buffer, entry.offset);
            }

            out.write(reinterpret_cast<const char*>(buffer.data()), static_cast<std::streamsize>(buffer.size()));
            entries.push_back(entry);
        }

        // Build index table
        header.entryCount = static_cast<uint32_t>(entries.size());
        header.indexOffset = static_cast<uint64_t>(out.tellp());

        std::vector<uint8_t> indexBytes;
        for (const auto& entry : entries) {
            uint16_t pathLen = static_cast<uint16_t>(entry.virtualPath.size());
            size_t oldSize = indexBytes.size();
            indexBytes.resize(oldSize + sizeof(uint16_t) + pathLen + sizeof(uint64_t) * 3 + sizeof(uint32_t) * 2);

            uint8_t* ptr = &indexBytes[oldSize];
            std::memcpy(ptr, &pathLen, sizeof(uint16_t)); ptr += sizeof(uint16_t);
            std::memcpy(ptr, entry.virtualPath.data(), pathLen); ptr += pathLen;
            std::memcpy(ptr, &entry.offset, sizeof(uint64_t)); ptr += sizeof(uint64_t);
            std::memcpy(ptr, &entry.compressedSize, sizeof(uint64_t)); ptr += sizeof(uint64_t);
            std::memcpy(ptr, &entry.uncompressedSize, sizeof(uint64_t)); ptr += sizeof(uint64_t);
            std::memcpy(ptr, &entry.crc32, sizeof(uint32_t)); ptr += sizeof(uint32_t);
            std::memcpy(ptr, &entry.flags, sizeof(uint32_t));
        }

        header.indexLength = static_cast<uint64_t>(indexBytes.size());
        header.checksum = calculateCRC32(indexBytes);

        // Encrypt index if package is encrypted
        if (encrypt) {
            cryptBuffer(indexBytes, header.indexOffset);
        }

        out.write(reinterpret_cast<const char*>(indexBytes.data()), static_cast<std::streamsize>(indexBytes.size()));

        // Rewind and rewrite complete header
        out.seekp(0, std::ios::beg);
        out.write(reinterpret_cast<const char*>(&header), sizeof(PakHeader));

        return true;
    }

    bool LabPak::packDirectory(const std::filesystem::path& outPakPath,
                              const std::filesystem::path& sourceDir,
                              const std::string& virtualPrefix,
                              bool encrypt) {
        if (!std::filesystem::exists(sourceDir)) {
            return false;
        }

        std::vector<std::pair<std::string, std::filesystem::path>> files;
        for (const auto& it : std::filesystem::recursive_directory_iterator(sourceDir)) {
            if (it.is_regular_file()) {
                if (it.path().extension() == ".lpak") continue;
                std::filesystem::path rel = std::filesystem::relative(it.path(), sourceDir);
                std::string vpath = (virtualPrefix.empty() ? "" : virtualPrefix + "/") + rel.generic_string();
                files.emplace_back(vpath, it.path());
            }
        }

        return createPackage(outPakPath, files, encrypt);
    }

} // namespace Lab
