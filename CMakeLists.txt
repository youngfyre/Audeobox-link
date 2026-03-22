cmake_minimum_required(VERSION 3.22)

project(AudeoboxSounds VERSION 0.0.1)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Point this at a local JUCE checkout. The default assumes a sibling folder:
#   Aueobox/
#   JUCE/
if (NOT DEFINED JUCE_DIR)
    set(JUCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/JUCE")
endif()

if (NOT EXISTS "${JUCE_DIR}/CMakeLists.txt")
    message(FATAL_ERROR "JUCE was not found. Set JUCE_DIR to your JUCE folder.")
endif()

add_subdirectory("${JUCE_DIR}" JUCE)

juce_add_plugin(AudeoboxSounds
    COMPANY_NAME "Audeobox"
    BUNDLE_ID com.audeobox.sounds
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    PLUGIN_MANUFACTURER_CODE ABox
    PLUGIN_CODE Abs1
    FORMATS Standalone VST3
    PRODUCT_NAME "Audeobox Sounds"
)

target_sources(AudeoboxSounds
    PRIVATE
        Source/PluginEditor.cpp
        Source/PluginEditor.h
        Source/PluginProcessor.cpp
        Source/PluginProcessor.h
)

juce_generate_juce_header(AudeoboxSounds)

target_compile_definitions(AudeoboxSounds
    PUBLIC
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
)

target_link_libraries(AudeoboxSounds
    PRIVATE
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)
