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
| What the fold drives | Pitch, over an MPE zone (or one channel) | The Mix CC |
| Lanes | 1–4, own count | 1–4, own count |
| Per-step | Value, Velocity, Chance, Gate | Value, Chance |
| Per-lane shaping | Length, Rate, Direction, Mix amount | The same four |
| Per-lane output | — | Its own Send / Number / Channel / Offset |
| Swing | Shared — one control, on the Notes page | Shared — driven by the Notes page |

The two differ per *step*, not per lane. Velocity and Gate are only ever arguments to *start a
note*, and a CC lane never starts one, so it has neither — which also keeps the plugin's
automatable parameter count from doubling for nothing. Everything a lane does as a lane —
how long it is, how fast it runs, which way it traverses, how much it contributes to its
stack's fold — is the same on both, because both are the same sequencer with a different
destination on the end of it.

### Per lane

| Control | Range | Notes |
|---|---|---|
| 16 step bars | 0–100 % | The lane's values |
| 16 step toggles | on/off | Hard mute for a step |
| Lane toggle | on/off | Mutes the whole lane: transparent for the mix, triggers nothing |
| Length | 1–16 | Shorter lanes phase against longer ones. Steps past the length grey out, and stay editable |
| Rate | 1/1 … 1/32, incl. triplets | Independent per lane — this is where the polyrhythm comes from |
| Mix amount | −100 % … +100 % | How much this lane contributes to its stack's fold. Signed, and drawn filling out from a marked centre |
| 16 velocity bars | 0–100 % | *Note lanes only.* Per-step accent, as a trim on the fixed master velocity of 100 (100 % is unity, so a bar only ever pulls a step below it) |
| 16 gate bars | 5–200 % | *Note lanes only.* How long each step's note is held, as % of the step. Above 100 % overlaps into the next step (see Polyphony) |
| 16 chance bars | 0–100 % | Per-step probability of firing |
| Direction | Forward, Reverse, Ping-Pong, Random | How the lane traverses its steps |
| RND / CLR / ⋯ | — | Pattern actions. RND, CLR and Invert act on the **selected row** |
| ✕ | — | Takes this lane out. The lanes below it move up to close the gap |

On a **Note lane** the sixteen tall bars edit one of four per-step rows at a time, picked with
the **Value / Velocity / Prob / Gate** selector down the left of the lane. Only the selected row
is drawn — the bars show one layer at a time and nothing else, so a lane reads as the pattern
you are actually editing.

Switching rows **slides** the bars from the heights they were at to the heights they are going
to, over about 140 ms, eased out. Sixteen bars changing height at once otherwise reads as the
grid being replaced; sliding them says the pattern stayed where it was and you changed which of
its rows you are looking at.

The same slide runs for every **pattern action that rewrites the row** — RND, CLR, Rotate,
Invert and Paste — for the same reason, and it is the same code: the slide reads its destination
live rather than capturing it, so it does not care whether the bars moved because a different
slider is being read or because that slider's value changed. Copy is the one entry that does not
slide, because it changes nothing on screen.

The slide only ever borrows the height a bar draws to — it never touches the parameter, so
nothing reaches the host or the undo history, and a pattern action is still exactly one undo
step.

A **CC lane** gets the same selector with one chip in it: **Value**, latched, because that is
the only layer its bars have. Velocity and Gate are only ever arguments to *start a note* and a
CC lane never starts one, so there is genuinely nothing else to offer — but the column stays and
the chip stays, so the CC tab's step grid lines up with the Notes tab's and reads as the same
grid with fewer layers behind it rather than as a different kind of control.

A CC lane's per-step **Chance** is the one parameter with no control of its own. It still works —
it decides whether a step reaches the fold, and the fold is what the CC output follows — but it
is reachable only through host automation.

