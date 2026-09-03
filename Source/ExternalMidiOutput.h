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
        fixed-size event is enough; there is no need to carry a full juce::MidiMessage (whose
        copy can allocate) across the ring buffer. Silently dropped if no device is open or the
        buffer is full.
    */
    void pushMessage (const juce::MidiMessage& message);

private:
    void run() override;

    struct QueuedEvent
    {
        uint8_t data[3] {};
        uint8_t length = 0;
    };

    // A step sequencer's own output is sparse -- at most a handful of events per block -- so
    // this is generous headroom, not a tuned figure.
    static constexpr int fifoCapacity = 2048;
    juce::AbstractFifo fifo { fifoCapacity };
    QueuedEvent queue[(size_t) fifoCapacity];

    // Checked by pushMessage() before it touches the fifo at all, so selecting no device (the
    // default) costs the audio thread one relaxed load rather than a queue push and a thread
    // wake it knows nobody will read.
    std::atomic<bool> deviceOpen { false };

    // Signalled after every push, so the background thread wakes with near-zero latency instead
    // of waiting out its poll interval; the timeout on wait() in run() is only a safety net for
    // the case where a signal lands just before the thread starts waiting.
    juce::WaitableEvent wakeUp;

    juce::CriticalSection deviceLock;
    std::unique_ptr<juce::MidiOutput> device;
    juce::String currentIdentifier;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ExternalMidiOutput)
};
