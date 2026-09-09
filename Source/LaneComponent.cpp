#include "LaneComponent.h"

namespace
{
    /** What each layer's bar edits. Out here rather than in the constructor's table because
        the slot's tooltip asks for it again every time the mouse rests on a bar, and the
        layer under the bars changes without the slot being rebuilt. */
    const char* layerTooltip (StepLayer layer) noexcept
    {
        switch (layer)
        {
            case StepLayer::velocity: return "This step's accent, as a trim on the global Velocity";
            case StepLayer::chance:   return "Probability this step fires";
            case StepLayer::gate:     return "How long this step's note is held, as % of the step";
            case StepLayer::spread:   return "How far above its own value this step may pick "
                                             "a pitch";
            case StepLayer::value:
            default:                  return "Step value -- drives pitch";
        }
    }

    /** The endpoints a value and a width come out as, for the read-outs.

        The parameter is a width so that it can be a row of its own -- sixteen bars, its own
        RND and CLR -- but a range is read as two ends, so every place a number is shown says
        it that way. Clamped exactly as the engine clamps its draw, so the read-out never
        promises a pitch that cannot be played.
    */
    juce::String spreadRangeText (float value, float spread)
    {
        const auto percent = [] (float v) { return juce::String (juce::roundToInt (v * 100.0f)); };

        const float low  = juce::jlimit (0.0f, 1.0f, value);
        const float high = juce::jmin (1.0f, low + spread);

        return percent (low) + "-" + percent (high) + "%";
    }

}

namespace
{
    /** How long a slide takes, and how it is paced.

        Short enough to feel like a state change rather than an animation you wait through --
        the point is to show that the bars moved rather than were replaced, and that reads in
        well under a fifth of a second. Eased out, so it leaves immediately on the click and
        settles into the new heights rather than arriving at speed.
    */
    constexpr double valueSlideMs = 140.0;

    float easeOut (float t) noexcept
    {
        const float inverse = 1.0f - t;
        return 1.0f - inverse * inverse * inverse;
    }
}

//==============================================================================
int lane::height()
{
    const int ruledBlock = ParamBlock::preferredHeight() + 1;
    const int ruledRow   = theme::paramRowHeight;

    // Length, Rate, Direction, Mix amount, then the action chips pushed to the foot. Both
    // kinds of lane carry all four, so there is one height rather than one per kind: a CC
    // lane's steps traverse exactly as a Note lane's do.
    const int paramHeight = ruledBlock + ruledRow + ruledRow + ruledBlock
                              + actionGap + actionHeight;

    return padTop + juce::jmax (wellHeight, paramHeight) + padBottom + separatorHeight;
}

//==============================================================================
StepSlot::StepSlot (juce::AudioProcessorValueTreeState& state, int laneIndex, int stepIndex,
                    params::LaneKind kind)
    : step (stepIndex), accent (theme::laneAccent (laneIndex))
{
    struct LayerSetup
    {
        juce::Slider& slider;
        juce::String  paramID;
        double        resetTo;
    };

    // Velocity, Gate and Spread are note-only parameters -- a CC lane's step has none of them
    // (see Parameters.cpp) -- so for a CC-kind slot only Value and Chance get built at all.
    //
    // Both the id and the double-click reset come from params, so a bar resets to exactly what
    // the lane's Clear puts that row back to.
    const LayerSetup setups[]
    {
        { valueSlider,    params::stepLayerId (laneIndex, stepIndex, StepLayer::value, kind),
                          params::stepLayerNeutral (StepLayer::value) },
        { spreadSlider,   params::stepLayerId (laneIndex, stepIndex, StepLayer::spread, kind),
                          params::stepLayerNeutral (StepLayer::spread) },
        { velocitySlider, params::stepLayerId (laneIndex, stepIndex, StepLayer::velocity, kind),
                          params::stepLayerNeutral (StepLayer::velocity) },
        { chanceSlider,   params::stepLayerId (laneIndex, stepIndex, StepLayer::chance, kind),
                          params::stepLayerNeutral (StepLayer::chance) },
        { gateSlider,     params::stepLayerId (laneIndex, stepIndex, StepLayer::gate, kind),
                          params::stepLayerNeutral (StepLayer::gate) },
    };

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>* attachments[]
        { &valueAttachment, &spreadAttachment, &velocityAttachment, &chanceAttachment,
          &gateAttachment };

    for (int i = 0; i < numStepLayers; ++i)
    {
        // Empty for a CC lane's velocity, gate and spread, which do not exist -- skipped
        // rather than attached to the note lane of the same number those ids would otherwise
        // resolve to.
        if (setups[i].paramID.isEmpty())
            continue;

        built[i] = true;

        auto& slider = setups[i].slider;

        slider.setSliderStyle (juce::Slider::LinearBarVertical);
        slider.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
        slider.setColour (juce::Slider::backgroundColourId, theme::stepGround);

        // On drag only. The hover half of it went with the slider's mouse handling, since a
        // component that is never under the mouse is never hovered; the slot's own tooltip
        // carries the value instead. See getTooltip.
        //
        // Which component the bubble hangs off is settled in parentHierarchyChanged(), not
        // here: a slot has no parent yet while it is being constructed.
        slider.setPopupDisplayEnabled (true, false, nullptr);
        slider.setDoubleClickReturnValue (true, setups[i].resetTo);

        // Every bar stays an absolute drag whatever modifier is held. JUCE reads Ctrl, Alt or
        // Cmd as "swap to velocity-sensitive drag" by default (see Slider's
        // isAbsoluteDragMode), which hides the cursor and turns the stroke into a relative
        // nudge -- so a modified drag across the step grid did almost nothing and looked
        // broken. It is also what the Pitch row's Alt-drag needs: that gesture reaches this
        // slot's Spread bar through the same absolute path an ordinary stroke takes, and
        // there is nothing here a velocity mode would be good for.
        slider.setVelocityModeParameters (1.0, 1, 0.0, false);

        // The slot takes the mouse for every bar and passes each event on to whichever one
        // the stroke has reached -- see this class's own comment. A bar that took its own
        // mouse-down would hold the rest of the drag whatever the cursor went on to do.
        slider.setInterceptsMouseClicks (false, false);

        theme::setRole (slider, theme::Role::stepBar);
        addChildComponent (slider);

        *attachments[i] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
            state, setups[i].paramID, slider);
    }

    // The window is drawn by the value bar, so the value bar has to be told when it changes.
    // Through onValueChange rather than by polling: the attachment drives the slider through
    // Slider::Listener, which leaves this callback free and fires it for a change arriving
    // from the host exactly as readily as for one from a drag.
    if (built[(int) StepLayer::spread])
    {
        spreadSlider.onValueChange = [this] { applySpread(); };
        applySpread();
    }

    // The bars are deaf to the mouse and the slot takes the gesture for all of them (see
    // this class's own comment), so the drag cursor belongs here rather than on the bar the
    // role would otherwise put it on. The trig is a real button and sets its own.
    setMouseCursor (juce::MouseCursor::UpDownResizeCursor);

    onButton.setColour (juce::ToggleButton::tickColourId, accent);
    onButton.setTooltip ("Mute or unmute this step");
    theme::setRole (onButton, theme::Role::stepTrig);
    addAndMakeVisible (onButton);

    onAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        state, params::stepOnId (laneIndex, stepIndex, kind), onButton);

    // ButtonAttachment listens through addListener rather than through either callback, so
    // onStateChange is free for the lane's own use.
    //
    // Guarded on the toggle actually having moved: onStateChange fires for every state the
    // button passes through, mouse-over and mouse-down included, so an unguarded hover ran the
    // whole colour pass and two repaints to arrive back at the colours already on screen.
    onButton.onStateChange = [this]
    {
        if (onButton.getToggleState() != (appliedTrigOn == 1))
            applyTrigState();
    };

    setLayer (StepLayer::value);
    applyTrigState();
}

