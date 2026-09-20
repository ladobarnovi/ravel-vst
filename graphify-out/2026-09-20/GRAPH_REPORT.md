# Graph Report - vst  (2026-09-19)

## Corpus Check
- 33 files · ~86,627 words
- Verdict: corpus is large enough that graph structure adds value.

## Summary
- 895 nodes · 1637 edges · 50 communities (41 shown, 7 thin omitted)
- Extraction: 91% EXTRACTED · 9% INFERRED · 0% AMBIGUOUS · INFERRED: 151 edges (avg confidence: 0.85)
- Token cost: 0 input · 0 output

## Graph Freshness
- Built from commit: `0d7af5d5`
- Run `git rev-parse HEAD` and compare to check if the graph is stale.
- Run `graphify update .` after code changes (no API cost).

## Community Hubs (Navigation)
- PresetManager
- build-macos CI job
- RavelAudioProcessorEditor
- Theme.h
- Parameters.cpp
- RavelLookAndFeel
- Controls.cpp
- RavelAudioProcessor
- SequencerEngine
- ExternalMidiOutput
- UndoHistory
- LaneComponent.cpp
- ParameterTables.h
- SequencerEngine.cpp
- Snapshot
- PresetBar
- LaneParams
- PluginProcessor.cpp
- ExternalMidiSelector
- PluginEditor.cpp
- ContentComponent
- MouseEvent
- Snapshot.cpp
- PluginProcessor.h
- LaneSnapshot
- main
- EngineTests.cpp
- int64_t
- Event
- layoutWorkspace
- StepLayer
- MockPlayHead
- Tab
- Counts
- CcLaneSnapshot
- LaneState
- Graphics
- String
- Voice
- Point
- Slider
- paint
- MpeChannelSlot
- PitchResult
- RavelFonts binary-data target
- SequencerEngine.h
- LaneComponent::slideThrough
- StepSlot::barArea

## God Nodes (most connected - your core abstractions)
1. `RavelAudioProcessorEditor` - 81 edges
2. `RavelAudioProcessor` - 69 edges
3. `SequencerEngine` - 68 edges
4. `PresetManager` - 43 edges
5. `Snapshot` - 35 edges
6. `RavelLookAndFeel` - 29 edges
7. `UndoHistory` - 28 edges
8. `PresetBar` - 27 edges
9. `ExternalMidiOutput` - 25 edges
10. `createParameterLayout()` - 23 edges

## Surprising Connections (you probably didn't know these)
- `Virtual port route for MPE (user-facing steps)` --semantically_similar_to--> `Live's channel collapse (why the virtual port is needed)`  [INFERRED] [semantically similar]
  Ravel-Setup.txt → README.md
- `Windows install (system or custom VST3 folder)` --semantically_similar_to--> `package.ps1 and the Inno Setup installer`  [INFERRED] [semantically similar]
  Ravel-Setup.txt → README.md
- `Virtual port route for MPE (user-facing steps)` --semantically_similar_to--> `Plugin-side MIDI port output (mirroring)`  [INFERRED] [semantically similar]
  Ravel-Setup.txt → README.md
- `macOS install (VST3 and AU bundles)` --semantically_similar_to--> `Building on macOS via CI`  [INFERRED] [semantically similar]
  Ravel-Setup.txt → README.md
- `RAVEL_VST3_DIR copy destination` --semantically_similar_to--> `package.ps1 and the Inno Setup installer`  [INFERRED] [semantically similar]
  CMakeLists.txt → README.md

## Import Cycles
- None detected.

## Hyperedges (group relationships)
- **Stateless timeline-derived state and randomness** — readme_timing_model, readme_hash_randomness, readme_spread, readme_probability, readme_swing, readme_boundary_epsilon_bug [EXTRACTED 1.00]
- **macOS cross-build, bundle identity and install flow** — _github_workflows_build_macos_build_macos, _github_workflows_build_macos_universal_binary, _github_workflows_build_macos_artifact_upload, cmakelists_per_format_bundle_ids, readme_macos_ci_build, ravel_setup_macos_install, ravel_setup_quarantine_flag [EXTRACTED 1.00]
- **Keeping MPE channels intact from plugin to instrument** — readme_mpe, readme_bend_range, readme_live_channel_collapse, readme_external_midi_output, ravel_setup_virtual_port_route, ravel_setup_port_caveats, readme_polyphony [EXTRACTED 1.00]

