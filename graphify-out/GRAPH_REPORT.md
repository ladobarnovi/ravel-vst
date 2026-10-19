# Graph Report - vst  (2026-10-19)

## Corpus Check
- 33 files · ~87,583 words
- Verdict: corpus is large enough that graph structure adds value.

## Summary
- 889 nodes · 1629 edges · 50 communities (44 shown, 4 thin omitted)
- Extraction: 91% EXTRACTED · 9% INFERRED · 0% AMBIGUOUS · INFERRED: 141 edges (avg confidence: 0.85)
- Token cost: 0 input · 0 output

## Graph Freshness
- Built from commit: `50d545aa`
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
- Stack
- UndoHistory.cpp
- LaneSnapshot
- main
- LanePattern
- EngineTests.cpp
- int64_t
- Watched
- layoutContent
- StepLayer
- MockPlayHead
- processBlock
- Counts
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
- LaneComponent::slideThrough
- StepSlot::barArea

## God Nodes (most connected - your core abstractions)
1. `RavelAudioProcessor` - 69 edges
2. `SequencerEngine` - 67 edges
3. `RavelAudioProcessorEditor` - 59 edges
4. `PresetManager` - 43 edges
5. `Snapshot` - 36 edges
6. `RavelLookAndFeel` - 29 edges
7. `UndoHistory` - 29 edges
8. `ExternalMidiOutput` - 27 edges
9. `PresetBar` - 27 edges
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

## Communities (50 total, 4 thin omitted)

### Community 0 - "PresetManager"
Cohesion: 0.08
Nodes (49): map, AudioProcessor, AudioProcessorValueTreeState, File, String, vector, Entry, children (+41 more)

### Community 1 - "build-macos CI job"
Cohesion: 0.05
Nodes (56): Bundle artifact upload path convention, build-macos CI job, JUCE_TAG pin (9.0.1), Universal binary configure (arm64 + x86_64), EDITOR_WANTS_KEYBOARD_FOCUS, IS_SYNTH + NEEDS_MIDI_OUTPUT plugin classification, Per-format macOS CFBundleIdentifier patch, PLUGIN_CODE Trln (frozen 4-char code) (+48 more)

### Community 2 - "RavelAudioProcessorEditor"
Cohesion: 0.06
Nodes (35): ComponentBoundsConstrainer, ControlGroup, AudioProcessorEditor, ControlRow, JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR, RavelAudioProcessorEditor, appliedCanRedo, appliedCanUndo (+27 more)

### Community 3 - "Theme.h"
Cohesion: 0.08
Nodes (51): MouseCursor, Ptr, accentOf(), chipFont(), chipWidth(), clearDrawProportion(), Font, cursorForRole() (+43 more)

### Community 4 - "Parameters.cpp"
Cohesion: 0.17
Nodes (51): ParameterLayout, Random, ccPrefix(), clearLaneRow(), copyLane(), copyLaneParameters(), AudioProcessorValueTreeState, LaneKind (+43 more)

### Community 5 - "RavelLookAndFeel"
Cohesion: 0.12
Nodes (45): Button, LookAndFeel_V4, SliderStyle, centredStrip(), Colour, ComboBox, Component, Font (+37 more)

### Community 6 - "Controls.cpp"
Cohesion: 0.07
Nodes (28): RowStyle, ControlGroup::add(), ControlGroup::ControlGroup(), ControlGroup::paint(), ControlGroup::setHeadingAccent(), ControlRow::ControlRow(), ControlRow::setTooltip(), AudioProcessorValueTreeState (+20 more)

### Community 7 - "RavelAudioProcessor"
Cohesion: 0.05
Nodes (32): AudioProcessor, AudioProcessorValueTreeState, JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR, RavelAudioProcessor, apvts, ccLaneParams, engine, externalMidiOutput (+24 more)

### Community 8 - "SequencerEngine"
Cohesion: 0.06
Nodes (33): atomic, atomic, SequencerEngine, ccCountdown, ccIntervalSamples, ccLaneHeldValue, ccLaneLastCcValue, ccLaneSlewedValue (+25 more)

