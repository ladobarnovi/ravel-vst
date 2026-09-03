#include "LaneComponent.h"

namespace
{
    constexpr int trigHeight   = 11;
    constexpr int trigGap      = 4;

    // Wide enough that the sixteen trig strips read as sixteen marks rather than as one line
    // ruled under the whole step area. Part of lane::stepSlotWidth, not extra to it: the
    // gap comes out of the slot, so the pitch from one step to the next stays exact.
    constexpr int slotGap      = 5;
}

//==============================================================================
StepSlot::StepSlot (juce::AudioProcessorValueTreeState& state, int laneIndex, int stepIndex,
                    params::LaneKind kind)
    : accent (theme::laneAccent (laneIndex)),
      // A CC step never has a selector to switch it away from Value, and it has no
      // Velocity/Gate parameter to show a tick for regardless -- see setNoteLayersAvailable.
      noteLayersAvailable (kind == params::LaneKind::note)
{
    struct LayerSetup
    {
        juce::Slider& slider;
        juce::String  paramID;
        double        resetTo;
        const char*   tooltip;
    };

    // Velocity and Gate are note-only parameters -- a CC lane's step has neither (see
    // Parameters.cpp) -- so for a CC-kind slot only Value and Chance get built at all.
    const bool isCc = kind == params::LaneKind::cc;

    const LayerSetup setups[]
    {
        { valueSlider,    params::stepValueId (laneIndex, stepIndex, kind),  0.0,  "Step value -- drives pitch" },
        { velocitySlider, isCc ? juce::String() : params::stepVelocityId (laneIndex, stepIndex),
          1.0,  "This step's accent, as a trim on the global Velocity" },
        { chanceSlider,   params::stepChanceId (laneIndex, stepIndex, kind), 1.0,  "Probability this step fires" },
        { gateSlider,     isCc ? juce::String() : params::stepGateId (laneIndex, stepIndex),
          60.0, "How long this step's note is held, as % of the step" },
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
        slider.setPopupDisplayEnabled (true, true, getParentComponent());
        slider.setDoubleClickReturnValue (true, setups[i].resetTo);
        slider.setTooltip (setups[i].tooltip);
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

void StepSlot::setNoteLayersAvailable (bool available)
{
    if (noteLayersAvailable == available)
        return;

    noteLayersAvailable = available;
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
    visible.setColour (juce::Slider::backgroundColourId,
                       withinLength ? theme::track
                                    : theme::track.interpolatedWith (theme::panel, 0.85f));
    visible.repaint();

    // Mixed toward the panel rather than made transparent: drawStepTrig sets its own alpha
    // on whatever colour it finds here, so an alpha stored on this one would be discarded.
    onButton.setColour (juce::ToggleButton::tickColourId,
                        live ? accent : accent.interpolatedWith (theme::panel, 0.8f));
    onButton.repaint();
}

juce::Rectangle<int> StepSlot::barArea() const
{
    return getLocalBounds().withTrimmedBottom (trigHeight + trigGap);
}

void StepSlot::drawLayerTick (juce::Graphics& g, const juce::Slider& slider, StepLayer layer) const
{
    const auto range = slider.getRange();

    if (range.getLength() <= 0.0)
        return;

    const auto bar = barArea().toFloat();
    const float proportion = (float) ((slider.getValue() - range.getStart()) / range.getLength());
    const float y = bar.getBottom() - bar.getHeight() * juce::jlimit (0.0f, 1.0f, proportion);

    // One quarter each, in layer order, so the visible ticks never overlap whichever layer
    // happens to be the selected one.
    const float width = bar.getWidth() / (float) numStepLayers;
    const float x = bar.getX() + width * (float) (int) layer;

    g.setColour (theme::text.withAlpha (withinLength ? 0.55f : 0.18f));
    g.fillRect (x, juce::jlimit (bar.getY(), bar.getBottom() - 1.5f, y - 0.75f), width, 1.5f);
}

void StepSlot::paintOverChildren (juce::Graphics& g)
{
    // The two layers that are not being edited show as ticks, so changing what the bars edit
    // never hides the rest of the step. Each is drawn only when it is away from its default,
    // so an untouched lane stays clean.
    if (currentLayer != StepLayer::value && valueSlider.getValue() > 0.001)
        drawLayerTick (g, valueSlider, StepLayer::value);

    // Velocity and gate go unread with Notes off, so their ticks come off with their buttons
    // -- a mark showing a value nothing acts on is worse than no mark.
    if (noteLayersAvailable && currentLayer != StepLayer::velocity && velocitySlider.getValue() < 0.999)
        drawLayerTick (g, velocitySlider, StepLayer::velocity);

    // Chance stays in both modes: it gates whether the step reaches the mix, and the mix is
    // what the CC output follows.
    if (currentLayer != StepLayer::chance && chanceSlider.getValue() < 0.999)
        drawLayerTick (g, chanceSlider, StepLayer::chance);

    if (noteLayersAvailable && currentLayer != StepLayer::gate && std::abs (gateSlider.getValue() - 60.0) > 0.5)
        drawLayerTick (g, gateSlider, StepLayer::gate);

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
    // Rate/Depth; Direction is Note-only -- a CC lane always folds Forward -- and a CC lane
    // goes on to add its own destination instead.
    auto* lengthRow = paramGroup.add (params::laneLengthId (laneIndex, kind), "Length");
    lengthRow->setTooltip ("How many of the eight steps the lane cycles through");
    paramGroup.add (params::laneDivId (laneIndex, kind),    "Rate");
    paramGroup.add (params::laneDepthId (laneIndex, kind),  "Depth");

    if (kind == params::LaneKind::note)
        paramGroup.add (params::laneDirId (laneIndex, kind), "Direction");

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

    // A flat fill and no outline: the lane reads as a surface a step lighter than the
    // window, which separates it without adding another rectangle to the picture.
    g.setColour (theme::panel);
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

    for (auto* slot : slots)
        slot->setBounds (r.removeFromLeft (slotWidth).withTrimmedRight (slotGap));

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

    // Velocity and gate ticks come off with them; chance ticks stay, because chance is still
    // gating the mix and so still moving the CC output.
    for (auto* slot : slots)
        slot->setNoteLayersAvailable (available);

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
