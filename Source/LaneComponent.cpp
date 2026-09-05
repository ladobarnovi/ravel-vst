#include "LaneComponent.h"

namespace
{
    constexpr int trigHeight   = 11;
    constexpr int trigGap      = 4;

    // Wide enough that the sixteen trig strips read as sixteen marks rather than as one line
    // ruled under the whole step area. Part of lane::stepSlotWidth, not extra to it: the
    // gap comes out of the slot, so the pitch from one step to the next stays exact.
    constexpr int slotGap      = 5;

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
            case StepLayer::value:
            default:                  return "Step value -- drives pitch";
        }
    }
}

//==============================================================================
StepSlot::StepSlot (juce::AudioProcessorValueTreeState& state, int laneIndex, int stepIndex,
                    params::LaneKind kind)
    : accent (theme::laneAccent (laneIndex))
{
    struct LayerSetup
    {
        juce::Slider& slider;
        juce::String  paramID;
        double        resetTo;
    };

    // Velocity and Gate are note-only parameters -- a CC lane's step has neither (see
    // Parameters.cpp) -- so for a CC-kind slot only Value and Chance get built at all.
    const bool isCc = kind == params::LaneKind::cc;

    const LayerSetup setups[]
    {
        { valueSlider,    params::stepValueId (laneIndex, stepIndex, kind),  0.0 },
        { velocitySlider, isCc ? juce::String() : params::stepVelocityId (laneIndex, stepIndex),  1.0 },
        { chanceSlider,   params::stepChanceId (laneIndex, stepIndex, kind), 1.0 },
        { gateSlider,     isCc ? juce::String() : params::stepGateId (laneIndex, stepIndex),      60.0 },
    };

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>* attachments[]
        { &valueAttachment, &velocityAttachment, &chanceAttachment, &gateAttachment };

    for (int i = 0; i < numStepLayers; ++i)
    {
        // Velocity (1) and Gate (3): skipped entirely for a CC lane, rather than attached
        // to the note lane of the same number that stepVelocityId/stepGateId would
        // otherwise silently resolve to.
        if (isCc && (i == (int) StepLayer::velocity || i == (int) StepLayer::gate))
            continue;

        auto& slider = setups[i].slider;

        slider.setSliderStyle (juce::Slider::LinearBarVertical);
        slider.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
        slider.setColour (juce::Slider::backgroundColourId, theme::track);

        // On drag only. The hover half of it went with the slider's mouse handling, since a
        // component that is never under the mouse is never hovered; the slot's own tooltip
        // carries the value instead. See getTooltip.
        slider.setPopupDisplayEnabled (true, false, getParentComponent());
        slider.setDoubleClickReturnValue (true, setups[i].resetTo);

        // The slot takes the mouse for all four bars and passes each event on to whichever
        // one the stroke has reached -- see this class's own comment. A bar that took its own
        // mouse-down would hold the rest of the drag whatever the cursor went on to do.
        slider.setInterceptsMouseClicks (false, false);

        theme::setRole (slider, theme::Role::stepBar);
        addChildComponent (slider);

        *attachments[i] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
            state, setups[i].paramID, slider);
    }

    onButton.setColour (juce::ToggleButton::tickColourId, accent);
    onButton.setTooltip ("Mute or unmute this step");
    onButton.setMouseCursor (juce::MouseCursor::PointingHandCursor);
    theme::setRole (onButton, theme::Role::stepTrig);
    addAndMakeVisible (onButton);

    onAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        state, params::stepOnId (laneIndex, stepIndex, kind), onButton);

    // ButtonAttachment listens through addListener rather than through either callback,
    // so onStateChange is free for the lane's own use.
    onButton.onStateChange = [this] { applyTrigState(); };

    setLayer (StepLayer::value);
    applyTrigState();
}

juce::Slider& StepSlot::sliderFor (StepLayer layer) noexcept
{
    switch (layer)
    {
        case StepLayer::velocity: return velocitySlider;
        case StepLayer::chance:   return chanceSlider;
        case StepLayer::gate:     return gateSlider;
        case StepLayer::value:
        default:                  return valueSlider;
    }
}

void StepSlot::setLayer (StepLayer layer)
{
    currentLayer = layer;

    for (auto l : { StepLayer::value, StepLayer::velocity, StepLayer::chance, StepLayer::gate })
        sliderFor (l).setVisible (l == layer);

    applyTrigState();
    repaint();
}

