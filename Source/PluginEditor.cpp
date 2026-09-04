#include "PluginEditor.h"

namespace
{
    // Tall enough for the left column's four layer buttons (Value, Velocity, Prob, Gate)
    // plus the lane number above them; the step bars take whatever is left, so a taller
    // lane just makes them taller. Also covers a CC lane's own parameter block: Length/
    // Rate/Depth/Direction plus Send/Number/Channel/Offset is 8 rows on ControlGroup's
    // 2-column grid, taller than a Note lane's own four -- but the layer buttons set the
    // floor for both, so one constant still fits.
    constexpr int laneHeight    = 150;

    constexpr int headerHeight  = 26;
    constexpr int laneBarHeight = 22;
    constexpr int gap           = 8;
    constexpr int margin        = 12;

    // The External MIDI control's own height, inside its header pill. Same as laneBarHeight
    // -- it carries a button (Rescan) the same way that bar's Add lane does -- rather than
    // theme::rowHeight, which is sized for a bare caption-plus-value row with nothing beside
    // it.
    constexpr int externalMidiRowHeight = laneBarHeight;

    // Fixed rather than however much of the header the title and history arrows leave over:
    // a fixed width is what lets the pill sit flush against the header's right edge instead
    // of stretching to fill it. Wide enough for "MIDI output" as a caption plus "Host MIDI
    // only" as the longest stock choice, both at theme::rowFont.
    constexpr int externalMidiComboWidth = 210;
    constexpr int externalMidiPadding    = 10;

    // The window's native (100%-zoom) width: whatever a lane needs to draw 16 steps at
    // lane::stepSlotWidth, plus the margin either side. Derived rather than typed in, so
    // changing the step width moves the window with it instead of leaving a gap between
    // the last step and the parameter block. Both workspaces share it -- a CC lane's own
    // destination fields replace Note-only screen space rather than adding any -- so
    // neither tab is ever wider than the other. The user can zoom in or out from here; see
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
        resolution that makes them worth dragging. Also grows and shrinks with which
        workspace is selected -- a CC lane is taller than a Note lane, and each workspace's
        own settings panel is only as tall as its own columns need.
    */
    int windowHeightForWorkspace (int numActiveLanes, int laneHeightForKind, int settingsPanelHeight)
    {
        return margin * 2 + headerHeight + gap
                 + TabStrip::height + gap
                 + numActiveLanes * (laneHeightForKind + gap)
                 + laneBarHeight + gap
                 + settingsPanelHeight;
    }

    /** A settings page's own panel height: the reduced(14, 8) margin layoutContent() applies
        (8 top, 8 bottom) plus however tall its tallest column actually is -- there is no
        longer a sub-tab strip to add on top of that, since the top-level Notes/CC split
        already separates what Pitch/Timing/Routing used to.
    */
    int settingsPanelHeightFor (const TabPage& page)
    {
        return 16 + page.getPreferredHeight();
    }
}

