# Graph Report - vst  (2026-09-11)

## Corpus Check
- 36 files · ~86,169 words
- Verdict: corpus is large enough that graph structure adds value.

## Summary
- 894 nodes · 1631 edges · 53 communities (45 shown, 6 thin omitted)
- Extraction: 91% EXTRACTED · 9% INFERRED · 0% AMBIGUOUS · INFERRED: 151 edges (avg confidence: 0.85)
- Token cost: 119,465 input · 0 output

## Community Hubs (Navigation)
- Preset File Management
- Docs, Build & Install
- Editor Shell & Tabs
- Theme Drawing Primitives
- Parameter Layout & Lane Copy
- Custom LookAndFeel
- Control Group Widgets
- Audio Processor Class
- Engine State & CC Runtime
- External MIDI Output
- Undo History
- Lane Component Behavior
- Scales, EDO & Divisions
- Voice & MPE Allocation
- Engine Snapshot Struct
- Preset Bar UI
- Lane Parameter Bindings
- Process Block & Buses
- MIDI Device Selector
- Lane Count & Layout Updates
- Editor Content Chrome
- Mouse & Stroke Input
- Editor Snapshot Tool
- Preset Menu Handling
- Header Name List Helpers
- Lane Snapshot Structs
- Processor Tests
- Lane Pattern Construction
- Engine Tests
- Timing & Hash Randomness
- Test Event Capture
- Workspace Layout
- Step Layer & Tooltips
- Mock Play Head
- Tab Model
- Test Counters
- CC Lane Snapshot
- Lane Runtime State
- Lane & Step Painting
- Program Name Stubs
- Voice Struct
- Slot Hit Testing
- Bar Value Mapping
- Content Paint
- MPE Channel Slot
- Pitch Result Struct
- Build Doc Cross-Refs
- Keyboard Shortcut Handling
- Engine Atomics
- Lane Slide Callback
- Step Bar Geometry

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
- `RAVEL_VST3_DIR copy destination` --semantically_similar_to--> `package.ps1 and the Inno Setup installer`  [INFERRED] [semantically similar]
  CMakeLists.txt → README.md
- `Virtual port route for MPE (user-facing steps)` --semantically_similar_to--> `Plugin-side MIDI port output (mirroring)`  [INFERRED] [semantically similar]
  Ravel-Setup.txt → README.md
- `Clearing the Gatekeeper quarantine flag` --semantically_similar_to--> `package.ps1 and the Inno Setup installer`  [INFERRED] [semantically similar]
  Ravel-Setup.txt → README.md

## Import Cycles
- None detected.

## Hyperedges (group relationships)
- **macOS cross-build, bundle identity and install flow** — _github_workflows_build_macos_build_macos, _github_workflows_build_macos_universal_binary, _github_workflows_build_macos_artifact_upload, cmakelists_per_format_bundle_ids, readme_macos_ci_build, ravel_setup_macos_install, ravel_setup_quarantine_flag [EXTRACTED 1.00]
- **Stateless timeline-derived state and randomness** — readme_timing_model, readme_hash_randomness, readme_spread, readme_probability, readme_swing, readme_boundary_epsilon_bug [EXTRACTED 1.00]
- **Keeping MPE channels intact from plugin to instrument** — readme_mpe, readme_bend_range, readme_live_channel_collapse, readme_external_midi_output, ravel_setup_virtual_port_route, ravel_setup_port_caveats, readme_polyphony [EXTRACTED 1.00]

## Communities (53 total, 6 thin omitted)

### Community 0 - "Preset File Management"
Cohesion: 0.08
Nodes (49): map, AudioProcessor, AudioProcessorValueTreeState, File, String, vector, Entry, children (+41 more)

### Community 1 - "Docs, Build & Install"
Cohesion: 0.05
Nodes (56): Bundle artifact upload path convention, build-macos CI job, JUCE_TAG pin (9.0.1), Universal binary configure (arm64 + x86_64), EDITOR_WANTS_KEYBOARD_FOCUS, IS_SYNTH + NEEDS_MIDI_OUTPUT plugin classification, Per-format macOS CFBundleIdentifier patch, PLUGIN_CODE Trln (frozen 4-char code) (+48 more)

