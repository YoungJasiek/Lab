cmake_minimum_required(VERSION 3.15)
project(LabEngine VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Strict compiler flags
if(MSVC)
    set(COMPILER_FLAGS /W4 /WX /EHsc)
else()
    set(COMPILER_FLAGS -Wall -Wextra -Wpedantic -Werror)
endif()

# Find dependencies (OpenGL + GLUT for Sprint 1)
find_package(OpenGL REQUIRED)
find_package(GLUT REQUIRED)

# Include directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}) # for GL/freeglut.h locally if needed

# Engine files
set(ENGINE_SOURCES
    src/LabCore.cpp
    src/LabRenderer.cpp
)

set(ENGINE_HEADERS
    include/LabCore.h
    include/LabRenderer.h
    include/LabMath.h
    include/LabCamera.h
    include/Lab.h
)

# Create an engine library
add_library(LabEngineLib STATIC ${ENGINE_SOURCES} ${ENGINE_HEADERS})
target_compile_options(LabEngineLib PRIVATE ${COMPILER_FLAGS})
target_include_directories(LabEngineLib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(LabEngineLib PUBLIC ${OPENGL_LIBRARIES} ${GLUT_LIBRARIES})

# Game executable
add_executable(Lab src/main.cpp)
target_compile_options(Lab PRIVATE ${COMPILER_FLAGS})
target_link_libraries(Lab PRIVATE LabEngineLib)
