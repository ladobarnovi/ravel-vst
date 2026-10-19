#pragma once

#include <juce_audio_devices/juce_audio_devices.h>

/** Sends MIDI straight out an OS MIDI port -- a loopMIDI port, most likely, feeding another
    Ableton track or an external device -- instead of returning it from processBlock. Ableton
    never sees these messages: they never enter its MIDI graph, so nothing about the host's own
    routing (or lack of it) affects whether they arrive.

    The audio thread only ever pushes fixed-size events into a lock-free ring buffer; it never
    touches the actual juce::MidiOutput; opening a port and writing to it are calls into the OS
    MIDI driver with no real-time guarantee. A dedicated background thread owns the device
    handle, drains the ring buffer, and makes that call.
*/
class ExternalMidiOutput final : private juce::Thread
{
public:
    ExternalMidiOutput();
    ~ExternalMidiOutput() override;

    /** Message-thread only. Pass an empty identifier to close the current device and go quiet. */
    void setDevice (const juce::String& deviceIdentifier);

    /** Message-thread only. Empty when no device is open. */
    juce::String getCurrentDeviceIdentifier() const;

    /** Audio-thread only, real-time safe: never blocks, never allocates. Every message Ravel's
        engine emits is a 1-3 byte channel-voice message -- note on/off, CC, pitch bend -- so a
        fixed-size event is enough; there is no need to carry a full juce::MidiMessage across
        the ring buffer. Silently dropped if no device is open or the buffer is full.

        Raw bytes rather than a juce::MidiMessage because the caller is iterating a MidiBuffer,
        whose metadata already exposes exactly these two things -- building a MidiMessage from
        them only to read them straight back out is work with nothing at the end of it.

        sendAtMs is a juce::Time::getMillisecondCounterHiRes() time: the drain thread holds the
        event until then (see sendTimeMs()). Events must be pushed in non-decreasing sendAtMs
        order, which they are when they come from consecutive blocks through sendTimeMs().
    */
    void pushMessage (const juce::uint8* data, int numBytes, double sendAtMs);

    /** When an event at sampleOffset into a block that started processing at blockStartMs
        should leave for the port.

        The audio thread runs a block in a burst, well before that block is heard, so sending
        each event the instant it is produced would collapse its position within the block --
        every event in it leaves at once, and lands anywhere up to a buffer early or late
        relative to the grid. Spacing them out by their offset keeps the intervals the engine
        computed, and adding one whole block of delay on top is what makes that possible: the
        last event of a block cannot be sent at its true time until the block has been
        produced. The result is a constant one-buffer delay instead of a jittery zero-to-one,
        which Live's Track Delay can then cancel exactly.
    */
    static double sendTimeMs (double blockStartMs, int sampleOffset, double sampleRate, int blockSize)
    {
        // An unprepared rate would make this infinite, and an event due never sits at the head
        // of the queue holding back every one behind it.
        if (sampleRate <= 0.0)
            return blockStartMs;

        return blockStartMs + 1000.0 * (double) (blockSize + sampleOffset) / sampleRate;
    }

private:
    void run() override;

    /** All Sound Off and All Notes Off on all sixteen channels.

        Sent to a port we are about to stop using. Ravel closes its own notes by emitting
        note-offs, and those reach this port the same way the note-ons did -- but a port being
        swapped away from, or closed with the plugin, will never be handed the ones that have
        not happened yet. Whatever it was sounding would hang there until the instrument was
        reset by hand, and the host's own copy of the stream gives no clue why.

        Both messages because instruments differ over which they honour, and neither is
        expensive: this runs on the message thread, once, when a port is being let go.
    */
    static void silence (juce::MidiOutput* target);

    struct QueuedEvent
    {
        double  sendAtMs = 0.0;
        uint8_t data[3] {};
        uint8_t length = 0;
    };

    // A step sequencer's own output is sparse -- at most a handful of events per block -- so
    // this is generous headroom, not a tuned figure.
    static constexpr int fifoCapacity = 2048;
    juce::AbstractFifo fifo { fifoCapacity };
    // On the heap, allocated once here: at 16 bytes an event it is 32 KB, too much to carry
    // inline in a processor that hosts and tests are free to construct on the stack.
    std::vector<QueuedEvent> queue = std::vector<QueuedEvent> ((size_t) fifoCapacity);

    // Checked by pushMessage() before it touches the fifo at all, so selecting no device (the
    // default) costs the audio thread one relaxed load rather than a queue push and a thread
    // wake it knows nobody will read.
    std::atomic<bool> deviceOpen { false };

    // Signalled after every push, so the background thread notices a newly queued event instead
    // of waiting out its poll interval; the timeout on wait() in run() is only a safety net for
    // the case where a signal lands just before the thread starts waiting.
    juce::WaitableEvent wakeUp;

    juce::CriticalSection deviceLock;
    std::unique_ptr<juce::MidiOutput> device;
    juce::String currentIdentifier;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ExternalMidiOutput)
};
