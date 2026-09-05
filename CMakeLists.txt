cmake_minimum_required(VERSION 3.22)

project(Ravel VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Where the built .vst3 gets dropped. JUCE's Windows default is
# "C:/Program Files/Common Files/VST3", which needs an elevated shell to write to,
# so we use a user-writable folder and point Live at it as a Custom VST3 Folder.
# USERPROFILE is Windows-only; macOS/Linux builds (e.g. the CI job that produces
# the Mac plugin) fall back to HOME so this cache default never resolves to a
# path outside the user's home.
if(WIN32)
    set(RAVEL_VST3_DIR_DEFAULT "$ENV{USERPROFILE}/Documents/VST3")
else()
    set(RAVEL_VST3_DIR_DEFAULT "$ENV{HOME}/Documents/VST3")
endif()

set(RAVEL_VST3_DIR "${RAVEL_VST3_DIR_DEFAULT}"
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
set_directory_properties(PROPERTIES JUCE_VST3_COPY_DIR "${RAVEL_VST3_DIR}")

# IS_SYNTH + NEEDS_MIDI_OUTPUT is the combination that matters here: Ableton Live
# does not host MIDI-effect plugins, but it *will* expose an instrument's MIDI
# output as a routing source on another track's "MIDI From" selector.
juce_add_plugin(Ravel
    COMPANY_NAME                "Ladob"
    PLUGIN_MANUFACTURER_CODE    Ldbo
    # Kept as "Trln" through the Ravel -> Ravel rename: this 4-char code feeds the
    # VST3 class ID, and changing it would make existing Live sessions see this as a
    # different plugin instead of relinking to it after a rescan.
    PLUGIN_CODE                 Trln
    # AU only ever builds on macOS (JUCE's CMake filters it out of the format
    # list on every other platform), so this stays one line instead of an
    # if(APPLE) branch: Windows configure just ignores it.
    # Standalone is for developing without a host: it wraps the same processor and
    # editor in an app with its own audio/MIDI device selection. There is no host
    # transport there, so the sequencer only advances with the Free Run parameter on.
    FORMATS                     VST3 AU Standalone
    PRODUCT_NAME                "Ravel"
    DESCRIPTION                 "Polyrhythmic step sequencer"
    IS_SYNTH                    TRUE
    NEEDS_MIDI_INPUT            TRUE
    NEEDS_MIDI_OUTPUT           TRUE
    IS_MIDI_EFFECT              FALSE
    # TRUE so the VST3 wrapper asks the host for keyboard focus. Without it the editor never
    # sees a keystroke and the Ctrl+Z undo shortcut cannot fire at all.
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD     TRUE
    VST3_CATEGORIES             "Instrument" "Tools"
)

# ---- Per-format bundle identifiers (macOS) -----------------------------------
# juce_add_plugin() derives a single CFBundleIdentifier ("com.<company>.<target>") on the
# shared-code target, and juceaide bakes that one string into every format's Info.plist.
# The .vst3, .component and .app therefore all ship claiming to be the same bundle, which
# Apple disallows: once a host has loaded two of them into one process,
# CFBundleGetBundleWithIdentifier() can only ever resolve to whichever registered first.
#
# JUCE exposes no per-format BUNDLE_ID option - the plists are generated at configure time
# from the shared target's properties - so patch them afterwards. juce_add_plugin() above
# has already written them, and CMake only copies them into the bundle at build time, so
# rewriting them here lands before the bundle is assembled and before it is signed.
#
# Only CFBundleIdentifier moves. The VST3 class ID is derived from PLUGIN_CODE /
# PLUGIN_MANUFACTURER_CODE and the AU is identified by its type/subtype/manufacturer, so
# existing sessions still relink to the plugin rather than seeing a new one. The
# JucePlugin_CFBundleIdentifier compile definition is set on the shared-code target and is
# deliberately left alone: juce_CoreMidi_mac.mm seeds the virtual MIDI port's unique ID
# from it, and that ID should stay stable across formats and across this change.
if(APPLE)
    foreach(ravel_format VST3 AU Standalone)
        if(NOT TARGET Ravel_${ravel_format})
            continue()
        endif()

        string(TOLOWER "${ravel_format}" ravel_format_suffix)
        set(ravel_bundle_id "com.Ladob.Ravel.${ravel_format_suffix}")

        get_target_property(ravel_plist Ravel_${ravel_format} MACOSX_BUNDLE_INFO_PLIST)

        if(ravel_plist AND EXISTS "${ravel_plist}")
            file(READ "${ravel_plist}" ravel_plist_contents)
            string(REGEX REPLACE
                "(<key>CFBundleIdentifier</key>[ \t\r\n]*<string>)[^<]*(</string>)"
                "\\1${ravel_bundle_id}\\2"
                ravel_plist_contents "${ravel_plist_contents}")
            file(WRITE "${ravel_plist}" "${ravel_plist_contents}")
        else()
            message(WARNING "No generated Info.plist for Ravel_${ravel_format}; "
                            "its CFBundleIdentifier still collides with the other formats.")
        endif()

        # The Xcode generator takes the code-signing identifier from PRODUCT_BUNDLE_IDENTIFIER
        # rather than reading the plist back, so without this the signature would still carry
        # the old shared ID while the plist carried the new one.
        set_target_properties(Ravel_${ravel_format} PROPERTIES
            XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER "${ravel_bundle_id}")
    endforeach()
endif()

target_sources(Ravel PRIVATE
    Source/Parameters.cpp
    Source/SequencerEngine.cpp
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/LaneComponent.cpp
    Source/Controls.cpp
    Source/UndoHistory.cpp
    Source/ExternalMidiOutput.cpp
)

target_compile_definitions(Ravel PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(Ravel
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
option(RAVEL_BUILD_TESTS "Build the sequencer engine tests" ON)

if(RAVEL_BUILD_TESTS)
    juce_add_console_app(RavelTests PRODUCT_NAME "RavelTests")

    target_sources(RavelTests PRIVATE
        Tests/EngineTests.cpp
        Source/SequencerEngine.cpp
    )

    target_include_directories(RavelTests PRIVATE Source)

    target_compile_definitions(RavelTests PRIVATE
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
    )

    target_link_libraries(RavelTests
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
    juce_add_console_app(RavelProcessorTests PRODUCT_NAME "RavelProcessorTests")

    target_sources(RavelProcessorTests PRIVATE
        Tests/ProcessorTests.cpp
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp
        Source/LaneComponent.cpp
        Source/Controls.cpp
        Source/Parameters.cpp
        Source/SequencerEngine.cpp
        Source/UndoHistory.cpp
        Source/ExternalMidiOutput.cpp
    )

    target_include_directories(RavelProcessorTests PRIVATE Source)

    target_compile_definitions(RavelProcessorTests PRIVATE
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JucePlugin_Name="Ravel"
    )

    target_link_libraries(RavelProcessorTests
        PRIVATE
            juce::juce_audio_utils
            juce::juce_audio_processors
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_warning_flags
    )

    enable_testing()
    add_test(NAME engine    COMMAND RavelTests)
    add_test(NAME processor COMMAND RavelProcessorTests)
endif()

if(RAVEL_SNAPSHOT_SOURCE)
    juce_add_console_app(RavelSnapshot PRODUCT_NAME "RavelSnapshot")
    target_sources(RavelSnapshot PRIVATE ${RAVEL_SNAPSHOT_SOURCE}
        Source/PluginProcessor.cpp Source/PluginEditor.cpp Source/LaneComponent.cpp
        Source/Controls.cpp Source/Parameters.cpp Source/SequencerEngine.cpp
        Source/UndoHistory.cpp Source/ExternalMidiOutput.cpp)
    target_include_directories(RavelSnapshot PRIVATE Source)
    target_compile_definitions(RavelSnapshot PRIVATE
        JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JucePlugin_Name="Ravel"
        JUCE_MODAL_LOOPS_PERMITTED=1)
    target_link_libraries(RavelSnapshot
        PRIVATE juce::juce_audio_utils juce::juce_audio_processors juce::juce_gui_extra
        PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
endif()
