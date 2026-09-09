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

    /** ppq / stepPpq lands a hair under an integer whenever the numbers are not exactly
        representable in binary -- ppqPerSample is 1/24000 at 120bpm/48kHz -- which pushed
        boundaries a sample late and made step lengths alternate between 5999 and 6001
        samples.

        Shared by resolveGlobalIndex() and by the boundary prediction that decides when to
        call it. Those two have to agree to the last bit about where a boundary sits: the
        prediction is only worth having because it is exact, and a second copy of this number
        is how it would quietly stop being.
    */
    constexpr double boundaryEpsilon = 1.0e-7;

    /** One RPN as a complete, self-closing transaction: parameter LSB, parameter MSB, data
        entry MSB, data entry LSB, then the Null RPN that deselects it again.

        The data entry LSB is not needed to carry any value we send -- none of these are
        14-bit -- but a receiver latches MSB and LSB independently, so a stale fine value
        left over from whatever RPN it handled last would ride along with ours. Sending an
        explicit zero is what makes "48" mean 48 semitones and not 48-and-a-bit.

        The Null RPN at the end matters more. Without it the parameter stays selected on that
        channel forever, and every later CC 6 or CC 38 on it is swallowed as data for this
        RPN rather than reaching the instrument as itself. On a plugin whose whole job is
        sending CCs -- where any lane can be pointed at CC 6 -- that turns a routine
        modulation into a silent rewrite of the instrument's pitch bend range.
    */
    void addRpn (juce::MidiBuffer& out, int sampleOffset, int channel, int rpnNumber, int value)
    {
        out.addEvent (juce::MidiMessage::controllerEvent (channel, 0x64, rpnNumber & 0x7f), sampleOffset);
        out.addEvent (juce::MidiMessage::controllerEvent (channel, 0x65, rpnNumber >> 7),    sampleOffset);
        out.addEvent (juce::MidiMessage::controllerEvent (channel, 0x06, value),             sampleOffset);
        out.addEvent (juce::MidiMessage::controllerEvent (channel, 0x26, 0),                 sampleOffset);

        // 127 in both parameter bytes is the Null RPN: "nothing is selected now".
        out.addEvent (juce::MidiMessage::controllerEvent (channel, 0x64, 0x7f), sampleOffset);
        out.addEvent (juce::MidiMessage::controllerEvent (channel, 0x65, 0x7f), sampleOffset);
    }
}

//==============================================================================
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
    for (auto* states : { noteLaneStates, ccLaneStates })
        for (int lane = 0; lane < params::numLanes; ++lane)
        {
            states[lane].lastGlobalIndex = std::numeric_limits<std::int64_t>::min();
            states[lane].step = 0;
        }

    for (auto& voice : voices)
    {
        voice.note = -1;
        voice.samplesRemaining = 0;
    }

    for (auto& mc : mpeChannels)
    {
        mc.voiceSlot = -1;
        mc.rangeSent = false;
    }

    slewedValue      = 0.0f;
    lastCcValue      = -1;
    ccCountdown      = 0;

    configuredMode      = -1;
    configuredBendRange = -1;
    configuredChannel   = -1;
    configuredPolyMode  = -1;

    configuredMpeOn         = false;
    configuredMpeWantedMode = -1;
    configuredMpeBendRange  = -1;
    configuredMpeFlag       = -1;

    for (int lane = 0; lane < params::numLanes; ++lane)
    {
        ccLaneHeldValue[lane]   = 0.0f;
        ccLaneSlewedValue[lane] = 0.0f;
        ccLaneLastCcValue[lane] = -1;
    }

    publishUiSteps (false);
}

