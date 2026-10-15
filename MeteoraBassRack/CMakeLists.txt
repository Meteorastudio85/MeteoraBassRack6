cmake_minimum_required(VERSION 3.22)
project(MeteoraBassRack VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# JUCE scaricato automaticamente (nessuna installazione richiesta)
include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.4
    GIT_SHALLOW    ON)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(MeteoraBassRack
    COMPANY_NAME              "Meteora Plugins"
    BUNDLE_ID                 com.meteoraplugins.bassrack
    PLUGIN_MANUFACTURER_CODE  Mtra
    PLUGIN_CODE               Mbrk
    FORMATS                   VST3 AU Standalone
    PRODUCT_NAME              "Meteora Bass Rack"
    IS_SYNTH                  FALSE
    NEEDS_MIDI_INPUT          FALSE
    NEEDS_MIDI_OUTPUT         FALSE
    IS_MIDI_EFFECT            FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD   FALSE
    VST3_CATEGORIES           Fx Dynamics EQ Distortion
    AU_MAIN_TYPE              kAudioUnitType_Effect)

# grafica e coordinate dentro il plugin
juce_add_binary_data(MeteoraBassRackAssets SOURCES
    Assets/api_skirt_0.png
    Assets/api_skirt_1.png
    Assets/api_skirt_2.png
    Assets/api_skirt_3.png
    Assets/background.jpg
    Assets/btn_on_0.png
    Assets/btn_on_1.png
    Assets/btn_on_2.png
    Assets/btn_on_3.png
    Assets/knob_in.png
    Assets/knob_in_light.png
    Assets/knob_pr.png
    Assets/knob_pr_light.png
    Assets/layout.json
    Assets/led_200_on.png
    Assets/led_40_on.png
    Assets/led_clip_on.png
    Assets/led_eq_on.png
    Assets/led_ts_on.png
    Assets/lever_down.png
    Assets/lever_up.png
    Assets/logo_glow.png
    Assets/pow_on.png
    Assets/vu_off.png)

target_sources(MeteoraBassRack PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/Dsp/AirwindowsApi550b.cpp
    Source/Dsp/AirwindowsLa2a.cpp
    Source/Dsp/AirwindowsTubeSmile.cpp
    Source/Dsp/AirwindowsVog.cpp)

target_compile_definitions(MeteoraBassRack PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(MeteoraBassRack
    PRIVATE
        MeteoraBassRackAssets
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
