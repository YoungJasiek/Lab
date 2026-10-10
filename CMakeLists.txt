cmake_minimum_required(VERSION 3.15)
project(LabEngine VERSION 0.1.0 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Enable automatic symbol export for Windows dynamic link libraries (.dll)
set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

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
set(GLFW_BUILD_WAYLAND OFF CACHE BOOL "" FORCE)
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

# 1. LabCore.dll - Core application lifecycle, Input, Math, Camera, Fonts, Dialogs, VFS, Asset Pipeline, Command/Undo, Export
add_library(LabCore SHARED
    src/LabCore.cpp
    src/LabInput.cpp
    src/LabPlugin.cpp
    src/LabEvent.cpp
    src/LabSceneNode.cpp
    src/LabVFS.cpp
    src/LabAsset.cpp
    src/LabCommand.cpp
    src/LabExportPipeline.cpp
    src/LabProfiler.cpp
    src/LabVCS.cpp
    src/LabPackage.cpp
    src/LabProject.cpp
    src/plugins/ProceduralLevelPlugin.cpp
    src/plugins/SpeedrunTelemetryPlugin.cpp
    src/plugins/RetroPostProcessPlugin.cpp
    src/plugins/DialogueQuestPlugin.cpp
    src/plugins/VoxelTerrainPlugin.cpp
    src/plugins/AINavigationPatrolPlugin.cpp
    src/plugins/ArcadeVehiclePlugin.cpp
    src/plugins/DiscordRPCPlugin.cpp
    src/plugins/LuaDropinPlugin.cpp
    include/LabCore.h
    include/LabInput.h
    include/LabPlugin.h
    include/LabEvent.h
    include/LabSceneNode.h
    include/LabVFS.h
    include/LabAsset.h
    include/LabCommand.h
    include/LabExportPipeline.h
    include/LabProfiler.h
    include/LabVCS.h
    include/LabPackage.h
    include/LabProject.h
    include/plugins/ProceduralLevelPlugin.h
    include/plugins/SpeedrunTelemetryPlugin.h
    include/plugins/RetroPostProcessPlugin.h
    include/plugins/DialogueQuestPlugin.h
    include/plugins/VoxelTerrainPlugin.h
    include/plugins/AINavigationPatrolPlugin.h
    include/plugins/ArcadeVehiclePlugin.h
    include/plugins/DiscordRPCPlugin.h
    include/plugins/LuaDropinPlugin.h
    include/LabApplication.h
    include/LabMath.h
    include/LabComponent.h
    include/LabTransform.h
    include/LabEntity.h
    include/LabCamera.h
    include/LabCameraScripts.h
    include/LabComponents.h
    include/LabFont.h
    include/LabDialogs.h
    include/LabExport.h
)
target_include_directories(LabCore PUBLIC ${LAB_INCLUDES})
target_compile_options(LabCore PRIVATE ${COMPILER_FLAGS})
target_compile_definitions(LabCore PRIVATE LAB_CORE_BUILD)
target_link_libraries(LabCore PUBLIC glfw glad OpenGL::GL lua)
if(WIN32)
    target_link_libraries(LabCore PUBLIC gdi32 comdlg32)
elseif(UNIX AND NOT APPLE)
    target_link_libraries(LabCore PUBLIC ${CMAKE_DL_LIBS} pthread m)
endif()

# 2. LabRender.dll - OpenGL 4.5+ DSA Renderer, Lighting, CSG, Decals, Post-process, Skeletal, 2D SpriteBatcher, RHI & Render Graph
add_library(LabRender SHARED
    src/LabRenderer.cpp
    src/LabTheme.cpp
    src/LabAsset2D.cpp
    src/LabSpriteBatcher.cpp
    src/LabRHI.cpp
    src/LabLight.cpp
    src/LabPostProcess.cpp
    src/LabCSG.cpp
    src/LabDecals.cpp
    src/LabFace.cpp
    src/LabSkeletal.cpp
    src/LabGraphics.cpp
    include/LabRenderer.h
    include/LabTheme.h
    include/LabAsset2D.h
    include/LabSpriteBatcher.h
    include/LabRHI.h
    include/LabLight.h
    include/LabPostProcess.h
    include/LabCSG.h
    include/LabDecals.h
    include/LabFace.h
    include/LabSkeletal.h
    include/LabGraphics.h
)
target_include_directories(LabRender PUBLIC ${LAB_INCLUDES})
target_compile_options(LabRender PRIVATE ${COMPILER_FLAGS})
target_compile_definitions(LabRender PRIVATE LAB_RENDER_BUILD)
target_link_libraries(LabRender PUBLIC LabCore glad OpenGL::GL)

# 3. LabAudio.dll - 3D Spatial Audio Engine, Procedural WAV Synthesizer
add_library(LabAudio SHARED
    src/LabAudio.cpp
    include/LabAudio.h
)
target_include_directories(LabAudio PUBLIC ${LAB_INCLUDES})
target_compile_options(LabAudio PRIVATE ${COMPILER_FLAGS})
target_link_libraries(LabAudio PUBLIC LabCore)
if(UNIX AND NOT APPLE)
    target_link_libraries(LabAudio PUBLIC ${CMAKE_DL_LIBS} pthread m)
endif()

# 4. LabPhysics.dll - AABB Collision Detection, MoveAndSlide, Dynamic Rigid Bodies, Raycasting, PhysicsWorld2D, CCD, Ragdoll, Vehicle, Fluid, Constraints
add_library(LabPhysics SHARED
    src/LabPhysics.cpp
    src/LabPhysics2D.cpp
    src/LabCCD.cpp
    src/LabRagdoll.cpp
    src/LabVehicle.cpp
    src/LabFluid.cpp
    src/LabConstraint.cpp
    src/LabCloth.cpp
    src/LabForceField.cpp
    src/LabCollision.cpp
    src/LabCombat.cpp
    include/LabPhysics.h
    include/LabPhysics2D.h
    include/LabCCD.h
    include/LabRagdoll.h
    include/LabVehicle.h
    include/LabFluid.h
    include/LabConstraint.h
    include/LabCloth.h
    include/LabForceField.h
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
    src/LabScene.cpp
    src/LabPickups.cpp
    src/LabWeapon.cpp
    src/LabParticles.cpp
    src/LabInteractive.cpp
    include/LabMap.h
    include/LabScene.h
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
    target_link_libraries(LabNetwork PUBLIC ws2_32 wininet)
elseif(UNIX AND NOT APPLE)
    target_link_libraries(LabNetwork PUBLIC ${CMAKE_DL_LIBS} pthread m)
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

# 11. LabHUDEditorCore.dll - 2D/HUD/GUI Editor Engine Backend
add_library(LabHUDEditorCore SHARED
    src/LabHUDEditor.cpp
    include/LabHUDEditor.h
)
target_include_directories(LabHUDEditorCore PUBLIC ${LAB_INCLUDES})
target_compile_options(LabHUDEditorCore PRIVATE ${COMPILER_FLAGS})
target_link_libraries(LabHUDEditorCore PUBLIC LabCore LabRender LabAnimation LabWorld)

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
    LabHUDEditorCore
)

# =============================================================================
# EXECUTABLE TARGETS (LINKED AGAINST MODULAR LAB ENGINE DLLS)
# =============================================================================

# 1. Standalone Lab Engine Editor Executable (LabEditor.exe - Primary LabSDK Tool)
add_executable(LabEditor src/editor_main.cpp)
target_compile_options(LabEditor PRIVATE ${COMPILER_FLAGS})
target_link_libraries(LabEditor PRIVATE LabEngineLib)

# 2. Standalone Universal Game Player / Runner (LabPlayer.exe - Used for Exported Games)
add_executable(LabPlayer src/player_main.cpp)
target_compile_options(LabPlayer PRIVATE ${COMPILER_FLAGS})
target_link_libraries(LabPlayer PRIVATE LabEngineLib)

# 3. Frozen-Life Flagship FPS Demo Game Executable (FrozenLife.exe)
add_executable(FrozenLife src/main.cpp)
target_compile_options(FrozenLife PRIVATE ${COMPILER_FLAGS})
target_link_libraries(FrozenLife PRIVATE LabEngineLib)

# 4. Standalone Headless Dedicated Server Executable
add_executable(LabServer src/server_main.cpp)
target_compile_options(LabServer PRIVATE ${COMPILER_FLAGS})
target_link_libraries(LabServer PRIVATE LabEngineLib)

# 5. Full Engine Automated Verification Suite
add_executable(TestVerify src/test_verify.cpp)
target_compile_options(TestVerify PRIVATE ${COMPILER_FLAGS})
target_link_libraries(TestVerify PRIVATE LabEngineLib)

# Unified asset replication to output directory (avoids parallel race conditions across executables)
add_custom_target(LabAssets ALL
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/assets"
        "${CMAKE_CURRENT_BINARY_DIR}/$<IF:$<BOOL:$<CONFIG>>,$<CONFIG>/,>assets"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/fonts"
        "${CMAKE_CURRENT_BINARY_DIR}/$<IF:$<BOOL:$<CONFIG>>,$<CONFIG>/,>fonts"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/plugins"
        "${CMAKE_CURRENT_BINARY_DIR}/$<IF:$<BOOL:$<CONFIG>>,$<CONFIG>/,>plugins"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/logos"
        "${CMAKE_CURRENT_BINARY_DIR}/$<IF:$<BOOL:$<CONFIG>>,$<CONFIG>/,>logos"
    COMMENT "Synchronizing engine assets, fonts, plugins and logos to output directory"
)
set(ASSET_TARGETS LabEditor LabPlayer FrozenLife LabServer TestVerify)
foreach(tgt ${ASSET_TARGETS})
    add_dependencies(${tgt} LabAssets)
endforeach()

