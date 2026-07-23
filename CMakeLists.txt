cmake_minimum_required(VERSION 3.24)

project(
    MegaDriveEnvironmentSampleGame
    VERSION 0.1.0
    DESCRIPTION "Dual-target sample game for MegaDriveEnvironment"
    LANGUAGES CXX
)

find_package(Python3 REQUIRED COMPONENTS Interpreter)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# MegaDriveEnvironment is a shared library by default. Do not re-link the sample
# executable merely because that dylib was rebuilt (implementation-only edits).
set(CMAKE_LINK_DEPENDS_NO_SHARED ON)

option(SAMPLE_ENABLE_WARNINGS "Enable useful compiler warnings for sample targets" ON)

set(MEGADRIVE_ENVIRONMENT_DIR
    "${CMAKE_CURRENT_SOURCE_DIR}/../MegaDriveEnvironment"
    CACHE PATH "Path to the MegaDriveEnvironment repository"
)

if(NOT EXISTS "${MEGADRIVE_ENVIRONMENT_DIR}/CMakeLists.txt")
    message(FATAL_ERROR
        "MegaDriveEnvironment was not found at '${MEGADRIVE_ENVIRONMENT_DIR}'. "
        "Clone it next to this repository or set MEGADRIVE_ENVIRONMENT_DIR."
    )
endif()

add_subdirectory(
    "${MEGADRIVE_ENVIRONMENT_DIR}"
    "${CMAKE_BINARY_DIR}/MegaDriveEnvironment"
)

include(cmake/ProjectOptions.cmake)
include(cmake/SourceManifest.cmake)

sample_read_shared_sources(SAMPLE_SHARED_SOURCES)

set(SAMPLE_GENERATED_DIR "${CMAKE_BINARY_DIR}/generated")
set(SAMPLE_ASSET_ROM "${CMAKE_BINARY_DIR}/sample_game_assets.bin")
set(SAMPLE_ASSET_LAYOUT_HEADER "${SAMPLE_GENERATED_DIR}/AssetLayout.hpp")
set(SAMPLE_ASSET_LAYOUT_JSON "${SAMPLE_GENERATED_DIR}/asset_layout.json")
set(SAMPLE_ASSET_PACK_BIN "${SAMPLE_GENERATED_DIR}/asset_pack.bin")
set(SAMPLE_ASSETS_WORK_DIR "${SAMPLE_GENERATED_DIR}/assets")
set(SAMPLE_FONT_DATA
    "${MEGADRIVE_ENVIRONMENT_DIR}/include/MegaDriveEnvironment/util/font/FontData.hpp"
)
set(SAMPLE_Z80_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/sound/z80/boing_ball_sfx.s")
set(SAMPLE_BOING_PCM "${CMAKE_CURRENT_SOURCE_DIR}/sound/amiga_assets/boing_pcm.bin")

include(cmake/Assets.cmake)

add_library(sample_game STATIC ${SAMPLE_SHARED_SOURCES})
add_library(SampleGame::Core ALIAS sample_game)
target_include_directories(sample_game PUBLIC
    include
    "${SAMPLE_GENERATED_DIR}"
)
target_compile_definitions(sample_game PUBLIC PC=1)
target_link_libraries(sample_game PUBLIC sample_project_options)
sample_enable_warnings(sample_game)
add_dependencies(sample_game sample_asset_rom)

add_library(sample_memory_pc STATIC src/Memory-PC.cpp)
add_library(SampleGame::MemoryPC ALIAS sample_memory_pc)
target_include_directories(sample_memory_pc PUBLIC include)
target_link_libraries(sample_memory_pc PUBLIC
    sample_game
    MegaDriveEnvironment::MegaDriveEnvironment
)
sample_enable_warnings(sample_memory_pc)

file(GLOB_RECURSE SAMPLE_RUNTIME_TEST_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/runtime_tests/*.cpp"
)

add_executable(mega_drive_environment_sample_game
    src/main-PC.cpp
    ${SAMPLE_RUNTIME_TEST_SOURCES}
)
add_executable(SampleGame::PC ALIAS mega_drive_environment_sample_game)
add_dependencies(mega_drive_environment_sample_game sample_asset_rom)
target_compile_definitions(mega_drive_environment_sample_game PRIVATE
    SAMPLE_ASSET_ROM_PATH="${SAMPLE_ASSET_ROM}"
    SAMPLE_GAME_VERSION="${PROJECT_VERSION}"
)
target_link_libraries(mega_drive_environment_sample_game PRIVATE sample_memory_pc)
if(TARGET png_shared)
    target_link_libraries(mega_drive_environment_sample_game PRIVATE png_shared)
elseif(TARGET png_static)
    target_link_libraries(mega_drive_environment_sample_game PRIVATE png_static)
else()
    message(FATAL_ERROR "MegaDriveEnvironment did not provide a libpng target for runtime diagnostics")
endif()

include(CTest)
if(BUILD_TESTING)
    include(cmake/Tests.cmake)
endif()
