#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
RavelAudioProcessor::RavelAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "RAVEL", params::createParameterLayout())
{
    for (int lane = 0; lane < params::numLanes; ++lane)
    {
        auto& lp = laneParams[lane];

        for (int step = 0; step < params::numSteps; ++step)
        {
            lp.values[step]  = apvts.getRawParameterValue (params::stepValueId (lane, step));
            lp.enabled[step] = apvts.getRawParameterValue (params::stepOnId (lane, step));
            lp.chance[step]  = apvts.getRawParameterValue (params::stepChanceId (lane, step));
            lp.stepVelocity[step] = apvts.getRawParameterValue (params::stepVelocityId (lane, step));
            lp.stepGate[step] = apvts.getRawParameterValue (params::stepGateId (lane, step));
            lp.stepSlide[step] = apvts.getRawParameterValue (params::stepSlideId (lane, step));
        }

        lp.active    = apvts.getRawParameterValue (params::laneOnId (lane));
        lp.length    = apvts.getRawParameterValue (params::laneLengthId (lane));
        lp.division  = apvts.getRawParameterValue (params::laneDivId (lane));
        lp.direction = apvts.getRawParameterValue (params::laneDirId (lane));
        lp.depth     = apvts.getRawParameterValue (params::laneDepthId (lane));
        lp.mode      = apvts.getRawParameterValue (params::laneModeId (lane));
        lp.nudge     = apvts.getRawParameterValue (params::laneNudgeId (lane));
        lp.humanize  = apvts.getRawParameterValue (params::laneHumanizeId (lane));
        lp.ccOn      = apvts.getRawParameterValue (params::laneCcOnId (lane));
        lp.ccNumber  = apvts.getRawParameterValue (params::laneCcNumId (lane));
        lp.ccChannel = apvts.getRawParameterValue (params::laneCcChanId (lane));
    }

    pOutputMode  = apvts.getRawParameterValue (params::outputModeId);
    pTriggerSrc  = apvts.getRawParameterValue (params::triggerSrcId);
    pQuantize    = apvts.getRawParameterValue (params::quantizeId);
    pBendRange   = apvts.getRawParameterValue (params::bendRangeId);
    pRootNote    = apvts.getRawParameterValue (params::rootNoteId);
    pRangeSteps  = apvts.getRawParameterValue (params::rangeStepsId);
    pScale       = apvts.getRawParameterValue (params::scaleId);
    pVelocity    = apvts.getRawParameterValue (params::velocityId);
    pMidiChannel = apvts.getRawParameterValue (params::midiChannelId);
    pCcNumber    = apvts.getRawParameterValue (params::ccNumberId);
    pCcChannel   = apvts.getRawParameterValue (params::ccChannelId);
    pOffset      = apvts.getRawParameterValue (params::offsetId);
    pSlew        = apvts.getRawParameterValue (params::slewId);
    pFreeRun     = apvts.getRawParameterValue (params::freeRunId);
    pSwing       = apvts.getRawParameterValue (params::swingId);
    pVoiceCount  = apvts.getRawParameterValue (params::voiceCountId);
    pPolyMode    = apvts.getRawParameterValue (params::polyModeId);
    pLaneCount   = apvts.getRawParameterValue (params::laneCountId);
}

//==============================================================================
void RavelAudioProcessor::prepareToPlay (double sampleRate, int)
{
    engine.prepare (sampleRate);
    freeRunPpq = 0.0;
}

void RavelAudioProcessor::releaseResources()
{
}

void RavelAudioProcessor::reset()
{
    engine.reset();
    freeRunPpq = 0.0;
}

bool RavelAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo();
}

//==============================================================================
SequencerEngine::Snapshot RavelAudioProcessor::buildSnapshot() const
{
    SequencerEngine::Snapshot s;

    const int laneCount = juce::jlimit (1, params::numLanes,
                                        (int) std::lround (pLaneCount->load()));

    for (int lane = 0; lane < params::numLanes; ++lane)
    {
        const auto& lp = laneParams[lane];
        auto& ls = s.lanes[lane];

        for (int step = 0; step < params::numSteps; ++step)
        {
            ls.values[step]  = lp.values[step]->load();
            ls.enabled[step] = lp.enabled[step]->load() > 0.5f;
            ls.chance[step]  = lp.chance[step]->load();
            ls.velocity[step] = lp.stepVelocity[step]->load();
            ls.gate[step]     = lp.stepGate[step]->load();
            ls.slide[step]    = lp.stepSlide[step]->load();
        }

        // A lane the instance has not been given yet is inert in exactly the same way a
        // muted one is, so the engine needs to know about only the one flag.
        ls.active    = lane < laneCount && lp.active->load() > 0.5f;
        ls.length    = (int) std::lround (lp.length->load());
        ls.division  = (int) std::lround (lp.division->load());
        ls.direction = (int) std::lround (lp.direction->load());
        ls.depth     = lp.depth->load();
        ls.mode      = (int) std::lround (lp.mode->load());
        ls.nudge     = lp.nudge->load();
        ls.humanize  = lp.humanize->load();
        ls.ccOn      = lp.ccOn->load() > 0.5f;
        ls.ccNumber  = (int) std::lround (lp.ccNumber->load());
        ls.ccChannel = (int) std::lround (lp.ccChannel->load());
    }

    s.outputMode    = (int) std::lround (pOutputMode->load());
    s.triggerSource = (int) std::lround (pTriggerSrc->load());
    s.quantize      = pQuantize->load() > 0.5f;
    s.bendRange     = (int) std::lround (pBendRange->load());
    s.root          = (int) std::lround (pRootNote->load());
    s.rangeSteps    = (int) std::lround (pRangeSteps->load());
    s.scale         = (int) std::lround (pScale->load());
    s.velocity      = (int) std::lround (pVelocity->load());
    s.midiChannel   = (int) std::lround (pMidiChannel->load());
    s.ccNumber      = (int) std::lround (pCcNumber->load());
    s.ccChannel     = (int) std::lround (pCcChannel->load());
    s.offset        = pOffset->load();
    s.slewMs        = pSlew->load();
    s.swing         = pSwing->load();
    s.voiceCount    = (int) std::lround (pVoiceCount->load());
    s.polyMode      = pPolyMode->load() > 0.5f;

    return s;
}

