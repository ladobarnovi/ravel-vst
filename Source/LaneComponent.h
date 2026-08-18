#pragma once

#include "Controls.h"
#include "Parameters.h"
#include "Theme.h"

/** Which of a step's three continuous parameters the tall bars currently edit.

    All three are full-height bars stacked in the same rectangle with one visible at a time,
    rather than three smaller bars competing for the slot. The two that are hidden still show
    as faint ticks, so the whole step stays readable while only one is editable -- and every
    bar keeps its own parameter attachment, since nothing has to be rebound when the
    selection changes.
*/
enum class StepLayer { value = 0, velocity = 1, chance = 2 };

//==============================================================================
/** One step: a tall bar for the selected layer, ticks for the other two, and a gate strip. */
class StepSlot final : public juce::Component
{
public:
    StepSlot (juce::AudioProcessorValueTreeState& state, int laneIndex, int stepIndex);

    void paintOverChildren (juce::Graphics&) override;
    void resized() override;

    void setPlaying (bool shouldBePlaying);
    void setLayer (StepLayer layer);

    /** Dims the whole slot while its lane is muted, so a muted lane still shows its pattern
        and its playhead but never competes with the lanes that are actually sounding. */
    void setLaneActive (bool laneIsActive);

    /** Marks the slot as sitting past the lane's Length, which the sequencer never reaches.
        The slot recedes into the panel rather than disappearing: it is still editable, so a
        pattern can be drawn past the end and brought into play by raising Length. */
    void setWithinLength (bool isWithinLength);

private:
    /** Recolours the visible bar to match the gate, so a muted step reads as muted without
        needing a separate indicator. */
    void applyGateState();

    /** A faint mark at a hidden layer's level, drawn over the visible bar. Each layer owns
        one of three horizontal thirds, so no two ticks can ever sit on top of each other. */
    void drawLayerTick (juce::Graphics&, const juce::Slider&, StepLayer) const;

    /** The rectangle the three bars share, which is the slot minus the gate strip. */
    juce::Rectangle<int> barArea() const;

    juce::Slider valueSlider, velocitySlider, chanceSlider;
    juce::ToggleButton onButton;

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> valueAttachment,
                                                                         velocityAttachment,
                                                                         chanceAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> onAttachment;

    juce::Colour accent;
    bool playing = false;
    bool laneActive = true;
    bool withinLength = true;
    StepLayer currentLayer = StepLayer::value;

    juce::Slider& sliderFor (StepLayer) noexcept;

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
    juce::ToggleButton onButton;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> onAttachment;

    juce::OwnedArray<StepSlot> slots;

    params::LanePattern& clipboard;

    ControlGroup paramGroup;

    juce::TextButton randomiseButton { "Rnd" }, clearButton { "Clr" }, menuButton { "..." };
    juce::Random random;

    // Which per-step parameter the eight bars edit. Per lane rather than global, so one lane
    // can be shown as accents while another is being dialled in for pitch.
    juce::TextButton layerButtons[3] { juce::TextButton ("Value"),
                                       juce::TextButton ("Velocity"),
                                       juce::TextButton ("Prob") };

    StepLayer currentLayer = StepLayer::value;

    void setLayer (StepLayer);

    /** Pushes the mute through to the slots and the lane's own accents. Tracks the last
        state it applied because Button::onStateChange also fires on hover, and repainting
        eight slots every time the mouse crosses the toggle is work for nothing. */
    void applyLaneState();

    int appliedLaneActive = -1;

    /** Greys the steps the lane's Length leaves out of the cycle. */
    void applyLength();

    // The Length row's own widget, watched so the steps follow it. Non-owning: the row
    // belongs to paramGroup.
    juce::Slider* lengthSlider = nullptr;
    int appliedLength = -1;

    // Set in resized(), drawn in paint(): the hairline between steps and parameters.
    int dividerX = 0;

    int playingStep = -1;

    void showActionsMenu();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LaneComponent)
};
