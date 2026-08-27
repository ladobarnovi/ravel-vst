#pragma once

#include "Controls.h"
#include "Parameters.h"
#include "Theme.h"

/** Fixed horizontal metrics of a lane.

    These live in the header rather than in LaneComponent.cpp because the editor's native
    window width is the sum of them plus the step area: the window is sized to give the
    steps the width they want, rather than the steps taking whatever a fixed window leaves
    over. See nativeContentWidth in PluginEditor.cpp.
*/
namespace lane
{
    inline constexpr int inset       = 10;  ///< Reduction applied to the lane's own bounds.
    inline constexpr int railWidth   = 3;   ///< The lane's accent stripe down the left edge.
    inline constexpr int railGap     = 8;
    inline constexpr int numberWidth = 54;  ///< Number label, and the layer buttons under it.
    inline constexpr int columnGap   = 10;  ///< That column to the first step.
    inline constexpr int dividerGap  = 14;  ///< Last step to the parameter block; holds the hairline.
    inline constexpr int paramWidth  = 280;

    /** Pitch of one step slot: the bar plus the gap before the next slot. */
    inline constexpr int stepSlotWidth = 40;

    /** Everything in a lane that is not step area. */
    inline constexpr int chromeWidth = inset * 2 + railWidth + railGap + numberWidth
                                         + columnGap + dividerGap + paramWidth;

    /** The width a lane wants: its chrome plus a full-size slot per step. */
    inline constexpr int nativeWidth = chromeWidth + params::numSteps * stepSlotWidth;
}

/** Which of a step's three continuous parameters the tall bars currently edit.

    All three are full-height bars stacked in the same rectangle with one visible at a time,
    rather than three smaller bars competing for the slot. The two that are hidden still show
    as faint ticks, so the whole step stays readable while only one is editable -- and every
    bar keeps its own parameter attachment, since nothing has to be rebound when the
    selection changes.
*/
enum class StepLayer { value = 0, velocity = 1, chance = 2, gate = 3, slide = 4 };

/** How many layers a step has, and how many StepLayer values there are. */
inline constexpr int numStepLayers = 5;

//==============================================================================
/** One step: a tall bar for the selected layer, ticks for the others, and a trig strip. */
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

    /** False in CC mode, where velocity and gate are never read. Suppresses those two ticks,
        which would otherwise go on showing values nothing in that mode acts on.

        Chance ticks are deliberately not covered by this: chance still gates the mix, and so
        still shapes the CC output, even with the selector gone.
    */
    void setNoteLayersAvailable (bool available);

    /** Marks the slot as sitting past the lane's Length, which the sequencer never reaches.
        The slot recedes into the panel rather than disappearing: it is still editable, so a
        pattern can be drawn past the end and brought into play by raising Length. */
    void setWithinLength (bool isWithinLength);

private:
    /** Recolours the visible bar to match the trig, so a muted step reads as muted without
        needing a separate indicator. */
    void applyTrigState();

    /** A faint mark at a hidden layer's level, drawn over the visible bar. Each layer owns
        one of four horizontal quarters, so no two ticks can ever sit on top of each other. */
    void drawLayerTick (juce::Graphics&, const juce::Slider&, StepLayer) const;

    /** The rectangle the bars share, which is the slot minus the trig strip. */
    juce::Rectangle<int> barArea() const;

    juce::Slider valueSlider, velocitySlider, chanceSlider, gateSlider, slideSlider;
    juce::ToggleButton onButton;

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> valueAttachment,
                                                                         velocityAttachment,
                                                                         chanceAttachment,
                                                                         gateAttachment,
                                                                         slideAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> onAttachment;

    juce::Colour accent;
    bool playing = false;
    bool laneActive = true;
    bool withinLength = true;
    bool noteLayersAvailable = true;
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

    /** Hidden on the last remaining lane, since an instance always has at least one. */
    void setCanRemove (bool canBeRemoved);

    /** Takes the layer selector away for CC mode, leaving the bars editing Value.

        Velocity and Gate are unread in that mode -- both are only ever arguments to
        startNote. Chance is not: it still gates whether a step reaches the mix, and the mix
        is the CC value. It is hidden here because it is not wanted, not because it is inert,
        so the slots go on drawing chance ticks -- see StepSlot::setNoteLayersAvailable. That
        keeps a step whose probability is still shaping the CC stream from doing it invisibly.
    */
    void setLayerSelectionAvailable (bool available);

    /** Invoked when this lane's Remove button is clicked. The editor supplies it, because
        removing a lane is a change to the stack rather than to the lane -- the lanes above
        this one move down, and the window resizes. */
    std::function<void()> onRemove;

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

    // Spelt out rather than abbreviated: these sit among value rows whose captions are whole
    // words, and "Clr" next to "Remove" was two controls a keystroke apart meaning "empty
    // this lane" and "destroy this lane" -- exactly the pair not to leave the reader decoding.
    // The layout sizes each one to its own label, so the widths differ.
    juce::TextButton randomiseButton { "Randomize" }, clearButton { "Clear" },
                     menuButton { "More" };

    juce::TextButton removeButton { "Remove" };

    juce::Random random;

    // Which per-step parameter the eight bars edit. Per lane rather than global, so one lane
    // can be shown as accents while another is being dialled in for pitch.
    juce::TextButton layerButtons[numStepLayers] { juce::TextButton ("Value"),
                                                   juce::TextButton ("Velocity"),
                                                   juce::TextButton ("Prob"),
                                                   juce::TextButton ("Gate"),
                                                   juce::TextButton ("Slide") };

    StepLayer currentLayer = StepLayer::value;
    bool layerSelectionAvailable = true;

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
