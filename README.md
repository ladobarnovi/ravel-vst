# Ravel

A polyrhythmic step sequencer, built as a VST3 for Ableton Live 12 on Windows.

Ravel holds **two independent stacks of lanes**: a **Notes** stack that drives pitch, and a
**CC** stack that drives MIDI CC. Each opens with one lane and goes up to four, added one at a
time, and each keeps its own lane count — growing one costs the other nothing.

Every lane is a 16-step sequencer with its own **length**, **clock rate** and **depth**. Within
a stack the lanes fold together into one value: the Note fold becomes pitch (or, in **Poly**
mode, each Note lane triggers its own note off its own clock), and the CC fold becomes the
**Mix CC**. On top of that, each CC lane also has its own direct tap onto its own CC number, so
one instance can modulate up to five destinations at once.

---

## What it does

### Two stacks

The window's top-level tabs are **Notes** and **CC**. They are not two views of the same lanes —
they are two separate sequencers sharing one clock, one transport and one plugin instance.

|  | Notes stack | CC stack |
|---|---|---|
| What the fold drives | Pitch, over an MPE zone | The Mix CC |
| Lanes | 1–4, own count | 1–4, own count |
| Per-step | Value, Velocity, Chance, Gate | Value, Chance |
| Per-lane shaping | Direction, Mix mode, Nudge, Humanize | — (always Forward / Add, no jitter) |
| Per-lane output | — | Its own Send / Number / Channel / Offset |
| Own Swing | Yes | Yes, independent |

CC lanes deliberately carry less. Velocity and Gate are only ever arguments to *start a note*,
and a CC lane never starts one, so it has neither — which also keeps the plugin's automatable
parameter count from doubling for nothing.

### Per lane

| Control | Range | Notes |
|---|---|---|
| 16 step bars | 0–100 % | The lane's values |
| 16 step toggles | on/off | Hard mute for a step |
| Lane toggle | on/off | Mutes the whole lane: transparent for the mix, triggers nothing |
| Length | 1–16 | Shorter lanes phase against longer ones. Steps past the length grey out, and stay editable |
| Rate | 1/1 … 1/32, incl. triplets | Independent per lane — this is where the polyrhythm comes from |
| Depth | −100 % … +100 % | How much this lane contributes to its stack's fold |
| 16 velocity bars | 0–100 % | *Note lanes only.* Per-step accent, as a trim on the global Velocity (100 % is unity) |
| 16 gate bars | 5–200 % | *Note lanes only.* How long each step's note is held, as % of the step. Above 100 % overlaps into the next step (see Polyphony) |
| 16 chance bars | 0–100 % | Per-step probability of firing |
| Direction | Forward, Reverse, Ping-Pong, Random | *Note lanes only* |
| Mix Mode | Add, Multiply, Max, S&H | *Note lanes only.* How this lane folds into the running mix |
| Nudge | −100 % … +100 % | *Note lanes only.* Shifts the whole lane earlier/later, up to half a step |
| Humanize | 0–100 % | *Note lanes only.* Random timing jitter, repeatable per bar |
| Send / Number / Channel / Offset | on/off, 0–127, 1–16, 0–100 % | *CC lanes only.* This lane's own CC destination |
| RND / CLR / ⋯ | — | Pattern actions |
| Remove | — | Takes this lane out. The lanes below it move up to close the gap |

On a **Note lane** the sixteen tall bars edit one of four per-step rows at a time, picked with
the **Value / Velocity / Prob / Gate** selector down the left of the lane. The three that are
not selected show as faint ticks across the bars, and only where they are away from their
default, so an untouched lane stays clean.

A **CC lane** has no selector — its bars always edit Value, and the column is left blank rather
than filled with four buttons that would do nothing. Its per-step Chance still exists and still
works (it decides whether a step reaches the fold, and the fold is what the CC output follows),
and its tick is still drawn, but it is reachable only through host automation.

### Probability

Each step has a **Chance**. A step that loses its roll behaves exactly like a step that's
switched off: transparent for the mix, and it fires nothing. Chance 100 % always fires,
0 % never does.

The roll is a hash of the timeline position, not a draw from a running RNG — so it holds
steady for the whole step, and **a loop skips exactly the same steps every time round**
rather than drifting. Same design as Random direction, for the same reason.

### Swing, Nudge and Humanize

