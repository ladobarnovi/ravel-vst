/*
    Exercises SequencerEngine against a synthetic timeline.

    The engine takes PPQ positions as plain arguments rather than reading a playhead
    itself, so the whole sequencer core can be driven from a console app with no
    plugin host involved -- which means we can assert on exact note numbers and
    sample offsets instead of listening for them.

    Timeline used throughout: 48 kHz, 120 bpm.
      ppqPerSample = 120 / 60 / 48000 = 1 / 24000
      one 1/16 step = 0.25 ppq       = 6000 samples
*/

#include "SequencerEngine.h"

#include <cstdio>
#include <vector>

namespace
{
    constexpr double sampleRate     = 48000.0;
    constexpr double ppqPerSample   = 1.0 / 24000.0;
    constexpr int    samplesPerStep = 6000;      // at 1/16
    constexpr int    blockSize      = 512;       // deliberately not a divisor of 6000

    int checksRun    = 0;
    int checksFailed = 0;

    void check (bool condition, const char* what)
    {
        ++checksRun;

        if (condition)
        {
            std::printf ("    ok    %s\n", what);
        }
        else
        {
            ++checksFailed;
            std::printf ("    FAIL  %s\n", what);
        }
    }

    void section (const char* name)
    {
        std::printf ("\n  %s\n", name);
    }


    //==========================================================================
    enum EventType { noteOn, noteOff, controller };

    struct Event
    {
        int sample;
        EventType type;
        int number;   // note number, or CC number
        int value;    // velocity, or CC value
    };

    /** Runs the engine over a timeline in blocks, flattening MIDI into absolute
        sample positions so assertions can ignore block boundaries.
    */
    std::vector<Event> run (SequencerEngine& engine,
                            const SequencerEngine::Snapshot& snapshot,
                            int totalSamples,
                            double startPpq = 0.0)
    {
        std::vector<Event> events;
        juce::MidiBuffer buffer;

        for (int pos = 0; pos < totalSamples; pos += blockSize)
        {
            const int numSamples = juce::jmin (blockSize, totalSamples - pos);

            buffer.clear();
            engine.process (snapshot, buffer, numSamples,
                            startPpq + ppqPerSample * (double) pos, ppqPerSample, true);

            for (const auto metadata : buffer)
            {
                const auto message = metadata.getMessage();
                const int at = pos + metadata.samplePosition;

                if (message.isNoteOn())
                    events.push_back ({ at, noteOn, message.getNoteNumber(), message.getVelocity() });
                else if (message.isNoteOff())
                    events.push_back ({ at, noteOff, message.getNoteNumber(), 0 });
                else if (message.isController())
                    events.push_back ({ at, controller, message.getControllerNumber(), message.getControllerValue() });
            }
        }

        return events;
    }

    void dumpSamples (const char* label, const std::vector<Event>& events)
    {
        std::printf ("          %s:", label);

        for (const auto& e : events)
            std::printf (" %d", e.sample);

        std::printf ("\n");
    }

    std::vector<Event> only (const std::vector<Event>& events, EventType type)
    {
        std::vector<Event> result;

        for (const auto& e : events)
            if (e.type == type)
                result.push_back (e);

        return result;
    }

    /** Lane 1 drives at full depth, lanes 2 and 3 are inert. Chromatic scale so a
        scale degree maps 1:1 onto a semitone, keeping expected pitches obvious.
    */
    SequencerEngine::Snapshot baseSnapshot()
    {
        SequencerEngine::Snapshot s;

        for (auto& lane : s.lanes)
        {
            for (int i = 0; i < params::numSteps; ++i)
            {
                lane.values[i]  = 0.0f;
                lane.enabled[i] = true;
            }

            lane.length    = params::numSteps;
            lane.division  = params::divIndex_1_16;
            lane.direction = 0;
            lane.depth     = 0.0f;
            lane.mode      = params::modeAdd;
        }

        s.lanes[0].depth = 1.0f;

        s.outputMode    = params::outNotes;
        s.triggerSource = 0;
        s.root          = 48;
        s.rangeSteps    = 12;
        s.scale         = 0;        // Chromatic
        s.velocity      = 100;
        s.gatePercent   = 60.0f;
        s.midiChannel   = 1;
        s.ccNumber      = 1;
        s.ccChannel     = 1;
        s.offset        = 0.0f;
        s.slewMs        = 0.0f;

        return s;
    }
}

