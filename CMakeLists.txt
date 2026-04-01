cmake_minimum_required(VERSION 3.22)

if(DEFINED OVERRIDE_VERSION)
    project(AudeoboxLink VERSION ${OVERRIDE_VERSION})
else()
    project(AudeoboxLink VERSION 0.1.0)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")

# Point this at a local JUCE checkout. Default assumes JUCE/ in the project root.
if (NOT DEFINED JUCE_DIR)
    set(JUCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/JUCE")
endif()

if (NOT EXISTS "${JUCE_DIR}/CMakeLists.txt")
    message(FATAL_ERROR "JUCE was not found. Set JUCE_DIR to your JUCE folder.")
endif()

add_subdirectory("${JUCE_DIR}" JUCE)

set(PLUGIN_FORMATS VST3)
if(APPLE)
    list(APPEND PLUGIN_FORMATS AU)
endif()

juce_add_plugin(AudeoboxLink
    COMPANY_NAME "Audeobox"
    BUNDLE_ID com.audeobox.link
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    PLUGIN_MANUFACTURER_CODE ABox
    PLUGIN_CODE Alk1
    FORMATS ${PLUGIN_FORMATS}
    PRODUCT_NAME "Audeobox Link"
    HARDENED_RUNTIME_ENABLED TRUE
    HARDENED_RUNTIME_OPTIONS com.apple.security.cs.allow-unsigned-executable-memory com.apple.security.network.client com.apple.security.cs.allow-jit
)

target_sources(AudeoboxLink
    PRIVATE
        Source/PluginEditor.cpp
        Source/PluginEditor.h
        Source/PluginProcessor.cpp
        Source/PluginProcessor.h
)

juce_generate_juce_header(AudeoboxLink)

target_compile_definitions(AudeoboxLink
    PUBLIC
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_DISPLAY_SPLASH_SCREEN=0
)

target_link_libraries(AudeoboxLink
    PRIVATE
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)
