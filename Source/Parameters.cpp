#include "Parameters.h"

namespace params
{

namespace
{
    juce::String lanePrefix (int lane)           { return "l" + juce::String (lane + 1); }
    juce::String stepPrefix (int lane, int step) { return lanePrefix (lane) + "_s" + juce::String (step + 1); }
}

juce::String stepValueId    (int lane, int step) { return stepPrefix (lane, step) + "_val"; }
juce::String stepOnId       (int lane, int step) { return stepPrefix (lane, step) + "_on"; }
juce::String stepChanceId   (int lane, int step) { return stepPrefix (lane, step) + "_chance"; }
juce::String stepVelocityId (int lane, int step) { return stepPrefix (lane, step) + "_vel"; }
juce::String laneOnId       (int lane)           { return lanePrefix (lane) + "_on"; }
juce::String laneLengthId   (int lane)           { return lanePrefix (lane) + "_length"; }
juce::String laneDivId      (int lane)           { return lanePrefix (lane) + "_div"; }
juce::String laneDirId      (int lane)           { return lanePrefix (lane) + "_dir"; }
juce::String laneDepthId    (int lane)           { return lanePrefix (lane) + "_depth"; }
juce::String laneModeId     (int lane)           { return lanePrefix (lane) + "_mode"; }
juce::String laneNudgeId    (int lane)           { return lanePrefix (lane) + "_nudge"; }
juce::String laneHumanizeId (int lane)           { return lanePrefix (lane) + "_humanize"; }
juce::String laneVelocityId (int lane)           { return lanePrefix (lane) + "_vel"; }
juce::String laneCcOnId     (int lane)           { return lanePrefix (lane) + "_cc_on"; }
juce::String laneCcNumId    (int lane)           { return lanePrefix (lane) + "_cc_num"; }
juce::String laneCcChanId   (int lane)           { return lanePrefix (lane) + "_cc_chan"; }

namespace
{
    constexpr int versionHint = 1;

    // Starting patterns. Lane 1 drives at full depth; lanes 2 and 3 start at zero
    // depth so the plugin is immediately understandable, and dialling either one up
    // folds it into the mix.
    constexpr float defaultValues[numLanes][numSteps]
    {
        { 0.00f, 0.42f, 0.17f, 0.58f, 0.25f, 0.83f, 0.33f, 0.50f },
        { 0.00f, 0.50f, 1.00f, 0.50f, 0.00f, 0.50f, 1.00f, 0.50f },
        { 1.00f, 0.66f, 0.33f, 0.00f, 0.33f, 0.66f, 1.00f, 0.50f },
        { 0.25f, 0.75f, 0.50f, 1.00f, 0.00f, 0.75f, 0.25f, 0.50f },
    };

    // Lengths and rates that do not divide into each other, so a lane added on top of the
    // ones already running lands somewhere new rather than doubling one of them.
    constexpr int   defaultLength[numLanes]   { 8, 5, 3, 7 };
    constexpr int   defaultDivision[numLanes] { divIndex_1_16, divIndex_1_8, divIndex_1_4, divIndex_1_8 };

    // Every lane starts at full depth. Adding a lane is now a deliberate act, so it has to
    // do something audible on the click -- a new lane at zero depth would look like the
    // button had failed. Depth is the first control in the lane's own block when it is too
    // much.
    constexpr float defaultDepth[numLanes]    { 1.0f, 1.0f, 1.0f, 1.0f };

    juce::String percentText (float value, int)
    {
        return juce::String (juce::roundToInt (value * 100.0f)) + " %";
    }

