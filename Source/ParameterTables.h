#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

#include <array>
#include <cmath>

/**
    The constants and lookup tables the sequencer core and the plugin layer both need.

    Split out of Parameters.h so that SequencerEngine, which touches nothing but MidiBuffer
    and arithmetic, does not have to drag in juce_audio_processors and the whole plugin
    framework behind it just to learn how long a 1/16 is. Parameters.h -- parameter IDs, the
    APVTS layout, the pattern actions -- includes this; the engine includes only this.

    Every table here is the single source of its own names. A parallel StringArray beside a
    data table is what let the two drift out of step, with nothing but a Release-stripped
    jassert holding them together and a silent out-of-bounds read waiting at the end of it.
*/
namespace params
{

// The most lanes an instance can have. Every lane's parameters exist from the moment the
// plugin is created, because a VST3 cannot grow its parameter list at runtime -- so "adding
// a lane" raises laneCount, and the lanes above the count are simply inert and hidden. Their
// step data stays where it is, which is what lets a removed lane come back unchanged.
inline constexpr int numLanes = 4;
inline constexpr int numSteps = 16;

//==============================================================================
/** Which pool a lane belongs to. Notes and CC each have their own independent stack of up
    to numLanes lanes -- a Note lane's mix drives pitch (and poly-mode note triggering); a
    CC lane's mix drives the Mix CC, and each CC lane also keeps its own direct tap onto its
    own CC number, independent of that fold. A VST3 cannot grow its parameter list at
    runtime, so both pools' parameters exist for all numLanes lanes from the start -- this
    is what selects which pool's ID a given lane/step belongs to.
*/
enum class LaneKind { note, cc };

//==============================================================================
// Clock divisions. The step length is measured in quarter notes, because that is the unit
// AudioPlayHead reports positions in (PPQ). Name and length live in one row: they are
// indexed by the same parameter value, so keeping them in two arrays only created the
// opportunity for one to be longer than the other.
struct DivisionDef
{
    const char* name;
    double      ppq;
};

inline constexpr DivisionDef divisions[]
{
    { "1/1",   4.0       },
    { "1/2",   2.0       },
    { "1/4",   1.0       },
    { "1/4T",  2.0 / 3.0 },
    { "1/8",   0.5       },
    { "1/8T",  1.0 / 3.0 },
    { "1/16",  0.25      },
    { "1/16T", 1.0 / 6.0 },
    { "1/32",  0.125     },
};

inline constexpr int numDivisions = (int) (sizeof (divisions) / sizeof (divisions[0]));

inline constexpr int divIndex_1_4  = 2;
inline constexpr int divIndex_1_8  = 4;
inline constexpr int divIndex_1_16 = 6;

/** A division's step length in quarter notes. Clamped, and the only way in: an unclamped
    subscript on the table is what a bad parameter value would turn into a bad read. */
inline constexpr double divisionPpq (int divisionIndex) noexcept
{
    // Clamped by hand rather than with juce::jlimit, which is not constexpr -- and staying
    // constexpr is what lets a fixed division fold at compile time instead of costing a call.
    return divisions[(size_t) (divisionIndex < 0 ? 0
                                                 : divisionIndex >= numDivisions ? numDivisions - 1
                                                                                 : divisionIndex)].ppq;
}

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

// RPN 6, the MPE Configuration Message: sent on a zone's master channel, its value is how
// many member channels follow it. Ravel only ever offers one Lower Zone, so the value sent
// is always SequencerEngine::mpeMemberChannels.
inline constexpr int mpeConfigurationRpn = 6;

/** Centre position of the 14-bit pitch wheel: no bend. */
inline constexpr int pitchBendCentre = 8192;

// The master velocity every note starts from, in place of the parameter that used to set it.
// Each step's own accent trims down from here, so this is the ceiling and 127 is deliberately
// not it: leaving headroom is what lets an accent read as an accent rather than as everything
// else being quieter.
inline constexpr int fixedVelocity = 100;

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
// Scales are stored as steps of an equal division of the octave rather than as semitones,
// so 19-, 23-, 31-, 41- and 53-EDO sit in the same table as the familiar 12-EDO ones and the
// engine needs one code path for all of them. Every tuning here keeps a 2:1 octave, which is what
// makes the mapping onto MIDI note numbers tractable: a full scale-octave is always exactly
// 12 semitones however many degrees it took to climb, so only the degrees *within* an octave
// ever fall between the keys.
//
// 53 is the largest EDO in the table and so sets the array width.
inline constexpr int maxScaleSize = 53;

struct ScaleDef
{
    /** What the Scale parameter shows. In the row rather than in a list beside it, so a
        scale cannot be added without naming it or named without existing. */
    const char* name;

