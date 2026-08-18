cmake_minimum_required(VERSION 3.15)
project(Cygenus VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(glfw3 REQUIRED)
find_package(GLEW REQUIRED)
find_package(OpenGL REQUIRED)
find_package(lua REQUIRED)

# --- ImGui (local) ---
set(IMGUI_DIR ${CMAKE_SOURCE_DIR}/third_party/imgui)
set(IMGUI_BACKENDS_DIR ${IMGUI_DIR}/backends)

# --- ENGINE SOURCES (for main executable) ---
file(GLOB_RECURSE ENGINE_SOURCES "src/*.c" "src/*.cpp")

# --- IMGUI SOURCES ---
set(IMGUI_SOURCES
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_demo.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_BACKENDS_DIR}/imgui_impl_glfw.cpp
    ${IMGUI_BACKENDS_DIR}/imgui_impl_opengl3.cpp
)

# --- EDITOR EXECUTABLE ---
add_executable(cygenus
    ${ENGINE_SOURCES}
    ${IMGUI_SOURCES}
)

target_link_libraries(cygenus
    glfw
    GLEW::GLEW
    ${OPENGL_LIBRARIES}
    lua::lua
)

target_include_directories(cygenus PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${IMGUI_DIR}
    ${IMGUI_BACKENDS_DIR}
)

# --- macOS SPECIFIC ---
if(APPLE)
    target_link_libraries(cygenus
        "-framework Cocoa"
        "-framework IOKit"
        "-framework OpenGL"
    )
endif()

# --- Windows SPECIFIC ---
if(WIN32)
    target_compile_definitions(cygenus PRIVATE GLEW_STATIC)
endif()