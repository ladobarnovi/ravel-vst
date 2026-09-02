#include "Parameters.h"

#include <vector>

namespace params
{

namespace
{
    // "n"/"c" rather than the old shared "l": the two pools' parameters are entirely
    // separate VST3 parameters, not the same lane wearing two hats.
    juce::String lanePrefix (int lane, LaneKind kind)
    {
        return (kind == LaneKind::cc ? "c" : "n") + juce::String (lane + 1);
    }

    juce::String stepPrefix (int lane, int step, LaneKind kind)
    {
        return lanePrefix (lane, kind) + "_s" + juce::String (step + 1);
    }

    // Always the note-lane prefix: stepVelocityId/stepGateId are note-only.
    juce::String notePrefix (int lane) { return lanePrefix (lane, LaneKind::note); }

    // laneCcOnId etc. always address the CC pool.
    juce::String ccPrefix (int lane) { return lanePrefix (lane, LaneKind::cc); }
}

juce::String stepValueId  (int lane, int step, LaneKind kind) { return stepPrefix (lane, step, kind) + "_val"; }
juce::String stepOnId     (int lane, int step, LaneKind kind) { return stepPrefix (lane, step, kind) + "_on"; }
juce::String stepChanceId (int lane, int step, LaneKind kind) { return stepPrefix (lane, step, kind) + "_chance"; }

juce::String stepVelocityId (int lane, int step) { return notePrefix (lane) + "_s" + juce::String (step + 1) + "_vel"; }
juce::String stepGateId     (int lane, int step) { return notePrefix (lane) + "_s" + juce::String (step + 1) + "_gate"; }

juce::String laneOnId       (int lane, LaneKind kind) { return lanePrefix (lane, kind) + "_on"; }
juce::String laneLengthId   (int lane, LaneKind kind) { return lanePrefix (lane, kind) + "_length"; }
juce::String laneDivId      (int lane, LaneKind kind) { return lanePrefix (lane, kind) + "_div"; }
juce::String laneDirId      (int lane, LaneKind kind) { return lanePrefix (lane, kind) + "_dir"; }
juce::String laneDepthId    (int lane, LaneKind kind) { return lanePrefix (lane, kind) + "_depth"; }
juce::String laneModeId     (int lane, LaneKind kind) { return lanePrefix (lane, kind) + "_mode"; }
juce::String laneNudgeId    (int lane, LaneKind kind) { return lanePrefix (lane, kind) + "_nudge"; }
juce::String laneHumanizeId (int lane, LaneKind kind) { return lanePrefix (lane, kind) + "_humanize"; }

juce::String laneCcOnId     (int lane) { return ccPrefix (lane) + "_on_send"; }
juce::String laneCcNumId    (int lane) { return ccPrefix (lane) + "_num"; }
juce::String laneCcChanId   (int lane) { return ccPrefix (lane) + "_chan"; }
juce::String laneCcOffsetId (int lane) { return ccPrefix (lane) + "_offset"; }

namespace
{
    constexpr int versionHint = 1;

    // Every lane starts as eight steps of zero: a flat pattern on the root, which is a blank
    // sheet to draw on rather than a demo to clear away first. A canned pattern would also
    // make the second lane you add sound like it came with an opinion.
    constexpr float defaultValues[numLanes][numSteps] {};

    constexpr int   defaultLength[numLanes]   { numSteps, numSteps, numSteps, numSteps };

    // Rates do differ per lane, because a stack of lanes all running at 1/16 is the one
    // starting point that cannot demonstrate what the plugin is for.
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

