cmake_minimum_required(VERSION 3.15)
project(LabEngine VERSION 0.1.0 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Strict compiler flags
if(MSVC)
    set(COMPILER_FLAGS /W4 /WX /EHsc)
else()
    set(COMPILER_FLAGS -Wall -Wextra -Wpedantic -Werror)
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

# Include directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src/glad/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/external/lua)

# Engine files
set(ENGINE_SOURCES
    src/LabCore.cpp
    src/LabRenderer.cpp
    src/LabMap.cpp
    src/LabAnim.cpp
    src/LabCombat.cpp
    src/LabAI.cpp
    src/LabCollision.cpp
    src/LabChat.cpp
    src/LabPickups.cpp
    src/LabWeapon.cpp
    src/LabParticles.cpp
    src/LabAudio.cpp
    src/LabArms.cpp
    src/LabLight.cpp
    src/LabSkeletal.cpp
    src/LabPhysics.cpp
    src/LabCSG.cpp
    src/LabDecals.cpp
    src/LabInteractive.cpp
    src/LabScript.cpp
    src/LabPostProcess.cpp
    src/LabFace.cpp
    src/LabNetwork.cpp
    src/LabStudio.cpp
    src/glad/src/gl.c
)

set(ENGINE_HEADERS
    include/LabCore.h
    include/LabRenderer.h
    include/LabMath.h
    include/LabCamera.h
    include/LabMap.h
    include/LabAnim.h
    include/LabSession.h
    include/LabCombat.h
    include/LabAI.h
    include/LabCollision.h
    include/LabChat.h
    include/LabPickups.h
    include/LabWeapon.h
    include/LabParticles.h
    include/LabAudio.h
    include/LabArms.h
    include/LabLight.h
    include/LabSkeletal.h
    include/LabPhysics.h
    include/LabCSG.h
    include/LabDecals.h
    include/LabInteractive.h
    include/LabScript.h
    include/LabPostProcess.h
    include/LabFace.h
    include/LabNetwork.h
    include/LabStudio.h
    include/Lab.h
)

# Create an engine library
add_library(LabEngineLib STATIC ${ENGINE_SOURCES} ${ENGINE_HEADERS})
target_compile_options(LabEngineLib PRIVATE ${COMPILER_FLAGS})
target_include_directories(LabEngineLib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_include_directories(LabEngineLib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src/glad/include)
target_include_directories(LabEngineLib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/external/lua)
target_link_libraries(LabEngineLib PUBLIC glfw OpenGL::GL lua)

# Game executable
add_executable(Lab src/main.cpp)
target_compile_options(Lab PRIVATE ${COMPILER_FLAGS})
target_link_libraries(Lab PRIVATE LabEngineLib)

# Standalone Hammer Map Editor executable
add_executable(LabHammer src/editor_main.cpp)
target_compile_options(LabHammer PRIVATE ${COMPILER_FLAGS})
target_link_libraries(LabHammer PRIVATE LabEngineLib)

# Standalone Headless Dedicated Server executable
add_executable(LabServer src/server_main.cpp)
target_compile_options(LabServer PRIVATE ${COMPILER_FLAGS})
target_link_libraries(LabServer PRIVATE LabEngineLib)

# Verification executable
add_executable(TestVerify src/test_verify.cpp)
target_compile_options(TestVerify PRIVATE ${COMPILER_FLAGS})
target_link_libraries(TestVerify PRIVATE LabEngineLib)

# Standalone Character & Weapon Studio executable (Valve Hammer styled)
add_executable(LabStudio src/studio_main.cpp)
target_compile_options(LabStudio PRIVATE ${COMPILER_FLAGS})
target_link_libraries(LabStudio PRIVATE LabEngineLib)

# Windows system libraries
if(WIN32)
    target_link_libraries(LabEngineLib PUBLIC gdi32 comdlg32 ws2_32)
endif()

# Automatically copy entire assets and fonts directory into output directory
add_custom_command(TARGET Lab POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/assets"
        "$<TARGET_FILE_DIR:Lab>/assets"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/fonts"
        "$<TARGET_FILE_DIR:Lab>/fonts"
)

add_custom_command(TARGET LabHammer POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/assets"
        "$<TARGET_FILE_DIR:LabHammer>/assets"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/fonts"
        "$<TARGET_FILE_DIR:LabHammer>/fonts"
)

add_custom_command(TARGET LabStudio POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/assets"
        "$<TARGET_FILE_DIR:LabStudio>/assets"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/fonts"
        "$<TARGET_FILE_DIR:LabStudio>/fonts"
)

