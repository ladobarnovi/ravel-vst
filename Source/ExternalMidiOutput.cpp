#include "ExternalMidiOutput.h"

//==============================================================================
ExternalMidiOutput::ExternalMidiOutput()
    : juce::Thread ("Ravel External MIDI")
{
    startThread (juce::Thread::Priority::high);
}

ExternalMidiOutput::~ExternalMidiOutput()
{
    // Nothing new gets queued from here on, so the drain below is finite.
    deviceOpen.store (false, std::memory_order_release);

    // run() waits on wakeUp, not on the Thread's own event, so signalThreadShouldExit() alone
    // would leave it sitting out the full 50 ms poll timeout before noticing -- once per
    // instance, every time a session closes.
    signalThreadShouldExit();
    wakeUp.signal();

    stopThread (1000);

    const juce::ScopedLock sl (deviceLock);
    silence (device.get());
}

//==============================================================================
void ExternalMidiOutput::silence (juce::MidiOutput* target)
{
    if (target == nullptr)
        return;

    for (int channel = 1; channel <= 16; ++channel)
    {
        target->sendMessageNow (juce::MidiMessage::allSoundOff (channel));
        target->sendMessageNow (juce::MidiMessage::allNotesOff (channel));
    }
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

    // Cleared first, so the audio thread stops queuing for the outgoing port before it is
    // silenced -- otherwise a note-on could be pushed between the silence and the swap and
    // arrive on a port nothing will ever close.
    deviceOpen.store (false, std::memory_order_release);

    {
        // Held across both the silence and the swap. That is thirty-two driver calls with the
        // drain thread locked out, which is exactly the stall the per-message locking in run()
        // exists to avoid -- but this runs only when the user picks a different port, and
        // leaving a note hanging on the one they just left is the worse outcome.
        const juce::ScopedLock sl (deviceLock);

        silence (device.get());

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
void ExternalMidiOutput::pushMessage (const juce::uint8* data, int numBytes, double sendAtMs)
{
    if (! deviceOpen.load (std::memory_order_acquire))
        return;

    // Every message SequencerEngine::process() emits is a 1-3 byte channel-voice message; a
    // longer one would be a bug upstream, not something to handle by growing this queue's
    // fixed-size slots.
    jassert (numBytes >= 1 && numBytes <= 3);

    if (numBytes < 1 || numBytes > 3)
        return;

    const auto scope = fifo.write (1);

    if (scope.blockSize1 + scope.blockSize2 == 0)
        return; // Full -- drop rather than block the audio thread waiting for room.

    const int index = scope.blockSize1 > 0 ? scope.startIndex1 : scope.startIndex2;

    auto& event = queue[(size_t) index];
    std::memcpy (event.data, data, (size_t) numBytes);
    event.length   = (uint8_t) numBytes;
    event.sendAtMs = sendAtMs;

    wakeUp.signal();
}

//==============================================================================
void ExternalMidiOutput::run()
{
    // Never more than this between checks of threadShouldExit() while idle.
    constexpr int pollMs = 50;

    while (! threadShouldExit())
    {
        int start1, size1, start2, size2;
        fifo.prepareToRead (1, start1, size1, start2, size2);

        if (size1 + size2 == 0)
        {
            // Nothing queued: sleep until pushMessage() signals. The timeout is only a safety
            // net against a signal landing in the instant before wait() is called.
            wakeUp.wait (pollMs);
            continue;
        }

        const auto& event = queue[(size_t) (size1 > 0 ? start1 : start2)];
        const double dueInMs = event.sendAtMs - juce::Time::getMillisecondCounterHiRes();

        if (dueInMs >= 1.0)
        {
            // Not due yet. wait() takes whole milliseconds and rounds this down, so the send
            // lands within about a millisecond of its time -- JUCE raises the Windows timer
            // resolution to 1 ms (timeBeginPeriod) the first time the hi-res counter is read,
            // which is what keeps a 1 ms wait from being a 15.6 ms one. A push in the meantime
            // wakes this early, which is harmless: the head of the queue is checked again.
            // ponytail: ~1 ms send granularity; spin the last millisecond if that is ever audible.
            wakeUp.wait (juce::jmin ((int) dueInMs, pollMs));
            continue;
        }

        {
            // Taken per message rather than around a run of them. setDevice() has to wait for
            // this lock, and holding it across a run of driver calls would make changing the
            // port stall the UI for as long as the queue is deep.
            const juce::ScopedLock sl (deviceLock);

            if (device != nullptr)
                device->sendMessageNow (juce::MidiMessage (event.data, (int) event.length));
        }

        fifo.finishedRead (1);
    }
}
