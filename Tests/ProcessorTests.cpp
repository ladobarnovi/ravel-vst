/*
    Drives the real RavelAudioProcessor::processBlock through a mock playhead.

    EngineTests covers the sequencer core; this covers the layer above it -- playhead
    handling, the free-run fallback, the parameter snapshot, and the plugin's declared
    MIDI capabilities. That is the layer where the plugin could compile, load and still
    emit nothing, so it is worth asserting on directly rather than only in a host.
*/

#include "PluginProcessor.h"

#include <chrono>
#include <cstdio>
#include <vector>

namespace
{
    int checksRun    = 0;
    int checksFailed = 0;

    void check (bool condition, const char* what)
    {
        ++checksRun;

        if (condition)
        {
            std::printf ("    ok    %s\n", what);
        }
        else
        {
            ++checksFailed;
            std::printf ("    FAIL  %s\n", what);
        }
    }

    void section (const char* name)
    {
        std::printf ("\n  %s\n", name);
    }

    struct MockPlayHead final : juce::AudioPlayHead
    {
        juce::Optional<PositionInfo> getPosition() const override { return info; }
        PositionInfo info;
    };

    struct Counts
    {
        int noteOns = 0;
        int noteOffs = 0;
        int controllers = 0;
        int firstNote = -1;
        int firstVelocity = -1;
        int firstCcNumber = -1;
    };

    /** Runs the processor for `totalSamples`, advancing the mock playhead exactly as a
        host would, and tallies what comes out of the MIDI buffer.
    */
    Counts runProcessor (RavelAudioProcessor& processor,
                         MockPlayHead& playHead,
                         bool hostPlaying,
                         int totalSamples,
                         int blockSize = 512)
    {
        Counts counts;

        juce::AudioBuffer<float> audio (2, blockSize);
        juce::MidiBuffer midi;

        playHead.info.setBpm (120.0);
        playHead.info.setIsPlaying (hostPlaying);

        for (int pos = 0; pos < totalSamples; pos += blockSize)
        {
            const int numSamples = juce::jmin (blockSize, totalSamples - pos);

            // 120 bpm at 48 kHz -> 1/24000 quarter notes per sample.
            playHead.info.setPpqPosition ((double) pos / 24000.0);
            playHead.info.setTimeInSamples ((juce::int64) pos);

            audio.clear();
            midi.clear();

            // Alias the storage so the buffer reports exactly numSamples on a partial
            // block -- passing the full-size buffer would make the processor advance
            // further than the timeline we set up.
            juce::AudioBuffer<float> block (audio.getArrayOfWritePointers(), 2, numSamples);

            processor.processBlock (block, midi);

            for (const auto metadata : midi)
            {
                const auto message = metadata.getMessage();

                if (message.isNoteOn())
                {
                    ++counts.noteOns;

                    if (counts.firstNote < 0)
                    {
                        counts.firstNote     = message.getNoteNumber();
                        counts.firstVelocity = message.getVelocity();
                    }
                }
                else if (message.isNoteOff())
                {
                    ++counts.noteOffs;
                }
                else if (message.isController())
                {
                    ++counts.controllers;

                    if (counts.firstCcNumber < 0)
                        counts.firstCcNumber = message.getControllerNumber();
                }
            }
        }

        return counts;
    }

    void setChoice (RavelAudioProcessor& processor, const juce::String& paramID, int index)
    {
        auto* param = processor.apvts.getParameter (paramID);
        jassert (param != nullptr);
        param->setValueNotifyingHost (param->convertTo0to1 ((float) index));
    }

    /** What a parameter comes up at, in plain (denormalised) units.

        Read back from the parameter rather than spelled out at each call site, because a
        literal here goes stale the moment a default moves and takes the test's meaning with
        it -- which is exactly what happened when the note-lane step default went from 0 to
        0.25 and left seven assertions in this file checking against a number the plugin no
        longer used. Asking the parameter keeps "back where it started" meaning that whatever
        the default becomes.
    */
    float defaultOf (RavelAudioProcessor& processor, const juce::String& paramID)
    {
        if (auto* param = dynamic_cast<juce::RangedAudioParameter*> (
                              processor.apvts.getParameter (paramID)))
            return param->getNormalisableRange().convertFrom0to1 (param->getDefaultValue());

        jassertfalse;
        return 0.0f;
    }
}