//==============================================================================
RavelAudioProcessorEditor::RavelAudioProcessorEditor (RavelAudioProcessor& p)
    : AudioProcessorEditor (&p), processorRef (p),
      notesSettingsPage (p.apvts), ccSettingsPage (p.apvts)
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

    // Standing alone in its own header pill rather than among a grid of peers the way a
    // TabPage's value rows do, so it gets the Role that draws itself as an obvious dropdown
    // -- a boxed, arrowed value -- rather than valueRow's bare caption/value pair, which
    // leans on that grid to read as a control at all. See theme::Role::selectChip.
    theme::setRole (externalMidiBox, theme::Role::selectChip);
    theme::setCaption (externalMidiBox, "MIDI output");
    externalMidiBox.setTooltip ("Mirrors every note and CC this instance generates straight out "
                                "a system MIDI port -- a loopMIDI port, most likely -- bypassing "
                                "Ableton's own MIDI routing entirely. The host still receives "
                                "the same events as always; this only adds a second destination");
    externalMidiBox.onChange = [this]
    {
        const int id = externalMidiBox.getSelectedId();
        const juce::String identifier = id >= 2 && id - 2 < externalMidiDeviceIds.size()
                                            ? externalMidiDeviceIds[id - 2]
                                            : juce::String();

        processorRef.externalMidiOutput.setDevice (identifier);

        // Plain state-tree property rather than an APVTS parameter -- see the field's own
        // comment in PluginEditor.h for why -- restored in PluginProcessor::setStateInformation.
        processorRef.apvts.state.setProperty ("externalMidiDevice", identifier, nullptr);
    };
    content.addAndMakeVisible (externalMidiBox);

    theme::styleActionButton (externalMidiRescanButton);
    externalMidiRescanButton.setTooltip ("Re-scan for MIDI ports -- a loopMIDI port created "
                                         "after this window opened won't appear until this is "
                                         "clicked");
    externalMidiRescanButton.onClick = [this] { populateExternalMidiDevices(); };
    content.addAndMakeVisible (externalMidiRescanButton);

    populateExternalMidiDevices();

    auto& state = processorRef.apvts;

    //--------------------------------------------------------------------------
    // Note lanes and CC lanes are two completely separate stacks, each built up front for
    // the same reason the old single stack was: a VST3 cannot add parameters later, so both
    // pools exist at full size from the start and each pool's own count decides how many of
    // its own lanes are shown and heard.
    for (int lane = 0; lane < params::numLanes; ++lane)
    {
        auto* component = noteLanes.add (new LaneComponent (state, lane, noteClipboard, params::LaneKind::note));
        component->onRemove = [this, lane] { removeNoteLane (lane); };

        notesWorkspace.addChildComponent (component);
    }

    addNoteLaneButton.setTooltip ("Add a lane at the bottom of the stack. Each lane carries "
                                  "its own Remove button");
    theme::styleActionButton (addNoteLaneButton);
    addNoteLaneButton.onClick = [this] { setNoteLaneCount (noteLaneCount + 1); };
    notesWorkspace.addChildComponent (addNoteLaneButton);

    for (int lane = 0; lane < params::numLanes; ++lane)
    {
        auto* component = ccLanes.add (new LaneComponent (state, lane, ccClipboard, params::LaneKind::cc));
        component->onRemove = [this, lane] { removeCcLane (lane); };

        ccWorkspace.addChildComponent (component);
    }

    addCcLaneButton.setTooltip ("Add a lane at the bottom of the stack. Each lane carries "
                                "its own Remove button");
    theme::styleActionButton (addCcLaneButton);
    addCcLaneButton.onClick = [this] { setCcLaneCount (ccLaneCount + 1); };
    ccWorkspace.addChildComponent (addCcLaneButton);

    buildWorkspaces();

    content.addAndMakeVisible (notesWorkspace);
    content.addChildComponent (ccWorkspace);

    outputTabs.addTab ("Notes", notesWorkspace);
    outputTabs.addTab ("CC",    ccWorkspace);
    content.addAndMakeVisible (outputTabs);

    // Polled on the timer rather than via a parameter listener, because listener
    // callbacks arrive on the audio thread and must not touch components.
    quantizeParam    = state.getRawParameterValue (params::quantizeId);
    scaleParam       = state.getRawParameterValue (params::scaleId);
    polyModeParam    = state.getRawParameterValue (params::polyModeId);
    noteLaneCountParam = state.getRawParameterValue (params::noteLaneCountId);
    ccLaneCountParam   = state.getRawParameterValue (params::ccLaneCountId);

    // Sizes the window as a side effect, so this stands in for the setSize() a
    // fixed-height editor would do here. Leaves it at 100% zoom, which the restore below
    // then overrides if the session remembers something else. Notes is applied second so
    // its (default-selected) count is the one that actually triggers the resize.
    applyCcLaneCount ((int) std::lround (ccLaneCountParam->load()));
    applyNoteLaneCount ((int) std::lround (noteLaneCountParam->load()));

    // The zoom the window was last closed at, stored as plain properties on the state tree
    // rather than as a parameter: it's UI state, not something a host should automate or
    // recall through undo. Absent on a session saved before this existed, in which case the
    // 100% size applyNoteLaneCount() just set is already correct.
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
void RavelAudioProcessorEditor::populateExternalMidiDevices()
{
    const auto currentIdentifier = processorRef.externalMidiOutput.getCurrentDeviceIdentifier();

    externalMidiBox.clear (juce::dontSendNotification);
    externalMidiDeviceIds.clear();

    externalMidiBox.addItem ("Host MIDI only", 1);
    int selectedId = 1;

    for (const auto& info : juce::MidiOutput::getAvailableDevices())
    {
        externalMidiDeviceIds.add (info.identifier);
        const int itemId = externalMidiDeviceIds.size() + 1; // ids start at 2, list is 0-based

        externalMidiBox.addItem (info.name, itemId);

        if (info.identifier == currentIdentifier)
            selectedId = itemId;
    }

    // dontSendNotification: this reflects state the processor already has, so it must not
    // loop back through onChange and call setDevice() again.
    externalMidiBox.setSelectedId (selectedId, juce::dontSendNotification);
}