    juce::String noteNameText (int midiNote)
    {
        return juce::MidiMessage::getMidiNoteName (midiNote, true, true, 3);
    }
}

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    for (int lane = 0; lane < numLanes; ++lane)
    {
        const auto laneName = "Lane " + juce::String (lane + 1) + " ";

        for (int step = 0; step < numSteps; ++step)
        {
            const auto stepName = laneName + "Step " + juce::String (step + 1);

            layout.add (std::make_unique<juce::AudioParameterFloat> (
                juce::ParameterID { stepValueId (lane, step), versionHint },
                stepName,
                juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f),
                defaultValues[lane][step],
                juce::AudioParameterFloatAttributes().withStringFromValueFunction (percentText)));

            layout.add (std::make_unique<juce::AudioParameterBool> (
                juce::ParameterID { stepOnId (lane, step), versionHint },
                stepName + " On",
                true));

            layout.add (std::make_unique<juce::AudioParameterFloat> (
                juce::ParameterID { stepChanceId (lane, step), versionHint },
                stepName + " Chance",
                juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f),
                1.0f,
                juce::AudioParameterFloatAttributes().withStringFromValueFunction (percentText)));

            // The step's own accent, as a trim on the lane's velocity rather than an absolute
            // number, so a pattern keeps its shape when the lane or the global Velocity is
            // moved. 100% is unity, which is what makes this addition silent for a session
            // saved before it existed.
            //
            // Attenuate-only, unlike the per-lane trim: the global and the lane already set
            // the ceiling, and a range that went above unity would draw every untouched step
            // as a half-height bar in the editor's velocity layer -- reading as "half" when
            // it means "unchanged".
            layout.add (std::make_unique<juce::AudioParameterFloat> (
                juce::ParameterID { stepVelocityId (lane, step), versionHint },
                stepName + " Velocity",
                juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f),
                1.0f,
                juce::AudioParameterFloatAttributes().withStringFromValueFunction (percentText)));
        }

        // The lane's own mute. True is "playing", and it is what a session saved before this
        // existed loads with, so nothing goes silent on upgrade. Muting reads as every step
        // in the lane being switched off at once -- see the engine.
        layout.add (std::make_unique<juce::AudioParameterBool> (
            juce::ParameterID { laneOnId (lane), versionHint },
            laneName + "On", true));

        layout.add (std::make_unique<juce::AudioParameterInt> (
            juce::ParameterID { laneLengthId (lane), versionHint },
            laneName + "Length", 1, numSteps, defaultLength[lane]));

        layout.add (std::make_unique<juce::AudioParameterChoice> (
            juce::ParameterID { laneDivId (lane), versionHint },
            laneName + "Division", divisionNames, defaultDivision[lane]));

        layout.add (std::make_unique<juce::AudioParameterChoice> (
            juce::ParameterID { laneDirId (lane), versionHint },
            laneName + "Direction", directionNames, 0));

        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { laneDepthId (lane), versionHint },
            laneName + "Depth",
            juce::NormalisableRange<float> (-1.0f, 1.0f, 0.001f),
            defaultDepth[lane],
            juce::AudioParameterFloatAttributes().withStringFromValueFunction (percentText)));

        layout.add (std::make_unique<juce::AudioParameterChoice> (
            juce::ParameterID { laneModeId (lane), versionHint },
            laneName + "Mode", modeNames, modeAdd));

        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { laneNudgeId (lane), versionHint },
            laneName + "Nudge",
            juce::NormalisableRange<float> (-1.0f, 1.0f, 0.01f), 0.0f,
            juce::AudioParameterFloatAttributes().withStringFromValueFunction (percentText)));

        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { laneHumanizeId (lane), versionHint },
            laneName + "Humanize",
            juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.0f,
            juce::AudioParameterFloatAttributes().withStringFromValueFunction (percentText)));

        // A trim on the global Velocity rather than an absolute value, so the global stays
        // the master control and a session saved before this existed sounds identical at
        // the 100% default.
        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { laneVelocityId (lane), versionHint },
            laneName + "Velocity",
            juce::NormalisableRange<float> (0.0f, 2.0f, 0.01f), 1.0f,
            juce::AudioParameterFloatAttributes().withStringFromValueFunction (percentText)));

        layout.add (std::make_unique<juce::AudioParameterBool> (
            juce::ParameterID { laneCcOnId (lane), versionHint },
            laneName + "CC On", false));

        // Sensible unused defaults: CC 20, 21, 22 for lanes 1-3.
        layout.add (std::make_unique<juce::AudioParameterInt> (
            juce::ParameterID { laneCcNumId (lane), versionHint },
            laneName + "CC Number", 0, 127, 20 + lane));

        layout.add (std::make_unique<juce::AudioParameterInt> (
            juce::ParameterID { laneCcChanId (lane), versionHint },
            laneName + "CC Channel", 1, 16, 1));
    }

    //==========================================================================
    // One lane to begin with. The other three exist as parameters from the start -- a VST3
    // cannot add any later -- but stay silent and hidden until this is raised.
    layout.add (std::make_unique<juce::AudioParameterInt> (
        juce::ParameterID { laneCountId, versionHint }, "Lanes", 1, numLanes, 1));

    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { outputModeId, versionHint }, "Output", outputModeNames, outNotes));

    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { triggerSrcId, versionHint }, "Trigger", triggerNames, 0));

    // On: pitch snaps to degrees of the selected scale. Off: continuous microtonal pitch,
    // carried as the nearest note plus a pitch bend. Defaults to on, which is the behaviour
    // the old three-way Pitch choice defaulted to.
    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { quantizeId, versionHint }, "Quantize", true));

    layout.add (std::make_unique<juce::AudioParameterInt> (
        juce::ParameterID { bendRangeId, versionHint }, "Bend Range", 1, 48, 2,
        juce::AudioParameterIntAttributes().withStringFromValueFunction (
            [] (int v, int) { return juce::String (v) + " st"; })));

    layout.add (std::make_unique<juce::AudioParameterInt> (
        juce::ParameterID { rootNoteId, versionHint }, "Root", 0, 127, 48,
        juce::AudioParameterIntAttributes().withStringFromValueFunction (
            [] (int v, int) { return noteNameText (v); })));

    // Semitones in the continuous pitch modes; scale degrees in Semitone mode, where the
    // octave span then depends on the scale -- 12 degrees is two and a half octaves of a
    // pentatonic but only a quarter of an octave of 53-EDO chromatic. Notes clamp to the MIDI
    // range, so how much of a large Range is actually reachable depends on Root.
    layout.add (std::make_unique<juce::AudioParameterInt> (
        juce::ParameterID { rangeStepsId, versionHint }, "Range", 1, 100, 12));

    // The two lists are indexed by the same parameter value, so they have to stay in step.
    jassert (scaleNames.size() == numScales);

    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { scaleId, versionHint }, "Scale", scaleNames, 4));

    layout.add (std::make_unique<juce::AudioParameterInt> (
        juce::ParameterID { velocityId, versionHint }, "Velocity", 1, 127, 100));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { gateLengthId, versionHint }, "Gate",
        juce::NormalisableRange<float> (5.0f, 200.0f, 1.0f), 60.0f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction (
            [] (float v, int) { return juce::String (juce::roundToInt (v)) + " %"; })));

    layout.add (std::make_unique<juce::AudioParameterInt> (
        juce::ParameterID { midiChannelId, versionHint }, "Note Channel", 1, 16, 1));

    layout.add (std::make_unique<juce::AudioParameterInt> (
        juce::ParameterID { ccNumberId, versionHint }, "CC Number", 0, 127, 1));

    layout.add (std::make_unique<juce::AudioParameterInt> (
        juce::ParameterID { ccChannelId, versionHint }, "CC Channel", 1, 16, 1));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { offsetId, versionHint }, "Offset",
        juce::NormalisableRange<float> (-1.0f, 1.0f, 0.001f), 0.0f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction (percentText)));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { slewId, versionHint }, "Slew",
        juce::NormalisableRange<float> (0.0f, 500.0f, 1.0f), 0.0f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction (
            [] (float v, int) { return juce::String (juce::roundToInt (v)) + " ms"; })));

    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { freeRunId, versionHint }, "Free Run", true));

    // 1 is the historical monophonic behaviour: a retrigger always closes the previous
    // note. Above 1, a Gate over 100% overlaps into the following step instead.
    layout.add (std::make_unique<juce::AudioParameterInt> (
        juce::ParameterID { voiceCountId, versionHint }, "Voices", 1, 8, 1));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { swingId, versionHint }, "Swing",
        juce::NormalisableRange<float> (-1.0f, 1.0f, 0.01f), 0.0f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction (percentText)));

    // Appended last, and defaulting to off, so a session saved before it existed loads
    // with the mixed-lane behaviour it was written with.
    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { polyModeId, versionHint }, "Poly", false));

    return layout;
}

