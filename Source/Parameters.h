#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include <array>
#include <cmath>

/**
    Parameter IDs, choice lists and the small lookup tables shared between the
    audio engine and the editor.
*/
namespace params
{

// The most lanes an instance can have. Every lane's parameters exist from the moment the
// plugin is created, because a VST3 cannot grow its parameter list at runtime -- so "adding
// a lane" raises laneCount, and the lanes above the count are simply inert and hidden. Their
// step data stays where it is, which is what lets a removed lane come back unchanged.
inline constexpr int numLanes = 4;
inline constexpr int numSteps = 8;

//==============================================================================
// Clock divisions. Each value is a step length measured in quarter notes,
// because that is the unit AudioPlayHead reports positions in (PPQ).
inline const juce::StringArray divisionNames
    { "1/1", "1/2", "1/4", "1/4T", "1/8", "1/8T", "1/16", "1/16T", "1/32" };

inline constexpr double divisionPpq[]
    { 4.0, 2.0, 1.0, 2.0 / 3.0, 0.5, 1.0 / 3.0, 0.25, 1.0 / 6.0, 0.125 };

inline constexpr int divIndex_1_4  = 2;
inline constexpr int divIndex_1_8  = 4;
inline constexpr int divIndex_1_16 = 6;

inline const juce::StringArray directionNames  { "Forward", "Reverse", "Ping-Pong", "Random" };
inline const juce::StringArray modeNames       { "Add", "Multiply", "Max", "S&H" };
inline const juce::StringArray outputModeNames { "Notes", "CC" };
inline const juce::StringArray triggerNames    { "Lane 1", "Lane 2", "Lane 3", "Lane 4", "Any Lane" };

//==============================================================================
// Pitch is either quantized to scale degrees or continuous. Continuous pitch is carried as
// the nearest note plus a pitch bend on the note channel, so a whole chord or a set of
// overlapping notes shares one bend -- the trade for surviving hosts that merge MIDI
// channels when routing between tracks.
//
// RPN 0 is pitch bend sensitivity, which the receiving instrument has to be told: leaving it
// at an instrument's own default while we scale for a different range plays the wrong
// interval.
inline constexpr int pitchBendRangeRpn = 0;

/** Centre position of the 14-bit pitch wheel: no bend. */
inline constexpr int pitchBendCentre = 8192;

/** The wheel value that expresses a pitch offset in semitones, for an instrument set to the
    given bend range. Clamped rather than wrapped, so an offset larger than the range bottoms
    out at the extreme instead of jumping to the opposite one.
*/
inline int pitchBendForSemitones (float semitones, int bendRange) noexcept
{
    const float normalised = juce::jlimit (-1.0f, 1.0f,
                                           semitones / (float) juce::jmax (1, bendRange));

    return juce::jlimit (0, 16383, pitchBendCentre + (int) std::lround (normalised * 8191.0f));
}

// Combine-mode indices, used by the engine's switch.
enum CombineMode { modeAdd = 0, modeMultiply = 1, modeMax = 2, modeSampleHold = 3 };

// Output-mode indices: the two are mutually exclusive.
enum OutputMode { outNotes = 0, outCC = 1 };

//==============================================================================
// Scales are stored as steps of an equal division of the octave rather than as semitones,
// so 19-, 23- and 53-EDO sit in the same table as the familiar 12-EDO ones and the engine
// needs one code path for all of them. Every tuning here keeps a 2:1 octave, which is what
// makes the mapping onto MIDI note numbers tractable: a full scale-octave is always exactly
// 12 semitones however many degrees it took to climb, so only the degrees *within* an octave
// ever fall between the keys.
//
// 53 is the largest EDO in the table and so sets the array width.
inline constexpr int maxScaleSize = 53;

struct ScaleDef
{
    /** Degrees above the root, in steps of `edo`. Only the first `size` entries are used. */
    std::array<int, maxScaleSize> intervals;
    int size;

