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
    juce::Slider valueSlider, chanceSlider;
    juce::ToggleButton onButton;

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> valueAttachment, chanceAttachment;
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
    LaneComponent (juce::AudioProcessorValueTreeState& state, int laneIndex,
                   params::LanePattern& sharedClipboard);

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

    params::LanePattern& clipboard;

    juce::Slider lengthSlider, depthSlider, nudgeSlider, humanizeSlider;
    juce::Slider ccNumberSlider, ccChannelSlider;
    juce::ComboBox divisionBox, directionBox, modeBox;
    juce::ToggleButton ccOnButton;

    juce::Label lengthLabel, depthLabel, divisionLabel, directionLabel, modeLabel;
    juce::Label nudgeLabel, humanizeLabel, ccOnLabel, ccNumberLabel, ccChannelLabel;

    juce::TextButton randomiseButton { "RND" }, clearButton { "CLR" }, menuButton { "..." };
    juce::Label patternLabel;
    juce::Random random;

    void showActionsMenu();

    using SliderAttachment   = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboBoxAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using ButtonAttachment   = juce::AudioProcessorValueTreeState::ButtonAttachment;

    std::unique_ptr<SliderAttachment> lengthAttachment, depthAttachment, nudgeAttachment,
                                      humanizeAttachment, ccNumberAttachment, ccChannelAttachment;
    std::unique_ptr<ComboBoxAttachment> divisionAttachment, directionAttachment, modeAttachment;
    std::unique_ptr<ButtonAttachment> ccOnAttachment;

    int playingStep = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LaneComponent)
};
