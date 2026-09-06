/*  TEMPORARY — delete once the Phase 2 engine rewrite is green.

    Proves that the rewritten SequencerEngine emits byte-identical MIDI to the engine as it
    stood before the rewrite (ReferenceEngine, a frozen copy of b07dbd1).

    EngineTests asserts on chosen scenarios, which is what you want from a suite you keep.
    This is the opposite: no opinion about what the engine should do, only that it does
    exactly what it used to, over a randomised sweep wide enough to reach the corners the
    hand-written cases do not -- swing against odd divisions, voice stealing under a gate
    over 100%, MPE channel exhaustion, negative PPQ pre-roll, mid-run parameter flips, and
    block sizes that are not divisors of a step.

    Any difference is a bug in the rewrite. If this reports one, fix the engine -- do not
    adjust the sweep to walk around it.
*/

#include "ReferenceEngine.h"
#include "SequencerEngine.h"

#include <chrono>
#include <cstdio>
#include <vector>

namespace
{
    int checksRun    = 0;
    int checksFailed = 0;

    void check (bool condition, const char* what)
    {
        ++checksRun;

        if (condition)
        {
            std::printf ("    ok    %s\n", what);
        }
        else
        {
            ++checksFailed;
            std::printf ("    FAIL  %s\n", what);
        }
    }

    void section (const char* name)
    {
        std::printf ("\n  %s\n", name);
    }

    //==========================================================================
    /** One randomised engine configuration, in a form both Snapshot types can be built
        from. The two structs are field-identical today, so the filler below is a template
        rather than two copies that could disagree about what "the same settings" means.
    */
    struct SweepConfig
    {
        struct Lane
        {
            float values[params::numSteps]   {};
            bool  enabled[params::numSteps]  {};
            float chance[params::numSteps]   {};
            float velocity[params::numSteps] {};
            float gate[params::numSteps]     {};

            bool  active    = true;
            int   length    = params::numSteps;
            int   division  = params::divIndex_1_16;
            int   direction = 0;
            float depth     = 1.0f;

            bool  ccOn      = false;
            int   ccNumber  = 20;
            int   ccChannel = 1;
            float ccOffset  = 0.0f;
        };

        Lane noteLanes[params::numLanes];
        Lane ccLanes[params::numLanes];

        int   noteTriggerSource = 0;
        bool  quantize     = true;
        int   bendRange    = 2;
        int   root         = 48;
        int   rangeOctaves = 2;
        int   scale        = 0;
        int   velocity     = params::fixedVelocity;
        int   midiChannel  = 1;
        bool  ccOn         = true;
        int   ccNumber     = 1;
        int   ccChannel    = 1;
        int   noteOctaves  = 0;
        float ccOffset     = 0.0f;
        float slewMs       = 0.0f;
        float swing        = 0.0f;
        int   voiceCount   = 1;
        bool  polyMode     = false;
        bool  mpeEnabled   = false;
    };

    /** Fills either engine's Snapshot from one config. Templated on the Snapshot type
        because ReferenceEngine::Snapshot and SequencerEngine::Snapshot are distinct types
        that happen to carry the same fields -- which is exactly the property under test.
    */
    template <typename SnapshotType>
    SnapshotType makeSnapshot (const SweepConfig& c)
    {
        SnapshotType s;

        const auto fillLane = [] (auto& target, const SweepConfig::Lane& source)
        {
            for (int step = 0; step < params::numSteps; ++step)
            {
                target.values[step]   = source.values[step];
                target.enabled[step]  = source.enabled[step];
                target.chance[step]   = source.chance[step];
                target.velocity[step] = source.velocity[step];
                target.gate[step]     = source.gate[step];
            }

            target.active    = source.active;
            target.length    = source.length;
            target.division  = source.division;
            target.direction = source.direction;
            target.depth     = source.depth;
            target.ccOn      = source.ccOn;
            target.ccNumber  = source.ccNumber;
            target.ccChannel = source.ccChannel;
            target.ccOffset  = source.ccOffset;
        };

        for (int lane = 0; lane < params::numLanes; ++lane)
        {
            fillLane (s.noteLanes[lane], c.noteLanes[lane]);
            fillLane (s.ccLanes[lane],   c.ccLanes[lane]);
        }

        s.noteTriggerSource = c.noteTriggerSource;
        s.quantize     = c.quantize;
        s.bendRange    = c.bendRange;
        s.root         = c.root;
        s.rangeOctaves = c.rangeOctaves;
        s.scale        = c.scale;
        s.velocity     = c.velocity;
        s.midiChannel  = c.midiChannel;
        s.ccOn         = c.ccOn;
        s.ccNumber     = c.ccNumber;
        s.ccChannel    = c.ccChannel;
        s.noteOctaves  = c.noteOctaves;
        s.ccOffset     = c.ccOffset;
        s.slewMs       = c.slewMs;
        s.swing        = c.swing;
        s.voiceCount   = c.voiceCount;
        s.polyMode     = c.polyMode;
        s.mpeEnabled   = c.mpeEnabled;

        return s;
    }

