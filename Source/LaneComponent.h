#pragma once

#include "Controls.h"
#include "Parameters.h"
#include "Theme.h"

/** Fixed metrics of a lane.

    These live in the header rather than in LaneComponent.cpp because the editor's native
    window width is the sum of them: the window is sized to give the steps the width they
    want, rather than the steps taking whatever a fixed window leaves over. See
    nativeContentWidth in PluginEditor.cpp.
*/
namespace lane
{
    //--------------------------------------------------------------------------
    // Across.
    inline constexpr int railWidth    = 3;   ///< The accent edge, hard against the lane's left.
    inline constexpr int slotWidth    = 32;  ///< Lane number, and the mute under it.
    inline constexpr int selectorWidth = 46; ///< Val / Vel / Prob / Gate -- Val alone on a CC lane.
    inline constexpr int paramWidth   = 190; ///< Length, Rate, Direction, Mix amount.
    inline constexpr int columnGap    = 14;
    inline constexpr int padRight     = 16;

    //--------------------------------------------------------------------------
    // Down.
    inline constexpr int padTop    = 12;
    inline constexpr int padBottom = 11;

    //--------------------------------------------------------------------------
    // The step area.
    inline constexpr int stepBarWidth = 48;
    inline constexpr int stepGap      = 4;   ///< Between one step slot and the next.
    inline constexpr int wellPadX      = 8;
    inline constexpr int wellPadTop    = 7;
    inline constexpr int wellPadBottom = 6;

    inline constexpr int barHeight    = 112; ///< The tall value bar.
    inline constexpr int stepInnerGap = 5;   ///< Bar to trig, and trig to number.
    inline constexpr int trigHeight   = 9;   ///< The on/off strip under a bar.
    inline constexpr int numberHeight = 10;  ///< 1..16 under the trigs.

    /** One step slot, top to bottom. */
    inline constexpr int slotHeight = barHeight + stepInnerGap + trigHeight
                                        + stepInnerGap + numberHeight;

    inline constexpr int wellHeight = wellPadTop + slotHeight + wellPadBottom;

    /** Sixteen full-width bars, the gaps between them, and the well's own padding. */
    inline constexpr int wellWidth = wellPadX * 2 + params::numSteps * stepBarWidth
                                        + (params::numSteps - 1) * stepGap;

    /** Everything in a lane that is not step area. */
    inline constexpr int chromeWidth = slotWidth + columnGap + selectorWidth + columnGap
                                          + columnGap + paramWidth + padRight;

    /** The width a lane wants. Both kinds are the same, so switching tabs does not shuffle
        the step grid sideways under the cursor. */
    inline constexpr int nativeWidth = chromeWidth + wellWidth;

    //--------------------------------------------------------------------------
    /** The hairline between one lane and the next, carried inside the lane's own bounds. */
    inline constexpr int separatorHeight = 1;

    /** Gap above a lane's action chips, and their height. */
    inline constexpr int actionGap    = 12;
    inline constexpr int actionHeight = 21;

    /** How tall a lane needs to be: whichever of its two columns wins.

        The step area is a fixed height, and the parameter column's is the sum of the four
        parameters every lane carries. One height for both kinds -- a CC lane's strip holds
        the same Length, Rate, Direction and Mix amount a Note lane's does, and the two only
        differ in how many layers sit behind the step bars.
    */
    int height();
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
/** One step: a tall bar for the selected layer, a trig strip, and its own number.

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

    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void resized() override;

    /** Where the value bubble a drag pops up gets parented. Not answerable at construction --
        see the definition. */
    void parentHierarchyChanged() override;

