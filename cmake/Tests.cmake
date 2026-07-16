add_test(
    NAME sample_asset_rom_encoding_test
    COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_build_assets.py"
            "${SAMPLE_FONT_DATA}"
)

add_test(
    NAME sample_source_manifest_test
    COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_source_manifest.py"
)

function(sample_add_cpp_test target source library)
    add_executable(${target} "${source}")
    target_link_libraries(${target} PRIVATE ${library})
    sample_enable_warnings(${target})
    add_test(NAME ${target} COMMAND ${target})
endfunction()

sample_add_cpp_test(sample_memory_test tests/MemoryTest.cpp sample_memory_pc)
sample_add_cpp_test(sample_controller_reader_test tests/ControllerReaderTest.cpp sample_game)
sample_add_cpp_test(sample_game_session_test tests/GameSessionTest.cpp sample_game)
sample_add_cpp_test(sample_boing_ball_demo_test tests/BoingBallDemoTest.cpp sample_game)
sample_add_cpp_test(sample_psg_sound_effects_test tests/PsgSoundEffectsTest.cpp sample_game)
sample_add_cpp_test(sample_boing_ball_fm_sfx_test tests/BoingBallFmSfxTest.cpp sample_game)
