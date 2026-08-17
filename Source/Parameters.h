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

inline constexpr int numLanes = 3;
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
inline const juce::StringArray outputModeNames { "Notes", "CC", "Notes + CC" };
inline const juce::StringArray triggerNames    { "Lane 1", "Lane 2", "Lane 3", "Any Lane" };
// Appended rather than reordered so a saved session's stored index keeps its meaning.
inline const juce::StringArray pitchModeNames
    { "Semitone", "Continuous (MPE)", "Continuous (Pitch Bend)" };

enum PitchMode { pitchSemitone = 0, pitchMpe = 1, pitchBend = 2 };

/** True for the modes where pitch is not quantized to semitones. Both continuous modes
    share the note-plus-bend maths and differ only in how the bend is channelled.
*/
inline bool isContinuousPitch (int pitchMode) noexcept
{
    return pitchMode == pitchMpe || pitchMode == pitchBend;
}

//==============================================================================
// MPE lower zone: channel 1 is the master, 2..16 are member channels. Notes are
// rotated across the member channels so per-note pitch bend on a new note can't
// pull the pitch of one still releasing.
inline constexpr int mpeMasterChannel  = 1;
inline constexpr int mpeMemberChannels = 15;

// RPN 6 is the MPE Configuration Message; RPN 0 is pitch bend sensitivity.
inline constexpr int mpeZoneRpn         = 6;
inline constexpr int pitchBendRangeRpn  = 0;

// Combine-mode indices, used by the engine's switch.
enum CombineMode { modeAdd = 0, modeMultiply = 1, modeMax = 2, modeSampleHold = 3 };

// Output-mode indices. Notes are active unless the mode is CC-only, and CC is
// active unless the mode is Notes-only.
enum OutputMode { outNotes = 0, outCC = 1, outBoth = 2 };

//==============================================================================
inline constexpr int maxScaleSize = 12;

struct ScaleDef
{
    std::array<int, maxScaleSize> intervals;
    int size;
};

inline const juce::StringArray scaleNames
{
    "Chromatic", "Major", "Natural Minor", "Harmonic Minor",
    "Pentatonic Minor", "Pentatonic Major", "Dorian", "Mixolydian", "Whole Tone"
};

inline constexpr ScaleDef scales[]
{
    { { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 }, 12 },   // Chromatic
    { { 0, 2, 4, 5, 7, 9, 11, 0, 0, 0, 0, 0 },   7 },   // Major
    { { 0, 2, 3, 5, 7, 8, 10, 0, 0, 0, 0, 0 },   7 },   // Natural Minor
    { { 0, 2, 3, 5, 7, 8, 11, 0, 0, 0, 0, 0 },   7 },   // Harmonic Minor
    { { 0, 3, 5, 7, 10, 0, 0, 0, 0, 0, 0, 0 },   5 },   // Pentatonic Minor
    { { 0, 2, 4, 7, 9, 0, 0, 0, 0, 0, 0, 0 },    5 },   // Pentatonic Major
    { { 0, 2, 3, 5, 7, 9, 10, 0, 0, 0, 0, 0 },   7 },   // Dorian
    { { 0, 2, 4, 5, 7, 9, 10, 0, 0, 0, 0, 0 },   7 },   // Mixolydian
    { { 0, 2, 4, 6, 8, 10, 0, 0, 0, 0, 0, 0 },   6 },   // Whole Tone
};

inline constexpr int numScales = (int) (sizeof (scales) / sizeof (scales[0]));

/** Converts a scale-degree offset into semitones, wrapping octaves as it goes.

    Mapping the mixed value onto scale *degrees* rather than semitones-then-snap
    means every step lands on a usable note and the range is distributed evenly,
    instead of clustering several steps onto the same snapped pitch.
*/
inline int scaleStepToSemitone (int step, int scaleIndex) noexcept
{
    const auto& s = scales[(size_t) juce::jlimit (0, numScales - 1, scaleIndex)];
    const int size = s.size;

    const int octave = (int) std::floor ((double) step / (double) size);
    const int degree = step - octave * size;

    return octave * 12 + s.intervals[(size_t) degree];
}

/** Linear mix-to-pitch mapping used by the continuous pitch modes.

    The scale is deliberately not consulted here. Continuous pitch is meant to be raw
    microtonal values, so Range is read as semitones and the mapping is a straight ramp.
    Scale quantisation applies only in Semitone mode.
*/
inline float continuousSemitones (float mix, int rangeSemitones) noexcept
{
    return mix * (float) rangeSemitones;
}

//==============================================================================
// Per-lane parameter IDs. Lanes and steps are 1-based in the ID strings so the
// host's parameter list reads the same way the UI does.
juce::String stepValueId  (int lane, int step);
juce::String stepOnId     (int lane, int step);
juce::String laneLengthId (int lane);
juce::String laneDivId    (int lane);
juce::String laneDirId    (int lane);
juce::String laneDepthId  (int lane);
juce::String laneModeId   (int lane);

// Global / output-section parameter IDs.
inline constexpr auto outputModeId   = "out_mode";
inline constexpr auto triggerSrcId   = "trig_src";
inline constexpr auto pitchModeId    = "pitch_mode";
inline constexpr auto bendRangeId    = "bend_range";
inline constexpr auto rootNoteId     = "root_note";
inline constexpr auto rangeStepsId   = "range_steps";
inline constexpr auto scaleId         = "scale";
inline constexpr auto velocityId     = "velocity";
inline constexpr auto gateLengthId   = "gate_len";
inline constexpr auto midiChannelId  = "midi_ch";
inline constexpr auto ccNumberId     = "cc_num";
inline constexpr auto ccChannelId    = "cc_ch";
inline constexpr auto offsetId       = "offset";
inline constexpr auto slewId         = "slew";
inline constexpr auto freeRunId      = "free_run";

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

} // namespace params
