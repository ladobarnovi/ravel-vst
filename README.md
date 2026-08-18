# TriLane

A polyrhythmic step sequencer, built as a VST3 for Ableton Live 12 on Windows. It opens
with one lane and goes up to four, added one at a time.

Each lane is an independent 8-step sequencer with its own **length**, **clock rate**,
**direction** and **depth**. The lanes are folded together into one value, and that value
drives either note pitch or a MIDI CC -- or, in **Poly** mode, each lane triggers its own note
off its own clock.

---

## What it does

**Per lane**

| Control | Range | Notes |
|---|---|---|
| 8 step bars | 0–100 % | The lane's values |
| 8 velocity bars | 0–100 % | Per-step accent, as a trim on the global Velocity (100 % is unity) |
| 8 chance bars | 0–100 % | Per-step probability of firing (thin bar under each value) |
| 8 gate bars | 5–200 % | How long each step's note is held, as % of the step. Above 100 % overlaps into the next step (see Polyphony) |
| 8 step toggles | on/off | Hard mute for a step |
| Lane toggle | on/off | Mutes the whole lane: transparent for the mix, triggers nothing |
| Length | 1–8 | Shorter lanes phase against longer ones. Steps past the length grey out, and stay editable |
| Rate | 1/1 … 1/32, incl. triplets | Independent per lane — this is where the polyrhythm comes from |
| Direction | Forward, Reverse, Ping-Pong, Random | |
| Depth | −100 % … +100 % | How much this lane affects the mix |
| Mix Mode | Add, Multiply, Max, S&H | How this lane folds into the running mix |
| Nudge | −100 % … +100 % | Shifts the whole lane earlier/later, up to half a step |
| Humanize | 0–100 % | Random timing jitter, repeatable per bar |
| Lane CC | on/off + number/channel | This lane's own CC destination |
| RND / CLR / ⋯ | — | Pattern actions |

### Probability

Each step has a **Chance**. A step that loses its roll behaves exactly like a step that's
switched off: transparent for the mix, and it fires nothing. Chance 100 % always fires,
0 % never does.

The roll is a hash of the timeline position, not a draw from a running RNG — so it holds
steady for the whole step, and **a loop skips exactly the same steps every time round**
rather than drifting. Same design as Random direction, for the same reason.

### Swing, Nudge and Humanize

**Swing** (Timing tab, global) delays every other step of the absolute grid, so it stays anchored
to the bar rather than to wherever a short pattern happened to start. **Nudge** shifts one
lane wholesale. **Humanize** adds per-step jitter, also hash-based, so it's repeatable.

All three move step *boundaries*, which meant reworking how the step index is derived. It's
still stateless — rather than `floor(ppq / stepPpq)`, it picks the largest candidate index
whose *shifted* boundary the timeline has passed, checking only the adjacent candidates.
That's sufficient because offsets are clamped to ±0.49 of a step, which also guarantees
boundaries stay monotonically ordered. With all three at zero it reduces exactly to the old
`floor()`, and there's a test asserting that.

### Per-lane CC outputs

Each lane can send **its own CC** on its own number and channel, alongside the mix CC — so
one instance can modulate up to four destinations. Lane CC follows the lane's raw step value
and **ignores Depth**, since Depth governs the lane's share of the mix, not its own output.
Inactive steps latch the previous level rather than dropping to zero. All CC streams share
the global **Slew**.

### Pattern actions

**RND** re-rolls a lane's values, **CLR** zeroes them. Both touch values only — the toggles
and chances are left alone, so a lane's rhythm survives a re-roll. The **⋯** menu has Rotate
Left/Right, Invert Values, and Copy/Paste Pattern (the clipboard is shared, so you can paste
one lane onto another). Rotate and paste move value, on/off, chance, velocity and gate
together — rotating only the values would slide a pattern out from under its own rhythm.

All of these go through the host as real parameter changes wrapped in change gestures, so they
land in automation and undo instead of silently mutating state behind the host's back. The
logic lives in `Parameters.cpp` rather than the button callbacks, which is what lets it be
tested without a UI.

**Mix modes.** Lanes are combined in lane order, starting from zero:

- **Add** — `mix += depth × value`. The plain-vanilla mode.
- **Multiply** — scales the mix by the step value. Depth 0 is a no-op, depth 100 % is a
  full multiply. Good for accents and for gating one lane with another.
- **Max** — takes whichever is larger, the mix so far or `depth × value`.
- **S&H** — samples the mix *as it stands at this lane's clock* and holds it. This re-times
  the lanes above it, so it only does something useful below the first lane (on lane 1 there
  is nothing upstream to sample).