**Swing** delays every other step of the absolute grid, so it stays anchored to the bar rather
than to wherever a short pattern happened to start. Each stack has **its own Swing** — Notes and
CC answer to their own clocks, so they can be shifted differently, or not at all. **Nudge**
shifts one lane wholesale and **Humanize** adds per-step jitter, also hash-based and so
repeatable; both are Note-lane controls.

All three move step *boundaries*, which meant reworking how the step index is derived. It's
still stateless — rather than `floor(ppq / stepPpq)`, it picks the largest candidate index
whose *shifted* boundary the timeline has passed, checking only the adjacent candidates.
That's sufficient because offsets are clamped to ±0.49 of a step, which also guarantees
boundaries stay monotonically ordered. With all three at zero it reduces exactly to the old
`floor()`, and there's a test asserting that.

### CC outputs

There are two kinds, and they are independent:

- **The Mix CC** (CC tab → Output) is the CC stack's fold, exactly as pitch is the Note stack's
  fold. Its **Send** switch, **Number**, **Channel** and **Offset** live on the CC tab.
- **Each CC lane's own tap** follows that lane's raw step value and **ignores Depth**, since
  Depth governs the lane's share of the fold, not its own output. Its Send, Number, Channel and
  Offset live on the lane's own strip. Defaults are CC 20, 21, 22 and 23 for lanes 1–4.

The two Offsets never cross: the CC tab's Offset shifts the Mix CC, and a lane's Offset shifts
only that lane's tap, so one lane can be recentred without moving the rest. Inactive steps latch
the previous level rather than dropping to zero. All CC streams share the global **Slew**.

### Pattern actions

**RND** re-rolls a lane's values, **CLR** zeroes them. Both touch values only — the toggles
and chances are left alone, so a lane's rhythm survives a re-roll. The **⋯** menu has Rotate
Left/Right, Invert Values, and Copy/Paste Pattern. Rotate and paste move value, on/off and
chance together — and, on a Note lane, velocity and gate as well — because rotating only the
values would slide a pattern out from under its own rhythm.

The clipboard is per stack: you can paste one Note lane onto another, or one CC lane onto
another, but not across the two.

All of these go through the host as real parameter changes wrapped in change gestures, so they
land in automation and undo instead of silently mutating state behind the host's back. The
logic lives in `Parameters.cpp` rather than the button callbacks, which is what lets it be
tested without a UI.

### Undo

The two arrows next to the title undo and redo the last edit, and **Ctrl+Z** / **Ctrl+Shift+Z**
do the same thing (**Ctrl+Y** is accepted for redo as well). An arrow greys out when there is
nothing on that side of the history, which is also the answer to a Ctrl+Z that appears to do
nothing: the plugin swallows the keystroke either way rather than letting it fall through to
the host, since running out of steps in the plugin and silently starting to undo the
*arrangement* instead would be a far worse surprise.

One edit is one turn of the message loop, so a pattern action that writes forty parameters is
a single step, while two clicks on two different steps are two. Host automation sends no
gestures and so never fills the history. The history lives on the processor rather than the
editor, and therefore survives closing the plugin window; loading a session clears it.

### Mix modes

Lanes are combined in lane order, starting from zero:

- **Add** — `mix += depth × value`. The plain-vanilla mode.
- **Multiply** — scales the mix by the step value. Depth 0 is a no-op, depth 100 % is a
  full multiply. Good for accents and for gating one lane with another.
- **Max** — takes whichever is larger, the mix so far or `depth × value`.
- **S&H** — samples the mix *as it stands at this lane's clock* and holds it. This re-times
  the lanes above it, so it only does something useful below the first lane (on lane 1 there
  is nothing upstream to sample).

A step that is toggled **off** is transparent for its lane — nothing is added, multiplied
or held — and it fires no note if that lane is the trigger source. Mix mode is a Note-lane
control; CC lanes always fold with Add.

### Lanes

Every lane starts as sixteen steps of zero — a flat pattern on the root, not a demo to clear
away — and lanes differ only in their default rate. *+ Add lane* sits under the last lane of the
current stack and appends one at the bottom; **each lane carries its own Remove**, at the right
of its action row, so any lane can go and not just the last one. The window grows and shrinks to
fit the lane count on its own; **it's also resizable by hand**, from the bottom-right corner or
the host's own window border. Dragging it doesn't reflow the layout, it zooms the whole thing
uniformly, between 60 % and 150 % of native size, so bars and text scale together rather than
the step area alone stretching. The size is remembered per session, the same way the pattern is.