## Communities (50 total, 7 thin omitted)

### Community 0 - "PresetManager"
Cohesion: 0.08
Nodes (49): map, AudioProcessor, AudioProcessorValueTreeState, File, String, vector, Entry, children (+41 more)

### Community 1 - "build-macos CI job"
Cohesion: 0.05
Nodes (56): Bundle artifact upload path convention, build-macos CI job, JUCE_TAG pin (9.0.1), Universal binary configure (arm64 + x86_64), EDITOR_WANTS_KEYBOARD_FOCUS, IS_SYNTH + NEEDS_MIDI_OUTPUT plugin classification, Per-format macOS CFBundleIdentifier patch, PLUGIN_CODE Trln (frozen 4-char code) (+48 more)

### Community 2 - "RavelAudioProcessorEditor"
Cohesion: 0.04
Nodes (56): ComponentBoundsConstrainer, ControlGroup, atomic, AudioProcessorEditor, ControlRow, JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR, OwnedArray, TabPage (+48 more)

### Community 3 - "Theme.h"
Cohesion: 0.08
Nodes (51): MouseCursor, Ptr, accentOf(), chipFont(), chipWidth(), clearDrawProportion(), Font, cursorForRole() (+43 more)

### Community 4 - "Parameters.cpp"
Cohesion: 0.16
Nodes (51): ParameterLayout, Random, ccPrefix(), clearLaneRow(), copyLane(), copyLaneParameters(), AudioProcessorValueTreeState, LaneKind (+43 more)

### Community 5 - "RavelLookAndFeel"
Cohesion: 0.11
Nodes (47): Button, LookAndFeel_V4, SliderStyle, StepSlot::drawnProportion(), StepSlot::setValueSlideProgress(), centredStrip(), Colour, ComboBox (+39 more)

### Community 6 - "Controls.cpp"
Cohesion: 0.07
Nodes (28): RowStyle, ControlGroup::add(), ControlGroup::ControlGroup(), ControlGroup::paint(), ControlGroup::setHeadingAccent(), ControlRow::ControlRow(), ControlRow::setTooltip(), AudioProcessorValueTreeState (+20 more)

### Community 7 - "RavelAudioProcessor"
Cohesion: 0.05
Nodes (32): AudioProcessor, AudioProcessorValueTreeState, JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR, RavelAudioProcessor, apvts, ccLaneParams, engine, externalMidiOutput (+24 more)

### Community 8 - "SequencerEngine"
Cohesion: 0.06
Nodes (32): atomic, SequencerEngine, ccCountdown, ccIntervalSamples, ccLaneHeldValue, ccLaneLastCcValue, ccLaneSlewedValue, ccLaneStates (+24 more)

### Community 9 - "ExternalMidiOutput"
Cohesion: 0.08
Nodes (29): AbstractFifo, CriticalSection, MidiOutput, String, ExternalMidiOutput, currentIdentifier, device, deviceLock (+21 more)

### Community 10 - "UndoHistory"
Cohesion: 0.10
Nodes (23): deque, vector, AudioProcessor, AudioProcessor, AudioProcessorParameter::Listener, JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR, JUCE_DECLARE_WEAK_REFERENCEABLE, UndoHistory (+15 more)

### Community 12 - "ParameterTables.h"
Cohesion: 0.06
Nodes (33): array, maxScaleSize, AudioProcessorValueTreeState, LaneKind, LaneComponent::LaneComponent(), StepSlot::StepSlot(), directionNameList(), StringArray (+25 more)

### Community 13 - "SequencerEngine.cpp"
Cohesion: 0.23
Nodes (21): addRpn(), MidiBuffer, drawWithinSpread(), advanceVoices, allocateMpeChannel, allocateVoice, anyVoiceActive, gateFor (+13 more)