    for (auto kind : { LaneKind::note, LaneKind::cc })
    {
        const bool isCc = kind == LaneKind::cc;

        for (int lane = 0; lane < numLanes; ++lane)
        {
            const auto laneName = juce::String (isCc ? "CC Lane " : "Note Lane ") + juce::String (lane + 1) + " ";

            for (int step = 0; step < numSteps; ++step)
            {
                const auto stepName = laneName + "Step " + juce::String (step + 1);

                layout.add (std::make_unique<juce::AudioParameterFloat> (
                    juce::ParameterID { stepValueId (lane, step, kind), versionHint },
                    stepName,
                    juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f),
                    defaultValues[lane][step],
                    juce::AudioParameterFloatAttributes().withStringFromValueFunction (percentText)));

                layout.add (std::make_unique<juce::AudioParameterBool> (
                    juce::ParameterID { stepOnId (lane, step, kind), versionHint },
                    stepName + " On",
                    true));

                layout.add (std::make_unique<juce::AudioParameterFloat> (
                    juce::ParameterID { stepChanceId (lane, step, kind), versionHint },
                    stepName + " Chance",
                    juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f),
                    1.0f,
                    juce::AudioParameterFloatAttributes().withStringFromValueFunction (percentText)));

                // Velocity and gate are only ever arguments to startNote, so a CC lane --
                // which never starts one -- carries neither. Skipping them here is what
                // keeps the plugin's automatable parameter count from doubling for nothing.
                if (isCc)
                    continue;

                // The step's own accent, as a trim on the global Velocity rather than an
                // absolute number, so a pattern keeps its shape when the global is moved.
                // 100% is unity.
                //
                // Attenuate-only: the global sets the ceiling for every note the plugin
                // fires, and a range that went above unity would draw every untouched step
                // as a half-height bar in the editor's velocity layer -- reading as "half"
                // when it means "unchanged".
                layout.add (std::make_unique<juce::AudioParameterFloat> (
                    juce::ParameterID { stepVelocityId (lane, step), versionHint },
                    stepName + " Velocity",
                    juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f),
                    1.0f,
                    juce::AudioParameterFloatAttributes().withStringFromValueFunction (percentText)));

                // How long this step's note is held, as a percentage of the step's own
                // length. Per step rather than a single master, so a lane can stab on some
                // steps and hold on others. Above 100% overlaps into the following step --
                // see Voices.
                layout.add (std::make_unique<juce::AudioParameterFloat> (
                    juce::ParameterID { stepGateId (lane, step), versionHint },
                    stepName + " Gate",
                    juce::NormalisableRange<float> (5.0f, 200.0f, 1.0f),
                    60.0f,
                    juce::AudioParameterFloatAttributes().withStringFromValueFunction (
                        [] (float v, int) { return juce::String (juce::roundToInt (v)) + " %"; })));
            }

            // The lane's own mute. True is "playing", and it is what a session saved before
            // this existed loads with, so nothing goes silent on upgrade. Muting reads as
            // every step in the lane being switched off at once -- see the engine.
            layout.add (std::make_unique<juce::AudioParameterBool> (
                juce::ParameterID { laneOnId (lane, kind), versionHint },
                laneName + "On", true));

            layout.add (std::make_unique<juce::AudioParameterInt> (
                juce::ParameterID { laneLengthId (lane, kind), versionHint },
                laneName + "Length", 1, numSteps, defaultLength[lane]));

            layout.add (std::make_unique<juce::AudioParameterChoice> (
                juce::ParameterID { laneDivId (lane, kind), versionHint },
                laneName + "Division", divisionNames, defaultDivision[lane]));

            layout.add (std::make_unique<juce::AudioParameterFloat> (
                juce::ParameterID { laneDepthId (lane, kind), versionHint },
                laneName + "Depth",
                juce::NormalisableRange<float> (-1.0f, 1.0f, 0.001f),
                defaultDepth[lane],
                juce::AudioParameterFloatAttributes().withStringFromValueFunction (percentText)));

            // Direction, Mix mode, Nudge and Humanize only ever shaped how a lane's own steps
            // traverse and fold -- a Note lane's nuance. A CC lane still folds into the Mix CC
            // the same way, just always Forward/Add/no-nudge/no-jitter: see LaneSnapshot's own
            // defaults, which is what an absent parameter here now leaves it reading.
            if (! isCc)
            {
                layout.add (std::make_unique<juce::AudioParameterChoice> (
                    juce::ParameterID { laneDirId (lane, kind), versionHint },
                    laneName + "Direction", directionNames, 0));

                layout.add (std::make_unique<juce::AudioParameterChoice> (
                    juce::ParameterID { laneModeId (lane, kind), versionHint },
                    laneName + "Mode", modeNames, modeAdd));

                layout.add (std::make_unique<juce::AudioParameterFloat> (
                    juce::ParameterID { laneNudgeId (lane, kind), versionHint },
                    laneName + "Nudge",
                    juce::NormalisableRange<float> (-1.0f, 1.0f, 0.01f), 0.0f,
                    juce::AudioParameterFloatAttributes().withStringFromValueFunction (percentText)));

                layout.add (std::make_unique<juce::AudioParameterFloat> (
                    juce::ParameterID { laneHumanizeId (lane, kind), versionHint },
                    laneName + "Humanize",
                    juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.0f,
                    juce::AudioParameterFloatAttributes().withStringFromValueFunction (percentText)));
            }