//==============================================================================
namespace
{
    void setParam (juce::AudioProcessorValueTreeState& state, const juce::String& paramID, float value)
    {
        if (auto* parameter = state.getParameter (paramID))
        {
            // Gestures so hosts treat each write as a deliberate automation edit.
            parameter->beginChangeGesture();
            parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
            parameter->endChangeGesture();
        }
    }

    float getParam (juce::AudioProcessorValueTreeState& state, const juce::String& paramID)
    {
        if (auto* value = state.getRawParameterValue (paramID))
            return value->load();

        return 0.0f;
    }
}

void randomiseLaneValues (juce::AudioProcessorValueTreeState& state, int lane, juce::Random& random)
{
    for (int step = 0; step < numSteps; ++step)
        setParam (state, stepValueId (lane, step), random.nextFloat());
}

void clearLaneValues (juce::AudioProcessorValueTreeState& state, int lane)
{
    for (int step = 0; step < numSteps; ++step)
        setParam (state, stepValueId (lane, step), 0.0f);
}

void invertLaneValues (juce::AudioProcessorValueTreeState& state, int lane)
{
    for (int step = 0; step < numSteps; ++step)
        setParam (state, stepValueId (lane, step),
                  1.0f - getParam (state, stepValueId (lane, step)));
}

void rotateLane (juce::AudioProcessorValueTreeState& state, int lane, int direction)
{
    if (direction == 0)
        return;

    // Snapshot first: writing in place would read values already overwritten.
    const auto pattern = copyLane (state, lane);

    const int shift = direction > 0 ? 1 : numSteps - 1;

    for (int step = 0; step < numSteps; ++step)
    {
        const int source = (step + numSteps - shift) % numSteps;

        setParam (state, stepValueId    (lane, step), pattern.values[source]);
        setParam (state, stepOnId       (lane, step), pattern.enabled[source] ? 1.0f : 0.0f);
        setParam (state, stepChanceId   (lane, step), pattern.chance[source]);
        setParam (state, stepVelocityId (lane, step), pattern.velocity[source]);
    }
}

LanePattern copyLane (juce::AudioProcessorValueTreeState& state, int lane)
{
    LanePattern pattern;

    for (int step = 0; step < numSteps; ++step)
    {
        pattern.values[step]   = getParam (state, stepValueId (lane, step));
        pattern.enabled[step]  = getParam (state, stepOnId (lane, step)) > 0.5f;
        pattern.chance[step]   = getParam (state, stepChanceId (lane, step));
        pattern.velocity[step] = getParam (state, stepVelocityId (lane, step));
    }

    pattern.valid = true;
    return pattern;
}

void pasteLane (juce::AudioProcessorValueTreeState& state, int lane, const LanePattern& pattern)
{
    if (! pattern.valid)
        return;

    for (int step = 0; step < numSteps; ++step)
    {
        setParam (state, stepValueId    (lane, step), pattern.values[step]);
        setParam (state, stepOnId       (lane, step), pattern.enabled[step] ? 1.0f : 0.0f);
        setParam (state, stepChanceId   (lane, step), pattern.chance[step]);
        setParam (state, stepVelocityId (lane, step), pattern.velocity[step]);
    }
}

} // namespace params