A step that is toggled **off** is transparent for its lane — nothing is added, multiplied
or held — and it fires no note if that lane is the trigger source.

**Lanes.** Every lane starts as eight steps of zero — a flat pattern on the root, not a demo
to clear away — and lanes differ only in their default rate. *+ Add lane* and *Remove lane N*
sit under the last lane, and the window grows and shrinks to fit. Lanes are added and removed at the bottom, and a removed lane keeps its
pattern: bringing it back restores it exactly. Muting a lane with its own toggle is the same
thing as switching every one of its steps off at once, so a muted lane is transparent for the
mix in every mix mode and triggers nothing in either mode.

All four lanes' parameters exist from the moment the plugin is loaded, because a VST3 cannot
add parameters later. The lane count only decides which of them are heard and shown, which is
what makes it automatable and undoable like any other control.

**Header and tabs**

The header carries **Output** (Notes or CC — mutually exclusive) and **Poly**. Everything
else global lives in three tabs:

- **Pitch** — Root, Scale, Range, Quantize / Bend range, Offset, Slew / Velocity, Voices
- **Timing** — Swing, Free run, Trigger
- **Routing** — Note channel, Mix CC, Mix CC channel, and each lane's own CC destination

Gate and per-step Velocity are per-step now — see the **Per lane** table above.

**What Range means depends on Quantize:**

| Quantize | Range unit | Scale |
|---|---|---|
| On | scale degrees | applied |
| Off | **semitones** | **bypassed** |

