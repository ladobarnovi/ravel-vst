#include "PluginEditor.h"

namespace
{
    constexpr int windowWidth   = 1000;

    constexpr int laneHeight    = 140;
    constexpr int headerHeight  = 26;
    constexpr int laneBarHeight = 22;
    constexpr int panelHeight   = 152;
    constexpr int gap           = 8;
    constexpr int margin        = 12;

    constexpr int meterWidth    = 210;

    // Square, and taller than a value row: the arrows are a click target rather than a line
    // of text, and 22px keeps them comfortably hittable inside a 26px header.
    constexpr int historyButton = 22;

    /** The window grows and shrinks with the lane count rather than the lanes sharing a
        fixed height between them: one lane in a window sized for four would be mostly empty
        panel, and four lanes squeezed into one lane's height would cost the step bars the
        resolution that makes them worth dragging.
    */
    int windowHeightForLanes (int numLanes)
    {
        return margin * 2 + headerHeight + gap
                 + numLanes * (laneHeight + gap)
                 + laneBarHeight + gap
                 + panelHeight;
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
RavelAudioProcessorEditor::RavelAudioProcessorEditor (RavelAudioProcessor& p)
    : AudioProcessorEditor (&p), processorRef (p),
      outputGroup (p.apvts), pitchPage (p.apvts), timingPage (p.apvts), routingPage (p.apvts)
{
    setLookAndFeel (&lookAndFeel);

    // Key presses bubble up from whichever child has focus, so this is what puts the editor
    // at the end of that chain for the undo shortcuts.
    setWantsKeyboardFocus (true);

    titleLabel.setText ("RAVEL", juce::dontSendNotification);
    titleLabel.setFont (theme::titleFont());
    titleLabel.setColour (juce::Label::textColourId, theme::text);
    titleLabel.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (titleLabel);

    // The same two entry points the keyboard shortcuts use, so a click and a Ctrl+Z are the
    // same operation. Refreshed straight afterwards rather than left to the timer, so the
    // arrow that just emptied its stack greys out on the click that emptied it.
    undoButton.setTooltip ("Undo the last edit (Ctrl+Z)");
    theme::setRole (undoButton, theme::Role::undoArrow);
    undoButton.onClick = [this] { processorRef.undoHistory.undo(); updateHistoryButtons(); };
    addAndMakeVisible (undoButton);

    redoButton.setTooltip ("Redo the last undone edit (Ctrl+Shift+Z)");
    theme::setRole (redoButton, theme::Role::redoArrow);
    redoButton.onClick = [this] { processorRef.undoHistory.redo(); updateHistoryButtons(); };
    addAndMakeVisible (redoButton);

    updateHistoryButtons();

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
        addChildComponent (lanes.add (new LaneComponent (state, lane, patternClipboard)));

    addLaneButton.setTooltip ("Add another lane. Lanes are added and removed at the bottom, "
                              "and a removed lane keeps its pattern");
    addLaneButton.onClick = [this] { setLaneCount (laneCount + 1); };
    addChildComponent (addLaneButton);

    removeLaneButton.setTooltip ("Remove the bottom lane. Its pattern is kept, and comes back "
                                 "with it");
    removeLaneButton.onClick = [this] { setLaneCount (laneCount - 1); };
    addChildComponent (removeLaneButton);

    buildTabs();

    // Polled on the timer rather than via a parameter listener, because listener
    // callbacks arrive on the audio thread and must not touch components.
    quantizeParam  = state.getRawParameterValue (params::quantizeId);
    scaleParam     = state.getRawParameterValue (params::scaleId);
    polyModeParam  = state.getRawParameterValue (params::polyModeId);
    laneCountParam = state.getRawParameterValue (params::laneCountId);

    // Sizes the window as a side effect, so this stands in for the setSize() a
    // fixed-height editor would do here.
    applyLaneCount ((int) std::lround (laneCountParam->load()));

    startTimerHz (30);
}

RavelAudioProcessorEditor::~RavelAudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

//==============================================================================
void RavelAudioProcessorEditor::buildTabs()
{
    auto& notes = pitchPage.addColumn ("Notes");
    notes.add (params::rootNoteId,   "Root");
    scaleRow = notes.add (params::scaleId, "Scale");
    scaleRow->setTooltip ("Scales named 19, 23, 31, 41 or 53 divide the octave into that many "
                          "equal steps. Their degrees land between the keys, so they play as a "
                          "note plus pitch bend -- one microtone at a time per channel");
    notes.add (params::rangeStepsId, "Range");
    quantizeRow = notes.add (params::quantizeId, "Quantize");
    quantizeRow->setTooltip ("On: pitch snaps to the selected scale. Off: continuous "
                             "microtonal pitch, sent as a note plus pitch bend");

    auto& bend = pitchPage.addColumn ("Bend");
    bendRangeRow = bend.add (params::bendRangeId, "Bend range");
    bend.add (params::offsetId, "Offset");
    bend.add (params::slewId,   "Slew");

    auto& voice = pitchPage.addColumn ("Voice");
    voice.add (params::velocityId,   "Velocity");
    voice.add (params::voiceCountId, "Voices");

    //--------------------------------------------------------------------------
    auto& clock = timingPage.addColumn ("Clock");
    clock.add (params::swingId,   "Swing");
    clock.add (params::freeRunId, "Free run");
    triggerRow = clock.add (params::triggerSrcId, "Trigger");

    //--------------------------------------------------------------------------
    auto& global = routingPage.addColumn ("Notes and mix");
    global.add (params::midiChannelId, "Note channel");
    global.add (params::ccNumberId,  "Mix CC");
    global.add (params::ccChannelId, "Mix CC channel");

    for (int lane = 0; lane < params::numLanes; ++lane)
    {
        auto& column = routingPage.addColumn ("Lane " + juce::String (lane + 1));
        laneRoutingColumns[lane] = &column;

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
void RavelAudioProcessorEditor::parentHierarchyChanged()
{
    if (isShowing() && ! hasKeyboardFocus (true))
        grabKeyboardFocus();
}

bool RavelAudioProcessorEditor::keyPressed (const juce::KeyPress& key)
{
    // commandModifier is Ctrl on Windows and Cmd on macOS, which is what each platform's
    // users reach for.
    if (! key.getModifiers().isCommandDown())
        return false;

    const bool undoKey = key.isKeyCode ('Z');
    const bool redoKey = key.isKeyCode ('Y');

    if (! undoKey && ! redoKey)
        return false;

    // Ctrl+Shift+Z is the other redo binding in wide use; both are accepted rather than
    // picking a side.
    if (redoKey || key.getModifiers().isShiftDown())
        processorRef.undoHistory.redo();
    else
        processorRef.undoHistory.undo();

    updateHistoryButtons();

    // Swallowed even with an empty history. Letting it fall through would hand the keystroke
    // to the host, so running out of steps in the plugin would silently start undoing the
    // arrangement instead -- a much worse surprise than a key that does nothing.
    return true;
}

//==============================================================================
void RavelAudioProcessorEditor::setLaneCount (int newCount)
{
    newCount = juce::jlimit (1, params::numLanes, newCount);

    if (auto* parameter = processorRef.apvts.getParameter (params::laneCountId))
    {
        // Through the parameter rather than straight into the member, so the host records
        // it as an edit and can automate and undo it like anything else.
        parameter->beginChangeGesture();
        parameter->setValueNotifyingHost (parameter->convertTo0to1 ((float) newCount));
        parameter->endChangeGesture();
    }

    applyLaneCount (newCount);
}

void RavelAudioProcessorEditor::applyLaneCount (int newCount)
{
    newCount = juce::jlimit (1, params::numLanes, newCount);

    if (laneCount == newCount)
        return;

    laneCount = newCount;

    for (int lane = 0; lane < lanes.size(); ++lane)
        lanes[lane]->setVisible (lane < laneCount);

    // A lane the instance does not have has nothing to route, so its column reads as
    // unavailable instead of as three controls that quietly do nothing.
    for (int lane = 0; lane < params::numLanes; ++lane)
        if (laneRoutingColumns[lane] != nullptr)
            laneRoutingColumns[lane]->setDimmed (lane >= laneCount);

    addLaneButton.setVisible (laneCount < params::numLanes);

    removeLaneButton.setVisible (laneCount > 1);
    removeLaneButton.setButtonText ("Remove lane " + juce::String (laneCount));

    // Every count maps to a different height, so this always lays the editor out again.
    setSize (windowWidth, windowHeightForLanes (laneCount));
}

//==============================================================================
void RavelAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (theme::background);

    if (! panelArea.isEmpty())
    {
        g.setColour (theme::panel);
        g.fillRoundedRectangle (panelArea.toFloat(), 6.0f);
    }
}

void RavelAudioProcessorEditor::resized()
{
    auto r = getLocalBounds().reduced (margin);

    //--------------------------------------------------------------------------
    auto header = r.removeFromTop (headerHeight);

    titleLabel.setBounds (header.removeFromLeft (92));
    header.removeFromLeft (12);

    // Grouped with the title rather than with the mode switches: undo acts on the whole
    // editor, and putting it in the row of parameters would read as one more parameter.
    auto history = header.removeFromLeft (historyButton * 2 + 4)
                         .withSizeKeepingCentre (historyButton * 2 + 4, historyButton);

    undoButton.setBounds (history.removeFromLeft (historyButton));
    history.removeFromLeft (4);
    redoButton.setBounds (history.removeFromLeft (historyButton));

    header.removeFromLeft (14);

    outputGroup.setBounds (header.removeFromLeft (370)
                                 .withSizeKeepingCentre (370, theme::rowHeight));

    auto meterArea = header.removeFromRight (meterWidth).withSizeKeepingCentre (meterWidth, 10);
    mixCaption.setBounds (meterArea.removeFromLeft (28));
    mixMeter.setBounds (meterArea);

    r.removeFromTop (gap);

    //--------------------------------------------------------------------------
    for (int lane = 0; lane < juce::jmin (laneCount, lanes.size()); ++lane)
    {
        lanes[lane]->setBounds (r.removeFromTop (laneHeight));
        r.removeFromTop (gap);
    }

    //--------------------------------------------------------------------------
    // The add/remove pair sits where the next lane would go, so the button that makes a
    // lane appear is already standing in its place.
    auto laneBar = r.removeFromTop (laneBarHeight);

    // Whichever buttons the current count leaves showing start at the left edge: a hidden
    // Add would otherwise leave a gap where it used to be.
    if (laneCount < params::numLanes)
    {
        addLaneButton.setBounds (laneBar.removeFromLeft (86));
        laneBar.removeFromLeft (6);
    }

    if (laneCount > 1)
        removeLaneButton.setBounds (laneBar.removeFromLeft (108));

    r.removeFromTop (gap);

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
void RavelAudioProcessorEditor::updateHistoryButtons()
{
    const int canUndo = processorRef.undoHistory.canUndo() ? 1 : 0;
    const int canRedo = processorRef.undoHistory.canRedo() ? 1 : 0;

    if (canUndo != appliedCanUndo)
    {
        appliedCanUndo = canUndo;
        undoButton.setEnabled (canUndo != 0);
    }

    if (canRedo != appliedCanRedo)
    {
        appliedCanRedo = canRedo;
        redoButton.setEnabled (canRedo != 0);
    }
}

//==============================================================================
void RavelAudioProcessorEditor::timerCallback()
{
    const auto& engine = processorRef.getEngine();

    // Polled, because anything at all that moves a parameter -- a step drag, a pattern
    // action, an automation gesture from the host -- puts an entry on the stack without
    // coming past the buttons.
    updateHistoryButtons();

    for (int lane = 0; lane < lanes.size(); ++lane)
        lanes[lane]->setPlayingStep (engine.getCurrentStep (lane));

    mixMeter.setValue (engine.getMixValue());

    if (quantizeParam != nullptr && scaleParam != nullptr)
    {
        const int quantize = quantizeParam->load() > 0.5f ? 1 : 0;
        const int scale    = (int) scaleParam->load();

        if (quantize != lastQuantize || scale != lastScale)
        {
            lastQuantize = quantize;
            lastScale    = scale;

            // Continuous pitch bypasses the scale entirely, so the row goes dim.
            scaleRow->setDimmed (quantize == 0);

            // Bend range is live wherever pitch can land between semitones: continuous mode,
            // and any scale in an EDO other than 12.
            bendRangeRow->setDimmed (quantize != 0 && ! params::scaleNeedsBend (scale));
        }
    }

    // Polled like the rest: the count can also move through host automation or an undo,
    // neither of which comes back through the buttons.
    if (laneCountParam != nullptr)
        applyLaneCount ((int) std::lround (laneCountParam->load()));

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