    //==========================================================================
    /** Everything the sweep varies about the lanes themselves. Split out from the globals
        below so a mid-run "automation" flip can move the globals without redrawing every
        step value underneath them -- which is what makes the config-change paths (a poly
        flip, an MPE flip, a bend-range change) reachable with notes already sounding.
    */
    void randomiseLanes (SweepConfig& c, juce::Random& r)
    {
        for (int pool = 0; pool < 2; ++pool)
        {
            for (int lane = 0; lane < params::numLanes; ++lane)
            {
                auto& l = pool == 0 ? c.noteLanes[lane] : c.ccLanes[lane];

                for (int step = 0; step < params::numSteps; ++step)
                {
                    l.values[step]  = r.nextFloat();
                    l.enabled[step] = r.nextInt (10) > 1;          // mostly on, some off

                    // Mostly certain, so probability is exercised without turning the whole
                    // sweep into a coin toss that fires almost nothing.
                    l.chance[step]   = r.nextInt (4) == 0 ? r.nextFloat() : 1.0f;
                    l.velocity[step] = r.nextFloat();

                    // Deliberately reaches past 100%, which is the case that makes a note
                    // overlap the following step and forces the voice allocator to work.
                    l.gate[step] = 5.0f + r.nextFloat() * 195.0f;
                }

                l.active    = r.nextInt (8) > 0;
                l.length    = 1 + r.nextInt (params::numSteps);
                l.division  = r.nextInt (params::numDivisions);
                l.direction = r.nextInt (4);
                l.depth     = r.nextFloat() * 2.0f - 1.0f;
                l.ccOn      = r.nextBool();
                l.ccNumber  = r.nextInt (128);
                l.ccChannel = 1 + r.nextInt (16);
                l.ccOffset  = r.nextFloat();
            }
        }
    }

    void randomiseGlobals (SweepConfig& c, juce::Random& r)
    {
        c.noteTriggerSource = r.nextInt (params::numLanes + 1);
        c.quantize     = r.nextBool();
        c.bendRange    = 1 + r.nextInt (48);
        c.root         = r.nextInt (128);
        c.rangeOctaves = 1 + r.nextInt (10);
        c.scale        = r.nextInt (params::numScales);
        c.velocity     = params::fixedVelocity;
        c.midiChannel  = 1 + r.nextInt (16);
        c.ccOn         = r.nextBool();
        c.ccNumber     = r.nextInt (128);
        c.ccChannel    = 1 + r.nextInt (16);
        c.noteOctaves  = r.nextInt (7) - 3;
        c.ccOffset     = r.nextFloat();
        c.slewMs       = r.nextInt (3) == 0 ? 0.0f : r.nextFloat() * 500.0f;
        c.swing        = r.nextInt (3) == 0 ? 0.0f : r.nextFloat() * 2.0f - 1.0f;
        c.voiceCount   = 1 + r.nextInt (8);
        c.polyMode     = r.nextBool();
        c.mpeEnabled   = r.nextBool();
    }

    //==========================================================================
    struct Event
    {
        int block  = 0;
        int sample = 0;
        std::vector<juce::uint8> bytes;
    };

    /** Flattens a block's MIDI into comparable records. Raw bytes rather than a parsed
        message, because "identical" here means identical on the wire, and a parse could
        hide a difference the receiving instrument would hear.
    */
    void collect (const juce::MidiBuffer& buffer, int blockIndex, std::vector<Event>& out)
    {
        for (const auto metadata : buffer)
            out.push_back ({ blockIndex, metadata.samplePosition,
                             std::vector<juce::uint8> (metadata.data, metadata.data + metadata.numBytes) });
    }

    juce::String describe (const Event& e)
    {
        juce::String s;
        s << "block " << e.block << " @" << e.sample << " [";

        for (size_t i = 0; i < e.bytes.size(); ++i)
            s << (i > 0 ? " " : "") << juce::String::toHexString (e.bytes[i]).paddedLeft ('0', 2);

        return s + "]";
    }

