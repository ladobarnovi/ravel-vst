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
    enum EventType { noteOn, noteOff, controller, pitchBend };

    struct Event
    {
        int sample;
        EventType type;
        int channel;
        int number;   // note number, or CC number
        int value;    // velocity, CC value, or bend position
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

                const int channel = message.getChannel();

                if (message.isNoteOn())
                    events.push_back ({ at, noteOn, channel, message.getNoteNumber(), message.getVelocity() });
                else if (message.isNoteOff())
                    events.push_back ({ at, noteOff, channel, message.getNoteNumber(), 0 });
                else if (message.isController())
                    events.push_back ({ at, controller, channel, message.getControllerNumber(), message.getControllerValue() });
                else if (message.isPitchWheel())
                    events.push_back ({ at, pitchBend, channel, 0, message.getPitchWheelValue() });
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

    /** Cuts the snapshot down to the first `count` lanes, the way an instance that has
        only been given that many does -- the processor folds its lane count into exactly
        this flag before the engine ever sees it.
    */
    void useLanes (SequencerEngine::Snapshot& s, int count)
    {
        for (int lane = count; lane < params::numLanes; ++lane)
            s.lanes[lane].active = false;
    }

    /** Sets every step's gate on one lane to the same percentage -- Gate is per-step now,
        where the tests used to set one value for the whole plugin. */
    void setLaneGate (SequencerEngine::Snapshot& s, int lane, float percent)
    {
        for (auto& g : s.lanes[lane].gate)
            g = percent;
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

            // Both default to unity in the parameter layout, so the harness has to set them
            // too -- a default-constructed Snapshot would leave every step at zero velocity.
            for (int i = 0; i < params::numSteps; ++i)
            {
                lane.chance[i]   = 1.0f;
                lane.velocity[i] = 1.0f;
                lane.gate[i]     = 60.0f;
            }

            lane.length    = params::numSteps;
            lane.division  = params::divIndex_1_16;
            lane.direction = 0;
            lane.depth     = 0.0f;
            lane.mode      = params::modeAdd;
            lane.nudge     = 0.0f;
            lane.humanize  = 0.0f;
            lane.ccOn      = false;
        }

        s.swing      = 0.0f;
        s.voiceCount = 1;

        s.lanes[0].depth = 1.0f;

        s.outputMode    = params::outNotes;
        s.triggerSource = 0;
        s.root          = 48;
        s.rangeSteps    = 12;
        s.scale         = 0;        // Chromatic
        s.velocity      = 100;
        s.midiChannel   = 1;
        s.ccNumber      = 1;
        s.ccChannel     = 1;
        s.offset        = 0.0f;
        s.slewMs        = 0.0f;
        s.quantize      = true;
        s.bendRange     = 2;

        return s;
    }
}

