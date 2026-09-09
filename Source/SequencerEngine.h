#pragma once

#include "ParameterTables.h"

#include <atomic>
#include <cstdint>
#include <limits>
#include <utility>

/**
    The sequencer core.

    Step positions are derived from the host's absolute PPQ position rather than accumulated
    from a running counter, so loops, jumps, scrubbing and tempo changes all land on exactly
    the step the timeline says they should, with no drift and no resync logic.

    Deriving one costs a division and a floor, which is why it happens once per step boundary
    and not once per sample. Everything about a lane -- which step it is on, that step's value,
    whether it fires -- is a function of that index, so all of it holds until the index changes,
    and at 1/16 and 120 bpm that is once every 6000 samples. See nextBoundarySample().
*/
class SequencerEngine
{
public:
    //==========================================================================
    /** What both kinds of lane have: a pattern, and the controls that decide how it is
        traversed. This is everything the step-resolution path reads, which is why it is the
        type that path takes -- a Note lane and a CC lane step identically, they only differ in
        what their value ends up driving.

        Read once per block off the audio thread's atomics, so the sample loop touches only
        plain floats.
    */
    struct LaneSnapshot
    {
        float values[params::numSteps] {};
        bool  enabled[params::numSteps] {};
        float chance[params::numSteps] {};

        /** The lane's mute -- and also how a lane the instance does not have yet is expressed.
            False makes every one of its steps behave as if switched off: nothing added to the
            mix, nothing triggered, and its CC latched where it was. It is also what lets
            buildSnapshot() leave the three arrays above untouched: nothing reads them while
            this is false. */
        bool  active    = true;
        int   length    = params::numSteps;
        int   division  = params::divIndex_1_16;
        int   direction = 0;
        float depth     = 0.0f;
    };

    /** A Note lane: the pattern above, plus the two things only a note has.

        Split from the CC lane rather than one struct carrying both sets of fields with half of
        them inert. That cost 512 bytes of a ~2.5 KB snapshot rebuilt every block on nothing --
        but the reason to fix it is that "a CC lane never reads velocity" was a comment, and is
        now a fact about the type.
    */
    struct NoteLaneSnapshot : LaneSnapshot
    {
        /** Per-step accent, as a trim on the global Velocity. 1 is unity. */
        float velocity[params::numSteps] {};

        /** Per-step note length, as a percentage of the step's own length. 100 touches the
            next step without overlapping it; above that overlaps into it (see Voices). */
        float gate[params::numSteps] {};

        /** How far above its own value this step may land, as a width: 0.3 on a step at 0.30
            plays somewhere in 0.30..0.60. The value is the floor of the range rather than its
            middle, so the bar the editor draws is the bottom of what the step can play and
            the window is the headroom above it. Zero -- the default, and what every step of a
            pattern written before this existed loads with -- is a window with one value in
            it, so it needs no special case anywhere.

            The draw is a hash of the timeline position rather than a running RNG, for the
            same reason Chance and Random direction are (see hashToUnitFloat). Here it is not
            only about a loop repeating: a lane is re-resolved at the start of every block
            whether or not it advanced, so a running RNG would re-roll mid-step and zipper the
            pitch. A pure function of the global index gives the same answer however often it
            is asked, which is what holds a step's pitch still for the step's whole life. */
        float spread[params::numSteps] {};
    };

    /** A CC lane: the pattern above, plus its own destination -- the whole reason a CC lane
        exists, rather than an optional tap on any lane.
    */
    struct CcLaneSnapshot : LaneSnapshot
    {
        bool  ccOn      = false;
        int   ccNumber  = 20;
        int   ccChannel = 1;

        /** Raises the floor of this CC lane's own tap: the step value then spans the range
            left above it, so a step reads as a percentage of the headroom and the tap never
            runs past the top. Independent of the other CC lanes and of the CC tab's own
            Offset (which shifts the Mix CC instead). */
        float ccOffset  = 0.0f;
    };

    struct Snapshot
    {
        NoteLaneSnapshot noteLanes[params::numLanes];
        CcLaneSnapshot   ccLanes[params::numLanes];

        // Which Note lane's advance fires the shared note in mixed (non-poly) mode. CC has
        // no equivalent: its output is never "triggered", it continuously reflects the fold.
        int   noteTriggerSource = 0;

        bool  quantize      = true;
        int   bendRange     = 2;
        int   root          = 48;
        int   rangeOctaves  = 2;
        int   scale         = 4;
        // The master velocity each step's accent scales down from. The plugin always passes
        // params::fixedVelocity -- there is no parameter behind this any more. It stays a
        // field so the engine's own tests can still prove the accent scales against it.
        int   velocity      = 100;
        int   midiChannel   = 1;

