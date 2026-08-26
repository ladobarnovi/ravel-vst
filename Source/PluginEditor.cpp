#include "PluginEditor.h"

namespace
{
    constexpr int laneHeight    = 140;
    constexpr int headerHeight  = 26;
    constexpr int laneBarHeight = 22;
    constexpr int panelHeight   = 152;
    constexpr int gap           = 8;
    constexpr int margin        = 12;

    // The window's native (100%-zoom) width: whatever a lane needs to draw 16 steps at
    // lane::stepSlotWidth, plus the margin either side. Derived rather than typed in, so
    // changing the step width moves the window with it instead of leaving a gap between
    // the last step and the parameter block. The user can zoom in or out from here; see
    // RavelAudioProcessorEditor::updateSizeConstraints().
    constexpr int nativeContentWidth = margin * 2 + lane::nativeWidth;

    // Square, and taller than a value row: the arrows are a click target rather than a line
    // of text, and 22px keeps them comfortably hittable inside a 26px header.
    constexpr int historyButton = 22;

    // How far the user can zoom the window either side of native size. Below 60% the step
    // bars stop being useful click targets; above 150% there's nothing left to reveal.
    constexpr double minZoom = 0.6;
    constexpr double maxZoom = 1.5;

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
RavelAudioProcessorEditor::RavelAudioProcessorEditor (RavelAudioProcessor& p)
    : AudioProcessorEditor (&p), processorRef (p),
      outputGroup (p.apvts), pitchPage (p.apvts), timingPage (p.apvts), routingPage (p.apvts)
{
    setLookAndFeel (&lookAndFeel);

    // Key presses bubble up from whichever child has focus, so this is what puts the editor
    // at the end of that chain for the undo shortcuts.
    setWantsKeyboardFocus (true);

    // content holds the actual UI at a fixed native pixel layout; the editor itself only
    // scales it to fill whatever size the user drags the window to. See resized().
    content.onResized = [this] { layoutContent(); };
    addAndMakeVisible (content);

    setConstrainer (&sizeConstrainer);
    setResizable (true, true);

    titleLabel.setText ("RAVEL", juce::dontSendNotification);
    titleLabel.setFont (theme::titleFont());
    titleLabel.setColour (juce::Label::textColourId, theme::text);
    titleLabel.setInterceptsMouseClicks (false, false);
    content.addAndMakeVisible (titleLabel);

    // The same two entry points the keyboard shortcuts use, so a click and a Ctrl+Z are the
    // same operation. Refreshed straight afterwards rather than left to the timer, so the
    // arrow that just emptied its stack greys out on the click that emptied it.
    undoButton.setTooltip ("Undo the last edit (Ctrl+Z)");
    theme::setRole (undoButton, theme::Role::undoArrow);
    undoButton.onClick = [this] { processorRef.undoHistory.undo(); updateHistoryButtons(); };
    content.addAndMakeVisible (undoButton);

    redoButton.setTooltip ("Redo the last undone edit (Ctrl+Shift+Z)");
    theme::setRole (redoButton, theme::Role::redoArrow);
    redoButton.onClick = [this] { processorRef.undoHistory.redo(); updateHistoryButtons(); };
    content.addAndMakeVisible (redoButton);

    updateHistoryButtons();

    auto& state = processorRef.apvts;

    // The two top-level mode switches: what the plugin emits, and whether the lanes are
    // mixed into one voice or run as three.
    outputGroup.add (params::outputModeId, "Output");
    outputGroup.add (params::polyModeId, "Poly")
               ->setTooltip ("Each lane triggers its own note off its own clock, instead of "
                             "the three mixing into one");
    outputGroup.setColumns (2);
    content.addAndMakeVisible (outputGroup);

    for (int lane = 0; lane < params::numLanes; ++lane)
    {
        auto* component = lanes.add (new LaneComponent (state, lane, patternClipboard));
        component->onRemove = [this, lane] { removeLane (lane); };

        content.addChildComponent (component);
    }

    addLaneButton.setTooltip ("Add a lane at the bottom of the stack. Each lane carries its "
                              "own Remove button");
    theme::styleActionButton (addLaneButton);
    addLaneButton.onClick = [this] { setLaneCount (laneCount + 1); };
    content.addChildComponent (addLaneButton);

    buildTabs();

    // Polled on the timer rather than via a parameter listener, because listener
    // callbacks arrive on the audio thread and must not touch components.
    quantizeParam   = state.getRawParameterValue (params::quantizeId);
    scaleParam      = state.getRawParameterValue (params::scaleId);
    polyModeParam   = state.getRawParameterValue (params::polyModeId);
    outputModeParam = state.getRawParameterValue (params::outputModeId);
    laneCountParam  = state.getRawParameterValue (params::laneCountId);

    // Sizes the window as a side effect, so this stands in for the setSize() a
    // fixed-height editor would do here. Leaves it at 100% zoom, which the restore below
    // then overrides if the session remembers something else.
    applyLaneCount ((int) std::lround (laneCountParam->load()));

    // Up front rather than left to the first timer tick, so an editor reopened on a session
    // already in CC mode never flashes a selector it is about to take away.
    applyOutputMode ((int) std::lround (outputModeParam->load()));

    // The zoom the window was last closed at, stored as plain properties on the state tree
    // rather than as a parameter: it's UI state, not something a host should automate or
    // recall through undo. Absent on a session saved before this existed, in which case the
    // 100% size applyLaneCount() just set is already correct.
    const int savedWidth  = (int) state.state.getProperty ("editorWidth",  0);
    const int savedHeight = (int) state.state.getProperty ("editorHeight", 0);

    if (savedWidth > 0 && savedHeight > 0)
    {
        const double savedScale = juce::jlimit (minZoom, maxZoom,
                                                 (double) savedHeight / (double) nativeContentHeight);

        setSize ((int) std::lround (nativeContentWidth  * savedScale),
                 (int) std::lround (nativeContentHeight * savedScale));
    }

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
    content.addAndMakeVisible (pitchPage);
    content.addChildComponent (timingPage);
    content.addChildComponent (routingPage);

    tabs.addTab ("Pitch",   pitchPage);
    tabs.addTab ("Timing",  timingPage);
    tabs.addTab ("Routing", routingPage);
    content.addAndMakeVisible (tabs);
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
    {
        lanes[lane]->setVisible (lane < laneCount);

        // Every lane can go except the last one standing, so this is a property of the count
        // rather than of which lane it is.
        lanes[lane]->setCanRemove (laneCount > 1);
    }

    // A lane the instance does not have has nothing to route, so its column reads as
    // unavailable instead of as three controls that quietly do nothing.
    for (int lane = 0; lane < params::numLanes; ++lane)
        if (laneRoutingColumns[lane] != nullptr)
            laneRoutingColumns[lane]->setDimmed (lane >= laneCount);

    addLaneButton.setVisible (laneCount < params::numLanes);

    // Every count maps to a different native height, so the constrainer and the window both
    // need to move to match -- at whatever zoom the user currently has, not back to 100%.
    updateSizeConstraints();
}

void RavelAudioProcessorEditor::applyOutputMode (int newMode)
{
    if (newMode == lastOutputMode)
        return;

    lastOutputMode = newMode;

    // In CC mode a lane is a plain value sequencer. Velocity and gate are unread there --
    // both are only ever arguments to startNote -- and the selector is not wanted for chance
    // alone, so it goes entirely and the bars edit Value. Chance does still gate the mix, and
    // so still shapes the CC stream, which is why the slots go on drawing its ticks.
    const bool selectableLayers = (newMode == params::outNotes);

    for (auto* lane : lanes)
        lane->setLayerSelectionAvailable (selectableLayers);
}

//==============================================================================
void RavelAudioProcessorEditor::removeLane (int laneIndex)
{
    // The shift and the new lane count are both written from inside this one callback, so the
    // history coalesces them into a single step and one Ctrl+Z puts the lane back.
    params::removeLane (processorRef.apvts, laneIndex);

    if (laneCountParam != nullptr)
        applyLaneCount ((int) std::lround (laneCountParam->load()));

    updateHistoryButtons();
}

//==============================================================================
void RavelAudioProcessorEditor::paint (juce::Graphics& g)
{
    // The panel background itself is drawn by content, in its own (unscaled) coordinate
    // space -- this fill is just the base coat, mostly covered but cheap insurance against
    // any rounding gap at content's scaled edges.
    g.fillAll (theme::background);
}

void RavelAudioProcessorEditor::updateSizeConstraints()
{
    // Preserves the user's current zoom across a lane-count change rather than snapping the
    // window back to 100% every time a lane is added or removed.
    const double previousScale = nativeContentHeight > 0 && getHeight() > 0
                                    ? (double) getHeight() / (double) nativeContentHeight
                                    : 1.0;

    nativeContentHeight = windowHeightForLanes (laneCount);

    // Locked so a drag-resize zooms uniformly rather than stretching bars into ellipses.
    sizeConstrainer.setFixedAspectRatio ((double) nativeContentWidth / (double) nativeContentHeight);
    sizeConstrainer.setSizeLimits (
        (int) std::lround (nativeContentWidth  * minZoom), (int) std::lround (nativeContentHeight * minZoom),
        (int) std::lround (nativeContentWidth  * maxZoom), (int) std::lround (nativeContentHeight * maxZoom));

    const double scale = juce::jlimit (minZoom, maxZoom, previousScale);

    setSize ((int) std::lround (nativeContentWidth  * scale),
             (int) std::lround (nativeContentHeight * scale));
}

void RavelAudioProcessorEditor::resized()
{
    if (nativeContentHeight <= 0)
        return; // Constructor hasn't run applyLaneCount() yet, so there's nothing to scale.

    // content stays at native pixel size always; only its transform changes, so none of its
    // children's layout math needs to know zoom exists.
    const float scale = (float) getWidth() / (float) nativeContentWidth;
    content.setTransform (juce::AffineTransform::scale (scale));
    content.setBounds (0, 0, nativeContentWidth, nativeContentHeight);

    // Persisted as plain state-tree properties (see the restore in the constructor) so the
    // window reopens at the size it was left, not back at 100%.
    processorRef.apvts.state.setProperty ("editorWidth",  getWidth(),  nullptr);
    processorRef.apvts.state.setProperty ("editorHeight", getHeight(), nullptr);
}

void RavelAudioProcessorEditor::layoutContent()
{
    auto r = content.getLocalBounds().reduced (margin);

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

    r.removeFromTop (gap);

    //--------------------------------------------------------------------------
    for (int lane = 0; lane < juce::jmin (laneCount, lanes.size()); ++lane)
    {
        lanes[lane]->setBounds (r.removeFromTop (laneHeight));
        r.removeFromTop (gap);
    }

    //--------------------------------------------------------------------------
    // Add sits where the next lane would go, so the button that makes a lane appear is
    // already standing in its place. Removing is a per-lane button now, inside the lane it
    // takes out, so nothing else shares this bar.
    auto laneBar = r.removeFromTop (laneBarHeight);
    addLaneButton.setBounds (laneBar.removeFromLeft (86));

    r.removeFromTop (gap);

    //--------------------------------------------------------------------------
    content.panelArea = r;

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

    if (outputModeParam != nullptr)
        applyOutputMode ((int) std::lround (outputModeParam->load()));

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
