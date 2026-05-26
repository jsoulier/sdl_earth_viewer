cmake_minimum_required(VERSION 3.24)
project(sdl_earth_viewer)

set(BINARY_DIR ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${BINARY_DIR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${BINARY_DIR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${BINARY_DIR})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${BINARY_DIR})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_RELEASE ${BINARY_DIR})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_DEBUG ${BINARY_DIR})
make_directory(${BINARY_DIR})

configure_file(LICENSE.txt ${BINARY_DIR} COPYONLY)
configure_file(README.md ${BINARY_DIR} COPYONLY)

# https://github.com/CesiumGS/cesium-native/issues/1214
set(VCPKG_PLATFORM_TOOLSET_VERSION "14.38")

set(CESIUM_TESTS_ENABLED OFF)
set(GLM_BUILD_LIBRARY OFF)

add_subdirectory(lib/SDL)
add_subdirectory(lib/cesium-native)
add_subdirectory(lib/glm)
add_subdirectory(lib/imgui)
add_subdirectory(lib/json)
add_executable(sdl_earth_viewer WIN32
    src/camera.cpp
    src/debug_group.cpp
    src/main.cpp
    src/prepare_renderer_resources.cpp
    src/shader.cpp
    src/task_processor.cpp
    src/tileset.cpp
)
target_link_libraries(sdl_earth_viewer PRIVATE
    Cesium3DTilesSelection
    CesiumAsync
    CesiumCurl
    CesiumGeometry
    CesiumGeospatial
    CesiumQuantizedMeshTerrain
    CesiumUtility
    SDL3::SDL3
    glm::glm
    imgui
    nlohmann_json
)
target_compile_definitions(sdl_earth_viewer PRIVATE GLM_FORCE_DEPTH_ZERO_TO_ONE)
set_target_properties(sdl_earth_viewer PROPERTIES CXX_STANDARD 23)
target_precompile_headers(sdl_earth_viewer PRIVATE
    <Cesium3DTilesSelection/BoundingVolume.h>
    <Cesium3DTilesSelection/IPrepareRendererResources.h>
    <Cesium3DTilesSelection/Tile.h>
    <Cesium3DTilesSelection/Tileset.h>
    <Cesium3DTilesSelection/TilesetExternals.h>
    <Cesium3DTilesSelection/ViewState.h>
    <CesiumAsync/AsyncSystem.h>
    <CesiumAsync/ITaskProcessor.h>
    <CesiumCurl/CurlAssetAccessor.h>
    <CesiumGltf/AccessorView.h>
    <CesiumGltf/Model.h>
    <CesiumRasterOverlays/IonRasterOverlay.h>
    <CesiumRasterOverlays/RasterOverlayTile.h>
    <CesiumUtility/CreditSystem.h>
    <SDL3/SDL.h>
    <algorithm>
    <any>
    <array>
    <cmath>
    <cstdint>
    <cstring>
    <exception>
    <filesystem>
    <format>
    <fstream>
    <functional>
    <glm/glm.hpp>
    <glm/gtc/matrix_transform.hpp>
    <glm/gtc/type_ptr.hpp>
    <imgui.h>
    <imgui_impl_sdl3.h>
    <imgui_impl_sdlgpu3.h>
    <iterator>
    <limits>
    <memory>
    <mutex>
    <nlohmann/json.hpp>
    <spdlog/sinks/base_sink.h>
    <spdlog/spdlog.h>
    <string>
    <string_view>
    <thread>
    <utility>
    <variant>
    <vector>
)

find_program(SHADERCROSS shadercross)
function(add_shader FILE)
    set(DEPENDS ${ARGN})
    set(HLSL ${CMAKE_SOURCE_DIR}/shaders/${FILE})
    set(SPV ${CMAKE_SOURCE_DIR}/shaders/bin/${FILE}.spv)
    set(DXIL ${CMAKE_SOURCE_DIR}/shaders/bin/${FILE}.dxil)
    set(MSL ${CMAKE_SOURCE_DIR}/shaders/bin/${FILE}.msl)
    set(JSON ${CMAKE_SOURCE_DIR}/shaders/bin/${FILE}.json)
    function(compile OUTPUT)
        add_custom_command(
            OUTPUT ${OUTPUT}
            COMMAND ${SHADERCROSS} ${HLSL} -s hlsl -o ${OUTPUT} -I shaders
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
            DEPENDS ${HLSL} ${DEPENDS}
            COMMENT ${OUTPUT}
        )
        get_filename_component(NAME ${OUTPUT} NAME)
        string(REPLACE . _ NAME ${NAME})
        set(NAME compile_${NAME})
        add_custom_target(${NAME} DEPENDS ${OUTPUT})
        add_dependencies(sdl_earth_viewer ${NAME})
    endfunction()
    if (EXISTS ${SHADERCROSS})
        compile(${SPV})
        compile(${DXIL})
        compile(${MSL})
        compile(${JSON})
    else()
        message("Using prebuilts since SDL_shadercross is missing")
    endif()
    function(package OUTPUT)
        get_filename_component(NAME ${OUTPUT} NAME)
        set(BINARY ${BINARY_DIR}/${NAME})
        add_custom_command(
            OUTPUT ${BINARY}
            COMMAND ${CMAKE_COMMAND} -E copy ${OUTPUT} ${BINARY}
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
            DEPENDS ${OUTPUT}
            COMMENT ${BINARY}
        )
        string(REPLACE . _ NAME ${NAME})
        set(NAME package_${NAME})
        add_custom_target(${NAME} DEPENDS ${BINARY})
        add_dependencies(sdl_earth_viewer ${NAME})
    endfunction()
    if(WIN32)
        package(${DXIL})
    endif()
    if(APPLE)
        package(${MSL})
    else()
        package(${SPV})
    endif()
    package(${JSON})
endfunction()
add_shader(atmosphere.frag)
add_shader(axes.frag)
add_shader(axes.vert)
add_shader(fullscreen.vert)
add_shader(space.frag)
add_shader(tileset.frag)
add_shader(tileset.vert)
