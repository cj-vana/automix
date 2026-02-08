# Compiler warning flags for AutoMix

function(set_automix_warnings target)
    target_compile_options(${target} PRIVATE
        $<$<CXX_COMPILER_ID:AppleClang,Clang>:
            -Wall
            -Wextra
            -Wpedantic
            -Wno-unused-parameter
            -Wno-sign-compare
        >
        $<$<CXX_COMPILER_ID:GNU>:
            -Wall
            -Wextra
            -Wpedantic
            -Wno-unused-parameter
        >
    )
endfunction()