//==============================================================================
int main()
{
    std::printf ("TriLane engine tests\n");

    //==========================================================================
    section ("Step timing and pitch mapping (4-step lane at 1/16)");
    {
        auto s = baseSnapshot();
        s.lanes[0].length = 4;
        s.lanes[0].values[0] = 0.00f;   // degree 0  -> note 48
        s.lanes[0].values[1] = 0.25f;   // degree 3  -> note 51
        s.lanes[0].values[2] = 0.50f;   // degree 6  -> note 54
        s.lanes[0].values[3] = 0.75f;   // degree 9  -> note 57

        SequencerEngine engine;
        engine.prepare (sampleRate);

        const auto ons = only (run (engine, s, 8 * samplesPerStep), noteOn);

        check (ons.size() == 8, "eight steps fire over two beats");

        bool positionsOk = true;
        bool pitchesOk   = true;
        const int expected[] { 48, 51, 54, 57, 48, 51, 54, 57 };

        for (size_t i = 0; i < ons.size() && i < 8; ++i)
        {
            positionsOk = positionsOk && (ons[i].sample == (int) i * samplesPerStep);
            pitchesOk   = pitchesOk   && (ons[i].number == expected[i]);
        }

        check (positionsOk, "note-ons land exactly on 6000-sample step boundaries");

        if (! positionsOk)
            dumpSamples ("actual", ons);
        check (pitchesOk,   "pitches follow step values and repeat with the lane length");
        check (! ons.empty() && ons[0].value == 100, "velocity is taken from the parameter");
    }

    //==========================================================================
    section ("Gate length");
    {
        auto s = baseSnapshot();
        s.lanes[0].length = 4;
        s.gatePercent = 60.0f;   // 60% of 6000 = 3600 samples

        SequencerEngine engine;
        engine.prepare (sampleRate);

        const auto events = run (engine, s, 4 * samplesPerStep);
        const auto ons  = only (events, noteOn);
        const auto offs = only (events, noteOff);

        check (ons.size() == offs.size(), "every note-on is matched by a note-off");

        bool gateOk = true;

        for (size_t i = 0; i < ons.size() && i < offs.size(); ++i)
            gateOk = gateOk && (offs[i].sample - ons[i].sample == 3600);

        check (gateOk, "note-off arrives 60% of a step after note-on");
    }

    //==========================================================================
    section ("Independent lane length (polyrhythm)");
    {
        auto s = baseSnapshot();
        s.lanes[0].length = 5;

        for (int i = 0; i < 5; ++i)
            s.lanes[0].values[i] = (float) i / 12.0f;   // degrees 0..4 -> notes 48..52

        SequencerEngine engine;
        engine.prepare (sampleRate);

        const auto ons = only (run (engine, s, 10 * samplesPerStep), noteOn);

        check (ons.size() == 10, "ten steps fire");

        bool cyclesOk = true;
        const int expected[] { 48, 49, 50, 51, 52, 48, 49, 50, 51, 52 };

        for (size_t i = 0; i < ons.size() && i < 10; ++i)
            cyclesOk = cyclesOk && (ons[i].number == expected[i]);

        check (cyclesOk, "a 5-step lane wraps on 5, not on 8");
    }

    //==========================================================================
    section ("Lane rate is independent per lane");
    {
        // Lane 1 at 1/16 inert, lane 2 at 1/4 is the trigger source.
        auto s = baseSnapshot();
        s.lanes[1].division = params::divIndex_1_4;
        s.lanes[1].depth    = 1.0f;
        s.lanes[0].depth    = 0.0f;
        s.triggerSource     = 1;

        SequencerEngine engine;
        engine.prepare (sampleRate);

        const auto ons = only (run (engine, s, 8 * samplesPerStep), noteOn);

        check (ons.size() == 2, "a 1/4 lane fires twice where a 1/16 lane fires eight times");
        check (ons.size() == 2 && ons[1].sample - ons[0].sample == 4 * samplesPerStep,
               "1/4 steps are four 1/16 steps apart");
    }

    //==========================================================================
    section ("Disabled steps are transparent and fire nothing");
    {
        auto s = baseSnapshot();
        s.lanes[0].length = 4;
        s.lanes[0].enabled[1] = false;
        s.lanes[0].enabled[3] = false;

        SequencerEngine engine;
        engine.prepare (sampleRate);

        const auto ons = only (run (engine, s, 8 * samplesPerStep), noteOn);

        check (ons.size() == 4, "only enabled steps trigger notes");

        bool spacingOk = true;

        for (const auto& e : ons)
            spacingOk = spacingOk && (e.sample % (2 * samplesPerStep) == 0);

        check (spacingOk, "surviving triggers stay on their original grid positions");

        if (! spacingOk)
            dumpSamples ("actual", ons);
    }

    //==========================================================================
    section ("Multiply mode");
    {
        // Lane 2 multiplies the chain by zero, so every note collapses to the root.
        auto s = baseSnapshot();
        s.lanes[0].length = 4;
        s.lanes[0].values[1] = 0.5f;
        s.lanes[0].values[2] = 1.0f;
        s.lanes[1].mode  = params::modeMultiply;
        s.lanes[1].depth = 1.0f;   // all lane-2 values are 0.0

        SequencerEngine engine;
        engine.prepare (sampleRate);

        const auto ons = only (run (engine, s, 4 * samplesPerStep), noteOn);

        bool allRoot = ! ons.empty();

        for (const auto& e : ons)
            allRoot = allRoot && (e.number == 48);

        check (allRoot, "multiplying by a zero-valued lane collapses the mix to the root");
    }

    //==========================================================================
    section ("Multiply at zero depth is a no-op");
    {
        auto s = baseSnapshot();
        s.lanes[0].length = 4;
        s.lanes[0].values[1] = 0.5f;      // degree 6 -> note 54
        s.lanes[1].mode  = params::modeMultiply;
        s.lanes[1].depth = 0.0f;

        SequencerEngine engine;
        engine.prepare (sampleRate);

        const auto ons = only (run (engine, s, 4 * samplesPerStep), noteOn);

        check (ons.size() == 4 && ons[1].number == 54,
               "depth 0 leaves the mix untouched");
    }

    //==========================================================================
    section ("Transport jump");
    {
        auto s = baseSnapshot();
        s.lanes[0].length = 4;

        SequencerEngine engine;
        engine.prepare (sampleRate);

        juce::MidiBuffer buffer;

        // Settle on the timeline, then jump as a loop wrap would.
        engine.process (s, buffer, blockSize, 0.0, ppqPerSample, true);

        buffer.clear();
        engine.process (s, buffer, blockSize, 8.0, ppqPerSample, true);

        bool firedAtZero = false;

        for (const auto metadata : buffer)
            if (metadata.getMessage().isNoteOn() && metadata.samplePosition == 0)
                firedAtZero = true;

        check (firedAtZero, "a jump to a new PPQ position retriggers immediately");
    }

    //==========================================================================
    section ("Transport stop releases the held note");
    {
        auto s = baseSnapshot();
        s.lanes[0].length = 4;
        s.gatePercent = 200.0f;   // long enough that the note is still held

        SequencerEngine engine;
        engine.prepare (sampleRate);

        juce::MidiBuffer buffer;
        engine.process (s, buffer, blockSize, 0.0, ppqPerSample, true);

        const bool wasHeld = [&]
        {
            for (const auto metadata : buffer)
                if (metadata.getMessage().isNoteOn())
                    return true;
            return false;
        }();

        buffer.clear();
        engine.process (s, buffer, blockSize, ppqPerSample * blockSize, ppqPerSample, false);

        bool released = false;

        for (const auto metadata : buffer)
            if (metadata.getMessage().isNoteOff())
                released = true;

        check (wasHeld, "a note is sounding before the stop");
        check (released, "stopping the transport sends note-off (no stuck note)");
    }

    //==========================================================================
    section ("CC output");
    {
        auto s = baseSnapshot();
        s.lanes[0].length = 4;
        s.lanes[0].values[1] = 1.0f;
        s.outputMode = params::outCC;
        s.ccNumber = 74;

        SequencerEngine engine;
        engine.prepare (sampleRate);

        const auto events = run (engine, s, 4 * samplesPerStep);
        const auto ccs = only (events, controller);

        check (only (events, noteOn).empty(), "CC-only mode emits no notes");
        check (! ccs.empty(), "CC-only mode emits controller events");

        bool numberOk = true;
        int maxValue = 0;

        for (const auto& e : ccs)
        {
            numberOk = numberOk && (e.number == 74);
            maxValue = juce::jmax (maxValue, e.value);
        }

        check (numberOk, "controller events use the configured CC number");
        check (maxValue == 127, "a step value of 1.0 reaches CC 127");
    }

    //==========================================================================
    section ("Reverse direction");
    {
        auto s = baseSnapshot();
        s.lanes[0].length = 4;
        s.lanes[0].direction = 1;

        for (int i = 0; i < 4; ++i)
            s.lanes[0].values[i] = (float) i / 12.0f;   // notes 48..51

        SequencerEngine engine;
        engine.prepare (sampleRate);

        const auto ons = only (run (engine, s, 4 * samplesPerStep), noteOn);

        check (ons.size() == 4 && ons[0].number == 51 && ons[1].number == 50,
               "reverse walks the lane backwards");
    }

    //==========================================================================
    section ("Random direction is locked to the timeline");
    {
        auto s = baseSnapshot();
        s.lanes[0].length = 8;
        s.lanes[0].direction = 3;

        for (int i = 0; i < 8; ++i)
            s.lanes[0].values[i] = (float) i / 12.0f;

        SequencerEngine engineA, engineB;
        engineA.prepare (sampleRate);
        engineB.prepare (sampleRate);

        const auto first  = only (run (engineA, s, 8 * samplesPerStep), noteOn);
        const auto second = only (run (engineB, s, 8 * samplesPerStep), noteOn);

        bool identical = first.size() == second.size() && ! first.empty();

        for (size_t i = 0; i < first.size() && i < second.size(); ++i)
            identical = identical && (first[i].number == second[i].number);

        check (identical, "the same timeline span produces the same random pattern");

        bool varied = false;

        for (size_t i = 1; i < first.size(); ++i)
            if (first[i].number != first[0].number)
                varied = true;

        check (varied, "random actually varies across steps");
    }

    //==========================================================================
    std::printf ("\n%d checks, %d failed\n", checksRun, checksFailed);

    return checksFailed == 0 ? 0 : 1;
}
