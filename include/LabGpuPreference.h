#pragma once

// Include this header in exactly ONE translation unit of every executable
// (not DLLs). The drivers read these exported symbols from the .exe at
// process start to pick the discrete GPU on hybrid (Optimus / Enduro) laptops.
// Without them OpenGL often runs on the integrated GPU, which looks like
// "20 FPS on an RTX 2060".
#if defined(_WIN32)
extern "C" {
    __declspec(dllexport) unsigned long NvOptimusEnablement = 0x00000001;
    __declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
}
#endif
