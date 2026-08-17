#include "LaneComponent.h"

namespace
{
    constexpr int gateHeight   = 5;
    constexpr int gateGap      = 4;

    // Wide enough that the eight gate strips read as eight marks rather than as one line
    // ruled under the whole step area.
    constexpr int slotGap      = 5;

    constexpr int railWidth    = 3;

    // Sized for the longest layer button, "Velocity". The column to the left of the steps was
    // empty space anyway, and abbreviating the labels down to three letters cost more in
    // legibility than the width was worth.
    constexpr int numberWidth  = 54;
    constexpr int paramWidth   = 280;
    constexpr int dividerGap   = 14;
}

//==============================================================================
StepSlot::StepSlot (juce::AudioProcessorValueTreeState& state, int laneIndex, int stepIndex)
    : accent (theme::laneAccent (laneIndex))
{
    struct LayerSetup
    {
        juce::Slider& slider;
        juce::String  paramID;
        double        resetTo;
        const char*   tooltip;
    };

    const LayerSetup setups[]
    {
        { valueSlider,    params::stepValueId (laneIndex, stepIndex),    0.0, "Step value -- drives pitch" },
        { velocitySlider, params::stepVelocityId (laneIndex, stepIndex), 1.0, "This step's accent, as a trim on the lane's velocity" },
        { chanceSlider,   params::stepChanceId (laneIndex, stepIndex),   1.0, "Probability this step fires" },
    };

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>* attachments[]
        { &valueAttachment, &velocityAttachment, &chanceAttachment };

    for (int i = 0; i < 3; ++i)
    {
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
    theme::setRole (onButton, theme::Role::stepGate);
    addAndMakeVisible (onButton);

    onAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        state, params::stepOnId (laneIndex, stepIndex), onButton);

    // ButtonAttachment listens through addListener rather than through either callback,
    // so onStateChange is free for the lane's own use.
    onButton.onStateChange = [this] { applyGateState(); };

    setLayer (StepLayer::value);
    applyGateState();
}

juce::Slider& StepSlot::sliderFor (StepLayer layer) noexcept
{
    switch (layer)
    {
        case StepLayer::velocity: return velocitySlider;
        case StepLayer::chance:   return chanceSlider;
        case StepLayer::value:
        default:                  return valueSlider;
    }
}

void StepSlot::setLayer (StepLayer layer)
{
    currentLayer = layer;

    for (auto l : { StepLayer::value, StepLayer::velocity, StepLayer::chance })
        sliderFor (l).setVisible (l == layer);

    applyGateState();
    repaint();
}

void StepSlot::applyGateState()
{
    const bool on = onButton.getToggleState();
    auto& visible = sliderFor (currentLayer);

    visible.setColour (juce::Slider::trackColourId, on ? accent : accent.withAlpha (0.25f));
    visible.repaint();
}

juce::Rectangle<int> StepSlot::barArea() const
{
    return getLocalBounds().withTrimmedBottom (gateHeight + gateGap);
}

void StepSlot::drawLayerTick (juce::Graphics& g, const juce::Slider& slider, StepLayer layer) const
{
    const auto range = slider.getRange();

    if (range.getLength() <= 0.0)
        return;

    const auto bar = barArea().toFloat();
    const float proportion = (float) ((slider.getValue() - range.getStart()) / range.getLength());
    const float y = bar.getBottom() - bar.getHeight() * juce::jlimit (0.0f, 1.0f, proportion);

    // One third each, in layer order, so the two visible ticks never overlap whichever layer
    // happens to be the selected one.
    const float width = bar.getWidth() / 3.0f;
    const float x = bar.getX() + width * (float) (int) layer;

    g.setColour (theme::text.withAlpha (0.55f));
    g.fillRect (x, juce::jlimit (bar.getY(), bar.getBottom() - 1.5f, y - 0.75f), width, 1.5f);
}

