#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <chrono>
#include <mutex>
#include <cstdint>

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
        static Profiler& instance();

        void beginFrame();
        void endFrame();
        void recordScope(const std::string& name, const std::string& thread, double durationUs, uint32_t depth = 0);

        // Memory tracker hooks
        void recordAlloc(size_t bytes);
        void recordFree(size_t bytes);
        void recordVRAMAlloc(size_t bytes);
        void recordVRAMFree(size_t bytes);

        // Render stats hooks
        void recordDrawCall(uint32_t triCount = 0);
        void recordVBOBytes(size_t bytes);

        const MemoryStats& getMemoryStats() const { return m_memoryStats; }
        const RenderStats& getRenderStats() const { return m_renderStats; }
        const std::vector<ProfileScope>& getLastFrameScopes() const { return m_completedFrameScopes; }
        double getLastFrameTimeMs() const { return m_lastFrameTimeMs; }

    private:
        Profiler() = default;
        mutable std::mutex m_mutex;
        std::chrono::high_resolution_clock::time_point m_frameStartTime;
        double m_lastFrameTimeMs{0.0};
        MemoryStats m_memoryStats;
        RenderStats m_renderStats;
        std::vector<ProfileScope> m_currentFrameScopes;
        std::vector<ProfileScope> m_completedFrameScopes;
    };

    class ProfileTimerScope {
    public:
        ProfileTimerScope(const std::string& name, const std::string& thread = "Main", uint32_t depth = 0);
        ~ProfileTimerScope();

    private:
        std::string m_name;
        std::string m_thread;
        uint32_t m_depth{0};
        std::chrono::high_resolution_clock::time_point m_start;
    };

} // namespace Lab
