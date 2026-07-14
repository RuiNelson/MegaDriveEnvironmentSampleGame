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

add_library(sample_game STATIC
    src/ControllerReader.cpp
    src/GameSession.cpp
    src/PsgSoundEffects.cpp
    src/SampleGame.cpp
    src/VdpUtils.cpp
)
target_include_directories(sample_game PUBLIC include)

add_library(sample_memory_pc STATIC
    src/Memory-PC.cpp
)
target_include_directories(sample_memory_pc PUBLIC include)
target_link_libraries(sample_memory_pc PUBLIC
    sample_game
    MegaDriveEnvironment::MegaDriveEnvironment
)

set(SAMPLE_ASSET_ROM "${CMAKE_CURRENT_BINARY_DIR}/sample_game_assets.bin")
set(SAMPLE_FONT_DATA
    "${MEGADRIVE_ENVIRONMENT_DIR}/include/MegaDriveEnvironment/util/font/FontData.hpp"
)
option(SAMPLE_ASSET_ROM_PREBUILT "Asset ROM was generated before CMake build" OFF)
if(SAMPLE_ASSET_ROM_PREBUILT)
    if(NOT EXISTS "${SAMPLE_ASSET_ROM}")
        message(FATAL_ERROR "Prebuilt asset ROM does not exist: ${SAMPLE_ASSET_ROM}")
    endif()
    add_custom_target(sample_asset_rom ALL)
else()
    add_custom_command(
        OUTPUT "${SAMPLE_ASSET_ROM}"
        COMMAND "${Python3_EXECUTABLE}"
                "${CMAKE_CURRENT_SOURCE_DIR}/tools/build_asset_rom.py"
                --output "${SAMPLE_ASSET_ROM}"
                --font-data "${SAMPLE_FONT_DATA}"
        DEPENDS
            "${CMAKE_CURRENT_SOURCE_DIR}/tools/build_asset_rom.py"
            "${SAMPLE_FONT_DATA}"
        COMMENT "Building raw 32-Mbit asset ROM"
        VERBATIM
    )
    add_custom_target(sample_asset_rom ALL DEPENDS "${SAMPLE_ASSET_ROM}")
endif()

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
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_build_asset_rom.py"
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

    add_executable(sample_psg_sound_effects_test tests/PsgSoundEffectsTest.cpp)
    target_link_libraries(sample_psg_sound_effects_test PRIVATE sample_game)
    add_test(NAME sample_psg_sound_effects_test COMMAND sample_psg_sound_effects_test)
endif()