### Community 2 - "Editor Shell & Tabs"
Cohesion: 0.04
Nodes (56): ComponentBoundsConstrainer, ControlGroup, atomic, AudioProcessorEditor, ControlRow, JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR, OwnedArray, TabPage (+48 more)

### Community 3 - "Theme Drawing Primitives"
Cohesion: 0.08
Nodes (51): MouseCursor, Ptr, accentOf(), chipFont(), chipWidth(), clearDrawProportion(), Font, cursorForRole() (+43 more)

### Community 4 - "Parameter Layout & Lane Copy"
Cohesion: 0.17
Nodes (50): ParameterLayout, Random, ccPrefix(), clearLaneRow(), copyLane(), copyLaneParameters(), AudioProcessorValueTreeState, LaneKind (+42 more)

### Community 5 - "Custom LookAndFeel"
Cohesion: 0.12
Nodes (45): Button, LookAndFeel_V4, SliderStyle, centredStrip(), Colour, ComboBox, Component, Font (+37 more)

### Community 6 - "Control Group Widgets"
Cohesion: 0.07
Nodes (28): RowStyle, ControlGroup::add(), ControlGroup::ControlGroup(), ControlGroup::paint(), ControlGroup::setHeadingAccent(), ControlRow::ControlRow(), ControlRow::setTooltip(), AudioProcessorValueTreeState (+20 more)

### Community 7 - "Audio Processor Class"
Cohesion: 0.05
Nodes (32): AudioProcessor, AudioProcessorValueTreeState, JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR, RavelAudioProcessor, apvts, ccLaneParams, engine, externalMidiOutput (+24 more)

### Community 8 - "Engine State & CC Runtime"
Cohesion: 0.06
Nodes (32): atomic, SequencerEngine, ccCountdown, ccIntervalSamples, ccLaneHeldValue, ccLaneLastCcValue, ccLaneSlewedValue, ccLaneStates (+24 more)

### Community 9 - "External MIDI Output"
Cohesion: 0.08
Nodes (29): AbstractFifo, CriticalSection, MidiOutput, String, ExternalMidiOutput, currentIdentifier, device, deviceLock (+21 more)

### Community 10 - "Undo History"
Cohesion: 0.10
Nodes (23): deque, vector, AudioProcessor, AudioProcessor, AudioProcessorParameter::Listener, JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR, JUCE_DECLARE_WEAK_REFERENCEABLE, UndoHistory (+15 more)

### Community 11 - "Lane Component Behavior"
Cohesion: 0.08
Nodes (4): easeOut(), LaneComponent::applyValueSlide(), StepSlot::drawnProportion(), StepSlot::setValueSlideProgress()

### Community 12 - "Scales, EDO & Divisions"
Cohesion: 0.09
Nodes (18): array, maxScaleSize, DivisionDef, name, ppq, divisionNameList(), edoChromatic(), StringArray (+10 more)

### Community 13 - "Voice & MPE Allocation"
Cohesion: 0.23
Nodes (21): addRpn(), MidiBuffer, drawWithinSpread(), advanceVoices, allocateMpeChannel, allocateVoice, anyVoiceActive, gateFor (+13 more)

### Community 14 - "Engine Snapshot Struct"
Cohesion: 0.10
Nodes (21): Snapshot, bendRange, ccChannel, ccLanes, ccNumber, ccOffset, ccOn, midiChannel (+13 more)

### Community 15 - "Preset Bar UI"
Cohesion: 0.12
Nodes (17): AlertWindow, Graphics, Component, File, JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR, TextButton, unique_ptr, PresetBar (+9 more)

### Community 16 - "Lane Parameter Bindings"
Cohesion: 0.12
Nodes (17): atomic, LaneParams, active, ccChannel, ccNumber, ccOffset, ccOn, chance (+9 more)

### Community 17 - "Process Block & Buses"
Cohesion: 0.13
Nodes (14): AudioBuffer, BusesLayout, AudioProcessor, AudioProcessorEditor, MidiBuffer, JUCE_CALLTYPE createPluginFilter(), buildSnapshot, createEditor (+6 more)

