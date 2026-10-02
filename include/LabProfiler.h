#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <chrono>
#include <mutex>

namespace Lab {

    struct ProfileScope {
        std::string name;
        std::string threadName;
        double startUs{0.0};
        double durationUs{0.0};
        uint32_t depth{0};
    };

    struct MemoryStats {
        size_t totalAllocatedRAM{0};
        size_t totalFreedRAM{0};
        size_t currentUsageRAM{0};
        size_t peakUsageRAM{0};
        size_t allocationCount{0};

        size_t currentUsageVRAM{0};
        size_t peakUsageVRAM{0};
    };

    struct RenderStats {
        uint32_t drawCalls{0};
        uint32_t triangles{0};
        uint32_t shaderBinds{0};
        size_t vboBytesUploaded{0};
    };

    class Profiler {
    public:
        static Profiler& instance() {
            static Profiler inst;
            return inst;
        }

        void beginFrame() {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_currentFrameScopes.clear();
            m_renderStats = RenderStats{};
            m_frameStartTime = std::chrono::high_resolution_clock::now();
        }

        void endFrame() {
            std::lock_guard<std::mutex> lock(m_mutex);
            auto now = std::chrono::high_resolution_clock::now();
            m_lastFrameTimeMs = std::chrono::duration_cast<std::chrono::duration<double, std::milli>>(now - m_frameStartTime).count();
            m_completedFrameScopes = m_currentFrameScopes;
        }

        void recordScope(const std::string& name, const std::string& thread, double durationUs, uint32_t depth = 0) {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_currentFrameScopes.push_back({ name, thread, 0.0, durationUs, depth });
        }

        // Memory tracker hooks
        void recordAlloc(size_t bytes) {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_memoryStats.totalAllocatedRAM += bytes;
            m_memoryStats.currentUsageRAM += bytes;
            m_memoryStats.allocationCount++;
            if (m_memoryStats.currentUsageRAM > m_memoryStats.peakUsageRAM) {
                m_memoryStats.peakUsageRAM = m_memoryStats.currentUsageRAM;
            }
        }

        void recordFree(size_t bytes) {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_memoryStats.totalFreedRAM += bytes;
            if (m_memoryStats.currentUsageRAM >= bytes) {
                m_memoryStats.currentUsageRAM -= bytes;
            } else {
                m_memoryStats.currentUsageRAM = 0;
            }
        }

        void recordVRAMAlloc(size_t bytes) {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_memoryStats.currentUsageVRAM += bytes;
            if (m_memoryStats.currentUsageVRAM > m_memoryStats.peakUsageVRAM) {
                m_memoryStats.peakUsageVRAM = m_memoryStats.currentUsageVRAM;
            }
        }

        void recordVRAMFree(size_t bytes) {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (m_memoryStats.currentUsageVRAM >= bytes) {
                m_memoryStats.currentUsageVRAM -= bytes;
            } else {
                m_memoryStats.currentUsageVRAM = 0;
            }
        }

        // Render stats hooks
        void recordDrawCall(uint32_t triCount = 0) {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_renderStats.drawCalls++;
            m_renderStats.triangles += triCount;
        }

        void recordVBOBytes(size_t bytes) {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_renderStats.vboBytesUploaded += bytes;
        }

        const MemoryStats& getMemoryStats() const { return m_memoryStats; }
        const RenderStats& getRenderStats() const { return m_renderStats; }
        const std::vector<ProfileScope>& getLastFrameScopes() const { return m_completedFrameScopes; }
        double getLastFrameTimeMs() const { return m_lastFrameTimeMs; }

    private:
        Profiler() = default;
        std::mutex m_mutex;
        std::chrono::high_resolution_clock::time_point m_frameStartTime;
        double m_lastFrameTimeMs{0.0};
        MemoryStats m_memoryStats;
        RenderStats m_renderStats;
        std::vector<ProfileScope> m_currentFrameScopes;
        std::vector<ProfileScope> m_completedFrameScopes;
    };

    class ProfileTimerScope {
    public:
        ProfileTimerScope(const std::string& name, const std::string& thread = "Main", uint32_t depth = 0)
            : m_name(name), m_thread(thread), m_depth(depth) {
            m_start = std::chrono::high_resolution_clock::now();
        }

        ~ProfileTimerScope() {
            auto end = std::chrono::high_resolution_clock::now();
            double us = std::chrono::duration_cast<std::chrono::duration<double, std::micro>>(end - m_start).count();
            Profiler::instance().recordScope(m_name, m_thread, us, m_depth);
        }

    private:
        std::string m_name;
        std::string m_thread;
        uint32_t m_depth{0};
        std::chrono::high_resolution_clock::time_point m_start;
    };

} // namespace Lab