        // The CC tab's own Mix destination, fed by the CC-lane fold. Off silences the Mix CC
        // entirely -- each CC lane's own Send is unaffected.
        bool  ccOn          = true;
        int   ccNumber      = 1;
        int   ccChannel     = 1;

        // Transposes the resolved pitch by whole octaves. Applied in pitchFor(), after the
        // fold and after the scale, so it never squashes the pattern against the mix clamp.
        int   noteOctaves   = 0;

        // Shifts the CC-lane mix before it becomes the Mix CC. Independent of any CC
        // lane's own ccOffset, which shifts that lane's own tap instead.
        float ccOffset      = 0.0f;

        // Smooths the Mix CC and every CC lane's own tap. Never touches pitch.
        float slewMs        = 0.0f;

        // One Swing for both pools: they run off the same host clock, and swinging a Note
        // lane against a CC lane reads as drift rather than as groove.
        float swing         = 0.0f;

        int   voiceCount    = 1;

        /** False: the lanes are mixed into one value that drives one note.
            True:  each lane triggers its own note off its own clock, so the lanes run as
                   independent voices. A lane at zero Depth stays silent, and Trigger is
                   unused because every lane triggers itself.
        */
        bool  polyMode      = false;

        /** True: every simultaneously-sounding note gets its own MIDI channel (a standard
            MPE Lower Zone) instead of sharing the Note Channel's one pitch wheel. Off
            reproduces the single-channel behaviour that predated it, where overlapping notes
            share one wheel and so cannot hold different microtones -- midiChannel is read
            only on that path.

            The parameter behind it defaults to on; this field defaults to off so the
            engine's own tests start from the simpler allocation path and opt in.
        */
        bool  mpeEnabled    = false;
    };

    //==========================================================================
    void prepare (double sampleRate);
    void reset();

    /** Generates MIDI for one block.

        @param ppqAtBlockStart  absolute quarter-note position of sample 0
        @param ppqPerSample     how far the timeline advances per sample
        @param transportRunning false releases any held note and stops stepping
    */
    void process (const Snapshot& snapshot,
                  juce::MidiBuffer& out,
                  int numSamples,
                  double ppqAtBlockStart,
                  double ppqPerSample,
                  bool transportRunning);

    /** Releases every sounding note immediately -- used on transport stop and reset. */
    void releaseAllVoices (juce::MidiBuffer& out, int sampleOffset);

    /** Voice slots each lane gets to itself in poly mode. This is the ceiling on Voices,
        which is why the pool is a fixed block per lane rather than a shared free list:
        a lane holding a long gate can then never have its note stolen by a faster lane.
    */
    static constexpr int voicesPerLane = 8;
    static constexpr int maxVoices     = params::numLanes * voicesPerLane;

    // Standard MPE Lower Zone: channel 1 is the master, channels 2-16 are the 15 member
    // channels each simultaneous note gets its own pitch bend on. Fixed, not user-configurable
    // in v1 -- keeping the zone shape constant is what keeps the channel pool a plain
    // fixed-size array instead of something that has to be resized on a parameter change.
    static constexpr int mpeMasterChannel     = 1;
    static constexpr int mpeMemberChannelBase = 2;
    static constexpr int mpeMemberChannels    = 15;

    //==========================================================================
    // Read by the editor's timer. Plain relaxed atomics: a torn read just means
    // one stale repaint frame.
    /** Hands the editor's timer the step each lane is sitting on, or -1 on every lane while
        nothing is playing.

        Called on the way out of every path through process(), the stopped one included --
        which used to return before reaching the store, leaving the playhead marker parked on
        whatever step the transport happened to halt on as though the sequencer were still
        running there.
    */
    void publishUiSteps (bool running) noexcept;

    /** -1 means this lane is not playing a step right now. */
    static constexpr int noStep = -1;

    int getCurrentStep (int lane, params::LaneKind kind = params::LaneKind::note) const noexcept
    {
        return (kind == params::LaneKind::cc ? ccUiStep[lane] : noteUiStep[lane])
                 .load (std::memory_order_relaxed);
    }

    /** Where inside its Spread window a note lane's current step actually landed, 0..1, or
        -1 while nothing is playing.

        Only worth drawing on a step that has a Spread: at zero width this is the step's own
        value, which the bar is already showing. It exists so a window can be read as a
        window -- a band with no mark in it says a step might land anywhere in a range, and
        says nothing about where it just did. */
    float getCurrentValue (int lane) const noexcept
    {
        return noteUiValue[lane].load (std::memory_order_relaxed);
    }

    /** -1 means this lane is not sounding a value right now. */
    static constexpr float noValue = -1.0f;

private:
    //==========================================================================
    static int stepIndexFor (std::int64_t globalIndex, int length, int direction, int laneIndex) noexcept;