void StepSlot::paintOverChildren (juce::Graphics& g)
{
    // The two layers that are not being edited show as ticks, so changing what the bars edit
    // never hides the rest of the step. Each is drawn only when it is away from its default,
    // so an untouched lane stays clean.
    if (currentLayer != StepLayer::value && valueSlider.getValue() > 0.001)
        drawLayerTick (g, valueSlider, StepLayer::value);

    if (currentLayer != StepLayer::velocity && velocitySlider.getValue() < 0.999)
        drawLayerTick (g, velocitySlider, StepLayer::velocity);

    if (currentLayer != StepLayer::chance && chanceSlider.getValue() < 0.999)
        drawLayerTick (g, chanceSlider, StepLayer::chance);

    if (! playing)
        return;

    // Drawn over the children rather than in paint(), because the bar fills the whole slot
    // and would cover anything painted underneath it.
    g.setColour (accent);
    g.drawRoundedRectangle (barArea().toFloat().reduced (0.75f), 3.0f, 1.5f);
}

void StepSlot::resized()
{
    auto r = getLocalBounds();

    onButton.setBounds (r.removeFromBottom (gateHeight));
    r.removeFromBottom (gateGap);

    valueSlider.setBounds (r);
    velocitySlider.setBounds (r);
    chanceSlider.setBounds (r);
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
                              params::LanePattern& sharedClipboard)
    : apvts (state), lane (laneIndex), accent (theme::laneAccent (laneIndex)),
      clipboard (sharedClipboard), paramGroup (state)
{
    numberLabel.setText (juce::String (laneIndex + 1), juce::dontSendNotification);
    numberLabel.setFont (juce::Font (juce::FontOptions (14.0f, juce::Font::bold)));
    numberLabel.setColour (juce::Label::textColourId, accent);
    numberLabel.setJustificationType (juce::Justification::centred);
    numberLabel.setInterceptsMouseClicks (false, false);
    numberLabel.setBorderSize (juce::BorderSize<int> (0));
    addAndMakeVisible (numberLabel);

    for (int step = 0; step < params::numSteps; ++step)
        addAndMakeVisible (slots.add (new StepSlot (state, laneIndex, step)));

    //--------------------------------------------------------------------------
    // Two columns, filled left to right then wrapping, so each row pairs a structural
    // parameter with one that shapes the lane's feel.
    paramGroup.add (params::laneLengthId (laneIndex), "Length");
    paramGroup.add (params::laneDivId (laneIndex),    "Rate");
    paramGroup.add (params::laneDirId (laneIndex),    "Direction");
    paramGroup.add (params::laneDepthId (laneIndex),  "Depth");
    paramGroup.add (params::laneModeId (laneIndex),   "Mix mode");
    paramGroup.add (params::laneNudgeId (laneIndex),  "Nudge")
              ->setTooltip ("Shift this whole lane earlier or later, up to half a step");
    paramGroup.add (params::laneVelocityId (laneIndex), "Velocity")
              ->setTooltip ("Trim on the global Velocity, for notes this lane fires");
    paramGroup.add (params::laneHumanizeId (laneIndex), "Humanise")
              ->setTooltip ("Random timing jitter, repeatable per bar");
    paramGroup.setColumns (2);
    addAndMakeVisible (paramGroup);

    //--------------------------------------------------------------------------
    randomiseButton.setTooltip ("Randomise this lane's 8 step values");
    randomiseButton.onClick = [this] { params::randomiseLaneValues (apvts, lane, random); };
    addAndMakeVisible (randomiseButton);

    clearButton.setTooltip ("Zero this lane's 8 step values");
    clearButton.onClick = [this] { params::clearLaneValues (apvts, lane); };
    addAndMakeVisible (clearButton);

    menuButton.setTooltip ("Rotate, invert, copy and paste this lane's pattern");
    menuButton.onClick = [this] { showActionsMenu(); };
    addAndMakeVisible (menuButton);

    //--------------------------------------------------------------------------
    static const char* layerTooltips[]
    {
        "Bars edit each step's value, which drives pitch",
        "Bars edit each step's velocity accent",
        "Bars edit each step's probability of firing",
    };

    for (int i = 0; i < 3; ++i)
    {
        auto& button = layerButtons[i];

        button.setTooltip (layerTooltips[i]);
        button.setClickingTogglesState (false);
        button.onClick = [this, i] { setLayer ((StepLayer) i); };
        addAndMakeVisible (button);
    }

    setLayer (StepLayer::value);
}