Removing a lane closes the gap behind it: every lane below the removed one moves up a slot,
bringing its own controls with it — rate, depth, direction, nudge, its CC destination, not only
its pattern. So removing lane 2 of 3 leaves you with the old lanes 1 and 3, in that order, which
is the thing a single *Remove lane N* button at the bottom could not express.

The lane accent colours stay with the *slot* rather than with the pattern, because they mark
where the sequencer is in the stack: after a removal the third lane's pattern is drawn in the
second lane's amber.

**Removal is destructive.** The lane that moved up has overwritten the removed one, and the
slot freed at the top of the stack goes back to its defaults — so *+ Add lane* always gives a
new lane rather than a copy of the one that just moved. **Ctrl+Z** is what brings a removed
lane back; the shift and the new lane count land as a single undo step.

Muting a lane with its own toggle is the same thing as switching every one of its steps off at
once, so a muted lane is transparent for its fold in every mix mode and triggers nothing. That
is also how you make an instance CC-only: mute its Note lanes.

All four lanes of both stacks exist as parameters from the moment the plugin is loaded, because
a VST3 cannot add parameters later. The two lane counts only decide which of them are heard and
shown, which is what makes them automatable and undoable like any other control.

### The tabs

The header carries the title and the two undo arrows, nothing else. Everything global sits under
whichever of the two top-level tabs it belongs to, laid out as a flat row of columns:

**Notes**

| Column | Controls |
|---|---|
| Pitch | Root, Scale, Range, Quantize |
| Output | Bend range, Offset |
| Voice | Velocity, Voices, Poly |
| Clock | Swing, Free run, Trigger |

**CC**

| Column | Controls |
|---|---|
| Output | Send, Number, Channel, Offset, Slew |
| Clock | Swing |

The two Offsets are not the same control. The **CC** one shifts that stack's fold before it
becomes the Mix CC, 0–100 %. The **Notes** one transposes in whole octaves, −3 … +3, and applies
*after* Root, Range and the scale have resolved a pitch — so a pattern keeps its shape and its
scale degrees and simply moves, instead of being squashed against the fold's 0–100 % clamp.
Notes still clamp to the MIDI range, so how much of a ±3 octave shift is reachable depends on
Root and Range. **Slew** only ever smooths CC — the Mix CC and every
lane's own tap — and never touches pitch, so it lives on the CC tab. **Free run** is one shared
switch for both stacks, because splitting it would mean running two independent timelines
through the whole engine for a narrow benefit. **Trigger** picks which Note lane's advance fires
the shared note, and dims in Poly mode, where every lane triggers itself.

### Root and Range

**Range** is counted in **octaves**, 1–10, default 2. What an octave is made of depends on
Quantize:

| Quantize | Range spans | Scale |
|---|---|---|
| On | `Range × the scale's degrees-per-octave` degrees | applied |
| Off | `Range × 12` semitones | **bypassed** |

Either way an octave is an octave, so a given Range value covers the same musical distance
whether the scale packs 5 degrees into an octave or 53 — which is what counting in octaves buys
over counting in raw degrees. On a five-note pentatonic, Range 2 is ten steps; on 53-EDO
chromatic it is 106, spread across the same two octaves.

Notes clamp to the MIDI range, so how much of a large Range is actually reachable depends on
**Root** — from the default Root of 48 (C3) there are only 79 semitones of headroom, so a Range
above about 6 octaves flattens out at the top. Drop Root to 12 or 24 to use the full span.

With Quantize on, mapping onto degrees rather than raw semitones is deliberate: it means every
step lands on a usable note instead of several steps snapping onto the same pitch.

### Scales and tunings

The **Scale** list holds the familiar 12-tone scales plus scales in five other equal
divisions of the octave. Scales prefixed with a number are in that EDO:

