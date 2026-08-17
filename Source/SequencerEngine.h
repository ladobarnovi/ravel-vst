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
        int   length    = params::numSteps;
        int   division  = params::divIndex_1_16;
        int   direction = 0;
        float depth     = 0.0f;
        int   mode      = params::modeAdd;
    };

    struct Snapshot
    {
        LaneSnapshot lanes[params::numLanes];

        int   outputMode    = params::outNotes;
        int   triggerSource = 0;
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

    /** Releases a held note immediately -- used on transport stop and reset. */
    void releaseHeldNote (juce::MidiBuffer& out, int sampleOffset);

    //==========================================================================
    // Read by the editor's timer. Plain relaxed atomics: a torn read just means
    // one stale repaint frame.
    int   getCurrentStep (int lane) const noexcept { return uiStep[lane].load (std::memory_order_relaxed); }
    float getMixValue()             const noexcept { return uiMix.load (std::memory_order_relaxed); }

private:
    //==========================================================================
    static int stepIndexFor (std::int64_t globalIndex, int length, int direction, int laneIndex) noexcept;

    struct LaneState
    {
        std::int64_t lastGlobalIndex = std::numeric_limits<std::int64_t>::min();
        int   step = 0;
        float held = 0.0f;
    };

    LaneState lanes[params::numLanes];

    double currentSampleRate = 44100.0;

    int   activeNote      = -1;
    int   activeChannel   = 1;
    int   noteOffCountdown = 0;

    float slewedValue = 0.0f;
    int   lastCcValue = -1;
    int   ccCountdown = 0;
    int   ccIntervalSamples = 32;

    std::atomic<int>   uiStep[params::numLanes] { {}, {}, {} };
    std::atomic<float> uiMix { 0.0f };
};
