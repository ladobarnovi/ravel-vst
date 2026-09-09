#include "PluginEditor.h"

namespace
{
    //--------------------------------------------------------------------------
    // The window is four stacked bands, edge to edge, with no outer margin: header, tab bar,
    // lane stack, settings footer. Each band carries its own ground colour and its own inner
    // padding, which is what separates them -- there is no gap between any two of them and no
    // frame drawn round anything.
    constexpr int headerHeight  = 43;   ///< 22px of controls, 10px above and below, 1px rule.
    constexpr int headerPadX    = 14;
    constexpr int headerGap     = 12;   ///< Between groups inside the header.
    constexpr int addLaneHeight = 40;

    // Square, and taller than a value row: the arrows are a click target rather than a line of
    // text, and 22px keeps them comfortably hittable inside the header.
    constexpr int historyButton = 22;

    /** The window's native (100%-zoom) width: whatever a lane needs to draw sixteen steps at
        lane::stepBarWidth. Derived rather than typed in, so changing the step width moves the
        window with it instead of leaving a gap between the last step and the parameter block.
        Both workspaces share it -- a CC lane reserves the layer selector's column rather than
        closing it up -- so neither tab is ever wider than the other. */
    constexpr int nativeContentWidth = lane::nativeWidth;

    // How far the user can zoom the window either side of native size. Below 60% the step bars
    // stop being useful click targets; above 150% there's nothing left to reveal.
    constexpr double minZoom = 0.6;
    constexpr double maxZoom = 1.5;

    //--------------------------------------------------------------------------
    // Settings-footer column widths, including each column's own 18px padding either side.
    // Fixed rather than a share of the window: the columns then land in the same places on both
    // tabs, and a row's caption stays near the value it names instead of being stretched away
    // from it.
    constexpr int settingsColumn     = 220;
    constexpr int settingsColumnWide = 250;   ///< Clock, whose rows carry a track and a read-out.
    constexpr int ccMixColumn        = 260;   ///< Mix CC, which carries a group break as well.
    constexpr int ccLaneColumn       = 200;

    /** The window grows and shrinks with the lane count rather than the lanes sharing a fixed
        height between them: one lane in a window sized for four would be mostly empty panel,
        and four lanes squeezed into one lane's height would cost the step bars the resolution
        that makes them worth dragging. Also grows and shrinks with which workspace is
        selected, since each one's footer is only as tall as its own columns need.
    */
    int windowHeightForWorkspace (int numActiveLanes, bool showingAddLane, int settingsPanelHeight)
    {
        return headerHeight
                 + TabStrip::height
                 + numActiveLanes * lane::height()
                 + (showingAddLane ? addLaneHeight : 0)
                 + settingsPanelHeight;
    }
}

//==============================================================================
void RavelAudioProcessorEditor::ContentComponent::paint (juce::Graphics& g)
{
    // The header's own band. A shallow vertical gradient rather than a flat fill: it is the one
    // band above the lane stack, and the gradient is what gives it an edge to sit on without a
    // second rule being drawn under the one already there.
    g.setGradientFill (juce::ColourGradient::vertical (theme::headerTop, (float) headerArea.getY(),
                                                       theme::headerBottom, (float) headerArea.getBottom()));
    g.fillRect (headerArea);

    g.setColour (theme::outline);
    g.fillRect (headerArea.withTop (headerArea.getBottom() - 1));

    // Four bars at four heights, one per lane accent -- the mark is the four lanes. Drawn
    // rather than shipped as an image so it follows theme::laneAccent instead of quietly
    // disagreeing with it.
    theme::drawLogoMark (g, markArea.toFloat());

    g.setFont (theme::wordmarkFont());
    g.setColour (theme::textBright);
    g.drawText ("Ravel", wordmarkArea, juce::Justification::centredLeft, false);

    // Separates the identity from the controls. The header runs left to right from what this
    // plugin *is*, through what the patch is, to where its output goes -- and this is the only
    // one of those joins that needs marking, because the other two are separated by space.
    g.setColour (theme::outline);
    g.fillRect (dividerArea);
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

    content.addAndMakeVisible (externalMidiSelector);

    content.addAndMakeVisible (presetBar);

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

        // A value painted across several steps in one stroke is one thing the user did, and
        // steps back in one press. The history is the processor's, which is why the lane has
        // to be told rather than reaching for it.
        component->onStrokeActive = [this] (bool active)
                                    { processorRef.undoHistory.setEditHeldOpen (active); };

        notesWorkspace.addChildComponent (component);
    }

    addNoteLaneButton.setTooltip ("Add a lane at the bottom of the stack. Each lane carries "
                                  "its own Remove button");
    theme::setRole (addNoteLaneButton, theme::Role::addLane);
    addNoteLaneButton.onClick = [this] { setNoteLaneCount (noteLaneCount + 1); };
    notesWorkspace.addChildComponent (addNoteLaneButton);

    for (int lane = 0; lane < params::numLanes; ++lane)
    {
        auto* component = ccLanes.add (new LaneComponent (state, lane, ccClipboard, params::LaneKind::cc));
        component->onRemove = [this, lane] { removeCcLane (lane); };

        component->onStrokeActive = [this] (bool active)
                                    { processorRef.undoHistory.setEditHeldOpen (active); };

        ccWorkspace.addChildComponent (component);
    }

    addCcLaneButton.setTooltip ("Add a lane at the bottom of the stack. Each lane carries "
                                "its own Remove button");
    theme::setRole (addCcLaneButton, theme::Role::addLane);
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
    mpeEnabledParam  = state.getRawParameterValue (params::mpeEnabledId);
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

    // Whatever the debounce above was still holding. Closing the window is the one moment the
    // size is certain not to change again, and the timer is not going to fire now.
    storeEditorSize();

    setLookAndFeel (nullptr);
}

