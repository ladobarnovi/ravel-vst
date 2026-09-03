#include "ExternalMidiOutput.h"

//==============================================================================
ExternalMidiOutput::ExternalMidiOutput()
    : juce::Thread ("Ravel External MIDI")
{
    startThread (juce::Thread::Priority::high);
}

ExternalMidiOutput::~ExternalMidiOutput()
{
    stopThread (1000);
}

//==============================================================================
void ExternalMidiOutput::setDevice (const juce::String& deviceIdentifier)
{
    // Opening/closing a MIDI port is an OS call with no real-time guarantee, so it happens here
    // on the message thread -- pushMessage() on the audio thread never reaches this far.
    std::unique_ptr<juce::MidiOutput> newDevice;

    if (deviceIdentifier.isNotEmpty())
        newDevice = juce::MidiOutput::openDevice (deviceIdentifier);

    const bool opened = newDevice != nullptr;

    {
        const juce::ScopedLock sl (deviceLock);
        device = std::move (newDevice);
        currentIdentifier = opened ? deviceIdentifier : juce::String();
    }

    // Written last, after the device is actually in place, since this is what tells
    // pushMessage() it is safe to start queuing for it.
    deviceOpen.store (opened, std::memory_order_release);
}

juce::String ExternalMidiOutput::getCurrentDeviceIdentifier() const
{
    const juce::ScopedLock sl (deviceLock);
    return currentIdentifier;
}

//==============================================================================
void ExternalMidiOutput::pushMessage (const juce::MidiMessage& message)
{
    if (! deviceOpen.load (std::memory_order_acquire))
        return;

    const int numBytes = message.getRawDataSize();

    // Every message SequencerEngine::process() emits is a 1-3 byte channel-voice message; a
    // longer one would be a bug upstream, not something to handle by growing this queue's
    // fixed-size slots.
    jassert (numBytes <= 3);

    if (numBytes > 3)
        return;

    const auto scope = fifo.write (1);

    if (scope.blockSize1 + scope.blockSize2 == 0)
        return; // Full -- drop rather than block the audio thread waiting for room.

    const int index = scope.blockSize1 > 0 ? scope.startIndex1 : scope.startIndex2;

    auto& event = queue[(size_t) index];
    std::memcpy (event.data, message.getRawData(), (size_t) numBytes);
    event.length = (uint8_t) numBytes;

    wakeUp.signal();
}

//==============================================================================
void ExternalMidiOutput::run()
{
    while (! threadShouldExit())
    {
        // Normally this sleeps until pushMessage() signals it; the timeout is only a safety
        // net against a signal landing in the instant before wait() is called.
        wakeUp.wait (50);

        const auto scope = fifo.read (fifo.getNumReady());

        auto sendRange = [this] (int start, int count)
        {
            const juce::ScopedLock sl (deviceLock);

            if (device == nullptr)
                return;

            for (int i = 0; i < count; ++i)
            {
                const auto& event = queue[(size_t) (start + i)];
                device->sendMessageNow (juce::MidiMessage (event.data, (int) event.length));
            }
        };

        if (scope.blockSize1 > 0)
            sendRange (scope.startIndex1, scope.blockSize1);

        if (scope.blockSize2 > 0)
            sendRange (scope.startIndex2, scope.blockSize2);
    }
}