    /** How far this step's boundary moves, as a fraction of a step, from swing. Clamped to
        +/-0.49 so boundaries stay monotonically ordered: adjacent offsets can then differ by
        at most 0.98 of a step, which keeps boundary(g+1) strictly after boundary(g) and lets
        the stateless index search below work. Swing is the only thing that shifts a boundary,
        so this depends on the grid position and the swing amount, not on the lane.
    */
    static float timingOffsetFor (std::int64_t globalIndex, float swing) noexcept;

    /** Resolves the current step index when boundaries have been shifted in time.

        With no swing this reduces exactly to floor(ppq / stepPpq). With swing it picks the
        largest candidate whose shifted boundary the timeline has passed, checking only the
        adjacent candidates -- which is sufficient because offsets are bounded to half a step.
    */
    static std::int64_t resolveGlobalIndex (double ppq, double stepPpq, float swing) noexcept;

    /** The first sample offset after `from` at which resolveGlobalIndex() stops returning
        `currentIndex`, or numSamples if it does not change again inside this block.

        This is what lets the sample loop stop asking. A lane's step, its value and whether it
        fires are all functions of the global index, so they are constant between boundaries --
        and at 1/16 and 120 bpm a boundary is 6000 samples apart. Resolving the index once per
        boundary instead of once per sample is the same answer, arrived at a few thousand times
        less often.

        Exact, not approximate: it inverts the same inequality resolveGlobalIndex() tests, then
        confirms the result against resolveGlobalIndex() itself, so a lane can never step on a
        different sample than it used to.
    */
    static int nextBoundarySample (std::int64_t currentIndex,
                                   double ppqAtBlockStart, double ppqPerSample,
                                   double stepPpq, float swing,
                                   int from, int numSamples) noexcept;

    /** Pitch bend sensitivity (RPN 0) for the note channel. Written out as raw RPN controller
        messages rather than via a JUCE helper, because those return a MidiBuffer by value and
        would allocate on the audio thread.
    */
    void sendPitchBendRange (juce::MidiBuffer& out, int sampleOffset, int channel, int bendRange);

    struct LaneState
    {
        std::int64_t lastGlobalIndex = std::numeric_limits<std::int64_t>::min();
        int step = 0;

        /** What the step resolved to after any Spread draw -- the step's own value wherever
            there is no Spread, and noValue on a lane that is muted or that the instance does
            not have. Held so the editor can be shown where the current step landed inside its
            window; the engine reads it off the run rather than from here. */
        float value = noValue;
    };

    LaneState noteLaneStates[params::numLanes];
    LaneState ccLaneStates[params::numLanes];

    double currentSampleRate = 44100.0;

    /** One sounding note. A voice list rather than a single held note is what lets a Gate
        above 100% overlap into the following step instead of cutting itself off.
    */
    struct Voice
    {
        int note    = -1;          // -1 when free
        int channel = 1;
        int samplesRemaining = 0;
    };

    Voice voices[maxVoices];

    /** One MPE member channel's own bookkeeping. Decoupled from the voice-slot pool above on
        purpose: up to maxVoices (32) slots can be logically active in poly mode, but only
        mpeMemberChannels (15) physical channels exist, so which slot currently owns a
        channel, and whether that channel's bend range has been announced, are tracked and
        stolen independently of slot stealing.
    */
    struct MpeChannelSlot
    {
        int  voiceSlot = -1;      // index into voices[], or -1 when this channel is free
        bool rangeSent = false;   // has RPN 0 gone out on this channel since it was last primed
    };

    MpeChannelSlot mpeChannels[mpeMemberChannels];

    bool anyVoiceActive() const noexcept;

    /** Counts `samples` off every sounding voice and emits note-off for those that run out.

        Takes a span rather than always stepping by one because the sample loop no longer
        calls it on every sample. Walking all 32 slots 48000 times a second, almost always to
        find that none of them had expired, was pure overhead: the loop now counts down to the
        soonest expiry and only comes here when it actually arrives. Passing 1 reproduces the
        per-sample behaviour exactly.
    */
    void advanceVoices (juce::MidiBuffer& out, int sampleOffset, int samples);

    /** The fewest samples any sounding voice has left, or int max when none is sounding --
        which the caller reads as "no note-off is due inside this block". */
    int soonestVoiceExpiry() const noexcept;

    /** Applies deferred countdown to every sounding voice without retiring any.

        Called before a note is started and once at the end of a block, so the slots are back
        on the caller's own clock: a fresh voice's gate is counted from the sample it starts
        on, and the allocator picks which voice to steal by how much gate each has left. Only
        ever called while the deferred span is shorter than the soonest expiry, so nothing it
        touches can already have run out.
    */
    void settleVoices (int samples) noexcept;

    /** The single place a voice actually goes off: note-off, freeing the slot, and -- if the
        slot was sounding on an MPE member channel -- freeing that channel back to the pool
        too. Every voice-retiring path funnels through here so the pool can never go stale
        relative to voices[].
    */
    void releaseVoice (juce::MidiBuffer& out, int sampleOffset, int slot);