    /** Equal divisions of the octave the intervals are counted in. */
    int edo;
};

/** Every step of an EDO, i.e. that tuning's own chromatic scale. A function because spelling
    out 53 consecutive integers by hand is noise, not documentation.
*/
constexpr ScaleDef edoChromatic (int edo) noexcept
{
    ScaleDef def { {}, edo, edo };

    for (int i = 0; i < edo; ++i)
        def.intervals[(size_t) i] = i;

    return def;
}

inline const juce::StringArray scaleNames
{
    "Chromatic", "Major", "Natural Minor", "Harmonic Minor",
    "Pentatonic Minor", "Pentatonic Major", "Dorian", "Mixolydian", "Whole Tone",

    "19 Chromatic", "19 Major", "19 Natural Minor", "19 Harmonic Minor",
    "19 Pentatonic Minor", "19 Blues",

    "23 Chromatic", "23 Pentatonic", "23 Mavila 7", "23 Mavila 9",

    "53 Chromatic", "53 Just Major", "53 Just Minor", "53 Pythagorean Major",
    "53 Just Pentatonic", "53 Rast", "53 Hicaz"
};

inline constexpr ScaleDef scales[]
{
    //--------------------------------------------------------------------------
    // 12-EDO. Unchanged, and still the only ones that land exactly on MIDI notes.
    edoChromatic (12),                          // Chromatic
    { { 0, 2, 4, 5, 7, 9, 11 }, 7, 12 },        // Major
    { { 0, 2, 3, 5, 7, 8, 10 }, 7, 12 },        // Natural Minor
    { { 0, 2, 3, 5, 7, 8, 11 }, 7, 12 },        // Harmonic Minor
    { { 0, 3, 5, 7, 10 },       5, 12 },        // Pentatonic Minor
    { { 0, 2, 4, 7, 9 },        5, 12 },        // Pentatonic Major
    { { 0, 2, 3, 5, 7, 9, 10 }, 7, 12 },        // Dorian
    { { 0, 2, 4, 5, 7, 9, 10 }, 7, 12 },        // Mixolydian
    { { 0, 2, 4, 6, 8, 10 },    6, 12 },        // Whole Tone

    //--------------------------------------------------------------------------
    // 19-EDO. Step 63.2 cents. A meantone tuning, so the diatonic scales below are the
    // ordinary ones respelled -- 3+3+2+3+3+3+2 instead of 2+2+1+2+2+2+1 -- and sound
    // recognisably major and minor, with thirds closer to just than 12-EDO manages.
    // What is new is that sharps and flats separate: C# sits a step below Db.
    edoChromatic (19),                          // 19 Chromatic
    { { 0, 3, 6, 8, 11, 14, 17 }, 7, 19 },      // 19 Major
    { { 0, 3, 5, 8, 11, 13, 16 }, 7, 19 },      // 19 Natural Minor
    { { 0, 3, 5, 8, 11, 13, 17 }, 7, 19 },      // 19 Harmonic Minor
    { { 0, 5, 8, 11, 16 },        5, 19 },      // 19 Pentatonic Minor
    { { 0, 5, 8, 9, 11, 16 },     6, 19 },      // 19 Blues

    //--------------------------------------------------------------------------
    // 23-EDO. Step 52.2 cents. The odd one out: its best fifth (13 steps, 678 cents) is a
    // quarter-tone flat, so diatonic harmony does not survive the trip and transcribing a
    // 12-EDO scale into it is pointless. What it does have is the mavila family, where a
    // flat fifth turns the diatonic scale inside out -- the "major" scale comes out with
    // two large steps and five small ones, the reverse of the usual arrangement. Those MOS
    // scales are generated by stacking that 13-step fifth, which is why they are the ones
    // offered here.
    edoChromatic (23),                          // 23 Chromatic
    { { 0, 3, 6, 13, 16 },              5, 23 },    // 23 Pentatonic  (2L 3s)
    { { 0, 3, 6, 9, 13, 16, 19 },       7, 23 },    // 23 Mavila 7    (2L 5s, antidiatonic)
    { { 0, 3, 6, 9, 12, 13, 16, 19, 22 }, 9, 23 },  // 23 Mavila 9    (7L 2s)

    //--------------------------------------------------------------------------
    // 53-EDO. Step 22.6 cents, the Holdrian comma. Its fifth is 31 steps (701.9 cents,
    // under a cent from just) and its major third 17 steps (384.9 cents), so it renders
    // 5-limit just intonation almost exactly -- and, separately, Pythagorean tuning, which
    // is why the two major scales below differ at all. It is also the grid Turkish makam
    // theory is written on, hence Rast and Hicaz.
    //
    // 53 degrees to the octave means Range is spending them fast: at the default Range of
    // 12 the chromatic scale covers a quarter of an octave, so turn Range up for these.
    edoChromatic (53),                          // 53 Chromatic
    { { 0, 9, 17, 22, 31, 39, 48 }, 7, 53 },    // 53 Just Major        9/8 5/4 4/3 3/2 5/3 15/8
    { { 0, 9, 14, 22, 31, 36, 45 }, 7, 53 },    // 53 Just Minor        9/8 6/5 4/3 3/2 8/5 9/5
    { { 0, 9, 18, 22, 31, 40, 49 }, 7, 53 },    // 53 Pythagorean Major stacked 3/2s
    { { 0, 9, 17, 31, 39 },         5, 53 },    // 53 Just Pentatonic
    { { 0, 9, 17, 22, 31, 40, 48 }, 7, 53 },    // 53 Rast   9-8-5-9-9-8-5 commas
    { { 0, 5, 17, 22, 31, 39, 44 }, 7, 53 },    // 53 Hicaz  5-12-5 tetrachord + Rast pentachord
};

inline constexpr int numScales = (int) (sizeof (scales) / sizeof (scales[0]));

/** Equal divisions of the octave the scale's degrees are measured in. */
inline int scaleEdo (int scaleIndex) noexcept
{
    return scales[(size_t) juce::jlimit (0, numScales - 1, scaleIndex)].edo;
}

/** True when the scale's degrees do not all coincide with 12-EDO semitones, so its notes
    only play in tune if they carry a pitch bend.
*/
inline bool scaleNeedsBend (int scaleIndex) noexcept
{
    return scaleEdo (scaleIndex) != 12;
}

/** Converts a scale-degree offset into semitones, wrapping octaves as it goes.

    Mapping the mixed value onto scale *degrees* rather than semitones-then-snap
    means every step lands on a usable note and the range is distributed evenly,
    instead of clustering several steps onto the same snapped pitch.

    Fractional for a non-12 EDO; whole numbers, exactly, for the 12-EDO scales.
*/
inline float scaleStepToSemitone (int step, int scaleIndex) noexcept
{
    const auto& s = scales[(size_t) juce::jlimit (0, numScales - 1, scaleIndex)];
    const int size = s.size;

    const int octave = (int) std::floor ((double) step / (double) size);
    const int degree = step - octave * size;

    // Counted in EDO steps first and converted once, so a whole number of octaves comes back
    // as an exact multiple of 12 rather than accumulating rounding per octave.
    return (float) (octave * s.edo + s.intervals[(size_t) degree]) * 12.0f / (float) s.edo;
}

/** Linear mix-to-pitch mapping used when Quantize is off.

    The scale is deliberately not consulted here. Continuous pitch is meant to be raw
    microtonal values, so Range is read as semitones and the mapping is a straight ramp.
    Scale quantisation applies only when Quantize is on.
*/
inline float continuousSemitones (float mix, int rangeSemitones) noexcept
{
    return mix * (float) rangeSemitones;
}

//==============================================================================
// Per-lane parameter IDs. Lanes and steps are 1-based in the ID strings so the
// host's parameter list reads the same way the UI does.
juce::String stepValueId    (int lane, int step);
juce::String stepOnId       (int lane, int step);
juce::String stepChanceId   (int lane, int step);
juce::String stepVelocityId (int lane, int step);
juce::String stepGateId     (int lane, int step);
juce::String laneOnId       (int lane);
juce::String laneLengthId   (int lane);
juce::String laneDivId      (int lane);
juce::String laneDirId      (int lane);
juce::String laneDepthId    (int lane);
juce::String laneModeId     (int lane);
juce::String laneNudgeId    (int lane);
juce::String laneHumanizeId (int lane);
juce::String laneCcOnId     (int lane);
juce::String laneCcNumId    (int lane);
juce::String laneCcChanId   (int lane);

// Global / output-section parameter IDs.
inline constexpr auto outputModeId   = "out_mode";
inline constexpr auto triggerSrcId   = "trig_src";
inline constexpr auto quantizeId     = "quantize";
inline constexpr auto bendRangeId    = "bend_range";
inline constexpr auto rootNoteId     = "root_note";
inline constexpr auto rangeStepsId   = "range_steps";
inline constexpr auto scaleId         = "scale";
inline constexpr auto velocityId     = "velocity";
inline constexpr auto midiChannelId  = "midi_ch";
inline constexpr auto ccNumberId     = "cc_num";
inline constexpr auto ccChannelId    = "cc_ch";
inline constexpr auto offsetId       = "offset";
inline constexpr auto slewId         = "slew";
inline constexpr auto freeRunId      = "free_run";
inline constexpr auto swingId        = "swing";
inline constexpr auto voiceCountId   = "voices";
inline constexpr auto polyModeId     = "poly_mode";

/** How many lanes this instance currently has, 1 to numLanes. */
inline constexpr auto laneCountId    = "lane_count";

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

//==============================================================================
// Pattern actions. These live here rather than in the button callbacks so they go
// through the host properly (change gestures, automation, undo) and can be tested
// without a UI. Both touch step *values* only -- the on/off toggles are left alone,
// so a lane's rhythm survives a re-roll.

/** Gives every step in the lane a new random value. */
void randomiseLaneValues (juce::AudioProcessorValueTreeState& state, int lane, juce::Random& random);

/** Zeroes every step value in the lane. */
void clearLaneValues (juce::AudioProcessorValueTreeState& state, int lane);

/** Mirrors every step value about the midpoint (value -> 1 - value). */
void invertLaneValues (juce::AudioProcessorValueTreeState& state, int lane);

/** Shifts the lane's steps round by one. Negative rotates left, positive rotates right.

    Value, on/off, chance, velocity and gate move together -- rotating only the values would
    slide a pattern out from under its own rhythm, accents and note lengths.
*/
void rotateLane (juce::AudioProcessorValueTreeState& state, int lane, int direction);

/** A whole lane's step data, for copy/paste between lanes. */
struct LanePattern
{
    float values[numSteps] {};
    bool  enabled[numSteps] {};
    float chance[numSteps] {};
    float velocity[numSteps] {};
    float gate[numSteps] {};
    bool  valid = false;
};

LanePattern copyLane (juce::AudioProcessorValueTreeState& state, int lane);
void pasteLane (juce::AudioProcessorValueTreeState& state, int lane, const LanePattern& pattern);

} // namespace params
