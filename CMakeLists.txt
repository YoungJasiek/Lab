cmake_minimum_required(VERSION 3.15)
project(LabEngine VERSION 0.1.0 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Enable automatic symbol export for Windows dynamic link libraries (.dll)
set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)

# Strict compiler flags with standard MSVC DLL warning suppressions
if(MSVC)
    set(COMPILER_FLAGS /W4 /EHsc /wd4251 /wd4275)
else()
    set(COMPILER_FLAGS -Wall -Wextra -Wpedantic)
endif()

include(FetchContent)

# Fetch GLFW
FetchContent_Declare(
  glfw
  GIT_REPOSITORY https://github.com/glfw/glfw.git
  GIT_TAG        3.4
)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(glfw)

# Add Lua 5.4 scripting runtime
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/external/lua/onelua.c")
    add_library(lua STATIC "${CMAKE_CURRENT_SOURCE_DIR}/external/lua/onelua.c")
    target_compile_definitions(lua PRIVATE MAKE_LIB)
    target_include_directories(lua PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/external/lua")
    if(MSVC)
        target_compile_options(lua PRIVATE /W3)
    endif()
else()
    FetchContent_Declare(
        lua
        GIT_REPOSITORY https://github.com/lua/lua.git
        GIT_TAG        v5.4.6
    )
    FetchContent_GetProperties(lua)
    if(NOT lua_POPULATED)
        FetchContent_Populate(lua)
        add_library(lua STATIC ${lua_SOURCE_DIR}/onelua.c)
        target_compile_definitions(lua PRIVATE MAKE_LIB)
        target_include_directories(lua PUBLIC ${lua_SOURCE_DIR})
        if(MSVC)
            target_compile_options(lua PRIVATE /W3)
        endif()
    endif()
endif()

# Find OpenGL
find_package(OpenGL REQUIRED)

# Glad OpenGL 4.5+ DSA loader library (Shared DLL so all engine modules share single function table)
add_library(glad SHARED src/glad/src/gl.c)
target_include_directories(glad PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src/glad/include)
target_compile_definitions(glad PRIVATE GLAD_API_CALL_EXPORT GLAD_API_CALL_EXPORT_BUILD)
target_compile_definitions(glad INTERFACE GLAD_API_CALL_EXPORT)

# Common include directories
set(LAB_INCLUDES
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src/glad/include
    ${CMAKE_CURRENT_SOURCE_DIR}/external/lua
)

# =============================================================================
# MODULAR LAB ENGINE DYNAMIC LINK LIBRARIES (.DLL)
# =============================================================================

# 1. LabCore.dll - Core application lifecycle, Input, Math, Camera, Fonts, Dialogs
add_library(LabCore SHARED
    src/LabCore.cpp
    include/LabCore.h
    include/LabMath.h
    include/LabCamera.h
    include/LabFont.h
    include/LabDialogs.h
    include/LabExport.h
)
target_include_directories(LabCore PUBLIC ${LAB_INCLUDES})
target_compile_options(LabCore PRIVATE ${COMPILER_FLAGS})
target_compile_definitions(LabCore PRIVATE LAB_CORE_BUILD)
target_link_libraries(LabCore PUBLIC glfw glad OpenGL::GL)
if(WIN32)
    target_link_libraries(LabCore PUBLIC gdi32 comdlg32)
endif()

# 2. LabRender.dll - OpenGL 4.5+ DSA Renderer, Lighting, CSG, Decals, Post-process, Skeletal
add_library(LabRender SHARED
    src/LabRenderer.cpp
    src/LabLight.cpp
    src/LabPostProcess.cpp
    src/LabCSG.cpp
    src/LabDecals.cpp
    src/LabFace.cpp
    src/LabSkeletal.cpp
    include/LabRenderer.h
    include/LabLight.h
    include/LabPostProcess.h
    include/LabCSG.h
    include/LabDecals.h
    include/LabFace.h
    include/LabSkeletal.h
)
target_include_directories(LabRender PUBLIC ${LAB_INCLUDES})
target_compile_options(LabRender PRIVATE ${COMPILER_FLAGS})
target_link_libraries(LabRender PUBLIC LabCore glad OpenGL::GL)

# 3. LabAudio.dll - 3D Spatial Audio Engine, Procedural WAV Synthesizer
add_library(LabAudio SHARED
    src/LabAudio.cpp
    include/LabAudio.h
)
target_include_directories(LabAudio PUBLIC ${LAB_INCLUDES})
target_compile_options(LabAudio PRIVATE ${COMPILER_FLAGS})
target_link_libraries(LabAudio PUBLIC LabCore)

# 4. LabPhysics.dll - AABB Collision Detection, MoveAndSlide, Dynamic Rigid Bodies, Raycasting
add_library(LabPhysics SHARED
    src/LabPhysics.cpp
    src/LabCollision.cpp
    src/LabCombat.cpp
    include/LabPhysics.h
    include/LabCollision.h
    include/LabCombat.h
)
target_include_directories(LabPhysics PUBLIC ${LAB_INCLUDES})
target_compile_options(LabPhysics PRIVATE ${COMPILER_FLAGS})
target_link_libraries(LabPhysics PUBLIC LabCore LabRender LabAudio)

# 5. LabAnimation.dll - Keyframe Animations & Tactical FPP Procedural Arms
add_library(LabAnimation SHARED
    src/LabAnim.cpp
    src/LabArms.cpp
    include/LabAnim.h
    include/LabArms.h
)
target_include_directories(LabAnimation PUBLIC ${LAB_INCLUDES})
target_compile_options(LabAnimation PRIVATE ${COMPILER_FLAGS})
target_link_libraries(LabAnimation PUBLIC LabCore LabRender)

# 6. LabWorld.dll - .LABMAP Format, Spawner Pads, Weapon Arsenal, Particles, Doors
add_library(LabWorld SHARED
    src/LabMap.cpp
    src/LabPickups.cpp
    src/LabWeapon.cpp
    src/LabParticles.cpp
    src/LabInteractive.cpp
    include/LabMap.h
    include/LabPickups.h
    include/LabWeapon.h
    include/LabParticles.h
    include/LabInteractive.h
    include/LabSession.h
    include/LabHUD.h
)
target_include_directories(LabWorld PUBLIC ${LAB_INCLUDES})
target_compile_options(LabWorld PRIVATE ${COMPILER_FLAGS})
target_link_libraries(LabWorld PUBLIC LabCore LabRender LabPhysics LabAudio LabAnimation)

# 7. LabNetwork.dll - Client Prediction, UDP Sockets, Dedicated Server, Server Browser, Chat
add_library(LabNetwork SHARED
    src/LabNetwork.cpp
    src/LabChat.cpp
    include/LabNetwork.h
    include/LabChat.h
)
target_include_directories(LabNetwork PUBLIC ${LAB_INCLUDES})
target_compile_options(LabNetwork PRIVATE ${COMPILER_FLAGS})
target_link_libraries(LabNetwork PUBLIC LabCore LabRender LabWorld LabPhysics)
if(WIN32)
    target_link_libraries(LabNetwork PUBLIC ws2_32)
endif()

# 8. LabAI.dll - CombatBot Sensory AI, State Machine, Combat Behaviors
add_library(LabAI SHARED
    src/LabAI.cpp
    include/LabAI.h
)
target_include_directories(LabAI PUBLIC ${LAB_INCLUDES})
target_compile_options(LabAI PRIVATE ${COMPILER_FLAGS})
target_link_libraries(LabAI PUBLIC LabCore LabRender LabPhysics LabAnimation LabWorld LabNetwork)

# 9. LabScript.dll - Lua 5.4 Scripting Engine Integration
add_library(LabScript SHARED
    src/LabScript.cpp
    include/LabScript.h
)
target_include_directories(LabScript PUBLIC ${LAB_INCLUDES})
target_compile_options(LabScript PRIVATE ${COMPILER_FLAGS})
target_link_libraries(LabScript PUBLIC LabCore LabWorld lua)

# 10. LabStudioCore.dll - Character & Weapon Studio Engine Backend
add_library(LabStudioCore SHARED
    src/LabStudio.cpp
    include/LabStudio.h
)
target_include_directories(LabStudioCore PUBLIC ${LAB_INCLUDES})
target_compile_options(LabStudioCore PRIVATE ${COMPILER_FLAGS})
target_link_libraries(LabStudioCore PUBLIC LabCore LabRender LabAnimation LabWorld LabAI)

# -----------------------------------------------------------------------------
# Unified Interface Target: LabEngineLib
# -----------------------------------------------------------------------------
add_library(LabEngineLib INTERFACE)
target_link_libraries(LabEngineLib INTERFACE
    LabCore
    LabRender
    LabAudio
    LabPhysics
    LabAnimation
    LabWorld
    LabAI
    LabNetwork
    LabScript
    LabStudioCore
)

# =============================================================================
# EXECUTABLE TARGETS (LINKED AGAINST MODULAR LAB ENGINE DLLS)
# =============================================================================

# 1. Lab Engine SDK Showcase / Sandbox Executable (Lab.exe)
add_executable(Lab src/engine_demo.cpp)
target_compile_options(Lab PRIVATE ${COMPILER_FLAGS})
target_link_libraries(Lab PRIVATE LabEngineLib)

# 2. Frozen-Life Flagship FPS Demo Game Executable (FrozenLife.exe)
add_executable(FrozenLife src/main.cpp)
target_compile_options(FrozenLife PRIVATE ${COMPILER_FLAGS})
target_link_libraries(FrozenLife PRIVATE LabEngineLib)

# 3. Standalone Hammer Map Editor Executable
add_executable(LabHammer src/editor_main.cpp)
target_compile_options(LabHammer PRIVATE ${COMPILER_FLAGS})
target_link_libraries(LabHammer PRIVATE LabEngineLib)

# 4. Standalone Headless Dedicated Server Executable
add_executable(LabServer src/server_main.cpp)
target_compile_options(LabServer PRIVATE ${COMPILER_FLAGS})
target_link_libraries(LabServer PRIVATE LabEngineLib)

# 5. Standalone Character & Weapon Studio Executable
add_executable(LabStudio src/studio_main.cpp)
target_compile_options(LabStudio PRIVATE ${COMPILER_FLAGS})
target_link_libraries(LabStudio PRIVATE LabEngineLib)

# 6. Full Engine Automated Verification Suite
add_executable(TestVerify src/test_verify.cpp)
target_compile_options(TestVerify PRIVATE ${COMPILER_FLAGS})
target_link_libraries(TestVerify PRIVATE LabEngineLib)

# Post-build asset replication to output directories
set(ASSET_TARGETS Lab FrozenLife LabHammer LabStudio TestVerify)
foreach(tgt ${ASSET_TARGETS})
    add_custom_command(TARGET ${tgt} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/assets"
            "$<TARGET_FILE_DIR:${tgt}>/assets"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/fonts"
            "$<TARGET_FILE_DIR:${tgt}>/fonts"
    )
endforeach()
