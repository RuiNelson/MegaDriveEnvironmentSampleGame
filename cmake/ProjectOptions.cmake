add_library(sample_project_options INTERFACE)
target_compile_features(sample_project_options INTERFACE cxx_std_23)

function(sample_enable_warnings target)
    if(SAMPLE_ENABLE_WARNINGS)
        target_compile_options(${target} PRIVATE
            "$<$<COMPILE_LANG_AND_ID:CXX,AppleClang,Clang,GNU>:-Wall;-Wextra;-Wpedantic>"
            "$<$<COMPILE_LANG_AND_ID:CXX,MSVC>:/W4;/permissive->"
        )
    endif()
endfunction()
