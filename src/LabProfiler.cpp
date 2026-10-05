#include "LabProfiler.h"

namespace Lab {

    Profiler& Profiler::instance() {
        static Profiler inst;
        return inst;
    }

    void Profiler::beginFrame() {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_currentFrameScopes.clear();
        m_renderStats = RenderStats{};
        m_frameStartTime = std::chrono::high_resolution_clock::now();
    }

    void Profiler::endFrame() {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto now = std::chrono::high_resolution_clock::now();
        m_lastFrameTimeMs = std::chrono::duration_cast<std::chrono::duration<double, std::milli>>(now - m_frameStartTime).count();
        m_completedFrameScopes = m_currentFrameScopes;
    }

    void Profiler::recordScope(const std::string& name, const std::string& thread, double durationUs, uint32_t depth) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_currentFrameScopes.push_back({ name, thread, 0.0, durationUs, depth });
    }

    void Profiler::recordAlloc(size_t bytes) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_memoryStats.totalAllocatedRAM += bytes;
        m_memoryStats.currentUsageRAM += bytes;
        m_memoryStats.allocationCount++;
        if (m_memoryStats.currentUsageRAM > m_memoryStats.peakUsageRAM) {
            m_memoryStats.peakUsageRAM = m_memoryStats.currentUsageRAM;
        }
    }

    void Profiler::recordFree(size_t bytes) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_memoryStats.totalFreedRAM += bytes;
        if (m_memoryStats.currentUsageRAM >= bytes) {
            m_memoryStats.currentUsageRAM -= bytes;
        } else {
            m_memoryStats.currentUsageRAM = 0;
        }
    }

    void Profiler::recordVRAMAlloc(size_t bytes) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_memoryStats.currentUsageVRAM += bytes;
        if (m_memoryStats.currentUsageVRAM > m_memoryStats.peakUsageVRAM) {
            m_memoryStats.peakUsageVRAM = m_memoryStats.currentUsageVRAM;
        }
    }

    void Profiler::recordVRAMFree(size_t bytes) {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_memoryStats.currentUsageVRAM >= bytes) {
            m_memoryStats.currentUsageVRAM -= bytes;
        } else {
            m_memoryStats.currentUsageVRAM = 0;
        }
    }

    void Profiler::recordDrawCall(uint32_t triCount) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_renderStats.drawCalls++;
        m_renderStats.triangles += triCount;
    }

    void Profiler::recordVBOBytes(size_t bytes) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_renderStats.vboBytesUploaded += bytes;
    }

    ProfileTimerScope::ProfileTimerScope(const std::string& name, const std::string& thread, uint32_t depth)
        : m_name(name), m_thread(thread), m_depth(depth) {
        m_start = std::chrono::high_resolution_clock::now();
    }

    ProfileTimerScope::~ProfileTimerScope() {
        auto end = std::chrono::high_resolution_clock::now();
        double us = std::chrono::duration_cast<std::chrono::duration<double, std::micro>>(end - m_start).count();
        Profiler::instance().recordScope(m_name, m_thread, us, m_depth);
    }

} // namespace Lab
