#pragma once

#include "Controls.h"
#include "Parameters.h"
#include "Theme.h"

/** Chance, drawn as a tick across its parent step bar.

    It covers the same rectangle as the value bar, but only claims the mouse in a narrow
    strip down the right-hand edge -- a child that fails hitTest passes the event through
    to the sibling underneath, so the bulk of the bar still drags the value. That is what
    lets one step present two continuous parameters without stacking two visible bars.
*/
class ChanceOverlay final : public juce::Slider
{
public:
    static constexpr int gutterWidth = 9;

    bool hitTest (int x, int) override { return x >= getWidth() - gutterWidth; }
};

//==============================================================================
/** One step: a tall value bar, a chance tick across it, and a gate strip beneath. */
class StepSlot final : public juce::Component
{
public:
    StepSlot (juce::AudioProcessorValueTreeState& state, int laneIndex, int stepIndex);

    void paintOverChildren (juce::Graphics&) override;
    void resized() override;

    void setPlaying (bool shouldBePlaying);

private:
    /** Recolours the value bar to match the gate, so a muted step reads as muted without
        needing a separate indicator. */
    void applyGateState();

    juce::Slider valueSlider;
    ChanceOverlay chanceSlider;
    juce::ToggleButton onButton;

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> valueAttachment, chanceAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> onAttachment;

    juce::Colour accent;
    bool playing = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (StepSlot)
};

//==============================================================================
/** A full lane: 8 steps plus the six parameters worth reaching for while it plays.

    The lane's routing parameters (its CC number, CC channel and CC enable) and its
    humanise amount live in the editor's tab pages instead: they are set once when the
    track is wired up, and keeping them here is what made every lane read as a wall of
    identical controls.
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

    juce::Label numberLabel;
    juce::OwnedArray<StepSlot> slots;

    params::LanePattern& clipboard;

    ControlGroup paramGroup;

    juce::TextButton randomiseButton { "Rnd" }, clearButton { "Clr" }, menuButton { "..." };
    juce::Random random;

    // Set in resized(), drawn in paint(): the hairline between steps and parameters.
    int dividerX = 0;

    int playingStep = -1;

    void showActionsMenu();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LaneComponent)
};