    /** Reports the first difference rather than a count: one concrete divergence with its
        block, sample offset and bytes is what you can actually debug from.
    */
    bool sameStream (const std::vector<Event>& reference, const std::vector<Event>& rewritten,
                     juce::String& detail)
    {
        const size_t common = juce::jmin (reference.size(), rewritten.size());

        for (size_t i = 0; i < common; ++i)
        {
            const auto& a = reference[i];
            const auto& b = rewritten[i];

            if (a.block == b.block && a.sample == b.sample && a.bytes == b.bytes)
                continue;

            detail = "event " + juce::String ((int) i)
                       + "\n            reference: " + describe (a)
                       + "\n            rewritten: " + describe (b);
            return false;
        }

        if (reference.size() != rewritten.size())
        {
            const bool referenceLonger = reference.size() > rewritten.size();
            const auto& extra = referenceLonger ? reference[common] : rewritten[common];

            detail = "streams agree for " + juce::String ((int) common) + " events, then "
                       + juce::String (referenceLonger ? "the rewrite stops early"
                                                       : "the rewrite emits an extra event")
                       + "\n            first unmatched: " + describe (extra);
            return false;
        }

        return true;
    }

    //==========================================================================
    /** One sweep iteration: both engines driven over the same timeline, in lockstep, with
        the same snapshot handed to each at every block.
    */
    bool runOneCase (int seed, juce::String& detail)
    {
        juce::Random r (seed);

        const double sampleRate   = r.nextBool() ? 48000.0 : 44100.0;
        const double bpm          = 60.0 + r.nextFloat() * 120.0;
        const double ppqPerSample = bpm / 60.0 / sampleRate;

        // Negative on some seeds: a host's pre-roll or count-in reports PPQ below zero, and
        // that is the path positiveMod() exists for.
        double ppq = r.nextInt (4) == 0 ? -2.0 - r.nextFloat() * 4.0
                                        : r.nextFloat() * 16.0;

        SweepConfig config;
        randomiseLanes (config, r);
        randomiseGlobals (config, r);

        ReferenceEngine reference;
        SequencerEngine rewritten;

        reference.prepare (sampleRate);
        rewritten.prepare (sampleRate);

        std::vector<Event> referenceEvents, rewrittenEvents;

        juce::MidiBuffer referenceBuffer, rewrittenBuffer;

        constexpr int numBlocks = 40;

        for (int block = 0; block < numBlocks; ++block)
        {
            // Deliberately not divisors of a step: a 1/16 at 120bpm/48k is 6000 samples, and
            // a block size that divided it evenly would put every boundary at offset 0 and
            // hide exactly the off-by-one this harness is here to catch.
            static constexpr int blockSizes[] { 64, 128, 127, 256, 333, 512, 1024, 61 };
            const int numSamples = blockSizes[r.nextInt ((int) std::size (blockSizes))];

            // Stopping and restarting mid-run exercises the release-everything path, and
            // restarting from a different PPQ exercises the transport-jump path.
            const bool transportRunning = r.nextInt (12) > 0;

            // Mid-run parameter movement, with notes already sounding -- this is what makes
            // the poly, MPE and bend-range reconfiguration branches reachable.
            if (r.nextInt (6) == 0)
                randomiseGlobals (config, r);

            if (r.nextInt (15) == 0)
                randomiseLanes (config, r);

            const auto referenceSnapshot = makeSnapshot<ReferenceEngine::Snapshot> (config);
            const auto rewrittenSnapshot = makeSnapshot<SequencerEngine::Snapshot> (config);

            referenceBuffer.clear();
            rewrittenBuffer.clear();

            reference.process (referenceSnapshot, referenceBuffer, numSamples,
                               ppq, ppqPerSample, transportRunning);
            rewritten.process (rewrittenSnapshot, rewrittenBuffer, numSamples,
                               ppq, ppqPerSample, transportRunning);

            collect (referenceBuffer, block, referenceEvents);
            collect (rewrittenBuffer, block, rewrittenEvents);

            // A stopped transport does not advance the timeline, the same way the processor's
            // own free-run bookkeeping does not while the host is stopped.
            if (transportRunning)
                ppq += ppqPerSample * (double) numSamples;

            // An occasional jump, so the stateless index resolution is asked to cope with a
            // loop point rather than only with a straight run.
            if (r.nextInt (20) == 0)
                ppq += (double) (r.nextInt (32) - 16);
        }

        // The UI step readouts are part of the engine's observable surface too, so a rewrite
        // that got the MIDI right but the playhead wrong would still be caught here.
        for (int lane = 0; lane < params::numLanes; ++lane)
        {
            for (auto kind : { params::LaneKind::note, params::LaneKind::cc })
            {
                if (reference.getCurrentStep (lane, kind) == rewritten.getCurrentStep (lane, kind))
                    continue;

                detail = "UI step readout diverged on "
                           + juce::String (kind == params::LaneKind::cc ? "CC" : "note")
                           + " lane " + juce::String (lane + 1)
                           + ": reference " + juce::String (reference.getCurrentStep (lane, kind))
                           + ", rewritten " + juce::String (rewritten.getCurrentStep (lane, kind));
                return false;
            }
        }

        return sameStream (referenceEvents, rewrittenEvents, detail);
    }

