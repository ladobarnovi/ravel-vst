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

    /** Deterministic 0..1 draw from a timeline position. Used for probability and
        humanize, so both are functions of *where* you are rather than of a running RNG --
        a loop replays identically instead of drifting.
    */
    float hashToUnitFloat (std::int64_t globalIndex, int laneIndex, std::uint64_t salt) noexcept
    {
        const auto h = splitmix64 ((std::uint64_t) globalIndex * 0x9E3779B97F4A7C15ull
                                   + (std::uint64_t) (laneIndex + 1) * 0xD1B54A32D192ED03ull
                                   + salt * 0xA24BAED4963EE407ull);

        return (float) ((double) (h >> 11) / 9007199254740992.0);   // 2^53
    }

    constexpr std::uint64_t probabilitySalt = 31;
    constexpr std::uint64_t humanizeSalt    = 97;

    /** One RPN as three controller messages, matching the byte order JUCE's
        MidiRPNGenerator produces: parameter LSB, parameter MSB, then data entry MSB.
        Data entry LSB is only required for 14-bit values, which none of these are.
    */
    void addRpn (juce::MidiBuffer& out, int sampleOffset, int channel, int rpnNumber, int value)
    {
        out.addEvent (juce::MidiMessage::controllerEvent (channel, 0x64, rpnNumber & 0x7f), sampleOffset);
        out.addEvent (juce::MidiMessage::controllerEvent (channel, 0x65, rpnNumber >> 7),    sampleOffset);
        out.addEvent (juce::MidiMessage::controllerEvent (channel, 0x06, value),             sampleOffset);
    }
}

//==============================================================================
void SequencerEngine::sendMpeConfiguration (juce::MidiBuffer& out, int sampleOffset, int bendRange)
{
    // Same three-part sequence as MPEMessages::setLowerZone().
    addRpn (out, sampleOffset, params::mpeMasterChannel, params::mpeZoneRpn, params::mpeMemberChannels);
    addRpn (out, sampleOffset, params::mpeMasterChannel + 1, params::pitchBendRangeRpn, bendRange);
    addRpn (out, sampleOffset, params::mpeMasterChannel, params::pitchBendRangeRpn, 2);
}

void SequencerEngine::clearMpeZone (juce::MidiBuffer& out, int sampleOffset)
{
    addRpn (out, sampleOffset, params::mpeMasterChannel, params::mpeZoneRpn, 0);
}

