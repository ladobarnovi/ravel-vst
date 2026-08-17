/*
    Drives the real TriLaneAudioProcessor::processBlock through a mock playhead.

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
    Counts runProcessor (TriLaneAudioProcessor& processor,
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

    void setChoice (TriLaneAudioProcessor& processor, const juce::String& paramID, int index)
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

    std::printf ("TriLane processor tests\n");

    //==========================================================================
    section ("Declared capabilities (what the host reads to decide routing)");
    {
        TriLaneAudioProcessor processor;

        check (processor.producesMidi(), "producesMidi() is true -- required for Live to offer it as a MIDI source");
        check (processor.acceptsMidi(),  "acceptsMidi() is true");
        check (! processor.isMidiEffect(), "isMidiEffect() is false -- it loads as an instrument");
        check (processor.getTotalNumOutputChannels() == 2, "declares a stereo output bus");
    }

    //==========================================================================
    section ("Host transport running");
    {
        TriLaneAudioProcessor processor;
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
        TriLaneAudioProcessor processor;
        processor.setPlayConfigDetails (0, 2, 48000.0, 512);
        processor.prepareToPlay (48000.0, 512);

        MockPlayHead playHead;
        processor.setPlayHead (&playHead);

        const auto counts = runProcessor (processor, playHead, false, 48000);

        check (counts.noteOns > 0, "notes are still produced while the transport is stopped");
    }

    //==========================================================================
    section ("Free Run disabled + transport stopped = silence");
    {
        TriLaneAudioProcessor processor;
        processor.setPlayConfigDetails (0, 2, 48000.0, 512);
        processor.prepareToPlay (48000.0, 512);

        if (auto* freeRun = processor.apvts.getParameter (params::freeRunId))
            freeRun->setValueNotifyingHost (0.0f);

        MockPlayHead playHead;
        processor.setPlayHead (&playHead);

        const auto counts = runProcessor (processor, playHead, false, 48000);

        check (counts.noteOns == 0, "nothing fires with Free Run off and the transport stopped");
    }

    //==========================================================================
    section ("No playhead at all (host provides none)");
    {
        TriLaneAudioProcessor processor;
        processor.setPlayConfigDetails (0, 2, 48000.0, 512);
        processor.prepareToPlay (48000.0, 512);

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
    section ("Output mode: CC only");
    {
        TriLaneAudioProcessor processor;
        processor.setPlayConfigDetails (0, 2, 48000.0, 512);
        processor.prepareToPlay (48000.0, 512);

        setChoice (processor, params::outputModeId, params::outCC);

        MockPlayHead playHead;
        processor.setPlayHead (&playHead);

        const auto counts = runProcessor (processor, playHead, true, 48000);

        check (counts.noteOns == 0, "CC mode emits NO notes -- this is the mode that silences note output");
        check (counts.controllers > 0, "CC mode emits controller events");
        check (counts.firstCcNumber == 1, "uses the default CC number");
    }

    //==========================================================================
    section ("Output mode: Notes + CC");
    {
        TriLaneAudioProcessor processor;
        processor.setPlayConfigDetails (0, 2, 48000.0, 512);
        processor.prepareToPlay (48000.0, 512);

        setChoice (processor, params::outputModeId, params::outBoth);

        MockPlayHead playHead;
        processor.setPlayHead (&playHead);

        const auto counts = runProcessor (processor, playHead, true, 48000);

        check (counts.noteOns == 8, "Notes + CC still emits the notes");
        check (counts.controllers > 0, "Notes + CC also emits controller events");
    }

    //==========================================================================
    section ("State round-trip");
    {
        TriLaneAudioProcessor a;
        setChoice (a, params::outputModeId, params::outBoth);

        juce::MemoryBlock state;
        a.getStateInformation (state);

        TriLaneAudioProcessor b;
        b.setStateInformation (state.getData(), (int) state.getSize());

        const auto* param = b.apvts.getRawParameterValue (params::outputModeId);

        check (param != nullptr && (int) std::lround (param->load()) == params::outBoth,
               "output mode survives save/load");
    }

    //==========================================================================
    std::printf ("\n%d checks, %d failed\n", checksRun, checksFailed);

    return checksFailed == 0 ? 0 : 1;
}
