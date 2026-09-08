#pragma once

#include "ParameterTables.h"

#include <juce_audio_processors/juce_audio_processors.h>

#include <vector>

/**
    Parameter IDs, the APVTS layout, and the pattern actions behind the editor's buttons.

    The lookup tables the audio engine needs as well -- lane and step counts, clock
    divisions, the scale table, the pitch-bend helpers -- live in ParameterTables.h, which
    this includes. Everything here needs juce_audio_processors; nothing there does, which is
    the whole point of the split. See that file's own header.
*/
namespace params
{

//==============================================================================
// The remaining choice lists. Neither is paired with a data table the way divisions and
// scales are, so they are spelled out here -- but both are built rather than declared, so
// there is no static-initialisation order to think about and no global juce::String
// construction at load time.

inline juce::StringArray directionNameList()
{
    return { "Forward", "Reverse", "Ping-Pong", "Random" };
}

/** Which Note lane's advance fires the shared note in mixed (non-poly) mode, with "Any
    Lane" on the end. Built from numLanes rather than written out, so the list cannot come
    to describe a different number of lanes than the plugin actually has -- the engine reads
    any index at or above numLanes as "any". */
inline juce::StringArray triggerNameList()
{
    juce::StringArray names;

    for (int lane = 0; lane < numLanes; ++lane)
        names.add ("Lane " + juce::String (lane + 1));

    names.add ("Any Lane");
    return names;
}

//==============================================================================
// Per-lane parameter IDs. Lanes and steps are 1-based in the ID strings so the
// host's parameter list reads the same way the UI does.
//
// The shared ones default to LaneKind::note so that every pre-existing call site --
// PluginEditor's note-lane code, the pattern-action helpers, the tests -- keeps compiling
// and keeps meaning exactly what it means today; only new CC code passes LaneKind::cc.
juce::String stepValueId    (int lane, int step, LaneKind kind = LaneKind::note);
juce::String stepOnId       (int lane, int step, LaneKind kind = LaneKind::note);
juce::String stepChanceId   (int lane, int step, LaneKind kind = LaneKind::note);

// A CC lane's steps carry no velocity or gate -- both are only ever arguments to
// startNote, and a CC lane never starts one -- so these parameters are note-only and
// never need to address a CC lane at all.
juce::String stepVelocityId (int lane, int step);
juce::String stepGateId     (int lane, int step);

//==============================================================================
/** Which of a step's four continuous parameters is being addressed.

    Lives here rather than in the editor because the pattern actions below take one: a lane's
    Randomize acts on whichever row its bars are currently showing, so "which row" is a
    parameter-domain idea and not only a UI one.
*/
enum class StepLayer { value = 0, velocity = 1, chance = 2, gate = 3 };

inline constexpr int numStepLayers = 4;

/** The parameter one step's given row lives in, or an empty string where that lane kind has no
    such row -- a CC lane has neither velocity nor gate. Callers skip the empty ones rather
    than addressing the note lane of the same number that stepVelocityId would resolve to. */
juce::String stepLayerId (int lane, int step, StepLayer layer, LaneKind kind = LaneKind::note);

/** What a row goes back to when it is cleared, and what a double-click on one of its bars
    resets to.

    Not zero for three of the four. Velocity, Chance and Gate are all *trims* on something that
    already works -- unity, always-fires, and a normal note length -- so zeroing them gives
    silent notes, a lane that never fires and zero-length notes, which are three ways of
    turning the lane off rather than of clearing it. Only Value, where zero is a real musical
    position, clears to zero.
*/
float stepLayerNeutral (StepLayer layer) noexcept;

/** The row's name as the UI writes it, for tooltips and menu entries that have to say which
    row an action is about to rewrite.

    Kind-dependent for one row. A Note lane's Value *is* its pitch -- that is the whole of what
    it drives -- so calling it anything else there makes the reader work out the connection
    themselves. A CC lane's Value drives a CC, so "Pitch" would be a plain lie.
*/
juce::String stepLayerName (StepLayer layer, LaneKind kind = LaneKind::note);

/** The same, abbreviated for the chips beside the steps, which have 46px to fit it in. */
juce::String stepLayerShortName (StepLayer layer, LaneKind kind = LaneKind::note);

juce::String laneOnId       (int lane, LaneKind kind = LaneKind::note);
juce::String laneLengthId   (int lane, LaneKind kind = LaneKind::note);
juce::String laneDivId      (int lane, LaneKind kind = LaneKind::note);
juce::String laneDirId      (int lane, LaneKind kind = LaneKind::note);
juce::String laneDepthId    (int lane, LaneKind kind = LaneKind::note);

// A CC lane's own destination -- not an optional tap on any lane any more, but the whole
// reason a CC lane exists. Note lanes never have these.
juce::String laneCcOnId     (int lane);
juce::String laneCcNumId    (int lane);
juce::String laneCcChanId   (int lane);
juce::String laneCcOffsetId (int lane);

// Which Note lane's advance fires the shared note in mixed (non-poly) mode. CC output has
// no equivalent: it is never "triggered", it continuously reflects the fold.
inline constexpr auto noteTriggerSrcId = "trig_src";

inline constexpr auto quantizeId     = "quantize";
inline constexpr auto bendRangeId    = "bend_range";
inline constexpr auto rootNoteId     = "root_note";
inline constexpr auto rangeOctavesId = "range_octaves";
inline constexpr auto scaleId         = "scale";
inline constexpr auto midiChannelId  = "midi_ch";


// The CC tab's own Mix destination -- fed by the CC-lane fold, the same way pitch is fed
// by the Note-lane fold.
inline constexpr auto ccOnId         = "cc_on";
inline constexpr auto ccNumberId     = "cc_num";
inline constexpr auto ccChannelId    = "cc_ch";

// Transposes the pitch the Note-lane fold resolves to, in whole octaves (-3..+3). Unlike
// ccOffsetId it does not shift the fold itself -- see the note by its parameter definition.
inline constexpr auto noteOffsetId   = "offset";

// Shifts the CC-lane mix before it becomes the Mix CC. Independent of any CC lane's own
// laneCcOffsetId, which shifts that lane's own tap instead.
inline constexpr auto ccOffsetId     = "cc_offset";

// Smooths the Mix CC and every CC lane's own tap. Never touches pitch.
inline constexpr auto slewId         = "slew";

// Whether the sequencer keeps stepping while the host transport is stopped. One shared
// switch: splitting it in two would mean running two independent timelines through the
// whole engine (voice allocation, the CC slew countdown and every lane's step resolution
// all key off a single ppq today) for a narrow benefit.
inline constexpr auto freeRunId      = "free_run";

// One Swing across both stacks. They answer to the same host clock, and a Note lane and a
// CC lane swung against each other read as drift rather than as groove -- so this is the
// same shared-switch reasoning as Free Run above. Keeps the "swing" id the Note-only
// version already used, so a saved session's value carries straight over.
inline constexpr auto swingId        = "swing";

inline constexpr auto voiceCountId   = "voices";
inline constexpr auto polyModeId     = "poly_mode";

// Gives every simultaneously-sounding note its own MIDI channel instead of sharing the Note
// Channel's one pitch wheel. Orthogonal to Poly -- it applies in mixed mode too, wherever
// Gate > 100% lets one step's note overlap the next. While it is on, midiChannelId is inert:
// the zone master is fixed at channel 1 and every note goes out on a member channel.
inline constexpr auto mpeEnabledId   = "mpe_on";

/** How many lanes this instance currently has in each pool, 1 to numLanes. Independent --
    growing one stack does not cost the other any room. */
inline constexpr auto noteLaneCountId = "note_lane_count";
inline constexpr auto ccLaneCountId   = "cc_lane_count";

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

//==============================================================================
// Pattern actions. These live here rather than in the button callbacks so they go
// through the host properly (change gestures, automation, undo) and can be tested
// without a UI. Both touch step *values* only -- the on/off toggles are left alone,
// so a lane's rhythm survives a re-roll.

// All three act on one row of the lane -- whichever the lane's bars are currently showing --
// rather than always on Value. Randomizing the row you are looking at is the only reading of
// the button that matches what is under it; the alternative is a Randomize that appears to do
// nothing whenever Prob or Gate is selected.
//
// Each spreads or mirrors across that row's *own* range, so Gate's 5..200 is randomised over
// 5..200 rather than over Value's 0..1.

/** Gives every step in the row a new random value, anywhere in that row's range. */
void randomiseLaneRow (juce::AudioProcessorValueTreeState& state, int lane, juce::Random& random,
                       LaneKind kind = LaneKind::note, StepLayer layer = StepLayer::value);

/** Puts every step in the row back to its neutral -- see stepLayerNeutral. */
void clearLaneRow (juce::AudioProcessorValueTreeState& state, int lane,
                   LaneKind kind = LaneKind::note, StepLayer layer = StepLayer::value);

/** Mirrors every step in the row about the middle of that row's own range. */
void invertLaneRow (juce::AudioProcessorValueTreeState& state, int lane,
                    LaneKind kind = LaneKind::note, StepLayer layer = StepLayer::value);

/** Shifts the lane's steps round by one. Negative rotates left, positive rotates right.

    Value, on/off and chance move together -- rotating only the values would slide a pattern
    out from under its own rhythm and accents. A note lane's velocity and gate move with them
    too; a CC lane has neither.
*/
void rotateLane (juce::AudioProcessorValueTreeState& state, int lane, int direction,
                 LaneKind kind = LaneKind::note);

/** Takes a lane out of the instance, closing the gap behind it.

    Every lane above `lane` moves down one -- all of it, the lane's own controls as well as
    its eight steps, not just the pattern -- and the slot the stack shrinks out of at the top
    is put back to its defaults. That is what makes `+ Add lane` always mean a new lane
    rather than a second copy of the one that just moved down.

    Removal is destructive: the removed lane's data is overwritten by the lane above it, and
    the only way back is the undo history. Keeping it was only ever possible while lanes came
    off the end -- once any lane can go, there is no free slot to keep it in. Every write here
    goes through a change gesture in one message callback, so the shift and the new lane count
    land as a single undo step.

    Does nothing at a lane count of 1, or for a lane this instance does not have.
*/
void removeLane (juce::AudioProcessorValueTreeState& state, int lane, LaneKind kind = LaneKind::note);

/** A whole lane's step data, for copy/paste between lanes of the same kind. Velocity and
    gate are along for a note lane's ride and simply unused when the pattern came from, or
    is pasted onto, a CC lane. */
struct LanePattern
{
    float values[numSteps] {};
    bool  enabled[numSteps] {};
    float chance[numSteps] {};
    float velocity[numSteps] {};
    float gate[numSteps] {};
    bool  valid = false;
};

LanePattern copyLane (juce::AudioProcessorValueTreeState& state, int lane, LaneKind kind = LaneKind::note);
void pasteLane (juce::AudioProcessorValueTreeState& state, int lane, const LanePattern& pattern,
                LaneKind kind = LaneKind::note);

} // namespace params