### Community 18 - "MIDI Device Selector"
Cohesion: 0.14
Nodes (15): Graphics, ExternalMidiSelector, box, deviceIds, ExternalMidiSelector::ExternalMidiSelector(), paint, refresh, rescanButton (+7 more)

### Community 19 - "Lane Count & Layout Updates"
Cohesion: 0.32
Nodes (15): applyCcLaneCount, applyNoteLaneCount, buildWorkspaces, parentHierarchyChanged, RavelAudioProcessorEditor::RavelAudioProcessorEditor(), removeCcLane, removeNoteLane, resized (+7 more)

### Community 20 - "Editor Content Chrome"
Cohesion: 0.15
Nodes (13): ContentComponent, dividerArea, headerArea, markArea, onResized, paint, wordmarkArea, Component (+5 more)

### Community 21 - "Mouse & Stroke Input"
Cohesion: 0.15
Nodes (14): MouseWheelDetails, MouseEvent, LaneComponent::continueStroke(), LaneComponent::endStroke(), LaneComponent::handStrokeTo(), LaneComponent::startStroke(), StepSlot::continueBarDrag(), StepSlot::endBarDrag() (+6 more)

### Community 22 - "Editor Snapshot Tool"
Cohesion: 0.28
Nodes (12): ComponentType, collectDescendants(), AudioProcessorValueTreeState, Component, String, vector, dialInDemoPattern(), findDescendant() (+4 more)

### Community 23 - "Preset Menu Handling"
Cohesion: 0.22
Nodes (12): PopupMenu, function, String, vector, addEntriesToMenu, handleMenuResult, PresetBar::PresetBar(), promptForName (+4 more)

### Community 24 - "Header Name List Helpers"
Cohesion: 0.22
Nodes (5): TabStrip(), LaneComponent, directionNameList(), StringArray, triggerNameList()

### Community 25 - "Lane Snapshot Structs"
Cohesion: 0.15
Nodes (13): LaneSnapshot, active, chance, depth, direction, division, enabled, length (+5 more)

### Community 26 - "Processor Tests"
Cohesion: 0.24
Nodes (8): MemoryBlock, getStateInformation, check(), String, defaultOf(), main(), section(), setChoice()

### Community 27 - "Lane Pattern Construction"
Cohesion: 0.18
Nodes (12): AudioProcessorValueTreeState, LaneKind, LaneComponent::LaneComponent(), StepSlot::StepSlot(), LanePattern, chance, enabled, gate (+4 more)

### Community 28 - "Engine Tests"
Cohesion: 0.29
Nodes (10): prepare, baseSnapshot(), check(), vector, dumpSamples(), main(), run(), section() (+2 more)

### Community 29 - "Timing & Hash Randomness"
Cohesion: 0.42
Nodes (9): int64_t, hashToUnitFloat(), positiveMod(), nextBoundarySample, resolveGlobalIndex, stepIndexFor, timingOffsetFor, splitmix64() (+1 more)

### Community 30 - "Test Event Capture"
Cohesion: 0.29
Nodes (8): EventType, Event, channel, number, sample, type, value, only()

### Community 31 - "Workspace Layout"
Cohesion: 0.25
Nodes (8): preferredWidth, OwnedArray, TabPage, TextButton, layoutWorkspace(), layoutContent, preferredWidth, WorkspaceType

### Community 32 - "Step Layer & Tooltips"
Cohesion: 0.29
Nodes (8): StepLayer, String, LaneComponent::setLayer(), layerTooltip(), spreadRangeText(), StepSlot::beginBarDrag(), StepSlot::getTooltip(), StepSlot::setLayer()

### Community 33 - "Mock Play Head"
Cohesion: 0.33
Nodes (6): AudioPlayHead, Optional, PositionInfo, MockPlayHead, info, runProcessor()

### Community 34 - "Tab Model"
Cohesion: 0.29
Nodes (7): Component, Rectangle, String, Tab, bounds, name, page

### Community 35 - "Test Counters"
Cohesion: 0.29
Nodes (7): Counts, controllers, firstCcNumber, firstNote, firstVelocity, noteOffs, noteOns