### Community 14 - "Snapshot"
Cohesion: 0.10
Nodes (21): Snapshot, bendRange, ccChannel, ccLanes, ccNumber, ccOffset, ccOn, midiChannel (+13 more)

### Community 15 - "PresetBar"
Cohesion: 0.09
Nodes (29): AlertWindow, PopupMenu, function, Graphics, String, vector, Component, File (+21 more)

### Community 16 - "LaneParams"
Cohesion: 0.12
Nodes (17): atomic, LaneParams, active, ccChannel, ccNumber, ccOffset, ccOn, chance (+9 more)

### Community 17 - "PluginProcessor.cpp"
Cohesion: 0.13
Nodes (14): AudioBuffer, BusesLayout, AudioProcessor, AudioProcessorEditor, MidiBuffer, JUCE_CALLTYPE createPluginFilter(), buildSnapshot, createEditor (+6 more)

### Community 18 - "ExternalMidiSelector"
Cohesion: 0.12
Nodes (18): Graphics, ExternalMidiSelector, box, deviceIds, ExternalMidiSelector::ExternalMidiSelector(), paint, preferredWidth, refresh (+10 more)

### Community 19 - "PluginEditor.cpp"
Cohesion: 0.27
Nodes (17): KeyPress, applyCcLaneCount, applyNoteLaneCount, buildWorkspaces, keyPressed, parentHierarchyChanged, RavelAudioProcessorEditor::RavelAudioProcessorEditor(), removeCcLane (+9 more)

### Community 20 - "ContentComponent"
Cohesion: 0.15
Nodes (13): ContentComponent, dividerArea, headerArea, markArea, onResized, paint, wordmarkArea, Component (+5 more)

### Community 21 - "MouseEvent"
Cohesion: 0.15
Nodes (14): MouseWheelDetails, MouseEvent, LaneComponent::continueStroke(), LaneComponent::endStroke(), LaneComponent::handStrokeTo(), LaneComponent::startStroke(), StepSlot::continueBarDrag(), StepSlot::endBarDrag() (+6 more)

### Community 22 - "Snapshot.cpp"
Cohesion: 0.28
Nodes (12): ComponentType, collectDescendants(), AudioProcessorValueTreeState, Component, String, vector, dialInDemoPattern(), findDescendant() (+4 more)

### Community 25 - "LaneSnapshot"
Cohesion: 0.15
Nodes (13): LaneSnapshot, active, chance, depth, direction, division, enabled, length (+5 more)

### Community 26 - "main"
Cohesion: 0.24
Nodes (8): MemoryBlock, getStateInformation, check(), String, defaultOf(), main(), section(), setChoice()

### Community 28 - "EngineTests.cpp"
Cohesion: 0.29
Nodes (10): prepare, baseSnapshot(), check(), vector, dumpSamples(), main(), run(), section() (+2 more)

### Community 29 - "int64_t"
Cohesion: 0.42
Nodes (9): int64_t, hashToUnitFloat(), positiveMod(), nextBoundarySample, resolveGlobalIndex, stepIndexFor, timingOffsetFor, splitmix64() (+1 more)

### Community 30 - "Event"
Cohesion: 0.29
Nodes (8): EventType, Event, channel, number, sample, type, value, only()

### Community 31 - "layoutWorkspace"
Cohesion: 0.40
Nodes (5): OwnedArray, TabPage, TextButton, layoutWorkspace(), WorkspaceType

### Community 32 - "StepLayer"
Cohesion: 0.29
Nodes (8): StepLayer, String, LaneComponent::setLayer(), layerTooltip(), spreadRangeText(), StepSlot::beginBarDrag(), StepSlot::getTooltip(), StepSlot::setLayer()

### Community 33 - "MockPlayHead"
Cohesion: 0.33
Nodes (6): AudioPlayHead, Optional, PositionInfo, MockPlayHead, info, runProcessor()

### Community 34 - "Tab"
Cohesion: 0.29
Nodes (7): Component, Rectangle, String, Tab, bounds, name, page

