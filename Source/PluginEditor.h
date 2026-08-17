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

    // Non-owning; point into the tab pages. Dimmed when the current mode ignores them.
    ControlRow* scaleRow = nullptr;
    ControlRow* bendRangeRow = nullptr;
    ControlRow* quantizeRow = nullptr;
    ControlRow* triggerRow = nullptr;

    std::atomic<float>* quantizeParam = nullptr;
    int lastQuantize = -1;

    // Watched alongside Quantize because the scale decides whether Bend Range is in play:
    // a 19-, 23- or 53-EDO scale rides on the wheel even with Quantize on.
    std::atomic<float>* scaleParam = nullptr;
    int lastScale = -1;

    std::atomic<float>* polyModeParam = nullptr;
    int lastPolyMode = -1;

    MixMeter mixMeter;

    juce::Rectangle<int> panelArea;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TriLaneAudioProcessorEditor)
};
