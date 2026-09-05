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

class LaneComponent;

//==============================================================================
/** One step: a tall bar for the selected layer, and a trig strip.

    The slot, not the bar inside it, is what takes the mouse. A bar is a Slider, and a Slider
    that is handed a mouse-down keeps every drag event that follows it, wherever the cursor
    goes -- which is exactly the behaviour to avoid here, since a drag that crosses into the
    next step should start editing that step instead. So the sliders are made deaf to the
    mouse, the slot receives the gesture, and the lane decides which slot's bar each event
    belongs to and hands it over. What the bar is given is still the ordinary Slider event
    stream, so snapping, the value bubble and the host gesture the undo history reads all
    behave as they would have.
*/
class StepSlot final : public juce::Component,
                       public juce::TooltipClient
{
public:
    /** For a CC-kind slot, only the Value and Chance sliders get an attachment and become
        visible at all -- a CC lane never starts a note, so it has no Velocity or Gate
        parameter to bind to in the first place (see Parameters.cpp). */
    StepSlot (juce::AudioProcessorValueTreeState& state, int laneIndex, int stepIndex,
             params::LaneKind kind = params::LaneKind::note);

    void paintOverChildren (juce::Graphics&) override;
    void resized() override;

    void mouseDown        (const juce::MouseEvent&) override;
    void mouseDrag        (const juce::MouseEvent&) override;
    void mouseUp          (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove   (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

    /** The bar's own tooltip, answered by the slot because the tooltip window can only ask
        whatever is under the mouse -- and the bar never is. Carries the step's current value
        as well as the layer's description: hovering a bar used to raise the slider's own
        value bubble, which went the way of its mouse handling. */
    juce::String getTooltip() override;

    /** True if the point, in this slot's coordinates, is in the bar rather than in the trig
        strip beneath it. */
    bool barContains (juce::Point<int> positionInSlot) const;

    /** The three stages of a stroke, as they reach this slot's bar. The event may come from
        anywhere -- a stroke that started three steps away is still one drag, and its events
        arrive in the coordinates of the slot it started in -- so each is rebased onto the
        bar before being handed over. */
    void beginBarDrag    (const juce::MouseEvent&);
    void continueBarDrag (const juce::MouseEvent&);
    void endBarDrag      (const juce::MouseEvent&);

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

    /** The bar a stroke would edit, or nullptr where the lane has no such layer to edit -- a
        CC step has no Velocity or Gate bar to reach for. */
    juce::Slider* activeBar() noexcept;

    /** Guards the one place the forwarding could turn back on itself: a Slider that makes no
        use of a wheel event passes it up to its parent, which is this slot. */
    bool forwardingWheel = false;

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

    ~LaneComponent() override;

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

    /** Invoked with true as a stroke across the step bars begins and false as it ends. The
        editor supplies it, because what it is for is the undo history, which lives on the
        processor: a stroke is one thing the user did and should step back in one press,
        and the history's own rule -- one turn of the message loop is one edit -- would
        otherwise make a separate step of every drag callback the stroke passes through.
        See UndoHistory::setEditHeldOpen. */
    std::function<void (bool)> onStrokeActive;

    //==========================================================================
    // Called by StepSlot, which receives the mouse but does not decide what it means: which
    // step a moving cursor is editing is the lane's business, since only the lane can see
    // the other fifteen.

    /** Opens a stroke on the slot the mouse went down in. */
    void startStroke (StepSlot& slot, const juce::MouseEvent&);

    /** Carries the stroke on, handing the drag to whichever slot the cursor has reached: the
        one it is leaving is released where it stands, and the one it arrives at picks the
        gesture up from there. */
    void continueStroke (const juce::MouseEvent&);

    /** Closes the stroke on whichever slot it ended over. */
    void endStroke (const juce::MouseEvent&);

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

    /** The slot the stroke in progress is editing, which is not necessarily the one it
        started in. Null between strokes. The slots outlive any stroke -- they are built once
        and never replaced -- so this does not need to be a SafePointer. */
    StepSlot* strokeSlot = nullptr;

    /** Where the stroke was when it was last heard from, in the lane's coordinates. A drag
        reports a handful of positions a frame apart, so this and the position that has just
        arrived are the two ends of a line the cursor has already travelled -- see
        continueStroke, which has to fill in the steps along it. */
    juce::Point<float> strokePosition;

    /** The step a stroke at this distance across the lane should be editing, or -1 in a lane
        with no steps at all.

        Nearest by centre rather than a hit test: the gaps between the slots, wider still
        between the groups of four, are dead space a hit test would drop the stroke into, and
        the bar would stop following the cursor for the few pixels between one step and the
        next. Height plays no part either -- a stroke that wanders above or below the row
        keeps painting the step it is over, with only the value it writes running out of
        range.
    */
    int slotIndexForStroke (float xInLane) const;

    /** Moves the stroke onto another step, releasing the one it is leaving where it stands
        and opening the new one at the given point. */
    void handStrokeTo (StepSlot& slot, const juce::MouseEvent& atPoint);

    /** Where a stroke that travelled from one point to the other crossed the given step:
        that step's centre, at the height the line between the two had reached by then. */
    juce::Point<float> pointCrossingSlot (int index, juce::Point<float> from,
                                          juce::Point<float> to) const;

    void showActionsMenu();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LaneComponent)
};
