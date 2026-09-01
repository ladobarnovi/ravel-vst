/*
    Drives the real RavelAudioProcessor::processBlock through a mock playhead.

    EngineTests covers the sequencer core; this covers the layer above it -- playhead
    handling, the free-run fallback, the parameter snapshot, and the plugin's declared
    MIDI capabilities. That is the layer where the plugin could compile, load and still
    emit nothing, so it is worth asserting on directly rather than only in a host.
*/

#include "PluginProcessor.h"

#include <cstdio>

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
                        counts.firstNote = message.getNoteNumber();
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

        for (int lane = 0; lane < params::numLanes; ++lane)
        {
            fullLength = fullLength
                          && (int) std::lround (value (params::laneLengthId (lane))) == params::numSteps;

            for (int step = 0; step < params::numSteps; ++step)
                flat = flat && std::abs (value (params::stepValueId (lane, step))) < 1.0e-6f;
        }

        check (flat, "every lane is sixteen steps of zero");
        check (fullLength, "and sixteen steps long");

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
        check (counts.firstNote == 48, "first note is the default root (C3)");
        check (counts.controllers == 0, "default output mode emits no CC");
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
    section ("Output: Notes off, CC on");
    {
        RavelAudioProcessor processor;
        processor.setPlayConfigDetails (0, 2, 48000.0, 512);
        processor.prepareToPlay (48000.0, 512);

        setChoice (processor, params::notesOnId, 0);
        setChoice (processor, params::ccOnId, 1);

        MockPlayHead playHead;
        processor.setPlayHead (&playHead);

        const auto counts = runProcessor (processor, playHead, true, 48000);

        check (counts.noteOns == 0, "Notes off emits NO notes even with CC on");
        check (counts.controllers > 0, "CC mode emits controller events");
        check (counts.firstCcNumber == 1, "uses the default CC number");
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

        params::randomiseLaneValues (processor.apvts, 0, random);
        params::clearLaneValues (processor.apvts, 0);

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
        params::randomiseLaneValues (processor.apvts, 0, random);

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

        params::invertLaneValues (processor.apvts, 0);

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
        check (std::abs (value (0, 0)) < 0.01f, "the second undo reaches the value it started at");

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

        check (std::abs (value()) < 0.01f, "undo took the value back");
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
    section ("A pattern action is one undo step, not sixteen");
    {
        RavelAudioProcessor processor;
        juce::Random random (0x5eed);

        params::randomiseLaneValues (processor.apvts, 0, random);

        check (processor.undoHistory.getUndoDepth() == 1,
               "randomising sixteen steps in one go records a single step");

        processor.undoHistory.undo();

        bool allZero = true;

        for (int step = 0; step < params::numSteps; ++step)
            allZero = allZero
                   && std::abs (processor.apvts.getRawParameterValue (params::stepValueId (0, step))->load()) < 1.0e-6f;

        check (allZero, "and one undo takes the whole lane back");

        //----------------------------------------------------------------------
        // Paste writes five parameters per step across the lane, which is the widest single
        // action the editor has.
        processor.undoHistory.closeCurrentEdit();

        params::randomiseLaneValues (processor.apvts, 1, random);
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
                       && std::abs (processor.apvts.getRawParameterValue (params::stepValueId (0, step))->load()) < 1.0e-6f;

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
            set (params::laneNudgeId (lane), 0.1f * (float) (lane + 1));
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

        check (std::abs (get (params::laneNudgeId (1)) - 0.3f) < 0.01f,
               "including the ones that are not part of a copyable pattern");

        //----------------------------------------------------------------------
        // The slot the stack shrank out of. Its default rate is lane 3's, not lane 1's,
        // which is what makes reading each parameter's own default the only correct way.
        const auto* division = processor.apvts.getParameter (params::laneDivId (2));

        check (division != nullptr
                 && std::abs (division->getValue() - division->getDefaultValue()) < 1.0e-6f,
               "the slot left free at the top goes back to that lane's own defaults");

        check (std::abs (get (params::stepValueId (2, 0))) < 0.01f,
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
    section ("State round-trip");
    {
        RavelAudioProcessor a;
        setChoice (a, params::notesOnId, 0);
        setChoice (a, params::ccOnId, 1);

        juce::MemoryBlock state;
        a.getStateInformation (state);

        RavelAudioProcessor b;
        b.setStateInformation (state.getData(), (int) state.getSize());

        const auto* notesOnParam = b.apvts.getRawParameterValue (params::notesOnId);
        const auto* ccOnParam    = b.apvts.getRawParameterValue (params::ccOnId);

        check (notesOnParam != nullptr && ccOnParam != nullptr
                && notesOnParam->load() < 0.5f && ccOnParam->load() > 0.5f,
               "Notes/CC survive save/load independently");
    }

    //==========================================================================
    section ("Legacy Output choice migrates to independent Notes/CC");
    {
        // Simulates a session saved before Notes and CC were split: a single "out_mode"
        // parameter (0 Notes, 1 CC) instead of the two switches. Built by taking a real
        // session and swapping the new params back out for the old one, rather than hand
        // writing the APVTS XML shape from scratch.
        RavelAudioProcessor donor;
        juce::MemoryBlock donorState;
        donor.getStateInformation (donorState);

        auto xml = juce::AudioProcessor::getXmlFromBinary (donorState.getData(), (int) donorState.getSize());
        auto tree = juce::ValueTree::fromXml (*xml);

        for (int i = tree.getNumChildren() - 1; i >= 0; --i)
        {
            const auto id = tree.getChild (i).getProperty ("id").toString();

            if (id == params::notesOnId || id == params::ccOnId)
                tree.removeChild (i, nullptr);
        }

        juce::ValueTree legacyOutputMode ("PARAM");
        legacyOutputMode.setProperty ("id", "out_mode", nullptr);
        legacyOutputMode.setProperty ("value", 1.0f, nullptr);   // the old CC value
        tree.appendChild (legacyOutputMode, nullptr);

        juce::MemoryBlock legacyState;
        juce::AudioProcessor::copyXmlToBinary (*tree.createXml(), legacyState);

        RavelAudioProcessor loaded;
        loaded.setStateInformation (legacyState.getData(), (int) legacyState.getSize());

        const auto* notesOnParam = loaded.apvts.getRawParameterValue (params::notesOnId);
        const auto* ccOnParam    = loaded.apvts.getRawParameterValue (params::ccOnId);

        check (notesOnParam != nullptr && ccOnParam != nullptr
                && notesOnParam->load() < 0.5f && ccOnParam->load() > 0.5f,
               "an old CC-mode session loads with Notes off and CC on");
    }

    //==========================================================================
    std::printf ("\n%d checks, %d failed\n", checksRun, checksFailed);

    return checksFailed == 0 ? 0 : 1;
}
