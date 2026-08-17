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
    };

    struct Snapshot
    {
        LaneSnapshot lanes[params::numLanes];

        int   outputMode    = params::outNotes;
        int   triggerSource = 0;
        int   pitchMode     = params::pitchSemitone;
        int   bendRange     = 2;
        int   root          = 48;
        int   rangeSteps    = 12;
        int   scale         = 4;
        int   velocity      = 100;
        float gatePercent   = 60.0f;
        int   midiChannel   = 1;
        int   ccNumber      = 1;
        int   ccChannel     = 1;
        float offset        = 0.0f;
        float slewMs        = 0.0f;
        float swing         = 0.0f;
        int   voiceCount    = 1;
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

    static constexpr int maxVoices = 8;

    //==========================================================================
    // Read by the editor's timer. Plain relaxed atomics: a torn read just means
    // one stale repaint frame.
    int   getCurrentStep (int lane) const noexcept { return uiStep[lane].load (std::memory_order_relaxed); }
    float getMixValue()             const noexcept { return uiMix.load (std::memory_order_relaxed); }

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

    /** Emits the MPE Configuration Message and per-note bend range. Written out as raw
        RPN controller messages rather than via juce::MPEMessages, because that returns a
        MidiBuffer by value and would allocate on the audio thread.
    */
    void sendMpeConfiguration (juce::MidiBuffer& out, int sampleOffset, int bendRange);
    void clearMpeZone (juce::MidiBuffer& out, int sampleOffset);

    /** Pitch bend sensitivity (RPN 0) for a single channel -- the non-MPE equivalent. */
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

    /** Picks a slot for a new note, emitting a note-off first if it has to reuse or steal
        one. Returns the index to fill in.
    */
    int allocateVoice (juce::MidiBuffer& out, int sampleOffset, int note, int channel, int voiceLimit);

    /** Retires voices that fall outside a reduced voice count. */
    void retireVoicesAbove (juce::MidiBuffer& out, int sampleOffset, int voiceLimit);

    float slewedValue = 0.0f;
    int   lastCcValue = -1;
    int   ccCountdown = 0;
    int   ccIntervalSamples = 32;

    // Per-lane CC streams. heldValue latches on inactive steps so a skipped step holds
    // its level rather than dropping to zero.
    float laneHeldValue[params::numLanes] {};
    float laneSlewedValue[params::numLanes] {};
    int   laneLastCcValue[params::numLanes] { -1, -1, -1 };

    // What the receiving instrument has already been told, so the RPNs are re-sent only
    // when the mode, range or target channel actually changes.
    int   configuredMode      = -1;
    int   configuredBendRange = -1;
    int   configuredChannel   = -1;
    int   memberChannelIndex  = 0;

    std::atomic<int>   uiStep[params::numLanes] { {}, {}, {} };
    std::atomic<float> uiMix { 0.0f };
};