### Community 35 - "Counts"
Cohesion: 0.29
Nodes (7): Counts, controllers, firstCcNumber, firstNote, firstVelocity, noteOffs, noteOns

### Community 36 - "CcLaneSnapshot"
Cohesion: 0.40
Nodes (5): CcLaneSnapshot, ccChannel, ccNumber, ccOffset, ccOn

### Community 37 - "LaneState"
Cohesion: 0.40
Nodes (5): int64_t, LaneState, lastGlobalIndex, step, value

### Community 38 - "Graphics"
Cohesion: 0.50
Nodes (4): Graphics, LaneComponent::paint(), StepSlot::paint(), StepSlot::paintOverChildren()

### Community 40 - "Voice"
Cohesion: 0.50
Nodes (4): Voice, channel, note, samplesRemaining

### Community 41 - "Point"
Cohesion: 0.67
Nodes (3): Point, LaneComponent::pointCrossingSlot(), StepSlot::barContains()

### Community 42 - "Slider"
Cohesion: 0.67
Nodes (3): Slider, StepSlot::activeBar(), StepSlot::proportionOf()

### Community 43 - "paint"
Cohesion: 0.67
Nodes (3): Graphics, RavelAudioProcessorEditor::ContentComponent::paint(), paint

### Community 44 - "MpeChannelSlot"
Cohesion: 0.67
Nodes (3): MpeChannelSlot, rangeSent, voiceSlot

### Community 45 - "PitchResult"
Cohesion: 0.67
Nodes (3): PitchResult, bend, note

## Knowledge Gaps
- **240 isolated node(s):** `name`, `page`, `bounds`, `data`, `length` (+235 more)
  These have ≤1 connection - possible missing edges or undocumented components. (Counts symbols only; 399 node(s) total have ≤1 connection when file, concept and rationale nodes are included.)
- **7 thin communities (<3 nodes) omitted from report** — run `graphify query` to explore isolated nodes.

## Suggested Questions
_Questions this graph is uniquely positioned to answer:_

- **Why does `RavelAudioProcessor` connect `RavelAudioProcessor` to `PresetManager`, `MockPlayHead`, `RavelAudioProcessorEditor`, `String`, `SequencerEngine`, `ExternalMidiOutput`, `UndoHistory`, `PresetBar`, `LaneParams`, `PluginProcessor.cpp`, `ExternalMidiSelector`, `PluginEditor.cpp`, `PluginProcessor.h`, `main`?**
  _High betweenness centrality (0.411) - this node is a cross-community bridge._
- **Why does `RavelAudioProcessorEditor` connect `RavelAudioProcessorEditor` to `RavelLookAndFeel`, `RavelAudioProcessor`, `paint`, `ParameterTables.h`, `PresetBar`, `ExternalMidiSelector`, `PluginEditor.cpp`, `ContentComponent`, `PluginProcessor.h`?**
  _High betweenness centrality (0.292) - this node is a cross-community bridge._
- **Why does `SequencerEngine` connect `SequencerEngine` to `CcLaneSnapshot`, `LaneState`, `RavelAudioProcessor`, `Voice`, `MpeChannelSlot`, `PitchResult`, `SequencerEngine.cpp`, `Snapshot`, `SequencerEngine.h`, `LaneSnapshot`, `EngineTests.cpp`, `int64_t`?**
  _High betweenness centrality (0.199) - this node is a cross-community bridge._
- **What connects `name`, `page`, `bounds` to the rest of the system?**
  _240 weakly-connected nodes found - possible documentation gaps or missing edges._
- **Should `PresetManager` be split into smaller, more focused modules?**
  _Cohesion score 0.07622504537205081 - nodes in this community are weakly interconnected._
- **Should `build-macos CI job` be split into smaller, more focused modules?**
  _Cohesion score 0.05194805194805195 - nodes in this community are weakly interconnected._
- **Should `RavelAudioProcessorEditor` be split into smaller, more focused modules?**
  _Cohesion score 0.03571428571428571 - nodes in this community are weakly interconnected._