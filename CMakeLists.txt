cmake_minimum_required(VERSION 3.22)

project(TriLane VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Where the built .vst3 gets dropped. JUCE's Windows default is
# "C:/Program Files/Common Files/VST3", which needs an elevated shell to write to,
# so we use a user-writable folder and point Live at it as a Custom VST3 Folder.
set(TRILANE_VST3_DIR "$ENV{USERPROFILE}/Documents/VST3"
    CACHE PATH "Folder the built VST3 is copied into after each build")

add_subdirectory(JUCE)

# Order matters twice over:
#  - after add_subdirectory(), because JUCE's _juce_set_default_properties()
#    unconditionally overwrites the GLOBAL JUCE_VST3_COPY_DIR while it is being
#    processed, so anything set earlier is discarded;
#  - before juce_add_plugin(), because that is when JUCE reads the property and
#    bakes the destination into the post-build copy step.
# JUCE_VST3_COPY_DIR is an INHERITED property, so a directory-scoped value is
# picked up by the plugin target created below.
set_directory_properties(PROPERTIES JUCE_VST3_COPY_DIR "${TRILANE_VST3_DIR}")

# IS_SYNTH + NEEDS_MIDI_OUTPUT is the combination that matters here: Ableton Live
# does not host MIDI-effect plugins, but it *will* expose an instrument's MIDI
# output as a routing source on another track's "MIDI From" selector.
juce_add_plugin(TriLane
    COMPANY_NAME                "Ladob"
    PLUGIN_MANUFACTURER_CODE    Ldbo
    PLUGIN_CODE                 Trln
    FORMATS                     VST3
    PRODUCT_NAME                "TriLane"
    DESCRIPTION                 "Three-lane polyrhythmic step sequencer"
    IS_SYNTH                    TRUE
    NEEDS_MIDI_INPUT            TRUE
    NEEDS_MIDI_OUTPUT           TRUE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD     TRUE
    VST3_CATEGORIES             "Instrument" "Tools"
)

target_sources(TriLane PRIVATE
    Source/Parameters.cpp
    Source/SequencerEngine.cpp
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/LaneComponent.cpp
    Source/Controls.cpp
)

target_compile_definitions(TriLane PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(TriLane
    PRIVATE
        juce::juce_audio_utils
        juce::juce_audio_processors
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

# ---- Tests -------------------------------------------------------------------
# The engine takes PPQ positions as arguments rather than reading a playhead itself,
# so the sequencer core can be driven from a console app with no plugin host and
# asserted against exact note numbers and sample offsets.
option(TRILANE_BUILD_TESTS "Build the sequencer engine tests" ON)

if(TRILANE_BUILD_TESTS)
    juce_add_console_app(TriLaneTests PRODUCT_NAME "TriLaneTests")

    target_sources(TriLaneTests PRIVATE
        Tests/EngineTests.cpp
        Source/SequencerEngine.cpp
    )

    target_include_directories(TriLaneTests PRIVATE Source)

    target_compile_definitions(TriLaneTests PRIVATE
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
    )

    target_link_libraries(TriLaneTests
        PRIVATE
            juce::juce_audio_processors
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_warning_flags
    )

    # Drives the real processor through a mock playhead, covering the layer above the
    # engine: playhead handling, free-run, the parameter snapshot and MIDI capabilities.
    # JucePlugin_Name is normally a compile definition on the plugin target, so it has
    # to be supplied by hand here.
    juce_add_console_app(TriLaneProcessorTests PRODUCT_NAME "TriLaneProcessorTests")

    target_sources(TriLaneProcessorTests PRIVATE
        Tests/ProcessorTests.cpp
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp
        Source/LaneComponent.cpp
        Source/Controls.cpp
        Source/Parameters.cpp
        Source/SequencerEngine.cpp
    )

    target_include_directories(TriLaneProcessorTests PRIVATE Source)

    target_compile_definitions(TriLaneProcessorTests PRIVATE
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JucePlugin_Name="TriLane"
    )

    target_link_libraries(TriLaneProcessorTests
        PRIVATE
            juce::juce_audio_utils
            juce::juce_audio_processors
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_warning_flags
    )

    enable_testing()
    add_test(NAME engine    COMMAND TriLaneTests)
    add_test(NAME processor COMMAND TriLaneProcessorTests)
endif()
