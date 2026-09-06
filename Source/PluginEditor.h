#pragma once

#include "Controls.h"
#include "ExternalMidiSelector.h"
#include "PresetBar.h"
#include "LaneComponent.h"
#include "PluginProcessor.h"
#include "RavelLookAndFeel.h"

//==============================================================================
class RavelAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                          private juce::Timer
{
public:
    explicit RavelAudioProcessorEditor (RavelAudioProcessor&);
    ~RavelAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    /** Ctrl+Z / Ctrl+Shift+Z (Cmd on macOS), plus Ctrl+Y for the hosts whose users expect it. */
    bool keyPressed (const juce::KeyPress& key) override;

    /** Nothing holds keyboard focus when the window first opens, and key presses only reach a
        component that does -- so without this the shortcuts would stay dead until something
        in the editor had been clicked. */
    void parentHierarchyChanged() override;

private:
    void timerCallback() override;

    /** Fills in each workspace's own flat settings page. Split out only because listing this
        many parameters inline would bury the layout code in the constructor. */
    void buildWorkspaces();

    /** Recomputes the natural (100%-zoom) size for whichever workspace is currently
        selected, updates the resize constrainer to match, and rescales the actual window
        to hold the user's current zoom level rather than snapping back to 100% -- which is
        what a plain setSize() here would do every time a lane is added or removed, or the
        top-level tab is switched to a workspace with a different lane count. */
    void updateSizeConstraints();

    /** The layout that used to live in resized(), now run against content's native bounds
        rather than the editor's actual (possibly zoomed) ones. */
    void layoutContent();

    // Declared first so it outlives every child that references it.
    RavelLookAndFeel lookAndFeel;

    RavelAudioProcessor& processorRef;

    // Every setTooltip() call across this editor is inert without this -- SettableTooltipClient
    // only stores the string, this is what watches the mouse and actually pops the popup.
    // Scoped to `this` rather than the desktop-wide default so it dies with the editor instead
    // of a global mouse listener outliving a closed plugin window.
    juce::TooltipWindow tooltipWindow { this };

    /** Everything else is a child of this rather than of the editor itself, laid out at a
        fixed native pixel size. The editor scales it with an AffineTransform to fill
        whatever size the user has dragged the window to -- one transform on one component,
        rather than every constant in every child's resized() needing to know about zoom. */
    struct ContentComponent final : public juce::Component
    {
        // No paint() at all. The window is one surface, edge to edge -- the editor fills it --
        // and what sits above that surface says so by being raised rather than by having a
        // darker frame drawn around it. This used to draw the two header pills on their
        // owners' behalf; each of them now draws its own, which is why nothing is left here.

        // resized() can't reach the outer editor's members directly, so it forwards to
        // layoutContent() instead of duplicating the layout here.
        void resized() override { if (onResized) onResized(); }

        std::function<void()> onResized;
    };

    ContentComponent content;

    // Constrains drag-resize to the content's aspect ratio (locked so the zoom is uniform
    // rather than stretching bars into ellipses) and to a sane zoom range either side of
    // native size. Recomputed in updateSizeConstraints() whenever the native size changes.
    juce::ComponentBoundsConstrainer sizeConstrainer;

    int nativeContentHeight = 0;

    /** The window size rides in the state tree so it can be restored, but resized() is called
        for every frame of a drag -- and every write is a ValueTree property change with
        listeners hanging off it, and one the host can notice and mark the project edited for.
        So the size is recorded here instead and written once the drag has stopped moving; see
        the tick counter in timerCallback().
    */
    void storeEditorSize();

    bool editorSizeDirty = false;
    int  editorSizeSettleTicks = 0;

    /** Ticks of the 30 Hz timer a resize has to stand still for before it is written. Half a
        second: long enough that a drag writes once at the end rather than throughout, short
        enough that letting go and immediately closing the window still records it. */
    static constexpr int editorSizeSettleDelay = 15;

    juce::Label titleLabel;

    // Icon buttons, not text: these are the only two actions in the window that are not a
    // parameter, and the arrow pair is recognised at 22px where two words would not fit the
    // header. The button text is still set, for the accessibility layer to read.
    juce::TextButton undoButton { "Undo" }, redoButton { "Redo" };

    /** Enables each arrow only while there is something on that side of the history, so a
        Ctrl+Z that does nothing has already explained itself before it is pressed. */
    void updateHistoryButtons();

    // Last states actually applied. setEnabled repaints, and this is polled at 30Hz.
    int appliedCanUndo = -1, appliedCanRedo = -1;

