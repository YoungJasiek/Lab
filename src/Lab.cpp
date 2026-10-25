#include "Lab.h"

namespace Lab {

    LAB_API const char* GetLabVersion() {
        return LAB_SDK_VERSION_STRING;
    }

    LAB_API const char* GetLabBuildString() {
        return "Lab Engine 64-bit Core (C++20 / OpenGL 4.6 DSA / Source Architecture)";
    }

} // namespace Lab