//==============================================================================
void RavelAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;

    // Silent instrument -- all of our output is MIDI.
    buffer.clear();

    const int numSamples = buffer.getNumSamples();

    // This is a generator, not a transformer: incoming MIDI is discarded so the
    // buffer we hand back contains only our own events.
    midiMessages.clear();

    double bpm         = 120.0;
    double ppqAtStart  = 0.0;
    bool   hostPlaying = false;
    bool   havePpq     = false;

    if (auto* hostPlayHead = getPlayHead())
    {
        if (const auto position = hostPlayHead->getPosition())
        {
            if (const auto hostBpm = position->getBpm())
                bpm = *hostBpm;

            if (const auto ppq = position->getPpqPosition())
            {
                ppqAtStart = *ppq;
                havePpq = true;
            }

            hostPlaying = position->getIsPlaying();
        }
    }

    const double sampleRate   = getSampleRate();
    const double ppqPerSample = (sampleRate > 0.0 && bpm > 0.0) ? (bpm / 60.0 / sampleRate) : 0.0;

    const bool freeRunEnabled = pFreeRun != nullptr && pFreeRun->load() > 0.5f;

    bool running = hostPlaying && havePpq;

    if (running)
    {
        // Track the host so free-run picks up from wherever the transport stopped.
        freeRunPpq = ppqAtStart + ppqPerSample * (double) numSamples;
    }
    else if (freeRunEnabled)
    {
        ppqAtStart  = freeRunPpq;
        freeRunPpq += ppqPerSample * (double) numSamples;
        running     = true;
    }

    engine.process (buildSnapshot(), midiMessages, numSamples, ppqAtStart, ppqPerSample, running);
}

//==============================================================================
juce::AudioProcessorEditor* RavelAudioProcessor::createEditor()
{
    return new RavelAudioProcessorEditor (*this);
}

void RavelAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (const auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void RavelAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (const auto xml = getXmlFromBinary (data, sizeInBytes))
    {
        // "TRILANE" is the tag a session saved before the TriLane -> Ravel rename carries;
        // accepting it here keeps those old sessions' patterns loading instead of silently
        // resetting to defaults.
        if (xml->hasTagName (apvts.state.getType()) || xml->hasTagName ("TRILANE"))
        {
            auto tree = juce::ValueTree::fromXml (*xml);
            migrateFixedThreeLaneState (tree);
            apvts.replaceState (tree);

            // Whatever this instance held before the host handed it a session is not a state
            // the user chose, so it is not one Ctrl+Z should be able to walk back into.
            undoHistory.clear();
        }
    }
}

//==============================================================================
void RavelAudioProcessor::migrateFixedThreeLaneState (juce::ValueTree& tree)
{
    // Anything APVTS does not find in the tree comes back as that parameter's default, so a
    // session written before lanes were countable would silently load as a one-lane instance
    // with its other two patterns hidden. The absence of lane_count is what identifies such a
    // session, and three is what it was written with.
    juce::ValueTree laneCount, trigger;

    for (int i = 0; i < tree.getNumChildren(); ++i)
    {
        const auto child = tree.getChild (i);
        const auto id = child.getProperty ("id").toString();

        if (id == params::laneCountId) laneCount = child;
        else if (id == params::triggerSrcId) trigger = child;
    }

    if (laneCount.isValid())
        return;

    juce::ValueTree added ("PARAM");
    added.setProperty ("id", params::laneCountId, nullptr);
    added.setProperty ("value", 3.0f, nullptr);
    tree.appendChild (added, nullptr);

    // Trigger gained a "Lane 4" entry ahead of "Any Lane", so the old index for Any -- which
    // was the end of a three-lane list -- now names a lane instead of naming all of them.
    if (trigger.isValid() && (int) std::lround ((float) trigger.getProperty ("value")) == 3)
        trigger.setProperty ("value", (float) params::numLanes, nullptr);
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new RavelAudioProcessor();
}
