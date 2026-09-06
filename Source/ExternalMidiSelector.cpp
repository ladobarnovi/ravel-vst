#include "ExternalMidiSelector.h"

namespace
{
    // Fixed rather than however much of the header the title and history arrows leave over: a
    // fixed width is what lets the pill sit flush against the header's right edge instead of
    // stretching to fill it. Wide enough for "MIDI output" as a caption plus "Host MIDI only"
    // as the longest stock choice, both at theme::rowFont.
    constexpr int comboWidth = 210;
    constexpr int padding    = 10;
    constexpr int gap        = 8;
}

//==============================================================================
ExternalMidiSelector::ExternalMidiSelector (RavelAudioProcessor& processor)
    : processorRef (processor)
{
    // Standing alone in its own header pill rather than among a grid of peers the way a
    // TabPage's value rows do, so it gets the Role that draws itself as an obvious dropdown --
    // a boxed, arrowed value -- rather than valueRow's bare caption/value pair, which leans on
    // that grid to read as a control at all. See theme::Role::selectChip.
    theme::setRole (box, theme::Role::selectChip);
    theme::setCaption (box, "MIDI output");
    box.setTooltip ("Mirrors every note and CC this instance generates straight out "
                    "a system MIDI port -- a loopMIDI port, most likely -- bypassing "
                    "Ableton's own MIDI routing entirely. The host still receives "
                    "the same events as always; this only adds a second destination");

    box.onChange = [this]
    {
        const int id = box.getSelectedId();
        const juce::String identifier = id >= 2 && id - 2 < deviceIds.size()
                                            ? deviceIds[id - 2]
                                            : juce::String();

        processorRef.externalMidiOutput.setDevice (identifier);

        // Plain state-tree property rather than an APVTS parameter -- see this class's own
        // header -- restored in PluginProcessor::setStateInformation.
        processorRef.apvts.state.setProperty ("externalMidiDevice", identifier, nullptr);
    };

    addAndMakeVisible (box);

    theme::styleActionButton (rescanButton);
    rescanButton.setTooltip ("Re-scan for MIDI ports -- a loopMIDI port created "
                             "after this window opened won't appear until this is "
                             "clicked");
    rescanButton.onClick = [this] { refresh(); };
    addAndMakeVisible (rescanButton);

    refresh();
}

//==============================================================================
int ExternalMidiSelector::preferredWidth()
{
    return padding * 2 + comboWidth + gap + theme::actionButtonWidth ("Rescan", rowHeight);
}

void ExternalMidiSelector::refresh()
{
    const auto currentIdentifier = processorRef.externalMidiOutput.getCurrentDeviceIdentifier();

    box.clear (juce::dontSendNotification);
    deviceIds.clear();

    box.addItem ("Host MIDI only", 1);
    int selectedId = 1;

    for (const auto& info : juce::MidiOutput::getAvailableDevices())
    {
        deviceIds.add (info.identifier);
        const int itemId = deviceIds.size() + 1;   // ids start at 2, list is 0-based

        box.addItem (info.name, itemId);

        if (info.identifier == currentIdentifier)
            selectedId = itemId;
    }

    // dontSendNotification: this reflects state the processor already has, so it must not loop
    // back through onChange and call setDevice() again.
    box.setSelectedId (selectedId, juce::dontSendNotification);
}

//==============================================================================
void ExternalMidiSelector::paint (juce::Graphics& g)
{
    // Its own pill rather than a rectangle the parent draws on its behalf: what sits above the
    // window's one surface says so by being raised, and the thing that is raised should be the
    // thing that knows how wide it is.
    g.setColour (theme::raised);
    g.fillRoundedRectangle (getLocalBounds().toFloat(), 5.0f);
}

void ExternalMidiSelector::resized()
{
    auto inner = getLocalBounds().reduced (padding, (getHeight() - rowHeight) / 2);

    rescanButton.setBounds (inner.removeFromRight (theme::actionButtonWidth ("Rescan", rowHeight)));
    inner.removeFromRight (gap);
    box.setBounds (inner);
}
