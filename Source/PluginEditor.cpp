#include "PluginEditor.h"

namespace
{
    constexpr int laneHeight   = 168;
    constexpr int headerHeight = 40;
    constexpr int gap          = 8;
    constexpr int margin       = 12;
}

//==============================================================================
ParamCell::ParamCell (juce::AudioProcessorValueTreeState& state,
                      const juce::String& paramID,
                      const juce::String& caption,
                      int textBoxWidth)
{
    theme::styleCaption (captionLabel, caption);
    addAndMakeVisible (captionLabel);

    auto* param = state.getParameter (paramID);

    if (auto* choiceParam = dynamic_cast<juce::AudioParameterChoice*> (param))
    {
        auto box = std::make_unique<juce::ComboBox>();
        box->addItemList (choiceParam->choices, 1);

        comboAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
            state, paramID, *box);

        control = std::move (box);
    }
    else if (dynamic_cast<juce::AudioParameterBool*> (param) != nullptr)
    {
        auto button = std::make_unique<juce::ToggleButton>();
        button->setColour (juce::ToggleButton::tickColourId, theme::text);

        buttonAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
            state, paramID, *button);

        control = std::move (button);
    }
    else
    {
        auto slider = std::make_unique<juce::Slider>();
        slider->setSliderStyle (juce::Slider::LinearHorizontal);
        slider->setTextBoxStyle (juce::Slider::TextBoxRight, false, textBoxWidth, 18);
        slider->setColour (juce::Slider::trackColourId, theme::text.withAlpha (0.55f));

        sliderAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
            state, paramID, *slider);

        control = std::move (slider);
    }

    addAndMakeVisible (*control);
}

void ParamCell::resized()
{
    auto r = getLocalBounds();

    captionLabel.setBounds (r.removeFromTop (13));
    r.removeFromTop (2);

    if (control != nullptr)
        control->setBounds (r.removeFromTop (juce::jmin (22, r.getHeight())));
}

void ParamCell::setDimmed (bool shouldBeDimmed)
{
    if (dimmed == shouldBeDimmed)
        return;

    dimmed = shouldBeDimmed;

    const float alpha = dimmed ? 0.35f : 1.0f;
    captionLabel.setAlpha (alpha);

    if (control != nullptr)
    {
        control->setAlpha (alpha);
        control->setEnabled (! dimmed);
    }
}

//==============================================================================
void MixMeter::setValue (float newValue)
{
    if (std::abs (newValue - value) < 0.002f)
        return;

    value = newValue;
    repaint();
}

void MixMeter::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();

    g.setColour (theme::track);
    g.fillRoundedRectangle (bounds, 3.0f);

    const float width = bounds.getWidth() * juce::jlimit (0.0f, 1.0f, value);

    if (width > 1.0f)
    {
        g.setColour (theme::text.withAlpha (0.8f));
        g.fillRoundedRectangle (bounds.withWidth (width), 3.0f);
    }

    g.setColour (theme::outline);
    g.drawRoundedRectangle (bounds.reduced (0.5f), 3.0f, 1.0f);
}

//==============================================================================
TriLaneAudioProcessorEditor::TriLaneAudioProcessorEditor (TriLaneAudioProcessor& p)
    : AudioProcessorEditor (&p), processorRef (p)
{
    setLookAndFeel (&lookAndFeel);

    titleLabel.setText ("TRILANE", juce::dontSendNotification);
    titleLabel.setFont (juce::Font (juce::FontOptions (20.0f, juce::Font::bold)));
    titleLabel.setColour (juce::Label::textColourId, theme::text);
    addAndMakeVisible (titleLabel);

    auto& state = processorRef.apvts;

    headerCells.add (new ParamCell (state, params::outputModeId, "Output"));
    headerCells.add (new ParamCell (state, params::triggerSrcId, "Trigger"));
    headerCells.add (new ParamCell (state, params::swingId,      "Swing", 48));
    headerCells.add (new ParamCell (state, params::voiceCountId, "Voices", 30));
    headerCells.add (new ParamCell (state, params::freeRunId,    "Free Run"));

    for (auto* cell : headerCells)
        addAndMakeVisible (cell);

    theme::styleCaption (mixCaption, "Mix");
    addAndMakeVisible (mixCaption);
    addAndMakeVisible (mixMeter);

    for (int lane = 0; lane < params::numLanes; ++lane)
        addAndMakeVisible (lanes.add (new LaneComponent (state, lane, patternClipboard)));

    outputSectionLabel.setText ("OUTPUT", juce::dontSendNotification);
    outputSectionLabel.setFont (theme::titleFont());
    outputSectionLabel.setColour (juce::Label::textColourId, theme::text);
    addAndMakeVisible (outputSectionLabel);

    outputCells.add (new ParamCell (state, params::rootNoteId,    "Root",       46));
    scaleCell = outputCells.add (new ParamCell (state, params::scaleId, "Scale"));
    outputCells.add (new ParamCell (state, params::rangeStepsId,  "Range",      38));
    outputCells.add (new ParamCell (state, params::pitchModeId,   "Pitch"));

    outputCells.add (new ParamCell (state, params::bendRangeId,   "Bend Range", 44));
    outputCells.add (new ParamCell (state, params::velocityId,    "Velocity",   38));
    outputCells.add (new ParamCell (state, params::gateLengthId,  "Gate",       50));
    noteChannelCell = outputCells.add (new ParamCell (state, params::midiChannelId, "Note Chan", 38));

    outputCells.add (new ParamCell (state, params::offsetId,      "Offset",     52));
    outputCells.add (new ParamCell (state, params::slewId,        "Slew",       58));
    outputCells.add (new ParamCell (state, params::ccNumberId,    "Mix CC",     38));
    outputCells.add (new ParamCell (state, params::ccChannelId,   "Mix CC Chan", 38));

    for (auto* cell : outputCells)
        addAndMakeVisible (cell);

    // Polled on the timer rather than via a parameter listener, because listener
    // callbacks arrive on the audio thread and must not touch components.
    pitchModeParam = processorRef.apvts.getRawParameterValue (params::pitchModeId);

    setSize (1120, 794);
    startTimerHz (30);
}