            if (! isCc)
                continue;

            // A CC lane's own destination -- the whole reason it exists, not an optional tap.
            layout.add (std::make_unique<juce::AudioParameterBool> (
                juce::ParameterID { laneCcOnId (lane), versionHint },
                laneName + "Send", true));

            // Sensible unused defaults: CC 20, 21, 22, 23 for lanes 1-4.
            layout.add (std::make_unique<juce::AudioParameterInt> (
                juce::ParameterID { laneCcNumId (lane), versionHint },
                laneName + "Number", 0, 127, 20 + lane));

            layout.add (std::make_unique<juce::AudioParameterInt> (
                juce::ParameterID { laneCcChanId (lane), versionHint },
                laneName + "Channel", 1, 16, 1));

            // Own offset per lane rather than sharing the CC tab's own Offset, so each
            // lane's tap can be recentred independently of the Mix CC.
            layout.add (std::make_unique<juce::AudioParameterFloat> (
                juce::ParameterID { laneCcOffsetId (lane), versionHint },
                laneName + "Offset",
                juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.0f,
                juce::AudioParameterFloatAttributes().withStringFromValueFunction (percentText)));
        }
    }

    //==========================================================================
    // One lane to begin with in each pool. The other three exist as parameters from the
    // start -- a VST3 cannot add any later -- but stay silent and hidden until this is
    // raised. Independent counts: growing one stack costs the other no room.
    layout.add (std::make_unique<juce::AudioParameterInt> (
        juce::ParameterID { noteLaneCountId, versionHint }, "Note Lanes", 1, numLanes, 1));

    layout.add (std::make_unique<juce::AudioParameterInt> (
        juce::ParameterID { ccLaneCountId, versionHint }, "CC Lanes", 1, numLanes, 1));

    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { noteTriggerSrcId, versionHint }, "Trigger", triggerNames, 0));

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

    // Octaves, not raw scale steps -- the engine multiplies this by the current scale's degree
    // count (5 for a pentatonic, 53 for 53-EDO chromatic) in Quantize mode, or by 12 semitones
    // in continuous mode, so a given Range value spans the same musical distance regardless of
    // scale. Notes clamp to the MIDI range, so how much of a large Range is actually reachable
    // depends on Root.
    layout.add (std::make_unique<juce::AudioParameterInt> (
        juce::ParameterID { rangeOctavesId, versionHint }, "Range", 1, 10, 2,
        juce::AudioParameterIntAttributes().withStringFromValueFunction (
            [] (int v, int) { return juce::String (v) + " oct"; })));

    // The two lists are indexed by the same parameter value, so they have to stay in step.
    jassert (scaleNames.size() == numScales);

    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { scaleId, versionHint }, "Scale", scaleNames, 4));

    layout.add (std::make_unique<juce::AudioParameterInt> (
        juce::ParameterID { velocityId, versionHint }, "Velocity", 1, 127, 100));

    layout.add (std::make_unique<juce::AudioParameterInt> (
        juce::ParameterID { midiChannelId, versionHint }, "Note Channel", 1, 16, 1));

    // The CC tab's own Mix destination -- fed by the CC-lane fold, the same way pitch is
    // fed by the Note-lane fold.
    layout.add (std::make_unique<juce::AudioParameterInt> (
        juce::ParameterID { ccNumberId, versionHint }, "CC Number", 0, 127, 1));

    layout.add (std::make_unique<juce::AudioParameterInt> (
        juce::ParameterID { ccChannelId, versionHint }, "CC Channel", 1, 16, 1));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { noteOffsetId, versionHint }, "Offset",
        juce::NormalisableRange<float> (-1.0f, 1.0f, 0.001f), 0.0f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction (percentText)));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ccOffsetId, versionHint }, "CC Offset",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.0f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction (percentText)));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { slewId, versionHint }, "Slew",
        juce::NormalisableRange<float> (0.0f, 500.0f, 1.0f), 0.0f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction (
            [] (float v, int) { return juce::String (juce::roundToInt (v)) + " ms"; })));

    // Off by default: the sequencer follows the host transport, and a plugin that starts
    // emitting notes the moment it is loaded -- before anything has been pressed -- is a
    // surprise rather than a convenience. One shared switch for both pools -- see the
    // comment on freeRunId.
    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { freeRunId, versionHint }, "Free Run", false));

    // 1 is the historical monophonic behaviour: a retrigger always closes the previous
    // note. Above 1, a Gate over 100% overlaps into the following step instead.
    layout.add (std::make_unique<juce::AudioParameterInt> (
        juce::ParameterID { voiceCountId, versionHint }, "Voices", 1, 8, 1));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { noteSwingId, versionHint }, "Swing",
        juce::NormalisableRange<float> (-1.0f, 1.0f, 0.01f), 0.0f,
        juce::AudioParameterFloatAttributes().withStringFromValueFunction (percentText)));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { ccSwingId, versionHint }, "CC Swing",
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

