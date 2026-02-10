include(FetchContent)

FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG v3.7.1
)
FetchContent_MakeAvailable(Catch2)

add_executable(AutoMixFFITests
    ffi_lifecycle_test.cpp
    ffi_process_test.cpp
    ffi_parameter_test.cpp
    ffi_metering_test.cpp
    ffi_edge_cases_test.cpp
    ffi_nan_test.cpp
)

target_include_directories(AutoMixFFITests
    PRIVATE
        "${CMAKE_SOURCE_DIR}/rust/automix-dsp/include"
)

target_link_libraries(AutoMixFFITests
    PRIVATE
        automix_dsp
        Catch2::Catch2WithMain
)

# macOS: link frameworks required by Rust std
if(APPLE)
    target_link_libraries(AutoMixFFITests
        PRIVATE
            "-framework Security"
            "-framework CoreFoundation"
    )
endif()

include(CTest)
include(Catch)
catch_discover_tests(AutoMixFFITests)