//==============================================================================
void SequencerEngine::publishUiSteps (bool running) noexcept
{
    for (int lane = 0; lane < params::numLanes; ++lane)
    {
        noteUiStep[lane].store (running ? noteLaneStates[lane].step : noStep,
                                std::memory_order_relaxed);
        ccUiStep[lane].store   (running ? ccLaneStates[lane].step   : noStep,
                                std::memory_order_relaxed);
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
float SequencerEngine::timingOffsetFor (std::int64_t globalIndex, float swing) noexcept
{
    // Swing delays every other step of the absolute grid, so it stays anchored to the
    // bar rather than to where a short pattern happens to have started.
    if (positiveMod (globalIndex, 2) == 0)
        return 0.0f;

    return juce::jlimit (-0.49f, 0.49f, swing * 0.5f);
}

std::int64_t SequencerEngine::resolveGlobalIndex (double ppq, double stepPpq,
                                                 float swing) noexcept
{
    const auto rawIndex = (std::int64_t) std::floor (ppq / stepPpq + boundaryEpsilon);

    if (swing == 0.0f)
        return rawIndex;

    const double tolerance = stepPpq * boundaryEpsilon;

    // Largest candidate whose shifted boundary has been reached. Offsets are bounded to
    // half a step, so the answer is always within one of the unshifted index.
    for (auto candidate = rawIndex + 1; candidate >= rawIndex - 1; --candidate)
    {
        const double boundary = ((double) candidate
                                 + (double) timingOffsetFor (candidate, swing)) * stepPpq;

        if (ppq + tolerance >= boundary)
            return candidate;
    }

    return rawIndex - 1;
}

int SequencerEngine::nextBoundarySample (std::int64_t currentIndex,
                                        double ppqAtBlockStart, double ppqPerSample,
                                        double stepPpq, float swing,
                                        int from, int numSamples) noexcept
{
    const double tolerance = stepPpq * boundaryEpsilon;

    // resolveGlobalIndex() leaves currentIndex behind as soon as ppq + tolerance reaches the
    // next shifted boundary, and ppq is affine in the sample number -- so inverting that one
    // inequality names the crossing sample outright, rather than testing it six thousand
    // times to find it.
    const double boundary = ((double) (currentIndex + 1)
                              + (double) timingOffsetFor (currentIndex + 1, swing)) * stepPpq;

    const double crossing = std::ceil ((boundary - tolerance - ppqAtBlockStart) / ppqPerSample);

    // Clamped as a double before the cast: a boundary far outside this block would otherwise
    // overflow the conversion rather than simply land past its end.
    int n = (int) juce::jlimit ((double) (from + 1), (double) numSamples, crossing);

    const auto indexAt = [&] (int sample)
    {
        return resolveGlobalIndex (ppqAtBlockStart + ppqPerSample * (double) sample, stepPpq, swing);
    };

    // The division above can land a unit either side of the true crossing when the numbers are
    // not exactly representable, so the answer is confirmed against the very function the old
    // per-sample loop called. Both loops correct by a sample or two -- they are a correction,
    // not a search, and that is what makes this a refactor rather than a reimplementation.
    while (n > from + 1 && indexAt (n - 1) != currentIndex)
        --n;

    while (n < numSamples && indexAt (n) == currentIndex)
        ++n;

    return n;
}

//==============================================================================
bool SequencerEngine::anyVoiceActive() const noexcept
{
    for (const auto& voice : voices)
        if (voice.note >= 0)
            return true;

    return false;
}

void SequencerEngine::releaseVoice (juce::MidiBuffer& out, int sampleOffset, int slot)
{
    if (voices[slot].note < 0)
        return;

    out.addEvent (juce::MidiMessage::noteOff (voices[slot].channel, voices[slot].note), sampleOffset);

    // If this slot was sounding on a pooled MPE member channel, free that channel back to the
    // pool too -- otherwise a retired voice would leave its channel permanently marked busy.
    const int memberIndex = voices[slot].channel - mpeMemberChannelBase;

    if (memberIndex >= 0 && memberIndex < mpeMemberChannels
        && mpeChannels[memberIndex].voiceSlot == slot)
        mpeChannels[memberIndex].voiceSlot = -1;

    voices[slot].note             = -1;
    voices[slot].samplesRemaining = 0;
}

void SequencerEngine::releaseAllVoices (juce::MidiBuffer& out, int sampleOffset)
{
    for (int i = 0; i < maxVoices; ++i)
        releaseVoice (out, sampleOffset, i);
}

void SequencerEngine::advanceVoices (juce::MidiBuffer& out, int sampleOffset, int samples)
{
    for (int i = 0; i < maxVoices; ++i)
    {
        if (voices[i].note < 0)
            continue;

        voices[i].samplesRemaining -= samples;

        if (voices[i].samplesRemaining <= 0)
            releaseVoice (out, sampleOffset, i);
    }
}

int SequencerEngine::soonestVoiceExpiry() const noexcept
{
    int soonest = std::numeric_limits<int>::max();

    for (const auto& voice : voices)
        if (voice.note >= 0)
            soonest = juce::jmin (soonest, voice.samplesRemaining);

    return soonest;
}

void SequencerEngine::settleVoices (int samples) noexcept
{
    if (samples <= 0)
        return;

    for (auto& voice : voices)
        if (voice.note >= 0)
            voice.samplesRemaining -= samples;
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

        releaseVoice (out, sampleOffset, i);
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
            releaseVoice (out, sampleOffset, i);
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

    releaseVoice (out, sampleOffset, stolen);
    return stolen;
}

int SequencerEngine::allocateMpeChannel (juce::MidiBuffer& out, int sampleOffset)
{
    for (int i = 0; i < mpeMemberChannels; ++i)
        if (mpeChannels[i].voiceSlot < 0)
            return i;

    // Every member channel is already sounding a note. Steal whichever backing voice is
    // closest to finishing anyway -- the same "least audible loss" rule allocateVoice() uses
    // when it runs out of slots.
    int stolen = 0;

    for (int i = 1; i < mpeMemberChannels; ++i)
        if (voices[mpeChannels[i].voiceSlot].samplesRemaining
              < voices[mpeChannels[stolen].voiceSlot].samplesRemaining)
            stolen = i;

    releaseVoice (out, sampleOffset, mpeChannels[stolen].voiceSlot);   // also frees the channel
    return stolen;
}

//==============================================================================
SequencerEngine::PitchResult SequencerEngine::pitchFor (float value, const Snapshot& s,
                                                       int bendRange) noexcept
{
    PitchResult result;

    if (s.quantize)
    {
        // Range is octaves; the scale converts that to degrees so the same Range value spans
        // the same musical distance whether the scale packs 5 degrees into an octave or 53.
        const int   rangeSteps = s.rangeOctaves * params::scaleSize (s.scale);
        const int   degree     = (int) std::lround (value * (float) rangeSteps);

        // Offset transposes here, in semitones, rather than adding scale degrees: an octave
        // is 12 semitones in every scale the tuning table describes, whereas a scale's degree
        // count varies, so semitones are what keep +1 oct meaning an octave in all of them.
        const float absolute   = (float) s.root + params::scaleStepToSemitone (degree, s.scale)
                                   + 12.0f * (float) s.noteOctaves;

        result.note = juce::jlimit (0, 127, (int) std::lround (absolute));

        // A 12-EDO scale lands exactly on note numbers and the wheel is left alone, as it
        // always was. Any other EDO puts most of its degrees between the keys, so the
        // residual rides on pitch bend -- the same trick continuous mode uses below. With
        // MPE off that is one bend per channel, so overlapping notes cannot hold different
        // microtones; with MPE on, startNote() gives each note its own channel instead.
        //
        // Written even when the residual is zero. Degree 0 of a 19-EDO scale is in tune only
        // if the bend the *previous* degree left on the channel is cleared.
        if (params::scaleNeedsBend (s.scale))
            result.bend = params::pitchBendForSemitones (absolute - (float) result.note, bendRange);

        return result;
    }

    // Raw microtonal pitch: Range is octaves (12 semitones each) and the scale is bypassed.
    // The nearest semitone carries the note number; the residual (at most half a semitone) is
    // expressed as pitch bend.
    //
    // With MPE off, the bend goes on the note channel, which is shared by every voice on it
    // -- so overlapping notes, including several poly lanes at once, cannot hold different
    // microtones. That is the cost of staying on one channel, which is what survives hosts
    // that merge MIDI channels when routing between tracks. MPE on lifts this: startNote()
    // resolves a separate channel per note instead of reusing the one passed in here.
    const float absolute = (float) s.root + params::continuousSemitones (value, s.rangeOctaves * 12)
                             + 12.0f * (float) s.noteOctaves;

    result.note = juce::jlimit (0, 127, (int) std::lround (absolute));
    result.bend = params::pitchBendForSemitones (absolute - (float) result.note, bendRange);

    return result;
}

int SequencerEngine::velocityFor (const Snapshot& s, int laneIndex, int stepIndex) noexcept
{
    const auto& ln = s.noteLanes[(size_t) juce::jlimit (0, params::numLanes - 1, laneIndex)];
    const float step = ln.velocity[(size_t) juce::jlimit (0, params::numSteps - 1, stepIndex)];

    // The global Velocity is the master and the step carries the accent, which defaults to
    // unity -- so an untouched pattern plays at the bare global value, and the accent can
    // only ever pull a step below it.
    const float scaled = (float) s.velocity * step;

    // Clamped to 1 rather than 0: a MIDI note-on at velocity 0 is a note-off, so a step
    // pulled to 0% plays as quietly as MIDI allows instead of silently retiring itself.
    return juce::jlimit (1, 127, (int) std::lround (scaled));
}

float SequencerEngine::gateFor (const Snapshot& s, int laneIndex, int stepIndex) noexcept
{
    const auto& ln = s.noteLanes[(size_t) juce::jlimit (0, params::numLanes - 1, laneIndex)];
    return ln.gate[(size_t) juce::jlimit (0, params::numSteps - 1, stepIndex)];
}

void SequencerEngine::startNote (juce::MidiBuffer& out, int sampleOffset, const PitchResult& pitch,
                                 int velocity, int gateSamples, int begin, int end,
                                 bool mpeOn, int fixedChannel, int bendRange)
{
    // Channel first: MPE picks a member channel from its own pool, independent of which
    // voice slot the note ends up in below.
    int channel     = fixedChannel;
    int memberIndex = -1;

    if (mpeOn)
    {
        memberIndex = allocateMpeChannel (out, sampleOffset);
        channel     = mpeMemberChannelBase + memberIndex;
    }

    // Allocate the slot second: if this steals or reuses a voice, its note-off has to be
    // ordered ahead of the bend and note-on that follow at this same sample.
    const int slot = allocateVoice (out, sampleOffset, pitch.note, channel, begin, end);

    if (mpeOn)
    {
        mpeChannels[memberIndex].voiceSlot = slot;

        // RPN 0 has to reach this member channel before any note relying on it -- lazily,
        // the first time it's used (or after a range change clears every sent flag), rather
        // than blasting all 15 up front: most patterns never touch more than a few.
        if (! mpeChannels[memberIndex].rangeSent)
        {
            sendPitchBendRange (out, sampleOffset, channel, bendRange);
            mpeChannels[memberIndex].rangeSent = true;
        }
    }

    if (pitch.bend >= 0)
        out.addEvent (juce::MidiMessage::pitchWheel (channel, pitch.bend), sampleOffset);

    out.addEvent (juce::MidiMessage::noteOn (channel, pitch.note,
                                            (juce::uint8) juce::jlimit (1, 127, velocity)),
                  sampleOffset);

    voices[slot].note             = pitch.note;
    voices[slot].channel          = channel;
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
        publishUiSteps (false);
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

    // MPE routes every simultaneous note to its own channel -- a completely different
    // channel scheme from the single fixed one -- so a flip mid-performance has to let go of
    // everything rather than leave it addressed under the scheme that just left.
    if (const int mpeFlag = s.mpeEnabled ? 1 : 0; configuredMpeFlag != mpeFlag)
    {
        releaseAllVoices (out, 0);
        configuredMpeFlag = mpeFlag;

        // Forces a fresh RPN 0 / RPN 6 (or, off, a fresh single-channel RPN 0) on whichever
        // path runs next -- the receiver's per-channel state after a stretch in the other
        // scheme can't be assumed.
        configuredMpeOn      = false;
        configuredMode       = -1;
        configuredBendRange  = -1;
        configuredChannel    = -1;
    }

    // If the voice count was turned down, anything sounding outside the new limit has to
    // be let go or it would hang forever.
    retireUnownedVoices (out, 0, voiceLimit, s.polyMode);

    const int noteChannel = juce::jlimit (1, 16, s.midiChannel);
    const int bendRange   = juce::jlimit (1, 48, s.bendRange);

    // Pitch rides on the wheel whenever it can land between semitones: in continuous mode
    // always, and in quantized mode when the scale divides the octave into something other
    // than 12.
    const bool wantsBend = ! s.quantize || params::scaleNeedsBend (s.scale);

    // The receiving instrument has to be told the bend range: an instrument left at its own
    // default while we scale for +/-2 semitones plays the wrong interval.
    if (! s.mpeEnabled)
    {
        const int wantedMode = wantsBend ? 0 : 1;

        const bool changed = configuredMode != wantedMode
                          || configuredBendRange != bendRange
                          || configuredChannel != noteChannel;

        if (changed)
        {
            if (wantsBend)
            {
                sendPitchBendRange (out, 0, noteChannel, bendRange);
            }
            else if (configuredMode == 0)
            {
                // Leaving a mode that bends -- Quantize turned back on, or a switch from a
                // microtonal scale to a 12-EDO one. The channel is still holding whatever
                // bend the last note set, and nothing in this mode ever writes the wheel
                // again, so every note that follows would play detuned by that leftover
                // amount. Centre it on the channel the bends actually went to.
                const int staleChannel = configuredChannel >= 1 ? configuredChannel : noteChannel;

                out.addEvent (juce::MidiMessage::pitchWheel (staleChannel, params::pitchBendCentre), 0);
            }

            configuredMode      = wantedMode;
            configuredBendRange = bendRange;
            configuredChannel   = noteChannel;
        }
    }
    else
    {
        const int wantedMode = wantsBend ? 0 : 1;

        if (! configuredMpeOn)
        {
            addRpn (out, 0, mpeMasterChannel, params::mpeConfigurationRpn, mpeMemberChannels);
            configuredMpeOn = true;

            // A fresh zone announcement means the receiver's per-channel state is unknown,
            // so force everything below to re-prime instead of trusting whatever these held
            // from the last time MPE was on.
            configuredMpeWantedMode = -1;
            configuredMpeBendRange  = -1;
        }

        const bool changed = configuredMpeWantedMode != wantedMode
                          || configuredMpeBendRange   != bendRange;

        if (changed)
        {
            if (wantedMode == 1 && configuredMpeWantedMode == 0)
            {
                // Leaving a mode that bends -- recentre the whole zone. Cheap (16 messages,
                // no allocation) and simpler than tracking exactly which member channels a
                // bend was ever written to.
                out.addEvent (juce::MidiMessage::pitchWheel (mpeMasterChannel, params::pitchBendCentre), 0);

                for (int i = 0; i < mpeMemberChannels; ++i)
                    out.addEvent (juce::MidiMessage::pitchWheel (mpeMemberChannelBase + i,
                                                                  params::pitchBendCentre), 0);
            }

            configuredMpeWantedMode = wantedMode;
            configuredMpeBendRange  = bendRange;

            // The zone's range, announced where the zone itself was announced. Strictly, MPE
            // sets the member range from any *member* channel -- which is what startNote()
            // does below, lazily, the first time each channel is used -- and the master
            // channel's own range is a separate parameter. But a receiver that takes RPN 0 on
            // the master as the zone's range is common enough, and the ones that ignore
            // per-member RPN 0 entirely are exactly the ones that strand us at the MPE
            // default of +/-48 while we scale for something else. Four messages, once per
            // range change, buys agreement with both readings.
            sendPitchBendRange (out, 0, mpeMasterChannel, bendRange);

            // The member channels get theirs lazily, the first time each is actually used --
            // see startNote(). Most patterns never touch more than a few.
            for (auto& mc : mpeChannels)
                mc.rangeSent = false;
        }
    }
    // One-pole slew coefficient, computed per block rather than per sample. Shared by the
    // Mix CC and every CC lane's own tap -- Slew never touches pitch.
    const float slewCoeff = s.slewMs <= 0.01f
                              ? 1.0f
                              : 1.0f - std::exp (-1.0f / (float) (s.slewMs * 0.001 * currentSampleRate));

    //==========================================================================
    /** What a lane is playing, and for how much longer.

        Everything here is a function of the lane's global index, so all of it holds until the
        lane crosses a step boundary -- 6000 samples at 1/16 and 120 bpm. The loop below
        refreshes a lane only on the sample its boundary actually falls on, where it used to
        re-derive all of this, division and floor included, for every lane on every sample.
    */
    struct LaneRun
    {
        double       stepPpq      = 0.0;
        int          length       = 1;
        std::int64_t globalIndex  = 0;
        int          step         = 0;
        bool         stepOn       = false;
        float        value        = 0.0f;

        /** depth * value. Held rather than recomputed because the fold below re-adds every
            lane's share whenever any one of them moves. */
        float        contribution = 0.0f;

        /** The sample this stops being true on. Starting at 0 is what makes the loop resolve
            every lane on its first pass, which is also where a lane that advanced between
            blocks gets to report it. */
        int          nextBoundary = 0;
    };

    LaneRun noteRuns[params::numLanes];
    LaneRun ccRuns[params::numLanes];

    // Rate and length cannot move inside a block -- they come off the snapshot -- so they are
    // read once here rather than per sample, as they used to be.
    for (int laneIndex = 0; laneIndex < params::numLanes; ++laneIndex)
    {
        noteRuns[laneIndex].stepPpq = params::divisionPpq (s.noteLanes[laneIndex].division);
        noteRuns[laneIndex].length  = juce::jlimit (1, params::numSteps, s.noteLanes[laneIndex].length);

        ccRuns[laneIndex].stepPpq   = params::divisionPpq (s.ccLanes[laneIndex].division);
        ccRuns[laneIndex].length    = juce::jlimit (1, params::numSteps, s.ccLanes[laneIndex].length);
    }

    /** Re-resolves one lane at sample n and reports whether it actually landed on a new step
        there -- which is what the trigger and the CC latch key off, and what tells a lane that
        merely started a block mid-step from one that just moved.
    */
    const auto refreshLane = [&] (LaneRun& run, const LaneSnapshot& ln, LaneState& state,
                                  int laneIndex, int n) -> bool
    {
        const double ppq = ppqAtBlockStart + ppqPerSample * (double) n;

        run.globalIndex = resolveGlobalIndex (ppq, run.stepPpq, s.swing);

        const bool advanced = (run.globalIndex != state.lastGlobalIndex);

        run.step = stepIndexFor (run.globalIndex, run.length, ln.direction, laneIndex);

        // A muted lane, or one the instance does not have yet, adds nothing to the fold and
        // fires nothing -- so none of its per-step data is ever looked at. Bailing out before
        // reading any of it is what lets buildSnapshot() skip writing it, and that is where the
        // cost actually sat: eighty atomic loads per note lane per block, for lanes the user
        // cannot even see. The index above is still resolved, so the lane keeps its place on
        // the timeline and comes back mid-pattern rather than from the top.
        if (! ln.active)
        {
            run.stepOn       = false;
            run.value        = 0.0f;
            run.contribution = 0.0f;
        }
        else
        {
            run.value = ln.values[(size_t) run.step];

            const float chance = ln.chance[(size_t) run.step];

            // A step that loses its probability roll behaves exactly like a step that is
            // switched off: transparent for the mix, and it fires nothing. The roll is a pure
            // function of the timeline position, so it holds steady for the whole step and
            // repeats identically next time round the loop.
            run.stepOn = ln.enabled[(size_t) run.step];

            if (run.stepOn && chance < 0.999f)
                run.stepOn = hashToUnitFloat (run.globalIndex, laneIndex, probabilitySalt) < chance;

            run.contribution = ln.depth * run.value;
        }

        if (advanced)
        {
            state.lastGlobalIndex = run.globalIndex;
            state.step = run.step;
        }

        run.nextBoundary = nextBoundarySample (run.globalIndex, ppqAtBlockStart, ppqPerSample,
                                               run.stepPpq, s.swing, n, numSamples);

        return advanced;
    };

    // Both folds re-add every lane's share from scratch rather than adjusting a running total
    // by the difference: the sum has to land on the same float it used to, and floating-point
    // addition does not associate. Skipping a silent lane rather than adding its zero is part
    // of that -- it is what the per-sample loop did.
    const auto foldNoteLanes = [&]
    {
        float accumulator = 0.0f;

        for (const auto& run : noteRuns)
            if (run.stepOn)
                accumulator += run.contribution;

        return juce::jlimit (0.0f, 1.0f, accumulator);
    };

    const auto foldCcLanes = [&]
    {
        float accumulator = 0.0f;

        for (const auto& run : ccRuns)
            if (run.stepOn)
                accumulator += run.contribution;

        return juce::jlimit (0.0f, 1.0f, accumulator + s.ccOffset);
    };

    float noteMix = 0.0f;
    float ccMix   = 0.0f;

    // Samples of voice countdown the loop has deferred, and how many it may defer before the
    // next note-off falls due. Read after the config releases and retireUnownedVoices() above,
    // so it already accounts for anything those let go of.
    int pendingVoiceSamples = 0;
    int voiceCountdown      = soonestVoiceExpiry();

    const auto gateSamplesFor = [&] (double stepPpq, float gatePercent)
    {
        return (int) std::lround ((stepPpq / ppqPerSample) * (gatePercent * 0.01));
    };

    // Fires one note: pitchFor() already resolves whether this mode bends at all, so there is
    // nothing left to do here but hand its result to the voice allocator.
    const auto fireNote = [&] (int n, float value, int velocity, int gateSamples, int begin, int end)
    {
        const auto pitch = pitchFor (value, s, bendRange);
        startNote (out, n, pitch, velocity, gateSamples, begin, end,
                   s.mpeEnabled, noteChannel, bendRange);
    };

    for (int n = 0; n < numSamples; ++n)
    {
        //----------------------------------------------------------------------
        // Note-lane fold. Only a lane whose boundary lands on this sample does any work; the
        // rest are still playing what they were.
        bool   triggered      = false;
        int    triggerLane    = 0;
        int    triggerStep    = 0;
        double triggerStepPpq = params::divisionPpq (params::divIndex_1_16);

        // Poly mode: each lane's own trigger, collected here rather than emitted inside the
        // lane loop, because advanceVoices() below has to order its expiring note-offs ahead
        // of any note-on landing on this same sample.
        bool   anyLaneTriggered = false;
        bool   laneTriggered[params::numLanes] {};
        float  laneTriggerValue[params::numLanes] {};
        int    laneTriggerStep[params::numLanes] {};
        double laneTriggerStepPpq[params::numLanes] {};

        bool noteLanesMoved = false;

        for (int laneIndex = 0; laneIndex < params::numLanes; ++laneIndex)
        {
            auto& run = noteRuns[laneIndex];

            if (n != run.nextBoundary)
                continue;

            noteLanesMoved = true;

            const auto& ln = s.noteLanes[laneIndex];

            if (! refreshLane (run, ln, noteLaneStates[laneIndex], laneIndex, n) || ! run.stepOn)
                continue;

            if (s.polyMode)
            {
                // Every lane is its own voice, so Trigger has nothing to select and a lane at
                // zero Depth is simply silent -- which keeps the stock preset, where only lane
                // 1 has Depth, sounding as one voice until another is dialled up.
                if (std::abs (ln.depth) > 1.0e-6f)
                {
                    anyLaneTriggered              = true;
                    laneTriggered[laneIndex]      = true;
                    laneTriggerValue[laneIndex]   = run.value;
                    laneTriggerStep[laneIndex]    = run.step;
                    laneTriggerStepPpq[laneIndex] = run.stepPpq;
                }
            }
            else
            {
                // Trigger source: a specific lane, or any lane that just advanced.
                const bool isTriggerLane = s.noteTriggerSource >= params::numLanes
                                             || laneIndex == s.noteTriggerSource;

                if (isTriggerLane)
                {
                    triggered      = true;
                    triggerLane    = laneIndex;
                    triggerStep    = run.step;
                    triggerStepPpq = run.stepPpq;
                }
            }
        }

        if (noteLanesMoved)
            noteMix = foldNoteLanes();

        //----------------------------------------------------------------------
        // CC-lane fold. Same shape as the note-lane fold above -- each active step adds its
        // share of Depth -- but over the CC pool's own lanes, with no Trigger concept: CC
        // output is never "triggered", it continuously reflects the fold.
        bool ccLanesMoved = false;

        for (int laneIndex = 0; laneIndex < params::numLanes; ++laneIndex)
        {
            auto& run = ccRuns[laneIndex];

            if (n != run.nextBoundary)
                continue;

            ccLanesMoved = true;

            // The lane's own tap follows its own step value, independent of Depth -- Depth
            // governs the lane's share of the Mix CC, not its own tap. Inactive steps latch
            // the previous level instead of dropping to zero.
            if (refreshLane (run, s.ccLanes[laneIndex], ccLaneStates[laneIndex], laneIndex, n)
                  && run.stepOn)
                ccLaneHeldValue[laneIndex] = run.value;
        }

        if (ccLanesMoved)
            ccMix = foldCcLanes();

        slewedValue += (ccMix - slewedValue) * slewCoeff;

        for (int laneIndex = 0; laneIndex < params::numLanes; ++laneIndex)
        {
            // Each CC lane's own Offset, not the CC tab's own one -- that already has a job
            // (the Mix CC) and moving every lane's tap by the same amount would leave no way
            // to recentre just one of them.
            const float target = juce::jlimit (0.0f, 1.0f,
                                               ccLaneHeldValue[laneIndex] + s.ccLanes[laneIndex].ccOffset);
            ccLaneSlewedValue[laneIndex] += (target - ccLaneSlewedValue[laneIndex]) * slewCoeff;
        }

        //----------------------------------------------------------------------
        {
            ++pendingVoiceSamples;

            if (pendingVoiceSamples >= voiceCountdown)
            {
                advanceVoices (out, n, pendingVoiceSamples);
                pendingVoiceSamples = 0;
                voiceCountdown      = soonestVoiceExpiry();
            }

            // Expiring note-offs are already out, above, which is the ordering that matters:
            // a voice being reused at this same sample has to go off before it comes back on.
            if (s.polyMode ? anyLaneTriggered : triggered)
                settleVoices (std::exchange (pendingVoiceSamples, 0));

            if (s.polyMode)
            {
                for (int laneIndex = 0; laneIndex < params::numLanes; ++laneIndex)
                {
                    if (! laneTriggered[laneIndex])
                        continue;

                    const int step = laneTriggerStep[laneIndex];

                    // The lane's own value drives its pitch. Offset is not applied here --
                    // pitchFor() transposes whatever pitch this resolves to, the same way it
                    // does for the mix in the other mode.
                    const float value = juce::jlimit (0.0f, 1.0f, laneTriggerValue[laneIndex]);

                    const int begin = laneIndex * voicesPerLane;

                    fireNote (n, value,
                              velocityFor (s, laneIndex, step),
                              gateSamplesFor (laneTriggerStepPpq[laneIndex], gateFor (s, laneIndex, step)),
                              begin, begin + voiceLimit);
                }
            }
            else if (triggered)
            {
                // The note belongs to whichever step of whichever lane triggered it, so that
                // step's accent and gate both apply even though the pitch came from the
                // combined mix.
                fireNote (n, noteMix,
                          velocityFor (s, triggerLane, triggerStep),
                          gateSamplesFor (triggerStepPpq, gateFor (s, triggerLane, triggerStep)),
                          0, voiceLimit);
            }

            if (s.polyMode ? anyLaneTriggered : triggered)
                voiceCountdown = soonestVoiceExpiry();
        }

        //----------------------------------------------------------------------
        if (--ccCountdown <= 0)
        {
            ccCountdown = ccIntervalSamples;

            if (s.ccOn)
            {
                const int ccValue = juce::jlimit (0, 127, (int) std::lround (slewedValue * 127.0f));

                if (ccValue != lastCcValue)
                {
                    lastCcValue = ccValue;
                    out.addEvent (juce::MidiMessage::controllerEvent (juce::jlimit (1, 16, s.ccChannel),
                                                                     juce::jlimit (0, 127, s.ccNumber),
                                                                     ccValue), n);
                }
            }

            // Each CC lane can also drive its own destination, so one instance can modulate
            // several parameters rather than only the Mix CC.
            for (int laneIndex = 0; laneIndex < params::numLanes; ++laneIndex)
            {
                const auto& ln = s.ccLanes[laneIndex];

                // Inert lanes stay off the wire. `active` folds in both the mute and the lane
                // count, so this covers a muted lane and -- the case that actually leaked --
                // a lane the instance has not been given yet: Send defaults to on and the CC
                // numbers are pre-assigned, so lanes 2-4 of a one-lane instance were each
                // announcing themselves at zero before anything had been drawn on them.
                if (! ln.active || ! ln.ccOn)
                    continue;

                const int laneCc = juce::jlimit (0, 127,
                                                 (int) std::lround (ccLaneSlewedValue[laneIndex] * 127.0f));

                if (laneCc != ccLaneLastCcValue[laneIndex])
                {
                    ccLaneLastCcValue[laneIndex] = laneCc;
                    out.addEvent (juce::MidiMessage::controllerEvent (juce::jlimit (1, 16, ln.ccChannel),
                                                                     juce::jlimit (0, 127, ln.ccNumber),
                                                                     laneCc), n);
                }
            }
        }
    }

    settleVoices (pendingVoiceSamples);

    publishUiSteps (true);
}
