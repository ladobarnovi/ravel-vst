#include "SequencerEngine.h"

namespace
{
    /** Modulo that always returns a non-negative result, so lanes keep stepping
        correctly when the host reports a negative PPQ (pre-roll, count-in).
    */
    std::int64_t positiveMod (std::int64_t a, std::int64_t b) noexcept
    {
        const std::int64_t m = a % b;
        return m < 0 ? m + b : m;
    }

    /** splitmix64 -- used to turn a step index into a repeatable pseudo-random
        choice. Hashing the timeline position instead of pulling from a running RNG
        keeps Random direction stateless, so a loop replays the same "random"
        pattern every time round rather than drifting.
    */
    std::uint64_t splitmix64 (std::uint64_t x) noexcept
    {
        x += 0x9E3779B97F4A7C15ull;
        x = (x ^ (x >> 30)) * 0xBF58476D1CE4E5B9ull;
        x = (x ^ (x >> 27)) * 0x94D049BB133111EBull;
        return x ^ (x >> 31);
    }
}

//==============================================================================
void SequencerEngine::prepare (double sampleRate)
{
    currentSampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;

    // Rate-limit CC output to roughly 1 ms so a fast lane cannot flood the
    // MIDI stream with redundant messages.
    ccIntervalSamples = juce::jmax (1, (int) std::round (currentSampleRate * 0.001));

    reset();
}

void SequencerEngine::reset()
{
    for (auto& lane : lanes)
    {
        lane.lastGlobalIndex = std::numeric_limits<std::int64_t>::min();
        lane.step = 0;
        lane.held = 0.0f;
    }

    activeNote       = -1;
    noteOffCountdown = 0;
    slewedValue      = 0.0f;
    lastCcValue      = -1;
    ccCountdown      = 0;
}

//==============================================================================
int SequencerEngine::stepIndexFor (std::int64_t globalIndex, int length, int direction, int laneIndex) noexcept
{
    if (length <= 1)
        return 0;

    switch (direction)
    {
        case 0: // Forward
            return (int) positiveMod (globalIndex, length);

        case 1: // Reverse
            return length - 1 - (int) positiveMod (globalIndex, length);

        case 2: // Ping-Pong -- 2n-2 long cycle so the endpoints aren't repeated
        {
            const std::int64_t period = 2 * length - 2;
            const std::int64_t p = positiveMod (globalIndex, period);
            return (int) (p < length ? p : period - p);
        }

        case 3: // Random
        {
            const auto h = splitmix64 ((std::uint64_t) globalIndex * 0x9E3779B97F4A7C15ull
                                       + (std::uint64_t) (laneIndex + 1) * 0xD1B54A32D192ED03ull);
            return (int) (h % (std::uint64_t) length);
        }

        default:
            return (int) positiveMod (globalIndex, length);
    }
}

//==============================================================================
void SequencerEngine::releaseHeldNote (juce::MidiBuffer& out, int sampleOffset)
{
    if (activeNote >= 0)
    {
        out.addEvent (juce::MidiMessage::noteOff (activeChannel, activeNote), sampleOffset);
        activeNote = -1;
    }

    noteOffCountdown = 0;
}