juce::Slider& StepSlot::sliderFor (StepLayer layer) noexcept
{
    switch (layer)
    {
        case StepLayer::spread:   return spreadSlider;
        case StepLayer::velocity: return velocitySlider;
        case StepLayer::chance:   return chanceSlider;
        case StepLayer::gate:     return gateSlider;
        case StepLayer::value:
        default:                  return valueSlider;
    }
}

void StepSlot::applySpread()
{
    // On the value bar rather than on the one that owns the number: the window is headroom
    // above that bar, and only legible against it. See theme::stepSpreadProperty.
    theme::setStepSpread (valueSlider, (float) spreadSlider.getValue());
    valueSlider.repaint();
}

void StepSlot::setLandedValue (float proportion)
{
    if (juce::approximatelyEqual (theme::stepLandedOf (valueSlider), proportion))
        return;

    theme::setStepLanded (valueSlider, proportion);
    valueSlider.repaint();
}

void StepSlot::setLayer (StepLayer layer)
{
    // The bar being hidden must not keep an override from a slide it was part of, or it would
    // come back at that height the next time this layer is selected.
    theme::clearDrawProportion (sliderFor (currentLayer));

    currentLayer = layer;

    for (int l = 0; l < numStepLayers; ++l)
        sliderFor ((StepLayer) l).setVisible ((StepLayer) l == layer);

    applyTrigState();
    repaint();
}

float StepSlot::proportionOf (const juce::Slider& slider)
{
    const auto range = slider.getRange();

    return range.getLength() > 0.0
             ? (float) juce::jlimit (0.0, 1.0, (slider.getValue() - range.getStart()) / range.getLength())
             : 0.0f;
}

float StepSlot::drawnProportion() const
{
    const float overridden = theme::drawProportionOf (const_cast<StepSlot*> (this)->sliderFor (currentLayer));

    return overridden >= 0.0f ? overridden
                              : proportionOf (const_cast<StepSlot*> (this)->sliderFor (currentLayer));
}

void StepSlot::beginValueSlide()
{
    transitionFrom = drawnProportion();
}

void StepSlot::setValueSlideProgress (float progress)
{
    if (transitionFrom < 0.0f)
        return;

    auto& bar = sliderFor (currentLayer);

    if (progress >= 1.0f)
    {
        transitionFrom = -1.0f;
        theme::clearDrawProportion (bar);
    }
    else
    {
        theme::setDrawProportion (bar, transitionFrom
                                         + (proportionOf (bar) - transitionFrom) * progress);
    }

    bar.repaint();
}

void StepSlot::setLaneActive (bool laneIsActive)
{
    if (laneActive == laneIsActive)
        return;

    laneActive = laneIsActive;
    applySlotAlpha();
}