    /** The header's preset pill. Its own component: which patch is loaded, how a browser
        menu is numbered and how a modal name prompt is kept alive were none of the editor's
        business. Polled from timerCallback() rather than running a timer of its own. */
    PresetBar presetBar { processorRef };

    // One clipboard per pool: pasting a Note lane's pattern onto a CC lane (or the reverse)
    // is a cross-domain operation that doesn't mean anything -- a CC lane never reads
    // velocity or gate -- so the two kinds don't share one.
    params::LanePattern noteClipboard, ccClipboard;

    // Every lane in both pools is built up front, because their parameters exist up front;
    // each pool's own count only decides how many of its own lanes are shown and heard.
    juce::OwnedArray<LaneComponent> noteLanes, ccLanes;

    juce::TextButton addNoteLaneButton { "+ Add lane" }, addCcLaneButton { "+ Add lane" };

    /** Writes the new count through the parameter rather than straight into the members, so
        the host records it as an edit and the editor picks it up on the next tick like any
        other parameter change. */
    void setNoteLaneCount (int newCount);
    void setCcLaneCount (int newCount);

    /** Shows that many lanes of the given pool, and resizes the window to fit if that pool's
        workspace is the one currently selected. */
    void applyNoteLaneCount (int newCount);
    void applyCcLaneCount (int newCount);

    /** Takes out the lane a lane's own Remove button belongs to. Lives here rather than in
        LaneComponent because it is the stack that changes: every lane above this one moves
        down, and the window shrinks by one lane. */
    void removeNoteLane (int laneIndex);
    void removeCcLane (int laneIndex);

    std::atomic<float>* noteLaneCountParam = nullptr;
    std::atomic<float>* ccLaneCountParam   = nullptr;
    int noteLaneCount = 0;
    int ccLaneCount   = 0;

    // The top-level split: which workspace -- Note lanes or CC lanes -- is currently shown.
    // Opens on Notes. Reuses TabStrip/TabPage exactly as the old Pitch/Timing/Routing strip
    // did; the difference is that a "page" here is a whole workspace (lane stack, add
    // button and settings) rather than only a settings column.
    /** A workspace's footer: the settings block below the lane stack. Raised off the
        window's surface the same as a lane is, rather than a hole cut into it or a bare
        continuation of the ground -- that's what marks it as its own section without
        needing a rule drawn at its top edge. Full window width and flush to the window's
        bottom edge, a bar rather than a card floating inside the outer margin (see
        layoutWorkspace()). Drawn by the workspace rather than by content because its
        position and height move with this workspace's own lane count, and content holds
        both workspaces at once.
    */
    struct WorkspaceComponent final : public juce::Component
    {
        void paint (juce::Graphics& g) override
        {
            if (settingsArea.isEmpty())
                return;

            g.setColour (theme::raised);
            g.fillRect (settingsArea);
        }

        juce::Rectangle<int> settingsArea;
    };

    WorkspaceComponent notesWorkspace, ccWorkspace;
    TabStrip outputTabs;

    // Watched on the timer, the same way lane count and the output switches already are, so
    // a tab switch resizes the window through the same path a lane-count change does.
    int lastOutputTab = -1;

    // Each workspace's own flat settings -- no further sub-tab strip underneath either, since
    // the top-level split already separates what Pitch/Timing/Routing used to.
    TabPage notesSettingsPage, ccSettingsPage;

    // Non-owning; point into notesSettingsPage. Dimmed when the current mode ignores them.
    ControlRow* scaleRow = nullptr;
    ControlRow* bendRangeRow = nullptr;
    ControlRow* triggerRow = nullptr;

    // Dimmed while MPE is on -- the zone fixes the channels then, not this parameter.
    ControlRow* noteChannelRow = nullptr;

    std::atomic<float>* quantizeParam = nullptr;
    int lastQuantize = -1;

    // Watched alongside Quantize because the scale decides whether Bend Range is in play:
    // a 19-, 23-, 31-, 41- or 53-EDO scale rides on the wheel even with Quantize on.
    std::atomic<float>* scaleParam = nullptr;
    int lastScale = -1;

    std::atomic<float>* polyModeParam = nullptr;
    int lastPolyMode = -1;

    std::atomic<float>* mpeEnabledParam = nullptr;
    int lastMpeEnabled = -1;

    /** The header's MIDI-output pill. Its own component now: enumerating ports, remembering
        which one is open and laying out its own two controls were three things the editor had
        no reason to know about. */
    ExternalMidiSelector externalMidiSelector { processorRef };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RavelAudioProcessorEditor)
};
