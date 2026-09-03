#pragma once

#include "ExternalMidiOutput.h"
#include "Parameters.h"
#include "SequencerEngine.h"
#include "UndoHistory.h"

class RavelAudioProcessor final : public juce::AudioProcessor
{
public:
    RavelAudioProcessor();
    ~RavelAudioProcessor() override = default;

    //==========================================================================
    void prepareToPlay (double sampleRate, int maximumExpectedSamplesPerBlock) override;
    void releaseResources() override;
    void reset() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==========================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override                      { return true; }

    const juce::String getName() const override          { return JucePlugin_Name; }

    bool acceptsMidi() const override                    { return true; }
    bool producesMidi() const override                   { return true; }
    bool isMidiEffect() const override                    { return false; }
    double getTailLengthSeconds() const override         { return 0.0; }

    int getNumPrograms() override                         { return 1; }
    int getCurrentProgram() override                      { return 0; }
    void setCurrentProgram (int) override                 {}
    const juce::String getProgramName (int) override      { return "Default"; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    //==========================================================================
    juce::AudioProcessorValueTreeState apvts;

    /** Declared after apvts, because it takes its parameter list from the processor and the
        parameters are not there until apvts has finished constructing.

        It lives on the processor rather than the editor so that closing the plugin window
        does not throw the history away. */
    UndoHistory undoHistory { *this };

    /** Mirrors every event this instance generates out to a system MIDI port, completely
        outside Ableton's own MIDI routing -- see ExternalMidiOutput's own header for why.
        Lives on the processor rather than the editor so the connection survives the editor
        being closed. */
    ExternalMidiOutput externalMidiOutput;

    const SequencerEngine& getEngine() const noexcept { return engine; }

private:
    //==========================================================================
    /** Cached raw-parameter pointers. Looking IDs up by string on the audio
        thread would mean hashing and locking, so we resolve them once here.
    */
    struct LaneParams
    {
        std::atomic<float>* values[params::numSteps] {};
        std::atomic<float>* enabled[params::numSteps] {};
        std::atomic<float>* chance[params::numSteps] {};

        // Note lanes only -- a CC lane has neither parameter.
        std::atomic<float>* stepVelocity[params::numSteps] {};
        std::atomic<float>* stepGate[params::numSteps] {};

        std::atomic<float>* active    = nullptr;
        std::atomic<float>* length    = nullptr;
        std::atomic<float>* division  = nullptr;
        std::atomic<float>* direction = nullptr;
        std::atomic<float>* depth     = nullptr;
        std::atomic<float>* mode      = nullptr;
        std::atomic<float>* nudge     = nullptr;
        std::atomic<float>* humanize  = nullptr;

        // CC lanes only -- a note lane no longer has a CC output of its own.
        std::atomic<float>* ccOn      = nullptr;
        std::atomic<float>* ccNumber  = nullptr;
        std::atomic<float>* ccChannel = nullptr;
        std::atomic<float>* ccOffset  = nullptr;
    };

    SequencerEngine::Snapshot buildSnapshot() const;

    LaneParams noteLaneParams[params::numLanes];
    LaneParams ccLaneParams[params::numLanes];

    std::atomic<float>* pNoteTriggerSrc = nullptr;
    std::atomic<float>* pQuantize     = nullptr;
    std::atomic<float>* pBendRange    = nullptr;
    std::atomic<float>* pRootNote     = nullptr;
    std::atomic<float>* pRangeOctaves = nullptr;
    std::atomic<float>* pScale        = nullptr;
    std::atomic<float>* pVelocity     = nullptr;
    std::atomic<float>* pMidiChannel  = nullptr;
    std::atomic<float>* pCcOn         = nullptr;
    std::atomic<float>* pCcNumber     = nullptr;
    std::atomic<float>* pCcChannel    = nullptr;
    std::atomic<float>* pNoteOffset   = nullptr;
    std::atomic<float>* pCcOffset     = nullptr;
    std::atomic<float>* pSlew         = nullptr;
    std::atomic<float>* pFreeRun      = nullptr;
    std::atomic<float>* pNoteSwing    = nullptr;
    std::atomic<float>* pCcSwing      = nullptr;
    std::atomic<float>* pVoiceCount   = nullptr;
    std::atomic<float>* pPolyMode     = nullptr;
    std::atomic<float>* pMpeEnabled   = nullptr;
    std::atomic<float>* pNoteLaneCount = nullptr;
    std::atomic<float>* pCcLaneCount   = nullptr;

    SequencerEngine engine;

    // Keeps the sequencer moving while the host transport is stopped, so you can
    // audition patterns without hitting play.
    double freeRunPpq = 0.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RavelAudioProcessor)
};
