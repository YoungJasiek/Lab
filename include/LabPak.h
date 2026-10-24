#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <filesystem>
#include <cstdint>
#include <memory>
#include <fstream>
#include <span>

namespace Lab {

    #pragma pack(push, 1)
    struct PakHeader {
        uint32_t magic = 0x4B41504C; // "LPAK" (little endian: 'L', 'P', 'A', 'K')
        uint32_t version = 1;
        uint32_t flags = 0;          // bit 0: encrypted, bit 1: compressed, bit 2: authenticated
        uint32_t entryCount = 0;
        uint64_t indexOffset = 0;
        uint64_t indexLength = 0;
        uint32_t checksum = 0;       // CRC32 of package index/content
    };
    #pragma pack(pop)

    enum PakFlags : uint32_t {
        PAK_FLAG_NONE       = 0,
        PAK_FLAG_ENCRYPTED  = (1 << 0),
        PAK_FLAG_COMPRESSED = (1 << 1),
        PAK_FLAG_VERIFIED   = (1 << 2)
    };

    struct PakEntry {
        std::string virtualPath;     // e.g. "assets/models/guard.glb" or "assets/weapons/pistol.glb"
        uint64_t offset = 0;         // Byte offset in .lpak file where payload begins
        uint64_t compressedSize = 0;
        uint64_t uncompressedSize = 0;
        uint32_t crc32 = 0;
        uint32_t flags = 0;
    };

    class LabPak {
    public:
        LabPak();
        ~LabPak();

        LabPak(const LabPak&) = delete;
        LabPak& operator=(const LabPak&) = delete;

        LabPak(LabPak&& other) noexcept;
        LabPak& operator=(LabPak&& other) noexcept;

        // Open an existing .lpak file for reading
        bool open(const std::filesystem::path& pakPath);
        void close();
        bool isOpen() const;

        // Query files inside package
        bool hasFile(const std::string& virtualPath) const;
        const PakEntry* getEntry(const std::string& virtualPath) const;
        std::vector<std::string> getFileList() const;

        // Read file contents
        bool readBinary(const std::string& virtualPath, std::vector<uint8_t>& outData) const;
        bool readText(const std::string& virtualPath, std::string& outText) const;

        // Package creation / baking utilities
        static bool createPackage(const std::filesystem::path& outPakPath,
                                  const std::vector<std::pair<std::string, std::filesystem::path>>& filesToPack,
                                  bool encrypt = true);

        static bool packDirectory(const std::filesystem::path& outPakPath,
                                  const std::filesystem::path& sourceDir,
                                  const std::string& virtualPrefix = "",
                                  bool encrypt = true);

        // CRC32 calculation helper
        static uint32_t calculateCRC32(std::span<const uint8_t> data);

        // Encryption / decryption helper (symmetric stream cipher with engine master key)
        static void cryptBuffer(std::span<uint8_t> buffer, uint64_t seedOffset = 0);

        const std::filesystem::path& getPakPath() const { return _pakPath; }
        const PakHeader& getHeader() const { return _header; }
        size_t getEntryCount() const { return _entries.size(); }

        static std::string normalizeVirtualPath(const std::string& path);

    private:
        std::filesystem::path _pakPath;
        mutable std::ifstream _fileStream;
        PakHeader _header;
        std::unordered_map<std::string, PakEntry> _entries;
    };

} // namespace Lab