//==============================================================================
void RavelAudioProcessorEditor::buildWorkspaces()
{
    auto& pitch = notesSettingsPage.addColumn ("Pitch", settingsColumn);
    pitch.add (params::rootNoteId, "Root");
    scaleRow = pitch.add (params::scaleId, "Scale");
    scaleRow->setTooltip ("Scales named 19, 23, 31, 41 or 53 divide the octave into that many "
                          "equal steps. Their degrees land between the keys, so they play as a "
                          "note plus pitch bend -- one microtone at a time per channel");
    pitch.add (params::rangeOctavesId, "Range");
    pitch.add (params::quantizeId, "Quantize")
         ->setTooltip ("On: pitch snaps to the selected scale. Off: continuous microtonal "
                       "pitch, sent as a note plus pitch bend");

    auto& output = notesSettingsPage.addColumn ("Output", settingsColumn);
    bendRangeRow = output.add (params::bendRangeId, "Bend range");
    bendRangeRow->setTooltip ("Has to match the instrument's own pitch bend range, or continuous "
                              "pitch plays the wrong interval. With MPE on the instrument ignores "
                              "its own setting and uses the MPE default of " + juce::String::charToString (0x00b1) + "48, "
                              "so leave this at 48 unless you turn MPE off");

    // Whole octaves, -3 to +3: seven positions rather than a continuum, so it gets seven cells
    // lighting out from a marked centre instead of a track. See RowStyle::octaves.
    output.add (params::noteOffsetId, "Offset", RowStyle::octaves)
          ->setTooltip ("Transposes every note by whole octaves, after Root, Range and the "
                        "scale have resolved the pitch -- the pattern keeps its shape and its "
                        "scale degrees, it just moves. Notes clamp to the MIDI range");

    output.add (params::mpeEnabledId, "MPE")
          ->setTooltip ("Gives every simultaneously-sounding note its own MIDI channel -- a "
                        "standard MPE zone, master channel 1 plus member channels 2-16 -- so "
                        "overlapping notes bend independently instead of sharing one wheel. "
                        "Channel is unused while this is on");
    noteChannelRow = output.add (params::midiChannelId, "Channel");
    noteChannelRow->setTooltip ("The single channel every note goes out on with MPE off. An MPE "
                               "zone fixes its own channels, so this does nothing while MPE is on");

    auto& voice = notesSettingsPage.addColumn ("Voice", settingsColumn);
    voice.add (params::voiceCountId, "Voices");
    voice.add (params::polyModeId, "Poly")
         ->setTooltip ("In Poly mode each lane outputs its own independent note");

    auto& clock = notesSettingsPage.addColumn ("Clock", settingsColumnWide);
    clock.add (params::swingId, "Swing", RowStyle::slider)
         ->setTooltip ("Delays every other step of the grid. Shared with the CC stack -- both "
                       "fold off the same host clock");
    clock.add (params::freeRunId, "Free run");
    triggerRow = clock.add (params::noteTriggerSrcId, "Trigger");

    notesWorkspace.addAndMakeVisible (notesSettingsPage);

    //--------------------------------------------------------------------------
    // The CC tab's own Mix destination: the CC-lane fold's output, same idea as pitch is the
    // Note-lane fold's output.
    auto& ccMix = ccSettingsPage.addColumn ("Mix CC", ccMixColumn);
    ccMix.add (params::ccOnId, "Send")
         ->setTooltip ("Turns the Mix CC on or off. Each CC lane's own Send is unaffected");
    ccMix.add (params::ccNumberId,  "Number");
    ccMix.add (params::ccChannelId, "Channel");
    ccMix.add (params::ccOffsetId,  "Offset", RowStyle::slider);

    // Slew is not strictly the Mix CC's -- it smooths every CC this plugin sends, each lane's
    // own tap included -- but it sits in this column all the same: it is the only global CC
    // control there is, and a rule and a heading to say so cost more attention than the
    // distinction is worth. The tooltip carries it.
    ccMix.add (params::slewId, "Slew", RowStyle::slider)
         ->setTooltip ("Smooths the Mix CC and every CC lane's own tap. Never touches pitch");

    // One column per CC lane, carrying the destination that used to sit on the lane strip. See
    // ccLaneColumns in the header for why it moved.
    for (int laneIndex = 0; laneIndex < params::numLanes; ++laneIndex)
    {
        auto& column = ccSettingsPage.addColumn ("Lane " + juce::String (laneIndex + 1), ccLaneColumn);

        // The same accent the lane's rail and step bars carry, which is what ties a column at
        // the bottom of the window to a strip at the top without either repeating the other.
        column.setHeadingAccent (theme::laneAccent (laneIndex));

        column.add (params::laneCcOnId (laneIndex), "Send")
              ->setTooltip ("Send this lane's own value as its own CC, independent of the Mix CC");
        column.add (params::laneCcNumId (laneIndex),  "Number");
        column.add (params::laneCcChanId (laneIndex), "Channel");
        column.add (params::laneCcOffsetId (laneIndex), "Offset", RowStyle::narrowSlider)
              ->setTooltip ("Raises the floor of this lane's own tap -- step values then span "
                            "what is left above it, so the total never passes 100%. "
                            "Independent of the Mix CC's Offset, which shifts the fold instead");

        ccLaneColumns[laneIndex] = &column;
    }

    // No Clock column here: Swing is shared with the Notes page, which is where it lives, and
    // Free run and Trigger were never CC concepts.
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

    // The footer keeps a column for every lane the plugin *could* have, and greys the ones this
    // instance does not currently have rather than taking them away. Their parameters still
    // exist -- a VST3 cannot add parameters later, so all four are always there -- and a column
    // that vanishes and reappears as the lane count changes is harder to read than one that
    // dims in place, because everything to its right would shuffle sideways each time.
    if (appliedCcLaneColumns != ccLaneCount)
    {
        appliedCcLaneColumns = ccLaneCount;

        for (int laneIndex = 0; laneIndex < params::numLanes; ++laneIndex)
            if (ccLaneColumns[laneIndex] != nullptr)
                ccLaneColumns[laneIndex]->setDimmed (laneIndex >= ccLaneCount);
    }

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
        ? windowHeightForWorkspace (noteLaneCount, addNoteLaneButton.isVisible(),
                                     notesSettingsPage.getPreferredHeight())
        : windowHeightForWorkspace (ccLaneCount, addCcLaneButton.isVisible(),
                                     ccSettingsPage.getPreferredHeight());

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

    // Not written here: this runs on every frame of a drag-resize. Marked instead, and written
    // by the timer once the size has stopped moving -- see storeEditorSize().
    editorSizeDirty = true;
    editorSizeSettleTicks = 0;
}