TriLaneAudioProcessorEditor::~TriLaneAudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

//==============================================================================
void TriLaneAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (theme::background);

    if (! outputPanelArea.isEmpty())
    {
        const auto bounds = outputPanelArea.toFloat();

        g.setColour (theme::panel);
        g.fillRoundedRectangle (bounds, 6.0f);

        g.setColour (theme::outline);
        g.drawRoundedRectangle (bounds.reduced (0.5f), 6.0f, 1.0f);
    }
}

void TriLaneAudioProcessorEditor::resized()
{
    auto r = getLocalBounds().reduced (margin);

    //--------------------------------------------------------------------------
    auto header = r.removeFromTop (headerHeight);

    titleLabel.setBounds (header.removeFromLeft (120).withTrimmedTop (4));

    headerCells[0]->setBounds (header.removeFromLeft (160).reduced (6, 2));
    headerCells[1]->setBounds (header.removeFromLeft (140).reduced (6, 2));
    headerCells[2]->setBounds (header.removeFromLeft (150).reduced (6, 2));
    headerCells[3]->setBounds (header.removeFromLeft (110).reduced (6, 2));
    headerCells[4]->setBounds (header.removeFromLeft (90).reduced (6, 2));

    auto meterArea = header.removeFromRight (200).reduced (6, 2);
    mixCaption.setBounds (meterArea.removeFromTop (13));
    meterArea.removeFromTop (2);
    mixMeter.setBounds (meterArea.removeFromTop (18));

    r.removeFromTop (gap);

    //--------------------------------------------------------------------------
    for (auto* lane : lanes)
    {
        lane->setBounds (r.removeFromTop (laneHeight));
        r.removeFromTop (gap);
    }

    //--------------------------------------------------------------------------
    outputPanelArea = r;

    auto panel = r.reduced (12, 10);
    outputSectionLabel.setBounds (panel.removeFromTop (18));
    panel.removeFromTop (4);

    constexpr int columns = 4;
    const int rows = (outputCells.size() + columns - 1) / columns;
    const int cellWidth = panel.getWidth() / columns;
    const int rowHeight = rows > 0 ? panel.getHeight() / rows : panel.getHeight();

    for (int row = 0; row < rows; ++row)
    {
        auto rowArea = (row == rows - 1) ? panel : panel.removeFromTop (rowHeight);

        for (int column = 0; column < columns; ++column)
        {
            const int index = row * columns + column;

            if (index >= outputCells.size())
                break;

            auto cell = (column == columns - 1) ? rowArea : rowArea.removeFromLeft (cellWidth);
            outputCells[index]->setBounds (cell.reduced (6, 4));
        }
    }
}

//==============================================================================
void TriLaneAudioProcessorEditor::timerCallback()
{
    const auto& engine = processorRef.getEngine();

    for (int lane = 0; lane < lanes.size(); ++lane)
        lanes[lane]->setPlayingStep (engine.getCurrentStep (lane));

    mixMeter.setValue (engine.getMixValue());

    if (pitchModeParam != nullptr)
    {
        const auto pitchMode = (int) std::lround (pitchModeParam->load());

        if (pitchMode != lastPitchMode)
        {
            lastPitchMode = pitchMode;

            // Scale is bypassed by the continuous modes, and MPE allocates its own
            // channels rather than using Note Chan.
            scaleCell->setDimmed (params::isContinuousPitch (pitchMode));
            noteChannelCell->setDimmed (pitchMode == params::pitchMpe);
        }
    }
}
