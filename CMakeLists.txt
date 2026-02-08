cmake_minimum_required(VERSION 3.25)

# Read version from file
file(READ "${CMAKE_CURRENT_LIST_DIR}/VERSION" CURRENT_VERSION)
string(STRIP "${CURRENT_VERSION}" CURRENT_VERSION)

project(AutoMix VERSION ${CURRENT_VERSION})

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---- Platform Configuration ----
if(APPLE)
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "Build universal binary")
    set(CMAKE_OSX_DEPLOYMENT_TARGET "12.0" CACHE STRING "Minimum macOS version")
endif()

# ---- Custom CMake modules ----
include(cmake/CompilerWarnings.cmake)

# ---- JUCE (git submodule) ----
add_subdirectory(JUCE)

# ---- Corrosion: Rust/Cargo integration ----
include(FetchContent)
FetchContent_Declare(
    Corrosion
    GIT_REPOSITORY https://github.com/corrosion-rs/corrosion.git
    GIT_TAG v0.5
)
FetchContent_MakeAvailable(Corrosion)

# Import Rust DSP crate as a static library
corrosion_import_crate(MANIFEST_PATH rust/automix-dsp/Cargo.toml)

# Generate C header via cbindgen (runs during Rust build via build.rs)
set(AUTOMIX_FFI_HEADER "${CMAKE_CURRENT_SOURCE_DIR}/rust/automix-dsp/include/automix_dsp.h")

# ---- Melatonin Inspector (debug GUI tool) ----
add_subdirectory(modules/melatonin_inspector)

# ---- Plugin target ----
juce_add_plugin(AutoMix
    COMPANY_NAME "AutoMix"
    BUNDLE_ID "com.automix.plugin"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD TRUE
    PLUGIN_MANUFACTURER_CODE Amix
    PLUGIN_CODE Amxp
    FORMATS AU Standalone
    PRODUCT_NAME "AutoMix"
    HARDENED_RUNTIME_ENABLED TRUE
    HARDENED_RUNTIME_OPTIONS "com.apple.security.device.audio-input"
    MICROPHONE_PERMISSION_ENABLED TRUE
    MICROPHONE_PERMISSION_TEXT "AutoMix needs microphone access for standalone operation."
)

juce_generate_juce_header(AutoMix)

target_sources(AutoMix
    PRIVATE
        source/PluginProcessor.cpp
        source/PluginProcessor.h
        source/PluginEditor.cpp
        source/PluginEditor.h
)

target_include_directories(AutoMix
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/source"
        "${CMAKE_CURRENT_SOURCE_DIR}/rust/automix-dsp/include"
)

target_link_libraries(AutoMix
    PRIVATE
        automix_dsp
        juce::juce_audio_utils
        juce::juce_audio_processors
        juce::juce_gui_extra
        juce::juce_dsp
        melatonin_inspector
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

target_compile_definitions(AutoMix
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_DISPLAY_SPLASH_SCREEN=0
        JUCE_REPORT_APP_USAGE=0
    PRIVATE
        AUTOMIX_VERSION="${CURRENT_VERSION}"
)

# Apply warnings
set_automix_warnings(AutoMix)

# ---- On macOS, link system frameworks needed by Rust std ----
if(APPLE)
    target_link_libraries(AutoMix PRIVATE "-framework Security" "-framework CoreFoundation")
endif()
