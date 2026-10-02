#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <chrono>

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
        static VCSManager& instance() {
            static VCSManager inst;
            return inst;
        }

        void setRepoRoot(const std::string& path) {
            m_repoRoot = path;
        }

        const std::string& getRepoRoot() const { return m_repoRoot; }

        void setFileStatus(const std::string& filePath, FileVCSStatus status) {
            m_fileStatuses[filePath] = status;
        }

        FileVCSStatus getFileStatus(const std::string& filePath) const {
            auto it = m_fileStatuses.find(filePath);
            if (it != m_fileStatuses.end()) {
                return it->second;
            }
            return FileVCSStatus::Unmodified;
        }

        std::vector<std::pair<std::string, FileVCSStatus>> getChangedFiles() const {
            std::vector<std::pair<std::string, FileVCSStatus>> changed;
            for (const auto& [path, status] : m_fileStatuses) {
                if (status != FileVCSStatus::Unmodified) {
                    changed.push_back({ path, status });
                }
            }
            return changed;
        }

        // Git LFS Asset Locking
        bool lockLFSAsset(const std::string& assetPath, const std::string& user) {
            auto it = m_locks.find(assetPath);
            if (it != m_locks.end()) {
                if (it->second.lockedBy != user) {
                    return false; // Locked by someone else
                }
                return true; // Already locked by user
            }
            m_locks[assetPath] = { assetPath, user, (uint64_t)std::chrono::system_clock::now().time_since_epoch().count() };
            m_fileStatuses[assetPath] = FileVCSStatus::LockedByMe;
            return true;
        }

        bool unlockLFSAsset(const std::string& assetPath, const std::string& user, bool force = false) {
            auto it = m_locks.find(assetPath);
            if (it == m_locks.end()) return true;

            if (it->second.lockedBy == user || force) {
                m_locks.erase(it);
                m_fileStatuses[assetPath] = FileVCSStatus::Unmodified;
                return true;
            }
            return false;
        }

        bool isLFSAssetLocked(const std::string& assetPath, std::string* outLockedBy = nullptr) const {
            auto it = m_locks.find(assetPath);
            if (it != m_locks.end()) {
                if (outLockedBy) *outLockedBy = it->second.lockedBy;
                return true;
            }
            return false;
        }

        // Revision history log
        void addCommit(const std::string& hash, const std::string& author, const std::string& message) {
            m_history.push_back({ hash, author, message, (uint64_t)std::chrono::system_clock::now().time_since_epoch().count() });
        }

        const std::vector<VCSRefCommit>& getHistory() const { return m_history; }

        void clear() {
            m_fileStatuses.clear();
            m_locks.clear();
            m_history.clear();
        }

    private:
        VCSManager() = default;
        std::string m_repoRoot{"."};
        std::unordered_map<std::string, FileVCSStatus> m_fileStatuses;
        std::unordered_map<std::string, LFSAssetLock> m_locks;
        std::vector<VCSRefCommit> m_history;
    };

} // namespace Lab
