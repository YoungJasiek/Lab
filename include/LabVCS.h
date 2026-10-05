#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <cstdint>

namespace Lab {

    enum class FileVCSStatus {
        Unmodified,
        Modified,
        Added,
        Deleted,
        Untracked,
        LockedByOther,
        LockedByMe
    };

    struct VCSRefCommit {
        std::string hash;
        std::string author;
        std::string message;
        uint64_t timestamp{0};
    };

    struct LFSAssetLock {
        std::string assetPath;
        std::string lockedBy;
        uint64_t lockTime{0};
    };

    class VCSManager {
    public:
        static VCSManager& instance();

        void setRepoRoot(const std::string& path);
        const std::string& getRepoRoot() const { return m_repoRoot; }

        void setFileStatus(const std::string& filePath, FileVCSStatus status);
        FileVCSStatus getFileStatus(const std::string& filePath) const;
        std::vector<std::pair<std::string, FileVCSStatus>> getChangedFiles() const;

        // Git LFS Asset Locking
        bool lockLFSAsset(const std::string& assetPath, const std::string& user);
        bool unlockLFSAsset(const std::string& assetPath, const std::string& user, bool force = false);
        bool isLFSAssetLocked(const std::string& assetPath, std::string* outLockedBy = nullptr) const;

        // Revision history log
        void addCommit(const std::string& hash, const std::string& author, const std::string& message);
        const std::vector<VCSRefCommit>& getHistory() const { return m_history; }

        void clear();

    private:
        VCSManager() = default;
        std::string m_repoRoot{"."};
        std::unordered_map<std::string, FileVCSStatus> m_fileStatuses;
        std::unordered_map<std::string, LFSAssetLock> m_locks;
        std::vector<VCSRefCommit> m_history;
    };

} // namespace Lab