//==============================================================================
int main()
{
    std::printf ("Ravel engine tests\n");

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
        // baseSnapshot() defaults every step's gate to 60%, i.e. 3600 of 6000 samples.

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
        setLaneGate (s, 0, 200.0f);   // long enough that the note is still held

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
    section ("Semitone pitch mode (default)");
    {
        auto s = baseSnapshot();
        s.lanes[0].length = 4;

        SequencerEngine engine;
        engine.prepare (sampleRate);

        const auto events = run (engine, s, 4 * samplesPerStep);
        const auto ons = only (events, noteOn);

        check (only (events, pitchBend).empty(), "semitone mode sends no pitch bend");

        bool onNoteChannel = ! ons.empty();

        for (const auto& e : ons)
            onNoteChannel = onNoteChannel && (e.channel == 1);

        check (onNoteChannel, "quantized mode uses the Note Channel parameter");
    }

    //==========================================================================
    section ("Continuous pitch lands exactly on a degree");
    {
        auto s = baseSnapshot();
        s.lanes[0].length = 1;
        s.lanes[0].values[0] = 0.25f;   // 0.25 * 12 = exactly 3 semitones
        s.scale      = 0;
        s.rangeSteps = 12;
        s.root       = 48;
        s.quantize   = false;

        SequencerEngine engine;
        engine.prepare (sampleRate);

        const auto events = run (engine, s, 2 * samplesPerStep);
        const auto ons   = only (events, noteOn);
        const auto bends = only (events, pitchBend);

        check (! ons.empty() && ons[0].number == 51, "exact degree gives note 51");

        bool centred = false;

        for (const auto& bend : bends)
            if (! ons.empty() && bend.sample == ons[0].sample && bend.channel == ons[0].channel)
                centred = std::abs (bend.value - params::pitchBendCentre) <= 1;

        check (centred, "bend is centred when no fractional part is needed");
    }

    //==========================================================================
    section ("Scale is bypassed when Quantize is off");
    {
        // The same pattern under a pentatonic scale and under chromatic must produce
        // identical pitches: continuous mode is raw semitones, the scale only applies
        // in Semitone mode.
        const auto pitchesForScale = [] (int scaleIndex)
        {
            auto s = baseSnapshot();
            s.lanes[0].length = 4;
            s.scale      = scaleIndex;
            s.rangeSteps = 12;
            s.root       = 48;
            s.quantize   = false;
            s.bendRange  = 2;

            for (int i = 0; i < 4; ++i)
                s.lanes[0].values[i] = (float) i * 0.1f;

            SequencerEngine engine;
            engine.prepare (sampleRate);

            const auto events = run (engine, s, 4 * samplesPerStep);
            const auto ons   = only (events, noteOn);
            const auto bends = only (events, pitchBend);

            std::vector<double> pitches;

            for (const auto& on : ons)
                for (const auto& bend : bends)
                    if (bend.sample == on.sample)
                        pitches.push_back ((double) on.number
                                           + ((double) bend.value - 8192.0) / 8191.0 * 2.0);

            return pitches;
        };

        const auto pentatonic = pitchesForScale (4);   // Pentatonic Minor
        const auto chromatic  = pitchesForScale (0);

        bool identical = ! pentatonic.empty() && pentatonic.size() == chromatic.size();

        for (size_t i = 0; i < pentatonic.size() && i < chromatic.size(); ++i)
            identical = identical && std::abs (pentatonic[i] - chromatic[i]) < 1.0e-6;

        check (identical, "pentatonic and chromatic give identical continuous pitch");

        bool linear = pentatonic.size() == 4;

        for (size_t i = 0; i < pentatonic.size() && i < 4; ++i)
        {
            const float  value    = (float) i * 0.1f;
            const double expected = 48.0 + (double) (value * 12.0f);

            linear = linear && std::abs (pentatonic[i] - expected) < 0.01;
        }

        check (linear, "continuous pitch is root + mix * range, in semitones");
    }

    //==========================================================================
    section ("Pitch is static for the duration of a step (no glide)");
    {
        auto s = baseSnapshot();
        s.lanes[0].length = 2;
        s.lanes[0].values[0] = 0.17f;
        s.lanes[0].values[1] = 0.61f;
        s.scale      = 0;
        s.rangeSteps = 12;
        s.quantize   = false;
        s.slewMs     = 400.0f;   // heavy slew: must affect CC only, never pitch

        SequencerEngine engine;
        engine.prepare (sampleRate);

        const auto events = run (engine, s, 4 * samplesPerStep);
        const auto ons   = only (events, noteOn);
        const auto bends = only (events, pitchBend);

        check (bends.size() == ons.size(),
               "exactly one bend per note -- no stream of bends between steps");

        // Step 0 recurs at index 2, and must land on the identical pitch despite the slew.
        bool repeatable = ons.size() >= 3;

        if (repeatable)
            repeatable = (ons[0].number == ons[2].number)
                       && (bends[0].value == bends[2].value);

        check (repeatable, "slew does not bleed into pitch: a repeated step is identical");
    }

    //==========================================================================
    section ("Continuous pitch via mono pitch bend");
    {
        auto s = baseSnapshot();
        s.lanes[0].length = 1;
        s.lanes[0].values[0] = 0.3f;   // -> 51.6 semitones with Chromatic, range 12, root 48
        s.scale       = 0;
        s.rangeSteps  = 12;
        s.root        = 48;
        s.quantize    = false;
        s.bendRange   = 2;
        s.midiChannel = 5;

        SequencerEngine engine;
        engine.prepare (sampleRate);

        const auto events = run (engine, s, 4 * samplesPerStep);
        const auto ons   = only (events, noteOn);
        const auto bends = only (events, pitchBend);

        check (! ons.empty(), "pitch bend mode fires notes");

        bool allOnNoteChannel = ! ons.empty() && ! bends.empty();

        for (const auto& e : ons)
            allOnNoteChannel = allOnNoteChannel && (e.channel == 5);

        for (const auto& e : bends)
            allOnNoteChannel = allOnNoteChannel && (e.channel == 5);

        check (allOnNoteChannel,
               "notes AND bends stay on the single Note Chan (survives channel merging)");

        bool reconstructs = false;

        if (! ons.empty())
        {
            for (const auto& bend : bends)
            {
                if (bend.sample == ons[0].sample)
                {
                    const double pitch = (double) ons[0].number
                                       + ((double) bend.value - 8192.0) / 8191.0 * (double) s.bendRange;

                    reconstructs = std::abs (pitch - 51.6) < 0.01;
                }
            }
        }

        check (reconstructs, "note + bend reconstructs 51.6 semitones");
    }

    //==========================================================================
    section ("Continuous pitch announces its bend range and nothing else");
    {
        auto s = baseSnapshot();
        s.quantize    = false;
        s.bendRange   = 7;
        s.midiChannel = 3;

        SequencerEngine engine;
        engine.prepare (sampleRate);

        juce::MidiBuffer buffer;
        engine.process (s, buffer, 512, 0.0, ppqPerSample, true);

        int rpnDataEntries = 0;
        bool sawRangeOnNoteChannel = false;
        int currentRpn = -1;

        for (const auto metadata : buffer)
        {
            const auto message = metadata.getMessage();

            if (! message.isController())
                continue;

            if (message.getControllerNumber() == 0x64)
                currentRpn = message.getControllerValue();

            if (message.getControllerNumber() == 0x06)
            {
                ++rpnDataEntries;

                if (currentRpn == params::pitchBendRangeRpn
                    && message.getControllerValue() == 7
                    && message.getChannel() == 3)
                    sawRangeOnNoteChannel = true;
            }
        }

        check (sawRangeOnNoteChannel, "bend range RPN 0 goes to the Note Chan");
        check (rpnDataEntries == 1, "only the bend range is announced, no other RPN");
    }

    //==========================================================================
    section ("Continuous mapping is linear across a range of values");
    {
        auto s = baseSnapshot();
        s.lanes[0].length = 4;
        s.scale      = 0;      // Chromatic
        s.rangeSteps = 24;
        s.root       = 36;
        s.quantize   = false;
        s.bendRange  = 2;

        for (int i = 0; i < 4; ++i)
            s.lanes[0].values[i] = (float) i * 0.1f;   // 0.0, 0.1, 0.2, 0.3

        SequencerEngine engine;
        engine.prepare (sampleRate);

        const auto events = run (engine, s, 4 * samplesPerStep);
        const auto ons   = only (events, noteOn);
        const auto bends = only (events, pitchBend);

        // Expected absolute pitches: 36 + value*24 -> 36.0, 38.4, 40.8, 43.2
        const double expected[] { 36.0, 38.4, 40.8, 43.2 };

        bool linear = ons.size() == 4;

        for (size_t i = 0; i < ons.size() && i < 4; ++i)
        {
            double pitch = -1.0;

            for (const auto& bend : bends)
                if (bend.sample == ons[i].sample)
                    pitch = (double) ons[i].number
                          + ((double) bend.value - 8192.0) / 8191.0 * (double) s.bendRange;

            linear = linear && (pitch > 0.0) && std::abs (pitch - expected[i]) < 0.01;
        }

        check (linear, "pitch = root + mix * range holds across the whole pattern");
    }

    //==========================================================================
    section ("Per-step Slide glides pitch from the previous note");
    {
        // A two-step lane in plain 12-EDO: note 48 then note 54. Only the second step slides,
        // for a full step, and the wide Bend Range keeps the six-semitone glide clear of the
        // wheel's limit. maxSlideStepFraction is 1, so the glide spans one whole 1/16 = 6000
        // samples: it starts at 6000 and lands at 12000, where the pattern loops back to 48.
        auto s = baseSnapshot();
        s.lanes[0].length = 2;
        s.quantize   = true;
        s.scale      = 0;        // Chromatic: a scale degree is a semitone
        s.rangeSteps = 12;
        s.root       = 48;
        s.bendRange  = 12;
        s.lanes[0].values[0] = 0.0f;    // degree 0 -> note 48
        s.lanes[0].values[1] = 0.5f;    // degree 6 -> note 54
        s.lanes[0].slide[0]  = 0.0f;    // the first note has nothing to glide from
        s.lanes[0].slide[1]  = 1.0f;    // full-step glide into note 54

        SequencerEngine engine;
        engine.prepare (sampleRate);

        const auto events = run (engine, s, 2 * samplesPerStep);
        const auto ons    = only (events, noteOn);
        const auto bends  = only (events, pitchBend);

        const bool notesAsExpected = ons.size() >= 2
                                   && ons[0].number == 48 && ons[0].sample == 0
                                   && ons[1].number == 54 && ons[1].sample == samplesPerStep;

        check (notesAsExpected, "the slid step still lands on its own note, on time");

        // note + bend, back to an absolute pitch, the same way a receiving instrument would.
        const auto reconstruct = [&] (int noteNumber, int bendValue)
        {
            return (double) noteNumber + ((double) bendValue - 8192.0) / 8191.0 * (double) s.bendRange;
        };

        // The wheel value riding the note-on at 6000 places the pitch back at the *previous*
        // note (48), not at 54 -- the glide starts from where the line left off.
        double startPitch = -1.0;

        for (const auto& b : bends)
            if (b.sample == samplesPerStep)
                startPitch = reconstruct (54, b.value);

        check (startPitch > 0.0 && std::abs (startPitch - 48.0) < 0.1,
               "the glide begins at the previous note's pitch");

        // Across the step the wheel ramps, and the last value before the pattern loops has
        // converged onto the target note.
        int    rampCount = 0;
        double endPitch  = -1.0;

        for (const auto& b : bends)
            if (b.sample > samplesPerStep && b.sample < 2 * samplesPerStep)
            {
                ++rampCount;
                endPitch = reconstruct (54, b.value);
            }

        check (rampCount > 10, "the glide is a ramp of many wheel updates, not one jump");
        check (endPitch > 0.0 && std::abs (endPitch - 54.0) < 0.1,
               "and it arrives on the target note by the end of the step");
    }

    //==========================================================================
    section ("A step at zero Slide leaves the wheel untouched in 12-EDO");
    {
        // The whole point of the default: no slide anywhere means the plugin never touches the
        // wheel in a plain 12-EDO scale, exactly as it did before Slide existed.
        auto s = baseSnapshot();
        s.lanes[0].length = 2;
        s.quantize = true;
        s.scale    = 0;
        s.lanes[0].values[0] = 0.0f;
        s.lanes[0].values[1] = 0.5f;
        // slide stays 0 on every step

        SequencerEngine engine;
        engine.prepare (sampleRate);

        const auto events = run (engine, s, 2 * samplesPerStep);

        check (only (events, pitchBend).empty(),
               "no slide, 12-EDO: not a single pitch-wheel message");
    }

    //==========================================================================
    section ("Turning Quantize back on recentres the pitch wheel");
    {
        // Continuous notes leave a bend on the channel. Quantized mode never writes the wheel
        // again, so without an explicit recentre every following note would play detuned.
        auto s = baseSnapshot();
        s.quantize = false;
        s.lanes[0].length = 1;
        s.lanes[0].values[0] = 0.3f;    // a value with a fractional semitone, so bend != centre
        s.rangeSteps = 12;

        SequencerEngine engine;
        engine.prepare (sampleRate);

        juce::MidiBuffer buffer;
        engine.process (s, buffer, 512, 0.0, ppqPerSample, true);

        int lastBend = params::pitchBendCentre;

        for (const auto metadata : buffer)
            if (metadata.getMessage().isPitchWheel())
                lastBend = metadata.getMessage().getPitchWheelValue();

        check (lastBend != params::pitchBendCentre, "continuous pitch leaves the wheel off centre");

        s.quantize = true;
        buffer.clear();
        engine.process (s, buffer, 512, ppqPerSample * 512, ppqPerSample, true);

        bool recentred = false;

        for (const auto metadata : buffer)
            if (metadata.getMessage().isPitchWheel()
                && metadata.getMessage().getPitchWheelValue() == params::pitchBendCentre)
                recentred = true;

        check (recentred, "switching Quantize on sends a centred pitch wheel");
    }

    //==========================================================================
    section ("Scale table invariants");
    {
        bool rootIsZero    = true;
        bool octavesExact  = true;
        bool degreesSorted = true;

        for (int i = 0; i < params::numScales; ++i)
        {
            const auto& def = params::scales[(size_t) i];

            rootIsZero = rootIsZero && def.intervals[0] == 0
                                    && std::abs (params::scaleStepToSemitone (0, i)) < 1.0e-6f;

            // One scale-octave is 12 semitones in every tuning here, upwards and downwards,
            // however many degrees it took to climb. That is what keeps a 19- or 53-EDO
            // pattern octave-aligned with everything else in the session.
            octavesExact = octavesExact
                        && std::abs (params::scaleStepToSemitone (def.size, i) - 12.0f) < 1.0e-6f
                        && std::abs (params::scaleStepToSemitone (-def.size, i) + 12.0f) < 1.0e-6f;

            for (int d = 1; d < def.size; ++d)
                degreesSorted = degreesSorted
                             && def.intervals[(size_t) d] > def.intervals[(size_t) d - 1]
                             && def.intervals[(size_t) d] < def.edo;
        }

        check (params::scaleNames.size() == params::numScales, "every scale in the table is named");
        check (rootIsZero,    "degree 0 is the root in every scale");
        check (octavesExact,  "a scale-octave is exactly 12 semitones in every tuning");
        check (degreesSorted, "degrees ascend and stay inside one octave of their EDO");
    }

    //==========================================================================
    section ("Non-12 EDO scales land where the tuning says");
    {
        const auto indexOfScale = [] (const char* name) { return params::scaleNames.indexOf (name); };

        const auto centsOfDegree = [&] (const char* name, int degree)
        {
            return (double) params::scaleStepToSemitone (degree, indexOfScale (name)) * 100.0;
        };

        // 19-EDO is a meantone: its major third (6 of 19 steps) is flatter than 12-EDO's 400
        // cents and closer to just intonation's 386.3.
        check (std::abs (centsOfDegree ("19 Major", 2) - 378.9) < 0.5,
               "the 19-EDO major third is 6 steps, 378.9 cents");

        check (std::abs (centsOfDegree ("19 Chromatic", 1) - 63.2) < 0.5,
               "one 19-EDO step is 63.2 cents");

        // The point of mavila: the scale is shaped like a major scale but its third degree
        // comes out minor-sized, because the generating fifth is flat.
        check (std::abs (centsOfDegree ("23 Mavila 7", 2) - 313.0) < 0.5,
               "the 23-EDO antidiatonic third is minor-sized at 313 cents");

        // 53-EDO's whole reason for existing: near-exact just thirds and fifths.
        check (std::abs (centsOfDegree ("53 Just Major", 2) - 386.3) < 2.0,
               "53-EDO renders 5/4 to within a couple of cents");

        check (std::abs (centsOfDegree ("53 Just Major", 4) - 702.0) < 1.0,
               "53-EDO renders 3/2 to within a cent");

        // Turkish makam is written on the same 53 commas; Rast differs from a just major
        // scale at the sixth, which is what stops it sounding like one.
        check (centsOfDegree ("53 Rast", 5) > centsOfDegree ("53 Just Major", 5),
               "Rast takes a higher sixth than the just major scale");
    }

    //==========================================================================
    section ("A quantized microtonal scale plays as note + bend");
    {
        auto s = baseSnapshot();
        s.scale      = params::scaleNames.indexOf ("19 Chromatic");
        s.quantize   = true;
        s.root       = 48;
        s.rangeSteps = 19;      // one 19-EDO octave over the full mix range
        s.bendRange  = 2;
        s.lanes[0].length = 4;

        // Degrees 0, 1, 7 and 19: the root, one step up, a scale note that falls between two
        // MIDI keys, and the octave.
        const int degrees[] { 0, 1, 7, 19 };

        for (int i = 0; i < 4; ++i)
            s.lanes[0].values[i] = (float) degrees[i] / 19.0f;

        SequencerEngine engine;
        engine.prepare (sampleRate);

        const auto events = run (engine, s, 4 * samplesPerStep);
        const auto ons    = only (events, noteOn);
        const auto bends  = only (events, pitchBend);

        bool inTune      = ons.size() == 4;
        bool alwaysBends = ons.size() == 4;

        for (size_t i = 0; i < ons.size() && i < 4; ++i)
        {
            double pitch = -1.0;

            for (const auto& bend : bends)
                if (bend.sample == ons[i].sample && bend.channel == ons[i].channel)
                    pitch = (double) ons[i].number
                          + ((double) bend.value - 8192.0) / 8191.0 * (double) s.bendRange;

            // A bend accompanies every note, including the two that need none: without it the
            // note would inherit whatever the previous degree left on the channel.
            alwaysBends = alwaysBends && pitch > 0.0;

            const double expected = 48.0 + (double) degrees[i] * 12.0 / 19.0;

            inTune = inTune && pitch > 0.0 && std::abs (pitch - expected) < 0.01;
        }

        check (alwaysBends, "every note in a microtonal scale carries its own bend");
        check (inTune,      "note + bend reconstructs the 19-EDO degree");

        bool octaveIsExact = false;

        for (const auto& bend : bends)
            if (! ons.empty() && ons.size() == 4 && bend.sample == ons[3].sample)
                octaveIsExact = ons[3].number == 60
                             && std::abs (bend.value - params::pitchBendCentre) <= 1;

        check (octaveIsExact, "the 19th degree is the octave, on the key, with a centred wheel");
    }

    //==========================================================================
    section ("A quantized microtonal scale announces its bend range");
    {
        auto s = baseSnapshot();
        s.scale       = params::scaleNames.indexOf ("53 Just Major");
        s.quantize    = true;
        s.bendRange   = 5;
        s.midiChannel = 2;

        SequencerEngine engine;
        engine.prepare (sampleRate);

        juce::MidiBuffer buffer;
        engine.process (s, buffer, 512, 0.0, ppqPerSample, true);

        bool announced = false;
        int currentRpn = -1;

        for (const auto metadata : buffer)
        {
            const auto message = metadata.getMessage();

            if (! message.isController())
                continue;

            if (message.getControllerNumber() == 0x64)
                currentRpn = message.getControllerValue();

            if (message.getControllerNumber() == 0x06
                && currentRpn == params::pitchBendRangeRpn
                && message.getControllerValue() == 5
                && message.getChannel() == 2)
                announced = true;
        }

        check (announced, "bend range RPN 0 goes out for a microtonal scale, not just continuous mode");
    }

    //==========================================================================
    section ("Leaving a microtonal scale recentres the pitch wheel");
    {
        // Same hazard as switching Quantize back on: a 12-EDO scale never writes the wheel,
        // so the bend the last 19-EDO note left behind would detune everything after it.
        auto s = baseSnapshot();
        s.scale = params::scaleNames.indexOf ("19 Chromatic");
        s.lanes[0].length = 1;
        s.lanes[0].values[0] = 7.0f / 19.0f;    // a degree that sits between two keys
        s.rangeSteps = 19;

        SequencerEngine engine;
        engine.prepare (sampleRate);

        juce::MidiBuffer buffer;
        engine.process (s, buffer, 512, 0.0, ppqPerSample, true);

        int lastBend = params::pitchBendCentre;

        for (const auto metadata : buffer)
            if (metadata.getMessage().isPitchWheel())
                lastBend = metadata.getMessage().getPitchWheelValue();

        check (lastBend != params::pitchBendCentre, "a 19-EDO degree leaves the wheel off centre");

        s.scale = 0;    // back to 12-EDO Chromatic
        buffer.clear();
        engine.process (s, buffer, 512, ppqPerSample * 512, ppqPerSample, true);

        bool recentred = false;
        bool stillBends = false;

        for (const auto metadata : buffer)
        {
            if (! metadata.getMessage().isPitchWheel())
                continue;

            if (metadata.getMessage().getPitchWheelValue() == params::pitchBendCentre)
                recentred = true;
            else
                stillBends = true;
        }

        check (recentred,   "switching to a 12-EDO scale sends a centred pitch wheel");
        check (! stillBends, "and a 12-EDO scale writes no bend of its own");
    }

    //==========================================================================
    section ("Per-step probability");
    {
        const auto countNotesWithChance = [] (float chance)
        {
            auto s = baseSnapshot();
            s.lanes[0].length = 8;

            for (int i = 0; i < params::numSteps; ++i)
                s.lanes[0].chance[i] = chance;

            SequencerEngine engine;
            engine.prepare (sampleRate);

            return only (run (engine, s, 64 * samplesPerStep), noteOn).size();
        };

        check (countNotesWithChance (1.0f) == 64, "chance 100% fires every step");
        check (countNotesWithChance (0.0f) == 0,  "chance 0% fires nothing");

        const auto half = countNotesWithChance (0.5f);
        check (half > 12 && half < 52, "chance 50% fires roughly half of 64 steps");
    }

    //==========================================================================
    section ("Probability is locked to the timeline");
    {
        auto s = baseSnapshot();
        s.lanes[0].length = 8;

        for (int i = 0; i < params::numSteps; ++i)
            s.lanes[0].chance[i] = 0.5f;

        SequencerEngine engineA, engineB;
        engineA.prepare (sampleRate);
        engineB.prepare (sampleRate);

        const auto first  = only (run (engineA, s, 16 * samplesPerStep), noteOn);
        const auto second = only (run (engineB, s, 16 * samplesPerStep), noteOn);

        bool identical = first.size() == second.size() && ! first.empty();

        for (size_t i = 0; i < first.size() && i < second.size(); ++i)
            identical = identical && first[i].sample == second[i].sample;

        check (identical, "the same timeline span skips exactly the same steps");
    }

    //==========================================================================
    section ("Probability makes a step transparent, like an off step");
    {
        // Lane 1 multiplies by zero, but with chance 0 it must not touch the mix at all.
        auto s = baseSnapshot();
        s.lanes[0].length = 4;
        s.lanes[0].values[1] = 0.5f;     // -> note 54 with range 12, chromatic
        s.scale = 0;
        s.lanes[1].mode   = params::modeMultiply;
        s.lanes[1].depth  = 1.0f;
        s.lanes[1].chance[0] = 0.0f;
        s.lanes[1].chance[1] = 0.0f;
        s.lanes[1].chance[2] = 0.0f;
        s.lanes[1].chance[3] = 0.0f;
        s.lanes[1].chance[4] = 0.0f;
        s.lanes[1].chance[5] = 0.0f;
        s.lanes[1].chance[6] = 0.0f;
        s.lanes[1].chance[7] = 0.0f;

        SequencerEngine engine;
        engine.prepare (sampleRate);

        const auto ons = only (run (engine, s, 4 * samplesPerStep), noteOn);

        check (ons.size() == 4 && ons[1].number == 54,
               "a lane whose steps all fail their roll leaves the mix untouched");
    }

    //==========================================================================
    section ("A muted lane is transparent and fires nothing");
    {
        // Lane 2 multiplies the chain by zero, which would collapse every note to the root.
        // Muted, it must leave the mix exactly as an absent lane would.
        auto s = baseSnapshot();
        s.lanes[0].length = 4;
        s.lanes[0].values[1] = 0.5f;     // -> note 54 with range 12, chromatic
        s.lanes[1].mode   = params::modeMultiply;
        s.lanes[1].depth  = 1.0f;
        s.lanes[1].active = false;

        SequencerEngine engine;
        engine.prepare (sampleRate);

        const auto ons = only (run (engine, s, 4 * samplesPerStep), noteOn);

        check (ons.size() == 4 && ons[1].number == 54,
               "a muted lane leaves the mix untouched");

        // Muting the lane that drives the notes silences the sequencer, in both modes.
        auto muted = baseSnapshot();
        muted.lanes[0].length = 4;
        muted.lanes[0].active = false;

        SequencerEngine mono;
        mono.prepare (sampleRate);

        check (only (run (mono, muted, 4 * samplesPerStep), noteOn).empty(),
               "muting the trigger lane stops the notes");

        muted.polyMode = true;
        muted.lanes[1].depth  = 1.0f;
        muted.lanes[1].length = 4;

        SequencerEngine poly;
        poly.prepare (sampleRate);

        const auto polyOns = only (run (poly, muted, 4 * samplesPerStep), noteOn);

        check (polyOns.size() == 4,
               "in poly mode only the muted lane goes quiet, not its neighbours");
    }

    //==========================================================================
    section ("Swing shifts alternate steps");
    {
        auto s = baseSnapshot();
        s.lanes[0].length = 4;
        s.swing = 0.5f;      // delay every other step by 25% of a step

        SequencerEngine engine;
        engine.prepare (sampleRate);

        const auto ons = only (run (engine, s, 4 * samplesPerStep), noteOn);

        check (ons.size() == 4, "swing does not change how many steps fire");

        // Even steps stay put; odd steps move late by swing/2 of a step.
        const int expectedShift = (int) std::lround (0.25 * samplesPerStep);

        bool shifted = ons.size() == 4;

        if (shifted)
        {
            shifted = ons[0].sample == 0
                   && std::abs (ons[1].sample - (samplesPerStep + expectedShift)) <= 1
                   && ons[2].sample == 2 * samplesPerStep
                   && std::abs (ons[3].sample - (3 * samplesPerStep + expectedShift)) <= 1;
        }

        check (shifted, "odd steps land late by half the swing amount, even steps do not move");
    }

    //==========================================================================
    section ("Zero swing and nudge reproduce the plain grid");
    {
        auto s = baseSnapshot();
        s.lanes[0].length = 4;
        s.swing = 0.0f;
        s.lanes[0].nudge = 0.0f;

        SequencerEngine engine;
        engine.prepare (sampleRate);

        const auto ons = only (run (engine, s, 8 * samplesPerStep), noteOn);

        bool onGrid = ons.size() == 8;

        for (size_t i = 0; i < ons.size() && i < 8; ++i)
            onGrid = onGrid && ons[i].sample == (int) i * samplesPerStep;

        check (onGrid, "with no timing offsets the boundaries are exactly as before");
    }

    //==========================================================================
    section ("Per-lane nudge shifts the whole lane");
    {
        auto s = baseSnapshot();
        s.lanes[0].length = 4;
        s.lanes[0].nudge = 0.4f;   // 20% of a step late

        SequencerEngine engine;
        engine.prepare (sampleRate);

        const auto ons = only (run (engine, s, 4 * samplesPerStep), noteOn);
        const int expectedShift = (int) std::lround (0.2 * samplesPerStep);

        // Nudge moves step 0's boundary past sample 0, so the cold start (index changing
        // from INT64_MIN) fires once at sample 0 before the shifted grid begins. That
        // immediate first fire is deliberate -- it is what makes a transport jump
        // retrigger straight away instead of waiting up to a whole step.
        bool shiftedGrid = ons.size() >= 4 && ons[0].sample == 0;

        for (size_t i = 1; i < ons.size() && shiftedGrid; ++i)
        {
            const int expected = expectedShift + (int) (i - 1) * samplesPerStep;
            shiftedGrid = std::abs (ons[i].sample - expected) <= 1;
        }

        check (shiftedGrid, "every step in the lane moves late by the same amount");
    }

    //==========================================================================
    section ("Per-lane CC output");
    {
        auto s = baseSnapshot();
        s.lanes[0].length = 2;
        s.lanes[0].values[0] = 0.0f;
        s.lanes[0].values[1] = 1.0f;
        s.lanes[0].depth = 0.0f;       // deliberately zero: lane CC ignores Depth
        s.lanes[0].ccOn = true;
        s.lanes[0].ccNumber = 20;
        s.lanes[0].ccChannel = 3;
        s.outputMode = params::outCC;

        SequencerEngine engine;
        engine.prepare (sampleRate);

        const auto ccs = only (run (engine, s, 4 * samplesPerStep), controller);

        bool sawLaneCc = false;
        int laneMax = -1;

        for (const auto& e : ccs)
            if (e.number == 20 && e.channel == 3)
            {
                sawLaneCc = true;
                laneMax = juce::jmax (laneMax, e.value);
            }

        check (sawLaneCc, "an enabled lane sends CC on its own number and channel");
        check (laneMax == 127, "lane CC reaches 127 despite Depth being zero");
    }

    //==========================================================================
    section ("Lane CC stays silent when disabled");
    {
        auto s = baseSnapshot();
        s.lanes[0].length = 2;
        s.lanes[0].values[1] = 1.0f;
        s.lanes[0].ccOn = false;
        s.lanes[0].ccNumber = 20;
        s.outputMode = params::outCC;

        SequencerEngine engine;
        engine.prepare (sampleRate);

        const auto ccs = only (run (engine, s, 4 * samplesPerStep), controller);

        bool sawLaneCc = false;

        for (const auto& e : ccs)
            if (e.number == 20)
                sawLaneCc = true;

        check (! sawLaneCc, "a disabled lane sends no CC");
    }

    //==========================================================================
    section ("Voices = 1 keeps the original monophonic behaviour");
    {
        auto s = baseSnapshot();
        s.lanes[0].length = 4;
        s.scale = 0;
        setLaneGate (s, 0, 180.0f);   // would overlap if polyphony were allowed
        s.voiceCount = 1;

        for (int i = 0; i < 4; ++i)
            s.lanes[0].values[i] = (float) (i + 1) / 12.0f;   // distinct pitches

        SequencerEngine engine;
        engine.prepare (sampleRate);

        const auto events = run (engine, s, 4 * samplesPerStep);

        // Walk the stream and track how many notes are sounding at once.
        int sounding = 0;
        int peak = 0;

        for (const auto& e : events)
        {
            if (e.type == noteOn)  ++sounding;
            if (e.type == noteOff) --sounding;

            peak = juce::jmax (peak, sounding);
        }

        check (peak == 1, "with one voice a long gate never overlaps");
    }

    //==========================================================================
    section ("Polyphony lets a long gate overlap");
    {
        auto s = baseSnapshot();
        s.lanes[0].length = 4;
        s.scale = 0;
        setLaneGate (s, 0, 180.0f);   // 1.8 steps long, so each note laps into the next
        s.voiceCount = 4;

        for (int i = 0; i < 4; ++i)
            s.lanes[0].values[i] = (float) (i + 1) / 12.0f;

        SequencerEngine engine;
        engine.prepare (sampleRate);

        const auto events = run (engine, s, 4 * samplesPerStep);

        int sounding = 0;
        int peak = 0;

        for (const auto& e : events)
        {
            if (e.type == noteOn)  ++sounding;
            if (e.type == noteOff) --sounding;

            peak = juce::jmax (peak, sounding);
        }

        check (peak == 2, "a 180% gate holds two notes at once");

        const auto ons  = only (events, noteOn);
        const auto offs = only (events, noteOff);

        // A 180% gate means the last note's release lands past the end of the run, so a
        // small tail of still-held voices is correct. What matters is that nothing leaks:
        // never more held than the voice count, and never a note-off without a note-on.
        const int unreleased = (int) ons.size() - (int) offs.size();

        check (unreleased >= 0 && unreleased <= s.voiceCount,
               "note-offs match note-ons apart from voices still held when the run ends");
    }

    //==========================================================================
    section ("Voice count is respected as a ceiling");
    {
        auto s = baseSnapshot();
        s.lanes[0].length = 8;
        s.scale = 0;
        setLaneGate (s, 0, 200.0f);
        s.voiceCount = 2;

        for (int i = 0; i < params::numSteps; ++i)
            s.lanes[0].values[i] = (float) (i + 1) / 24.0f;

        SequencerEngine engine;
        engine.prepare (sampleRate);

        const auto events = run (engine, s, 8 * samplesPerStep);

        int sounding = 0;
        int peak = 0;

        for (const auto& e : events)
        {
            if (e.type == noteOn)  ++sounding;
            if (e.type == noteOff) --sounding;

            peak = juce::jmax (peak, sounding);
        }

        check (peak <= 2, "never more notes sounding than the voice count allows");
    }

    //==========================================================================
    section ("Repeated pitch reuses its voice instead of hanging");
    {
        // Every step is the same pitch on the same channel with a long gate. MIDI cannot
        // distinguish two identical notes, so the engine must retrigger rather than stack.
        auto s = baseSnapshot();
        s.lanes[0].length = 1;
        s.lanes[0].values[0] = 0.25f;
        s.scale = 0;
        setLaneGate (s, 0, 190.0f);
        s.voiceCount = 4;

        SequencerEngine engine;
        engine.prepare (sampleRate);

        const auto events = run (engine, s, 6 * samplesPerStep);

        int sounding = 0;
        int peak = 0;

        for (const auto& e : events)
        {
            if (e.type == noteOn)  ++sounding;
            if (e.type == noteOff) --sounding;

            peak = juce::jmax (peak, sounding);
        }

        check (peak == 1, "an identical pitch never stacks on itself");
    }

    //==========================================================================
    section ("Transport stop releases every voice");
    {
        auto s = baseSnapshot();
        s.lanes[0].length = 4;
        s.scale = 0;
        setLaneGate (s, 0, 190.0f);
        s.voiceCount = 4;

        for (int i = 0; i < 4; ++i)
            s.lanes[0].values[i] = (float) (i + 1) / 12.0f;

        SequencerEngine engine;
        engine.prepare (sampleRate);

        juce::MidiBuffer buffer;
        int sounding = 0;

        // Run far enough to get two notes overlapping.
        for (int pos = 0; pos < samplesPerStep + blockSize; pos += blockSize)
        {
            buffer.clear();
            engine.process (s, buffer, blockSize, ppqPerSample * pos, ppqPerSample, true);

            for (const auto metadata : buffer)
            {
                if (metadata.getMessage().isNoteOn())  ++sounding;
                if (metadata.getMessage().isNoteOff()) --sounding;
            }
        }

        check (sounding == 2, "two notes are sounding before the stop");

        buffer.clear();
        engine.process (s, buffer, blockSize, 0.0, ppqPerSample, false);

        int released = 0;

        for (const auto metadata : buffer)
            if (metadata.getMessage().isNoteOff())
                ++released;

        check (released == sounding, "stopping releases all of them, leaving nothing stuck");
    }

    //==========================================================================
    section ("Poly mode gives each lane its own voice");
    {
        // Three lanes, one step each so every lane repeats one pitch, and three different
        // divisions so their triggers land on different grids.
        auto s = baseSnapshot();
        s.polyMode = true;
        s.scale = 0;               // Chromatic: a degree is a semitone
        s.rangeSteps = 12;
        useLanes (s, 3);

        for (int lane = 0; lane < params::numLanes; ++lane)
        {
            s.lanes[lane].length = 1;
            s.lanes[lane].depth  = 1.0f;
            setLaneGate (s, lane, 50.0f);
        }

        s.lanes[0].values[0] = 0.0f;             // root, 48
        s.lanes[1].values[0] = 4.0f / 12.0f;     // +4, 52
        s.lanes[2].values[0] = 7.0f / 12.0f;     // +7, 55

        s.lanes[0].division = params::divIndex_1_16;
        s.lanes[1].division = params::divIndex_1_8;
        s.lanes[2].division = params::divIndex_1_4;

        SequencerEngine engine;
        engine.prepare (sampleRate);

        const auto events = run (engine, s, 4 * samplesPerStep);
        const auto ons    = only (events, noteOn);

        bool saw48 = false, saw52 = false, saw55 = false;

        for (const auto& e : ons)
        {
            if (e.number == 48) saw48 = true;
            if (e.number == 52) saw52 = true;
            if (e.number == 55) saw55 = true;
        }

        check (saw48 && saw52 && saw55, "all three lanes emit their own pitch");

        // One 1/4 = four 1/16 steps, so over four 1/16 steps lane 1 fires four times,
        // lane 2 twice and lane 3 once: seven note-ons in total.
        check (ons.size() == 7, "each lane fires on its own division, not a shared one");

        // At sample 0 every lane's first step begins together, so their note-ons must share
        // that offset -- this is the simultaneous onset the mixed mode cannot produce.
        int togetherAtZero = 0;

        for (const auto& e : ons)
            if (e.sample == 0)
                ++togetherAtZero;

        check (togetherAtZero == 3, "lanes starting together produce simultaneous note-ons");
    }

    //==========================================================================
    section ("Per-step velocity accents individual steps");
    {
        // One lane, four steps at distinct pitches so each note is identifiable, with a
        // different accent on each.
        auto s = baseSnapshot();
        s.scale = 0;
        s.velocity = 100;
        setLaneGate (s, 0, 40.0f);
        s.lanes[0].length = 4;

        for (int i = 0; i < 4; ++i)
            s.lanes[0].values[i] = (float) i / 12.0f;

        s.lanes[0].velocity[0] = 1.0f;     // 100, unity
        s.lanes[0].velocity[1] = 0.25f;    // 25
        s.lanes[0].velocity[2] = 0.6f;     // 60
        s.lanes[0].velocity[3] = 0.0f;     // clamps up to 1, never 0

        SequencerEngine engine;
        engine.prepare (sampleRate);

        const auto ons = only (run (engine, s, 4 * samplesPerStep), noteOn);

        int v[4] { -1, -1, -1, -1 };

        for (const auto& e : ons)
            if (e.number >= 48 && e.number <= 51)
                v[e.number - 48] = e.value;

        check (v[0] == 100, "an accent of 100% plays at the global velocity");
        check (v[1] == 25,  "a quiet step is scaled down on its own");
        check (v[2] == 60,  "each step scales independently of its neighbours");
        check (v[3] == 1,   "a step at 0% clamps to 1, since velocity 0 would be a note-off");
    }

    //==========================================================================
    section ("The global velocity is the ceiling");
    {
        // The step accent only ever attenuates, so with no lane trim left there is nothing
        // that can push a note above the global value.
        auto s = baseSnapshot();
        s.scale = 0;
        s.velocity = 80;
        setLaneGate (s, 0, 40.0f);
        s.lanes[0].length = 1;
        s.lanes[0].values[0] = 0.0f;
        s.lanes[0].velocity[0] = 1.0f;

        SequencerEngine engine;
        engine.prepare (sampleRate);

        const auto ons = only (run (engine, s, 2 * samplesPerStep), noteOn);

        check (! ons.empty() && ons[0].value == 80,
               "a step at full accent plays at exactly the global velocity");

        s.lanes[0].velocity[0] = 0.5f;      // step accent

        SequencerEngine scaled;
        scaled.prepare (sampleRate);

        const auto quieter = only (run (scaled, s, 2 * samplesPerStep), noteOn);

        check (! quieter.empty() && quieter[0].value == 40,
               "80 global x 50% step gives 40");
    }

    //==========================================================================
    section ("Depth still gates a poly lane, without setting its velocity");
    {
        auto s = baseSnapshot();
        s.polyMode = true;
        s.scale = 0;
        s.velocity = 100;

        for (int lane = 0; lane < params::numLanes; ++lane)
        {
            s.lanes[lane].length = 1;
            s.lanes[lane].values[0] = (float) lane / 12.0f;
            setLaneGate (s, lane, 50.0f);
        }

        s.lanes[0].depth = 1.0f;
        s.lanes[1].depth = 0.05f;   // barely in the mix, but still a full-velocity note
        s.lanes[2].depth = 0.0f;    // silent

        SequencerEngine engine;
        engine.prepare (sampleRate);

        const auto ons = only (run (engine, s, 2 * samplesPerStep), noteOn);

        int v48 = -1, v49 = -1;
        bool sawSilentLane = false;

        for (const auto& e : ons)
        {
            if (e.number == 48) v48 = e.value;
            if (e.number == 49) v49 = e.value;
            if (e.number == 50) sawSilentLane = true;
        }

        check (! sawSilentLane, "a lane at zero depth stays silent");
        check (v48 == 100 && v49 == 100,
               "depth no longer scales velocity, so a quiet mix share still plays full");
    }

    //==========================================================================
    section ("In mixed mode the triggering step supplies the velocity");
    {
        // Trigger = Any Lane, lane 2 on a slower division with its steps accented down. Its
        // notes should come out quieter than lane 1's, even though the pitch they play comes
        // from the combined mix.
        auto s = baseSnapshot();
        s.scale = 0;
        s.velocity = 100;
        s.triggerSource = params::numLanes;    // Any Lane
        s.voiceCount = 4;

        s.lanes[0].length = 1;
        s.lanes[0].division = params::divIndex_1_16;
        s.lanes[0].velocity[0] = 1.0f;
        setLaneGate (s, 0, 40.0f);

        s.lanes[1].length = 1;
        s.lanes[1].division = params::divIndex_1_4;
        s.lanes[1].velocity[0] = 0.3f;
        setLaneGate (s, 1, 40.0f);

        // Under Any Lane every lane is a trigger lane and the last one to advance at a given
        // sample wins, so lane 3 -- which runs at 1/16 by default -- has to be switched off
        // or it would claim every trigger and supply its own velocity. The instance has two
        // lanes, which takes care of lane 4 the same way.
        for (int i = 0; i < params::numSteps; ++i)
            s.lanes[2].enabled[i] = false;

        useLanes (s, 3);

        SequencerEngine engine;
        engine.prepare (sampleRate);

        const auto ons = only (run (engine, s, 4 * samplesPerStep), noteOn);

        bool sawFull = false, sawTrimmed = false;

        for (const auto& e : ons)
        {
            if (e.value == 100) sawFull = true;
            if (e.value == 30)  sawTrimmed = true;
        }

        check (sawFull,    "steps triggered by lane 1 play at its own accent");
        check (sawTrimmed, "steps triggered by lane 2 carry that step's accent instead");
    }

    //==========================================================================
    section ("Poly mode keeps each lane's voices to itself");
    {
        // Lane 1 holds a note far past its own step while lane 2 hammers away. With a shared
        // voice pool lane 2 would steal lane 1's slot; with a block per lane it cannot.
        auto s = baseSnapshot();
        s.polyMode = true;
        s.scale = 0;
        s.voiceCount = 1;

        s.lanes[0].length = 1;
        s.lanes[0].values[0] = 0.0f;             // note 48
        s.lanes[0].division = params::divIndex_1_4;
        s.lanes[0].depth = 1.0f;
        setLaneGate (s, 0, 400.0f);   // four steps long

        s.lanes[1].length = 1;
        s.lanes[1].values[0] = 9.0f / 12.0f;     // note 57
        s.lanes[1].division = params::divIndex_1_16;
        s.lanes[1].depth = 1.0f;
        setLaneGate (s, 1, 400.0f);

        SequencerEngine engine;
        engine.prepare (sampleRate);

        const auto events = run (engine, s, 4 * samplesPerStep);

        // Lane 1's note 48 starts at 0 and its gate runs four 1/4 steps, so nothing should
        // release it inside this run. Lane 2's note 57 retriggers throughout.
        int offsOf48 = 0, onsOf57 = 0;

        for (const auto& e : events)
        {
            if (e.type == noteOff && e.number == 48) ++offsOf48;
            if (e.type == noteOn  && e.number == 57) ++onsOf57;
        }

        check (offsOf48 == 0, "a fast lane never steals a slow lane's held note");
        check (onsOf57 > 1,   "the fast lane still retriggers inside its own block");
    }

    //==========================================================================
    section ("Switching the poly switch releases what was sounding");
    {
        auto s = baseSnapshot();
        s.scale = 0;
        s.voiceCount = 4;
        s.lanes[0].length = 1;
        s.lanes[0].values[0] = 0.0f;
        setLaneGate (s, 0, 400.0f);

        SequencerEngine engine;
        engine.prepare (sampleRate);

        juce::MidiBuffer buffer;
        int sounding = 0;

        for (int pos = 0; pos < 2 * blockSize; pos += blockSize)
        {
            buffer.clear();
            engine.process (s, buffer, blockSize, ppqPerSample * pos, ppqPerSample, true);

            for (const auto metadata : buffer)
            {
                if (metadata.getMessage().isNoteOn())  ++sounding;
                if (metadata.getMessage().isNoteOff()) --sounding;
            }
        }

        check (sounding == 1, "a note is held in mixed mode");

        // Flipping the switch moves voice ownership, so the held note has to be released
        // rather than abandoned in a slot the new mode does not scan.
        s.polyMode = true;
        buffer.clear();
        engine.process (s, buffer, blockSize, ppqPerSample * 2 * blockSize, ppqPerSample, true);

        int released = 0;

        for (const auto metadata : buffer)
            if (metadata.getMessage().isNoteOff())
                ++released;

        check (released >= 1, "flipping the switch releases it instead of leaving it stuck");
    }

    //==========================================================================
    std::printf ("\n%d checks, %d failed\n", checksRun, checksFailed);

    return checksFailed == 0 ? 0 : 1;
}