    /** Picks a slot in [begin, end) for a new note, emitting a note-off first if it has to
        reuse or steal one. Returns the index to fill in.
    */
    int allocateVoice (juce::MidiBuffer& out, int sampleOffset, int note, int channel,
                       int begin, int end);

    /** Picks a free member channel for a new note-on, stealing the one closest to finishing
        (same "least audible loss" rule allocateVoice() uses for slots) if all 15 are already
        sounding. Returns an index 0..mpeMemberChannels-1; the caller still has to record
        which voice slot ends up owning it.
    */
    int allocateMpeChannel (juce::MidiBuffer& out, int sampleOffset);

    /** Whether the current mode and voice count still own a slot. In poly mode a lane's
        block always starts at the same index, so only the offset within it is checked --
        which is what keeps a held note in slot 0 of lane 2 alive when Voices changes. */
    static bool slotIsOwned (int slot, int voiceLimit, bool polyMode) noexcept;

    /** Releases voices in slots the current mode and voice count no longer own. */
    void retireUnownedVoices (juce::MidiBuffer& out, int sampleOffset, int voiceLimit, bool polyMode);

    /** The note and pitch bend a 0..1 value maps to under the current pitch setting. Channel
        is resolved separately, by startNote() -- it is no longer a function of pitch mapping,
        since MPE picks it from the member-channel pool rather than the fixed note channel.
    */
    struct PitchResult
    {
        int   note     = 0;
        int   bend     = -1;    // -1 means no bend is needed
    };

    static PitchResult pitchFor (float value, const Snapshot& s, int bendRange) noexcept;

    /** The MIDI velocity for a note fired by a given step: the global Velocity scaled by
        that step's own accent. */
    static int velocityFor (const Snapshot& s, int laneIndex, int stepIndex) noexcept;

    /** That step's own gate length, as a percentage of the step's length. */
    static float gateFor (const Snapshot& s, int laneIndex, int stepIndex) noexcept;

    /** Resolves the channel (fixed, or a freshly pooled MPE member channel), allocates a slot
        in [begin, end), emits the bend and note-on, and arms the gate.
    */
    void startNote (juce::MidiBuffer& out, int sampleOffset, const PitchResult& pitch,
                    int velocity, int gateSamples, int begin, int end,
                    bool mpeOn, int fixedChannel, int bendRange);

    float slewedValue = 0.0f;
    int   lastCcValue = -1;
    int   ccCountdown = 0;
    int   ccIntervalSamples = 32;

    // Per-CC-lane tap streams. heldValue latches on inactive steps so a skipped step holds
    // its level rather than dropping to zero.
    float ccLaneHeldValue[params::numLanes] {};
    float ccLaneSlewedValue[params::numLanes] {};
    int   ccLaneLastCcValue[params::numLanes] { -1, -1, -1, -1 };

    // What the receiving instrument has already been told, so the RPNs are re-sent only
    // when the mode, range or target channel actually changes.
    // -1 until the first block, then 0 while pitch rides on the wheel (continuous mode, or a
    // quantized scale in an EDO other than 12) and 1 while it does not.
    int   configuredMode      = -1;
    int   configuredBendRange = -1;
    int   configuredChannel   = -1;

    // Slot ownership differs between the two modes, so anything still sounding when the
    // switch is flipped is released rather than left for the other mode to inherit.
    int   configuredPolyMode  = -1;

    // Mirrors configuredMode/configuredBendRange/configuredChannel above, but for the MPE
    // path: RPN 6 addresses the whole zone from the master channel, and RPN 0 has to reach
    // each member channel individually rather than one shared note channel. Kept fully
    // separate from the trio above so the non-MPE path's behaviour is untouched, byte for
    // byte, whenever MPE is off.
    bool  configuredMpeOn         = false;   // has RPN 6 gone out for the current "on" stretch
    int   configuredMpeWantedMode = -1;      // 0 while pitch rides the wheel, 1 while it does not
    int   configuredMpeBendRange  = -1;

    // Channel scheme differs between MPE on/off, same reasoning as configuredPolyMode above:
    // a flip mid-performance releases everything rather than leave it addressed under the
    // scheme that just left.
    int   configuredMpeFlag       = -1;

    std::atomic<int>   noteUiStep[params::numLanes] {};
    std::atomic<int>   ccUiStep[params::numLanes] {};
    // Spelled out rather than value-initialised, because zero is a real position inside a
    // window and this has to start at "nothing landed here yet": publishUiSteps only runs
    // from process(), so an editor opened before the first block would otherwise be told
    // every lane was sitting at the bottom of its range.
    std::atomic<float> noteUiValue[params::numLanes] { -1.0f, -1.0f, -1.0f, -1.0f };
};
