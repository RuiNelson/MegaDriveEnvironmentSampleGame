cmake_minimum_required(VERSION 3.24)
project(MegaDriveEnvironmentSampleGame LANGUAGES CXX)

find_package(Python3 REQUIRED COMPONENTS Interpreter)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

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

set(SAMPLE_GENERATED_DIR "${CMAKE_BINARY_DIR}/generated")
set(SAMPLE_ASSET_ROM "${CMAKE_BINARY_DIR}/sample_game_assets.bin")
set(SAMPLE_ASSET_LAYOUT_HEADER "${SAMPLE_GENERATED_DIR}/AssetLayout.hpp")
set(SAMPLE_ASSET_LAYOUT_JSON "${SAMPLE_GENERATED_DIR}/asset_layout.json")
set(SAMPLE_ASSET_PACK_BIN "${SAMPLE_GENERATED_DIR}/asset_pack.bin")
set(SAMPLE_ASSETS_WORK_DIR "${SAMPLE_GENERATED_DIR}/assets")
set(SAMPLE_FONT_DATA
    "${MEGADRIVE_ENVIRONMENT_DIR}/include/MegaDriveEnvironment/util/font/FontData.hpp"
)
set(SAMPLE_Z80_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/z80/boing_ball_sfx.s")
set(SAMPLE_BOING_PCM "${CMAKE_CURRENT_SOURCE_DIR}/assets/boing_pcm.bin")

option(SAMPLE_ASSET_ROM_PREBUILT "Asset ROM was generated before CMake build" OFF)
if(SAMPLE_ASSET_ROM_PREBUILT)
    if(NOT EXISTS "${SAMPLE_ASSET_ROM}")
        message(FATAL_ERROR "Prebuilt asset ROM does not exist: ${SAMPLE_ASSET_ROM}")
    endif()
    if(NOT EXISTS "${SAMPLE_ASSET_LAYOUT_HEADER}")
        message(FATAL_ERROR
            "Prebuilt asset layout header does not exist: ${SAMPLE_ASSET_LAYOUT_HEADER}"
        )
    endif()
    add_custom_target(sample_asset_rom ALL)
else()
    add_custom_command(
        OUTPUT
            "${SAMPLE_ASSET_ROM}"
            "${SAMPLE_ASSET_LAYOUT_HEADER}"
            "${SAMPLE_ASSET_LAYOUT_JSON}"
            "${SAMPLE_ASSET_PACK_BIN}"
        COMMAND "${Python3_EXECUTABLE}"
                "${CMAKE_CURRENT_SOURCE_DIR}/tools/build_assets.py"
                --output "${SAMPLE_ASSET_ROM}"
                --font-data "${SAMPLE_FONT_DATA}"
                --z80-source "${SAMPLE_Z80_SOURCE}"
                --boing-pcm "${SAMPLE_BOING_PCM}"
                --work-dir "${SAMPLE_ASSETS_WORK_DIR}"
                --layout-header "${SAMPLE_ASSET_LAYOUT_HEADER}"
                --layout-json "${SAMPLE_ASSET_LAYOUT_JSON}"
                --pack-binary "${SAMPLE_ASSET_PACK_BIN}"
        DEPENDS
            "${CMAKE_CURRENT_SOURCE_DIR}/tools/build_assets.py"
            "${CMAKE_CURRENT_SOURCE_DIR}/tools/asset_pack.py"
            "${CMAKE_CURRENT_SOURCE_DIR}/tools/assemble_z80.py"
            "${CMAKE_CURRENT_SOURCE_DIR}/tools/boing_pcm.py"
            "${CMAKE_CURRENT_SOURCE_DIR}/tools/tiles.py"
            "${SAMPLE_FONT_DATA}"
            "${SAMPLE_Z80_SOURCE}"
            "${SAMPLE_BOING_PCM}"
        COMMENT "Building asset ROM (PCM + Z80 driver + tiles)"
        VERBATIM
    )
    add_custom_target(sample_asset_rom ALL DEPENDS
        "${SAMPLE_ASSET_ROM}"
        "${SAMPLE_ASSET_LAYOUT_HEADER}"
    )
endif()

add_library(sample_game STATIC
    src/BoingBallDemo.cpp
    src/BoingBallFmSfx.cpp
    src/ControllerReader.cpp
    src/GameSession.cpp
    src/PsgSoundEffects.cpp
    src/SampleGame.cpp
    src/VdpUtils.cpp
)
target_include_directories(sample_game PUBLIC
    include
    "${SAMPLE_GENERATED_DIR}"
)
target_compile_definitions(sample_game PUBLIC PC=1)
add_dependencies(sample_game sample_asset_rom)

add_library(sample_memory_pc STATIC
    src/Memory-PC.cpp
)
target_include_directories(sample_memory_pc PUBLIC include)
target_link_libraries(sample_memory_pc PUBLIC
    sample_game
    MegaDriveEnvironment::MegaDriveEnvironment
)

add_executable(mega_drive_environment_sample_game
    src/main-PC.cpp
)

target_include_directories(mega_drive_environment_sample_game PRIVATE include)
add_dependencies(mega_drive_environment_sample_game sample_asset_rom)
target_compile_definitions(mega_drive_environment_sample_game PRIVATE
    SAMPLE_ASSET_ROM_PATH="${SAMPLE_ASSET_ROM}"
)
target_link_libraries(mega_drive_environment_sample_game PRIVATE
    sample_memory_pc
)

include(CTest)
if(BUILD_TESTING)
    add_test(
        NAME sample_asset_rom_encoding_test
        COMMAND "${Python3_EXECUTABLE}"
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_build_assets.py"
                "${SAMPLE_FONT_DATA}"
    )

    add_executable(sample_memory_test tests/MemoryTest.cpp)
    target_link_libraries(sample_memory_test PRIVATE sample_memory_pc)
    add_test(NAME sample_memory_test COMMAND sample_memory_test)

    add_executable(sample_controller_reader_test tests/ControllerReaderTest.cpp)
    target_link_libraries(sample_controller_reader_test PRIVATE sample_game)
    add_test(NAME sample_controller_reader_test COMMAND sample_controller_reader_test)

    add_executable(sample_game_session_test tests/GameSessionTest.cpp)
    target_link_libraries(sample_game_session_test PRIVATE sample_game)
    add_test(NAME sample_game_session_test COMMAND sample_game_session_test)

    add_executable(sample_boing_ball_demo_test tests/BoingBallDemoTest.cpp)
    target_link_libraries(sample_boing_ball_demo_test PRIVATE sample_game)
    add_test(NAME sample_boing_ball_demo_test COMMAND sample_boing_ball_demo_test)

    add_executable(sample_psg_sound_effects_test tests/PsgSoundEffectsTest.cpp)
    target_link_libraries(sample_psg_sound_effects_test PRIVATE sample_game)
    add_test(NAME sample_psg_sound_effects_test COMMAND sample_psg_sound_effects_test)

    add_executable(sample_boing_ball_fm_sfx_test tests/BoingBallFmSfxTest.cpp)
    target_link_libraries(sample_boing_ball_fm_sfx_test PRIVATE sample_game)
    add_test(NAME sample_boing_ball_fm_sfx_test COMMAND sample_boing_ball_fm_sfx_test)
endif()