A scale degree is not always a whole semitone — see [Scales and tunings](#scales-and-tunings)
below.

Maximum is 100. Notes clamp to the MIDI range, so how much of a large Range is actually
reachable depends on **Root** — from the default Root of 48 (C3) there are only 79 semitones
of headroom, so a Range above that flattens out at the top. Drop Root to 12 or 24 to use the
full span.

With Quantize on, mapping onto degrees rather than raw semitones is deliberate: it means
every step lands on a usable note instead of several steps snapping onto the same pitch. On a
five-note pentatonic, 12 degrees is nearly two and a half octaves.

### Scales and tunings

The **Scale** list holds the familiar 12-tone scales plus scales in three other equal
divisions of the octave. Scales prefixed with a number are in that EDO:

| Tuning | Step | Scales | Why it's there |
|---|---|---|---|
| 12-EDO | 100 ¢ | Chromatic, Major, Natural/Harmonic Minor, both Pentatonics, Dorian, Mixolydian, Whole Tone | The usual |
| **19-EDO** | 63.2 ¢ | Chromatic, Major, Natural/Harmonic Minor, Pentatonic Minor, Blues | A meantone. The diatonic scales are the ordinary ones respelled 3-3-2-3-3-3-2, so they still sound major and minor, with thirds nearer just than 12-EDO manages. Sharps and flats separate: C♯ sits a step *below* D♭ |
| **23-EDO** | 52.2 ¢ | Chromatic, Pentatonic, Mavila 7, Mavila 9 | The awkward one — its best fifth is a quarter-tone flat, so diatonic harmony doesn't survive the trip. What it has instead is **mavila**, where that flat fifth turns the diatonic scale inside out: the major-scale-shaped scale comes out with two large steps and five small ones, and its third degree is minor-sized |
| **53-EDO** | 22.6 ¢ (the Holdrian comma) | Chromatic, Just Major, Just Minor, Pythagorean Major, Just Pentatonic, Rast, Hicaz | Fifth 701.9 ¢, major third 384.9 ¢ — it renders 5-limit just intonation to within a couple of cents, and Pythagorean tuning separately, which is why the two major scales differ at all. It's also the grid Turkish makam theory is written on |

Every tuning keeps a 2:1 octave, so a full scale-octave is always exactly 12 semitones however
many degrees it took to climb, and patterns stay octave-aligned with everything else in the
session. Only the degrees *within* an octave fall between the keys.

**Those in-between degrees play as a note plus pitch bend**, the same mechanism continuous
pitch uses — so with Quantize on, a non-12 scale is subject to the same limit: **one microtone
at a time per channel.** Overlapping notes (a step's Gate over 100 %, Voices above 1, or four
poly lanes at once) share the channel's wheel, so they can't hold different microtones. Keep to one
voice for microtonal work, or give the lanes separate instances. `Bend range` becomes live and
is announced by RPN just as it is in continuous mode; the residual never exceeds half a
semitone, so the ±2 default is plenty.

Remember that **Range is counted in degrees**. 53-EDO chromatic spends them fast — at the
default Range of 12 it covers a quarter of an octave — so turn Range up for the larger EDOs.

### Quantize and continuous pitch

**Quantize** (on by default) is the pitch mode switch:

- **On** — the mixed value snaps to the nearest degree of the selected **Scale**.
- **Off** — continuous, unquantized pitch. The **Scale** setting has no effect at all, and
  Range is read directly as semitones:

  ```
  pitch = Root + mix × Range      (semitones)
  ```

Either way, pitch goes out as a note plus pitch bend, both on the single **Note channel**
(Routing tab) — the nearest semitone carries the note number, and the residual — never more
than half a semitone — goes out as pitch bend, sent just before the note-on so the note starts
already in tune. With Quantize on and a 12-EDO scale the residual is always exactly zero, so
no bend is sent at all; a 19-, 23- or 53-EDO scale needs one even with Quantize on, for the
same reason continuous pitch does (see [Scales and tunings](#scales-and-tunings)).

There is no glide or portamento anywhere. Each step is one discrete pitch, held for the step
and jumping at the next boundary — exactly one pitch bend per note, not a stream of them.
`Slew` smooths the **CC** output only and never touches pitch, so a repeated step always
plays the identical pitch no matter how high Slew is set.

**Bend Range** is transmitted, not assumed: whenever the range, the target channel, or
*whether pitch bends at all* changes — Quantize, or switching to or from a non-12 scale —
TriLane sends pitch bend sensitivity (RPN 0) on the Note channel. Smaller Bend Range means
finer resolution; ±2 is the default and is plenty, since the residual never exceeds half a
semitone.

Turning bending off again — Quantize back on with a 12-EDO scale — explicitly recentres the
wheel. Nothing in that mode ever writes the wheel again, so the bend the last note left on
the channel would otherwise detune every note that followed.

That RPN is written out as a raw controller event rather than via a JUCE helper that returns
a `MidiBuffer` by value and would allocate on the audio thread.

**Free Run** (off by default) keeps the sequencer moving while the transport is stopped, so
you can audition patterns without pressing play. Off, the sequencer follows the host
transport and a freshly loaded instance stays silent until you press play.

### Polyphony

**Voices** (Pitch tab, 1–8) is the ceiling on notes sounding at once. At **1** the behaviour is
the original monophonic one — a retrigger always closes the previous note, so a step's Gate
over 100 % simply cuts itself off. Above 1, a step with a long Gate **overlaps into the
following step**.

This is about overlapping gates, not chords: pitch comes from the single mixed value, so
simultaneous triggers would land on the same note. Set a step's Gate above 100 % to hear it.

Two details the engine has to get right:

- **A repeated pitch reuses its voice rather than stacking.** MIDI can't distinguish two
  identical note-ons on one channel, so one note-off would silence both and the survivor
  would hang forever. Same pitch + same channel retriggers in place.
- **Turning Voices down releases anything outside the new limit**, rather than orphaning it.

Every voice shares the single **Note channel** and its one pitch bend register, mono or poly —
so overlapping notes that need different microtones (Quantize off, or a non-12-EDO scale)
can't have them; the most recent bend applies to all of them. That's a MIDI limitation of a
single channel, not a bug — keep Voices at 1 for clean microtonal work, or spread lanes
across separate instances on separate channels if you need both poly and microtonality.

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

134 checks across two suites, neither needing a plugin host.

`Tests/EngineTests.cpp` (96 checks) drives `SequencerEngine` over a synthetic timeline. The
engine takes PPQ positions as plain arguments rather than reading a playhead itself, which is
what makes that possible. Covers step timing, gate length, per-lane length and rate, disabled
steps, the mix modes, transport jumps, stuck-note release on stop, CC output, directions, and
the continuous-pitch path — including that note number plus pitch bend reconstructs the
intended fractional pitch, and that the bend range is actually transmitted.

`Tests/ProcessorTests.cpp` (38 checks) drives the real `TriLaneAudioProcessor::processBlock`
through a mock playhead. This covers the layer where the plugin could compile, load and still
emit nothing: playhead handling, the free-run fallback, the parameter snapshot, state
round-trip, every pattern action, and the MIDI capability flags a host reads to decide whether
to offer the plugin as a MIDI source.

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
3. Set TriLane's **Output** to `CC`, and pick a **CC Number**.
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
| `Source/PluginEditor.*` | Window layout, header, and the Pitch/Timing/Routing tabs |
| `Source/LaneComponent.*` | One lane: 8 steps plus its controls |
| `Source/Controls.*` | Shared row/column/tab building blocks the editor and lanes are built from |
| `Source/Theme.h` | Colours and custom widget drawing |
| `Tests/EngineTests.cpp` | Engine tests, run as a standalone console app |
| `Tests/ProcessorTests.cpp` | Processor tests, driven through a mock playhead |

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
