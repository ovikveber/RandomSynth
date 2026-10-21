cmake_minimum_required(VERSION 3.22)

project(RandomSynth VERSION 1.0.0 LANGUAGES C CXX)

if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

set(JUCE_DIR "D:/avst made/lib/JUCE-9.0.1" CACHE PATH "Path to a JUCE checkout")
add_subdirectory(${JUCE_DIR} JUCE)

juce_add_plugin(RandomSynth
    COMPANY_NAME "OV"
    PLUGIN_MANUFACTURER_CODE Ovsn
    PLUGIN_CODE Rsyn
    PRODUCT_NAME "RandomSynth"
    VERSION 1.0.0
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD FALSE
    FORMATS Standalone)

target_sources(RandomSynth PRIVATE
    src/World.cpp
    src/PluginProcessor.cpp
    src/PluginEditor.cpp)

target_compile_features(RandomSynth PRIVATE cxx_std_17)

target_compile_definitions(RandomSynth PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(RandomSynth PRIVATE
    juce_audio_utils
    juce_dsp)

if(MSVC)
    target_compile_options(RandomSynth PRIVATE /utf-8)
endif()

set_target_properties(RandomSynth PROPERTIES JUCE_COMPANY_NAME "OV")