void StepSlot::setLaneActive (bool laneIsActive)
{
    if (laneActive == laneIsActive)
        return;

    laneActive = laneIsActive;
    applyTrigState();
    repaint();
}

void StepSlot::setWithinLength (bool isWithinLength)
{
    if (withinLength == isWithinLength)
        return;

    withinLength = isWithinLength;
    applyTrigState();
    repaint();
}

void StepSlot::applyTrigState()
{
    const bool on = onButton.getToggleState();

    // Either reason for the step never firing -- a muted lane, or a step the lane's Length
    // leaves out of the cycle -- lands on the same faint treatment.
    const bool live = laneActive && withinLength;

    auto& visible = sliderFor (currentLayer);

    // Three levels rather than two: an inert step sits below even an off step, so the
    // difference between "this step is off" and "this step never runs" stays readable.
    const float alpha = ! live ? 0.12f : (on ? 1.0f : 0.25f);

    visible.setColour (juce::Slider::trackColourId, accent.withAlpha (alpha));

    // The empty part of the bar carries the out-of-range state on its own, which is what
    // makes it visible on a step whose value is zero -- there is no fill there to dim.
    // Mixed toward theme::raised, which is the lane's own fill -- an out-of-range slot
    // has to fade into the surface it is actually drawn on. Mixing toward theme::surface (the
    // ground under the lane, not the lane) would overshoot past it and leave the slot
    // reading as a dark hole punched in the lane rather than as an absent step.
    visible.setColour (juce::Slider::backgroundColourId,
                       withinLength ? theme::track
                                    : theme::track.interpolatedWith (theme::raised, 0.85f));
    visible.repaint();

    // Mixed toward the lane rather than made transparent: drawStepTrig sets its own alpha
    // on whatever colour it finds here, so an alpha stored on this one would be discarded.
    onButton.setColour (juce::ToggleButton::tickColourId,
                        live ? accent : accent.interpolatedWith (theme::raised, 0.8f));
    onButton.repaint();
}

juce::Rectangle<int> StepSlot::barArea() const
{
    return getLocalBounds().withTrimmedBottom (trigHeight + trigGap);
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
    if (auto* bar = activeBar())
        return bar->getTextFromValue (bar->getValue()) + " -- " + layerTooltip (currentLayer);

    return layerTooltip (currentLayer);
}

//==============================================================================
void StepSlot::beginBarDrag (const juce::MouseEvent& e)
{
    if (auto* bar = activeBar())
        bar->mouseDown (e.getEventRelativeTo (bar));
}

void StepSlot::continueBarDrag (const juce::MouseEvent& e)
{
    if (auto* bar = activeBar())
        bar->mouseDrag (e.getEventRelativeTo (bar));
}

void StepSlot::endBarDrag (const juce::MouseEvent& e)
{
    if (auto* bar = activeBar())
        bar->mouseUp (e.getEventRelativeTo (bar));
}

