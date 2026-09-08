#include "ExternalMidiSelector.h"

namespace
{
    // Fixed rather than however much of the header the wordmark and history arrows leave over:
    // a fixed width is what lets this sit flush against the header's right edge instead of
    // stretching to fill it. Wide enough for "Host MIDI only", the longest stock choice.
    constexpr int comboWidth = 150;
    constexpr int gap        = 8;

    /** The caption is drawn by this component rather than by the chip, because the chip is a
        boxed value and the caption sits outside that box -- on the header's own ground, the
        way a field's label does. */
    const juce::String captionText { "MIDI output" };
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

    theme::setRole (rescanButton, theme::Role::headerButton);
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
    const int captionWidth = (int) std::ceil (
        juce::GlyphArrangement::getStringWidth (theme::rowFont(), captionText));

    return captionWidth + gap + comboWidth + gap + theme::chipWidth ("Rescan", 20);
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
    // No pill behind it any more. The header is its own band, and a raised rectangle inside a
    // band that is already raised off the window reads as a third level of depth for something
    // that is only a caption and two controls.
    const int captionWidth = (int) std::ceil (
        juce::GlyphArrangement::getStringWidth (theme::rowFont(), captionText));

    g.setFont (theme::rowFont());
    g.setColour (theme::textFaint);
    g.drawText (captionText, getLocalBounds().withWidth (captionWidth),
                juce::Justification::centredLeft, false);
}

void ExternalMidiSelector::resized()
{
    auto inner = getLocalBounds().withSizeKeepingCentre (getWidth(), rowHeight);

    const int captionWidth = (int) std::ceil (
        juce::GlyphArrangement::getStringWidth (theme::rowFont(), captionText));

    inner.removeFromLeft (captionWidth + gap);

    rescanButton.setBounds (inner.removeFromRight (theme::chipWidth ("Rescan", 20)));
    inner.removeFromRight (gap);
    box.setBounds (inner);
}