//==============================================================================
void RavelAudioProcessorEditor::buildWorkspaces()
{
    // No sub-tab strip under either page any more: the top-level Notes/CC split already
    // separates what Pitch/Timing/Routing used to, so what is left under each tab is short
    // enough to lay out as one flat row of columns.
    auto& pitch = notesSettingsPage.addColumn ("Pitch");
    pitch.add (params::rootNoteId,   "Root");
    scaleRow = pitch.add (params::scaleId, "Scale");
    scaleRow->setTooltip ("Scales named 19, 23, 31, 41 or 53 divide the octave into that many "
                          "equal steps. Their degrees land between the keys, so they play as a "
                          "note plus pitch bend -- one microtone at a time per channel");
    pitch.add (params::rangeOctavesId, "Range");
    pitch.add (params::quantizeId, "Quantize")
         ->setTooltip ("On: pitch snaps to the selected scale. Off: continuous microtonal "
                       "pitch, sent as a note plus pitch bend");

    auto& output = notesSettingsPage.addColumn ("Output");
    bendRangeRow = output.add (params::bendRangeId, "Bend range");
    bendRangeRow->setTooltip ("This property has to match your instrument's pitch bend range value");
    output.add (params::noteOffsetId, "Offset")
          ->setTooltip ("Transposes every note by whole octaves, after Root, Range and the "
                        "scale have resolved the pitch -- the pattern keeps its shape and "
                        "its scale degrees, it just moves. Notes clamp to the MIDI range");

    auto& voice = notesSettingsPage.addColumn ("Voice");
    voice.add (params::voiceCountId, "Voices");
    voice.add (params::polyModeId, "Poly")
         ->setTooltip ("In Poly mode each lane outputs its own independent note");

    auto& clock = notesSettingsPage.addColumn ("Clock");
    clock.add (params::swingId,   "Swing")
         ->setTooltip ("Delays every other step of the grid. Shared with the CC stack -- "
                       "both fold off the same host clock");
    clock.add (params::freeRunId, "Free run");
    triggerRow = clock.add (params::noteTriggerSrcId, "Trigger");

    notesWorkspace.addAndMakeVisible (notesSettingsPage);

    //--------------------------------------------------------------------------
    // The CC tab's own Mix destination: the CC-lane fold's output, same idea as pitch is the
    // Note-lane fold's output. Each CC lane's own destination lives on its own strip instead
    // of here -- see LaneComponent.
    auto& ccOutput = ccSettingsPage.addColumn ("Output");
    ccOutput.add (params::ccOnId,      "Send")
            ->setTooltip ("Turns the Mix CC on or off. Each CC lane's own Send is unaffected");
    ccOutput.add (params::ccNumberId,  "Number");
    ccOutput.add (params::ccChannelId, "Channel");
    ccOutput.add (params::ccOffsetId,  "Offset");
    ccOutput.add (params::slewId, "Slew")
            ->setTooltip ("Smooths the Mix CC and every CC lane's own tap. Never touches "
                         "pitch");

    // No Clock column here: Swing is shared with the Notes page, which is where it lives,
    // and Free run and Trigger were never CC concepts.
    ccWorkspace.addAndMakeVisible (ccSettingsPage);
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
void RavelAudioProcessorEditor::setNoteLaneCount (int newCount)
{
    newCount = juce::jlimit (1, params::numLanes, newCount);

    if (auto* parameter = processorRef.apvts.getParameter (params::noteLaneCountId))
    {
        // Through the parameter rather than straight into the member, so the host records
        // it as an edit and can automate and undo it like anything else.
        parameter->beginChangeGesture();
        parameter->setValueNotifyingHost (parameter->convertTo0to1 ((float) newCount));
        parameter->endChangeGesture();
    }

    applyNoteLaneCount (newCount);
}

void RavelAudioProcessorEditor::setCcLaneCount (int newCount)
{
    newCount = juce::jlimit (1, params::numLanes, newCount);

    if (auto* parameter = processorRef.apvts.getParameter (params::ccLaneCountId))
    {
        parameter->beginChangeGesture();
        parameter->setValueNotifyingHost (parameter->convertTo0to1 ((float) newCount));
        parameter->endChangeGesture();
    }

    applyCcLaneCount (newCount);
}

void RavelAudioProcessorEditor::applyNoteLaneCount (int newCount)
{
    newCount = juce::jlimit (1, params::numLanes, newCount);

    if (noteLaneCount == newCount)
        return;

    noteLaneCount = newCount;

    for (int lane = 0; lane < noteLanes.size(); ++lane)
    {
        noteLanes[lane]->setVisible (lane < noteLaneCount);

        // Every lane can go except the last one standing, so this is a property of the count
        // rather than of which lane it is.
        noteLanes[lane]->setCanRemove (noteLaneCount > 1);
    }

    addNoteLaneButton.setVisible (noteLaneCount < params::numLanes);

    // Only resizes the window if Notes is the workspace actually on screen -- a background
    // tab's count changing (through undo, or host automation) should not jump the window
    // the user is not even looking at.
    if (outputTabs.getSelectedIndex() == 0)
        updateSizeConstraints();
}

void RavelAudioProcessorEditor::applyCcLaneCount (int newCount)
{
    newCount = juce::jlimit (1, params::numLanes, newCount);

    if (ccLaneCount == newCount)
        return;

    ccLaneCount = newCount;

    for (int lane = 0; lane < ccLanes.size(); ++lane)
    {
        ccLanes[lane]->setVisible (lane < ccLaneCount);
        ccLanes[lane]->setCanRemove (ccLaneCount > 1);
    }

    addCcLaneButton.setVisible (ccLaneCount < params::numLanes);

    if (outputTabs.getSelectedIndex() == 1)
        updateSizeConstraints();
}

//==============================================================================
void RavelAudioProcessorEditor::removeNoteLane (int laneIndex)
{
    // The shift and the new lane count are both written from inside this one callback, so the
    // history coalesces them into a single step and one Ctrl+Z puts the lane back.
    params::removeLane (processorRef.apvts, laneIndex, params::LaneKind::note);

    if (noteLaneCountParam != nullptr)
        applyNoteLaneCount ((int) std::lround (noteLaneCountParam->load()));

    updateHistoryButtons();
}

void RavelAudioProcessorEditor::removeCcLane (int laneIndex)
{
    params::removeLane (processorRef.apvts, laneIndex, params::LaneKind::cc);

    if (ccLaneCountParam != nullptr)
        applyCcLaneCount ((int) std::lround (ccLaneCountParam->load()));

    updateHistoryButtons();
}

//==============================================================================
void RavelAudioProcessorEditor::paint (juce::Graphics& g)
{
    // The one ground the whole window sits on. content draws no rectangle of its own over
    // it any more, so this is not a base coat under a panel -- it is the surface, and it
    // reaches the window's edges rather than leaving a darker margin framing them.
    g.fillAll (theme::surface);
}

void RavelAudioProcessorEditor::updateSizeConstraints()
{
    // Preserves the user's current zoom across a lane-count change (or a tab switch) rather
    // than snapping the window back to 100% every time either happens.
    const double previousScale = nativeContentHeight > 0 && getHeight() > 0
                                    ? (double) getHeight() / (double) nativeContentHeight
                                    : 1.0;

    nativeContentHeight = outputTabs.getSelectedIndex() == 0
        ? windowHeightForWorkspace (noteLaneCount, laneHeight, settingsPanelHeightFor (notesSettingsPage))
        : windowHeightForWorkspace (ccLaneCount, laneHeight, settingsPanelHeightFor (ccSettingsPage));

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
        return; // Constructor hasn't run applyNoteLaneCount() yet, so there's nothing to scale.

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

namespace
{
    /** The lane-stack-plus-add-button-plus-footer layout, run once per workspace against
        that workspace's own bounds. Both get this same shape -- only the lane kind, lane
        height and settings page differ -- so it is written once rather than duplicated for
        Notes and for CC.

        A template only because WorkspaceComponent is private to the editor and this lives
        outside it; there is one instantiation, and it is the same code either way.
    */
    template <typename WorkspaceType>
    void layoutWorkspace (WorkspaceType& workspace, int activeLaneCount, int laneHeightForKind,
                         juce::OwnedArray<LaneComponent>& lanesArray, juce::TextButton& addButton,
                         TabPage& settingsPage)
    {
        // The workspace itself is full window width and runs to the window's bottom edge
        // (see layoutContent) so that the footer below can span edge to edge and sit flush
        // against the bottom -- a bar, not a card floating inside the outer margin. The lane
        // stack and add-lane bar are not the footer, so they get that margin back here,
        // horizontally, to stay aligned under the header and tab strip above them.
        auto r = workspace.getLocalBounds().reduced (margin, 0);

        for (int lane = 0; lane < juce::jmin (activeLaneCount, lanesArray.size()); ++lane)
        {
            lanesArray[lane]->setBounds (r.removeFromTop (laneHeightForKind));
            r.removeFromTop (gap);
        }

        // Add sits where the next lane would go, so the button that makes a lane appear is
        // already standing in its place. Removing is a per-lane button, inside the lane it
        // takes out, so nothing else shares this bar.
        auto laneBar = r.removeFromTop (laneBarHeight);
        addButton.setBounds (laneBar.removeFromLeft (86));

        r.removeFromTop (gap);

        // The footer: full workspace width (not r's margin-inset width) and down to the
        // workspace's own bottom edge, which is the window's bottom edge. See
        // WorkspaceComponent::paint(). Its fill goes edge to edge, but the settings page
        // inside it is inset by the same margin the lane stack and Add lane button use
        // above -- reduced(margin, ...) rather than r's own bounds, since r is already
        // margin-inset and reducing it again would double up -- so the Pitch/Output/Voice/
        // Clock columns line up with the lane cards and the button, not with the bar's own
        // wider edges.
        workspace.settingsArea = juce::Rectangle<int> (0, r.getY(),
                                                        workspace.getWidth(), workspace.getHeight() - r.getY());
        settingsPage.setBounds (workspace.settingsArea.reduced (margin, 8));
    }
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

    //--------------------------------------------------------------------------
    // Opposite the logo, flush against the header's right edge, on a raised pill rather than
    // flat on the surface -- see ContentComponent::paint(). Global rather than
    // per-workspace, so the header is where it belongs: it routes both Note and CC output
    // alike, not something either tab owns.
    const int externalMidiChipWidth = externalMidiPadding * 2 + externalMidiComboWidth + gap
                                        + theme::actionButtonWidth ("Rescan", externalMidiRowHeight);

    auto externalMidiChip = header.removeFromRight (externalMidiChipWidth);
    content.externalMidiArea = externalMidiChip;

    auto externalMidiInner = externalMidiChip.reduced (
        externalMidiPadding, (headerHeight - externalMidiRowHeight) / 2);

    externalMidiRescanButton.setBounds (externalMidiInner.removeFromRight (
        theme::actionButtonWidth ("Rescan", externalMidiRowHeight)));
    externalMidiInner.removeFromRight (gap);
    externalMidiBox.setBounds (externalMidiInner);

    r.removeFromTop (gap);

    //--------------------------------------------------------------------------
    outputTabs.setBounds (r.removeFromTop (TabStrip::height));
    r.removeFromTop (gap);

    // Full window width rather than r's margin-inset width, and down to content's own
    // bottom edge rather than stopping at r's bottom -- the workspace needs both so the
    // footer it lays out (see layoutWorkspace) can span edge to edge and sit flush against
    // the window's bottom. Both workspaces get the same bounds; the tab strip decides which
    // one is visible.
    juce::Rectangle<int> workspaceBounds (0, r.getY(), nativeContentWidth, nativeContentHeight - r.getY());
    notesWorkspace.setBounds (workspaceBounds);
    ccWorkspace.setBounds (workspaceBounds);

    layoutWorkspace (notesWorkspace, noteLaneCount, laneHeight, noteLanes, addNoteLaneButton, notesSettingsPage);
    layoutWorkspace (ccWorkspace, ccLaneCount, laneHeight, ccLanes, addCcLaneButton, ccSettingsPage);
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

    for (int lane = 0; lane < noteLanes.size(); ++lane)
        noteLanes[lane]->setPlayingStep (engine.getCurrentStep (lane, params::LaneKind::note));

    for (int lane = 0; lane < ccLanes.size(); ++lane)
        ccLanes[lane]->setPlayingStep (engine.getCurrentStep (lane, params::LaneKind::cc));

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

    // Polled like the rest: either count can also move through host automation or an undo,
    // neither of which comes back through the buttons.
    if (noteLaneCountParam != nullptr)
        applyNoteLaneCount ((int) std::lround (noteLaneCountParam->load()));

    if (ccLaneCountParam != nullptr)
        applyCcLaneCount ((int) std::lround (ccLaneCountParam->load()));

    // A tab switch resizes the window through the same path a lane-count change does, since
    // it really is a different lane count (and a different lane height, and a different
    // settings panel) coming on screen.
    const int currentTab = outputTabs.getSelectedIndex();

    if (currentTab != lastOutputTab)
    {
        lastOutputTab = currentTab;
        updateSizeConstraints();
    }

    if (polyModeParam != nullptr)
    {
        const int poly = polyModeParam->load() > 0.5f ? 1 : 0;

        if (poly != lastPolyMode)
        {
            lastPolyMode = poly;

            // In poly mode every lane triggers itself, so there is nothing for Trigger to
            // select. Depth is deliberately left alone: it still shapes the mix that drives
            // the CC output, and additionally becomes note velocity.
            triggerRow->setDimmed (poly != 0);
        }
    }
}