    /** Degrees above the root, in steps of `edo`. Only the first `size` entries are used. */
    std::array<int, maxScaleSize> intervals;
    int size;

    /** Equal divisions of the octave the intervals are counted in. */
    int edo;
};

/** Every step of an EDO, i.e. that tuning's own chromatic scale. A function because spelling
    out 53 consecutive integers by hand is noise, not documentation.
*/
constexpr ScaleDef edoChromatic (const char* name, int edo) noexcept
{
    ScaleDef def { name, {}, edo, edo };

    for (int i = 0; i < edo; ++i)
        def.intervals[(size_t) i] = i;

    return def;
}

inline constexpr ScaleDef scales[]
{

    //--------------------------------------------------------------------------
    // 12-EDO. Unchanged, and still the only ones that land exactly on MIDI notes.
    edoChromatic ("Chromatic",             12),
    { "Major",                 { 0, 2, 4, 5, 7, 9, 11 }, 7, 12 },
    { "Natural Minor",         { 0, 2, 3, 5, 7, 8, 10 }, 7, 12 },
    { "Harmonic Minor",        { 0, 2, 3, 5, 7, 8, 11 }, 7, 12 },
    { "Pentatonic Minor",      { 0, 3, 5, 7, 10 }, 5, 12 },
    { "Pentatonic Major",      { 0, 2, 4, 7, 9 }, 5, 12 },
    { "Dorian",                { 0, 2, 3, 5, 7, 9, 10 }, 7, 12 },
    { "Mixolydian",            { 0, 2, 4, 5, 7, 9, 10 }, 7, 12 },
    { "Whole Tone",            { 0, 2, 4, 6, 8, 10 }, 6, 12 },

    //--------------------------------------------------------------------------
    // 19-EDO. Step 63.2 cents. A meantone tuning, so the diatonic scales below are the
    // ordinary ones respelled -- 3+3+2+3+3+3+2 instead of 2+2+1+2+2+2+1 -- and sound
    // recognisably major and minor, with thirds closer to just than 12-EDO manages.
    // What is new is that sharps and flats separate: C# sits a step below Db.
    edoChromatic ("19 Chromatic",          19),
    { "19 Major",              { 0, 3, 6, 8, 11, 14, 17 }, 7, 19 },
    { "19 Natural Minor",      { 0, 3, 5, 8, 11, 13, 16 }, 7, 19 },
    { "19 Harmonic Minor",     { 0, 3, 5, 8, 11, 13, 17 }, 7, 19 },
    { "19 Pentatonic Minor",   { 0, 5, 8, 11, 16 }, 5, 19 },
    { "19 Blues",              { 0, 5, 8, 9, 11, 16 }, 6, 19 },

    //--------------------------------------------------------------------------
    // 23-EDO. Step 52.2 cents. The odd one out: its best fifth (13 steps, 678 cents) is a
    // quarter-tone flat, so diatonic harmony does not survive the trip and transcribing a
    // 12-EDO scale into it is pointless. What it does have is the mavila family, where a
    // flat fifth turns the diatonic scale inside out -- the "major" scale comes out with
    // two large steps and five small ones, the reverse of the usual arrangement. Those MOS
    // scales are generated by stacking that 13-step fifth, which is why they are the ones
    // offered here.
    edoChromatic ("23 Chromatic",          23),
    { "23 Pentatonic",         { 0, 3, 6, 13, 16 }, 5, 23 },                  // (2L 3s)
    { "23 Mavila 7",           { 0, 3, 6, 9, 13, 16, 19 }, 7, 23 },           // (2L 5s, antidiatonic)
    { "23 Mavila 9",           { 0, 3, 6, 9, 12, 13, 16, 19, 22 }, 9, 23 },   // (7L 2s)

    //--------------------------------------------------------------------------
    // 31-EDO. Step 38.7 cents. The best meantone in this table: its fifth (18 steps, 696.8
    // cents) is close to quarter-comma meantone, which makes its major third (10 steps, 387.1
    // cents) fall within a cent and a half of just (386.3) -- closer than 19-EDO manages. The
    // diatonic scales are the ordinary ones respelled 5-5-3-5-5-5-3, same idea as 19-EDO's
    // 3-3-2-3-3-3-2, just with two more degrees of room, so sharps and flats separate further
    // (the chromatic semitone is 2 steps here, versus 19-EDO's 1).
    edoChromatic ("31 Chromatic",          31),
    { "31 Major",              { 0, 5, 10, 13, 18, 23, 28 }, 7, 31 },
    { "31 Natural Minor",      { 0, 5, 8, 13, 18, 21, 26 }, 7, 31 },
    { "31 Harmonic Minor",     { 0, 5, 8, 13, 18, 21, 28 }, 7, 31 },
    { "31 Pentatonic Minor",   { 0, 8, 13, 18, 26 }, 5, 31 },
    { "31 Blues",              { 0, 8, 13, 15, 18, 26 }, 6, 31 },             // (adds the flat-5 blue note)

    //--------------------------------------------------------------------------
    // 41-EDO. Step 29.3 cents. The opposite trade from 31: its fifth (24 steps, 702.4 cents)
    // is within half a cent of pure 3/2, better than 12-EDO's own, so it's the one to reach for
    // when what matters is Pythagorean-accurate fifths rather than sweeter thirds (its major
    // third, 13 steps at 380.5 cents, is a passable 5/4 but not a standout). The diatonic
    // scales are the ordinary ones respelled 7-7-3-7-7-7-3 -- the same construction as 12-EDO's
    // 2-2-1-2-2-2-1 and 19-EDO's 3-3-2-3-3-3-2, just carried on a near-pure chain of fifths.
    edoChromatic ("41 Chromatic",          41),
    { "41 Major",              { 0, 7, 14, 17, 24, 31, 38 }, 7, 41 },
    { "41 Natural Minor",      { 0, 7, 10, 17, 24, 27, 34 }, 7, 41 },
    { "41 Harmonic Minor",     { 0, 7, 10, 17, 24, 27, 38 }, 7, 41 },
    { "41 Pentatonic Minor",   { 0, 10, 17, 24, 34 }, 5, 41 },

    //--------------------------------------------------------------------------
    // 53-EDO. Step 22.6 cents, the Holdrian comma. Its fifth is 31 steps (701.9 cents,
    // under a cent from just) and its major third 17 steps (384.9 cents), so it renders
    // 5-limit just intonation almost exactly -- and, separately, Pythagorean tuning, which
    // is why the two major scales below differ at all. It is also the grid Turkish makam
    // theory is written on, hence Rast and Hicaz.
    //
    // 53 degrees to the octave means Range is spending them fast: at the default Range of
    // 12 the chromatic scale covers a quarter of an octave, so turn Range up for these.
    edoChromatic ("53 Chromatic",          53),
    { "53 Just Major",         { 0, 9, 17, 22, 31, 39, 48 }, 7, 53 },         // 9/8 5/4 4/3 3/2 5/3 15/8
    { "53 Just Minor",         { 0, 9, 14, 22, 31, 36, 45 }, 7, 53 },         // 9/8 6/5 4/3 3/2 8/5 9/5
    { "53 Pythagorean Major",  { 0, 9, 18, 22, 31, 40, 49 }, 7, 53 },         // stacked 3/2s
    { "53 Just Pentatonic",    { 0, 9, 17, 31, 39 }, 5, 53 },
    { "53 Rast",               { 0, 9, 17, 22, 31, 40, 48 }, 7, 53 },         // 9-8-5-9-9-8-5 commas
    { "53 Hicaz",              { 0, 5, 17, 22, 31, 39, 44 }, 7, 53 },         // 5-12-5 tetrachord + Rast pentachord
};

inline constexpr int numScales = (int) (sizeof (scales) / sizeof (scales[0]));

/** The scale at an index, clamped. Every accessor below goes through this rather than
    subscripting the table, so an out-of-range parameter value can only ever pick the wrong
    scale, never read past the end of the table. */
inline constexpr const ScaleDef& scaleAt (int scaleIndex) noexcept
{
    return scales[(size_t) (scaleIndex < 0 ? 0
                                           : scaleIndex >= numScales ? numScales - 1
                                                                     : scaleIndex)];
}

/** What the Scale parameter shows for this index. */
inline const char* scaleName (int scaleIndex) noexcept    { return scaleAt (scaleIndex).name; }

/** Equal divisions of the octave the scale's degrees are measured in. */
inline int scaleEdo (int scaleIndex) noexcept             { return scaleAt (scaleIndex).edo; }

/** Degrees the scale packs into one octave -- 5 for a pentatonic, 53 for 53-EDO chromatic. */
inline int scaleSize (int scaleIndex) noexcept            { return scaleAt (scaleIndex).size; }

/** True when the scale's degrees do not all coincide with 12-EDO semitones, so its notes
    only play in tune if they carry a pitch bend.
*/
inline bool scaleNeedsBend (int scaleIndex) noexcept      { return scaleEdo (scaleIndex) != 12; }

/** The index of the scale with this name, or -1. For tests and for anything that wants to
    name a scale without hard-coding where it sits in the table. */
inline int scaleIndexNamed (juce::StringRef name) noexcept
{
    for (int i = 0; i < numScales; ++i)
        if (juce::StringRef (scales[(size_t) i].name) == name)
            return i;

    return -1;
}

/** Converts a scale-degree offset into semitones, wrapping octaves as it goes.

    Mapping the mixed value onto scale *degrees* rather than semitones-then-snap
    means every step lands on a usable note and the range is distributed evenly,
    instead of clustering several steps onto the same snapped pitch.

    Fractional for a non-12 EDO; whole numbers, exactly, for the 12-EDO scales.
*/
inline float scaleStepToSemitone (int step, int scaleIndex) noexcept
{
    const auto& s = scaleAt (scaleIndex);
    const int size = s.size;

    const int octave = (int) std::floor ((double) step / (double) size);
    const int degree = step - octave * size;

    // Counted in EDO steps first and converted once, so a whole number of octaves comes back
    // as an exact multiple of 12 rather than accumulating rounding per octave.
    return (float) (octave * s.edo + s.intervals[(size_t) degree]) * 12.0f / (float) s.edo;
}

//==============================================================================
// The choice lists AudioParameterChoice needs, built by walking the tables above rather
// than written out beside them. Functions rather than inline globals, so there is also no
// static-initialisation order to think about.

inline juce::StringArray divisionNameList()
{
    juce::StringArray names;

    for (const auto& division : divisions)
        names.add (division.name);

    return names;
}

inline juce::StringArray scaleNameList()
{
    juce::StringArray names;

    for (const auto& scale : scales)
        names.add (scale.name);

    return names;
}

} // namespace params
