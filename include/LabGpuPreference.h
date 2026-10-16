#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <algorithm>
#include <iostream>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <dxgi1_6.h>
#endif

// Driver-level high-performance GPU exports for hybrid systems.
// Define LAB_DEFINE_GPU_EXPORTS in main executable translation units, or include this header directly in main_*.cpp.
#if defined(_WIN32) && !defined(LAB_CORE_BUILD) && !defined(LAB_RENDER_BUILD) && !defined(LAB_WORLD_BUILD) && !defined(LAB_PHYSICS_BUILD) && !defined(LAB_AUDIO_BUILD) && !defined(LAB_AI_BUILD) && !defined(LAB_ANIM_BUILD) && !defined(LAB_NET_BUILD) && !defined(LAB_SCRIPT_BUILD) && !defined(LAB_STUDIO_BUILD) && !defined(LAB_HUD_BUILD)
#ifndef LAB_GPU_EXPORTS_DEFINED
#define LAB_GPU_EXPORTS_DEFINED
extern "C" {
    __declspec(dllexport) unsigned long NvOptimusEnablement = 0x00000001;
    __declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
}
#endif
#endif

namespace Lab {

    struct GpuAdapterInfo {
        uint32_t index = 0;
        std::string name;
        uint32_t vendorId = 0;
        uint32_t deviceId = 0;
        uint64_t dedicatedVramBytes = 0;
        uint64_t sharedMemoryBytes = 0;
        bool isSoftwareAdapter = false;
        bool isHighPerfPreferredByOS = false;
        uint64_t strengthScore = 0;
    };

    class GpuSelector {
    public:
        // Computes hardware strength score so the engine always picks the STRONGEST GPU
        // (even in multi-GPU setups or when comparing a strong APU against a weak legacy adapter).
        static uint64_t computeStrengthScore(const GpuAdapterInfo& adapter) {
            if (adapter.isSoftwareAdapter) {
                return 0;
            }
            const uint64_t dedicatedMB = adapter.dedicatedVramBytes / (1024ULL * 1024ULL);
            const uint64_t sharedMB = adapter.sharedMemoryBytes / (1024ULL * 1024ULL);

            // Dedicated VRAM is weighted 100x per MB; shared memory is weighted 5x per MB
            uint64_t score = dedicatedMB * 100ULL + sharedMB * 5ULL;

            // OS DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE top rank bonus
            if (adapter.isHighPerfPreferredByOS) {
                score += 25000ULL;
            }

            // Vendor architecture tier bonus for known high-throughput GPU vendors (NVIDIA, AMD, Intel Arc)
            if (adapter.vendorId == 0x10DE || adapter.vendorId == 0x1002) {
                score += 10000ULL;
            } else if (adapter.vendorId == 0x8086) {
                score += 4000ULL;
            }
            return score;
        }

        // Picks the strongest adapter from a list of candidates
        static GpuAdapterInfo selectStrongestFromList(std::vector<GpuAdapterInfo> adapters) {
            if (adapters.empty()) {
                GpuAdapterInfo fallback;
                fallback.name = "Default System GPU";
                return fallback;
            }
            for (auto& a : adapters) {
                a.strengthScore = computeStrengthScore(a);
            }
            std::stable_sort(adapters.begin(), adapters.end(), [](const GpuAdapterInfo& a, const GpuAdapterInfo& b) {
                return a.strengthScore > b.strengthScore;
            });
            return adapters.front();
        }

