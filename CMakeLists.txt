cmake_minimum_required(VERSION 3.22)

project(KlangbildLimit VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# JUCE: liegt der Ordner "JUCE" neben dieser Datei, wird er benutzt,
# sonst wird JUCE automatisch von GitHub geladen.
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/JUCE/CMakeLists.txt")
    add_subdirectory(JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.4
        GIT_SHALLOW ON)
    FetchContent_MakeAvailable(JUCE)
endif()

set(KB_FORMATS VST3 Standalone)
if(APPLE)
    set(KB_FORMATS AU VST3 Standalone)
endif()

juce_add_plugin(KlangbildLimit
    COMPANY_NAME "Klangbild"
    BUNDLE_ID "de.klangbild.limit"
    PLUGIN_MANUFACTURER_CODE Klbd
    PLUGIN_CODE Kbli
    FORMATS ${KB_FORMATS}
    PRODUCT_NAME "Klangbild Limit"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    AU_MAIN_TYPE kAudioUnitType_Effect
    VST3_CATEGORIES Fx Dynamics
    COPY_PLUGIN_AFTER_BUILD FALSE)

juce_add_binary_data(LimitAssets SOURCES
    Resources/Yellowtail-Regular.ttf
    Resources/Oswald-Medium.ttf
    Resources/Oswald-Regular.ttf)

target_sources(KlangbildLimit PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/VintageUI.cpp)

target_include_directories(KlangbildLimit PRIVATE Source)

target_compile_definitions(KlangbildLimit PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(KlangbildLimit
    PRIVATE
        LimitAssets
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
