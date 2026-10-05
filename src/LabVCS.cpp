#include "LabVCS.h"
#include <chrono>

namespace Lab {

    VCSManager& VCSManager::instance() {
        static VCSManager inst;
        return inst;
    }

    void VCSManager::setRepoRoot(const std::string& path) {
        m_repoRoot = path;
    }

    void VCSManager::setFileStatus(const std::string& filePath, FileVCSStatus status) {
        m_fileStatuses[filePath] = status;
    }

    FileVCSStatus VCSManager::getFileStatus(const std::string& filePath) const {
        auto it = m_fileStatuses.find(filePath);
        if (it != m_fileStatuses.end()) {
            return it->second;
        }
        return FileVCSStatus::Unmodified;
    }

    std::vector<std::pair<std::string, FileVCSStatus>> VCSManager::getChangedFiles() const {
        std::vector<std::pair<std::string, FileVCSStatus>> changed;
        for (const auto& [path, status] : m_fileStatuses) {
            if (status != FileVCSStatus::Unmodified) {
                changed.push_back({ path, status });
            }
        }
        return changed;
    }

    bool VCSManager::lockLFSAsset(const std::string& assetPath, const std::string& user) {
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

    bool VCSManager::unlockLFSAsset(const std::string& assetPath, const std::string& user, bool force) {
        auto it = m_locks.find(assetPath);
        if (it == m_locks.end()) return true;

        if (it->second.lockedBy == user || force) {
            m_locks.erase(it);
            m_fileStatuses[assetPath] = FileVCSStatus::Unmodified;
            return true;
        }
        return false;
    }

    bool VCSManager::isLFSAssetLocked(const std::string& assetPath, std::string* outLockedBy) const {
        auto it = m_locks.find(assetPath);
        if (it != m_locks.end()) {
            if (outLockedBy) *outLockedBy = it->second.lockedBy;
            return true;
        }
        return false;
    }

    void VCSManager::addCommit(const std::string& hash, const std::string& author, const std::string& message) {
        m_history.push_back({ hash, author, message, (uint64_t)std::chrono::system_clock::now().time_since_epoch().count() });
    }

    void VCSManager::clear() {
        m_fileStatuses.clear();
        m_locks.clear();
        m_history.clear();
    }

} // namespace Lab
