#pragma once

#include "PluginProcessor.h"
#include "Theme.h"

/**
    The header's MIDI-output pill: a port chooser and a Rescan button, on their own raised
    ground.

    Global rather than per-workspace -- it routes both Note and CC output alike -- which is why
    it lives in the header rather than in either tab. It owns its own width and its own pill
    background so the editor's layout only has to decide where to put it, and knows nothing
    about how many controls are inside.

    There is no APVTS parameter behind any of this: which port this machine has plugged in is
    not something a host should automate or recall through undo. The identifier rides in the
    state tree instead -- see ExternalMidiOutput's own header.
*/
class ExternalMidiSelector final : public juce::Component
{
public:
    explicit ExternalMidiSelector (RavelAudioProcessor& processor);

    /** Width the pill needs for its contents. The editor asks rather than assumes, so adding
        a control in here cannot leave the two disagreeing about how much room it takes. */
    static int preferredWidth();

    /** Height of the row inside the pill -- the editor centres the pill on this. */
    static constexpr int rowHeight = 22;

    /** Re-enumerates the system's MIDI outputs. Called once at construction and again from
        Rescan: a port created in loopMIDI after this window opened otherwise never appears. */
    void refresh();

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    RavelAudioProcessor& processorRef;

    juce::ComboBox box;
    juce::TextButton rescanButton { "Rescan" };

    /** Parallel to the ComboBox's items from id 2 up (id 1 is the fixed "Host MIDI only"
        entry): deviceIds[id - 2] is that item's device identifier. */
    juce::StringArray deviceIds;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ExternalMidiSelector)
};
