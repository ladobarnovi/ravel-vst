#pragma once

#include "Controls.h"
#include "LaneComponent.h"
#include "PluginProcessor.h"
#include "Theme.h"

/** Read-out of the combined value the three lanes currently produce. */
class MixMeter final : public juce::Component
{
public:
    void setValue (float newValue);
    void paint (juce::Graphics&) override;

private:
    float value = 0.0f;
};

//==============================================================================
class TriLaneAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                          private juce::Timer
{
public:
    explicit TriLaneAudioProcessorEditor (TriLaneAudioProcessor&);
    ~TriLaneAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    /** Fills in the three tab pages. Split out only because listing 28 parameters inline
        buries the layout code in the constructor. */
    void buildTabs();

    // Declared first so it outlives every child that references it.
    TriLaneLookAndFeel lookAndFeel;

    TriLaneAudioProcessor& processorRef;

    juce::Label titleLabel, mixCaption;

    // Shared by all three lanes, so a pattern can be copied from one and pasted onto another.
    params::LanePattern patternClipboard;

    juce::OwnedArray<LaneComponent> lanes;

    // Output mode is the one global that changes what the plugin *is*, so it stays in the
    // header rather than going behind a tab with the rest of the setup.
    ControlGroup outputGroup;

    TabPage pitchPage, timingPage, routingPage;
    TabStrip tabs;

    // Non-owning; point into the tab pages. Dimmed when the pitch mode ignores them.
    ControlRow* scaleRow = nullptr;
    ControlRow* bendRangeRow = nullptr;
    ControlRow* noteChannelRow = nullptr;

    std::atomic<float>* pitchModeParam = nullptr;
    int lastPitchMode = -1;

    MixMeter mixMeter;

    juce::Rectangle<int> panelArea;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TriLaneAudioProcessorEditor)
};