void StepSlot::setWithinLength (bool isWithinLength)
{
    if (withinLength == isWithinLength)
        return;

    withinLength = isWithinLength;
    applySlotAlpha();
}

void StepSlot::applySlotAlpha()
{
    // Both states fade the whole slot -- bar, trig and number together -- rather than being
    // mixed into each colour separately, and they multiply: a step past the end of a muted
    // lane is fainter than either on its own. That is what keeps the three readings ("off",
    // "never runs", "lane is muted") distinguishable instead of collapsing into one grey.
    //
    // On the slot rather than on its children because the number is painted by the slot
    // itself, and a child-by-child fade would leave it at full strength.
    setAlpha ((laneActive ? 1.0f : 0.34f) * (withinLength ? 1.0f : 0.3f));
    repaint();
}

void StepSlot::parentHierarchyChanged()
{
    // The value bubble is parented to the editor rather than left to JUCE's default, which is
    // the desktop: a desktop-level window from inside a plugin editor can surface behind the
    // host, or on whichever monitor the host is not on. The editor rather than this slot's own
    // parent, so the bubble is not clipped to the lane strip it came from.
    auto* host = findParentComponentOfClass<juce::AudioProcessorEditor>();

    if (host == nullptr)
        return;

    for (int layer = 0; layer < numStepLayers; ++layer)
        sliderFor ((StepLayer) layer).setPopupDisplayEnabled (true, false, host);
}

void StepSlot::applyTrigState()
{
    const bool on = onButton.getToggleState();

    appliedTrigOn = on ? 1 : 0;

    auto& visible = sliderFor (currentLayer);

    // Only on/off and the playhead are mixed into the colours here. Muting and running past
    // the lane's Length fade the whole slot instead -- see applySlotAlpha -- so this does not
    // have to know about either.
    const float alpha = ! on    ? 0.16f
                      : playing ? 1.0f
                                : 0.82f;

    visible.setColour (juce::Slider::trackColourId,
                       (playing && on ? accent.brighter (0.28f) : accent).withAlpha (alpha));

    visible.setColour (juce::Slider::backgroundColourId,
                       on ? theme::stepGround : theme::stepGroundOff);

    // Drawn as a hairline round the bar by the LookAndFeel. The ground alone cannot carry the
    // off state on a step whose value is zero, because there is no fill above it to contrast
    // against.
    theme::setStepOff (visible, ! on);
    visible.repaint();

    onButton.setColour (juce::ToggleButton::tickColourId, accent);
    onButton.repaint();

    repaint();
}

juce::Rectangle<int> StepSlot::barArea() const
{
    return getLocalBounds().withTrimmedBottom (lane::numberHeight + lane::stepInnerGap
                                                 + lane::trigHeight + lane::stepInnerGap);
}

bool StepSlot::barContains (juce::Point<int> positionInSlot) const
{
    return barArea().contains (positionInSlot);
}

juce::Slider* StepSlot::activeBar() noexcept
{
    // Visibility is what tells the two apart: setLayer shows exactly the one bar the slot is
    // editing, and on a CC slot the layers that were never built are never shown.
    auto& bar = sliderFor (currentLayer);
    return bar.isVisible() ? &bar : nullptr;
}

juce::String StepSlot::getTooltip()
{
    auto* bar = activeBar();

    if (bar == nullptr)
        return layerTooltip (currentLayer);

    auto text = bar->getTextFromValue (bar->getValue()) + " -- " + layerTooltip (currentLayer);

    // Both rows that describe one pitch say it in endpoints, since that is how a range is
    // thought of even though the parameter behind it is a width. On the Pitch row the hint
    // for the gesture goes with it: an Alt-drag is only discoverable where it works, and it
    // works here.
    if (const float spread = (float) spreadSlider.getValue();
        built[(int) StepLayer::spread] && spread > 0.001f)
    {
        if (currentLayer == StepLayer::value)
            text += " (plays " + spreadRangeText ((float) valueSlider.getValue(), spread) + ")";
        else if (currentLayer == StepLayer::spread)
            text += " (" + spreadRangeText ((float) valueSlider.getValue(), spread) + ")";
    }

    if (currentLayer == StepLayer::value && built[(int) StepLayer::spread])
        text += ". Alt-drag to widen its Spread";

    return text;
}

//==============================================================================
void StepSlot::beginBarDrag (const juce::MouseEvent& e, StepLayer layer)
{
    // A bar the slot does not have leaves strokeBar null and the stroke inert, which is what
    // an Alt-drag on a CC lane comes to: there is no Spread there to widen.
    strokeBar = built[(int) layer] ? &sliderFor (layer) : nullptr;

    if (strokeBar != nullptr)
        strokeBar->mouseDown (e.getEventRelativeTo (strokeBar));
}

void StepSlot::continueBarDrag (const juce::MouseEvent& e)
{
    if (strokeBar != nullptr)
        strokeBar->mouseDrag (e.getEventRelativeTo (strokeBar));
}

void StepSlot::endBarDrag (const juce::MouseEvent& e)
{
    if (strokeBar == nullptr)
        return;

    strokeBar->mouseUp (e.getEventRelativeTo (strokeBar));
    strokeBar = nullptr;
}

