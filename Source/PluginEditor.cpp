#include "PluginEditor.h"

namespace
{
    constexpr int laneHeight   = 150;
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
    headerCells.add (new ParamCell (state, params::freeRunId,    "Free Run"));

    for (auto* cell : headerCells)
        addAndMakeVisible (cell);

    theme::styleCaption (mixCaption, "Mix");
    addAndMakeVisible (mixCaption);
    addAndMakeVisible (mixMeter);

    for (int lane = 0; lane < params::numLanes; ++lane)
        addAndMakeVisible (lanes.add (new LaneComponent (state, lane)));

    outputSectionLabel.setText ("OUTPUT", juce::dontSendNotification);
    outputSectionLabel.setFont (theme::titleFont());
    outputSectionLabel.setColour (juce::Label::textColourId, theme::text);
    addAndMakeVisible (outputSectionLabel);

    outputCells.add (new ParamCell (state, params::rootNoteId,    "Root",       46));
    outputCells.add (new ParamCell (state, params::scaleId,       "Scale"));
    outputCells.add (new ParamCell (state, params::rangeStepsId,  "Range",      38));
    outputCells.add (new ParamCell (state, params::velocityId,    "Velocity",   38));
    outputCells.add (new ParamCell (state, params::gateLengthId,  "Gate",       50));
    outputCells.add (new ParamCell (state, params::offsetId,      "Offset",     52));
    outputCells.add (new ParamCell (state, params::slewId,        "Slew",       58));
    outputCells.add (new ParamCell (state, params::midiChannelId, "Note Chan",  38));
    outputCells.add (new ParamCell (state, params::ccNumberId,    "CC Number",  38));
    outputCells.add (new ParamCell (state, params::ccChannelId,   "CC Chan",    38));

    for (auto* cell : outputCells)
        addAndMakeVisible (cell);

    setSize (1010, 704);
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
    headerCells[2]->setBounds (header.removeFromLeft (90).reduced (6, 2));

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

    constexpr int columns = 5;
    const int rowHeight = panel.getHeight() / 2;
    const int cellWidth = panel.getWidth() / columns;

    auto topRow    = panel.removeFromTop (rowHeight);
    auto bottomRow = panel;

    for (int i = 0; i < outputCells.size(); ++i)
    {
        auto& row = (i < columns) ? topRow : bottomRow;
        const bool isLastInRow = ((i % columns) == columns - 1);

        auto cell = isLastInRow ? row : row.removeFromLeft (cellWidth);
        outputCells[i]->setBounds (cell.reduced (6, 4));
    }
}

//==============================================================================
void TriLaneAudioProcessorEditor::timerCallback()
{
    const auto& engine = processorRef.getEngine();

    for (int lane = 0; lane < lanes.size(); ++lane)
        lanes[lane]->setPlayingStep (engine.getCurrentStep (lane));

    mixMeter.setValue (engine.getMixValue());
}
