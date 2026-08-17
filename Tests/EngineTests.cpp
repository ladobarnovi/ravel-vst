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

            for (int i = 0; i < params::numSteps; ++i)
                lane.chance[i] = 1.0f;

            lane.length    = params::numSteps;
            lane.division  = params::divIndex_1_16;
            lane.direction = 0;
            lane.depth     = 0.0f;
            lane.mode      = params::modeAdd;
            lane.nudge     = 0.0f;
            lane.humanize  = 0.0f;
            lane.ccOn      = false;
        }

        s.swing = 0.0f;

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
        s.pitchMode     = params::pitchSemitone;
        s.bendRange     = 2;

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

        check (onNoteChannel, "semitone mode uses the Note Channel parameter");
    }

    //==========================================================================
    section ("Continuous pitch via MPE");
    {
        auto s = baseSnapshot();
        s.lanes[0].length = 1;
        s.lanes[0].values[0] = 0.3f;   // 0.3 * 12 = 3.6 semitones above root 48 -> 51.6
        s.scale      = 0;              // Chromatic, so degrees are plain semitones
        s.rangeSteps = 12;
        s.root       = 48;
        s.pitchMode  = params::pitchMpe;
        s.bendRange  = 2;

        SequencerEngine engine;
        engine.prepare (sampleRate);

        const auto events = run (engine, s, 4 * samplesPerStep);
        const auto ons   = only (events, noteOn);
        const auto bends = only (events, pitchBend);

        check (! ons.empty(), "continuous mode still fires notes");
        check (bends.size() >= ons.size(), "every note is preceded by a pitch bend");

        // The real invariant: note number plus bend must reconstruct the fractional pitch.
        bool reconstructs = false;

        if (! ons.empty())
        {
            for (const auto& bend : bends)
            {
                if (bend.sample == ons[0].sample && bend.channel == ons[0].channel)
                {
                    const double pitch = (double) ons[0].number
                                       + ((double) bend.value - 8192.0) / 8191.0 * (double) s.bendRange;

                    reconstructs = std::abs (pitch - 51.6) < 0.01;
                }
            }
        }

        check (reconstructs, "note + bend reconstructs 51.6 semitones");

        bool onMemberChannels = ! ons.empty();

        for (const auto& e : ons)
            onMemberChannels = onMemberChannels && e.channel >= 2 && e.channel <= 16;

        check (onMemberChannels, "notes are sent on MPE member channels (2-16)");
        check (ons.size() >= 2 && ons[0].channel != ons[1].channel,
               "consecutive notes rotate across member channels");
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
        s.pitchMode  = params::pitchMpe;

        SequencerEngine engine;
        engine.prepare (sampleRate);

        const auto events = run (engine, s, 2 * samplesPerStep);
        const auto ons   = only (events, noteOn);
        const auto bends = only (events, pitchBend);

        check (! ons.empty() && ons[0].number == 51, "exact degree gives note 51");

        bool centred = false;

        for (const auto& bend : bends)
            if (! ons.empty() && bend.sample == ons[0].sample && bend.channel == ons[0].channel)
                centred = std::abs (bend.value - 8192) <= 1;

        check (centred, "bend is centred when no fractional part is needed");
    }

    //==========================================================================
    section ("MPE configuration message");
    {
        auto s = baseSnapshot();
        s.pitchMode = params::pitchMpe;
        s.bendRange = 12;

        SequencerEngine engine;
        engine.prepare (sampleRate);

        juce::MidiBuffer buffer;
        engine.process (s, buffer, 512, 0.0, ppqPerSample, true);

        bool sawZone = false;
        bool sawBendRange = false;
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
                if (currentRpn == params::mpeZoneRpn
                    && message.getControllerValue() == params::mpeMemberChannels
                    && message.getChannel() == 1)
                    sawZone = true;

                if (currentRpn == params::pitchBendRangeRpn
                    && message.getControllerValue() == 12
                    && message.getChannel() == 2)
                    sawBendRange = true;
            }
        }

        check (sawZone, "RPN 6 declares 15 member channels on the master channel");
        check (sawBendRange, "RPN 0 transmits the 12-semitone per-note bend range on channel 2");
    }

    //==========================================================================
    section ("Scale is bypassed in continuous modes");
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
            s.pitchMode  = params::pitchBend;
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
        s.pitchMode  = params::pitchBend;
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
        s.pitchMode   = params::pitchBend;
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
    section ("Pitch bend mode sends no MPE zone message");
    {
        auto s = baseSnapshot();
        s.pitchMode   = params::pitchBend;
        s.bendRange   = 7;
        s.midiChannel = 3;

        SequencerEngine engine;
        engine.prepare (sampleRate);

        juce::MidiBuffer buffer;
        engine.process (s, buffer, 512, 0.0, ppqPerSample, true);

        bool sawZoneRpn = false;
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
                if (currentRpn == params::mpeZoneRpn)
                    sawZoneRpn = true;

                if (currentRpn == params::pitchBendRangeRpn
                    && message.getControllerValue() == 7
                    && message.getChannel() == 3)
                    sawRangeOnNoteChannel = true;
            }
        }

        check (! sawZoneRpn, "no MPE configuration message is sent in pitch bend mode");
        check (sawRangeOnNoteChannel, "bend range RPN 0 goes to the Note Chan");
    }

    //==========================================================================
    section ("Continuous mapping is linear across a range of values");
    {
        auto s = baseSnapshot();
        s.lanes[0].length = 4;
        s.scale      = 0;      // Chromatic
        s.rangeSteps = 24;
        s.root       = 36;
        s.pitchMode  = params::pitchBend;
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
    section ("Switching away from MPE clears the zone");
    {
        auto s = baseSnapshot();
        s.pitchMode = params::pitchMpe;

        SequencerEngine engine;
        engine.prepare (sampleRate);

        juce::MidiBuffer buffer;
        engine.process (s, buffer, 512, 0.0, ppqPerSample, true);

        // Now switch to pitch bend mode and check the zone gets torn down.
        s.pitchMode = params::pitchBend;
        buffer.clear();
        engine.process (s, buffer, 512, ppqPerSample * 512, ppqPerSample, true);

        bool clearedZone = false;
        int currentRpn = -1;

        for (const auto metadata : buffer)
        {
            const auto message = metadata.getMessage();

            if (! message.isController())
                continue;

            if (message.getControllerNumber() == 0x64)
                currentRpn = message.getControllerValue();

            if (message.getControllerNumber() == 0x06
                && currentRpn == params::mpeZoneRpn
                && message.getControllerValue() == 0)
                clearedZone = true;
        }

        check (clearedZone, "leaving MPE sends RPN 6 with 0 member channels");
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
    std::printf ("\n%d checks, %d failed\n", checksRun, checksFailed);

    return checksFailed == 0 ? 0 : 1;
}
