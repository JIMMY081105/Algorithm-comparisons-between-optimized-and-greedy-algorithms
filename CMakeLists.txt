cmake_minimum_required(VERSION 3.16)
project(SmartWasteClearance VERSION 1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ========================================
# Find GLFW
# ========================================
# If using vcpkg, pass -DCMAKE_TOOLCHAIN_FILE=[vcpkg]/scripts/buildsystems/vcpkg.cmake
find_package(glfw3 3.3 REQUIRED)

# ========================================
# Find OpenGL
# ========================================
find_package(OpenGL REQUIRED)

# ========================================
# GLAD — OpenGL function loader
# ========================================
add_library(glad STATIC
    external/glad/src/glad.c
)
target_include_directories(glad PUBLIC
    external/glad/include
)

# ========================================
# Dear ImGui — immediate mode GUI library
# ========================================
add_library(imgui STATIC
    external/imgui/imgui.cpp
    external/imgui/imgui_draw.cpp
    external/imgui/imgui_tables.cpp
    external/imgui/imgui_widgets.cpp
    external/imgui/imgui_demo.cpp
    external/imgui/imgui_impl_glfw.cpp
    external/imgui/imgui_impl_opengl3.cpp
)
target_include_directories(imgui PUBLIC
    external/imgui
    external/glad/include
)
target_link_libraries(imgui PUBLIC
    glfw
    OpenGL::GL
    glad
)

# ========================================
# Main application
# ========================================
set(CORE_SOURCES
    src/core/WasteNode.cpp
    src/core/Truck.cpp
    src/core/RouteResult.cpp
    src/core/CostModel.cpp
    src/core/MapGraph.cpp
    src/core/WasteSystem.cpp
    src/core/EventLog.cpp
)

set(ALGORITHM_SOURCES
    src/algorithms/RegularRoute.cpp
    src/algorithms/GreedyRoute.cpp
    src/algorithms/MSTRoute.cpp
    src/algorithms/TSPRoute.cpp
    src/algorithms/ComparisonManager.cpp
)

set(VISUALIZATION_SOURCES
    src/visualization/RenderUtils.cpp
    src/visualization/IsometricRenderer.cpp
    src/visualization/OceanShader.cpp
    src/visualization/AnimationController.cpp
    src/visualization/DashboardStyle.cpp
    src/visualization/DashboardUI.cpp
)

set(PERSISTENCE_SOURCES
    src/persistence/FileExporter.cpp
    src/persistence/ResultLogger.cpp
)

set(APP_SOURCES
    src/app/Application.cpp
    src/main.cpp
)

add_executable(SmartWasteClearance
    ${CORE_SOURCES}
    ${ALGORITHM_SOURCES}
    ${VISUALIZATION_SOURCES}
    ${PERSISTENCE_SOURCES}
    ${APP_SOURCES}
)

target_include_directories(SmartWasteClearance PRIVATE
    include
    external/imgui
    external/glad/include
)

target_link_libraries(SmartWasteClearance PRIVATE
    glfw
    OpenGL::GL
    glad
    imgui
)

# On Windows, suppress the console window for Release builds
if(WIN32)
    set_target_properties(SmartWasteClearance PROPERTIES
        WIN32_EXECUTABLE $<CONFIG:Release>
    )
    # But keep console for Debug builds so we can see error output
endif()

# Copy output directory structure for exports
add_custom_command(TARGET SmartWasteClearance POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
    "$<TARGET_FILE_DIR:SmartWasteClearance>/output"
)