### Community 9 - "ExternalMidiOutput"
Cohesion: 0.08
Nodes (30): AbstractFifo, CriticalSection, MidiOutput, String, ExternalMidiOutput, currentIdentifier, device, deviceLock (+22 more)

### Community 10 - "UndoHistory"
Cohesion: 0.11
Nodes (13): vector, AudioProcessor, AudioProcessorParameter::Listener, deque, JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR, JUCE_DECLARE_WEAK_REFERENCEABLE, UndoHistory, editHeld (+5 more)

### Community 11 - "LaneComponent.cpp"
Cohesion: 0.08
Nodes (4): easeOut(), LaneComponent::applyValueSlide(), StepSlot::drawnProportion(), StepSlot::setValueSlideProgress()

### Community 12 - "ParameterTables.h"
Cohesion: 0.09
Nodes (18): array, maxScaleSize, DivisionDef, name, ppq, divisionNameList(), edoChromatic(), StringArray (+10 more)

### Community 13 - "SequencerEngine.cpp"
Cohesion: 0.29
Nodes (18): addRpn(), MidiBuffer, advanceVoices, allocateMpeChannel, allocateVoice, gateFor, process, publishUiSteps (+10 more)

### Community 14 - "Snapshot"
Cohesion: 0.10
Nodes (21): Snapshot, bendRange, ccChannel, ccLanes, ccNumber, ccOffset, ccOn, midiChannel (+13 more)

### Community 15 - "PresetBar"
Cohesion: 0.09
Nodes (30): AlertWindow, PopupMenu, function, Graphics, String, vector, Component, File (+22 more)

### Community 16 - "LaneParams"
Cohesion: 0.12
Nodes (17): atomic, LaneParams, active, ccChannel, ccNumber, ccOffset, ccOn, chance (+9 more)

### Community 17 - "PluginProcessor.cpp"
Cohesion: 0.14
Nodes (12): BusesLayout, MemoryBlock, AudioProcessor, AudioProcessorEditor, JUCE_CALLTYPE createPluginFilter(), createEditor, getStateInformation, isBusesLayoutSupported (+4 more)

### Community 18 - "ExternalMidiSelector"
Cohesion: 0.07
Nodes (28): Component, Rectangle, String, Tab, bounds, name, page, TabStrip() (+20 more)

### Community 19 - "PluginEditor.cpp"
Cohesion: 0.27
Nodes (15): KeyPress, LaneKind, applyLaneCount, buildWorkspaces, keyPressed, parentHierarchyChanged, RavelAudioProcessorEditor::RavelAudioProcessorEditor(), removeLane (+7 more)

### Community 20 - "ContentComponent"
Cohesion: 0.15
Nodes (13): ContentComponent, dividerArea, headerArea, markArea, onResized, paint, wordmarkArea, Component (+5 more)

### Community 21 - "MouseEvent"
Cohesion: 0.15
Nodes (14): MouseWheelDetails, MouseEvent, LaneComponent::continueStroke(), LaneComponent::endStroke(), LaneComponent::handStrokeTo(), LaneComponent::startStroke(), StepSlot::continueBarDrag(), StepSlot::endBarDrag() (+6 more)

### Community 22 - "Snapshot.cpp"
Cohesion: 0.28
Nodes (12): ComponentType, collectDescendants(), AudioProcessorValueTreeState, Component, String, vector, dialInDemoPattern(), findDescendant() (+4 more)

### Community 23 - "Stack"
Cohesion: 0.15
Nodes (12): OwnedArray, AudioProcessorValueTreeState, TextButton, Stack, addButton, clipboard, count, countParam (+4 more)

### Community 24 - "UndoHistory.cpp"
Cohesion: 0.21
Nodes (12): AudioProcessor, deque, captureBeforeEdit, clear, parameterGestureChanged, redo, restore, setEditHeldOpen (+4 more)

### Community 25 - "LaneSnapshot"
Cohesion: 0.11
Nodes (18): CcLaneSnapshot, ccChannel, ccNumber, ccOffset, ccOn, LaneSnapshot, active, chance (+10 more)

### Community 26 - "main"
Cohesion: 0.29
Nodes (7): check(), String, defaultOf(), main(), runProcessor(), section(), setChoice()