| Tuning | Step | Scales | Why it's there |
|---|---|---|---|
| 12-EDO | 100 ¢ | Chromatic, Major, Natural/Harmonic Minor, both Pentatonics, Dorian, Mixolydian, Whole Tone | The usual |
| **19-EDO** | 63.2 ¢ | Chromatic, Major, Natural/Harmonic Minor, Pentatonic Minor, Blues | A meantone. The diatonic scales are the ordinary ones respelled 3-3-2-3-3-3-2, so they still sound major and minor, with thirds nearer just than 12-EDO manages. Sharps and flats separate: C♯ sits a step *below* D♭ |
| **23-EDO** | 52.2 ¢ | Chromatic, Pentatonic, Mavila 7, Mavila 9 | The awkward one — its best fifth is a quarter-tone flat, so diatonic harmony doesn't survive the trip. What it has instead is **mavila**, where that flat fifth turns the diatonic scale inside out: the major-scale-shaped scale comes out with two large steps and five small ones, and its third degree is minor-sized |
| **31-EDO** | 38.7 ¢ | Chromatic, Major, Natural/Harmonic Minor, Pentatonic Minor, Blues | The best meantone here. Fifth 696.8 ¢, major third 387.1 ¢ — within a cent and a half of just, closer than 19-EDO gets. Same 5-3-5-5-3-5-3-style respelling as 19-EDO, with more room between sharps and flats |
| **41-EDO** | 29.3 ¢ | Chromatic, Major, Natural/Harmonic Minor, Pentatonic Minor | The opposite trade from 31: fifth 702.4 ¢, under half a cent from pure 3/2 — better than 12-EDO's own — at the cost of a merely passable major third (380.5 ¢). Reach for it when the fifths need to be exact rather than the thirds |
| **53-EDO** | 22.6 ¢ (the Holdrian comma) | Chromatic, Just Major, Just Minor, Pythagorean Major, Just Pentatonic, Rast, Hicaz | Fifth 701.9 ¢, major third 384.9 ¢ — it renders 5-limit just intonation to within a couple of cents, and Pythagorean tuning separately, which is why the two major scales differ at all. It's also the grid Turkish makam theory is written on |

Every tuning keeps a 2:1 octave, so a full scale-octave is always exactly 12 semitones however
many degrees it took to climb, and patterns stay octave-aligned with everything else in the
session. Only the degrees *within* an octave fall between the keys.

**Those in-between degrees play as a note plus pitch bend**, the same mechanism continuous
pitch uses — so with Quantize on, a non-12 scale is subject to the same limit: **one microtone
at a time per channel.** Overlapping notes (a step's Gate over 100 %, Voices above 1, or four
poly lanes at once) share the channel's wheel, so they can't hold different microtones. Keep to
one voice for microtonal work, or give the lanes separate instances. `Bend range` becomes live
and is announced by RPN just as it is in continuous mode; the residual never exceeds half a
semitone, so the ±2 default is plenty.

### Quantize and continuous pitch

**Quantize** (on by default) is the pitch mode switch:

- **On** — the mixed value snaps to the nearest degree of the selected **Scale**.
- **Off** — continuous, unquantized pitch. The **Scale** setting has no effect at all, and
  Range is read as 12 semitones per octave:

  ```
  pitch = Root + mix × Range × 12      (semitones)
  ```

