# TriLane

A three-lane polyrhythmic step sequencer, built as a VST3 for Ableton Live 12 on Windows.

Each lane is an independent 8-step sequencer with its own **length**, **clock rate**,
**direction** and **depth**. The three lanes are folded together into one value, and that
value drives note pitch and/or a MIDI CC.

---

## What it does

**Per lane**

| Control | Range | Notes |
|---|---|---|
| 8 step bars | 0–100 % | Each step also has an on/off toggle |
| Length | 1–8 | Shorter lanes phase against longer ones |
| Rate | 1/1 … 1/32, incl. triplets | Independent per lane — this is where the polyrhythm comes from |
| Direction | Forward, Reverse, Ping-Pong, Random | |
| Depth | −100 % … +100 % | How much this lane affects the mix |
| Mix Mode | Add, Multiply, Max, S&H | How this lane folds into the running mix |

**Mix modes.** Lanes are combined in order 1 → 2 → 3, starting from zero:

- **Add** — `mix += depth × value`. The plain-vanilla mode.
- **Multiply** — scales the mix by the step value. Depth 0 is a no-op, depth 100 % is a
  full multiply. Good for accents and for gating one lane with another.
- **Max** — takes whichever is larger, the mix so far or `depth × value`.
- **S&H** — samples the mix *as it stands at this lane's clock* and holds it. This re-times
  the lanes above it, so it only does something useful on lane 2 or 3 (on lane 1 there is
  nothing upstream to sample).

A step that is toggled **off** is transparent for its lane — nothing is added, multiplied
or held — and it fires no note if that lane is the trigger source.

**Output section**

Root, Scale, Range, Pitch, Bend Range, Velocity, Gate, Offset, Slew, and separate MIDI
channels for notes and CC.

**What Range means depends on the Pitch mode:**

| Pitch mode | Range unit | Scale |
|---|---|---|
| Semitone | scale degrees | applied |
| Continuous (MPE / Pitch Bend) | **semitones** | **bypassed** |

Maximum is 100. Notes clamp to the MIDI range, so how much of a large Range is actually
reachable depends on **Root** — from the default Root of 48 (C3) there are only 79 semitones
of headroom, so a Range above that flattens out at the top. Drop Root to 12 or 24 to use the
full span.

In Semitone mode, mapping onto degrees rather than raw semitones is deliberate: it means
every step lands on a usable note instead of several steps snapping onto the same pitch. On a
five-note pentatonic, 12 degrees is nearly two and a half octaves.

### Continuous (unquantized) pitch

**Pitch** switches between three modes:

- **Semitone** (default) — the mixed value quantizes to the nearest scale degree.
- **Continuous (MPE)** — unquantized, with the bend sent per-note on rotating MPE member
  channels 2–16 (channel 1 is the zone master). Polyphony-safe: a long gate overlapping the
  next note can't have its pitch pulled by the new note's bend. **`Note Chan` is ignored.**
- **Continuous (Pitch Bend)** — unquantized, with plain channel pitch bend on the single
  `Note Chan`. Monophonic, but it survives hosts that merge MIDI channels when routing
  between tracks, which Ableton is reported to do. Start here if MPE comes through flat.

Both continuous modes use the same maths: the nearest semitone carries the note number and
the residual — never more than half a semitone — goes out as pitch bend, sent just before the
note-on so the note starts already in tune.

**The scale is bypassed entirely in continuous modes.** These are raw microtonal values, so
the mapping is a straight ramp and the Scale setting has no effect at all:

```
pitch = Root + mix × Range      (semitones)
```

There is no glide or portamento anywhere. Each step is one discrete microtonal pitch, held
for the step and jumping at the next boundary — exactly one pitch bend per note, not a stream
of them. `Slew` smooths the **CC** output only and never touches pitch, so a repeated step
always plays the identical pitch no matter how high Slew is set.

**Bend Range** is transmitted, not assumed. The MPE default per-note range is ±48 semitones,
so an instrument left at that default while the plugin scaled for ±2 would play 24× the
intended interval. Whenever the mode, range or target channel changes, TriLane sends:

- MPE mode — the MPE Configuration Message (RPN 6) plus the per-note bend range (RPN 0)
- Pitch Bend mode — pitch bend sensitivity (RPN 0) on `Note Chan`, and no zone message

Switching away from MPE tears the zone down (RPN 6 with zero member channels) so the
instrument isn't left in MPE mode. Smaller Bend Range means finer resolution; ±2 is the
default and is plenty, since the residual never exceeds half a semitone.

Those RPNs are written out as raw controller events rather than via `juce::MPEMessages`,
which returns a `MidiBuffer` by value and would allocate on the audio thread.

**Free Run** keeps the sequencer moving while the transport is stopped, so you can
audition patterns without pressing play.

---

## Building

Requires MSVC and CMake. Install both once, from an **elevated** PowerShell:

```powershell
winget install --id Microsoft.VisualStudio.2022.BuildTools -e --accept-package-agreements --accept-source-agreements --override "--wait --quiet --norestart --add Microsoft.VisualStudio.Workload.VCTools --includeRecommended"
```

```powershell
winget install --id Kitware.CMake -e --accept-package-agreements --accept-source-agreements
```