Everything else on a CC lane's strip is what a Note lane has: Length, Rate, **Direction** and Mix
amount. Its own **Send / Number / Channel / Offset** are not there, because they are a destination
rather than a pattern — set once and then left — so they live in the CC tab's footer, one column
per lane. See [CC outputs](#cc-outputs).

### Probability

Each step has a **Chance**. A step that loses its roll behaves exactly like a step that's
switched off: transparent for the mix, and it fires nothing. Chance 100 % always fires,
0 % never does.

The roll is a hash of the timeline position, not a draw from a running RNG — so it holds
steady for the whole step, and **a loop skips exactly the same steps every time round**
rather than drifting. Same design as Random direction, for the same reason.

### Swing

**Swing** delays every other step of the absolute grid, so it stays anchored to the bar rather
than to wherever a short pattern happened to start. It is **one control across both stacks**,
living on the Notes page: they fold off the same host clock, and a Note lane swung against a CC
lane reads as drift rather than as groove. Same shared-switch reasoning as Free Run.

It moves step *boundaries*, which meant reworking how the step index is derived. It's still
stateless — rather than `floor(ppq / stepPpq)`, it picks the largest candidate index whose
*shifted* boundary the timeline has passed, checking only the adjacent candidates. That's
sufficient because the offset is clamped to ±0.49 of a step, which also guarantees boundaries
stay monotonically ordered. At zero swing it reduces exactly to the old `floor()`, and there's
a test asserting that.

### CC outputs

There are two kinds, and they are independent:

- **The Mix CC** (CC tab → *Mix CC*) is the CC stack's fold, exactly as pitch is the Note
  stack's fold. Its **Send** switch, **Number**, **Channel** and **Offset** are the first column
  of the CC tab's footer.
- **Each CC lane's own tap** follows that lane's raw step value and **ignores Mix amount**, since
  Mix amount governs the lane's share of the fold, not its own output. Its Send, Number, Channel
  and Offset are that lane's own column in the same footer, headed *Lane 1* … *Lane 4* and marked
  with the lane's accent. Defaults are CC 20, 21, 22 and 23 for lanes 1–4.

A column for a lane the instance does not currently have is greyed rather than taken away: all
four lanes' parameters exist from the moment the plugin loads, and a column that vanished and
reappeared as the lane count changed would shuffle everything to its right each time.

The two Offsets never cross: the CC tab's Offset shifts the Mix CC, and a lane's Offset shifts
only that lane's tap, so one lane can be recentred without moving the rest. Inactive steps latch
the previous level rather than dropping to zero. All CC streams share the global **Slew**.

### Pattern actions

**RND** re-rolls a row and **CLR** resets one — and the row they act on is whichever the lane's
bars are currently showing. With Prob selected, RND re-rolls the probabilities; with Gate
selected, the gates. Anything else would be a button that appears to do nothing whenever you are
not on Value. Invert, in the **⋯** menu, follows the selection the same way, and names the row it
is about to mirror.

Each acts across that row's *own* range, not over 0–1: Gate runs 5–200, so randomising it spreads
over 5–200 and inverting it mirrors about 102.5 rather than about 0.5.

**CLR resets rather than zeroes.** Only Value clears to zero. Velocity, Prob and Gate are trims
on something that already works — unity, always-fires, and a normal note length — so zeroing them
gives silent notes, a lane that never fires, and zero-length notes, which are three ways of
switching the lane off rather than of clearing it. Each goes back to its own neutral (1, 1 and
60 %), which is also what double-clicking one of its bars resets to; the two read from the same
table so they cannot drift apart.

The step toggles are never touched by any of these, so a lane's rhythm survives a re-roll. Rotate
and Paste are the exception to the whole selected-row rule: they move value, on/off and chance
together — and, on a Note lane, velocity and gate as well — because rotating only one row would
slide it out from under the rest of the pattern.

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

### Presets

The pill beside the history arrows holds the loaded preset. Click the name to open the
browser, or step through the list with the chevrons either side of it — that pair is meant for
auditioning, so you can spin through a folder of patches without going back to the menu each
time.

| | |
|---|---|
| **Save** | Overwrites the loaded preset. With nothing loaded it asks for a name instead, so it can never overwrite something you didn't mean to |
| **Init** | Every parameter back to its default. One undo step, like any other edit |
| **Save as / Rename / Delete** | In the name menu. Delete asks first — it's the only preset action undo cannot walk back |
| **Show presets folder** | Opens `Documents\Ravel\Presets` in Explorer |

A dot after the name means the patch has been edited since it was loaded. Reloading the same
preset from the menu is how you throw that edit away.

Presets live as `.ravelpreset` XML files in `Documents\Ravel\Presets`. Subfolders show up as
submenus, so you can organise them in Explorer and the plugin follows. A preset is **only the
patch** — it deliberately leaves the MIDI output device and the window size alone, since those
describe your machine rather than the sound.

Loading a preset is an ordinary edit: it writes the parameters the same way the UI does, which
means one **Ctrl+Z** takes the whole thing back, and the host sees it as a real change rather
than a state swap behind its back. Which preset a patch came from is remembered with the Live
set, so reopening a session shows its name rather than "Init".

Files are keyed by parameter ID and hold plain values, so presets survive the plugin gaining
parameters later: an ID a build doesn't recognise is ignored, and a parameter the file doesn't
mention loads at its default.

### The fold

Lanes are combined in lane order, starting from zero: each active step adds its own share,
`mix += depth × value`. Both stacks fold the same way.

A step that is toggled **off** is transparent for its lane — nothing is added — and it fires
no note if that lane is the trigger source.

### Lanes

Every lane starts flat — sixteen steps of one value, not a demo to clear away — and lanes differ
only in their default rate. The **first Note lane** is the one exception: its steps come up at
25 % so a freshly loaded instance is audibly doing something rather than looking broken. Every
lane after it, and every CC lane, starts at zero, because a lane you just added is a blank sheet
to draw on and one that arrives already pitched is something to clear away first. *+ Add lane* sits under the last lane of the
current stack and appends one at the bottom; **each lane carries its own Remove**, at the right
of its action row, so any lane can go and not just the last one. The window grows and shrinks to
fit the lane count on its own; **it's also resizable by hand**, from the bottom-right corner or
the host's own window border. Dragging it doesn't reflow the layout, it zooms the whole thing
uniformly, between 60 % and 150 % of native size, so bars and text scale together rather than
the step area alone stretching. The size is remembered per session, the same way the pattern is.

Removing a lane closes the gap behind it: every lane below the removed one moves up a slot,
bringing its own controls with it — rate, depth, direction, its CC destination, not only
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
once, so a muted lane is transparent for its fold and triggers nothing. That is also how you
make an instance CC-only: mute its Note lanes.

All four lanes of both stacks exist as parameters from the moment the plugin is loaded, because
a VST3 cannot add parameters later. The two lane counts only decide which of them are heard and
shown, which is what makes them automatable and undoable like any other control.

### The tabs

The header runs left to right from what the plugin *is* to where its output goes: the mark and
wordmark, the two history arrows, then the **preset pill** — steppers either side of the loaded
patch's name, with **Save** and **Init** beside it — and, hard against the right edge, the **MIDI
output** chooser and **Rescan**. The MIDI output routes both stacks alike and so belongs to
neither tab (see [MPE into Live](#mpe-into-live-the-virtual-port-route)).

Everything else global sits under whichever of the two top-level tabs it belongs to, in the
footer below that tab's lane stack, laid out as a row of headed columns:

**Notes**

| Column | Controls |
|---|---|
| Pitch | Root, Scale, Range, Quantize |
| Output | Bend range, Offset, MPE, Channel |
| Voice | Voices, Poly |
| Clock | Swing, Free run, Trigger |

**CC**

| Column | Controls |
|---|---|
| Mix CC | Send, Number, Channel, Offset, Slew |
| Lane 1 … Lane 4 | Send, Number, Channel, Offset — that lane's own tap |

**Slew** is not strictly the Mix CC's — it smooths every CC this plugin sends, the lane taps
included — but it sits in that column anyway, as an ordinary fifth row. It is the only global CC
control there is, and a rule and a sub-heading to mark the distinction cost more attention than
the distinction is worth; the tooltip carries it instead.

Swing is not repeated here — it is shared, and lives on the Notes page.

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
**Root**. The default Root of 24 (C0) leaves 103 semitones of headroom above it, enough for a
Range of 8 octaves before the top flattens out; raising Root buys that headroom back at the
bottom. Note **Offset** transposes on top of this and is clamped by the same ceiling.

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

**Quantize** (off by default) is the pitch mode switch:

- **On** — the mixed value snaps to the nearest degree of the selected **Scale**.
- **Off** — continuous, unquantized pitch. The **Scale** setting has no effect at all, and
  Range is read as 12 semitones per octave:

  ```
  pitch = Root + mix × Range × 12      (semitones)
  ```

Either way, pitch goes out as a note plus pitch bend — the nearest semitone carries the note
number, and the residual — never more than half a semitone — goes out as pitch bend, sent just
before the note-on so the note starts already in tune. With **MPE** on (the default) each
note gets its own member channel, so the bend is the note's alone; with it off, everything
goes out on the single **Channel** below it and shares that one wheel (see [MPE](#mpe)).
With Quantize on and a 12-EDO scale the residual is always exactly zero,
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

With MPE on — the default — overlapping notes each hold their own microtone, because each
one is on its own channel. With it off they share one channel and one bend register, so the
most recent bend applies to all of them. See below.

### MPE

**MPE** (Notes tab → Output) is **on by default**. Output is then a standard **MPE Lower
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
listening on one channel will hear only the notes that land there. **Channel**, directly under
the switch, is the single-channel fallback for those instruments: it is greyed out while MPE
is on, since the zone fixes its own channels, and with MPE off it selects the one channel
every note and every pitch bend goes out on. That costs poly microtonality — one channel, one
wheel — which is why the default is the zone.

Getting that zone into an instrument inside Live takes a virtual MIDI port rather than Live's
own routing, for reasons that are Live's rather than Ravel's — see
[MPE into Live](#mpe-into-live-the-virtual-port-route).

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

255 checks across two suites, neither needing a plugin host.

`Tests/EngineTests.cpp` (118 checks) drives `SequencerEngine` over a synthetic timeline. The
engine takes PPQ positions as plain arguments rather than reading a playhead itself, which is
what makes that possible. Covers step timing, gate length, per-lane length and rate, disabled
steps, the fold, transport jumps, stuck-note release on stop, directions, probability,
swing, per-step velocity, polyphony and poly mode, the Mix CC and each lane's own CC
tap (including that the two Offsets stay out of each other's way), and the continuous-pitch
path — including that note number plus pitch bend reconstructs the intended fractional pitch,
that non-12 EDO scales land where the tuning says, and that the bend range is actually
transmitted.

`Tests/ProcessorTests.cpp` (137 checks) drives the real `RavelAudioProcessor::processBlock`
through a mock playhead. This covers the layer where the plugin could compile, load and still
emit nothing: playhead handling, the free-run fallback, the parameter snapshot, state
round-trip, every pattern action — including that RND, CLR and Invert act on the selected row,
across that row's own range, and skip a row the lane kind does not have — lane add/remove and its
undo behaviour, and the MIDI capability flags a host reads to decide whether to offer the plugin
as a MIDI source.

Worth keeping: these tests caught a real bug. Step boundaries were landing one sample late
at some positions, because `ppqPerSample` is `1/24000` at 120 bpm / 48 kHz — not exactly
representable in binary — so `floor(ppq / stepLength)` returned the previous step and step
lengths alternated between 5999 and 6001 samples. Fixed with a boundary epsilon in
`SequencerEngine::process`, sized ~1000× smaller than one sample's worth of PPQ so it can
only ever snap a value already inside rounding noise.

### Looking at the UI

The editor is a `juce::Component`, and a Component can paint itself into an image without ever
reaching a desktop window — so the whole window can be rendered to a PNG from a build step
rather than by loading the VST3 into a DAW and taking a screenshot by hand. Layout constants are
the kind of thing that stays wrong by four pixels until someone actually looks at it.

Off unless asked for, because it builds a second copy of the editor:

```powershell
cmake -S . -B build -DRAVEL_SNAPSHOT_SOURCE=Tests/Snapshot.cpp
cmake --build build --target RavelSnapshot --config Debug
.\build\RavelSnapshot_artefacts\Debug\RavelSnapshot.exe out.png notes 3
```

A fourth argument — the label of a chip on the lane strips, so `Val`, `Vel`, `Prob`, `Gate`,
`RND` or `CLR` — clicks that chip on every lane and writes one numbered frame per sample point
across the slide that follows, so the animation can be reviewed from stills. Build the tool `--config Release` for that: a Debug paint of the window
costs more wall clock than the slide lasts, so every frame would show it already finished.

The first three arguments are the output file, which tab (`notes` or `cc`) and how many lanes. It dials in a
fixed patch first — odd lane lengths, a muted lane, some steps switched off — because at its
defaults the window shows none of the states worth checking: no wrap marker, no out-of-cycle
steps, no negative Mix amount.

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

That is the simple case, and it is enough as long as every note can share one channel. It is
**not** enough for MPE: this path collapses the zone. If you are running a non-12-EDO scale,
Quantize off, or anything polyphonic that leans on per-note bend, use the route below instead.

### MPE into Live: the virtual port route

With MPE on, Ravel's output is an MPE Lower Zone — one member channel per sounding note, each
with its own pitch bend (see [MPE](#mpe)). Live discards that on the way in, twice over:

- **Track-to-track routing collapses channels.** `MIDI From → 1-Ravel` hands the receiving
  track the notes with their channel stripped, so fifteen member channels arrive as one. Every
  note then shares a single pitch-bend register and the most recent bend wins — two overlapping
  notes cannot hold different microtones, which is the exact problem the zone exists to solve.
- **Live's own external MIDI output collapses them too.** `MIDI To → loopMIDI Port` makes you
  pick one channel (`Ch. 1`, `Ch. 2` …), so leaving Live for a virtual port is the same
  flattening one step later.

So Ravel writes to the port itself. The **MIDI output** dropdown at the top right of the header
opens a system MIDI port from inside the plugin and sends every note, CC and pitch bend straight
to it, channels intact. Those messages never enter Live's MIDI graph at all, so nothing about
Live's routing can touch them.

It **mirrors** rather than moves: the host still receives exactly the same events on the
plugin's own output as it always did. Picking a port adds a second destination, it does not
redirect the first — which is why the last step below is leaving the Ravel track's **MIDI To**
disconnected. That, rather than anything the plugin decides on its own, is what makes the
virtual port the only path a note takes.

#### Windows — loopMIDI

1. Install [loopMIDI](https://www.tobias-erichsen.de/software/loopmidi.html) and create a port.
   Name it something you will recognise in a dropdown — `Ravel` will do.
2. In Ravel's header, set **MIDI output → Ravel**. Ports are enumerated when the window opens,
   so if you created it after that, hit **Rescan** first.
3. Live **Preferences → Link/Tempo/MIDI**: find that port in the **MIDI Ports** list under
   **Input** and switch on **Track** and **MPE**. The MPE toggle is the one that matters — it
   is what tells Live to read channels 2–16 as a single zone instead of as fifteen unrelated
   channels. Leave **Remote** off, or those notes will start MIDI-mapping things.
4. On the instrument track: **MIDI From → Ravel**, the channel selector beneath it on **All
   Channels**, **Monitor** on **In**.
5. Load an MPE-capable instrument and turn its own MPE on — Live's Wavetable, Sampler, Drift
   and Meld each have an MPE tab; third-party plugins have their own switch.
6. Back on the Ravel track, leave **MIDI To** on **None**. If it is still pointed at the
   instrument track, every note arrives twice — once with its channel, once without.

#### macOS — the IAC Driver

macOS ships loopMIDI's equivalent under a worse name; there is nothing to install.

1. Open **Audio MIDI Setup**, then **Window → Show MIDI Studio**, and double-click **IAC
   Driver**.
2. Tick **Device is online**. `Bus 1` exists by default; add a port if the list is empty.
3. From there, steps 2–6 above are identical — read `IAC Driver Bus 1` wherever they say
   `Ravel`.

#### What to watch for

- **Do not close the loop.** The Ravel track's **MIDI From** must not be that port, and the
  instrument track's **MIDI To** must not be either.
- **Timing is block-granular, not sample-accurate.** Events leave for the port as the audio
  block that produced them is processed, and their sample offset within that block is dropped,
  so a note can land up to one buffer away from where the host's own copy of it lands — under
  3 ms at 128 samples / 48 kHz. Live's input port adds its own latency on top. Fine for playing
  parts; the host output stays the sample-accurate path.
- **The choice is saved with the session, by port.** Open that session on a machine where the
  port does not exist and Ravel quietly stays on host output rather than failing.
- **One port per instance.** Two instances writing to the same port both allocate out of the
  same 15 member channels without knowing about each other, and will steal each other's notes.
  Give each its own port.

### Modulating Live's own parameters

Worth being upfront: **a VST3 cannot reach into Live and drive another device's knob.**
There is no such mechanism in the plugin format. What works is a MIDI CC loopback:

1. Create a virtual port — loopMIDI on Windows, an IAC Driver bus on macOS, both set up
   [above](#mpe-into-live-the-virtual-port-route).
2. **Preferences → Link/Tempo/MIDI**: enable that port as an **Input**, with both
   **Track** and **Remote** switched on. **MPE** stays off here — this is plain single-channel
   CC.
3. On the **CC** tab, build a pattern and pick a **Number** — either the Mix CC in the Output
   column, or a single lane's own Send and Number on its own strip.
4. On the track receiving Ravel's MIDI, set **MIDI To → loopMIDI Port**.
5. Start playback so CC is flowing, press **Ctrl+M**, click the parameter you want to
   modulate, and Live latches onto the incoming CC.

Step 4 can equally be Ravel's own **MIDI output** dropdown pointed at that port, which leaves
the track's **MIDI To** free for an instrument. Channel collapse does not matter to CC, so
either path works here; the dropdown is only strictly necessary for notes.

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
| `Source/ParameterTables.h` | Lane and step counts, clock divisions, the scale table, pitch-bend helpers |
| `Source/Parameters.*` | Parameter IDs, the APVTS layout, pattern actions |
| `Source/SequencerEngine.*` | The sequencer core and MIDI generation |
| `Source/PluginProcessor.*` | Plugin plumbing, playhead handling, state save/load |
| `Source/PluginEditor.*` | Window layout, the header, and the Notes/CC workspaces |
| `Source/PresetBar.*` | The header's preset pill, Save and Init: browser menu, name prompt, edited marker |
| `Source/ExternalMidiSelector.*` | The header's MIDI-output chooser: port list and Rescan |
| `Source/LaneComponent.*` | One lane: 16 steps plus its controls, in either kind |
| `Source/Controls.*` | Shared row/column/tab building blocks the editor and lanes are built from |
| `Source/PresetManager.*` | Saving, loading and browsing patches |
| `Source/UndoHistory.*` | The edit history behind the arrows and Ctrl+Z |
| `Source/ExternalMidiOutput.*` | Mirrors output to a system MIDI port, off the audio thread |
| `Source/Theme.h` | Colours, metrics, widget roles and the small drawing helpers |
| `Source/Theme.cpp` | The two embedded Archivo faces, created once and cached |
| `Source/RavelLookAndFeel.*` | Draws every custom widget, dispatching on `theme::roleOf()` |
| `Assets/*.ttf` | Archivo Regular and SemiBold, compiled in by the `RavelFonts` target |
| `Tests/EngineTests.cpp` | Engine tests, run as a standalone console app |
| `Tests/ProcessorTests.cpp` | Processor tests, driven through a mock playhead |
| `Tests/Snapshot.cpp` | Renders the editor to a PNG with no host — see [Looking at the UI](#looking-at-the-ui) |

Both lane kinds are the same `LaneComponent`, told at construction which `params::LaneKind` it
is; the same goes for the pattern actions and the engine's lane fold. That is what keeps the two
stacks from being two copies of the same code with a Note/CC flag sprinkled through both.

### How timing works

Step positions are derived from the host's absolute PPQ position rather than accumulated from
a running counter:

```
globalIndex = floor(ppqPosition / stepLengthInQuarterNotes)
step        = f(globalIndex, length, direction)
```

In exchange for a division and a `floor()`, loops, transport jumps, scrubbing and tempo changes
all land on exactly the step the timeline says they should, with no drift and no resync logic.
It also means **Random** direction is a hash of the timeline position rather than a running RNG
— so a loop replays the same random pattern every time round instead of wandering.

That derivation runs once per step boundary, not once per sample. Everything about a lane —
which step it is on, that step's value, whether it fires — is a function of `globalIndex`, so
all of it holds until the index changes; at 1/16 and 120 bpm that is once every 6000 samples.
The engine inverts the same inequality it would otherwise have tested sample by sample to work
out which sample the next boundary lands on, then confirms that answer against the resolver
itself, so the result is exact rather than approximate — a lane steps on the sample it always
did.