//==============================================================================
int main()
{
    // APVTS inherits from Timer, so the message manager has to exist.
    const juce::ScopedJuceInitialiser_GUI juceInit;

    std::printf ("Ravel processor tests\n");

    //==========================================================================
    section ("Declared capabilities (what the host reads to decide routing)");
    {
        RavelAudioProcessor processor;

        check (processor.producesMidi(), "producesMidi() is true -- required for Live to offer it as a MIDI source");
        check (processor.acceptsMidi(),  "acceptsMidi() is true");
        check (! processor.isMidiEffect(), "isMidiEffect() is false -- it loads as an instrument");
        check (processor.getTotalNumOutputChannels() == 2, "declares a stereo output bus");
    }

    //==========================================================================
    section ("What a freshly loaded instance starts as");
    {
        RavelAudioProcessor processor;

        const auto value = [&processor] (const juce::String& id)
        {
            const auto* p = processor.apvts.getRawParameterValue (id);
            return p != nullptr ? p->load() : -1.0f;
        };

        check ((int) std::lround (value (params::noteLaneCountId)) == 1, "one lane");
        check (value (params::freeRunId) < 0.5f, "Free Run off, so it follows the transport");

        bool flat = true, fullLength = true;

        // "Flat" means every step in a lane agrees with the others, not that every step is
        // zero and not that the lanes agree with each other: the first Note lane comes up
        // pitched so a fresh instance is audibly doing something, and the rest come up at zero
        // so a lane you add is a blank sheet. So each lane is measured against its own default.
        for (int lane = 0; lane < params::numLanes; ++lane)
        {
            fullLength = fullLength
                          && (int) std::lround (value (params::laneLengthId (lane))) == params::numSteps;

            const float laneDefault = defaultOf (processor, params::stepValueId (lane, 0));

            for (int step = 0; step < params::numSteps; ++step)
                flat = flat && std::abs (value (params::stepValueId (lane, step)) - laneDefault) < 1.0e-6f;
        }

        check (flat, "every lane is sixteen steps of one value");
        check (fullLength, "and sixteen steps long");

        //----------------------------------------------------------------------
        // The first lane carries the audible default; every lane after it starts silent, so
        // adding a lane gives something to draw on rather than a pattern to clear away.
        check (defaultOf (processor, params::stepValueId (0, 0)) > 0.0f,
               "the first Note lane comes up pitched");

        bool laterLanesSilent = true;

        for (int lane = 1; lane < params::numLanes; ++lane)
            for (int step = 0; step < params::numSteps; ++step)
                laterLanesSilent = laterLanesSilent
                                     && defaultOf (processor, params::stepValueId (lane, step)) == 0.0f;

        check (laterLanesSilent, "every Note lane after the first comes up flat at zero");

        MockPlayHead playHead;
        processor.setPlayConfigDetails (0, 2, 48000.0, 512);
        processor.prepareToPlay (48000.0, 512);
        processor.setPlayHead (&playHead);

        const auto counts = runProcessor (processor, playHead, false, 48000);

        check (counts.noteOns == 0, "and it stays silent until the host starts playing");
    }

    //==========================================================================
    section ("Host transport running");
    {
        RavelAudioProcessor processor;
        processor.setPlayConfigDetails (0, 2, 48000.0, 512);
        processor.prepareToPlay (48000.0, 512);

        MockPlayHead playHead;
        processor.setPlayHead (&playHead);

        // One second at 120 bpm = 2 beats = 8 sixteenths.
        const auto counts = runProcessor (processor, playHead, true, 48000);

        check (counts.noteOns == 8, "eight notes over two beats at the default 1/16 rate");
        check (counts.noteOffs == counts.noteOns, "every note-on is matched by a note-off");
        // Derived from the defaults rather than typed in, so moving Root, Range or the step
        // default moves this with them instead of leaving it asserting a stale pitch. A stock
        // instance has Quantize off, one lane at full Depth, and every step at the same value,
        // so the mix that reaches pitchFor() is just that step default.
        const float defaultMix   = defaultOf (processor, params::stepValueId (0, 0))
                                     * defaultOf (processor, params::laneDepthId (0));
        const int   defaultRoot  = (int) std::lround (defaultOf (processor, params::rootNoteId));
        const int   defaultRange = (int) std::lround (defaultOf (processor, params::rangeOctavesId));

        const int expectedNote = juce::jlimit (0, 127, (int) std::lround (
            (float) defaultRoot + params::continuousSemitones (defaultMix, defaultRange * 12)));

        check (counts.firstNote == expectedNote,
               "first note is the root transposed by the default step value");
        check (counts.firstVelocity == params::fixedVelocity,
               "and plays at the pinned master velocity, 100");
        check (counts.controllers > 0, "CC is emitted alongside notes -- both are always on");
    }

    //==========================================================================
    section ("Free Run (transport stopped)");
    {
        RavelAudioProcessor processor;
        processor.setPlayConfigDetails (0, 2, 48000.0, 512);
        processor.prepareToPlay (48000.0, 512);

        // Free Run is off by default, so this is the one place that has to switch it on.
        if (auto* freeRun = processor.apvts.getParameter (params::freeRunId))
            freeRun->setValueNotifyingHost (1.0f);

        MockPlayHead playHead;
        processor.setPlayHead (&playHead);

        const auto counts = runProcessor (processor, playHead, false, 48000);

        check (counts.noteOns > 0, "notes are still produced while the transport is stopped");
    }

    //==========================================================================
    section ("Free Run disabled + transport stopped = silence");
    {
        RavelAudioProcessor processor;
        processor.setPlayConfigDetails (0, 2, 48000.0, 512);
        processor.prepareToPlay (48000.0, 512);

        MockPlayHead playHead;
        processor.setPlayHead (&playHead);

        const auto counts = runProcessor (processor, playHead, false, 48000);

        check (counts.noteOns == 0, "nothing fires with Free Run off and the transport stopped");
    }

    //==========================================================================
    section ("No playhead at all (host provides none)");
    {
        RavelAudioProcessor processor;
        processor.setPlayConfigDetails (0, 2, 48000.0, 512);
        processor.prepareToPlay (48000.0, 512);

        if (auto* freeRun = processor.apvts.getParameter (params::freeRunId))
            freeRun->setValueNotifyingHost (1.0f);

        juce::AudioBuffer<float> audio (2, 512);
        juce::MidiBuffer midi;
        int noteOns = 0;

        for (int i = 0; i < 100; ++i)
        {
            audio.clear();
            midi.clear();
            processor.processBlock (audio, midi);

            for (const auto metadata : midi)
                if (metadata.getMessage().isNoteOn())
                    ++noteOns;
        }

        check (noteOns > 0, "free-run still drives the sequencer with no playhead present");
    }

    //==========================================================================
    section ("Pattern actions act on the selected row");
    {
        RavelAudioProcessor processor;
        juce::Random random (0x1234);

        const auto plain = [&processor] (const juce::String& id)
        {
            return processor.apvts.getRawParameterValue (id)->load();
        };

        const auto valueOf  = [&] (int step) { return plain (params::stepValueId (0, step)); };
        const auto chanceOf = [&] (int step) { return plain (params::stepChanceId (0, step)); };
        const auto gateOf   = [&] (int step) { return plain (params::stepGateId (0, step)); };

        //---------------------------------------------------------------------- randomise
        // Chance starts at 1 on every step, so anything below it is this action's doing.
        params::randomiseLaneRow (processor.apvts, 0, random, params::LaneKind::note,
                                  params::StepLayer::chance);

        bool chanceMoved = false;
        bool valueUntouched = true;

        for (int step = 0; step < params::numSteps; ++step)
        {
            chanceMoved = chanceMoved || chanceOf (step) < 0.999f;

            // Note lanes start every step at 0.25; randomising Chance must not have moved it.
            valueUntouched = valueUntouched && std::abs (valueOf (step) - 0.25f) < 1.0e-6f;
        }

        check (chanceMoved, "randomise moves the selected row");
        check (valueUntouched, "randomise leaves the rows it was not pointed at alone");

        //---------------------------------------------------------------------- own range
        // Gate runs 5..200, not 0..1: randomising it must spread over its own range rather
        // than pinning every step to the bottom of it.
        params::randomiseLaneRow (processor.apvts, 0, random, params::LaneKind::note,
                                  params::StepLayer::gate);

        bool gateInRange = true;
        bool gateAboveUnity = false;

        for (int step = 0; step < params::numSteps; ++step)
        {
            gateInRange = gateInRange && gateOf (step) >= 5.0f && gateOf (step) <= 200.0f;
            gateAboveUnity = gateAboveUnity || gateOf (step) > 1.0f;
        }

        check (gateInRange, "randomise stays inside the selected row's own range");
        check (gateAboveUnity, "randomise spreads over the row's own range, not over 0..1");

        //---------------------------------------------------------------------- clear
        // Clearing Chance returns it to 1, not to 0: a row of zeroes there is a lane that
        // never fires, which is switching the lane off rather than clearing it.
        params::clearLaneRow (processor.apvts, 0, params::LaneKind::note,
                              params::StepLayer::chance);

        bool chanceNeutral = true;

        for (int step = 0; step < params::numSteps; ++step)
            chanceNeutral = chanceNeutral && std::abs (chanceOf (step) - 1.0f) < 1.0e-6f;

        check (chanceNeutral, "clear puts the selected row back to its neutral, not to zero");

        //---------------------------------------------------------------------- invert
        params::clearLaneRow (processor.apvts, 0, params::LaneKind::note, params::StepLayer::gate);
        params::invertLaneRow (processor.apvts, 0, params::LaneKind::note, params::StepLayer::gate);

        // 60 sits at (60-5)/195 = 0.282 of the way up Gate's range; mirrored that is 0.718,
        // which is 145. Mirroring in 0..1 instead would have given 140.
        check (std::abs (gateOf (0) - 145.0f) < 1.5f,
               "invert mirrors about the middle of the row's own range");

        //---------------------------------------------------------------------- CC lanes
        // A CC lane has no velocity row at all. Asking for one must do nothing rather than
        // reaching the note lane of the same number that stepVelocityId would resolve to.
        const float noteVelocityBefore = plain (params::stepVelocityId (0, 0));

        params::randomiseLaneRow (processor.apvts, 0, random, params::LaneKind::cc,
                                  params::StepLayer::velocity);

        check (std::abs (plain (params::stepVelocityId (0, 0)) - noteVelocityBefore) < 1.0e-6f,
               "a row the lane kind does not have is skipped, not redirected");
    }

    //==========================================================================
    section ("Per-lane randomise and clear");
    {
        RavelAudioProcessor processor;
        juce::Random random (0x5eed);

        const auto stepValue = [&processor] (int lane, int step)
        {
            return processor.apvts.getRawParameterValue (params::stepValueId (lane, step))->load();
        };

        const auto stepEnabled = [&processor] (int lane, int step)
        {
            return processor.apvts.getRawParameterValue (params::stepOnId (lane, step))->load() > 0.5f;
        };

        //----------------------------------------------------------------------
        // Every lane starts at zero now, so lane 2 is given something to lose before lane 1
        // is cleared -- otherwise a clear that reached across lanes would look like a pass.
        for (int step = 0; step < params::numSteps; ++step)
            if (auto* p = processor.apvts.getParameter (params::stepValueId (1, step)))
                p->setValueNotifyingHost (p->convertTo0to1 (0.5f));

        params::randomiseLaneRow (processor.apvts, 0, random);
        params::clearLaneRow (processor.apvts, 0);

        bool allZero = true;

        for (int step = 0; step < params::numSteps; ++step)
            allZero = allZero && std::abs (stepValue (0, step)) < 1.0e-6f;

        check (allZero, "clear zeroes every step value in the lane");

        bool otherLanesIntact = false;

        for (int step = 0; step < params::numSteps; ++step)
            if (stepValue (1, step) > 0.0f)
                otherLanesIntact = true;

        check (otherLanesIntact, "clear leaves the other lanes untouched");

        //----------------------------------------------------------------------
        params::randomiseLaneRow (processor.apvts, 0, random);

        bool anyNonZero = false;
        bool varied = false;

        for (int step = 0; step < params::numSteps; ++step)
        {
            if (stepValue (0, step) > 0.0f)
                anyNonZero = true;

            if (std::abs (stepValue (0, step) - stepValue (0, 0)) > 1.0e-4f)
                varied = true;
        }

        check (anyNonZero, "randomise writes non-zero values");
        check (varied, "randomise gives the steps differing values");

        bool inRange = true;

        for (int step = 0; step < params::numSteps; ++step)
            inRange = inRange && stepValue (0, step) >= 0.0f && stepValue (0, step) <= 1.0f;

        check (inRange, "randomised values stay inside 0..1");

        //----------------------------------------------------------------------
        bool togglesIntact = true;

        for (int step = 0; step < params::numSteps; ++step)
            togglesIntact = togglesIntact && stepEnabled (0, step);

        check (togglesIntact, "neither action disturbs the step on/off toggles");
    }

    //==========================================================================
    section ("Pattern actions: invert, rotate, copy/paste");
    {
        RavelAudioProcessor processor;

        const auto value = [&processor] (int lane, int step)
        {
            return processor.apvts.getRawParameterValue (params::stepValueId (lane, step))->load();
        };

        const auto setValue = [&processor] (int lane, int step, float v)
        {
            if (auto* p = processor.apvts.getParameter (params::stepValueId (lane, step)))
                p->setValueNotifyingHost (p->convertTo0to1 (v));
        };

        const auto setEnabled = [&processor] (int lane, int step, bool on)
        {
            if (auto* p = processor.apvts.getParameter (params::stepOnId (lane, step)))
                p->setValueNotifyingHost (on ? 1.0f : 0.0f);
        };

        // Stays under 0.5 at the last step regardless of numSteps, so the synthetic pattern
        // below never clips against the parameter's 0..1 range the way a fixed /10.0f did
        // once numSteps passed 10.
        const auto stepValue = [] (int step) { return (float) step / (float) (params::numSteps * 2); };

        //---------------------------------------------------------------------- invert
        for (int step = 0; step < params::numSteps; ++step)
            setValue (0, step, stepValue (step));

        params::invertLaneRow (processor.apvts, 0);

        bool inverted = true;

        for (int step = 0; step < params::numSteps; ++step)
            inverted = inverted && std::abs (value (0, step) - (1.0f - stepValue (step))) < 0.01f;

        check (inverted, "invert mirrors every value about the midpoint");

        //---------------------------------------------------------------------- rotate
        for (int step = 0; step < params::numSteps; ++step)
        {
            setValue (0, step, stepValue (step));
            setEnabled (0, step, step == 0);   // only step 0 enabled, so we can track it
        }

        params::rotateLane (processor.apvts, 0, 1);

        // Rotating right by one: old step 0 is now step 1, and the old last step wraps to 0.
        const bool valuesMoved = std::abs (value (0, 1) - stepValue (0)) < 0.01f
                              && std::abs (value (0, 2) - stepValue (1)) < 0.01f
                              && std::abs (value (0, 0) - stepValue (params::numSteps - 1)) < 0.01f;

        check (valuesMoved, "rotate right shifts values round by one, wrapping");

        const bool onStateFollowed =
            processor.apvts.getRawParameterValue (params::stepOnId (0, 1))->load() > 0.5f
            && processor.apvts.getRawParameterValue (params::stepOnId (0, 0))->load() < 0.5f;

        check (onStateFollowed, "rotate moves the on/off state along with the value");

        params::rotateLane (processor.apvts, 0, -1);

        const bool roundTrip = std::abs (value (0, 0) - stepValue (0)) < 0.01f
                            && std::abs (value (0, params::numSteps - 1) - stepValue (params::numSteps - 1)) < 0.01f;

        check (roundTrip, "rotating left then undoes rotating right");

        //---------------------------------------------------------------------- copy/paste
        for (int step = 0; step < params::numSteps; ++step)
            setValue (1, step, 0.0f);

        const auto pattern = params::copyLane (processor.apvts, 0);
        check (pattern.valid, "copy produces a valid pattern");

        params::pasteLane (processor.apvts, 1, pattern);

        bool pasted = true;

        for (int step = 0; step < params::numSteps; ++step)
            pasted = pasted && std::abs (value (1, step) - value (0, step)) < 0.001f;

        check (pasted, "paste reproduces the source lane onto another lane");

        const params::LanePattern empty;
        for (int step = 0; step < params::numSteps; ++step)
            setValue (2, step, 0.25f);

        params::pasteLane (processor.apvts, 2, empty);

        check (std::abs (value (2, 0) - 0.25f) < 0.01f,
               "pasting an empty clipboard is a no-op");
    }

    //==========================================================================
    section ("Lane count decides how many lanes are heard");
    {
        RavelAudioProcessor processor;
        processor.setPlayConfigDetails (0, 2, 48000.0, 512);
        processor.prepareToPlay (48000.0, 512);

        MockPlayHead playHead;
        processor.setPlayHead (&playHead);

        // Poly mode, so each lane that plays contributes its own note-ons and the count
        // shows up directly in the tally. Over four 1/16 steps lane 1 fires four times and
        // lane 2, at 1/8, twice.
        setChoice (processor, params::polyModeId, 1);

        setChoice (processor, params::noteLaneCountId, 1);
        const auto one = runProcessor (processor, playHead, true, 4 * 6000);

        check (one.noteOns == 4, "a one-lane instance plays only lane 1");

        RavelAudioProcessor second;
        second.setPlayConfigDetails (0, 2, 48000.0, 512);
        second.prepareToPlay (48000.0, 512);
        second.setPlayHead (&playHead);
        setChoice (second, params::polyModeId, 1);
        setChoice (second, params::noteLaneCountId, 2);

        const auto two = runProcessor (second, playHead, true, 4 * 6000);

        check (two.noteOns == 6, "adding a lane brings its own clock in with it");
    }

    //==========================================================================
    // There is no message loop in a console app, so the window that coalesces one user action
    // into one undo step never reopens on its own. closeCurrentEdit() stands in for the loop
    // turning over, which is also what lets a test say exactly where it expects the seam
    // between two edits to fall.
    section ("Undo steps back through single edits");
    {
        RavelAudioProcessor processor;

        const auto value = [&processor] (int lane, int step)
        {
            return processor.apvts.getRawParameterValue (params::stepValueId (lane, step))->load();
        };

        const auto setValue = [&processor] (int lane, int step, float v)
        {
            if (auto* p = processor.apvts.getParameter (params::stepValueId (lane, step)))
            {
                p->beginChangeGesture();
                p->setValueNotifyingHost (p->convertTo0to1 (v));
                p->endChangeGesture();
            }
        };

        check (! processor.undoHistory.canUndo(), "a fresh instance has nothing to undo");

        setValue (0, 0, 0.25f);
        processor.undoHistory.closeCurrentEdit();

        setValue (0, 0, 0.75f);
        processor.undoHistory.closeCurrentEdit();

        check (processor.undoHistory.getUndoDepth() == 2, "two separate edits are two steps");

        check (processor.undoHistory.undo(), "undo reports that it moved");
        check (std::abs (value (0, 0) - 0.25f) < 0.01f, "the first undo restores the previous value");

        check (processor.undoHistory.undo(), "undo moves again");
        check (std::abs (value (0, 0) - defaultOf (processor, params::stepValueId (0, 0))) < 0.01f,
               "the second undo reaches the value it started at");

        check (! processor.undoHistory.undo(), "undo stops at the beginning rather than wrapping");
    }

    //==========================================================================
    section ("Redo retraces what undo walked back");
    {
        RavelAudioProcessor processor;

        const auto value = [&processor] ()
        {
            return processor.apvts.getRawParameterValue (params::stepValueId (0, 0))->load();
        };

        if (auto* p = processor.apvts.getParameter (params::stepValueId (0, 0)))
        {
            p->beginChangeGesture();
            p->setValueNotifyingHost (p->convertTo0to1 (0.5f));
            p->endChangeGesture();
        }

        processor.undoHistory.closeCurrentEdit();
        processor.undoHistory.undo();

        check (std::abs (value() - defaultOf (processor, params::stepValueId (0, 0))) < 0.01f,
               "undo took the value back");
        check (processor.undoHistory.canRedo(), "and left something to redo");

        check (processor.undoHistory.redo(), "redo reports that it moved");
        check (std::abs (value() - 0.5f) < 0.01f, "redo puts the value back");

        check (! processor.undoHistory.redo(), "redo stops at the top of the stack");

        //----------------------------------------------------------------------
        // Editing after an undo is a new branch: the states that were undone are no longer
        // anywhere the user can get back to, so holding them would be a trap.
        processor.undoHistory.undo();
        processor.undoHistory.closeCurrentEdit();

        if (auto* p = processor.apvts.getParameter (params::stepValueId (0, 0)))
        {
            p->beginChangeGesture();
            p->setValueNotifyingHost (p->convertTo0to1 (0.9f));
            p->endChangeGesture();
        }

        check (! processor.undoHistory.canRedo(), "a fresh edit discards the redo branch");
    }

    //==========================================================================
    section ("Spread is a row of the note lane's pattern");
    {
        RavelAudioProcessor processor;
        juce::Random random (0x59ead);

        const auto set = [&processor] (const juce::String& id, float actual)
        {
            if (auto* p = processor.apvts.getParameter (id))
                p->setValueNotifyingHost (p->convertTo0to1 (actual));
        };

        const auto get = [&processor] (const juce::String& id)
        {
            return processor.apvts.getRawParameterValue (id)->load();
        };

        const auto spreadOf = [&] (int lane, int step) { return get (params::stepSpreadId (lane, step)); };

        //---------------------------------------------------------------------- default
        // Zero on every step, which is what makes this additive: a session or preset written
        // before Spread existed loads with no window anywhere and plays as it always did.
        bool allZero = true;

        for (int step = 0; step < params::numSteps; ++step)
            allZero = allZero && spreadOf (0, step) < 1.0e-6f;

        check (allZero, "every step opens with no Spread at all");

        //---------------------------------------------------------------------- randomise
        params::randomiseLaneRow (processor.apvts, 0, random, params::LaneKind::note,
                                  params::StepLayer::spread);

        bool spreadMoved = false;
        bool valueUntouched = true;

        for (int step = 0; step < params::numSteps; ++step)
        {
            spreadMoved = spreadMoved || spreadOf (0, step) > 1.0e-6f;
            valueUntouched = valueUntouched && std::abs (get (params::stepValueId (0, step)) - 0.25f) < 1.0e-6f;
        }

        check (spreadMoved, "randomise reaches the Spread row when it is the selected one");
        check (valueUntouched, "and leaves the pitches it is a window on alone");

        //---------------------------------------------------------------------- clear
        // Back to zero rather than to a neutral trim: no wander is a real position for this
        // row, the way it is for Value and unlike Velocity, Prob and Gate.
        params::clearLaneRow (processor.apvts, 0, params::LaneKind::note, params::StepLayer::spread);

        bool cleared = true;

        for (int step = 0; step < params::numSteps; ++step)
            cleared = cleared && spreadOf (0, step) < 1.0e-6f;

        check (cleared, "clear puts the Spread row back to zero");

        //---------------------------------------------------------------------- CC lanes
        // A CC lane has no Spread row. Asking for one must do nothing rather than reach the
        // note lane of the same number that stepSpreadId would resolve to.
        set (params::stepSpreadId (0, 0), 0.4f);

        params::randomiseLaneRow (processor.apvts, 0, random, params::LaneKind::cc,
                                  params::StepLayer::spread);

        check (std::abs (spreadOf (0, 0) - 0.4f) < 1.0e-6f,
               "a CC lane has no Spread row to reach, and the note lane's is left alone");

        //---------------------------------------------------------------------- rotate
        // Spread travels with the value it is measured from: a window left behind by its own
        // floor is a range belonging to a note that has moved somewhere else.
        for (int step = 0; step < params::numSteps; ++step)
            set (params::stepSpreadId (0, step), 0.03f * (float) step);

        params::rotateLane (processor.apvts, 0, 1);

        check (std::abs (spreadOf (0, 1) - 0.0f) < 0.01f
                 && std::abs (spreadOf (0, 2) - 0.03f) < 0.01f
                 && std::abs (spreadOf (0, 0) - 0.03f * (float) (params::numSteps - 1)) < 0.01f,
               "rotate carries each step's Spread along with its value");

        //---------------------------------------------------------------------- copy/paste
        const auto pattern = params::copyLane (processor.apvts, 0);
        params::pasteLane (processor.apvts, 1, pattern);

        bool pasted = true;

        for (int step = 0; step < params::numSteps; ++step)
            pasted = pasted && std::abs (spreadOf (1, step) - spreadOf (0, step)) < 0.001f;

        check (pasted, "and copy/paste carries it onto another lane");

        //---------------------------------------------------------------------- lane removal
        set (params::noteLaneCountId, 2.0f);
        params::removeLane (processor.apvts, 0);

        check (std::abs (spreadOf (0, 2) - 0.03f) < 0.01f,
               "a lane moving down to close a gap brings its Spread with it");
    }

    //==========================================================================
    section ("A pattern action is one undo step, not sixteen");
    {
        RavelAudioProcessor processor;
        juce::Random random (0x5eed);

        params::randomiseLaneRow (processor.apvts, 0, random);

        check (processor.undoHistory.getUndoDepth() == 1,
               "randomising sixteen steps in one go records a single step");

        processor.undoHistory.undo();

        // Back to where the lane started, which is its default value rather than zero.
        const float stepDefault = defaultOf (processor, params::stepValueId (0, 0));

        bool allBackToDefault = true;

        for (int step = 0; step < params::numSteps; ++step)
            allBackToDefault = allBackToDefault
                   && std::abs (processor.apvts.getRawParameterValue (params::stepValueId (0, step))->load()
                                  - stepDefault) < 1.0e-6f;

        check (allBackToDefault, "and one undo takes the whole lane back");

        //----------------------------------------------------------------------
        // Paste writes five parameters per step across the lane, which is the widest single
        // action the editor has.
        processor.undoHistory.closeCurrentEdit();

        params::randomiseLaneRow (processor.apvts, 1, random);
        processor.undoHistory.closeCurrentEdit();

        const auto pattern = params::copyLane (processor.apvts, 1);

        // Measured either side of the paste rather than against a running total, so the check
        // says "the paste added one step" instead of restating the whole section's arithmetic.
        const int depthBeforePaste = processor.undoHistory.getUndoDepth();
        params::pasteLane (processor.apvts, 0, pattern);

        check (processor.undoHistory.getUndoDepth() == depthBeforePaste + 1,
               "a paste over a lane is also a single step");

        processor.undoHistory.undo();

        bool pasteUndone = true;

        for (int step = 0; step < params::numSteps; ++step)
            pasteUndone = pasteUndone
                       && std::abs (processor.apvts.getRawParameterValue (params::stepValueId (0, step))->load()
                                      - stepDefault) < 1.0e-6f;

        check (pasteUndone, "and undoing it leaves the target lane as it was");
    }

    //==========================================================================
    section ("Host automation does not enter the history");
    {
        RavelAudioProcessor processor;

        // No gestures: this is what a host moving an automation lane looks like, as opposed
        // to a user dragging the control in the editor.
        if (auto* p = processor.apvts.getParameter (params::stepValueId (0, 0)))
            for (int i = 1; i <= 20; ++i)
                p->setValueNotifyingHost ((float) i / 20.0f);

        check (! processor.undoHistory.canUndo(),
               "twenty automated writes leave the history empty");
    }

    //==========================================================================
    section ("Loading a session clears the history");
    {
        RavelAudioProcessor a;

        if (auto* p = a.apvts.getParameter (params::stepValueId (0, 0)))
        {
            p->beginChangeGesture();
            p->setValueNotifyingHost (p->convertTo0to1 (0.4f));
            p->endChangeGesture();
        }

        check (a.undoHistory.canUndo(), "the edit is in the history before the load");

        juce::MemoryBlock state;
        a.getStateInformation (state);
        a.setStateInformation (state.getData(), (int) state.getSize());

        check (! a.undoHistory.canUndo(),
               "loading a session leaves nothing to step back into");
    }

    //==========================================================================
    section ("Removing a lane");
    {
        RavelAudioProcessor processor;

        const auto set = [&processor] (const juce::String& id, float actual)
        {
            if (auto* p = processor.apvts.getParameter (id))
                p->setValueNotifyingHost (p->convertTo0to1 (actual));
        };

        const auto get = [&processor] (const juce::String& id)
        {
            return processor.apvts.getRawParameterValue (id)->load();
        };

        set (params::noteLaneCountId, 3.0f);

        // Each lane is stamped with something recognisable, in a step value and in a lane
        // control, so a shift that moved only the pattern would still be caught.
        for (int lane = 0; lane < 3; ++lane)
        {
            set (params::stepValueId (lane, 0), 0.1f * (float) (lane + 1));
            set (params::laneLengthId (lane), (float) (lane + 4));
            set (params::laneDepthId (lane), 0.1f * (float) (lane + 1));
        }

        params::removeLane (processor.apvts, 1);

        check ((int) std::lround (get (params::noteLaneCountId)) == 2,
               "removing a lane drops the lane count by one");

        check (std::abs (get (params::stepValueId (0, 0)) - 0.1f) < 0.01f,
               "the lanes below the removed one stay where they are");

        check (std::abs (get (params::stepValueId (1, 0)) - 0.3f) < 0.01f,
               "and the lane above it moves down into its place");

        check ((int) std::lround (get (params::laneLengthId (1))) == 6,
               "a lane moving down brings its own controls with it, not just its pattern");

        check (std::abs (get (params::laneDepthId (1)) - 0.3f) < 0.01f,
               "including the ones that are not part of a copyable pattern");

        //----------------------------------------------------------------------
        // The slot the stack shrank out of. Its default rate is lane 3's, not lane 1's,
        // which is what makes reading each parameter's own default the only correct way.
        const auto* division = processor.apvts.getParameter (params::laneDivId (2));

        check (division != nullptr
                 && std::abs (division->getValue() - division->getDefaultValue()) < 1.0e-6f,
               "the slot left free at the top goes back to that lane's own defaults");

        check (std::abs (get (params::stepValueId (2, 0))
                           - defaultOf (processor, params::stepValueId (2, 0))) < 0.01f,
               "so adding a lane again gives a new lane, not a copy of the one that moved");
    }

    //==========================================================================
    section ("Removing a lane: limits and undo");
    {
        RavelAudioProcessor processor;

        const auto get = [&processor] (const juce::String& id)
        {
            return processor.apvts.getRawParameterValue (id)->load();
        };

        params::removeLane (processor.apvts, 0);

        check ((int) std::lround (get (params::noteLaneCountId)) == 1,
               "the last remaining lane cannot be removed");

        //----------------------------------------------------------------------
        if (auto* p = processor.apvts.getParameter (params::noteLaneCountId))
            p->setValueNotifyingHost (p->convertTo0to1 (3.0f));

        if (auto* p = processor.apvts.getParameter (params::stepValueId (1, 0)))
            p->setValueNotifyingHost (0.5f);

        processor.undoHistory.closeCurrentEdit();

        params::removeLane (processor.apvts, 3);

        check ((int) std::lround (get (params::noteLaneCountId)) == 3,
               "a lane this instance does not have cannot be removed either");

        //----------------------------------------------------------------------
        const int depthBefore = processor.undoHistory.getUndoDepth();

        params::removeLane (processor.apvts, 1);

        check (processor.undoHistory.getUndoDepth() == depthBefore + 1,
               "a removal is one undo step, not one per parameter it shifted");

        processor.undoHistory.undo();

        check ((int) std::lround (get (params::noteLaneCountId)) == 3,
               "undoing a removal brings the lane count back");

        check (std::abs (get (params::stepValueId (1, 0)) - 0.5f) < 0.01f,
               "and puts the removed lane's pattern back with it");
    }

    //==========================================================================
    // Proves the wiring end to end -- parameter, through buildSnapshot(), into the engine --
    // rather than only the engine's own handling of a Snapshot's direction field, which
    // EngineTests already covers generically for both lane pools.
    section ("A CC lane's own Direction reverses which step its tap latches first");
    {
        RavelAudioProcessor processor;

        const auto set = [&processor] (const juce::String& id, float actual)
        {
            if (auto* p = processor.apvts.getParameter (id))
                p->setValueNotifyingHost (p->convertTo0to1 (actual));
        };

        set (params::laneLengthId (0, params::LaneKind::cc), 4.0f);
        set (params::laneCcNumId (0), 50.0f);
        set (params::laneDirId (0, params::LaneKind::cc), 1.0f);   // Reverse

        for (int step = 0; step < 4; ++step)
            set (params::stepValueId (0, step, params::LaneKind::cc), 0.3f * (float) step);

        processor.setPlayConfigDetails (0, 2, 48000.0, 512);
        processor.prepareToPlay (48000.0, 512);

        MockPlayHead playHead;
        processor.setPlayHead (&playHead);
        playHead.info.setBpm (120.0);
        playHead.info.setIsPlaying (true);

        std::vector<int> laneCcValues;

        juce::AudioBuffer<float> audio (2, 512);
        juce::MidiBuffer midi;

        // One beat at 120bpm/48kHz/1/16 steps = 4 steps of 6000 samples, enough to see the
        // first two steps' worth of the lane's own tap.
        for (int pos = 0; pos < 12000; pos += 512)
        {
            const int numSamples = juce::jmin (512, 12000 - pos);

            playHead.info.setPpqPosition ((double) pos / 24000.0);
            playHead.info.setTimeInSamples ((juce::int64) pos);

            audio.clear();
            midi.clear();

            juce::AudioBuffer<float> block (audio.getArrayOfWritePointers(), 2, numSamples);
            processor.processBlock (block, midi);

            for (const auto metadata : midi)
            {
                const auto message = metadata.getMessage();

                if (message.isController() && message.getControllerNumber() == 50)
                    laneCcValues.push_back (message.getControllerValue());
            }
        }

        check (laneCcValues.size() >= 2, "the lane's own tap sends at least two CC events");

        // Reverse walks a length-4 lane 3, 2, 1, 0 -- see EngineTests' "Reverse direction" --
        // so the tap latches step 3's value (0.9, CC 114) first and step 2's (0.6, CC 76)
        // second, rather than Forward's step 0 (CC 0) then step 1 (CC 38).
        check (laneCcValues.size() >= 2
                 && laneCcValues[0] == 114 && laneCcValues[1] == 76,
               "and the CC lane's own Direction parameter reaches the engine, walking backwards");
    }

    //==========================================================================
    // Pins what a *fresh* instance comes up at, as the user actually sees it -- the
    // displayed text, not just the raw number. Nothing in the plugin stamps a preset over
    // the layout, so these are the layout defaults; a host or the standalone wrapper
    // restoring a saved state is a separate path and will show whatever it saved.
    section ("A fresh instance comes up at the documented defaults");
    {
        RavelAudioProcessor processor;

        const auto textOf = [&processor] (const juce::String& paramID)
        {
            auto* param = processor.apvts.getParameter (paramID);
            return param != nullptr ? param->getCurrentValueAsText() : juce::String ("<missing>");
        };

        const auto valueOf = [&processor] (const juce::String& paramID)
        {
            auto* raw = processor.apvts.getRawParameterValue (paramID);
            return raw != nullptr ? raw->load() : -1.0f;
        };

        // Printed unconditionally: if one of these ever drifts, the value that replaced it is
        // more useful than the bare failure.
        std::printf ("          Root %s, Scale %s, Quantize %s\n",
                     textOf (params::rootNoteId).toRawUTF8(),
                     textOf (params::scaleId).toRawUTF8(),
                     textOf (params::quantizeId).toRawUTF8());

        check (std::abs (valueOf (params::rootNoteId) - 24.0f) < 0.5f,
               "Root is MIDI 24");
        check (textOf (params::rootNoteId) == "C0",
               "and reads as C0 -- not C0#, and not the old C2");

        check (textOf (params::scaleId) == "Chromatic",
               "Scale is Chromatic");

        check (valueOf (params::quantizeId) < 0.5f,
               "Quantize is off");

        check (std::abs (valueOf (params::noteOffsetId)) < 0.5f,
               "Offset is 0 octaves");

        // Removed outright, not merely defaulted -- a host or an old session cannot bring
        // it back by writing it into the state.
        check (processor.apvts.getParameter ("velocity") == nullptr,
               "the master velocity parameter no longer exists");

        // Continuous microtonal pitch is what a stock instance plays, and that only holds
        // up under polyphony with a channel per note -- so MPE defaults on, which leaves
        // the Note Channel it overrides inert at its own default.
        check (valueOf (params::mpeEnabledId) > 0.5f,
               "MPE is on");
        check (std::abs (valueOf (params::midiChannelId) - 1.0f) < 0.5f,
               "and Note Channel, which it overrides while on, is 1");
    }

    //==========================================================================
    // Confirms the mpe_on parameter actually reaches the engine's channel allocation, not
    // just the Snapshot field -- EngineTests exercises the allocator directly, this exercises
    // the wiring APVTS -> buildSnapshot() -> SequencerEngine that carries the flag to it, in
    // both directions, plus that a stock instance speaks MPE with nothing set.
    section ("MPE routes each note to its own channel; Note Channel takes over with it off");
    {
        const auto twoOverlappingNoteOnChannels = [] (RavelAudioProcessor& processor)
        {
            processor.setPlayConfigDetails (0, 2, 48000.0, 512);
            processor.prepareToPlay (48000.0, 512);

            setChoice (processor, params::voiceCountId, 2);
            setChoice (processor, params::stepGateId (0, 0), 200);   // overlaps into step 2

            MockPlayHead playHead;
            processor.setPlayHead (&playHead);
            playHead.info.setBpm (120.0);
            playHead.info.setIsPlaying (true);

            juce::AudioBuffer<float> audio (2, 512);
            juce::MidiBuffer midi;
            std::vector<int> onChannels;

            for (int pos = 0; pos < 2 * 6000; pos += 512)
            {
                const int numSamples = juce::jmin (512, 2 * 6000 - pos);

                playHead.info.setPpqPosition ((double) pos / 24000.0);
                playHead.info.setTimeInSamples ((juce::int64) pos);

                audio.clear();
                midi.clear();

                juce::AudioBuffer<float> block (audio.getArrayOfWritePointers(), 2, numSamples);
                processor.processBlock (block, midi);

                for (const auto metadata : midi)
                    if (metadata.getMessage().isNoteOn())
                        onChannels.push_back (metadata.getMessage().getChannel());
            }

            return onChannels;
        };

        RavelAudioProcessor mpeOn;   // untouched: mpe_on defaults to on
        const auto onChannels = twoOverlappingNoteOnChannels (mpeOn);

        check (onChannels.size() >= 2, "two overlapping notes fire");
        check (onChannels.size() >= 2 && onChannels[0] != onChannels[1],
               "and land on different channels with nothing configured -- MPE defaults on");

        // A member channel, never the zone master. Getting this wrong would look like MPE
        // in a channel count while actually stacking notes on the master.
        bool allMembers = ! onChannels.empty();

        for (int channel : onChannels)
            allMembers = allMembers && channel >= SequencerEngine::mpeMemberChannelBase
                                    && channel < SequencerEngine::mpeMemberChannelBase
                                                   + SequencerEngine::mpeMemberChannels;

        check (allMembers, "every note-on sits on a member channel, not the master");

        // The other direction: no zone, every note on the one channel Note Channel names.
        // Channel 7 rather than the default 1, so a pass means the parameter was actually
        // read and not that the notes happened to land on the master anyway.
        RavelAudioProcessor mpeOff;
        setChoice (mpeOff, params::mpeEnabledId, 0);
        setChoice (mpeOff, params::midiChannelId, 7);

        const auto offChannels = twoOverlappingNoteOnChannels (mpeOff);

        bool allOnNoteChannel = ! offChannels.empty();

        for (int channel : offChannels)
            allOnNoteChannel = allOnNoteChannel && channel == 7;

        check (allOnNoteChannel, "with MPE off every note-on goes out on the Note Channel");
    }

    //==========================================================================
    // Presets. Redirected away from Documents/Ravel/Presets first, so running the tests can
    // neither read nor delete anything the user has actually saved.
    const auto presetRoot = juce::File::getSpecialLocation (juce::File::tempDirectory)
                              .getChildFile ("RavelPresetTests");

    presetRoot.deleteRecursively();
    PresetManager::setPresetDirectory (presetRoot);

    section ("A preset round-trips every parameter");
    {
        RavelAudioProcessor processor;

        const auto setPlain = [&processor] (const juce::String& id, float plain)
        {
            if (auto* p = processor.apvts.getParameter (id))
            {
                p->beginChangeGesture();
                p->setValueNotifyingHost (p->convertTo0to1 (plain));
                p->endChangeGesture();
            }
        };

        const auto plain = [&processor] (const juce::String& id)
        {
            return processor.apvts.getRawParameterValue (id)->load();
        };

        // Read off the parameter rather than written in by hand, so changing a default in
        // Parameters.cpp cannot leave this asserting against the old one. A note step's is
        // 0.25, not zero.
        const auto defaultPlain = [&processor] (const juce::String& id)
        {
            auto* p = processor.apvts.getParameter (id);
            return p != nullptr ? p->convertFrom0to1 (p->getDefaultValue()) : 0.0f;
        };

        // One of each kind the layout produces: a float, a stepped choice, a bool, an int.
        setPlain (params::stepValueId (0, 3), 0.42f);
        setPlain (params::laneDivId (1), (float) params::divIndex_1_8);
        setPlain (params::quantizeId, 1.0f);
        setPlain (params::rangeOctavesId, 3.0f);
        setPlain (params::stepValueId (0, 3, params::LaneKind::cc), 0.77f);

        check (processor.presetManager.saveAs ("Round trip"), "saveAs writes a preset");
        check (processor.presetManager.getDisplayName() == "Round trip",
               "and the saved name becomes the current one");

        // Everything back to defaults, so a successful reload cannot be the values simply
        // never having moved.
        processor.presetManager.loadInit();

        check (std::abs (plain (params::stepValueId (0, 3))
                           - defaultPlain (params::stepValueId (0, 3))) < 1.0e-6f,
               "Init puts the parameters back to their defaults");
        check (processor.presetManager.getDisplayName() == "Init",
               "and detaches the chip from any preset");

        const auto file = presetRoot.getChildFile ("Round trip.ravelpreset");

        check (processor.presetManager.load (file), "the preset loads back");
        check (std::abs (plain (params::stepValueId (0, 3)) - 0.42f) < 1.0e-4f, "a float step value survives");
        check ((int) plain (params::laneDivId (1)) == params::divIndex_1_8, "a choice survives");
        check (plain (params::quantizeId) > 0.5f, "a toggle survives");
        check ((int) plain (params::rangeOctavesId) == 3, "an integer survives");
        check (std::abs (plain (params::stepValueId (0, 3, params::LaneKind::cc)) - 0.77f) < 1.0e-4f,
               "and so does a CC lane's own step");
    }

    //==========================================================================
    section ("A preset is the patch, not the session");
    {
        RavelAudioProcessor processor;

        // The three things that ride in the state tree without being parameters.
        processor.apvts.state.setProperty ("externalMidiDevice", "some-loopMIDI-port", nullptr);
        processor.apvts.state.setProperty ("editorWidth", 1400, nullptr);
        processor.apvts.state.setProperty ("editorHeight", 500, nullptr);

        check (processor.presetManager.saveAs ("Environment"), "the preset saves");

        const auto xml = juce::XmlDocument::parse (presetRoot.getChildFile ("Environment.ravelpreset"));

        check (xml != nullptr, "and parses back as XML");
        check (xml != nullptr && xml->hasTagName ("RAVELPRESET"),
               "under its own tag, not the session's");
        check (xml != nullptr && ! xml->hasAttribute ("externalMidiDevice"),
               "the external MIDI device is not in it");
        check (xml != nullptr && ! xml->hasAttribute ("editorWidth")
                              && ! xml->hasAttribute ("editorHeight"),
               "nor is the window size");
        check (xml != nullptr && ! xml->hasAttribute ("currentPreset"),
               "nor which preset the session was sitting on");
        check (xml != nullptr && xml->getIntAttribute ("schemaVersion") == 1,
               "and it carries a schema version");

        // A session file is not a preset, and must not load as one.
        const auto sessionFile = presetRoot.getChildFile ("NotAPreset.ravelpreset");

        if (const auto sessionXml = processor.apvts.copyState().createXml())
            sessionXml->writeTo (sessionFile);

        check (! processor.presetManager.load (sessionFile),
               "a file that is not a preset is refused rather than half-applied");
    }

    //==========================================================================
    section ("Presets outlive the build that wrote them");
    {
        RavelAudioProcessor processor;

        const auto plain = [&processor] (const juce::String& id)
        {
            return processor.apvts.getRawParameterValue (id)->load();
        };

        // Hand-written rather than saved: this is the shape of a preset from a build whose
        // parameter list was not this one -- it is missing most of them, and carries one this
        // build has never heard of.
        const auto file = presetRoot.getChildFile ("Partial.ravelpreset");

        file.replaceWithText (R"(<?xml version="1.0" encoding="UTF-8"?>
<RAVELPRESET schemaVersion="1">
  <PARAM id=")" + juce::String (params::stepValueId (0, 0)) + R"(" value="0.6"/>
  <PARAM id="a_parameter_this_build_does_not_have" value="123"/>
</RAVELPRESET>)");

        // Move something the file says nothing about, so "went to its default" is
        // distinguishable from "was never touched".
        if (auto* p = processor.apvts.getParameter (params::stepValueId (0, 1)))
        {
            p->beginChangeGesture();
            p->setValueNotifyingHost (p->convertTo0to1 (0.9f));
            p->endChangeGesture();
        }

        auto* unlisted = processor.apvts.getParameter (params::stepValueId (0, 1));
        const float unlistedDefault = unlisted->convertFrom0to1 (unlisted->getDefaultValue());

        check (processor.presetManager.load (file), "a preset with unknown parameters still loads");
        check (std::abs (plain (params::stepValueId (0, 0)) - 0.6f) < 1.0e-4f,
               "the parameters it does name are applied");
        check (std::abs (plain (params::stepValueId (0, 1)) - unlistedDefault) < 1.0e-6f,
               "and the ones it does not go to their defaults rather than being left as they were");
    }

    //==========================================================================
    section ("Loading a preset is one undo step");
    {
        RavelAudioProcessor processor;

        const auto plain = [&processor] ()
        {
            return processor.apvts.getRawParameterValue (params::stepValueId (0, 0))->load();
        };

        juce::Random random (0x9e11);
        params::randomiseLaneRow (processor.apvts, 0, random);
        processor.undoHistory.closeCurrentEdit();

        check (processor.presetManager.saveAs ("Undo"), "a preset saves");

        const auto saved = plain();

        params::clearLaneRow (processor.apvts, 0);
        processor.undoHistory.closeCurrentEdit();

        const int depthBeforeLoad = processor.undoHistory.getUndoDepth();

        processor.presetManager.load (presetRoot.getChildFile ("Undo.ravelpreset"));

        // The whole reason a load writes parameters instead of calling replaceState: sixty
        // or more values move, and Ctrl+Z has to walk back over all of them at once.
        check (processor.undoHistory.getUndoDepth() == depthBeforeLoad + 1,
               "loading a preset records exactly one step, not one per parameter");
        check (std::abs (plain() - saved) < 1.0e-4f, "and the patch it names is what is now loaded");

        processor.undoHistory.closeCurrentEdit();

        check (processor.undoHistory.undo(), "undo moves");
        check (std::abs (plain()) < 1.0e-6f, "and one press puts the whole patch back");
    }

    //==========================================================================
    section ("The dirty marker tracks the patch against its preset");
    {
        RavelAudioProcessor processor;

        const auto nudge = [&processor] (float plain)
        {
            if (auto* p = processor.apvts.getParameter (params::stepValueId (0, 0)))
            {
                p->beginChangeGesture();
                p->setValueNotifyingHost (p->convertTo0to1 (plain));
                p->endChangeGesture();
            }
        };

        nudge (0.3f);

        check (processor.presetManager.saveAs ("Dirty"), "a preset saves");
        check (! processor.presetManager.isDirty(), "saving leaves the patch clean");

        nudge (0.8f);
        check (processor.presetManager.isDirty(), "an edit marks it dirty");

        processor.presetManager.load (presetRoot.getChildFile ("Dirty.ravelpreset"));
        check (! processor.presetManager.isDirty(), "reloading clears it again");

        // The load moved a parameter itself, which must not be mistaken for the user editing
        // away from the preset that was being loaded.
        check (processor.presetManager.hasCurrent(), "and the preset is still the current one");

        check (processor.presetManager.saveToCurrent(), "Save overwrites the loaded preset");
        check (processor.presetManager.hasCurrent(), "which stays the current one");
    }

    //==========================================================================
    section ("Which preset a session was on survives being reopened");
    {
        juce::MemoryBlock state;
        juce::String savedName;

        {
            RavelAudioProcessor processor;
            processor.presetManager.saveAs ("Session");
            savedName = processor.presetManager.getDisplayName();
            processor.getStateInformation (state);
        }

        RavelAudioProcessor reopened;
        check (reopened.presetManager.getDisplayName() == "Init",
               "a fresh instance starts unattached");

        reopened.setStateInformation (state.getData(), (int) state.getSize());

        check (reopened.presetManager.getDisplayName() == savedName,
               "and a restored session comes back showing the preset it was on");
        check (! reopened.presetManager.isDirty(), "clean, because it was clean when saved");
    }

    //==========================================================================
    section ("Stepping through the browser");
    {
        RavelAudioProcessor processor;

        // "Session" and the rest from the sections above are in the folder too; these two
        // bracket them alphabetically, which is the order the browser lists them in.
        processor.presetManager.saveAs ("Aaa first");
        processor.presetManager.loadInit();
        processor.presetManager.saveAs ("Zzz last");

        processor.presetManager.refresh();

        check (processor.presetManager.loadRelative (-1), "stepping back from the last one moves");
        check (processor.presetManager.getDisplayName() != "Zzz last", "onto a different preset");

        processor.presetManager.load (presetRoot.getChildFile ("Aaa first.ravelpreset"));
        check (! processor.presetManager.loadRelative (-1),
               "and stepping back off the first one stops rather than wrapping");

        processor.presetManager.load (presetRoot.getChildFile ("Zzz last.ravelpreset"));
        check (! processor.presetManager.loadRelative (1),
               "as does stepping forward off the last");
    }

    //==========================================================================
    section ("Renaming and deleting");
    {
        RavelAudioProcessor processor;

        processor.presetManager.saveAs ("Before rename");

        check (processor.presetManager.renameCurrent ("After rename"), "rename moves the file");
        check (presetRoot.getChildFile ("After rename.ravelpreset").existsAsFile(),
               "the new name is on disk");
        check (! presetRoot.getChildFile ("Before rename.ravelpreset").existsAsFile(),
               "and the old one is gone");
        check (processor.presetManager.getDisplayName() == "After rename", "the chip follows it");

        check (processor.presetManager.deleteCurrent(), "delete removes it");
        check (! presetRoot.getChildFile ("After rename.ravelpreset").existsAsFile(),
               "the file is gone");
        check (! processor.presetManager.hasCurrent(), "and nothing is loaded any more");
        check (processor.presetManager.isDirty(),
               "the patch is marked dirty -- there is no longer a file matching what is on screen");

        check (! processor.presetManager.saveToCurrent(),
               "Save has nothing to overwrite, which is what makes the editor ask for a name");
    }

    presetRoot.deleteRecursively();
    PresetManager::setPresetDirectory ({});

    //==========================================================================
    //==========================================================================
    section ("Closing the external MIDI output does not wait out its poll interval");
    {
        // The drain thread sleeps on its own WaitableEvent with a 50 ms timeout as a safety
        // net. juce::Thread::stopThread() signals the Thread's own event, not that one, so
        // until the destructor started signalling it by hand every instance sat out the
        // remainder of that timeout on the way down -- invisible on one, a stall on a set of
        // them closing together.
        //
        // The pause before each teardown is the whole point: destroy the object immediately
        // and the thread has not reached the wait yet, so it exits promptly either way and the
        // check passes whether the bug is present or not. Several rounds because the time left
        // on a 50 ms wait is uniform in [0, 50) -- one round could get lucky, three cannot.
        constexpr int rounds = 3;

        std::int64_t teardownMs = 0;

        for (int i = 0; i < rounds; ++i)
        {
            // Held by pointer so the destructor runs where it can be timed, rather than at the
            // end of a scope after the clock has been read.
            auto output = std::make_unique<ExternalMidiOutput>();

            // Long enough that the thread is certainly parked in wakeUp.wait().
            juce::Thread::sleep (60);

            const auto start = std::chrono::steady_clock::now();
            output.reset();
            teardownMs += std::chrono::duration_cast<std::chrono::milliseconds> (
                              std::chrono::steady_clock::now() - start).count();
        }

        check (teardownMs < 20,
               "it is signalled awake rather than left to time out");
    }

    std::printf ("\n%d checks, %d failed\n", checksRun, checksFailed);

    return checksFailed == 0 ? 0 : 1;
}