//==============================================================================
void StepSlot::mouseDown (const juce::MouseEvent& e)
{
    // The trig strip is a button of its own and takes its own clicks, so the only part of the
    // slot that reaches here is the bar -- except for the few pixels of gap between the two,
    // which start nothing.
    if (! barContains (e.getPosition()))
        return;

    if (auto* owner = findParentComponentOfClass<LaneComponent>())
        owner->startStroke (*this, e);
}

void StepSlot::mouseDrag (const juce::MouseEvent& e)
{
    // Still this slot's event however far the cursor has gone: JUCE keeps a drag with the
    // component the button went down in. Which step it now belongs to is the lane's to say.
    if (auto* owner = findParentComponentOfClass<LaneComponent>())
        owner->continueStroke (e);
}

void StepSlot::mouseUp (const juce::MouseEvent& e)
{
    if (auto* owner = findParentComponentOfClass<LaneComponent>())
        owner->endStroke (e);
}

void StepSlot::mouseDoubleClick (const juce::MouseEvent& e)
{
    if (! barContains (e.getPosition()))
        return;

    // Reaches the bar directly rather than through the lane: a double click is one step's own
    // reset, and there is no stroke for it to be part of.
    if (auto* bar = activeBar())
        bar->mouseDoubleClick (e.getEventRelativeTo (bar));
}

void StepSlot::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
{
    // A Slider that makes no use of a wheel event hands it to its parent, which is this slot
    // -- so an unwanted wheel would come straight back and be offered to the bar again. The
    // second time through it goes on up instead, which is where the slider was sending it.
    if (forwardingWheel)
    {
        Component::mouseWheelMove (e, wheel);
        return;
    }

    if (auto* bar = activeBar())
    {
        const juce::ScopedValueSetter<bool> guard (forwardingWheel, true);
        bar->mouseWheelMove (e.getEventRelativeTo (bar), wheel);
    }
}

void StepSlot::paint (juce::Graphics& g)
{
    // The step's own number, under the trig. Drawn by the slot rather than by the lane so it
    // moves with the slot and can pick up the playhead without the lane having to repaint a
    // strip of sixteen labels.
    auto numberArea = getLocalBounds().removeFromBottom (lane::numberHeight);

    g.setFont (theme::stepNumFont());
    g.setColour (playing ? accent : theme::stepNumber);
    g.drawText (juce::String (step + 1), numberArea, juce::Justification::centred, false);
}

void StepSlot::paintOverChildren (juce::Graphics& g)
{
    if (! playing)
        return;

    // Over the children rather than in paint(), because the bar fills this rectangle and would
    // cover anything painted underneath it.
    //
    // A ring rather than a fill, so the playhead never hides the value it is standing on. A
    // muted lane keeps its playhead -- it is still running, and unmuting it mid-bar should not
    // be a surprise.
    g.setColour (accent.withAlpha (0.55f));
    g.drawRoundedRectangle (barArea().toFloat().reduced (0.5f), 1.5f, 1.0f);
}

void StepSlot::resized()
{
    auto r = getLocalBounds();

    r.removeFromBottom (lane::numberHeight);
    r.removeFromBottom (lane::stepInnerGap);

    onButton.setBounds (r.removeFromBottom (lane::trigHeight));
    r.removeFromBottom (lane::stepInnerGap);

    // Every bar gets the same rectangle whether or not it is the visible one: the Spread bar
    // is dragged while it is hidden, and a slider maps a drag onto its value through its own
    // bounds.
    valueSlider.setBounds (r);
    spreadSlider.setBounds (r);
    velocitySlider.setBounds (r);
    chanceSlider.setBounds (r);
    gateSlider.setBounds (r);
}

void StepSlot::setPlaying (bool shouldBePlaying)
{
    if (playing == shouldBePlaying)
        return;

    playing = shouldBePlaying;

    // Only the step under the playhead carries a landed mark, so the one being left drops it
    // here rather than waiting to be told: the lane only ever pushes a position to the step
    // that has one.
    if (! playing)
        setLandedValue (-1.0f);

    // The playhead brightens the bar's fill as well as adding a ring, so the colours have to
    // be rebuilt rather than only repainted.
    applyTrigState();
}

