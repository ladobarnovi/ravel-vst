#pragma once

#include "Controls.h"
#include "LaneComponent.h"
#include "PluginProcessor.h"
#include "Theme.h"

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
        void paint (juce::Graphics& g) override
        {
            if (! panelArea.isEmpty())
            {
                g.setColour (theme::panel);
                g.fillRoundedRectangle (panelArea.toFloat(), 6.0f);
            }
        }

        // resized() can't reach the outer editor's members directly, so it forwards to
        // layoutContent() instead of duplicating the layout here.
        void resized() override { if (onResized) onResized(); }

        std::function<void()> onResized;
        juce::Rectangle<int> panelArea;
    };

    ContentComponent content;

    // Constrains drag-resize to the content's aspect ratio (locked so the zoom is uniform
    // rather than stretching bars into ellipses) and to a sane zoom range either side of
    // native size. Recomputed in updateSizeConstraints() whenever the native size changes.
    juce::ComponentBoundsConstrainer sizeConstrainer;

    int nativeContentHeight = 0;

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
    juce::Component notesWorkspace, ccWorkspace;
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

    std::atomic<float>* quantizeParam = nullptr;
    int lastQuantize = -1;

    // Watched alongside Quantize because the scale decides whether Bend Range is in play:
    // a 19-, 23-, 31-, 41- or 53-EDO scale rides on the wheel even with Quantize on.
    std::atomic<float>* scaleParam = nullptr;
    int lastScale = -1;

    std::atomic<float>* polyModeParam = nullptr;
    int lastPolyMode = -1;

    // Global, not per-workspace -- routes both Note and CC output alike, so it sits in its own
    // row under the header rather than in either tab's settings page. Styled as a valueRow
    // ComboBox (see theme::Role) even though it has no APVTS parameter behind it: there is
    // nothing here for a host to automate or recall through undo, only an environment choice
    // that differs machine to machine -- see ExternalMidiOutput's own header.
    juce::ComboBox externalMidiBox;
    juce::TextButton externalMidiRescanButton { "Rescan" };

    // Parallel to the ComboBox's items from id 2 up (id 1 is the fixed "Host MIDI only"
    // entry): externalMidiDeviceIds[id - 2] is that item's device identifier.
    juce::StringArray externalMidiDeviceIds;

    /** Re-enumerates system MIDI outputs and rebuilds the ComboBox's item list, keeping
        whichever device is currently open selected if it's still in the list. Called once at
        startup and again on demand from the Rescan button -- a port created in loopMIDI after
        the plugin window opened otherwise never appears without reopening the editor. */
    void populateExternalMidiDevices();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RavelAudioProcessorEditor)
};
