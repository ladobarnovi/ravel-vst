#include "Parameters.h"

namespace params
{

juce::String stepValueId  (int lane, int step) { return "l" + juce::String (lane + 1) + "_s" + juce::String (step + 1) + "_val"; }
juce::String stepOnId     (int lane, int step) { return "l" + juce::String (lane + 1) + "_s" + juce::String (step + 1) + "_on"; }
juce::String laneLengthId (int lane)           { return "l" + juce::String (lane + 1) + "_length"; }
juce::String laneDivId    (int lane)           { return "l" + juce::String (lane + 1) + "_div"; }
juce::String laneDirId    (int lane)           { return "l" + juce::String (lane + 1) + "_dir"; }
juce::String laneDepthId  (int lane)           { return "l" + juce::String (lane + 1) + "_depth"; }
juce::String laneModeId   (int lane)           { return "l" + juce::String (lane + 1) + "_mode"; }

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
    };

    constexpr int   defaultLength[numLanes]   { 8, 5, 3 };
    constexpr int   defaultDivision[numLanes] { divIndex_1_16, divIndex_1_8, divIndex_1_4 };
    constexpr float defaultDepth[numLanes]    { 1.0f, 0.0f, 0.0f };

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
        }

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
    }

    //==========================================================================
    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { outputModeId, versionHint }, "Output", outputModeNames, outNotes));

    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { triggerSrcId, versionHint }, "Trigger", triggerNames, 0));

    // Defaults to Semitone so existing sessions keep the behaviour they were saved with.
    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { pitchModeId, versionHint }, "Pitch", pitchModeNames, pitchSemitone));

    layout.add (std::make_unique<juce::AudioParameterInt> (
        juce::ParameterID { bendRangeId, versionHint }, "Bend Range", 1, 48, 2,
        juce::AudioParameterIntAttributes().withStringFromValueFunction (
            [] (int v, int) { return juce::String (v) + " st"; })));

    layout.add (std::make_unique<juce::AudioParameterInt> (
        juce::ParameterID { rootNoteId, versionHint }, "Root", 0, 127, 48,
        juce::AudioParameterIntAttributes().withStringFromValueFunction (
            [] (int v, int) { return noteNameText (v); })));

    layout.add (std::make_unique<juce::AudioParameterInt> (
        juce::ParameterID { rangeStepsId, versionHint }, "Range", 1, 36, 12));

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

    return layout;
}

} // namespace params
