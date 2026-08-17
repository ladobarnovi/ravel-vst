#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
TriLaneAudioProcessor::TriLaneAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "TRILANE", params::createParameterLayout())
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
        }

        lp.length    = apvts.getRawParameterValue (params::laneLengthId (lane));
        lp.division  = apvts.getRawParameterValue (params::laneDivId (lane));
        lp.direction = apvts.getRawParameterValue (params::laneDirId (lane));
        lp.depth     = apvts.getRawParameterValue (params::laneDepthId (lane));
        lp.mode      = apvts.getRawParameterValue (params::laneModeId (lane));
        lp.nudge     = apvts.getRawParameterValue (params::laneNudgeId (lane));
        lp.humanize  = apvts.getRawParameterValue (params::laneHumanizeId (lane));
        lp.velocity  = apvts.getRawParameterValue (params::laneVelocityId (lane));
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
    pGateLength  = apvts.getRawParameterValue (params::gateLengthId);
    pMidiChannel = apvts.getRawParameterValue (params::midiChannelId);
    pCcNumber    = apvts.getRawParameterValue (params::ccNumberId);
    pCcChannel   = apvts.getRawParameterValue (params::ccChannelId);
    pOffset      = apvts.getRawParameterValue (params::offsetId);
    pSlew        = apvts.getRawParameterValue (params::slewId);
    pFreeRun     = apvts.getRawParameterValue (params::freeRunId);
    pSwing       = apvts.getRawParameterValue (params::swingId);
    pVoiceCount  = apvts.getRawParameterValue (params::voiceCountId);
    pPolyMode    = apvts.getRawParameterValue (params::polyModeId);
}

//==============================================================================
void TriLaneAudioProcessor::prepareToPlay (double sampleRate, int)
{
    engine.prepare (sampleRate);
    freeRunPpq = 0.0;
}

void TriLaneAudioProcessor::releaseResources()
{
}

void TriLaneAudioProcessor::reset()
{
    engine.reset();
    freeRunPpq = 0.0;
}

bool TriLaneAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo();
}

//==============================================================================
SequencerEngine::Snapshot TriLaneAudioProcessor::buildSnapshot() const
{
    SequencerEngine::Snapshot s;

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
        }

        ls.length    = (int) std::lround (lp.length->load());
        ls.division  = (int) std::lround (lp.division->load());
        ls.direction = (int) std::lround (lp.direction->load());
        ls.depth     = lp.depth->load();
        ls.mode      = (int) std::lround (lp.mode->load());
        ls.nudge     = lp.nudge->load();
        ls.humanize  = lp.humanize->load();
        ls.velocityScale = lp.velocity->load();
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
    s.gatePercent   = pGateLength->load();
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
void TriLaneAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
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
juce::AudioProcessorEditor* TriLaneAudioProcessor::createEditor()
{
    return new TriLaneAudioProcessorEditor (*this);
}

void TriLaneAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (const auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void TriLaneAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (const auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new TriLaneAudioProcessor();
}