void RavelAudioProcessorEditor::storeEditorSize()
{
    if (! editorSizeDirty)
        return;

    editorSizeDirty = false;

    // Persisted as plain state-tree properties (see the restore in the constructor) so the
    // window reopens at the size it was left, not back at 100%.
    processorRef.apvts.state.setProperty ("editorWidth",  getWidth(),  nullptr);
    processorRef.apvts.state.setProperty ("editorHeight", getHeight(), nullptr);
}

namespace
{
    /** The lane-stack-plus-add-button-plus-footer layout, run once per workspace against that
        workspace's own bounds. Both get this same shape -- only the lane stack and the
        settings page differ -- so it is written once rather than duplicated for Notes and for
        CC. Lanes of both kinds are the same height, so this does not need to know which it is
        laying out.

        A template only because WorkspaceComponent is private to the editor and this lives
        outside it; there is one instantiation, and it is the same code either way.
    */
    template <typename WorkspaceType>
    void layoutWorkspace (WorkspaceType& workspace, int activeLaneCount,
                          juce::OwnedArray<LaneComponent>& lanesArray, juce::TextButton& addButton,
                          TabPage& settingsPage)
    {
        // Full width, no outer margin: a lane is a row of a list that runs edge to edge, and it
        // carries its own padding and its own bottom hairline. See LaneComponent::paint().
        auto r = workspace.getLocalBounds();

        const int laneHeight = lane::height();

        for (int laneIndex = 0; laneIndex < juce::jmin (activeLaneCount, lanesArray.size()); ++laneIndex)
            lanesArray[laneIndex]->setBounds (r.removeFromTop (laneHeight));

        // Add sits where the next lane would go, so the button that makes a lane appear is
        // already standing in its place -- and spans the full width, because that is the shape
        // of the thing it adds. Removing is a per-lane button inside the lane it takes out, so
        // nothing else shares this bar.
        if (addButton.isVisible())
            addButton.setBounds (r.removeFromTop (addLaneHeight));

        // The footer: full workspace width and down to the workspace's own bottom edge, which
        // is the window's bottom edge. Its band goes edge to edge (see
        // WorkspaceComponent::paint) while the columns inside it carry their own padding.
        workspace.settingsArea = r;
        settingsPage.setBounds (r);
    }
}