### Community 36 - "CC Lane Snapshot"
Cohesion: 0.40
Nodes (5): CcLaneSnapshot, ccChannel, ccNumber, ccOffset, ccOn

### Community 37 - "Lane Runtime State"
Cohesion: 0.40
Nodes (5): int64_t, LaneState, lastGlobalIndex, step, value

### Community 38 - "Lane & Step Painting"
Cohesion: 0.50
Nodes (4): Graphics, LaneComponent::paint(), StepSlot::paint(), StepSlot::paintOverChildren()

### Community 40 - "Voice Struct"
Cohesion: 0.50
Nodes (4): Voice, channel, note, samplesRemaining

### Community 41 - "Slot Hit Testing"
Cohesion: 0.67
Nodes (3): Point, LaneComponent::pointCrossingSlot(), StepSlot::barContains()

### Community 42 - "Bar Value Mapping"
Cohesion: 0.67
Nodes (3): Slider, StepSlot::activeBar(), StepSlot::proportionOf()

### Community 43 - "Content Paint"
Cohesion: 0.67
Nodes (3): Graphics, RavelAudioProcessorEditor::ContentComponent::paint(), paint

### Community 44 - "MPE Channel Slot"
Cohesion: 0.67
Nodes (3): MpeChannelSlot, rangeSent, voiceSlot

### Community 45 - "Pitch Result Struct"
Cohesion: 0.67
Nodes (3): PitchResult, bend, note

## Knowledge Gaps
- **240 isolated node(s):** `name`, `page`, `bounds`, `data`, `length` (+235 more)
  These have ≤1 connection - possible missing edges or undocumented components. (Counts symbols only; 400 node(s) total have ≤1 connection when file, concept and rationale nodes are included.)
- **6 thin communities (<3 nodes) omitted from report** — run `graphify query` to explore isolated nodes.

## Suggested Questions
_Questions this graph is uniquely positioned to answer:_

- **Why does `RavelAudioProcessor` connect `Audio Processor Class` to `Preset File Management`, `Mock Play Head`, `Editor Shell & Tabs`, `Program Name Stubs`, `Engine State & CC Runtime`, `External MIDI Output`, `Undo History`, `Preset Bar UI`, `Lane Parameter Bindings`, `Process Block & Buses`, `MIDI Device Selector`, `Lane Count & Layout Updates`, `Preset Menu Handling`, `Header Name List Helpers`, `Processor Tests`?**
  _High betweenness centrality (0.412) - this node is a cross-community bridge._
- **Why does `RavelAudioProcessorEditor` connect `Editor Shell & Tabs` to `Custom LookAndFeel`, `Audio Processor Class`, `Content Paint`, `Keyboard Shortcut Handling`, `Preset Bar UI`, `MIDI Device Selector`, `Lane Count & Layout Updates`, `Editor Content Chrome`, `Header Name List Helpers`, `Lane Pattern Construction`, `Workspace Layout`?**
  _High betweenness centrality (0.292) - this node is a cross-community bridge._
- **Why does `SequencerEngine` connect `Engine State & CC Runtime` to `CC Lane Snapshot`, `Lane Runtime State`, `Audio Processor Class`, `Voice Struct`, `MPE Channel Slot`, `Pitch Result Struct`, `Voice & MPE Allocation`, `Engine Snapshot Struct`, `Engine Atomics`, `Lane Snapshot Structs`, `Engine Tests`, `Timing & Hash Randomness`?**
  _High betweenness centrality (0.199) - this node is a cross-community bridge._
- **What connects `name`, `page`, `bounds` to the rest of the system?**
  _240 weakly-connected nodes found - possible documentation gaps or missing edges._
- **Should `Preset File Management` be split into smaller, more focused modules?**
  _Cohesion score 0.07622504537205081 - nodes in this community are weakly interconnected._
- **Should `Docs, Build & Install` be split into smaller, more focused modules?**
  _Cohesion score 0.05194805194805195 - nodes in this community are weakly interconnected._
- **Should `Editor Shell & Tabs` be split into smaller, more focused modules?**
  _Cohesion score 0.03571428571428571 - nodes in this community are weakly interconnected._