//==============================================================================
void StepSlot::mouseDown (const juce::MouseEvent& e)
{
    // The trig strip is a button of its own and takes its own clicks, so the only part of
    // the slot that reaches here is the bar -- except for the few pixels of gap between the
    // two, which start nothing.
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

    // Reaches the bar directly rather than through the lane: a double click is one step's
    // own reset, and there is no stroke for it to be part of.
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

void StepSlot::paintOverChildren (juce::Graphics& g)
{
    if (! playing)
        return;

    // Drawn over the children rather than in paint(), because the bar fills the whole slot
    // and would cover anything painted underneath it. A muted lane keeps its playhead --
    // it is still running, and unmuting it mid-bar should not be a surprise.
    g.setColour (laneActive ? accent : accent.withAlpha (0.3f));
    g.drawRoundedRectangle (barArea().toFloat().reduced (0.75f), 3.0f, 1.5f);
}

void StepSlot::resized()
{
    auto r = getLocalBounds();

    onButton.setBounds (r.removeFromBottom (trigHeight));
    r.removeFromBottom (trigGap);

    valueSlider.setBounds (r);
    velocitySlider.setBounds (r);
    chanceSlider.setBounds (r);
    gateSlider.setBounds (r);
}

void StepSlot::setPlaying (bool shouldBePlaying)
{
    if (playing == shouldBePlaying)
        return;

    playing = shouldBePlaying;
    repaint();
}

//==============================================================================
LaneComponent::LaneComponent (juce::AudioProcessorValueTreeState& state, int laneIndex,
                              params::LanePattern& sharedClipboard, params::LaneKind kind)
    : apvts (state), lane (laneIndex), kind (kind), accent (theme::laneAccent (laneIndex)),
      clipboard (sharedClipboard), paramGroup (state)
{
    numberLabel.setText (juce::String (laneIndex + 1), juce::dontSendNotification);
    numberLabel.setFont (juce::Font (juce::FontOptions (14.0f, juce::Font::bold)));
    numberLabel.setColour (juce::Label::textColourId, accent);
    numberLabel.setJustificationType (juce::Justification::centred);
    numberLabel.setInterceptsMouseClicks (false, false);
    numberLabel.setBorderSize (juce::BorderSize<int> (0));
    addAndMakeVisible (numberLabel);

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
    // Two columns, filled left to right then wrapping, so each row pairs a structural
    // parameter with one that shapes the lane's feel. A Note lane and a CC lane share Length/
    // Rate/Depth/Direction; a CC lane goes on to add its own destination besides.
    auto* lengthRow = paramGroup.add (params::laneLengthId (laneIndex, kind), "Length");
    lengthRow->setTooltip ("How many of the eight steps the lane cycles through");
    paramGroup.add (params::laneDivId (laneIndex, kind),    "Rate");
    paramGroup.add (params::laneDepthId (laneIndex, kind),  "Mix Amount");
    paramGroup.add (params::laneDirId (laneIndex, kind),    "Direction");

    if (kind == params::LaneKind::cc)
    {
        paramGroup.add (params::laneCcOnId (laneIndex), "Send")
                  ->setTooltip ("Send this lane's own value as its own CC, independent of "
                               "the Mix CC");
        paramGroup.add (params::laneCcNumId (laneIndex), "Number");
        paramGroup.add (params::laneCcChanId (laneIndex), "Channel");
        paramGroup.add (params::laneCcOffsetId (laneIndex), "Offset")
                  ->setTooltip ("Shifts this lane's own tap. Independent of the CC tab's own "
                               "Offset, which shifts the Mix CC instead");
    }

    paramGroup.setColumns (2);
    addAndMakeVisible (paramGroup);

    //--------------------------------------------------------------------------
    randomiseButton.setTooltip ("Randomize this lane's 8 step values");
    theme::styleActionButton (randomiseButton);
    randomiseButton.onClick = [this, kind] { params::randomiseLaneValues (apvts, lane, random, kind); };
    addAndMakeVisible (randomiseButton);

    clearButton.setTooltip ("Zero this lane's 8 step values");
    theme::styleActionButton (clearButton);
    clearButton.onClick = [this, kind] { params::clearLaneValues (apvts, lane, kind); };
    addAndMakeVisible (clearButton);

    menuButton.setTooltip ("Rotate, invert, copy and paste this lane's pattern");
    theme::styleActionButton (menuButton);
    menuButton.onClick = [this] { showActionsMenu(); };
    addAndMakeVisible (menuButton);

    removeButton.setTooltip ("Remove this lane. The lanes below it move up to close the gap, "
                             "and this one's pattern goes with it -- Ctrl+Z brings it back");
    theme::styleActionButton (removeButton);
    removeButton.onClick = [this] { if (onRemove != nullptr) onRemove(); };

    // Added hidden: the editor turns it on for every lane once there is more than one.
    addChildComponent (removeButton);

    //--------------------------------------------------------------------------
    if (kind == params::LaneKind::note)
    {
        static const char* layerTooltips[]
        {
            "Bars edit each step's value, which drives pitch",
            "Bars edit each step's velocity accent",
            "Bars edit each step's probability of firing",
            "Bars edit each step's gate, how long its note is held",
        };

        for (int i = 0; i < numStepLayers; ++i)
        {
            auto& button = layerButtons[i];

            button.setTooltip (layerTooltips[i]);
            button.setClickingTogglesState (false);
            button.onClick = [this, i] { setLayer ((StepLayer) i); };
            addAndMakeVisible (button);
        }
    }
    else
    {
        // A CC lane has no layer to select -- the bars always edit Value -- so the selector
        // never appears, leaving its column blank rather than four buttons that would do
        // nothing.
        for (auto& button : layerButtons)
            button.setVisible (false);
    }

    // The attachment drives the slider through Slider::Listener, the same way the step
    // trigs' does, so onValueChange is free for the lane's own use -- and it fires for a
    // change from the host as readily as for a drag.
    lengthSlider = dynamic_cast<juce::Slider*> (&lengthRow->getControl());

    if (lengthSlider != nullptr)
        lengthSlider->onValueChange = [this] { applyLength(); };

    setLayer (StepLayer::value);
    applyLaneState();
    applyLength();
}

LaneComponent::~LaneComponent()
{
    // A lane cannot normally go while a stroke is running -- the button that removes it is
    // not reachable with a step bar holding the mouse -- but an undo step left held open
    // would quietly swallow every edit made after it into the same step.
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

    slot.beginBarDrag (e);
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
    strokeSlot->beginBarDrag (atPoint);
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

    // How far along the cursor's travel this step sits, so the height can be read off the
    // line between the two reported positions. A stroke that only moved vertically never
    // reaches here, but the division is guarded all the same.
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
    if (lengthSlider == nullptr)
        return;

    const int length = juce::jlimit (1, params::numSteps, (int) std::lround (lengthSlider->getValue()));

    if (appliedLength == length)
        return;

    appliedLength = length;

    for (int i = 0; i < slots.size(); ++i)
        slots.getUnchecked (i)->setWithinLength (i < length);
}

void LaneComponent::applyLaneState()
{
    const int active = onButton.getToggleState() ? 1 : 0;

    if (appliedLaneActive == active)
        return;

    appliedLaneActive = active;

    numberLabel.setColour (juce::Label::textColourId,
                           active != 0 ? accent : accent.withAlpha (0.35f));
    numberLabel.repaint();

    for (auto* slot : slots)
        slot->setLaneActive (active != 0);

    // The accent stripe is painted here, not by a child.
    repaint();
}

void LaneComponent::setLayer (StepLayer layer)
{
    currentLayer = layer;

    for (int i = 0; i < numStepLayers; ++i)
        layerButtons[i].setToggleState (i == (int) layer, juce::dontSendNotification);

    for (auto* slot : slots)
        slot->setLayer (layer);
}

//==============================================================================
void LaneComponent::showActionsMenu()
{
    juce::PopupMenu menu;
    menu.setLookAndFeel (&getLookAndFeel());

    menu.addItem (1, "Rotate left");
    menu.addItem (2, "Rotate right");
    menu.addItem (3, "Invert values");
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
                            const auto kind = safeThis->kind;

                            switch (result)
                            {
                                case 1: params::rotateLane (state, laneIndex, -1, kind); break;
                                case 2: params::rotateLane (state, laneIndex, 1, kind); break;
                                case 3: params::invertLaneValues (state, laneIndex, kind); break;
                                case 4: safeThis->clipboard = params::copyLane (state, laneIndex, kind); break;
                                case 5: params::pasteLane (state, laneIndex, safeThis->clipboard, kind); break;
                                default: break;
                            }
                        });
}

