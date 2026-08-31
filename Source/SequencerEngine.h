#pragma once

#include "Parameters.h"

#include <atomic>
#include <cstdint>
#include <limits>

/**
    The sequencer core.

    Step positions are derived from the host's absolute PPQ position rather than
    accumulated from a running counter. That costs a floor() per lane per sample
    but it means loops, jumps, scrubbing and tempo changes all land on exactly the
    step the timeline says they should, with no drift and no resync logic.
*/
class SequencerEngine
{
public:
    //==========================================================================
    /** Per-lane parameter values, read once per block off the audio thread's
        atomics so the inner sample loop touches only plain floats.
    */
    struct LaneSnapshot
    {
        float values[params::numSteps] {};
        bool  enabled[params::numSteps] {};
        float chance[params::numSteps] {};

        /** Per-step accent, as a trim on the global Velocity. 1 is unity. */
        float velocity[params::numSteps] {};

        /** Per-step note length, as a percentage of the step's own length. 100 touches the
            next step without overlapping it; above that overlaps into it (see Voices). */
        float gate[params::numSteps] {};

        /** The lane's mute. False makes every one of its steps behave as if switched off:
            nothing added to the mix, nothing triggered, and its CC latched where it was. */
        bool  active    = true;
        int   length    = params::numSteps;
        int   division  = params::divIndex_1_16;
        int   direction = 0;
        float depth     = 0.0f;
        int   mode      = params::modeAdd;
        float nudge     = 0.0f;
        float humanize  = 0.0f;

        bool  ccOn      = false;
        int   ccNumber  = 20;
        int   ccChannel = 1;

        /** Shifts this lane's own CC output, independent of the other lanes and of the
            global Offset (which only ever touches pitch and the Mix CC). */
        float ccOffset  = 0.0f;
    };

    struct Snapshot
    {
        LaneSnapshot lanes[params::numLanes];

        // Independent -- both can be on together, and both can be off to mute the instance
        // entirely.
        bool  notesOn       = true;
        bool  ccOn          = false;
        int   triggerSource = 0;
        bool  quantize      = true;
        int   bendRange     = 2;
        int   root          = 48;
        int   rangeSteps    = 12;
        int   scale         = 4;
        int   velocity      = 100;
        int   midiChannel   = 1;
        int   ccNumber      = 1;
        int   ccChannel     = 1;
        float offset        = 0.0f;
        float slewMs        = 0.0f;
        float swing         = 0.0f;
        int   voiceCount    = 1;

        /** False: the lanes are mixed into one value that drives one note.
            True:  each lane triggers its own note off its own clock, so the lanes run as
                   independent voices. A lane at zero Depth stays silent, and Trigger is
                   unused because every lane triggers itself.
        */
        bool  polyMode      = false;
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

    //==========================================================================
    // Read by the editor's timer. Plain relaxed atomics: a torn read just means
    // one stale repaint frame.
    int   getCurrentStep (int lane) const noexcept { return uiStep[lane].load (std::memory_order_relaxed); }

private:
    //==========================================================================
    static int stepIndexFor (std::int64_t globalIndex, int length, int direction, int laneIndex) noexcept;

    /** How far this step's boundary moves, as a fraction of a step, from swing + nudge +
        humanize. Clamped to +/-0.49 so boundaries stay monotonically ordered: adjacent
        offsets can then differ by at most 0.98 of a step, which keeps boundary(g+1)
        strictly after boundary(g) and lets the stateless index search below work.
    */
    static float timingOffsetFor (std::int64_t globalIndex, const LaneSnapshot& lane,
                                  float swing, int laneIndex) noexcept;

    /** Resolves the current step index when boundaries have been shifted in time.

        With no offsets this reduces exactly to floor(ppq / stepPpq). With offsets it picks
        the largest candidate whose shifted boundary the timeline has passed, checking only
        the adjacent candidates -- which is sufficient because offsets are bounded to half
        a step.
    */
    static std::int64_t resolveGlobalIndex (double ppq, double stepPpq, const LaneSnapshot& lane,
                                            float swing, int laneIndex) noexcept;

    /** Pitch bend sensitivity (RPN 0) for the note channel. Written out as raw RPN controller
        messages rather than via a JUCE helper, because those return a MidiBuffer by value and
        would allocate on the audio thread.
    */
    void sendPitchBendRange (juce::MidiBuffer& out, int sampleOffset, int channel, int bendRange);

    struct LaneState
    {
        std::int64_t lastGlobalIndex = std::numeric_limits<std::int64_t>::min();
        int   step = 0;
        float held = 0.0f;
    };

    LaneState lanes[params::numLanes];

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

    bool anyVoiceActive() const noexcept;

    /** Counts down each sounding voice and emits note-off as they expire. */
    void advanceVoices (juce::MidiBuffer& out, int sampleOffset);

    /** Picks a slot in [begin, end) for a new note, emitting a note-off first if it has to
        reuse or steal one. Returns the index to fill in.
    */
    int allocateVoice (juce::MidiBuffer& out, int sampleOffset, int note, int channel,
                       int begin, int end);

    /** Whether the current mode and voice count still own a slot. In poly mode a lane's
        block always starts at the same index, so only the offset within it is checked --
        which is what keeps a held note in slot 0 of lane 2 alive when Voices changes. */
    static bool slotIsOwned (int slot, int voiceLimit, bool polyMode) noexcept;

    /** Releases voices in slots the current mode and voice count no longer own. */
    void retireUnownedVoices (juce::MidiBuffer& out, int sampleOffset, int voiceLimit, bool polyMode);

    /** The note, channel and pitch bend a 0..1 value maps to under the current pitch setting. */
    struct PitchResult
    {
        int   note     = 0;
        int   channel  = 1;
        int   bend     = -1;    // -1 means no bend is needed
    };

    static PitchResult pitchFor (float value, const Snapshot& s, int noteChannel, int bendRange) noexcept;

    /** The MIDI velocity for a note fired by a given step: the global Velocity scaled by
        that step's own accent. */
    static int velocityFor (const Snapshot& s, int laneIndex, int stepIndex) noexcept;

    /** That step's own gate length, as a percentage of the step's length. */
    static float gateFor (const Snapshot& s, int laneIndex, int stepIndex) noexcept;

    /** Allocates a slot in [begin, end), emits the bend and note-on, and arms the gate. */
    void startNote (juce::MidiBuffer& out, int sampleOffset, const PitchResult& pitch,
                    int velocity, int gateSamples, int begin, int end);

    float slewedValue = 0.0f;
    int   lastCcValue = -1;
    int   ccCountdown = 0;
    int   ccIntervalSamples = 32;

    // Per-lane CC streams. heldValue latches on inactive steps so a skipped step holds
    // its level rather than dropping to zero.
    float laneHeldValue[params::numLanes] {};
    float laneSlewedValue[params::numLanes] {};
    int   laneLastCcValue[params::numLanes] { -1, -1, -1, -1 };

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

    std::atomic<int>   uiStep[params::numLanes] {};
};