void randomiseLaneValues (juce::AudioProcessorValueTreeState& state, int lane, juce::Random& random,
                          LaneKind kind)
{
    for (int step = 0; step < numSteps; ++step)
        setParam (state, stepValueId (lane, step, kind), random.nextFloat());
}

void clearLaneValues (juce::AudioProcessorValueTreeState& state, int lane, LaneKind kind)
{
    for (int step = 0; step < numSteps; ++step)
        setParam (state, stepValueId (lane, step, kind), 0.0f);
}

void invertLaneValues (juce::AudioProcessorValueTreeState& state, int lane, LaneKind kind)
{
    for (int step = 0; step < numSteps; ++step)
        setParam (state, stepValueId (lane, step, kind),
                  1.0f - getParam (state, stepValueId (lane, step, kind)));
}

void rotateLane (juce::AudioProcessorValueTreeState& state, int lane, int direction, LaneKind kind)
{
    if (direction == 0)
        return;

    // Snapshot first: writing in place would read values already overwritten.
    const auto pattern = copyLane (state, lane, kind);

    const int shift = direction > 0 ? 1 : numSteps - 1;

    for (int step = 0; step < numSteps; ++step)
    {
        const int source = (step + numSteps - shift) % numSteps;

        setParam (state, stepValueId  (lane, step, kind), pattern.values[source]);
        setParam (state, stepOnId     (lane, step, kind), pattern.enabled[source] ? 1.0f : 0.0f);
        setParam (state, stepChanceId (lane, step, kind), pattern.chance[source]);

        // A CC lane has neither -- only a note lane's velocity and gate move with the rest.
        if (kind == LaneKind::note)
        {
            setParam (state, stepVelocityId (lane, step), pattern.velocity[source]);
            setParam (state, stepGateId     (lane, step), pattern.gate[source]);
        }
    }
}

//==============================================================================
namespace
{
    /** Every parameter ID a lane owns, in a fixed order.

        Copying a lane and resetting one both walk this, so the two cannot drift apart -- and
        because the order only has to be self-consistent, two calls for different lanes line
        up index for index.
    */
    std::vector<juce::String> laneParameterIds (int lane, LaneKind kind)
    {
        std::vector<juce::String> ids;
        ids.reserve ((size_t) (numSteps * 5 + 12));

        for (int step = 0; step < numSteps; ++step)
        {
            ids.push_back (stepValueId  (lane, step, kind));
            ids.push_back (stepOnId     (lane, step, kind));
            ids.push_back (stepChanceId (lane, step, kind));

            if (kind == LaneKind::note)
            {
                ids.push_back (stepVelocityId (lane, step));
                ids.push_back (stepGateId     (lane, step));
            }
        }

        ids.push_back (laneOnId       (lane, kind));
        ids.push_back (laneLengthId   (lane, kind));
        ids.push_back (laneDivId      (lane, kind));
        ids.push_back (laneDepthId    (lane, kind));

        if (kind == LaneKind::note)
        {
            ids.push_back (laneDirId      (lane, kind));
            ids.push_back (laneModeId     (lane, kind));
            ids.push_back (laneNudgeId    (lane, kind));
            ids.push_back (laneHumanizeId (lane, kind));
        }

        if (kind == LaneKind::cc)
        {
            ids.push_back (laneCcOnId     (lane));
            ids.push_back (laneCcNumId    (lane));
            ids.push_back (laneCcChanId   (lane));
            ids.push_back (laneCcOffsetId (lane));
        }

        return ids;
    }

