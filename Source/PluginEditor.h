#pragma once

#include "Controls.h"
#include "ExternalMidiSelector.h"
#include "PresetBar.h"
#include "LaneComponent.h"
#include "PluginProcessor.h"
#include "RavelLookAndFeel.h"

#include <cmath>
#include <utility>

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
        component that does -- so without this the shortcuts would stay dead until something in
        the editor had been clicked. */
    void parentHierarchyChanged() override;

private:
    void timerCallback() override;

    /** Fills in each workspace's own settings footer. Split out only because listing this many
        parameters inline would bury the layout code in the constructor. */
    void buildWorkspaces();

    /** Recomputes the natural (100%-zoom) size for whichever workspace is currently selected,
        updates the resize constrainer to match, and rescales the actual window to hold the
        user's current zoom level rather than snapping back to 100% -- which is what a plain
        setSize() here would do every time a lane is added or removed, or the top-level tab is
        switched to a workspace with a different lane count. */
    void updateSizeConstraints();

    /** Lays out content against its native bounds rather than the editor's actual (possibly
        zoomed) ones -- resized() only rescales it. */
    void layoutContent();

    // Declared first so it outlives every child that references it.
    RavelLookAndFeel lookAndFeel;

    RavelAudioProcessor& processorRef;

    // Every setTooltip() call across this editor is inert without this -- SettableTooltipClient
    // only stores the string, this is what watches the mouse and actually pops the popup.
    // Scoped to `this` rather than the desktop-wide default so it dies with the editor instead
    // of a global mouse listener outliving a closed plugin window.
    juce::TooltipWindow tooltipWindow { this };

    /** Everything else is a child of this rather than of the editor itself, laid out at a fixed
        native pixel size. The editor scales it with an AffineTransform to fill whatever size
        the user has dragged the window to -- one transform on one component, rather than every
        constant in every child's resized() needing to know about zoom.

        Paints the header band and the wordmark in it. Those go here rather than in a component
        of their own because the header's controls are this component's children: a separate
        header component would either have to adopt them all or be painted over by them.
    */
    struct ContentComponent final : public juce::Component
    {
        void paint (juce::Graphics& g) override;

        // resized() can't reach the outer editor's members directly, so it forwards to
        // layoutContent() instead of duplicating the layout here.
        void resized() override { if (onResized) onResized(); }

        std::function<void()> onResized;

        juce::Rectangle<int> headerArea, markArea, wordmarkArea, dividerArea;
    };

    ContentComponent content;

    // Constrains drag-resize to the content's aspect ratio (locked so the zoom is uniform
    // rather than stretching bars into ellipses) and to a sane zoom range either side of native
    // size. Recomputed in updateSizeConstraints() whenever the native size changes.
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

    // Icon buttons, not text: these are the only two actions in the window that are not a
    // parameter, and the arrow pair is recognised at 22px where two words would not fit the
    // header. The button text is still set, for the accessibility layer to read.
    juce::TextButton undoButton { "Undo" }, redoButton { "Redo" };

    /** Enables each arrow only while there is something on that side of the history, so a
        Ctrl+Z that does nothing has already explained itself before it is pressed. */
    void updateHistoryButtons();

    // Last states actually applied. setEnabled repaints, and this is polled at 30Hz.
    int appliedCanUndo = -1, appliedCanRedo = -1;

    /** The header's preset pill, plus Save and Init. Its own component: which patch is loaded,
        how a browser menu is numbered and how a modal name prompt is kept alive are none of
        the editor's business. Polled from timerCallback() rather than running a timer of its
        own. */
    PresetBar presetBar { processorRef };

    /** A workspace: the lane stack, the add-lane bar, and the settings footer under both.

        Paints the footer's own band. Drawn by the workspace rather than by content because its
        position and height move with this workspace's own lane count, and content holds both
        workspaces at once.
    */
    struct WorkspaceComponent final : public juce::Component
    {
        void paint (juce::Graphics& g) override
        {
            if (settingsArea.isEmpty())
                return;

            g.setColour (theme::footer);
            g.fillRect (settingsArea);

            g.setColour (theme::outline);
            g.fillRect (settingsArea.withHeight (1));
        }

        juce::Rectangle<int> settingsArea;
    };

    /** One of the two stacks -- Notes or CC -- and everything the editor holds per stack.

        Indexed by LaneKind, which already runs through Parameters and the engine, so the
        editor draws the same distinction those layers draw rather than inventing a second one,
        and nothing in it has to name a stack twice to do a thing to both.
    */
    struct Stack
    {
        /** The settings page is the only thing in here that needs the parameter tree.
            Deliberately not explicit, so the editor's member-init list can build both stacks
            from one braced initialiser each. */
        Stack (juce::AudioProcessorValueTreeState& state) : settingsPage (state) {}

        WorkspaceComponent workspace;
        TabPage settingsPage;

        // Every lane in both stacks is built up front, because its parameters exist up front:
        // a VST3 cannot add parameters later. The count only decides how many of them are
        // shown and heard.
        juce::OwnedArray<LaneComponent> lanes;

        juce::TextButton addButton { "+ Add lane" };

        // One clipboard per stack: pasting a Note lane's pattern onto a CC lane (or the
        // reverse) is a cross-domain operation that doesn't mean anything -- a CC lane never
        // reads velocity or gate -- so the two kinds don't share one.
        params::LanePattern clipboard;

        std::atomic<float>* countParam = nullptr;
        int count = 0;
    };

    /** Indexed by LaneKind. The tab strip is built in the same order, so a kind and its tab
        index are the same number -- which is what lets applyLaneCount ask whether the stack it
        is changing is the one on screen without a second mapping. */
    Stack stacks[2];

    Stack& stackFor (params::LaneKind kind) noexcept { return stacks[(int) kind]; }

    /** Writes the new count through the parameter rather than straight into the member, so the
        host records it as an edit and the editor picks it up on the next tick like any other
        parameter change. */
    void setLaneCount (params::LaneKind kind, int newCount);

    /** Shows that many lanes of the given stack, and resizes the window to fit if that stack's
        workspace is the one currently selected. */
    void applyLaneCount (params::LaneKind kind, int newCount);

    /** Takes out the lane a lane's own Remove button belongs to. Lives here rather than in
        LaneComponent because it is the stack that changes: every lane above this one moves
        down, and the window shrinks by one lane. */
    void removeLane (params::LaneKind kind, int laneIndex);

    /** The lane-stack-plus-add-button-plus-footer layout, run once per stack against that
        stack's own bounds. Both stacks get this same shape, and lanes of both kinds are the
        same height, so it does not need to know which one it is laying out. */
    void layoutWorkspace (Stack& stack);

    TabStrip outputTabs;

    // Watched on the timer, the same way lane count and the output switches already are, so a
    // tab switch resizes the window through the same path a lane-count change does.
    int lastOutputTab = -1;

    // Non-owning; point into the Notes stack's settings page. Dimmed when the current mode
    // ignores them.
    ControlRow* scaleRow = nullptr;
    ControlRow* bendRangeRow = nullptr;
    ControlRow* triggerRow = nullptr;

    // Dimmed while MPE is on -- the zone fixes the channels then, not this parameter.
    ControlRow* noteChannelRow = nullptr;

    /** One column per CC lane in the CC page's footer, holding that lane's own destination --
        Send, Number, Channel, Offset.

        Here rather than on the lane strips. A destination is a property of where a lane goes
        rather than of the pattern in it: it is set once and then left, while everything on the
        strip is dialled while the sequencer runs. On the strip it would also make a CC lane's
        parameter block twice the height of a Note lane's, forcing the two workspaces to
        different lane heights for no reason the user could see.

        Non-owning; they belong to the CC stack's settings page. Held so the columns for lanes
        this instance does not currently have can be greyed rather than hidden -- the
        parameters still exist,
        and a column that vanishes and reappears as the lane count changes is harder to read
        than one that dims.
    */
    ControlGroup* ccLaneColumns[params::numLanes] {};
    int appliedCcLaneColumns = -1;

    /** A parameter read on the timer, and the value the UI was last brought into line with.

        Polled rather than listened to: a parameter listener's callback arrives on the audio
        thread and must not touch components. Every one of these decides whether some row is
        dimmed, and dimming repaints, so the work has to happen on the change rather than on
        each of the thirty ticks a second that find it unchanged.
    */
    struct Watched
    {
        std::atomic<float>* param = nullptr;

        /** Last value acted on. Starts at -1, which no parameter holds, so the first tick
            after the editor opens always counts as a change and sets the initial state. */
        int last = -1;

        /** True once per change, false on every tick in between. Rounded to the int the UI
            actually switches on: a bool parameter lands on 0 or 1, a choice on its index. */
        bool changed() noexcept
        {
            if (param == nullptr)
                return false;

            const int current = (int) std::lround (param->load());
            return std::exchange (last, current) != current;
        }
    };

    // Quantize and Scale are a pair: the scale decides whether Bend Range is in play, because
    // a 19-, 23-, 31-, 41- or 53-EDO scale rides on the wheel even with Quantize on. Either
    // one moving re-dims the same two rows.
    Watched quantize, pitchScale, polyMode, mpeEnabled;

    /** The header's MIDI-output chooser. Its own component: enumerating ports, remembering
        which one is open and laying out its own two controls are three things the editor has
        no reason to know about. */
    ExternalMidiSelector externalMidiSelector { processorRef };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RavelAudioProcessorEditor)
};
