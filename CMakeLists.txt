cmake_minimum_required(VERSION 3.22)

set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "Minimum macOS version")
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

project(DubSiren VERSION 0.1.0 LANGUAGES C CXX)

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

option(SIREN_COPY_PLUGIN "Copy the plug-ins into the system plug-in folders after building" ON)

set(SIREN_FORMATS VST3 Standalone)
if (APPLE)
    list(APPEND SIREN_FORMATS AU)
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
add_test(NAME DspTest COMMAND SirenDspTest)
