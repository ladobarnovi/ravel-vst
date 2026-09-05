#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include <vector>

/**
    Undo and redo for everything the editor can change.

    AudioProcessorValueTreeState takes an UndoManager, but that slot cannot be used as an edit
    history here. APVTS only copies parameter values into its ValueTree from a timer -- see
    AudioProcessorValueTreeState::timerCallback -- so a transaction would hold "whatever
    happened to change in the last 20 to 500 ms" rather than one user action, and an undo
    issued before the pending flush would be undone again by it a moment later. The history is
    therefore kept here, over the parameters themselves, where the writes actually happen.

    A snapshot is every parameter's normalised value in parameter-index order. That order is
    fixed for the life of the plugin -- a VST3 cannot gain or lose parameters at runtime -- so
    a snapshot is a flat array of floats, and restoring one is an index-wise compare and write.

    What counts as one undo step is decided by gestures. Every write the plugin makes to a
    parameter is bracketed by beginChangeGesture/endChangeGesture, whether it comes from a
    slider attachment, a step bar, or one of the pattern actions, so the start of a gesture is
    the start of an edit. Host automation sends no gestures, which is what stops a moving
    automation lane from filling the history with states nobody asked to return to.
*/
class UndoHistory final : private juce::AudioProcessorParameter::Listener
{
public:
    explicit UndoHistory (juce::AudioProcessor& processorToTrack);
    ~UndoHistory() override;

    /** Steps back one edit. False if there was nothing to step back to. */
    bool undo();

    /** Steps forward again. False if the last thing that happened was not an undo. */
    bool redo();

    bool canUndo() const noexcept { return ! undoStack.empty(); }
    bool canRedo() const noexcept { return ! redoStack.empty(); }

    /** Drops the history. The state a session had before the host loaded a different one is
        not somewhere the user can meaningfully step back into, so loading clears it. */
    void clear();

    //==========================================================================
    // The rest is for tests, which have no message loop to turn over between edits.

    int getUndoDepth() const noexcept { return (int) undoStack.size(); }
    int getRedoDepth() const noexcept { return (int) redoStack.size(); }

    /** Ends the current coalescing window, so the next gesture starts a new undo step. Stands
        in for the message loop turning over between two separate user actions. */
    void closeCurrentEdit() noexcept { editOpen = false; }

    //==========================================================================
    /** Holds the current undo step open until it is turned off again.

        One turn of the message loop is one edit, which is the right seam for everything that
        happens in a single click but the wrong one for a gesture that spans many: a value
        painted across the step bars in one stroke is one thing the user did, and arrives as
        a separate parameter gesture in a separate drag callback for every step it crosses.
        The editor brackets such a stroke with this, so it steps back in one press rather
        than one press per step. Left off, the message-loop rule stands.
    */
    void setEditHeldOpen (bool shouldHold) noexcept;

private:
    //==========================================================================
    void parameterValueChanged (int, float) override {}
    void parameterGestureChanged (int parameterIndex, bool gestureIsStarting) override;

    using Snapshot = std::vector<float>;

    Snapshot takeSnapshot() const;
    void restore (const Snapshot& snapshot);

    void captureBeforeEdit();

    juce::AudioProcessor& processor;

    std::vector<Snapshot> undoStack, redoStack;

    // One user action is one turn of the message loop. A pattern action writes up to forty
    // parameters in a tight loop and has to land as a single step, while two clicks on two
    // different steps must not merge -- and those are told apart by which message callback
    // they arrived in, not by how far apart in time they were. Gestures beginning while this
    // is set join the edit already recorded.
    bool editOpen = false;

    // While set, the window above is not allowed to close on its own -- see setEditHeldOpen.
    bool editHeld = false;

    // Restoring writes parameters exactly the way the UI does, gestures included, so without
    // this guard the restore would be recorded as a fresh edit and undo could never move.
    bool restoring = false;

    // The oldest step is dropped once this is reached. A snapshot is one float per parameter,
    // so the whole history at this depth is a few hundred KB.
    static constexpr size_t maxDepth = 128;

    JUCE_DECLARE_WEAK_REFERENCEABLE (UndoHistory)
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (UndoHistory)
};
