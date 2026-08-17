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

    onButton.setColour (juce::ToggleButton::tickColourId, accent);
    addAndMakeVisible (onButton);

    valueAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        state, params::stepValueId (laneIndex, stepIndex), valueSlider);

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

    auto toggleArea = r.removeFromBottom (18);
    onButton.setBounds (toggleArea);

    r.removeFromBottom (4);
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
LaneComponent::LaneComponent (juce::AudioProcessorValueTreeState& state, int laneIndex)
    : apvts (state), lane (laneIndex), accent (theme::laneAccent (laneIndex))
{
    titleLabel.setText ("LANE " + juce::String (laneIndex + 1), juce::dontSendNotification);
    titleLabel.setFont (theme::titleFont());
    titleLabel.setColour (juce::Label::textColourId, accent);
    titleLabel.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (titleLabel);

    for (int step = 0; step < params::numSteps; ++step)
        addAndMakeVisible (slots.add (new StepSlot (state, laneIndex, step)));

    //--------------------------------------------------------------------------
    lengthSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    lengthSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 34, 18);
    lengthSlider.setColour (juce::Slider::trackColourId, accent.withAlpha (0.75f));
    addAndMakeVisible (lengthSlider);
    styleCaption (lengthLabel, "Length");
    addAndMakeVisible (lengthLabel);

    depthSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    depthSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 48, 18);
    depthSlider.setColour (juce::Slider::trackColourId, accent.withAlpha (0.75f));
    depthSlider.setDoubleClickReturnValue (true, 0.0);
    addAndMakeVisible (depthSlider);
    styleCaption (depthLabel, "Depth");
    addAndMakeVisible (depthLabel);

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
    using SliderAtt   = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboBoxAtt = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    lengthAttachment    = std::make_unique<SliderAtt>   (state, params::laneLengthId (laneIndex), lengthSlider);
    depthAttachment     = std::make_unique<SliderAtt>   (state, params::laneDepthId (laneIndex),  depthSlider);
    divisionAttachment  = std::make_unique<ComboBoxAtt> (state, params::laneDivId (laneIndex),    divisionBox);
    directionAttachment = std::make_unique<ComboBoxAtt> (state, params::laneDirId (laneIndex),    directionBox);
    modeAttachment      = std::make_unique<ComboBoxAtt> (state, params::laneModeId (laneIndex),   modeBox);
}

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
    // Two rows of three cells: length / rate / direction, then depth / mode.
    const int rowHeight = r.getHeight() / 2;
    auto topRow    = r.removeFromTop (rowHeight);
    auto bottomRow = r;

    const int cellWidth = r.getWidth() / 3;

    const auto placeCell = [] (juce::Rectangle<int> cell, juce::Label& caption, juce::Component& control)
    {
        cell = cell.reduced (6, 4);
        caption.setBounds (cell.removeFromTop (13));
        cell.removeFromTop (2);
        control.setBounds (cell.removeFromTop (juce::jmin (22, cell.getHeight())));
    };

    placeCell (topRow.removeFromLeft (cellWidth),    lengthLabel,    lengthSlider);
    placeCell (topRow.removeFromLeft (cellWidth),    divisionLabel,  divisionBox);
    placeCell (topRow,                               directionLabel, directionBox);

    placeCell (bottomRow.removeFromLeft (cellWidth), depthLabel,     depthSlider);
    placeCell (bottomRow.removeFromLeft (cellWidth), modeLabel,      modeBox);
}

void LaneComponent::setPlayingStep (int stepIndex)
{
    if (playingStep == stepIndex)
        return;

    playingStep = stepIndex;

    for (int i = 0; i < slots.size(); ++i)
        slots.getUnchecked (i)->setPlaying (i == stepIndex);
}
