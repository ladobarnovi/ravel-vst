#pragma once

#include "Parameters.h"
#include "SequencerEngine.h"

class TriLaneAudioProcessor final : public juce::AudioProcessor
{
public:
    TriLaneAudioProcessor();
    ~TriLaneAudioProcessor() override = default;

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
        std::atomic<float>* stepVelocity[params::numSteps] {};
        std::atomic<float>* length    = nullptr;
        std::atomic<float>* division  = nullptr;
        std::atomic<float>* direction = nullptr;
        std::atomic<float>* depth     = nullptr;
        std::atomic<float>* mode      = nullptr;
        std::atomic<float>* nudge     = nullptr;
        std::atomic<float>* humanize  = nullptr;
        std::atomic<float>* velocity  = nullptr;
        std::atomic<float>* ccOn      = nullptr;
        std::atomic<float>* ccNumber  = nullptr;
        std::atomic<float>* ccChannel = nullptr;
    };

    SequencerEngine::Snapshot buildSnapshot() const;

    LaneParams laneParams[params::numLanes];

    std::atomic<float>* pOutputMode  = nullptr;
    std::atomic<float>* pTriggerSrc  = nullptr;
    std::atomic<float>* pQuantize    = nullptr;
    std::atomic<float>* pBendRange   = nullptr;
    std::atomic<float>* pRootNote    = nullptr;
    std::atomic<float>* pRangeSteps  = nullptr;
    std::atomic<float>* pScale       = nullptr;
    std::atomic<float>* pVelocity    = nullptr;
    std::atomic<float>* pGateLength  = nullptr;
    std::atomic<float>* pMidiChannel = nullptr;
    std::atomic<float>* pCcNumber    = nullptr;
    std::atomic<float>* pCcChannel   = nullptr;
    std::atomic<float>* pOffset      = nullptr;
    std::atomic<float>* pSlew        = nullptr;
    std::atomic<float>* pFreeRun     = nullptr;
    std::atomic<float>* pSwing       = nullptr;
    std::atomic<float>* pVoiceCount  = nullptr;
    std::atomic<float>* pPolyMode    = nullptr;

    SequencerEngine engine;

    // Keeps the sequencer moving while the host transport is stopped, so you can
    // audition patterns without hitting play.
    double freeRunPpq = 0.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TriLaneAudioProcessor)
};