    //==========================================================================
    /** Same snapshot, same block size, many blocks -- so the number reported is the engine's
        own per-block cost rather than the sweep's bookkeeping.
    */
    double measureNsPerBlock (bool useRewritten)
    {
        constexpr int numSamples = 128;
        constexpr int numBlocks  = 20000;

        juce::Random r (0x8eaf);

        SweepConfig config;
        randomiseLanes (config, r);
        randomiseGlobals (config, r);

        // A representative worst case rather than a stock patch: every lane live, swing on,
        // slew on, so the measurement covers the path the rewrite is meant to shorten.
        config.swing      = 0.35f;
        config.slewMs     = 40.0f;
        config.polyMode   = true;
        config.mpeEnabled = true;
        config.voiceCount = 4;

        for (int lane = 0; lane < params::numLanes; ++lane)
        {
            config.noteLanes[lane].active = true;
            config.ccLanes[lane].active   = true;
            config.ccLanes[lane].ccOn     = true;
        }

        const double sampleRate   = 48000.0;
        const double ppqPerSample = 120.0 / 60.0 / sampleRate;

        ReferenceEngine reference;
        SequencerEngine rewritten;

        reference.prepare (sampleRate);
        rewritten.prepare (sampleRate);

        const auto referenceSnapshot = makeSnapshot<ReferenceEngine::Snapshot> (config);
        const auto rewrittenSnapshot = makeSnapshot<SequencerEngine::Snapshot> (config);

        juce::MidiBuffer buffer;
        double ppq = 0.0;

        const auto start = std::chrono::steady_clock::now();

        for (int block = 0; block < numBlocks; ++block)
        {
            buffer.clear();

            if (useRewritten)
                rewritten.process (rewrittenSnapshot, buffer, numSamples, ppq, ppqPerSample, true);
            else
                reference.process (referenceSnapshot, buffer, numSamples, ppq, ppqPerSample, true);

            ppq += ppqPerSample * (double) numSamples;
        }

        const auto elapsed = std::chrono::steady_clock::now() - start;

        return (double) std::chrono::duration_cast<std::chrono::nanoseconds> (elapsed).count()
                 / (double) numBlocks;
    }
}

//==============================================================================
int main()
{
    std::printf ("Ravel engine equivalence\n");

    //==========================================================================
    section ("The rewritten engine matches the frozen one, event for event");
    {
        constexpr int numCases = 1000;

        int firstFailingSeed = -1;
        juce::String firstDetail;

        for (int seed = 1; seed <= numCases; ++seed)
        {
            juce::String detail;

            if (runOneCase (seed, detail))
                continue;

            firstFailingSeed = seed;
            firstDetail = detail;
            break;
        }

        if (firstFailingSeed >= 0)
            std::printf ("\n    seed %d diverged:\n            %s\n\n",
                         firstFailingSeed, firstDetail.toRawUTF8());

        check (firstFailingSeed < 0,
               "1000 randomised timelines produce byte-identical MIDI");
    }

    //==========================================================================
    section ("Cost per block (4 note + 4 CC lanes, swing, slew, poly, MPE)");
    {
        // Warm both paths before timing either, so neither pays for a cold cache.
        measureNsPerBlock (false);
        measureNsPerBlock (true);

        const double before = measureNsPerBlock (false);
        const double after  = measureNsPerBlock (true);

        std::printf ("    reference: %8.1f ns/block\n", before);
        std::printf ("    rewritten: %8.1f ns/block", after);

        if (after > 0.0)
            std::printf ("   (%.2fx)", before / after);

        std::printf ("\n");

        // Not a threshold to pass, only a guard against the rewrite quietly costing more.
        check (after <= before * 1.05,
               "the rewrite is no slower than the engine it replaces");
    }

    std::printf ("\n%d checks, %d failed\n", checksRun, checksFailed);

    return checksFailed == 0 ? 0 : 1;
}