void RavelAudioProcessorEditor::layoutContent()
{
    auto r = content.getLocalBounds();

    //--------------------------------------------------------------------------
    // The header band runs edge to edge; its contents are inset from it.
    content.headerArea = r.removeFromTop (headerHeight);

    auto header = content.headerArea.withTrimmedBottom (1).reduced (headerPadX, 0);

    // The mark and the wordmark together are the identity: drawn by content, not laid out as
    // components, because neither of them is clickable.
    content.markArea = header.removeFromLeft ((int) theme::logoMarkWidth)
                             .withSizeKeepingCentre ((int) theme::logoMarkWidth, 17);
    header.removeFromLeft (8);

    const int wordmarkWidth = (int) std::ceil (
        juce::GlyphArrangement::getStringWidth (theme::wordmarkFont(), "Ravel"));

    content.wordmarkArea = header.removeFromLeft (wordmarkWidth);
    header.removeFromLeft (headerGap);

    content.dividerArea = header.removeFromLeft (1).withSizeKeepingCentre (1, 20);
    header.removeFromLeft (headerGap);

    //--------------------------------------------------------------------------
    // Grouped with the identity rather than with the patch controls: undo acts on the whole
    // editor, and putting it beside Save would read as one more preset action.
    auto history = header.removeFromLeft (historyButton * 2 + 4)
                         .withSizeKeepingCentre (historyButton * 2 + 4, historyButton);

    undoButton.setBounds (history.removeFromLeft (historyButton));
    history.removeFromLeft (4);
    redoButton.setBounds (history.removeFromLeft (historyButton));

    header.removeFromLeft (headerGap);

    //--------------------------------------------------------------------------
    // Fixed width and left-aligned rather than stretched to fill what the wordmark leaves over
    // -- the empty header between this and the MIDI chooser is what keeps the header's two
    // groups reading as two groups.
    presetBar.setBounds (header.removeFromLeft (PresetBar::preferredWidth()));

    // Opposite the mark, flush against the header's right edge. Global rather than
    // per-workspace, so the header is where it belongs: it routes both Note and CC output
    // alike, not something either tab owns.
    externalMidiSelector.setBounds (header.removeFromRight (ExternalMidiSelector::preferredWidth()));

    //--------------------------------------------------------------------------
    outputTabs.setBounds (r.removeFromTop (TabStrip::height));

    // Both workspaces get the same bounds -- everything left under the tab strip, down to the
    // window's bottom edge. The tab strip decides which one is visible.
    notesWorkspace.setBounds (r);
    ccWorkspace.setBounds (r);

    layoutWorkspace (notesWorkspace, noteLaneCount, noteLanes, addNoteLaneButton, notesSettingsPage);
    layoutWorkspace (ccWorkspace, ccLaneCount, ccLanes, addCcLaneButton, ccSettingsPage);
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
    // A resize that has stood still for half a second is a resize the user has finished.
    if (editorSizeDirty && ++editorSizeSettleTicks >= editorSizeSettleDelay)
        storeEditorSize();

    const auto& engine = processorRef.getEngine();

    // Polled, because anything at all that moves a parameter -- a step drag, a pattern
    // action, an automation gesture from the host -- puts an entry on the stack without
    // coming past the buttons.
    updateHistoryButtons();

    // Same reasoning for the preset chip's edited dot, with one addition: the parameter that
    // makes a patch dirty can move on the audio thread, so the PresetManager only ever sets an
    // atomic flag and this is where it reaches the UI.
    presetBar.tick();

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

    if (mpeEnabledParam != nullptr)
    {
        const int mpe = mpeEnabledParam->load() > 0.5f ? 1 : 0;

        if (mpe != lastMpeEnabled)
        {
            lastMpeEnabled = mpe;

            // The zone fixes its own channels -- master 1, members 2-16 -- while MPE is on,
            // so Channel has nothing left to select.
            noteChannelRow->setDimmed (mpe != 0);
        }
    }
}
