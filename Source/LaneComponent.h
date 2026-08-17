#pragma once

#include "Parameters.h"
#include "Theme.h"

/** One step: a vertical value bar with an on/off toggle underneath. */
class StepSlot final : public juce::Component
{
public:
    StepSlot (juce::AudioProcessorValueTreeState& state, int laneIndex, int stepIndex);

    void paint (juce::Graphics&) override;
    void resized() override;

    void setPlaying (bool shouldBePlaying);

private:
    juce::Slider valueSlider;
    juce::ToggleButton onButton;

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> valueAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> onAttachment;

    juce::Colour accent;
    bool playing = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (StepSlot)
};

//==============================================================================
/** A full lane: 8 steps plus the lane's length, clock division, direction,
    depth and combine mode.
*/
class LaneComponent final : public juce::Component
{
public:
    LaneComponent (juce::AudioProcessorValueTreeState& state, int laneIndex);

    void paint (juce::Graphics&) override;
    void resized() override;

    /** Called from the editor's timer with the lane's current step. */
    void setPlayingStep (int stepIndex);

private:
    juce::AudioProcessorValueTreeState& apvts;
    const int lane;
    const juce::Colour accent;

    juce::Label titleLabel;
    juce::OwnedArray<StepSlot> slots;

    juce::Slider lengthSlider, depthSlider;
    juce::ComboBox divisionBox, directionBox, modeBox;
    juce::Label lengthLabel, depthLabel, divisionLabel, directionLabel, modeLabel;

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>   lengthAttachment, depthAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> divisionAttachment, directionAttachment, modeAttachment;

    int playingStep = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LaneComponent)
};
