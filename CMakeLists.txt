cmake_minimum_required(VERSION 3.22)

# macOS: one universal binary that runs from 10.13 on (set before project()).
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "Minimum macOS version")
# Windows: link the C++ runtime statically, so the plugin needs no Visual C++ redistributable.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# The one place the version lives: plugin metadata, bundle versions and the "vX.Y.Z" in the header.
project(DubSiren VERSION 1.0.1 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)

# --- JUCE ---
# Point SIREN_JUCE_DIR (or the JUCE_DIR environment variable) at a local JUCE checkout;
# without one, the pinned JUCE release is downloaded at configure time.
set(SIREN_JUCE_VERSION "8.0.14")
set(SIREN_JUCE_DIR "$ENV{JUCE_DIR}" CACHE PATH "Local JUCE checkout (empty = download JUCE ${SIREN_JUCE_VERSION})")
if (SIREN_JUCE_DIR AND EXISTS "${SIREN_JUCE_DIR}/CMakeLists.txt")
    add_subdirectory("${SIREN_JUCE_DIR}" JUCE_build)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
            GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
            GIT_TAG        ${SIREN_JUCE_VERSION}
            GIT_SHALLOW    TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

# Copying into the system plug-in folders is handy while developing; CI turns it off
# (on Windows it would need admin rights).
option(SIREN_COPY_PLUGIN "Copy the plug-ins into the system plug-in folders after building" ON)

set(SIREN_FORMATS VST3 Standalone)
if (APPLE)
    list(APPEND SIREN_FORMATS AU)
elseif (UNIX)
    list(APPEND SIREN_FORMATS LV2)
endif()

juce_add_plugin(DubSiren
        PRODUCT_NAME "dub_siren"
        COMPANY_NAME "Tilo Hauk"
        BUNDLE_ID com.tilohauk.dubsiren
        IS_SYNTH TRUE
        NEEDS_MIDI_INPUT TRUE
        NEEDS_MIDI_OUTPUT FALSE
        IS_MIDI_EFFECT FALSE
        COPY_PLUGIN_AFTER_BUILD ${SIREN_COPY_PLUGIN}
        VST3_CATEGORIES Instrument Synth
        AU_MAIN_TYPE kAudioUnitType_MusicDevice
        LV2URI https://github.com/tilohk/dub_siren

        PLUGIN_MANUFACTURER_CODE Tlhk
        PLUGIN_CODE Dbsr

        FORMATS ${SIREN_FORMATS}
)

target_sources(DubSiren PRIVATE
        src/PluginProcessor.cpp
        src/PluginProcessor.h
        src/PluginEditor.cpp
        src/PluginEditor.h
        src/dsp/Oscillator.h
        src/dsp/Lfo.h
        src/dsp/Envelope.h
        src/dsp/SirenVoice.h
        src/dsp/TapeDelay.h
        src/dsp/Drive.h
        src/params/Presets.h
        src/params/Parameters.h
        src/ui/SirenLookAndFeel.h
)

target_include_directories(DubSiren PRIVATE src)

target_compile_definitions(DubSiren PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
)

target_compile_features(DubSiren PRIVATE cxx_std_20)

target_link_libraries(DubSiren
        PRIVATE
            juce::juce_audio_utils
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_lto_flags
            juce::juce_recommended_warning_flags
)

# --- Tests: offline DSP checks (plain C++, no JUCE) ---
enable_testing()
add_executable(SirenDspTest tests/DspTest.cpp)
target_include_directories(SirenDspTest PRIVATE src)
target_compile_features(SirenDspTest PRIVATE cxx_std_20)
target_compile_definitions(SirenDspTest PRIVATE $<$<CXX_COMPILER_ID:MSVC>:_USE_MATH_DEFINES>)
add_test(NAME DspTest COMMAND SirenDspTest)

# --- Dev tool: render the editor to PNG (no window needed) ---
option(SIREN_BUILD_UI_SNAPSHOT "Build the SirenUiSnapshot tool" OFF)
if (SIREN_BUILD_UI_SNAPSHOT)
    juce_add_console_app(SirenUiSnapshot PRODUCT_NAME "SirenUiSnapshot")
    target_sources(SirenUiSnapshot PRIVATE tools/UiSnapshot.cpp src/PluginProcessor.cpp src/PluginEditor.cpp)
    target_include_directories(SirenUiSnapshot PRIVATE src)
    target_compile_definitions(SirenUiSnapshot PRIVATE
            JucePlugin_Name="dub_siren"
            JucePlugin_VersionString="${PROJECT_VERSION}"
            JUCE_WEB_BROWSER=0
            JUCE_USE_CURL=0)
    target_compile_features(SirenUiSnapshot PRIVATE cxx_std_20)
    target_link_libraries(SirenUiSnapshot PRIVATE juce::juce_audio_utils juce::juce_recommended_config_flags)
endif()
