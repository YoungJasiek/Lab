#pragma once

// =============================================================================
// Lab Engine Dynamic Link Library (DLL) Export / Import Macros
// =============================================================================

#if defined(_WIN32) || defined(__CYGWIN__)
    #if defined(LAB_CORE_BUILD)
        #define LAB_CORE_API __declspec(dllexport)
    #else
        #define LAB_CORE_API __declspec(dllimport)
    #endif

    #if defined(LAB_RENDER_BUILD)
        #define LAB_RENDER_API __declspec(dllexport)
    #else
        #define LAB_RENDER_API
    #endif
#else
    #define LAB_CORE_API __attribute__((visibility("default")))
    #define LAB_RENDER_API __attribute__((visibility("default")))
#endif