//==============================================================================
void LaneComponent::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();

    // A flat fill and no outline: the lane reads as a card raised off the window's one
    // surface, which separates it without adding another rectangle to the picture. This is
    // the only thing separating it now -- the window no longer draws a panel behind the
    // lane stack -- so matching theme::surface here would leave a lane, the gap around it,
    // the Add lane bar and the settings block below it all one indistinguishable colour.
    g.setColour (theme::raised);
    g.fillRoundedRectangle (bounds, 6.0f);

    // Accent stripe down the left edge identifies the lane at a glance, and goes faint
    // while the lane is muted so the state reads from across the window.
    g.setColour (appliedLaneActive == 0 ? accent.withAlpha (0.25f) : accent);
    g.fillRoundedRectangle (bounds.getX() + 10.0f, bounds.getY() + 14.0f, (float) lane::railWidth,
                            bounds.getHeight() - 28.0f, 1.5f);

    g.setColour (theme::outline);
    g.fillRect ((float) dividerX, bounds.getY() + 14.0f, 1.0f, bounds.getHeight() - 28.0f);
}

void LaneComponent::resized()
{
    auto r = getLocalBounds().reduced (lane::inset, 10);

    r.removeFromLeft (lane::railWidth);
    r.removeFromLeft (lane::railGap);

    auto leftColumn = r.removeFromLeft (lane::numberWidth);

    // The lane number and its mute share the top row: the toggle belongs with the label
    // that identifies the lane, and the layer buttons below keep their full width.
    auto identityRow = leftColumn.removeFromTop (18);
    onButton.setBounds (identityRow.removeFromRight (16).reduced (1, 2));
    numberLabel.setBounds (identityRow.withTrimmedRight (4));

    leftColumn.removeFromTop (6);

    for (auto& button : layerButtons)
    {
        // Skipped entirely with Notes off, where the whole selector is hidden.
        if (! button.isVisible())
            continue;

        button.setBounds (leftColumn.removeFromTop (theme::rowHeight));
        leftColumn.removeFromTop (2);
    }

    r.removeFromLeft (lane::columnGap);

    //--------------------------------------------------------------------------
    auto paramBlock = r.removeFromRight (lane::paramWidth);
    r.removeFromRight (lane::dividerGap);
    dividerX = r.getRight() + lane::dividerGap / 2;

    //--------------------------------------------------------------------------
    // A fixed pitch rather than a share of what is left over, so a step is the same width
    // whatever zoom the window is at -- the window itself is sized from this. Clamped only
    // so a host that forces the editor narrower than its native size still lays out.
    const int slotWidth = juce::jmin (lane::stepSlotWidth, r.getWidth() / params::numSteps);

    for (int i = 0; i < slots.size(); ++i)
    {
        slots.getUnchecked (i)->setBounds (r.removeFromLeft (slotWidth).withTrimmedRight (slotGap));

        // Extra room after every fourth step, so the row reads as four groups instead of
        // one long strip -- skipped after the last step, which already ends at the divider.
        if ((i + 1) % 4 == 0 && i + 1 < slots.size())
            r.removeFromLeft (lane::groupGap);
    }

    //--------------------------------------------------------------------------
    // Pushed to the two ends of the lane rather than centred as one block: the parameter
    // rows sit level with the top of the step bars and the pattern buttons with the bottom,
    // so the whitespace collects between them. That separates the two by what they are --
    // settings that stay put, and actions that rewrite the pattern under them -- instead of
    // leaving the actions looking like one more row of the block above.
    const int groupHeight  = paramGroup.getPreferredHeight();
    const int buttonHeight = theme::rowHeight;

    paramGroup.setBounds (paramBlock.removeFromTop (groupHeight));

    auto actionRow = paramBlock.removeFromBottom (buttonHeight);

    // Hard right, a wide gap clear of the pattern buttons. It is the only action in the lane
    // that a second click does not undo, so it should not sit where the hand passes on the
    // way to the ones that do.
    removeButton.setBounds (actionRow.removeFromRight (
                                theme::actionButtonWidth (removeButton.getButtonText(), buttonHeight)));

    // Each takes only the width its own label needs, so the gap before Remove absorbs the
    // difference rather than the buttons padding out to meet it.
    for (auto* button : { &randomiseButton, &clearButton, &menuButton })
    {
        button->setBounds (actionRow.removeFromLeft (
                               theme::actionButtonWidth (button->getButtonText(), buttonHeight)));
        actionRow.removeFromLeft (4);
    }
}

void LaneComponent::setCanRemove (bool canBeRemoved)
{
    removeButton.setVisible (canBeRemoved);
}

void LaneComponent::setLayerSelectionAvailable (bool available)
{
    if (layerSelectionAvailable == available)
        return;

    layerSelectionAvailable = available;

    for (auto& button : layerButtons)
        button.setVisible (available);

    // With nothing left to select, the bars go back to Value -- otherwise Notes could be
    // switched off with them still editing a layer whose button has just gone.
    if (! available)
        setLayer (StepLayer::value);

    resized();
}

void LaneComponent::setPlayingStep (int stepIndex)
{
    if (playingStep == stepIndex)
        return;

    playingStep = stepIndex;

    for (int i = 0; i < slots.size(); ++i)
        slots.getUnchecked (i)->setPlaying (i == stepIndex);
}
