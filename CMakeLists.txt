cmake_minimum_required(VERSION 3.16)
project(SmartWasteClearance VERSION 1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(POLICY CMP0135)
    cmake_policy(SET CMP0135 NEW)
endif()

# ========================================
# GLFW - bundled source, with download fallback
# ========================================
set(GLFW_BUILD_DOCS     OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS    OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL        OFF CACHE BOOL "" FORCE)

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/external/glfw/CMakeLists.txt")
    add_subdirectory(external/glfw)
else()
    include(FetchContent)
    FetchContent_Declare(
        glfw
        URL https://github.com/glfw/glfw/archive/refs/tags/3.3.9.zip
    )
    FetchContent_MakeAvailable(glfw)
endif()

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
# Dear ImGui - immediate mode GUI library
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
    src/algorithms/DijkstraRoute.cpp
    src/algorithms/BellmanFordRoute.cpp
    src/algorithms/FloydWarshallRoute.cpp
    src/algorithms/ComparisonManager.cpp
)

set(VISUALIZATION_SOURCES
    src/visualization/RenderUtils.cpp
    src/visualization/CityRenderUtils.cpp
    src/visualization/CityBatchGeometry.cpp
    src/visualization/CityShapeDrawing.cpp
    src/visualization/IsometricRenderer.cpp
    src/visualization/SeaThemeRenderer.cpp
    src/visualization/SeaThemeRendererHelpers.cpp
    src/visualization/SeaThemeRendererTransit.cpp
    src/visualization/SeaThemeRendererGround.cpp
    src/visualization/SeaThemeRendererNodes.cpp
    src/visualization/SeaThemeRendererBoat.cpp
    src/visualization/SeaThemeRendererDecor.cpp
    src/visualization/CityThemeRenderer.cpp
    src/visualization/CityThemeRendererGround.cpp
    src/visualization/CityThemeRendererTransit.cpp
    src/visualization/CityThemeRendererActors.cpp
    src/visualization/CityThemeRendererNetwork.cpp
    src/visualization/CityThemeRendererAssets.cpp
    src/visualization/CityThemeRendererTraffic.cpp
    src/visualization/CityThemeRendererRoutes.cpp
    src/visualization/CityThemeRendererBuildings.cpp
    src/visualization/CityThemeRendererEnvironment.cpp
    src/visualization/CityAssetLibrary.cpp
    src/visualization/OceanShader.cpp
    src/visualization/AnimationController.cpp
    src/visualization/DashboardStyle.cpp
    src/visualization/DashboardUI.cpp
    src/visualization/DashboardPanels.cpp
    src/visualization/ChatbotPanel.cpp
)

set(AI_SOURCES
    src/ai/ChatbotService.cpp
)

set(ENVIRONMENT_SOURCES
    src/environment/EnvironmentController.cpp
    src/environment/MissionPresentationUtils.cpp
    src/environment/SeasonProfile.cpp
)

set(PERSISTENCE_SOURCES
    src/persistence/FileExporter.cpp
    src/persistence/ResultLogger.cpp
)

set(APP_SOURCES
    src/app/Application.cpp
    src/app/ApplicationActions.cpp
    src/app/ApplicationChatbot.cpp
    src/main.cpp
)

add_executable(SmartWasteClearance
    ${CORE_SOURCES}
    ${ALGORITHM_SOURCES}
    ${ENVIRONMENT_SOURCES}
    ${VISUALIZATION_SOURCES}
    ${PERSISTENCE_SOURCES}
    ${AI_SOURCES}
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

if(WIN32)
    target_link_libraries(SmartWasteClearance PRIVATE wininet)
endif()

if(MINGW)
    get_filename_component(MINGW_BIN_DIR "${CMAKE_CXX_COMPILER}" DIRECTORY)
    set(MINGW_RUNTIME_DLLS
        "${MINGW_BIN_DIR}/libgcc_s_seh-1.dll"
        "${MINGW_BIN_DIR}/libstdc++-6.dll"
        "${MINGW_BIN_DIR}/libwinpthread-1.dll"
    )

    function(copy_mingw_runtime_dlls target_name)
        foreach(runtime_dll IN LISTS MINGW_RUNTIME_DLLS)
            if(EXISTS "${runtime_dll}")
                add_custom_command(TARGET ${target_name} POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${runtime_dll}"
                    "$<TARGET_FILE_DIR:${target_name}>"
                )
            endif()
        endforeach()
    endfunction()

    copy_mingw_runtime_dlls(SmartWasteClearance)
endif()

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

enable_testing()

add_executable(EnvironmentCoreTests
    tests/EnvironmentCoreTests.cpp
    ${CORE_SOURCES}
    ${ALGORITHM_SOURCES}
    ${ENVIRONMENT_SOURCES}
    src/visualization/RenderUtils.cpp
    src/visualization/CityRenderUtils.cpp
    src/visualization/CityBatchGeometry.cpp
    src/visualization/CityShapeDrawing.cpp
    src/visualization/IsometricRenderer.cpp
    src/visualization/SeaThemeRenderer.cpp
    src/visualization/SeaThemeRendererHelpers.cpp
    src/visualization/SeaThemeRendererTransit.cpp
    src/visualization/SeaThemeRendererGround.cpp
    src/visualization/SeaThemeRendererNodes.cpp
    src/visualization/SeaThemeRendererBoat.cpp
    src/visualization/SeaThemeRendererDecor.cpp
    src/visualization/CityThemeRenderer.cpp
    src/visualization/CityThemeRendererGround.cpp
    src/visualization/CityThemeRendererTransit.cpp
    src/visualization/CityThemeRendererActors.cpp
    src/visualization/CityThemeRendererNetwork.cpp
    src/visualization/CityThemeRendererAssets.cpp
    src/visualization/CityThemeRendererTraffic.cpp
    src/visualization/CityThemeRendererRoutes.cpp
    src/visualization/CityThemeRendererBuildings.cpp
    src/visualization/CityThemeRendererEnvironment.cpp
    src/visualization/CityAssetLibrary.cpp
    src/visualization/OceanShader.cpp
)

target_include_directories(EnvironmentCoreTests PRIVATE
    include
    external/imgui
    external/glad/include
)

target_link_libraries(EnvironmentCoreTests PRIVATE
    glfw
    OpenGL::GL
    glad
    imgui
)

if(MINGW)
    copy_mingw_runtime_dlls(EnvironmentCoreTests)
endif()

add_test(NAME EnvironmentCoreTests COMMAND EnvironmentCoreTests)

file(GLOB_RECURSE BACKEND_TEST_CASE_SOURCES CONFIGURE_DEPENDS
    tests/backend/algorithms/*.cpp
    tests/backend/comparison/*.cpp
    tests/backend/core/*.cpp
    tests/backend/graph/*.cpp
    tests/backend/integration/*.cpp
    tests/backend/persistence/*.cpp
)

add_executable(BackendLogicTests
    tests/backend/BackendLogicTestMain.cpp
    ${BACKEND_TEST_CASE_SOURCES}
    ${CORE_SOURCES}
    ${ALGORITHM_SOURCES}
    ${PERSISTENCE_SOURCES}
)

target_include_directories(BackendLogicTests PRIVATE
    include
    tests/backend/common
)

if(MINGW)
    copy_mingw_runtime_dlls(BackendLogicTests)
endif()

add_test(NAME BackendLogicTests COMMAND BackendLogicTests)

add_custom_target(smoke_dashboard
    DEPENDS SmartWasteClearance EnvironmentCoreTests BackendLogicTests
)
