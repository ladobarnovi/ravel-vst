#include "PluginEditor.h"

namespace
{
    constexpr int windowWidth  = 1000;
    constexpr int windowHeight = 654;

    constexpr int laneHeight   = 140;
    constexpr int headerHeight = 26;
    constexpr int gap          = 8;
    constexpr int margin       = 12;

    constexpr int meterWidth   = 210;
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
    const float corner = bounds.getHeight() * 0.5f;

    g.setColour (theme::track);
    g.fillRoundedRectangle (bounds, corner);

    const float width = bounds.getWidth() * juce::jlimit (0.0f, 1.0f, value);

    if (width > 1.0f)
    {
        g.setColour (theme::text.withAlpha (0.8f));
        g.fillRoundedRectangle (bounds.withWidth (juce::jmax (width, bounds.getHeight())), corner);
    }
}

//==============================================================================
TriLaneAudioProcessorEditor::TriLaneAudioProcessorEditor (TriLaneAudioProcessor& p)
    : AudioProcessorEditor (&p), processorRef (p),
      outputGroup (p.apvts), pitchPage (p.apvts), timingPage (p.apvts), routingPage (p.apvts)
{
    setLookAndFeel (&lookAndFeel);

    titleLabel.setText ("TRILANE", juce::dontSendNotification);
    titleLabel.setFont (theme::titleFont());
    titleLabel.setColour (juce::Label::textColourId, theme::text);
    titleLabel.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (titleLabel);

    auto& state = processorRef.apvts;

    // The two top-level mode switches: what the plugin emits, and whether the lanes are
    // mixed into one voice or run as three.
    outputGroup.add (params::outputModeId, "Output");
    outputGroup.add (params::polyModeId, "Poly")
               ->setTooltip ("Each lane triggers its own note off its own clock, instead of "
                             "the three mixing into one");
    outputGroup.setColumns (2);
    addAndMakeVisible (outputGroup);

    theme::styleHeading (mixCaption, "Mix");
    addAndMakeVisible (mixCaption);
    addAndMakeVisible (mixMeter);

    for (int lane = 0; lane < params::numLanes; ++lane)
        addAndMakeVisible (lanes.add (new LaneComponent (state, lane, patternClipboard)));

    buildTabs();

    // Polled on the timer rather than via a parameter listener, because listener
    // callbacks arrive on the audio thread and must not touch components.
    pitchModeParam = state.getRawParameterValue (params::pitchModeId);
    polyModeParam  = state.getRawParameterValue (params::polyModeId);

    setSize (windowWidth, windowHeight);
    startTimerHz (30);
}

TriLaneAudioProcessorEditor::~TriLaneAudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

//==============================================================================
void TriLaneAudioProcessorEditor::buildTabs()
{
    auto& notes = pitchPage.addColumn ("Notes");
    notes.add (params::rootNoteId,   "Root");
    scaleRow = notes.add (params::scaleId, "Scale");
    notes.add (params::rangeStepsId, "Range");
    notes.add (params::pitchModeId,  "Pitch");

    auto& bend = pitchPage.addColumn ("Bend");
    bendRangeRow = bend.add (params::bendRangeId, "Bend range");
    bend.add (params::offsetId, "Offset");
    bend.add (params::slewId,   "Slew");

    auto& voice = pitchPage.addColumn ("Voice");
    voice.add (params::velocityId,   "Velocity");
    voice.add (params::gateLengthId, "Gate");
    voice.add (params::voiceCountId, "Voices");

    //--------------------------------------------------------------------------
    auto& clock = timingPage.addColumn ("Clock");
    clock.add (params::swingId,   "Swing");
    clock.add (params::freeRunId, "Free run");
    triggerRow = clock.add (params::triggerSrcId, "Trigger");

    //--------------------------------------------------------------------------
    auto& global = routingPage.addColumn ("Notes and mix");
    noteChannelRow = global.add (params::midiChannelId, "Note channel");
    global.add (params::ccNumberId,  "Mix CC");
    global.add (params::ccChannelId, "Mix CC channel");

    for (int lane = 0; lane < params::numLanes; ++lane)
    {
        auto& column = routingPage.addColumn ("Lane " + juce::String (lane + 1));

        column.add (params::laneCcOnId (lane), "Send CC")
              ->setTooltip ("Send this lane's own value as CC, independent of Depth");
        column.add (params::laneCcNumId (lane),  "CC number");
        column.add (params::laneCcChanId (lane), "CC channel");
    }

    //--------------------------------------------------------------------------
    addAndMakeVisible (pitchPage);
    addChildComponent (timingPage);
    addChildComponent (routingPage);

    tabs.addTab ("Pitch",   pitchPage);
    tabs.addTab ("Timing",  timingPage);
    tabs.addTab ("Routing", routingPage);
    addAndMakeVisible (tabs);
}

//==============================================================================
void TriLaneAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (theme::background);

    if (! panelArea.isEmpty())
    {
        g.setColour (theme::panel);
        g.fillRoundedRectangle (panelArea.toFloat(), 6.0f);
    }
}

void TriLaneAudioProcessorEditor::resized()
{
    auto r = getLocalBounds().reduced (margin);

    //--------------------------------------------------------------------------
    auto header = r.removeFromTop (headerHeight);

    titleLabel.setBounds (header.removeFromLeft (92));
    header.removeFromLeft (12);

    outputGroup.setBounds (header.removeFromLeft (370)
                                 .withSizeKeepingCentre (370, theme::rowHeight));

    auto meterArea = header.removeFromRight (meterWidth).withSizeKeepingCentre (meterWidth, 10);
    mixCaption.setBounds (meterArea.removeFromLeft (28));
    mixMeter.setBounds (meterArea);

    r.removeFromTop (gap);

    //--------------------------------------------------------------------------
    for (auto* lane : lanes)
    {
        lane->setBounds (r.removeFromTop (laneHeight));
        r.removeFromTop (gap);
    }

    //--------------------------------------------------------------------------
    panelArea = r;

    auto panel = r.reduced (14, 8);
    tabs.setBounds (panel.removeFromTop (TabStrip::height));
    panel.removeFromTop (6);

    // Every page gets the same bounds; the tab strip decides which one is visible.
    pitchPage.setBounds (panel);
    timingPage.setBounds (panel);
    routingPage.setBounds (panel);
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

            // Scale is bypassed by the continuous modes, bend range is only sent by them,
            // and MPE allocates its own channels rather than using Note channel.
            scaleRow->setDimmed (params::isContinuousPitch (pitchMode));
            bendRangeRow->setDimmed (! params::isContinuousPitch (pitchMode));
            noteChannelRow->setDimmed (pitchMode == params::pitchMpe);
        }
    }

    if (polyModeParam != nullptr)
    {
        const int poly = polyModeParam->load() > 0.5f ? 1 : 0;

        if (poly != lastPolyMode)
        {
            lastPolyMode = poly;

            // In poly mode every lane triggers itself, so there is nothing for Trigger to
            // select. Mix mode and Depth are deliberately left alone: both still shape the
            // mix that drives the CC output, and Depth additionally becomes note velocity.
            triggerRow->setDimmed (poly != 0);
        }
    }
}