    void mouseDown        (const juce::MouseEvent&) override;
    void mouseDrag        (const juce::MouseEvent&) override;
    void mouseUp          (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove   (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

    /** The bar's own tooltip, answered by the slot because the tooltip window can only ask
        whatever is under the mouse -- and the bar never is. Carries the step's current value
        as well as the layer's description. */
    juce::String getTooltip() override;

    /** True if the point, in this slot's coordinates, is in the bar rather than in the trig
        strip or the number beneath it. */
    bool barContains (juce::Point<int> positionInSlot) const;

    /** The three stages of a stroke, as they reach this slot's bar. The event may come from
        anywhere -- a stroke that started three steps away is still one drag, and its events
        arrive in the coordinates of the slot it started in -- so each is rebased onto the bar
        before being handed over. */
    void beginBarDrag    (const juce::MouseEvent&);
    void continueBarDrag (const juce::MouseEvent&);
    void endBarDrag      (const juce::MouseEvent&);

    void setPlaying (bool shouldBePlaying);
    void setLayer (StepLayer layer);

    /** Dims the whole slot while its lane is muted, so a muted lane still shows its pattern
        and its playhead but never competes with the lanes that are actually sounding. */
    void setLaneActive (bool laneIsActive);

    /** Marks the slot as sitting past the lane's Length, which the sequencer never reaches.
        The slot recedes rather than disappearing: it is still editable, so a pattern can be
        drawn past the end and brought into play by raising Length. */
    void setWithinLength (bool isWithinLength);

private:
    /** Recolours the visible bar for the step's own on/off and for the playhead. */
    void applyTrigState();

    /** Fades the whole slot for the two states that are not the step's own: its lane being
        muted, and it sitting past that lane's Length. Both multiply, so a step past the end of
        a muted lane is fainter than either on its own. */
    void applySlotAlpha();

    /** The rectangle the bars share: the slot minus the trig strip and the number. */
    juce::Rectangle<int> barArea() const;

    const int step;

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

    // The toggle state the colours on screen were last built for. Tri-state so the first pass
    // always runs; see the onStateChange guard in the constructor.
    int  appliedTrigOn = -1;
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
/** A full lane: 16 steps plus the parameters worth reaching for while it plays.

    Both kinds carry the same strip -- Length, Rate, Direction and Mix amount -- because both
    are the same sequencer with a different destination on the end of it. They differ in one
    place only: how many layers sit behind the step bars, and so how many chips the selector
    beside them offers. A Note lane has four (Value, Velocity, Prob, Gate); a CC lane has
    Value alone, because Velocity and Gate are only ever arguments to starting a note and a CC
    lane never starts one.

    A CC lane's own Send/Number/Channel/Offset live in the CC page's footer rather than here,
    one column per lane -- they are a destination, which is a property of where the lane goes
    rather than of the pattern in it, and putting them on the strip made a CC lane twice the
    parameter block of a Note lane for something the user sets once.
*/
class LaneComponent final : public juce::Component
{
public:
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

    /** Invoked when this lane's Remove button is clicked. The editor supplies it, because
        removing a lane is a change to the stack rather than to the lane -- the lanes above
        this one move down, and the window resizes. */
    std::function<void()> onRemove;

    /** Invoked with true as a stroke across the step bars begins and false as it ends. The
        editor supplies it, because what it is for is the undo history, which lives on the
        processor: a stroke is one thing the user did and should step back in one press, and
        the history's own rule -- one turn of the message loop is one edit -- would otherwise
        make a separate step of every drag callback the stroke passes through.
        See UndoHistory::setEditHeldOpen. */
    std::function<void (bool)> onStrokeActive;

    //==========================================================================
    // Called by StepSlot, which receives the mouse but does not decide what it means: which
    // step a moving cursor is editing is the lane's business, since only the lane can see the
    // other fifteen.

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

    //--------------------------------------------------------------------------
    // The parameter column. Length and Mix amount are blocks -- a caption and a read-out over
    // a full-width control -- because neither is legible as a number on the end of a row:
    // Length is a position in sixteen, and Mix amount is signed.
    ParamBlock lengthBlock;
    ControlGroup rateGroup;
    ParamBlock mixBlock;

    // Abbreviated, unlike the settings footer's captions. These four sit in a 46px column
    // beside the steps, and the words they stand for do not fit it -- the tooltips carry the
    // full names.
    juce::TextButton layerButtons[numStepLayers] { juce::TextButton ("Val"),
                                                   juce::TextButton ("Vel"),
                                                   juce::TextButton ("Prob"),
                                                   juce::TextButton ("Gate") };

    // Abbreviated for the same reason: the action row has 190px to hold four controls, and
    // the two destructive ones are glyphs rather than words so they cannot be misread at a
    // glance as more of the same.
    juce::TextButton randomiseButton { "RND" }, clearButton { "CLR" };
    juce::TextButton menuButton { "More" }, removeButton { "Remove" };

    juce::Random random;

    // Which per-step parameter the sixteen bars edit. Per lane rather than global, so one
    // lane can be shown as accents while another is being dialled in for pitch.
    StepLayer currentLayer = StepLayer::value;

    void setLayer (StepLayer);

    /** Pushes the mute through to the slots and the lane's own accents. Tracks the last state
        it applied because Button::onStateChange also fires on hover, and repainting sixteen
        slots every time the mouse crosses the toggle is work for nothing. */
    void applyLaneState();

    int appliedLaneActive = -1;

    /** Greys the steps the lane's Length leaves out of the cycle, and moves the wrap mark. */
    void applyLength();

    int appliedLength = -1;

    // Set in resized(), drawn in paint().
    juce::Rectangle<int> wellArea;

    /** Where inside the well the cycle wraps back to step one, or -1 at full length, where
        there is nothing to mark. */
    int wrapX = -1;

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

        Nearest by centre rather than a hit test: the gaps between the slots are dead space a
        hit test would drop the stroke into, and the bar would stop following the cursor for
        the few pixels between one step and the next. Height plays no part either -- a stroke
        that wanders above or below the row keeps painting the step it is over, with only the
        value it writes running out of range.
    */
    int slotIndexForStroke (float xInLane) const;

    /** Moves the stroke onto another step, releasing the one it is leaving where it stands
        and opening the new one at the given point. */
    void handStrokeTo (StepSlot& slot, const juce::MouseEvent& atPoint);

    /** Where a stroke that travelled from one point to the other crossed the given step: that
        step's centre, at the height the line between the two had reached by then. */
    juce::Point<float> pointCrossingSlot (int index, juce::Point<float> from,
                                          juce::Point<float> to) const;

    void showActionsMenu();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LaneComponent)
};
