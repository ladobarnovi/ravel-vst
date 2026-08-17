#include "LaneComponent.h"

namespace
{
    constexpr int gateHeight   = 5;
    constexpr int gateGap      = 4;

    // Wide enough that the eight gate strips read as eight marks rather than as one line
    // ruled under the whole step area.
    constexpr int slotGap      = 5;

    constexpr int railWidth    = 3;
    constexpr int numberWidth  = 16;
    constexpr int paramWidth   = 280;
    constexpr int dividerGap   = 14;
}

//==============================================================================
StepSlot::StepSlot (juce::AudioProcessorValueTreeState& state, int laneIndex, int stepIndex)
    : accent (theme::laneAccent (laneIndex))
{
    valueSlider.setSliderStyle (juce::Slider::LinearBarVertical);
    valueSlider.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    valueSlider.setColour (juce::Slider::backgroundColourId, theme::track);
    valueSlider.setPopupDisplayEnabled (true, true, getParentComponent());
    valueSlider.setDoubleClickReturnValue (true, 0.0);
    theme::setRole (valueSlider, theme::Role::stepBar);
    addAndMakeVisible (valueSlider);

    // Added after the value bar so it paints on top of it; see ChanceOverlay for how the
    // two share the same rectangle without fighting over the mouse.
    chanceSlider.setSliderStyle (juce::Slider::LinearBarVertical);
    chanceSlider.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    chanceSlider.setColour (juce::Slider::backgroundColourId, juce::Colours::transparentBlack);
    chanceSlider.setColour (juce::Slider::trackColourId, theme::text.withAlpha (0.7f));
    chanceSlider.setPopupDisplayEnabled (true, true, getParentComponent());
    chanceSlider.setDoubleClickReturnValue (true, 1.0);
    chanceSlider.setTooltip ("Chance this step fires -- drag the right edge of the bar");
    theme::setRole (chanceSlider, theme::Role::stepChance);
    addAndMakeVisible (chanceSlider);

    onButton.setColour (juce::ToggleButton::tickColourId, accent);
    onButton.setTooltip ("Mute or unmute this step");
    theme::setRole (onButton, theme::Role::stepGate);
    addAndMakeVisible (onButton);

    valueAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        state, params::stepValueId (laneIndex, stepIndex), valueSlider);

    chanceAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        state, params::stepChanceId (laneIndex, stepIndex), chanceSlider);

    onAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        state, params::stepOnId (laneIndex, stepIndex), onButton);

    // ButtonAttachment listens through addListener rather than through either callback,
    // so onStateChange is free for the lane's own use.
    onButton.onStateChange = [this] { applyGateState(); };
    applyGateState();
}

void StepSlot::applyGateState()
{
    const bool on = onButton.getToggleState();

    valueSlider.setColour (juce::Slider::trackColourId, on ? accent : accent.withAlpha (0.25f));
    valueSlider.repaint();
}

void StepSlot::paintOverChildren (juce::Graphics& g)
{
    if (! playing)
        return;

    // Drawn over the children rather than in paint(), because the value bar now fills the
    // whole slot and would cover anything painted underneath it.
    g.setColour (accent);
    g.drawRoundedRectangle (valueSlider.getBounds().toFloat().reduced (0.75f), 3.0f, 1.5f);
}

void StepSlot::resized()
{
    auto r = getLocalBounds();

    onButton.setBounds (r.removeFromBottom (gateHeight));
    r.removeFromBottom (gateGap);

    valueSlider.setBounds (r);
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
    // Two columns, filled left to right then wrapping, so each row pairs a length-ish
    // parameter with a character-ish one.
    paramGroup.add (params::laneLengthId (laneIndex), "Length");
    paramGroup.add (params::laneDivId (laneIndex),    "Rate");
    paramGroup.add (params::laneDirId (laneIndex),    "Direction");
    paramGroup.add (params::laneDepthId (laneIndex),  "Depth");
    paramGroup.add (params::laneModeId (laneIndex),   "Mix mode");
    paramGroup.add (params::laneNudgeId (laneIndex),  "Nudge")
              ->setTooltip ("Shift this whole lane earlier or later, up to half a step");
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
    numberLabel.setBounds (r.removeFromLeft (numberWidth));
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
    const int groupHeight  = ControlGroup::heightForRows (3, false);
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
