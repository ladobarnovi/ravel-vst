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

    /** Fills in the three tab pages. Split out only because listing 28 parameters inline
        buries the layout code in the constructor. */
    void buildTabs();

    /** Recomputes the natural (100%-zoom) size for the current lane count, updates the
        resize constrainer to match, and rescales the actual window to hold the user's
        current zoom level rather than snapping back to 100% -- which is what a plain
        setSize() here would do every time a lane is added or removed. */
    void updateSizeConstraints();

    /** The layout that used to live in resized(), now run against content's native bounds
        rather than the editor's actual (possibly zoomed) ones. */
    void layoutContent();

    // Declared first so it outlives every child that references it.
    RavelLookAndFeel lookAndFeel;

    RavelAudioProcessor& processorRef;

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

    // Shared by all lanes, so a pattern can be copied from one and pasted onto another.
    params::LanePattern patternClipboard;

    // Every lane is built up front, because their parameters exist up front; the count only
    // decides how many are shown and heard.
    juce::OwnedArray<LaneComponent> lanes;

    juce::TextButton addLaneButton { "+ Add lane" };

    /** Writes the new count through the parameter rather than straight into the members, so
        the host records it as an edit and the editor picks it up on the next tick like any
        other parameter change. */
    void setLaneCount (int newCount);

    /** Shows that many lanes, and resizes the window to fit. */
    void applyLaneCount (int newCount);

    /** Hides the per-step layer selector while Notes is off, where a lane is a plain value
        sequencer -- velocity and gate are only ever read on the note path. Applied from the
        constructor as well as the timer, so an editor opened with Notes off never shows the
        selector even briefly. */
    void applyNotesOn (bool notesOn);

    /** Takes out the lane a lane's own Remove button belongs to. Lives here rather than in
        LaneComponent because it is the stack that changes: every lane above this one moves
        down, and the window shrinks by one lane. */
    void removeLane (int laneIndex);

    std::atomic<float>* laneCountParam = nullptr;
    int laneCount = 0;

    // Output mode is the one global that changes what the plugin *is*, so it stays in the
    // header rather than going behind a tab with the rest of the setup.
    ControlGroup outputGroup;

    TabPage pitchPage, timingPage, routingPage;
    TabStrip tabs;

    // Non-owning; point into the tab pages. Dimmed when the current mode ignores them.
    ControlRow* scaleRow = nullptr;
    ControlRow* bendRangeRow = nullptr;
    ControlRow* quantizeRow = nullptr;
    ControlRow* triggerRow = nullptr;

    // Slew only ever smooths the CC output, so it goes dim whenever CC is off -- otherwise
    // it looks live while doing nothing.
    ControlRow* slewRow = nullptr;

    // One per lane, dimmed while the instance does not have that lane.
    ControlGroup* laneRoutingColumns[params::numLanes] {};

    std::atomic<float>* quantizeParam = nullptr;
    int lastQuantize = -1;

    // Watched alongside Quantize because the scale decides whether Bend Range is in play:
    // a 19-, 23-, 31-, 41- or 53-EDO scale rides on the wheel even with Quantize on.
    std::atomic<float>* scaleParam = nullptr;
    int lastScale = -1;

    std::atomic<float>* polyModeParam = nullptr;
    int lastPolyMode = -1;

    // Watched because it decides which per-step layers a lane can usefully edit: velocity
    // and gate are read only on the note path.
    std::atomic<float>* notesOnParam = nullptr;
    int lastNotesOn = -1;

    // Watched because it decides whether Slew is doing anything.
    std::atomic<float>* ccOnParam = nullptr;
    int lastCcOn = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RavelAudioProcessorEditor)
};