void LaneComponent::setLayer (StepLayer layer)
{
    currentLayer = layer;

    for (int i = 0; i < 3; ++i)
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

                            switch (result)
                            {
                                case 1: params::rotateLane (state, laneIndex, -1); break;
                                case 2: params::rotateLane (state, laneIndex, 1); break;
                                case 3: params::invertLaneValues (state, laneIndex); break;
                                case 4: safeThis->clipboard = params::copyLane (state, laneIndex); break;
                                case 5: params::pasteLane (state, laneIndex, safeThis->clipboard); break;
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

    // Accent stripe down the left edge identifies the lane at a glance.
    g.setColour (accent);
    g.fillRoundedRectangle (bounds.getX() + 10.0f, bounds.getY() + 14.0f, (float) railWidth,
                            bounds.getHeight() - 28.0f, 1.5f);

    g.setColour (theme::outline);
    g.fillRect ((float) dividerX, bounds.getY() + 14.0f, 1.0f, bounds.getHeight() - 28.0f);
}

void LaneComponent::resized()
{
    auto r = getLocalBounds().reduced (10, 10);

    r.removeFromLeft (railWidth);
    r.removeFromLeft (8);

    auto leftColumn = r.removeFromLeft (numberWidth);
    numberLabel.setBounds (leftColumn.removeFromTop (18));
    leftColumn.removeFromTop (6);

    for (auto& button : layerButtons)
    {
        button.setBounds (leftColumn.removeFromTop (theme::rowHeight));
        leftColumn.removeFromTop (2);
    }

    r.removeFromLeft (10);

    //--------------------------------------------------------------------------
    auto paramBlock = r.removeFromRight (paramWidth);
    r.removeFromRight (dividerGap);
    dividerX = r.getRight() + dividerGap / 2;

    //--------------------------------------------------------------------------
    const int slotWidth = r.getWidth() / params::numSteps;

    for (auto* slot : slots)
        slot->setBounds (r.removeFromLeft (slotWidth).withTrimmedRight (slotGap));

    //--------------------------------------------------------------------------
    // Parameter rows plus the pattern buttons, as one block centred against the steps.
    const int groupHeight  = ControlGroup::heightForRows (4, false);
    const int buttonHeight = theme::rowHeight;
    const int blockHeight  = groupHeight + theme::rowGap * 2 + buttonHeight;

    paramBlock = paramBlock.withSizeKeepingCentre (paramBlock.getWidth(), blockHeight);

    paramGroup.setBounds (paramBlock.removeFromTop (groupHeight));
    paramBlock.removeFromTop (theme::rowGap * 2);

    auto buttonRow = paramBlock.removeFromTop (buttonHeight).removeFromLeft (paramWidth / 2 - 14);
    const int buttonWidth = (buttonRow.getWidth() - 8) / 3;

    randomiseButton.setBounds (buttonRow.removeFromLeft (buttonWidth));
    buttonRow.removeFromLeft (4);
    clearButton.setBounds (buttonRow.removeFromLeft (buttonWidth));
    buttonRow.removeFromLeft (4);
    menuButton.setBounds (buttonRow.removeFromLeft (buttonWidth));
}

void LaneComponent::setPlayingStep (int stepIndex)
{
    if (playingStep == stepIndex)
        return;

    playingStep = stepIndex;

    for (int i = 0; i < slots.size(); ++i)
        slots.getUnchecked (i)->setPlaying (i == stepIndex);
}
