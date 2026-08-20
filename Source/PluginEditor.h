#pragma once

#include "Controls.h"
#include "LaneComponent.h"
#include "PluginProcessor.h"
#include "Theme.h"

/** Read-out of the combined value the lanes currently produce. */
class MixMeter final : public juce::Component
{
public:
    void setValue (float newValue);
    void paint (juce::Graphics&) override;

private:
    float value = 0.0f;
};

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

    // Declared first so it outlives every child that references it.
    RavelLookAndFeel lookAndFeel;

    RavelAudioProcessor& processorRef;

    juce::Label titleLabel, mixCaption;

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

    /** Hides the per-step layer selector in CC mode, where a lane is a plain value
        sequencer. Applied from the constructor as well as the timer, so an editor opened
        in CC mode never shows the selector even briefly. */
    void applyOutputMode (int newMode);

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
    std::atomic<float>* outputModeParam = nullptr;
    int lastOutputMode = -1;

    MixMeter mixMeter;

    juce::Rectangle<int> panelArea;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RavelAudioProcessorEditor)
};