//==============================================================================
LaneComponent::LaneComponent (juce::AudioProcessorValueTreeState& state, int laneIndex,
                              params::LanePattern& sharedClipboard, params::LaneKind kind)
    : apvts (state), lane (laneIndex), kind (kind), accent (theme::laneAccent (laneIndex)),
      clipboard (sharedClipboard),
      lengthBlock (state, params::laneLengthId (laneIndex, kind), "Length",
                   theme::Role::lengthBar, accent),
      rateGroup (state),
      mixBlock (state, params::laneDepthId (laneIndex, kind), "Mix amount",
                theme::Role::bipolarBar, accent)
{
    onButton.setColour (juce::ToggleButton::tickColourId, accent);
    onButton.setTooltip ("Mute or unmute this lane");
    addAndMakeVisible (onButton);

    onAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        state, params::laneOnId (laneIndex, kind), onButton);

    // As in StepSlot: the attachment listens through addListener, so onStateChange is ours.
    onButton.onStateChange = [this] { applyLaneState(); };

    for (int step = 0; step < params::numSteps; ++step)
        addAndMakeVisible (slots.add (new StepSlot (state, laneIndex, step, kind)));

    //--------------------------------------------------------------------------
    // One column, not two. Four parameters in 190px read as a list; the same four in two
    // columns of 95px read as a table with nothing in the cells, and neither Length nor Mix
    // amount fits beside its own caption at that width anyway.
    lengthBlock.setTooltip ("How many of the sixteen steps the lane cycles through");
    theme::setRuled (lengthBlock, true);
    addAndMakeVisible (lengthBlock);

    rateGroup.setRowHeight (theme::paramRowHeight);
    rateGroup.add (params::laneDivId (laneIndex, kind), "Rate");

    // Both kinds: a CC lane's steps traverse the same way a Note lane's do, and the engine
    // reads the parameter for both (see PluginProcessor's lane snapshot).
    rateGroup.add (params::laneDirId (laneIndex, kind), "Direction");

    addAndMakeVisible (rateGroup);

    theme::setRuled (mixBlock, true);
    addAndMakeVisible (mixBlock);

    // The rows inside rateGroup take the same rule the two blocks either side of them do, so
    // the column reads as four separated parameters rather than as two blocks with a gap.
    for (auto* child : rateGroup.getChildren())
        theme::setRuled (*child, true);

    //--------------------------------------------------------------------------
    theme::setRole (randomiseButton, theme::Role::laneAction);
    randomiseButton.onClick = [this, kind]
    {
        slideThrough ([this, kind]
                      { params::randomiseLaneRow (apvts, lane, random, kind, currentLayer); });
    };
    addAndMakeVisible (randomiseButton);

    theme::setRole (clearButton, theme::Role::laneAction);
    clearButton.onClick = [this, kind]
    {
        slideThrough ([this, kind]
                      { params::clearLaneRow (apvts, lane, kind, currentLayer); });
    };
    addAndMakeVisible (clearButton);

    theme::setRole (menuButton, theme::Role::laneMenu);
    menuButton.onClick = [this] { showActionsMenu(); };
    addAndMakeVisible (menuButton);

    removeButton.setTooltip ("Remove this lane. The lanes below it move up to close the gap, "
                             "and this one's pattern goes with it -- Ctrl+Z brings it back");
    theme::setRole (removeButton, theme::Role::laneRemove);
    removeButton.onClick = [this] { if (onRemove != nullptr) onRemove(); };

    // Added hidden: the editor turns it on for every lane once there is more than one.
    addChildComponent (removeButton);

    //--------------------------------------------------------------------------
    // A CC lane builds the selector too, but only its first chip. Velocity and Gate are only
    // ever arguments to starting a note and a CC lane never starts one, so there is genuinely
    // nothing for the other three to select -- but a blank column left the CC tab's steps
    // looking like a different kind of grid from the Notes tab's, when they are the same grid
    // with fewer layers behind it. One latched chip says "Value, and that is all there is"
    // where an empty column said nothing at all.
    const int builtLayers = kind == params::LaneKind::note ? numStepLayers : 1;

    static const char* layerTooltips[]
    {
        "the bars edit each step's value",
        "the bars edit how far above its own value each step may pick a pitch",
        "the bars edit each step's accent",
        "the bars edit each step's chance of firing",
        "the bars edit how long each step's note is held",
    };

    for (int i = 0; i < numStepLayers; ++i)
    {
        auto& button = layerButtons[i];
        const auto layer = (StepLayer) i;

        button.setButtonText (params::stepLayerShortName (layer, kind));

        if (i >= builtLayers)
        {
            button.setVisible (false);
            continue;
        }

        button.setTooltip (params::stepLayerName (layer, kind) + ": " + layerTooltips[i]);
        button.setClickingTogglesState (false);
        theme::setRole (button, theme::Role::layerChip);
        theme::setAccent (button, accent);
        button.onClick = [this, i] { setLayer ((StepLayer) i); };
        addAndMakeVisible (button);
    }

    // The attachment drives the slider through Slider::Listener, the same way the step trigs'
    // does, so onValueChange is free for the lane's own use -- and it fires for a change from
    // the host as readily as for a drag.
    lengthBlock.getSlider().onValueChange = [this] { applyLength(); lengthBlock.repaint(); };

    setLayer (StepLayer::value);
    applyLaneState();
    applyLength();
}

LaneComponent::~LaneComponent()
{
    // A lane cannot normally go while a stroke is running -- the button that removes it is not
    // reachable with a step bar holding the mouse -- but an undo step left held open would
    // quietly swallow every edit made after it into the same step.
    if (strokeSlot != nullptr && onStrokeActive != nullptr)
        onStrokeActive (false);
}

//==============================================================================
void LaneComponent::startStroke (StepSlot& slot, const juce::MouseEvent& e)
{
    strokeSlot = &slot;
    strokePosition = e.getEventRelativeTo (this).position;

    if (onStrokeActive != nullptr)
        onStrokeActive (true);

    // Alt opens the stroke on Spread instead of the row on screen -- but only from the Pitch
    // row, which is the one that draws the window. There the gesture is an edit to what is
    // already under the cursor, and leaving the row to reach the chip and coming back to see
    // the result is three actions for one thought. From Vel, Prob or Gate it would be a
    // modifier that silently rewrites a row you cannot see, which is a trap rather than a
    // shortcut.
    //
    // Sampled once, here, so the stroke keeps writing the row it opened on however the
    // modifier is held for the rest of it.
    strokeLayer = e.mods.isAltDown() && currentLayer == StepLayer::value ? StepLayer::spread
                                                                        : currentLayer;

    slot.beginBarDrag (e, strokeLayer);
}

