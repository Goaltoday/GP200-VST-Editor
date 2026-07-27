cmake_minimum_required(VERSION 3.22)

project(GP200VST VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_subdirectory(external/JUCE)

juce_add_plugin(GP200Plugin
    COMPANY_NAME "GP200Studio"

    PLUGIN_MANUFACTURER_CODE Gp2S
    PLUGIN_CODE Gp20

    FORMATS VST3

    PRODUCT_NAME "GP200 VST"

    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT TRUE
    IS_MIDI_EFFECT FALSE

    NEEDS_WEB_BROWSER FALSE
    NEEDS_CURL FALSE

    COPY_PLUGIN_AFTER_BUILD FALSE
)

juce_generate_juce_header(GP200Plugin)

juce_add_binary_data(GP200RibbonAssets
    SOURCES
        assets/ribbon/vol.svg
        assets/ribbon/pre.svg
        assets/ribbon/wah.svg
        assets/ribbon/dst.svg
        assets/ribbon/amp.svg
        assets/ribbon/nr.svg
        assets/ribbon/cab.svg
        assets/ribbon/eq.svg
        assets/ribbon/mod.svg
        assets/ribbon/dly.svg
        assets/ribbon/rvb.svg
        assets/fonts/SpaceGrotesk-Regular.ttf
        assets/fonts/SpaceGrotesk-Medium.ttf
        assets/fonts/SpaceGrotesk-SemiBold.ttf
        assets/fonts/OFL.txt
)


target_sources(GP200Plugin
    PRIVATE
        source/GP200Plugin/PluginProcessor.cpp
        source/GP200Plugin/PluginProcessor.h
        source/GP200Plugin/PluginEditor.cpp
        source/GP200Plugin/PluginEditor.h
        source/GP200Plugin/EffectBlockComponent.cpp
        source/GP200Plugin/EffectBlockComponent.h
        source/GP200Plugin/TunerDisplayComponent.cpp
        source/GP200Plugin/TunerDisplayComponent.h
	source/GP200Plugin/TunerEngine.cpp
	source/GP200Plugin/TunerEngine.h

        source/libgp200/GP200Constants.h
        source/libgp200/MidiDeviceScanner.cpp
        source/libgp200/MidiDeviceScanner.h
        source/libgp200/MidiConnection.cpp
        source/libgp200/MidiConnection.h
        source/libgp200/GP200Preset.cpp
        source/libgp200/GP200Preset.h
        source/libgp200/GP200EffectDatabase.cpp
        source/libgp200/GP200EffectDatabase.h
        source/libgp200/GP200EffectParamDatabase.cpp
        source/libgp200/GP200EffectParamDatabase.h
        source/libgp200/GP200IR.cpp
        source/libgp200/GP200IR.h
        source/libgp200/GP200SoundClone.cpp
        source/libgp200/GP200SoundClone.h
	source/GP200Plugin/ToneMatch/ToneMatchTypes.h
        source/GP200Plugin/ToneMatch/IToneMatchSolver.h
        source/GP200Plugin/ToneMatch/ToneMatchCapture.cpp
        source/GP200Plugin/ToneMatch/ToneMatchCapture.h
	source/GP200Plugin/ToneMatch/ToneAnalysis.cpp
	source/GP200Plugin/ToneMatch/ToneAnalysis.h
	source/GP200Plugin/ToneMatch/ToneMatchPanel.cpp
	source/GP200Plugin/ToneMatch/ToneMatchPanel.h
source/GP200Plugin/ToneMatch/ToneMatchComparison.h
source/GP200Plugin/ToneMatch/ToneMatchComparison.cpp

source/GP200Plugin/ToneMatch/ToneMatchCurveComponent.h
source/GP200Plugin/ToneMatch/ToneMatchCurveComponent.cpp
source/GP200Plugin/ToneMatch/SolverV1.h
source/GP200Plugin/ToneMatch/SolverV1.cpp

)

target_include_directories(GP200Plugin
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/source
        ${CMAKE_CURRENT_SOURCE_DIR}/source/GP200Plugin
        ${CMAKE_CURRENT_SOURCE_DIR}/source/libgp200
)

target_compile_definitions(GP200Plugin
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(GP200Plugin
    PRIVATE
        GP200RibbonAssets
        juce::juce_audio_devices
        juce::juce_audio_processors
        juce::juce_audio_utils
        juce::juce_gui_extra
  	juce::juce_dsp

    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

if(MSVC)
    target_compile_options(GP200Plugin
        PRIVATE
            /utf-8
            /permissive-
            /Zc:__cplusplus
    )
endif()

source_group(
    TREE ${CMAKE_CURRENT_SOURCE_DIR}/source
    PREFIX "Source"
    FILES
        source/GP200Plugin/PluginProcessor.cpp
        source/GP200Plugin/PluginProcessor.h
        source/GP200Plugin/PluginEditor.cpp
        source/GP200Plugin/PluginEditor.h
        source/GP200Plugin/EffectBlockComponent.cpp
        source/GP200Plugin/EffectBlockComponent.h
        source/GP200Plugin/TunerDisplayComponent.cpp
        source/GP200Plugin/TunerDisplayComponent.h
	source/GP200Plugin/TunerEngine.cpp
	source/GP200Plugin/TunerEngine.h

        source/libgp200/GP200Constants.h
        source/libgp200/MidiDeviceScanner.cpp
        source/libgp200/MidiDeviceScanner.h
        source/libgp200/MidiConnection.cpp
        source/libgp200/MidiConnection.h
        source/libgp200/GP200Preset.cpp
        source/libgp200/GP200Preset.h
        source/libgp200/GP200EffectDatabase.cpp
        source/libgp200/GP200EffectDatabase.h
        source/libgp200/GP200EffectParamDatabase.cpp
        source/libgp200/GP200EffectParamDatabase.h
        source/libgp200/GP200IR.cpp
        source/libgp200/GP200IR.h
        source/libgp200/GP200SoundClone.cpp
        source/libgp200/GP200SoundClone.h
	source/GP200Plugin/ToneMatch/ToneMatchTypes.h
        source/GP200Plugin/ToneMatch/IToneMatchSolver.h
        source/GP200Plugin/ToneMatch/ToneMatchCapture.cpp
        source/GP200Plugin/ToneMatch/ToneMatchCapture.h
	source/GP200Plugin/ToneMatch/ToneAnalysis.cpp
	source/GP200Plugin/ToneMatch/ToneAnalysis.h
	source/GP200Plugin/ToneMatch/ToneMatchPanel.cpp
	source/GP200Plugin/ToneMatch/ToneMatchPanel.h
source/GP200Plugin/ToneMatch/ToneMatchComparison.h
source/GP200Plugin/ToneMatch/ToneMatchComparison.cpp

source/GP200Plugin/ToneMatch/ToneMatchCurveComponent.h
source/GP200Plugin/ToneMatch/ToneMatchCurveComponent.cpp
source/GP200Plugin/ToneMatch/SolverV1.h
source/GP200Plugin/ToneMatch/SolverV1.cpp
)