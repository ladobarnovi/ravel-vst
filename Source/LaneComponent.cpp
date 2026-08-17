#include "LaneComponent.h"

namespace
{
    using theme::styleCaption;

    /** Populates a ComboBox from the choice parameter it is about to be attached to,
        so the item order always matches the parameter's own index order.
    */
    void fillFromChoiceParam (juce::AudioProcessorValueTreeState& state,
                              const juce::String& paramID,
                              juce::ComboBox& box)
    {
        if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (state.getParameter (paramID)))
            box.addItemList (choice->choices, 1);
    }

    void styleCompactSlider (juce::Slider& slider, juce::Colour accent, int textBoxWidth)
    {
        slider.setSliderStyle (juce::Slider::LinearHorizontal);
        slider.setTextBoxStyle (juce::Slider::TextBoxRight, false, textBoxWidth, 18);
        slider.setColour (juce::Slider::trackColourId, accent.withAlpha (0.75f));
    }
}

//==============================================================================
StepSlot::StepSlot (juce::AudioProcessorValueTreeState& state, int laneIndex, int stepIndex)
    : accent (theme::laneAccent (laneIndex))
{
    valueSlider.setSliderStyle (juce::Slider::LinearBarVertical);
    valueSlider.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    valueSlider.setColour (juce::Slider::trackColourId, accent);
    valueSlider.setColour (juce::Slider::backgroundColourId, theme::track);
    valueSlider.setPopupDisplayEnabled (true, true, getParentComponent());
    valueSlider.setDoubleClickReturnValue (true, 0.0);
    addAndMakeVisible (valueSlider);

    // Thin bar under the value: the step's chance of firing. Dimmer than the value bar
    // so a lane still reads as a pattern at a glance.
    chanceSlider.setSliderStyle (juce::Slider::LinearBar);
    chanceSlider.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    chanceSlider.setColour (juce::Slider::trackColourId, accent.withAlpha (0.45f));
    chanceSlider.setColour (juce::Slider::backgroundColourId, theme::track);
    chanceSlider.setPopupDisplayEnabled (true, true, getParentComponent());
    chanceSlider.setDoubleClickReturnValue (true, 1.0);
    chanceSlider.setTooltip ("Chance this step fires");
    addAndMakeVisible (chanceSlider);

    onButton.setColour (juce::ToggleButton::tickColourId, accent);
    addAndMakeVisible (onButton);

    valueAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        state, params::stepValueId (laneIndex, stepIndex), valueSlider);

    chanceAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        state, params::stepChanceId (laneIndex, stepIndex), chanceSlider);

    onAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        state, params::stepOnId (laneIndex, stepIndex), onButton);
}

void StepSlot::paint (juce::Graphics& g)
{
    if (! playing)
        return;

    // Playhead ring around the whole slot.
    g.setColour (accent.withAlpha (0.9f));
    g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (0.75f), 4.0f, 1.5f);
}