void LaneComponent::continueStroke (const juce::MouseEvent& e)
{
    if (strokeSlot == nullptr)
        return;

    const auto laneEvent = e.getEventRelativeTo (this);
    const auto to = laneEvent.position;

    const int target = slotIndexForStroke (to.x);
    int index = slots.indexOf (strokeSlot);

    if (target < 0 || index < 0)
        return;

    const int direction = target > index ? 1 : -1;

    // Every step between the last position and this one, not only the one the cursor has
    // landed on. A drag reports positions a frame apart, and at any speed worth calling a
    // swipe those are further apart than a step is wide -- so without this a quick stroke
    // would paint every third step and leave the rest exactly as it found them, which is the
    // one thing the stroke exists to avoid.
    while (index != target)
    {
        index += direction;

        // Each is taken at the height the cursor had as it crossed that step rather than at
        // the height it has ended up at, so a swipe drawn as a diagonal comes out as a ramp
        // instead of as a row of equal bars.
        const auto crossing = pointCrossingSlot (index, strokePosition, to);

        handStrokeTo (*slots.getUnchecked (index), laneEvent.withNewPosition (crossing));
    }

    strokeSlot->continueBarDrag (laneEvent);
    strokePosition = to;
}

void LaneComponent::handStrokeTo (StepSlot& slot, const juce::MouseEvent& atPoint)
{
    // The step being left keeps whatever it was last dragged to: it is released where it
    // stands rather than reverted, which is the whole point of the stroke. Ending its drag
    // also closes its host gesture, so each step remains its own edit as far as the host is
    // concerned -- only the undo history joins them, and only because the editor holds the
    // step open for the length of the stroke. See onStrokeActive.
    strokeSlot->endBarDrag (atPoint);
    strokeSlot = &slot;
    strokeSlot->beginBarDrag (atPoint, strokeLayer);
}

void LaneComponent::endStroke (const juce::MouseEvent& e)
{
    if (strokeSlot == nullptr)
        return;

    strokeSlot->endBarDrag (e);
    strokeSlot = nullptr;

    if (onStrokeActive != nullptr)
        onStrokeActive (false);
}

juce::Point<float> LaneComponent::pointCrossingSlot (int index, juce::Point<float> from,
                                                     juce::Point<float> to) const
{
    const auto x = (float) slots.getUnchecked (index)->getBounds().getCentreX();

    // How far along the cursor's travel this step sits, so the height can be read off the line
    // between the two reported positions. A stroke that only moved vertically never reaches
    // here, but the division is guarded all the same.
    const float span = to.x - from.x;
    const float t = std::abs (span) > 0.001f ? juce::jlimit (0.0f, 1.0f, (x - from.x) / span)
                                             : 1.0f;

    return { x, from.y + t * (to.y - from.y) };
}

int LaneComponent::slotIndexForStroke (float xInLane) const
{
    int nearest = -1;
    float nearestDistance = 0.0f;

    for (int i = 0; i < slots.size(); ++i)
    {
        const float distance = std::abs ((float) slots.getUnchecked (i)->getBounds().getCentreX()
                                             - xInLane);

        if (nearest < 0 || distance < nearestDistance)
        {
            nearest = i;
            nearestDistance = distance;
        }
    }

    return nearest;
}

//==============================================================================
void LaneComponent::applyLength()
{
    const int length = juce::jlimit (1, params::numSteps,
                                     (int) std::lround (lengthBlock.getSlider().getValue()));

    if (appliedLength == length)
        return;

    appliedLength = length;

    for (int i = 0; i < slots.size(); ++i)
        slots.getUnchecked (i)->setWithinLength (i < length);

    // The wrap mark sits in the gap after the last step in the cycle, not on it -- it marks
    // where the lane returns to step one, which is a boundary rather than a step.
    if (length >= params::numSteps || slots.isEmpty())
        wrapX = -1;
    else
        wrapX = slots.getUnchecked (length - 1)->getRight() + lane::stepGap / 2;

    repaint();
}

void LaneComponent::applyLaneState()
{
    const int active = onButton.getToggleState() ? 1 : 0;

    if (appliedLaneActive == active)
        return;

    appliedLaneActive = active;

    for (auto* slot : slots)
        slot->setLaneActive (active != 0);

    // The parameter column and the layer selector recede with the lane, so a muted lane reads
    // as one dimmed block rather than as live controls beside dead steps. The steps dim
    // themselves, above, because they have three states to express and this has one.
    const float alpha = active != 0 ? 1.0f : 0.34f;

    lengthBlock.setAlpha (alpha);
    rateGroup.setAlpha (alpha);
    mixBlock.setAlpha (alpha);

    for (auto& button : layerButtons)
        button.setAlpha (alpha);

    // The accent rail is painted here, not by a child.
    repaint();
}

void LaneComponent::updateActionTooltips()
{
    // RND, CLR and the menu all act on the selected row, and which row that is has to be
    // readable from the button rather than inferred from what happens after pressing it.
    const auto row = params::stepLayerName (currentLayer, kind);

    randomiseButton.setTooltip ("Randomize this lane's sixteen " + row + " steps");
    clearButton.setTooltip ("Put this lane's sixteen " + row + " steps back to their default");
    menuButton.setTooltip ("Rotate or copy the whole pattern, or invert its " + row + " row");
}