void SequencerEngine::sendPitchBendRange (juce::MidiBuffer& out, int sampleOffset,
                                          int channel, int bendRange)
{
    addRpn (out, sampleOffset, channel, params::pitchBendRangeRpn, bendRange);
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

    for (auto& voice : voices)
    {
        voice.note = -1;
        voice.samplesRemaining = 0;
    }

    slewedValue      = 0.0f;
    lastCcValue      = -1;
    ccCountdown      = 0;

    configuredMode      = -1;
    configuredBendRange = -1;
    configuredChannel   = -1;
    memberChannelIndex  = 0;
    configuredPolyMode  = -1;

    for (int lane = 0; lane < params::numLanes; ++lane)
    {
        laneHeldValue[lane]   = 0.0f;
        laneSlewedValue[lane] = 0.0f;
        laneLastCcValue[lane] = -1;
    }
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
float SequencerEngine::timingOffsetFor (std::int64_t globalIndex, const LaneSnapshot& lane,
                                        float swing, int laneIndex) noexcept
{
    float offset = lane.nudge * 0.5f;

    // Swing delays every other step of the absolute grid, so it stays anchored to the
    // bar rather than to where a short pattern happens to have started.
    if (positiveMod (globalIndex, 2) != 0)
        offset += swing * 0.5f;

    if (lane.humanize > 0.0f)
    {
        const auto draw = hashToUnitFloat (globalIndex, laneIndex, humanizeSalt);
        offset += (draw * 2.0f - 1.0f) * lane.humanize * 0.5f;
    }

    return juce::jlimit (-0.49f, 0.49f, offset);
}

std::int64_t SequencerEngine::resolveGlobalIndex (double ppq, double stepPpq,
                                                 const LaneSnapshot& lane,
                                                 float swing, int laneIndex) noexcept
{
    // See the note on boundaryEpsilon below: ppq / stepPpq lands a hair under an integer
    // when the numbers aren't exactly representable in binary.
    constexpr double boundaryEpsilon = 1.0e-7;

    const auto rawIndex = (std::int64_t) std::floor (ppq / stepPpq + boundaryEpsilon);

    const bool shifted = lane.nudge != 0.0f || lane.humanize > 0.0f || swing != 0.0f;

    if (! shifted)
        return rawIndex;

    const double tolerance = stepPpq * boundaryEpsilon;

    // Largest candidate whose shifted boundary has been reached. Offsets are bounded to
    // half a step, so the answer is always within one of the unshifted index.
    for (auto candidate = rawIndex + 1; candidate >= rawIndex - 1; --candidate)
    {
        const double boundary = ((double) candidate
                                 + (double) timingOffsetFor (candidate, lane, swing, laneIndex))
                              * stepPpq;

        if (ppq + tolerance >= boundary)
            return candidate;
    }

    return rawIndex - 1;
}

//==============================================================================
bool SequencerEngine::anyVoiceActive() const noexcept
{
    for (const auto& voice : voices)
        if (voice.note >= 0)
            return true;

    return false;
}

void SequencerEngine::releaseAllVoices (juce::MidiBuffer& out, int sampleOffset)
{
    for (auto& voice : voices)
    {
        if (voice.note >= 0)
            out.addEvent (juce::MidiMessage::noteOff (voice.channel, voice.note), sampleOffset);

        voice.note = -1;
        voice.samplesRemaining = 0;
    }
}

void SequencerEngine::advanceVoices (juce::MidiBuffer& out, int sampleOffset)
{
    for (auto& voice : voices)
    {
        if (voice.note < 0)
            continue;

        if (--voice.samplesRemaining <= 0)
        {
            out.addEvent (juce::MidiMessage::noteOff (voice.channel, voice.note), sampleOffset);
            voice.note = -1;
            voice.samplesRemaining = 0;
        }
    }
}

bool SequencerEngine::slotIsOwned (int slot, int voiceLimit, bool polyMode) noexcept
{
    const int limit = juce::jlimit (1, voicesPerLane, voiceLimit);

    // Poly gives every lane a fixed block of voicesPerLane slots and uses the first
    // `limit` of each; mono only ever uses the first block.
    return polyMode ? (slot % voicesPerLane) < limit
                    : slot < limit;
}

void SequencerEngine::retireUnownedVoices (juce::MidiBuffer& out, int sampleOffset,
                                           int voiceLimit, bool polyMode)
{
    for (int i = 0; i < maxVoices; ++i)
    {
        if (voices[i].note < 0 || slotIsOwned (i, voiceLimit, polyMode))
            continue;

        out.addEvent (juce::MidiMessage::noteOff (voices[i].channel, voices[i].note), sampleOffset);
        voices[i].note = -1;
        voices[i].samplesRemaining = 0;
    }
}

int SequencerEngine::allocateVoice (juce::MidiBuffer& out, int sampleOffset,
                                    int note, int channel, int begin, int end)
{
    const int first = juce::jlimit (0, maxVoices - 1, begin);
    const int last  = juce::jlimit (first + 1, maxVoices, end);

    // Same pitch already sounding on the same channel: MIDI cannot tell two identical
    // notes apart, so one note-off would silence both. Reuse that voice instead.
    for (int i = first; i < last; ++i)
    {
        if (voices[i].note == note && voices[i].channel == channel)
        {
            out.addEvent (juce::MidiMessage::noteOff (channel, note), sampleOffset);
            return i;
        }
    }

    for (int i = first; i < last; ++i)
        if (voices[i].note < 0)
            return i;

    // All busy -- steal whichever is closest to finishing, as the least audible loss.
    int stolen = first;

    for (int i = first + 1; i < last; ++i)
        if (voices[i].samplesRemaining < voices[stolen].samplesRemaining)
            stolen = i;

    out.addEvent (juce::MidiMessage::noteOff (voices[stolen].channel, voices[stolen].note), sampleOffset);
    return stolen;
}

//==============================================================================
SequencerEngine::PitchResult SequencerEngine::pitchFor (float value, const Snapshot& s,
                                                       int noteChannel, int bendRange)
{
    PitchResult result;

    if (params::isContinuousPitch (s.pitchMode))
    {
        // Raw microtonal pitch: Range is semitones and the scale is bypassed. Nearest
        // semitone carries the note number; the residual (at most half a semitone) is
        // expressed as pitch bend.
        const float absolute = (float) s.root + params::continuousSemitones (value, s.rangeSteps);

        result.note = juce::jlimit (0, 127, (int) std::lround (absolute));

        if (s.pitchMode == params::pitchMpe)
        {
            // Rotate member channels so a new note's bend can't pull the pitch of one that
            // is still sounding. This is also what makes polyphonic microtonal pitch
            // possible at all, in poly mode as much as with an overlapping gate.
            result.channel = params::mpeMasterChannel + 1 + memberChannelIndex;
            memberChannelIndex = (memberChannelIndex + 1) % params::mpeMemberChannels;
        }
        else
        {
            // Single channel: survives hosts that merge MIDI channels when routing between
            // tracks, but the bend is shared by every voice on it, so overlapping notes --
            // including three poly lanes at once -- cannot hold different microtones.
            result.channel = noteChannel;
        }

        const float residual   = absolute - (float) result.note;
        const float normalised = juce::jlimit (-1.0f, 1.0f, residual / (float) bendRange);

        result.bend = juce::jlimit (0, 16383, 8192 + (int) std::lround (normalised * 8191.0f));
    }
    else
    {
        const int degree   = (int) std::lround (value * (float) s.rangeSteps);
        const int semitone = params::scaleStepToSemitone (degree, s.scale);

        result.note    = juce::jlimit (0, 127, s.root + semitone);
        result.channel = noteChannel;
    }

    return result;
}

int SequencerEngine::velocityFor (const Snapshot& s, int laneIndex, int stepIndex) noexcept
{
    const auto& ln = s.lanes[(size_t) juce::jlimit (0, params::numLanes - 1, laneIndex)];
    const float step = ln.velocity[(size_t) juce::jlimit (0, params::numSteps - 1, stepIndex)];

    // Three multiplied trims: the global Velocity is the master, the lane balances against
    // its neighbours, and the step carries the accent. All default to unity, so the whole
    // chain collapses to the bare global value.
    const float scaled = (float) s.velocity * ln.velocityScale * step;

    // Clamped to 1 rather than 0: a MIDI note-on at velocity 0 is a note-off, so a step
    // pulled to 0% plays as quietly as MIDI allows instead of silently retiring itself.
    return juce::jlimit (1, 127, (int) std::lround (scaled));
}

void SequencerEngine::startNote (juce::MidiBuffer& out, int sampleOffset, const PitchResult& pitch,
                                 int velocity, int gateSamples, int begin, int end)
{
    // Allocate first: if this steals or reuses a voice, its note-off has to be ordered
    // ahead of the bend and note-on that follow at this same sample.
    const int slot = allocateVoice (out, sampleOffset, pitch.note, pitch.channel, begin, end);

    if (pitch.bend >= 0)
        out.addEvent (juce::MidiMessage::pitchWheel (pitch.channel, pitch.bend), sampleOffset);

    out.addEvent (juce::MidiMessage::noteOn (pitch.channel, pitch.note,
                                            (juce::uint8) juce::jlimit (1, 127, velocity)),
                  sampleOffset);

    voices[slot].note             = pitch.note;
    voices[slot].channel          = pitch.channel;
    voices[slot].samplesRemaining = juce::jmax (1, gateSamples);
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
        releaseAllVoices (out, 0);
        return;
    }

    const int voiceLimit = juce::jlimit (1, voicesPerLane, s.voiceCount);

    // The two modes lay their voices out differently, so a note sounding across the switch
    // would be left in a slot the new mode does not consider its own -- and never released.
    if (const int polyFlag = s.polyMode ? 1 : 0; configuredPolyMode != polyFlag)
    {
        releaseAllVoices (out, 0);
        configuredPolyMode = polyFlag;
    }

    // If the voice count was turned down, anything sounding outside the new limit has to
    // be let go or it would hang forever.
    retireUnownedVoices (out, 0, voiceLimit, s.polyMode);

    const bool notesEnabled = (s.outputMode != params::outCC);
    const bool ccEnabled    = (s.outputMode != params::outNotes);

    const int noteChannel = juce::jlimit (1, 16, s.midiChannel);
    const int bendRange   = juce::jlimit (1, 48, s.bendRange);

    // The receiving instrument has to be told the bend range -- the MPE default is +/-48
    // semitones, so an instrument left at that default while we scale for +/-2 plays 24x
    // the intended interval.
    if (notesEnabled)
    {
        const bool changed = configuredMode != s.pitchMode
                          || configuredBendRange != bendRange
                          || (s.pitchMode == params::pitchBend && configuredChannel != noteChannel);

        if (changed)
        {
            // Don't leave the instrument stuck in MPE mode after switching away from it.
            if (configuredMode == params::pitchMpe && s.pitchMode != params::pitchMpe)
                clearMpeZone (out, 0);

            if (s.pitchMode == params::pitchMpe)
                sendMpeConfiguration (out, 0, bendRange);
            else if (s.pitchMode == params::pitchBend)
                sendPitchBendRange (out, 0, noteChannel, bendRange);

            configuredMode      = s.pitchMode;
            configuredBendRange = bendRange;
            configuredChannel   = noteChannel;
        }
    }

    // One-pole slew coefficient, computed per block rather than per sample.
    const float slewCoeff = s.slewMs <= 0.01f
                              ? 1.0f
                              : 1.0f - std::exp (-1.0f / (float) (s.slewMs * 0.001 * currentSampleRate));

    for (int n = 0; n < numSamples; ++n)
    {
        const double ppq = ppqAtBlockStart + ppqPerSample * (double) n;

        float accumulator   = 0.0f;
        bool  triggered     = false;
        int   triggerLane   = 0;
        int   triggerStep   = 0;
        double triggerStepPpq = params::divisionPpq[params::divIndex_1_16];

        // Poly mode: each lane's own trigger, collected here rather than emitted inside the
        // lane loop, because advanceVoices() below has to order its expiring note-offs
        // ahead of any note-on landing on this same sample.
        bool   laneTriggered[params::numLanes] {};
        float  laneTriggerValue[params::numLanes] {};
        int    laneTriggerStep[params::numLanes] {};
        double laneTriggerStepPpq[params::numLanes] {};

        //----------------------------------------------------------------------
        for (int laneIndex = 0; laneIndex < params::numLanes; ++laneIndex)
        {
            const auto& ln    = s.lanes[laneIndex];
            auto&       state = lanes[laneIndex];

            const double stepPpq = params::divisionPpq[(size_t) juce::jlimit (
                0, (int) params::divisionNames.size() - 1, ln.division)];
            const int length = juce::jlimit (1, params::numSteps, ln.length);

            // Note on the epsilon inside resolveGlobalIndex: ppq / stepPpq lands a hair
            // under an integer whenever the numbers aren't exactly representable in binary
            // -- ppqPerSample is 1/24000 at 120bpm/48kHz -- which pushed boundaries a
            // sample late and made step lengths alternate between 5999 and 6001 samples.
            const auto globalIndex = resolveGlobalIndex (ppq, stepPpq, ln, s.swing, laneIndex);
            const bool advanced    = (globalIndex != state.lastGlobalIndex);

            const int step = stepIndexFor (globalIndex, length, ln.direction, laneIndex);

            const float value  = ln.values[(size_t) step];
            const float chance = ln.chance[(size_t) step];

            // A step that loses its probability roll behaves exactly like a step that is
            // switched off: transparent for the mix, and it fires nothing. The roll is a
            // pure function of the timeline position, so it holds steady for the whole
            // step and repeats identically next time round the loop.
            bool stepOn = ln.enabled[(size_t) step];

            if (stepOn && chance < 0.999f)
                stepOn = hashToUnitFloat (globalIndex, laneIndex, probabilitySalt) < chance;

            if (advanced)
            {
                state.lastGlobalIndex = globalIndex;
                state.step = step;

                // Sample & Hold captures the chain as it stands *at this lane's*
                // clock, which is what lets a slow lane re-time faster ones.
                if (ln.mode == params::modeSampleHold && stepOn)
                    state.held = accumulator;

                // Per-lane CC follows the lane's own step value, independent of Depth --
                // Depth governs the lane's share of the mix, not its own output. Inactive
                // steps latch the previous level instead of dropping to zero.
                if (stepOn)
                    laneHeldValue[laneIndex] = value;
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

            if (s.polyMode)
            {
                // Every lane is its own voice, so Trigger has nothing to select and a lane
                // at zero Depth is simply silent -- which keeps the stock preset, where only
                // lane 1 has Depth, sounding as one voice until another is dialled up.
                if (advanced && stepOn && std::abs (ln.depth) > 1.0e-6f)
                {
                    laneTriggered[laneIndex]      = true;
                    laneTriggerValue[laneIndex]   = value;
                    laneTriggerStep[laneIndex]    = step;
                    laneTriggerStepPpq[laneIndex] = stepPpq;
                }
            }
            else
            {
                // Trigger source: a specific lane, or any lane that just advanced.
                const bool isTriggerLane = s.triggerSource >= params::numLanes
                                             || laneIndex == s.triggerSource;

                if (advanced && stepOn && isTriggerLane)
                {
                    triggered      = true;
                    triggerLane    = laneIndex;
                    triggerStep    = step;
                    triggerStepPpq = stepPpq;
                }
            }
        }

        //----------------------------------------------------------------------
        const float mix = juce::jlimit (0.0f, 1.0f, accumulator + s.offset);

        slewedValue += (mix - slewedValue) * slewCoeff;

        if (ccEnabled)
            for (int laneIndex = 0; laneIndex < params::numLanes; ++laneIndex)
                laneSlewedValue[laneIndex] += (laneHeldValue[laneIndex] - laneSlewedValue[laneIndex])
                                            * slewCoeff;

        //----------------------------------------------------------------------
        if (notesEnabled)
        {
            advanceVoices (out, n);

            const auto gateSamplesFor = [&] (double stepPpq)
            {
                return (int) std::lround ((stepPpq / ppqPerSample) * (s.gatePercent * 0.01));
            };

            if (s.polyMode)
            {
                for (int laneIndex = 0; laneIndex < params::numLanes; ++laneIndex)
                {
                    if (! laneTriggered[laneIndex])
                        continue;

                    const auto& ln = s.lanes[laneIndex];

                    // The lane's own value drives its pitch, and Offset applies to it the
                    // same way it applies to the mix in the other mode.
                    const float value = juce::jlimit (0.0f, 1.0f,
                                                      laneTriggerValue[laneIndex] + s.offset);

                    const auto pitch = pitchFor (value, s, noteChannel, bendRange);

                    const int begin = laneIndex * voicesPerLane;

                    startNote (out, n, pitch,
                               velocityFor (s, laneIndex, laneTriggerStep[laneIndex]),
                               gateSamplesFor (laneTriggerStepPpq[laneIndex]),
                               begin, begin + voiceLimit);
                }
            }
            else if (triggered)
            {
                const auto pitch = pitchFor (mix, s, noteChannel, bendRange);

                // The note belongs to whichever step of whichever lane triggered it, so that
                // step's accent applies even though the pitch came from the combined mix.
                startNote (out, n, pitch, velocityFor (s, triggerLane, triggerStep),
                           gateSamplesFor (triggerStepPpq), 0, voiceLimit);
            }
        }
        else if (anyVoiceActive())
        {
            releaseAllVoices (out, n);
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

            // Each lane can also drive its own destination, so one instance can modulate
            // several parameters rather than only the combined mix.
            for (int laneIndex = 0; laneIndex < params::numLanes; ++laneIndex)
            {
                const auto& ln = s.lanes[laneIndex];

                if (! ln.ccOn)
                    continue;

                const int laneCc = juce::jlimit (0, 127,
                                                 (int) std::lround (laneSlewedValue[laneIndex] * 127.0f));

                if (laneCc != laneLastCcValue[laneIndex])
                {
                    laneLastCcValue[laneIndex] = laneCc;
                    out.addEvent (juce::MidiMessage::controllerEvent (juce::jlimit (1, 16, ln.ccChannel),
                                                                     juce::jlimit (0, 127, ln.ccNumber),
                                                                     laneCc), n);
                }
            }
        }

        if (n == numSamples - 1)
            uiMix.store (mix, std::memory_order_relaxed);
    }

    for (int laneIndex = 0; laneIndex < params::numLanes; ++laneIndex)
        uiStep[laneIndex].store (lanes[laneIndex].step, std::memory_order_relaxed);
}
