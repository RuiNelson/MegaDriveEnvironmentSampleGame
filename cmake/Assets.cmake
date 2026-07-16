# Asset generation is deliberately owned by CMake so every build entry point
# produces the same ROM and generated header. The checked-in PCM is an input;
# rebuilding must never modify the source tree.
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
        "${CMAKE_CURRENT_SOURCE_DIR}/tools/tiles.py"
        "${CMAKE_CURRENT_SOURCE_DIR}/sound/tools/assemble_z80.py"
        "${SAMPLE_FONT_DATA}"
        "${SAMPLE_Z80_SOURCE}"
        "${SAMPLE_BOING_PCM}"
    COMMENT "Building sample-game asset ROM"
    VERBATIM
)

add_custom_target(sample_asset_rom ALL DEPENDS
    "${SAMPLE_ASSET_ROM}"
    "${SAMPLE_ASSET_LAYOUT_HEADER}"
)