void LaneComponent::setLayer (StepLayer layer)
{
    // False on the constructor's own call, which sets the layer the lane opens on -- there is
    // nothing on screen yet to slide away from.
    const bool changed = layer != currentLayer;

    // Captured before the layer changes under them, because each slot records where its bar is
    // drawn *now* -- including mid-slide, if the selector is clicked twice in quick
    // succession.
    if (changed)
        for (auto* slot : slots)
            slot->beginValueSlide();

    currentLayer = layer;

    for (int i = 0; i < numStepLayers; ++i)
        layerButtons[i].setToggleState (i == (int) layer, juce::dontSendNotification);

    for (auto* slot : slots)
        slot->setLayer (layer);

    updateActionTooltips();

    if (changed)
        startValueSlide();
}

void LaneComponent::startValueSlide()
{
    valueSlideStartMs = juce::Time::getMillisecondCounterHiRes();

    // Applied at zero straight away rather than left to the first timer callback, so the new
    // heights never show for the frame between the click and that callback.
    applyValueSlide (0.0f);

    startTimerHz (60);
}

void LaneComponent::slideThrough (const std::function<void()>& edit)
{
    // Before the edit, because writing the parameters moves the sliders under us: the
    // attachments apply a message-thread parameter change synchronously, so by the time edit()
    // returns the bars already hold their new heights and there is nothing left to capture.
    for (auto* slot : slots)
        slot->beginValueSlide();

    edit();

    startValueSlide();
}

void LaneComponent::applyValueSlide (float progress)
{
    const float eased = progress >= 1.0f ? 1.0f : easeOut (progress);

    for (auto* slot : slots)
        slot->setValueSlideProgress (eased);
}

void LaneComponent::timerCallback()
{
    const double elapsed = juce::Time::getMillisecondCounterHiRes() - valueSlideStartMs;
    const float progress = (float) juce::jlimit (0.0, 1.0, elapsed / valueSlideMs);

    applyValueSlide (progress);

    if (progress >= 1.0f)
        stopTimer();
}

//==============================================================================
void LaneComponent::showActionsMenu()
{
    juce::PopupMenu menu;
    menu.setLookAndFeel (&getLookAndFeel());

    menu.addItem (1, "Rotate left");
    menu.addItem (2, "Rotate right");
    menu.addItem (3, "Invert " + params::stepLayerName (currentLayer, kind));
    menu.addSeparator();
    menu.addItem (4, "Copy pattern");
    menu.addItem (5, "Paste pattern", clipboard.valid);

    // The callback fires after this component could have been torn down.
    const juce::Component::SafePointer<LaneComponent> safeThis (this);

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&menuButton),
                        [safeThis] (int result)
                        {
                            if (safeThis == nullptr)
                                return;

                            auto& state = safeThis->apvts;
                            const int laneIndex = safeThis->lane;
                            const auto laneKind = safeThis->kind;

                            // Copy is the one entry that changes nothing on screen, so it is
                            // the one that does not slide.
                            if (result == 4)
                            {
                                safeThis->clipboard = params::copyLane (state, laneIndex, laneKind);
                                return;
                            }

                            safeThis->slideThrough ([&]
                            {
                                switch (result)
                                {
                                    case 1: params::rotateLane (state, laneIndex, -1, laneKind); break;
                                    case 2: params::rotateLane (state, laneIndex, 1, laneKind); break;
                                    case 3: params::invertLaneRow (state, laneIndex, laneKind,
                                                                   safeThis->currentLayer); break;
                                    case 5: params::pasteLane (state, laneIndex, safeThis->clipboard, laneKind); break;
                                    default: break;
                                }
                            });
                        });
}

//==============================================================================
void LaneComponent::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds();

    // The accent edge: hard against the lane's left, full height, no inset. It is the lane's
    // name as much as the number beside it is -- the same colour its steps and its footer
    // column carry -- so it runs the whole strip rather than floating inside a margin.
    g.setColour (appliedLaneActive == 0 ? accent.withAlpha (0.3f) : accent);
    g.fillRect (bounds.withWidth (lane::railWidth)
                      .withTrimmedBottom (lane::separatorHeight));

    // The step area is cut into the window rather than raised off it: it is the one part of a
    // lane you draw *into*, and the sunken ground is what says so.
    const float wellAlpha = appliedLaneActive == 0 ? 0.5f : 1.0f;

    g.setColour (theme::well.interpolatedWith (theme::surface, 1.0f - wellAlpha));
    g.fillRoundedRectangle (wellArea.toFloat(), 2.0f);

    g.setColour (theme::outlineSoft.withMultipliedAlpha (wellAlpha));
    g.drawRoundedRectangle (wellArea.toFloat().reduced (0.5f), 2.0f, 1.0f);

    // Where the cycle returns to step one. Inside the well and clear of its padding, so it
    // reads as a mark on the grid rather than as an edge of it.
    if (wrapX > 0)
    {
        g.setColour (theme::wrapLine);
        g.fillRect (wrapX, wellArea.getY() + 6, 1, wellArea.getHeight() - 12);
    }

    // The hairline between this lane and the next. Lanes are rows of one list, not cards --
    // the accent edges already separate them, and a border round each one would put four
    // rectangles on screen competing with the step grid inside them.
    g.setColour (theme::outlineSoft);
    g.fillRect (bounds.removeFromBottom (lane::separatorHeight));
}

