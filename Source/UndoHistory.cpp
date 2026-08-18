#include "UndoHistory.h"

UndoHistory::UndoHistory (juce::AudioProcessor& processorToTrack)
    : processor (processorToTrack)
{
    // Every parameter, rather than the processor's own listener interface: gestures are
    // reported through AudioProcessorParameter::Listener, and that is the only callback that
    // fires early enough to snapshot the value a gesture is about to overwrite.
    for (auto* parameter : processor.getParameters())
        parameter->addListener (this);
}

UndoHistory::~UndoHistory()
{
    for (auto* parameter : processor.getParameters())
        parameter->removeListener (this);
}

//==============================================================================
void UndoHistory::parameterGestureChanged (int, bool gestureIsStarting)
{
    // The end of a gesture says nothing that the start did not already say, and a gesture we
    // are producing ourselves is a restore, not an edit.
    if (! gestureIsStarting || restoring)
        return;

    captureBeforeEdit();
}

void UndoHistory::captureBeforeEdit()
{
    if (editOpen)
        return;

    editOpen = true;

    // Reopened once the current message callback has returned, so everything a single click
    // sets off -- a randomise's eight writes, a lane paste's forty -- lands as one step.
    juce::WeakReference<UndoHistory> weakThis (this);

    juce::MessageManager::callAsync ([weakThis]
    {
        if (auto* self = weakThis.get())
            self->editOpen = false;
    });

    undoStack.push_back (takeSnapshot());

    if (undoStack.size() > maxDepth)
        undoStack.erase (undoStack.begin());

    // Editing after an undo is a new branch: what was undone is no longer reachable.
    redoStack.clear();
}

//==============================================================================
UndoHistory::Snapshot UndoHistory::takeSnapshot() const
{
    const auto& parameters = processor.getParameters();

    Snapshot snapshot;
    snapshot.reserve ((size_t) parameters.size());

    for (auto* parameter : parameters)
        snapshot.push_back (parameter->getValue());

    return snapshot;
}

void UndoHistory::restore (const Snapshot& snapshot)
{
    const auto& parameters = processor.getParameters();

    // The parameter list is fixed for the life of the plugin, so a snapshot of another size
    // could only come from a bug in the code that made it.
    if ((int) snapshot.size() != parameters.size())
    {
        jassertfalse;
        return;
    }

    const juce::ScopedValueSetter<bool> guard (restoring, true);

    for (int i = 0; i < parameters.size(); ++i)
    {
        auto* parameter = parameters.getUnchecked (i);
        const float target = snapshot[(size_t) i];

        // Only what actually moved. Writing all of them would have the host record a couple
        // of hundred automation points for the sake of the one step that changed.
        if (juce::approximatelyEqual (parameter->getValue(), target))
            continue;

        // Gestures so the write reads to the host as a deliberate edit, the same as the one
        // being undone did -- which is also what lets the host's own undo see it.
        parameter->beginChangeGesture();
        parameter->setValueNotifyingHost (target);
        parameter->endChangeGesture();
    }
}

//==============================================================================
bool UndoHistory::undo()
{
    if (undoStack.empty())
        return false;

    // Taken before the restore, so redo returns to what is on screen right now rather than to
    // whatever the next entry down happens to hold.
    redoStack.push_back (takeSnapshot());

    const auto target = undoStack.back();
    undoStack.pop_back();

    restore (target);
    return true;
}

bool UndoHistory::redo()
{
    if (redoStack.empty())
        return false;

    undoStack.push_back (takeSnapshot());

    const auto target = redoStack.back();
    redoStack.pop_back();

    restore (target);
    return true;
}

void UndoHistory::clear()
{
    undoStack.clear();
    redoStack.clear();
}