### Community 27 - "LanePattern"
Cohesion: 0.18
Nodes (12): AudioProcessorValueTreeState, LaneKind, LaneComponent::LaneComponent(), StepSlot::StepSlot(), LanePattern, chance, enabled, gate (+4 more)

### Community 28 - "EngineTests.cpp"
Cohesion: 0.17
Nodes (18): EventType, prepare, baseSnapshot(), check(), vector, dumpSamples(), Event, channel (+10 more)

### Community 29 - "int64_t"
Cohesion: 0.38
Nodes (10): int64_t, drawWithinSpread(), hashToUnitFloat(), positiveMod(), nextBoundarySample, resolveGlobalIndex, stepIndexFor, timingOffsetFor (+2 more)

### Community 30 - "Watched"
Cohesion: 0.40
Nodes (4): atomic, Watched, last, param

### Community 31 - "layoutContent"
Cohesion: 0.67
Nodes (3): layoutContent, layoutWorkspace, preferredWidth

### Community 32 - "StepLayer"
Cohesion: 0.29
Nodes (8): StepLayer, String, LaneComponent::setLayer(), layerTooltip(), spreadRangeText(), StepSlot::beginBarDrag(), StepSlot::getTooltip(), StepSlot::setLayer()

### Community 33 - "MockPlayHead"
Cohesion: 0.40
Nodes (5): AudioPlayHead, Optional, PositionInfo, MockPlayHead, info

### Community 34 - "processBlock"
Cohesion: 0.40
Nodes (4): AudioBuffer, MidiBuffer, buildSnapshot, processBlock

### Community 35 - "Counts"
Cohesion: 0.29
Nodes (7): Counts, controllers, firstCcNumber, firstNote, firstVelocity, noteOffs, noteOns

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
Cohesion: 0.50
Nodes (4): PitchResult, bend, note, pitchFor

## Knowledge Gaps
- **233 isolated node(s):** `name`, `page`, `bounds`, `sendAtMs`, `data` (+228 more)
  These have ≤1 connection - possible missing edges or undocumented components. (Counts symbols only; 389 node(s) total have ≤1 connection when file, concept and rationale nodes are included.)
- **4 thin communities (<3 nodes) omitted from report** — run `graphify query` to explore isolated nodes.

## Suggested Questions
_Questions this graph is uniquely positioned to answer:_

- **Why does `RavelAudioProcessor` connect `RavelAudioProcessor` to `PresetManager`, `RavelAudioProcessorEditor`, `processBlock`, `String`, `SequencerEngine`, `ExternalMidiOutput`, `UndoHistory`, `PresetBar`, `LaneParams`, `PluginProcessor.cpp`, `ExternalMidiSelector`, `PluginEditor.cpp`, `main`?**
  _High betweenness centrality (0.408) - this node is a cross-community bridge._
- **Why does `RavelAudioProcessorEditor` connect `RavelAudioProcessorEditor` to `RavelLookAndFeel`, `RavelAudioProcessor`, `paint`, `PresetBar`, `ExternalMidiSelector`, `PluginEditor.cpp`, `ContentComponent`, `Stack`, `Watched`, `layoutContent`?**
  _High betweenness centrality (0.245) - this node is a cross-community bridge._
- **Why does `SequencerEngine` connect `SequencerEngine` to `LaneState`, `RavelAudioProcessor`, `Voice`, `MpeChannelSlot`, `PitchResult`, `SequencerEngine.cpp`, `Snapshot`, `LaneSnapshot`, `EngineTests.cpp`, `int64_t`?**
  _High betweenness centrality (0.200) - this node is a cross-community bridge._
- **What connects `name`, `page`, `bounds` to the rest of the system?**
  _233 weakly-connected nodes found - possible documentation gaps or missing edges._
- **Should `PresetManager` be split into smaller, more focused modules?**
  _Cohesion score 0.07622504537205081 - nodes in this community are weakly interconnected._
- **Should `build-macos CI job` be split into smaller, more focused modules?**
  _Cohesion score 0.05194805194805195 - nodes in this community are weakly interconnected._
- **Should `RavelAudioProcessorEditor` be split into smaller, more focused modules?**
  _Cohesion score 0.05714285714285714 - nodes in this community are weakly interconnected._