void LaneComponent::resized()
{
    auto r = getLocalBounds().withTrimmedBottom (lane::separatorHeight);

    r.removeFromTop (lane::padTop);
    r.removeFromBottom (lane::padBottom);
    r.removeFromRight (lane::padRight);

    //--------------------------------------------------------------------------
    // The mute is the only thing left of the steps now, so it is centred in the whole run from
    // the accent rail to the layer chips -- its own column plus the gap after it -- rather
    // than parked at the left of a column sized for a lane number that is no longer drawn.
    // Measured off the rail rather than off x=0 so the margin either side is the space the eye
    // actually sees, not the space the rail is sitting in.
    auto slotColumn = r.removeFromLeft (lane::slotWidth + lane::columnGap)
                       .withTrimmedLeft (lane::railWidth);

    // Level with the first layer chip: the lane's own switch and the switch for what its bars
    // show belong on the same line.
    onButton.setBounds (slotColumn.removeFromTop (lane::layerChipHeight)
                                  .withSizeKeepingCentre (lane::muteSize, lane::muteSize));

    //--------------------------------------------------------------------------
    auto selectorColumn = r.removeFromLeft (lane::selectorWidth);
    r.removeFromLeft (lane::columnGap);

    for (auto& button : layerButtons)
    {
        // Skipped entirely on a CC lane, where the whole selector is hidden but its column is
        // still reserved.
        if (! button.isVisible())
            continue;

        button.setBounds (selectorColumn.removeFromTop (lane::layerChipHeight));
        selectorColumn.removeFromTop (lane::layerChipGap);
    }

    //--------------------------------------------------------------------------
    auto paramBlock = r.removeFromRight (lane::paramWidth);
    r.removeFromRight (lane::columnGap);

    //--------------------------------------------------------------------------
    // The step area takes what is left, top-aligned: a Note lane's parameter column is taller
    // than the well, and the two are read across rather than as one centred block.
    wellArea = r.withHeight (lane::wellHeight);

    auto steps = wellArea.reduced (lane::wellPadX, 0)
                         .withTrimmedTop (lane::wellPadTop)
                         .withTrimmedBottom (lane::wellPadBottom);

    // A fixed pitch rather than a share of what is left over, so a step is the same width
    // whatever zoom the window is at -- the window itself is sized from this. Clamped only so
    // a host that forces the editor narrower than its native size still lays out.
    const int slotWidth = juce::jmin (lane::stepBarWidth,
                                      (steps.getWidth() - (params::numSteps - 1) * lane::stepGap)
                                          / params::numSteps);

    for (int i = 0; i < slots.size(); ++i)
    {
        slots.getUnchecked (i)->setBounds (steps.removeFromLeft (slotWidth));

        if (i + 1 < slots.size())
            steps.removeFromLeft (lane::stepGap);
    }

    // The wrap mark is derived from the slots' bounds, which have just moved -- so the
    // cached length is cleared to force applyLength() past its early-out and recompute it.
    appliedLength = -1;
    applyLength();

    //--------------------------------------------------------------------------
    // Length, Rate, [Direction] and Mix amount stack from the top; the action chips are
    // pushed to the foot. That separates the two by what they are -- settings that stay put,
    // and actions that rewrite the pattern under them -- instead of leaving the actions
    // looking like one more row of the block above.
    auto actionRow = paramBlock.removeFromBottom (lane::actionHeight);

    lengthBlock.setBounds (paramBlock.removeFromTop (ParamBlock::preferredHeight() + 1));

    // Rate and Direction, on both kinds of lane.
    rateGroup.setBounds (paramBlock.removeFromTop (2 * theme::paramRowHeight));

    mixBlock.setBounds (paramBlock.removeFromTop (ParamBlock::preferredHeight() + 1));

    //--------------------------------------------------------------------------
    // Hard right, a wide gap clear of the pattern chips. Removing a lane is the only action
    // here that a second click does not undo, so it does not sit where the hand passes on the
    // way to the ones that do.
    removeButton.setBounds (actionRow.removeFromRight (26));

    for (auto* button : { &randomiseButton, &clearButton })
    {
        button->setBounds (actionRow.removeFromLeft (theme::chipWidth (button->getButtonText())));
        actionRow.removeFromLeft (4);
    }

    menuButton.setBounds (actionRow.removeFromLeft (28));
}

void LaneComponent::setCanRemove (bool canBeRemoved)
{
    removeButton.setVisible (canBeRemoved);
}

void LaneComponent::setPlayingStep (int stepIndex)
{
    if (playingStep == stepIndex)
        return;

    playingStep = stepIndex;

    for (int i = 0; i < slots.size(); ++i)
        slots.getUnchecked (i)->setPlaying (i == stepIndex);
}

void LaneComponent::setPlayingValue (float proportion)
{
    // Only the step under the playhead is marked, so this needs no loop: every other slot
    // dropped its mark as the playhead left it. See StepSlot::setPlaying.
    if (playingStep >= 0 && playingStep < slots.size())
        slots.getUnchecked (playingStep)->setLandedValue (proportion);
}
