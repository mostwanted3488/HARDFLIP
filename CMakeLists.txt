cmake_minimum_required(VERSION 3.22)

# Universal macOS binary (Apple Silicon + Intel). Must be set before project().
if (APPLE)
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "Build architectures for macOS" FORCE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "10.15" CACHE STRING "Minimum macOS version" FORCE)
endif()

project(HARDFLIP VERSION 0.1.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.8
    GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(JUCE)

# --- DSP unit tests (no JUCE needed): cmake --build . --target hardflip_dsp_tests
add_executable(hardflip_dsp_tests EXCLUDE_FROM_ALL Tests/dsp_test.cpp)

# --- font (SIL OFL): fetched once at configure time; if offline the UI falls back to system fonts
set(HF_FONT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/UI/fonts)
set(HF_FONT ${HF_FONT_DIR}/PermanentMarker-Regular.ttf)
if (NOT EXISTS ${HF_FONT})
    file(MAKE_DIRECTORY ${HF_FONT_DIR})
    file(DOWNLOAD https://raw.githubusercontent.com/google/fonts/main/ofl/permanentmarker/PermanentMarker-Regular.ttf
         ${HF_FONT} STATUS font_status TLS_VERIFY ON)
    list(GET font_status 0 font_code)
    if (NOT font_code EQUAL 0)
        message(WARNING "Could not download PermanentMarker-Regular.ttf; the UI will use fallback fonts")
        file(WRITE ${HF_FONT} "")
    endif()
endif()

# --- web UI + font + JUCE's JS bridge, embedded in the plugin binary
juce_add_binary_data(HardflipAssets SOURCES
    UI/index.html
    UI/fonts/PermanentMarker-Regular.ttf
    ${juce_SOURCE_DIR}/modules/juce_gui_extra/native/javascript/index.js
    ${juce_SOURCE_DIR}/modules/juce_gui_extra/native/javascript/check_native_interop.js)

set(HF_FORMATS VST3)
if (APPLE)
    list(APPEND HF_FORMATS AU)
endif()

set(HF_EXTRA_PLUGIN_ARGS "")
if (WIN32)
    list(APPEND HF_EXTRA_PLUGIN_ARGS NEEDS_WEBVIEW2 TRUE)
endif()

juce_add_plugin(HARDFLIP
    COMPANY_NAME "BUYORBEALOSER"
    BUNDLE_ID com.buyorbealoser.hardflip
    PLUGIN_MANUFACTURER_CODE Bobl
    PLUGIN_CODE Hflp
    FORMATS ${HF_FORMATS}
    PRODUCT_NAME "HARDFLIP"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    VST3_CATEGORIES Fx Distortion
    AU_MAIN_TYPE kAudioUnitType_Effect
    COPY_PLUGIN_AFTER_BUILD FALSE
    NEEDS_WEB_BROWSER TRUE
    ${HF_EXTRA_PLUGIN_ARGS})

target_sources(HARDFLIP PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginProcessor.h
    Source/PluginEditor.cpp
    Source/PluginEditor.h
    Source/DSP/ClipDsp.h)

target_compile_definitions(HARDFLIP PUBLIC
    JUCE_WEB_BROWSER=1
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)
if (WIN32)
    target_compile_definitions(HARDFLIP PUBLIC JUCE_USE_WIN_WEBVIEW2_WITH_STATIC_LINKING=1)
endif()

target_link_libraries(HARDFLIP
    PRIVATE
        HardflipAssets
        juce::juce_audio_utils
        juce::juce_dsp
        juce::juce_gui_extra
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags)