    /** Moves one lane's parameters onto another's of the same kind.

        Normalised values, so nothing here has to know the range or the type of any individual
        parameter -- a lane holds floats, ints, choices and bools, and this treats them alike.
    */
    void copyLaneParameters (juce::AudioProcessorValueTreeState& state, int from, int to, LaneKind kind)
    {
        const auto sourceIds = laneParameterIds (from, kind);
        const auto targetIds = laneParameterIds (to, kind);

        for (size_t i = 0; i < sourceIds.size(); ++i)
        {
            auto* source = state.getParameter (sourceIds[(size_t) i]);
            auto* target = state.getParameter (targetIds[(size_t) i]);

            if (source == nullptr || target == nullptr)
                continue;

            target->beginChangeGesture();
            target->setValueNotifyingHost (source->getValue());
            target->endChangeGesture();
        }
    }

    /** Puts a lane back to what a freshly loaded instance would have given it. Read from each
        parameter's own default rather than written out here, because a lane's defaults are
        not the same for every lane -- the rate and the CC number both depend on which one it
        is.
    */
    void resetLaneParameters (juce::AudioProcessorValueTreeState& state, int lane, LaneKind kind)
    {
        for (const auto& id : laneParameterIds (lane, kind))
        {
            if (auto* parameter = state.getParameter (id))
            {
                parameter->beginChangeGesture();
                parameter->setValueNotifyingHost (parameter->getDefaultValue());
                parameter->endChangeGesture();
            }
        }
    }

    const char* laneCountIdFor (LaneKind kind)
    {
        return kind == LaneKind::cc ? ccLaneCountId : noteLaneCountId;
    }
}

void removeLane (juce::AudioProcessorValueTreeState& state, int lane, LaneKind kind)
{
    const int count = (int) std::lround (getParam (state, laneCountIdFor (kind)));

    // An instance always has at least one lane, and a lane it does not have cannot go.
    if (count <= 1 || lane < 0 || lane >= count)
        return;

    // Upwards from the hole, so each lane is read before the one below it is overwritten.
    for (int target = lane; target < count - 1; ++target)
        copyLaneParameters (state, target + 1, target, kind);

    resetLaneParameters (state, count - 1, kind);

    setParam (state, laneCountIdFor (kind), (float) (count - 1));
}

//==============================================================================
LanePattern copyLane (juce::AudioProcessorValueTreeState& state, int lane, LaneKind kind)
{
    LanePattern pattern;

    for (int step = 0; step < numSteps; ++step)
    {
        pattern.values[step]  = getParam (state, stepValueId (lane, step, kind));
        pattern.enabled[step] = getParam (state, stepOnId (lane, step, kind)) > 0.5f;
        pattern.chance[step]  = getParam (state, stepChanceId (lane, step, kind));

        if (kind == LaneKind::note)
        {
            pattern.velocity[step] = getParam (state, stepVelocityId (lane, step));
            pattern.gate[step]     = getParam (state, stepGateId (lane, step));
        }
    }

    pattern.valid = true;
    return pattern;
}

void pasteLane (juce::AudioProcessorValueTreeState& state, int lane, const LanePattern& pattern, LaneKind kind)
{
    if (! pattern.valid)
        return;

    for (int step = 0; step < numSteps; ++step)
    {
        setParam (state, stepValueId  (lane, step, kind), pattern.values[step]);
        setParam (state, stepOnId     (lane, step, kind), pattern.enabled[step] ? 1.0f : 0.0f);
        setParam (state, stepChanceId (lane, step, kind), pattern.chance[step]);

        if (kind == LaneKind::note)
        {
            setParam (state, stepVelocityId (lane, step), pattern.velocity[step]);
            setParam (state, stepGateId     (lane, step), pattern.gate[step]);
        }
    }
}

} // namespace params