void StepSlot::resized()
{
    auto r = getLocalBounds().reduced (2);

    onButton.setBounds (r.removeFromBottom (16));
    r.removeFromBottom (3);

    chanceSlider.setBounds (r.removeFromBottom (9));
    r.removeFromBottom (3);

    valueSlider.setBounds (r);
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
      clipboard (sharedClipboard)
{
    titleLabel.setText ("LANE " + juce::String (laneIndex + 1), juce::dontSendNotification);
    titleLabel.setFont (theme::titleFont());
    titleLabel.setColour (juce::Label::textColourId, accent);
    titleLabel.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (titleLabel);

    for (int step = 0; step < params::numSteps; ++step)
        addAndMakeVisible (slots.add (new StepSlot (state, laneIndex, step)));

    //--------------------------------------------------------------------------
    styleCompactSlider (lengthSlider, accent, 34);
    addAndMakeVisible (lengthSlider);
    styleCaption (lengthLabel, "Length");
    addAndMakeVisible (lengthLabel);

    styleCompactSlider (depthSlider, accent, 48);
    depthSlider.setDoubleClickReturnValue (true, 0.0);
    addAndMakeVisible (depthSlider);
    styleCaption (depthLabel, "Depth");
    addAndMakeVisible (depthLabel);

    styleCompactSlider (nudgeSlider, accent, 48);
    nudgeSlider.setDoubleClickReturnValue (true, 0.0);
    nudgeSlider.setTooltip ("Shift this whole lane earlier or later, up to half a step");
    addAndMakeVisible (nudgeSlider);
    styleCaption (nudgeLabel, "Nudge");
    addAndMakeVisible (nudgeLabel);

    styleCompactSlider (humanizeSlider, accent, 44);
    humanizeSlider.setTooltip ("Random timing jitter, repeatable per bar");
    addAndMakeVisible (humanizeSlider);
    styleCaption (humanizeLabel, "Humanize");
    addAndMakeVisible (humanizeLabel);

    styleCompactSlider (ccNumberSlider, accent, 38);
    addAndMakeVisible (ccNumberSlider);
    styleCaption (ccNumberLabel, "CC Number");
    addAndMakeVisible (ccNumberLabel);

    styleCompactSlider (ccChannelSlider, accent, 38);
    addAndMakeVisible (ccChannelSlider);
    styleCaption (ccChannelLabel, "CC Chan");
    addAndMakeVisible (ccChannelLabel);

    ccOnButton.setColour (juce::ToggleButton::tickColourId, accent);
    ccOnButton.setTooltip ("Send this lane's own value as CC, independent of Depth");
    addAndMakeVisible (ccOnButton);
    styleCaption (ccOnLabel, "Lane CC");
    addAndMakeVisible (ccOnLabel);

    fillFromChoiceParam (state, params::laneDivId (laneIndex), divisionBox);
    addAndMakeVisible (divisionBox);
    styleCaption (divisionLabel, "Rate");
    addAndMakeVisible (divisionLabel);

    fillFromChoiceParam (state, params::laneDirId (laneIndex), directionBox);
    addAndMakeVisible (directionBox);
    styleCaption (directionLabel, "Direction");
    addAndMakeVisible (directionLabel);

    fillFromChoiceParam (state, params::laneModeId (laneIndex), modeBox);
    addAndMakeVisible (modeBox);
    styleCaption (modeLabel, "Mix Mode");
    addAndMakeVisible (modeLabel);

    //--------------------------------------------------------------------------
    styleCaption (patternLabel, "Pattern");
    addAndMakeVisible (patternLabel);

    randomiseButton.setTooltip ("Randomise this lane's 8 step values");
    randomiseButton.setColour (juce::TextButton::textColourOffId, accent);
    randomiseButton.onClick = [this] { params::randomiseLaneValues (apvts, lane, random); };
    addAndMakeVisible (randomiseButton);

    clearButton.setTooltip ("Zero this lane's 8 step values");
    clearButton.onClick = [this] { params::clearLaneValues (apvts, lane); };
    addAndMakeVisible (clearButton);

    menuButton.setTooltip ("Rotate, invert, copy and paste this lane's pattern");
    menuButton.onClick = [this] { showActionsMenu(); };
    addAndMakeVisible (menuButton);

    //--------------------------------------------------------------------------
    lengthAttachment    = std::make_unique<SliderAttachment>   (state, params::laneLengthId (laneIndex),   lengthSlider);
    depthAttachment     = std::make_unique<SliderAttachment>   (state, params::laneDepthId (laneIndex),    depthSlider);
    nudgeAttachment     = std::make_unique<SliderAttachment>   (state, params::laneNudgeId (laneIndex),    nudgeSlider);
    humanizeAttachment  = std::make_unique<SliderAttachment>   (state, params::laneHumanizeId (laneIndex), humanizeSlider);
    ccNumberAttachment  = std::make_unique<SliderAttachment>   (state, params::laneCcNumId (laneIndex),    ccNumberSlider);
    ccChannelAttachment = std::make_unique<SliderAttachment>   (state, params::laneCcChanId (laneIndex),   ccChannelSlider);
    divisionAttachment  = std::make_unique<ComboBoxAttachment> (state, params::laneDivId (laneIndex),      divisionBox);
    directionAttachment = std::make_unique<ComboBoxAttachment> (state, params::laneDirId (laneIndex),      directionBox);
    modeAttachment      = std::make_unique<ComboBoxAttachment> (state, params::laneModeId (laneIndex),     modeBox);
    ccOnAttachment      = std::make_unique<ButtonAttachment>   (state, params::laneCcOnId (laneIndex),     ccOnButton);
}

//==============================================================================
void LaneComponent::showActionsMenu()
{
    juce::PopupMenu menu;
    menu.setLookAndFeel (&getLookAndFeel());

    menu.addItem (1, "Rotate Left");
    menu.addItem (2, "Rotate Right");
    menu.addItem (3, "Invert Values");
    menu.addSeparator();
    menu.addItem (4, "Copy Pattern");
    menu.addItem (5, "Paste Pattern", clipboard.valid);

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

    g.setColour (theme::panel);
    g.fillRoundedRectangle (bounds, 6.0f);

    g.setColour (theme::outline);
    g.drawRoundedRectangle (bounds.reduced (0.5f), 6.0f, 1.0f);

    // Accent stripe down the left edge identifies the lane at a glance.
    g.setColour (accent.withAlpha (0.85f));
    g.fillRoundedRectangle (bounds.getX() + 4.0f, bounds.getY() + 10.0f, 3.0f,
                            bounds.getHeight() - 20.0f, 1.5f);
}

void LaneComponent::resized()
{
    auto r = getLocalBounds().reduced (12, 10);

    auto titleArea = r.removeFromLeft (52);
    titleLabel.setBounds (titleArea.removeFromTop (18));

    r.removeFromLeft (4);

    //--------------------------------------------------------------------------
    constexpr int slotWidth = 44;
    auto stepsArea = r.removeFromLeft (params::numSteps * slotWidth);

    for (auto* slot : slots)
        slot->setBounds (stepsArea.removeFromLeft (slotWidth).reduced (2, 0));

    r.removeFromLeft (16);

    //--------------------------------------------------------------------------
    // Three rows of four cells.
    constexpr int columns = 4;
    const int cellWidth = r.getWidth() / columns;
    const int rowHeight = r.getHeight() / 3;

    const auto placeCell = [] (juce::Rectangle<int> cell, juce::Label& caption, juce::Component& control)
    {
        cell = cell.reduced (6, 4);
        caption.setBounds (cell.removeFromTop (13));
        cell.removeFromTop (2);
        control.setBounds (cell.removeFromTop (juce::jmin (22, cell.getHeight())));
    };

    auto row1 = r.removeFromTop (rowHeight);
    auto row2 = r.removeFromTop (rowHeight);
    auto row3 = r;

    placeCell (row1.removeFromLeft (cellWidth), lengthLabel,    lengthSlider);
    placeCell (row1.removeFromLeft (cellWidth), divisionLabel,  divisionBox);
    placeCell (row1.removeFromLeft (cellWidth), directionLabel, directionBox);
    placeCell (row1,                            depthLabel,     depthSlider);

    placeCell (row2.removeFromLeft (cellWidth), modeLabel,      modeBox);
    placeCell (row2.removeFromLeft (cellWidth), nudgeLabel,     nudgeSlider);
    placeCell (row2.removeFromLeft (cellWidth), humanizeLabel,  humanizeSlider);
    placeCell (row2,                            ccOnLabel,      ccOnButton);

    placeCell (row3.removeFromLeft (cellWidth), ccNumberLabel,  ccNumberSlider);
    placeCell (row3.removeFromLeft (cellWidth), ccChannelLabel, ccChannelSlider);

    //--------------------------------------------------------------------------
    auto buttonCell = row3.removeFromLeft (cellWidth).reduced (6, 4);
    patternLabel.setBounds (buttonCell.removeFromTop (13));
    buttonCell.removeFromTop (2);

    auto buttonRow = buttonCell.removeFromTop (juce::jmin (22, buttonCell.getHeight()));
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