Either way, pitch goes out as a note plus pitch bend — the nearest semitone carries the note
number, and the residual — never more than half a semitone — goes out as pitch bend, sent just
before the note-on so the note starts already in tune. Each note gets its own MPE member
channel, so the bend is the note's alone (see [MPE](#mpe)). With Quantize on and a 12-EDO scale the residual is always exactly zero,
so no bend is sent at all; a 19-, 23-, 31-, 41- or 53-EDO scale needs one even with Quantize on,
for the same reason continuous pitch does (see [Scales and tunings](#scales-and-tunings)).

There is no glide or portamento anywhere. Each step is one discrete pitch, held for the step
and jumping at the next boundary — exactly one pitch bend per note, not a stream of them.
`Slew` smooths the **CC** output only and never touches pitch, so a repeated step always
plays the identical pitch no matter how high Slew is set.

**Bend Range** is transmitted, not assumed: whenever the range, the target channel, or
*whether pitch bends at all* changes — Quantize, or switching to or from a non-12 scale —
Ravel sends pitch bend sensitivity (RPN 0) on a member channel just before that channel's
first note. Changing the range — or changing *whether pitch bends at all*, via Quantize or a
switch to or from a non-12 scale — marks every channel unprimed, so each one is re-sent the
range the next time it is used. Smaller Bend Range means
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

**Voices** (Notes tab → Voice, 1–8) is the ceiling on notes sounding at once. At **1** the
behaviour is the original monophonic one — a retrigger always closes the previous note, so a
step's Gate over 100 % simply cuts itself off. Above 1, a step with a long Gate **overlaps into
the following step**.

In mixed mode this is about overlapping gates, not chords: pitch comes from the single mixed
value, so simultaneous triggers would land on the same note. Set a step's Gate above 100 % to
hear it. **Poly** mode is the other way to get several notes at once — each Note lane triggers
its own note off its own clock and its own value, so the stack runs as independent voices and
**Trigger** goes unused. Each lane gets its own block of voice slots, so one lane's notes can
never steal another's.

Two details the engine has to get right:

- **A repeated pitch reuses its voice rather than stacking.** MIDI can't distinguish two
  identical note-ons on one channel, so one note-off would silence both and the survivor
  would hang forever. Same pitch + same channel retriggers in place.
- **Turning Voices down releases anything outside the new limit**, rather than orphaning it.
  So does flipping the Poly switch, which re-partitions the slots underneath.

Overlapping notes each hold their own microtone, because each one is on its own channel —
see below.

### MPE

Ravel always speaks MPE, and there is no switch for it. Output is a standard **MPE Lower
Zone**: channel 1 is the zone master, channels 2–16 are the 15 member channels, and every
simultaneously-sounding note is allocated its own member channel with its own pitch bend.
The zone is announced with RPN 6 on the master channel before any note goes out, and again
after a transport reset, since a receiver's state cannot be assumed across one.

That is what makes poly microtonality work at all. One channel has one pitch bend register,
so on a single channel the most recent bend applies to every note sounding on it — two
overlapping notes could not hold different microtones. A channel each removes the conflict,
so Quantize-off and non-12-EDO scales stay in tune under polyphony.

Fifteen member channels is the ceiling. A sixteenth simultaneous note steals the channel
whose note is closest to finishing rather than exceeding the pool.

The receiving instrument has to be in MPE mode for this to sound right; a non-MPE instrument
listening on one channel will hear only the notes that land there. There is no single-channel
fallback — **Note channel** used to select one and has been removed along with the switch,
since the zone master is fixed at channel 1.

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

The build drops `Ravel.vst3` into `%USERPROFILE%\Documents\VST3`. That folder is used
instead of `C:\Program Files\Common Files\VST3` because the latter needs an elevated shell
to write to on every build. Change it by passing `-DRAVEL_VST3_DIR=...` at configure time.

In Live: **Preferences → Plug-Ins → VST3 Plug-In Custom Folder**, point it at
`Documents\VST3`, and hit **Rescan**.

> **Close Live before rebuilding.** Once Live has loaded the plugin it holds the DLL open,
> and the next build dies with `LNK1104: cannot open file ... Ravel.vst3`. That is a file
> lock, not a code error — quit Live and build again.

### Building on macOS via CI

There's no Mac in this project's development loop, so Ravel is cross-built for macOS in CI
rather than on a local machine. `.github/workflows/build-macos.yml` runs on a `macos-14`
GitHub Actions runner, clones the same JUCE tag the Windows build uses (JUCE isn't committed
to this repo — see `.gitignore`), and configures with `-DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"`
so the output runs on both Apple Silicon and Intel Macs.

To run it: push this repo to GitHub, open the **Actions** tab, select **Build macOS Plugin**,
and click **Run workflow**. When it finishes, the run page has two downloadable artifacts,
`Ravel-VST3-macOS` and `Ravel-AU-macOS` — each a zip containing the `.vst3`/`.component`
bundle. Unzip, then move each bundle into the standard per-user plugin folder so Live and
Logic find it without any custom-folder setup:

```bash
mv Ravel.vst3 ~/Library/Audio/Plug-Ins/VST3/
mv Ravel.component ~/Library/Audio/Plug-Ins/Components/
```

The build is unsigned — there's no Apple Developer certificate in this pipeline, so Gatekeeper
will refuse to load it on first launch of Live. Clear the quarantine flag once, from Terminal:

```bash
xattr -cr ~/Library/Audio/Plug-Ins/VST3/Ravel.vst3
xattr -cr ~/Library/Audio/Plug-Ins/Components/Ravel.component
```

(Signing and notarizing for distribution to other people needs a paid Apple Developer account —
out of scope for a plugin only running on your own machine.)

### Tests

```powershell
.\build.ps1
.\build\RavelTests_artefacts\Release\RavelTests.exe
.\build\RavelProcessorTests_artefacts\Release\RavelProcessorTests.exe
```

184 checks across two suites, neither needing a plugin host.

`Tests/EngineTests.cpp` (117 checks) drives `SequencerEngine` over a synthetic timeline. The
engine takes PPQ positions as plain arguments rather than reading a playhead itself, which is
what makes that possible. Covers step timing, gate length, per-lane length and rate, disabled
steps, the mix modes, transport jumps, stuck-note release on stop, directions, probability,
swing and nudge, per-step velocity, polyphony and poly mode, the Mix CC and each lane's own CC
tap (including that the two Offsets stay out of each other's way), and the continuous-pitch
path — including that note number plus pitch bend reconstructs the intended fractional pitch,
that non-12 EDO scales land where the tuning says, and that the bend range is actually
transmitted.

`Tests/ProcessorTests.cpp` (67 checks) drives the real `RavelAudioProcessor::processBlock`
through a mock playhead. This covers the layer where the plugin could compile, load and still
emit nothing: playhead handling, the free-run fallback, the parameter snapshot, state
round-trip, every pattern action, lane add/remove and its undo behaviour, and the MIDI
capability flags a host reads to decide whether to offer the plugin as a MIDI source.

Worth keeping: these tests caught a real bug. Step boundaries were landing one sample late
at some positions, because `ppqPerSample` is `1/24000` at 120 bpm / 48 kHz — not exactly
representable in binary — so `floor(ppq / stepLength)` returned the previous step and step
lengths alternated between 5999 and 6001 samples. Fixed with a boundary epsilon in
`SequencerEngine::process`, sized ~1000× smaller than one sample's worth of PPQ so it can
only ever snap a value already inside rounding noise.

---

## Using it in Live 12

### Sequencing notes

Live does not host MIDI-effect plugins, so Ravel is built as an *instrument* that emits
MIDI. It is silent by design — its output is MIDI, not audio.

1. Drop **Ravel** on a MIDI track (say Track 1).
2. On Track 2, load the instrument you actually want to hear.
3. On Track 2 set **MIDI From → 1-Ravel → Ravel**, and set **Monitor** to **In**.

Track 2 now plays whatever Ravel sequences. This is the same routing trick Scaler and
Cthulhu use in Live.

### Modulating Live's own parameters

Worth being upfront: **a VST3 cannot reach into Live and drive another device's knob.**
There is no such mechanism in the plugin format. What works is a MIDI CC loopback:

1. Install [loopMIDI](https://www.tobias-erichsen.de/software/loopmidi.html) and create a port.
2. **Preferences → Link/Tempo/MIDI**: enable that port as an **Input**, with both
   **Track** and **Remote** switched on.
3. On the **CC** tab, build a pattern and pick a **Number** — either the Mix CC in the Output
   column, or a single lane's own Send and Number on its own strip.
4. On the track receiving Ravel's MIDI, set **MIDI To → loopMIDI Port**.
5. Start playback so CC is flowing, press **Ctrl+M**, click the parameter you want to
   modulate, and Live latches onto the incoming CC.

Because each CC lane has its own destination on top of the Mix CC, one instance can drive up to
five mapped parameters. If this instance is only for CC, mute its Note lanes so it stops
emitting notes.

Caveats worth knowing before you rely on it: MIDI mapping is 7-bit, so you get 128
discrete values, and it is control-rate rather than sample-accurate. It is fine for filter
sweeps, sends and macros; it is not a substitute for real modulation. If you want true
parameter modulation with full resolution, that is a Max for Live device — Live 12 Suite
already includes M4L, and its modulation API can target any parameter directly.

---

## Layout

| File | Contents |
|---|---|
| `Source/Parameters.*` | Parameter IDs, choice lists, scale tables, pattern actions |
| `Source/SequencerEngine.*` | The sequencer core and MIDI generation |
| `Source/PluginProcessor.*` | Plugin plumbing, playhead handling, state save/load |
| `Source/PluginEditor.*` | Window layout, header, and the Notes/CC workspaces |
| `Source/LaneComponent.*` | One lane: 16 steps plus its controls, in either kind |
| `Source/Controls.*` | Shared row/column/tab building blocks the editor and lanes are built from |
| `Source/UndoHistory.*` | The edit history behind the arrows and Ctrl+Z |
| `Source/Theme.h` | Colours and custom widget drawing |
| `Tests/EngineTests.cpp` | Engine tests, run as a standalone console app |
| `Tests/ProcessorTests.cpp` | Processor tests, driven through a mock playhead |

Both lane kinds are the same `LaneComponent`, told at construction which `params::LaneKind` it
is; the same goes for the pattern actions and the engine's lane fold. That is what keeps the two
stacks from being two copies of the same code with a Note/CC flag sprinkled through both.

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