Then, from a normal shell:

```powershell
.\build.ps1
```

The build drops `TriLane.vst3` into `%USERPROFILE%\Documents\VST3`. That folder is used
instead of `C:\Program Files\Common Files\VST3` because the latter needs an elevated shell
to write to on every build. Change it by passing `-DTRILANE_VST3_DIR=...` at configure time.

In Live: **Preferences → Plug-Ins → VST3 Plug-In Custom Folder**, point it at
`Documents\VST3`, and hit **Rescan**.

> **Close Live before rebuilding.** Once Live has loaded the plugin it holds the DLL open,
> and the next build dies with `LNK1104: cannot open file ... TriLane.vst3`. That is a file
> lock, not a code error — quit Live and build again.

### Tests

```powershell
.\build.ps1
.\build\TriLaneTests_artefacts\Release\TriLaneTests.exe
.\build\TriLaneProcessorTests_artefacts\Release\TriLaneProcessorTests.exe
```

63 checks across two suites, neither needing a plugin host.

`Tests/EngineTests.cpp` (46 checks) drives `SequencerEngine` over a synthetic timeline. The
engine takes PPQ positions as plain arguments rather than reading a playhead itself, which is
what makes that possible. Covers step timing, gate length, per-lane length and rate, disabled
steps, the mix modes, transport jumps, stuck-note release on stop, CC output, directions, and
the MPE continuous-pitch path — including that note number plus pitch bend reconstructs the
intended fractional pitch, and that the bend range is actually transmitted.

`Tests/ProcessorTests.cpp` (17 checks) drives the real `TriLaneAudioProcessor::processBlock`
through a mock playhead. This covers the layer where the plugin could compile, load and still
emit nothing: playhead handling, the free-run fallback, the parameter snapshot, state
round-trip, and the MIDI capability flags a host reads to decide whether to offer the plugin
as a MIDI source.

Worth keeping: these tests caught a real bug. Step boundaries were landing one sample late
at some positions, because `ppqPerSample` is `1/24000` at 120 bpm / 48 kHz — not exactly
representable in binary — so `floor(ppq / stepLength)` returned the previous step and step
lengths alternated between 5999 and 6001 samples. Fixed with a boundary epsilon in
`SequencerEngine::process`, sized ~1000× smaller than one sample's worth of PPQ so it can
only ever snap a value already inside rounding noise.

---

## Using it in Live 12

### Sequencing notes

Live does not host MIDI-effect plugins, so TriLane is built as an *instrument* that emits
MIDI. It is silent by design — its output is MIDI, not audio.

1. Drop **TriLane** on a MIDI track (say Track 1).
2. On Track 2, load the instrument you actually want to hear.
3. On Track 2 set **MIDI From → 1-TriLane → TriLane**, and set **Monitor** to **In**.

Track 2 now plays whatever TriLane sequences. This is the same routing trick Scaler and
Cthulhu use in Live.

### Modulating Live's own parameters

Worth being upfront: **a VST3 cannot reach into Live and drive another device's knob.**
There is no such mechanism in the plugin format. What works is a MIDI CC loopback:

1. Install [loopMIDI](https://www.tobias-erichsen.de/software/loopmidi.html) and create a port.
2. **Preferences → Link/Tempo/MIDI**: enable that port as an **Input**, with both
   **Track** and **Remote** switched on.
3. Set TriLane's **Output** to `CC` or `Notes + CC`, and pick a **CC Number**.
4. On the track receiving TriLane's MIDI, set **MIDI To → loopMIDI Port**.
5. Start playback so CC is flowing, press **Ctrl+M**, click the parameter you want to
   modulate, and Live latches onto the incoming CC.

Caveats worth knowing before you rely on it: MIDI mapping is 7-bit, so you get 128
discrete values, and it is control-rate rather than sample-accurate. It is fine for filter
sweeps, sends and macros; it is not a substitute for real modulation. If you want true
parameter modulation with full resolution, that is a Max for Live device — Live 12 Suite
already includes M4L, and its modulation API can target any parameter directly.

---

## Layout

| File | Contents |
|---|---|
| `Source/Parameters.*` | Parameter IDs, choice lists, scale tables |
| `Source/SequencerEngine.*` | The sequencer core and MIDI generation |
| `Source/PluginProcessor.*` | Plugin plumbing, playhead handling, state save/load |
| `Source/PluginEditor.*` | Window layout, output section |
| `Source/LaneComponent.*` | One lane: 8 steps plus its controls |
| `Source/Theme.h` | Colours and custom widget drawing |
| `Tests/EngineTests.cpp` | Engine tests, run as a standalone console app |

### How timing works

Step positions are derived from the host's absolute PPQ position each sample, rather than
accumulated from a running counter:

```
globalIndex = floor(ppqPosition / stepLengthInQuarterNotes)
step        = f(globalIndex, length, direction)
```

That costs a `floor()` per lane per sample, and in exchange loops, transport jumps,
scrubbing and tempo changes all land on exactly the step the timeline says they should,
with no drift and no resync logic. It also means **Random** direction is a hash of the
timeline position rather than a running RNG — so a loop replays the same random pattern
every time round instead of wandering.