//==============================================================================
void SequencerEngine::process (const Snapshot& s,
                               juce::MidiBuffer& out,
                               int numSamples,
                               double ppqAtBlockStart,
                               double ppqPerSample,
                               bool transportRunning)
{
    // A stopped transport (or a nonsensical tempo) means no stepping. Drop any
    // held note so Live isn't left with a stuck voice.
    if (! transportRunning || ppqPerSample <= 0.0)
    {
        releaseHeldNote (out, 0);
        return;
    }

    const bool notesEnabled = (s.outputMode != params::outCC);
    const bool ccEnabled    = (s.outputMode != params::outNotes);

    // One-pole slew coefficient, computed per block rather than per sample.
    const float slewCoeff = s.slewMs <= 0.01f
                              ? 1.0f
                              : 1.0f - std::exp (-1.0f / (float) (s.slewMs * 0.001 * currentSampleRate));

    for (int n = 0; n < numSamples; ++n)
    {
        const double ppq = ppqAtBlockStart + ppqPerSample * (double) n;

        float accumulator   = 0.0f;
        bool  triggered     = false;
        double triggerStepPpq = params::divisionPpq[params::divIndex_1_16];

        //----------------------------------------------------------------------
        for (int laneIndex = 0; laneIndex < params::numLanes; ++laneIndex)
        {
            const auto& ln    = s.lanes[laneIndex];
            auto&       state = lanes[laneIndex];

            const double stepPpq = params::divisionPpq[(size_t) juce::jlimit (
                0, (int) params::divisionNames.size() - 1, ln.division)];
            const int length = juce::jlimit (1, params::numSteps, ln.length);

            // ppq / stepPpq lands a hair under an integer whenever the numbers
            // involved aren't exactly representable in binary -- ppqPerSample is
            // 1/24000 at 120bpm/48kHz -- which pushes the boundary a sample late and
            // makes step lengths alternate between 5999 and 6001 samples. The epsilon
            // is ~1000x smaller than one sample's worth of PPQ, so it can only snap a
            // value already within rounding noise of the boundary, never shift a step
            // onto the wrong sample.
            constexpr double boundaryEpsilon = 1.0e-7;

            const auto globalIndex = (std::int64_t) std::floor (ppq / stepPpq + boundaryEpsilon);
            const bool advanced    = (globalIndex != state.lastGlobalIndex);

            const int step = stepIndexFor (globalIndex, length, ln.direction, laneIndex);

            const float value   = ln.values[(size_t) step];
            const bool  stepOn  = ln.enabled[(size_t) step];

            if (advanced)
            {
                state.lastGlobalIndex = globalIndex;
                state.step = step;

                // Sample & Hold captures the chain as it stands *at this lane's*
                // clock, which is what lets a slow lane re-time faster ones.
                if (ln.mode == params::modeSampleHold && stepOn)
                    state.held = accumulator;
            }

            if (stepOn)
            {
                switch (ln.mode)
                {
                    case params::modeAdd:
                        accumulator += ln.depth * value;
                        break;

                    case params::modeMultiply:
                    {
                        // depth 0 is a no-op, depth 1 is a full multiply.
                        const float d = std::abs (ln.depth);
                        accumulator *= (1.0f - d + d * value);
                        break;
                    }

                    case params::modeMax:
                        accumulator = juce::jmax (accumulator, ln.depth * value);
                        break;

                    case params::modeSampleHold:
                        accumulator = state.held;
                        break;

                    default:
                        break;
                }
            }

            // Trigger source: a specific lane, or any lane that just advanced.
            const bool isTriggerLane = s.triggerSource >= params::numLanes
                                         || laneIndex == s.triggerSource;

            if (advanced && stepOn && isTriggerLane)
            {
                triggered      = true;
                triggerStepPpq = stepPpq;
            }
        }

        //----------------------------------------------------------------------
        const float mix = juce::jlimit (0.0f, 1.0f, accumulator + s.offset);

        slewedValue += (mix - slewedValue) * slewCoeff;

        //----------------------------------------------------------------------
        if (notesEnabled)
        {
            if (noteOffCountdown > 0 && --noteOffCountdown == 0)
                releaseHeldNote (out, n);

            if (triggered)
            {
                // Monophonic: retrigger always closes the previous note first.
                releaseHeldNote (out, n);

                const int degree   = (int) std::lround (mix * (float) s.rangeSteps);
                const int semitone = params::scaleStepToSemitone (degree, s.scale);
                const int note     = juce::jlimit (0, 127, s.root + semitone);

                activeChannel = juce::jlimit (1, 16, s.midiChannel);
                activeNote    = note;

                out.addEvent (juce::MidiMessage::noteOn (activeChannel, note,
                                                        (juce::uint8) juce::jlimit (1, 127, s.velocity)), n);

                const double stepSamples = triggerStepPpq / ppqPerSample;
                noteOffCountdown = juce::jmax (1, (int) std::lround (stepSamples * (s.gatePercent * 0.01)));
            }
        }
        else if (activeNote >= 0)
        {
            releaseHeldNote (out, n);
        }

        //----------------------------------------------------------------------
        if (ccEnabled && --ccCountdown <= 0)
        {
            ccCountdown = ccIntervalSamples;

            const int ccValue = juce::jlimit (0, 127, (int) std::lround (slewedValue * 127.0f));

            if (ccValue != lastCcValue)
            {
                lastCcValue = ccValue;
                out.addEvent (juce::MidiMessage::controllerEvent (juce::jlimit (1, 16, s.ccChannel),
                                                                 juce::jlimit (0, 127, s.ccNumber),
                                                                 ccValue), n);
            }
        }

        if (n == numSamples - 1)
            uiMix.store (mix, std::memory_order_relaxed);
    }

    for (int laneIndex = 0; laneIndex < params::numLanes; ++laneIndex)
        uiStep[laneIndex].store (lanes[laneIndex].step, std::memory_order_relaxed);
}