        // Enumerates all physical GPUs on the machine and scores them by hardware capability
        static std::vector<GpuAdapterInfo> enumerateSystemAdapters() {
            std::vector<GpuAdapterInfo> result;
#if defined(_WIN32)
            HMODULE dxgiMod = LoadLibraryA("dxgi.dll");
            if (!dxgiMod) {
                return result;
            }

            using CreateDXGIFactory1Fn = HRESULT(WINAPI*)(REFIID, void**);
            auto createFactory1 = reinterpret_cast<CreateDXGIFactory1Fn>(
                GetProcAddress(dxgiMod, "CreateDXGIFactory1"));
            if (!createFactory1) {
                FreeLibrary(dxgiMod);
                return result;
            }

            IDXGIFactory1* factory1 = nullptr;
            if (FAILED(createFactory1(__uuidof(IDXGIFactory1), reinterpret_cast<void**>(&factory1))) || !factory1) {
                FreeLibrary(dxgiMod);
                return result;
            }

            // Query top OS high-performance LUID via IDXGIFactory6 if available
            LUID highPerfLuid{};
            bool hasHighPerfLuid = false;
            IDXGIFactory6* factory6 = nullptr;
            if (SUCCEEDED(factory1->QueryInterface(__uuidof(IDXGIFactory6), reinterpret_cast<void**>(&factory6))) && factory6) {
                IDXGIAdapter1* prefAdapter = nullptr;
                if (SUCCEEDED(factory6->EnumAdapterByGpuPreference(
                        0, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,
                        __uuidof(IDXGIAdapter1), reinterpret_cast<void**>(&prefAdapter))) && prefAdapter) {
                    DXGI_ADAPTER_DESC1 prefDesc{};
                    if (SUCCEEDED(prefAdapter->GetDesc1(&prefDesc))) {
                        highPerfLuid = prefDesc.AdapterLuid;
                        hasHighPerfLuid = true;
                    }
                    prefAdapter->Release();
                }
                factory6->Release();
            }

            IDXGIAdapter1* adapter = nullptr;
            for (UINT i = 0; factory1->EnumAdapters1(i, &adapter) != DXGI_ERROR_NOT_FOUND; ++i) {
                DXGI_ADAPTER_DESC1 desc{};
                if (SUCCEEDED(adapter->GetDesc1(&desc))) {
                    GpuAdapterInfo info;
                    info.index = i;

                    char narrowName[256] = {};
                    WideCharToMultiByte(CP_UTF8, 0, desc.Description, -1, narrowName, sizeof(narrowName) - 1, nullptr, nullptr);
                    info.name = narrowName;
                    info.vendorId = desc.VendorId;
                    info.deviceId = desc.DeviceId;
                    info.dedicatedVramBytes = static_cast<uint64_t>(desc.DedicatedVideoMemory);
                    info.sharedMemoryBytes = static_cast<uint64_t>(desc.SharedSystemMemory);
                    info.isSoftwareAdapter = (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) != 0 ||
                                             (desc.VendorId == 0x1414 && desc.DeviceId == 0x8c);
                    if (hasHighPerfLuid &&
                        desc.AdapterLuid.LowPart == highPerfLuid.LowPart &&
                        desc.AdapterLuid.HighPart == highPerfLuid.HighPart) {
                        info.isHighPerfPreferredByOS = true;
                    }
                    info.strengthScore = computeStrengthScore(info);
                    if (!info.isSoftwareAdapter) {
                        result.push_back(info);
                    }
                }
                adapter->Release();
            }

            factory1->Release();
            FreeLibrary(dxgiMod);
#endif
            return result;
        }

        // Enforces the strongest GPU in the system before OpenGL/WGL context initialization
        static GpuAdapterInfo enforceStrongestGpu() {
            auto adapters = enumerateSystemAdapters();
            GpuAdapterInfo strongest = selectStrongestFromList(adapters);

#if defined(_WIN32)
            // Force Windows 10/11 DirectX/WGL UserGpuPreferences for this executable to High Performance (GpuPreference=2;)
            char exePath[MAX_PATH] = {};
            if (GetModuleFileNameA(nullptr, exePath, MAX_PATH) > 0) {
                HKEY hKey = nullptr;
                if (RegCreateKeyExA(HKEY_CURRENT_USER,
                                    "Software\\Microsoft\\DirectX\\UserGpuPreferences",
                                    0, nullptr, 0, KEY_SET_VALUE, nullptr, &hKey, nullptr) == ERROR_SUCCESS) {
                    const char* prefValue = "GpuPreference=2;";
                    RegSetValueExA(hKey, exePath, 0, REG_SZ,
                                   reinterpret_cast<const BYTE*>(prefValue),
                                   static_cast<DWORD>(std::strlen(prefValue) + 1));
                    RegCloseKey(hKey);
                }
            }
            // Set Optimus / Radeon / Mesa runtime hints to pick the strongest adapter
            _putenv_s("SHIM_MCCOMPAT", "0x800000001");
#else
            setenv("DRI_PRIME", "1", 0);
            setenv("__NV_PRIME_RENDER_OFFLOAD", "1", 0);
            setenv("__GLX_VENDOR_LIBRARY_NAME", "nvidia", 0);
#endif
            return strongest;
        }
    };

} // namespace Lab
