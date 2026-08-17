#pragma once

#include "LaneComponent.h"
#include "PluginProcessor.h"
#include "Theme.h"

/** A caption plus whichever widget suits the parameter's type.

    Building the widget from the parameter rather than declaring one per control
    keeps the output section to a list of IDs instead of thirty near-identical
    member declarations.
*/
class ParamCell final : public juce::Component
{
public:
    ParamCell (juce::AudioProcessorValueTreeState& state,
               const juce::String& paramID,
               const juce::String& caption,
               int textBoxWidth = 58);

    void resized() override;

    /** Greys out and disables the cell, for parameters the current mode ignores. */
    void setDimmed (bool shouldBeDimmed);

private:
    juce::Label captionLabel;
    std::unique_ptr<juce::Component> control;
    bool dimmed = false;

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>   sliderAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> comboAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>   buttonAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ParamCell)
};

//==============================================================================
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

    // Declared first so it outlives every child that references it.
    TriLaneLookAndFeel lookAndFeel;

    TriLaneAudioProcessor& processorRef;

    juce::Label titleLabel, outputSectionLabel, mixCaption;

    // Shared by all three lanes, so a pattern can be copied from one and pasted onto another.
    params::LanePattern patternClipboard;

    juce::OwnedArray<LaneComponent> lanes;
    juce::OwnedArray<ParamCell> headerCells, outputCells;

    // Non-owning; point into outputCells. Dimmed when the pitch mode ignores them.
    ParamCell* scaleCell = nullptr;
    ParamCell* noteChannelCell = nullptr;

    std::atomic<float>* pitchModeParam = nullptr;
    int lastPitchMode = -1;

    MixMeter mixMeter;

    juce::Rectangle<int> outputPanelArea;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TriLaneAudioProcessorEditor)
};
