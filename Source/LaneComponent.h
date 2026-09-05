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

    /** Extra space after every fourth step, so the sixteen steps read as four groups of
        four instead of one undifferentiated row. On top of stepSlotWidth's own per-step
        pitch, not in place of it -- see LaneComponent::resized(). */
    inline constexpr int groupGap = 6;

    /** One of these gaps sits between each pair of groups: three for four groups of four. */
    inline constexpr int numGroupGaps = params::numSteps / 4 - 1;

    /** Everything in a lane that is not step area. */
    inline constexpr int chromeWidth = inset * 2 + railWidth + railGap + numberWidth
                                         + columnGap + dividerGap + paramWidth;

    /** The width a lane wants: its chrome, a full-size slot per step, and the group gaps
        between them. */
    inline constexpr int nativeWidth = chromeWidth + params::numSteps * stepSlotWidth
                                         + numGroupGaps * groupGap;
}

/** Which of a step's four continuous parameters the tall bars currently edit.

    All four are full-height bars stacked in the same rectangle with one visible at a time,
    rather than four smaller bars competing for the slot. Every bar keeps its own parameter
    attachment, since nothing has to be rebound when the selection changes.
*/
enum class StepLayer { value = 0, velocity = 1, chance = 2, gate = 3 };

/** How many layers a step has, and how many StepLayer values there are. */
inline constexpr int numStepLayers = 4;

//==============================================================================
/** One step: a tall bar for the selected layer, and a trig strip. */
class StepSlot final : public juce::Component
{
public:
    /** For a CC-kind slot, only the Value and Chance sliders get an attachment and become
        visible at all -- a CC lane never starts a note, so it has no Velocity or Gate
        parameter to bind to in the first place (see Parameters.cpp). */
    StepSlot (juce::AudioProcessorValueTreeState& state, int laneIndex, int stepIndex,
             params::LaneKind kind = params::LaneKind::note);

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
    /** Recolours the visible bar to match the trig, so a muted step reads as muted without
        needing a separate indicator. */
    void applyTrigState();

    /** The rectangle the bars share, which is the slot minus the trig strip. */
    juce::Rectangle<int> barArea() const;

    juce::Slider valueSlider, velocitySlider, chanceSlider, gateSlider;
    juce::ToggleButton onButton;

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> valueAttachment,
                                                                         velocityAttachment,
                                                                         chanceAttachment,
                                                                         gateAttachment;
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
/** A full lane: 16 steps plus the feel parameters worth reaching for while it plays.

    A Note lane and a CC lane share the same Length/Rate/Depth/Direction. Only a CC lane also
    carries its own Send/Number/Channel/Offset, since only CC has a per-lane destination
    independent of that fold. See LaneKind.
*/
class LaneComponent final : public juce::Component
{
public:
    /** For a CC-kind lane, no layer selector is ever created -- the bars always edit Value
        -- and the parameter block goes on to add the lane's own Send/Number/Channel/Offset
        after Direction: a CC lane still folds into the Mix CC like any lane folds into a mix,
        and keeps its own direct tap besides. */
    LaneComponent (juce::AudioProcessorValueTreeState& state, int laneIndex,
                   params::LanePattern& sharedClipboard,
                   params::LaneKind kind = params::LaneKind::note);

    void paint (juce::Graphics&) override;
    void resized() override;

    /** Called from the editor's timer with the lane's current step. */
    void setPlayingStep (int stepIndex);

    /** Hidden on the last remaining lane, since an instance always has at least one. */
    void setCanRemove (bool canBeRemoved);

    /** Takes the layer selector away while Notes is off, leaving the bars editing Value.

        Velocity and Gate are unread with Notes off -- both are only ever arguments to
        startNote.
    */
    void setLayerSelectionAvailable (bool available);

    /** Invoked when this lane's Remove button is clicked. The editor supplies it, because
        removing a lane is a change to the stack rather than to the lane -- the lanes above
        this one move down, and the window resizes. */
    std::function<void()> onRemove;

private:
    juce::AudioProcessorValueTreeState& apvts;
    const int lane;
    const params::LaneKind kind;
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
                                                   juce::TextButton ("Gate") };

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
