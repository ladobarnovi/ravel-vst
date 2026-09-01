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
        auto& np = noteLaneParams[lane];

        for (int step = 0; step < params::numSteps; ++step)
        {
            np.values[step]  = apvts.getRawParameterValue (params::stepValueId (lane, step));
            np.enabled[step] = apvts.getRawParameterValue (params::stepOnId (lane, step));
            np.chance[step]  = apvts.getRawParameterValue (params::stepChanceId (lane, step));
            np.stepVelocity[step] = apvts.getRawParameterValue (params::stepVelocityId (lane, step));
            np.stepGate[step] = apvts.getRawParameterValue (params::stepGateId (lane, step));
        }

        np.active    = apvts.getRawParameterValue (params::laneOnId (lane));
        np.length    = apvts.getRawParameterValue (params::laneLengthId (lane));
        np.division  = apvts.getRawParameterValue (params::laneDivId (lane));
        np.direction = apvts.getRawParameterValue (params::laneDirId (lane));
        np.depth     = apvts.getRawParameterValue (params::laneDepthId (lane));
        np.mode      = apvts.getRawParameterValue (params::laneModeId (lane));
        np.nudge     = apvts.getRawParameterValue (params::laneNudgeId (lane));
        np.humanize  = apvts.getRawParameterValue (params::laneHumanizeId (lane));

        auto& cp = ccLaneParams[lane];

        for (int step = 0; step < params::numSteps; ++step)
        {
            cp.values[step]  = apvts.getRawParameterValue (params::stepValueId (lane, step, params::LaneKind::cc));
            cp.enabled[step] = apvts.getRawParameterValue (params::stepOnId (lane, step, params::LaneKind::cc));
            cp.chance[step]  = apvts.getRawParameterValue (params::stepChanceId (lane, step, params::LaneKind::cc));
        }

        cp.active    = apvts.getRawParameterValue (params::laneOnId (lane, params::LaneKind::cc));
        cp.length    = apvts.getRawParameterValue (params::laneLengthId (lane, params::LaneKind::cc));
        cp.division  = apvts.getRawParameterValue (params::laneDivId (lane, params::LaneKind::cc));
        cp.direction = apvts.getRawParameterValue (params::laneDirId (lane, params::LaneKind::cc));
        cp.depth     = apvts.getRawParameterValue (params::laneDepthId (lane, params::LaneKind::cc));
        cp.mode      = apvts.getRawParameterValue (params::laneModeId (lane, params::LaneKind::cc));
        cp.nudge     = apvts.getRawParameterValue (params::laneNudgeId (lane, params::LaneKind::cc));
        cp.humanize  = apvts.getRawParameterValue (params::laneHumanizeId (lane, params::LaneKind::cc));
        cp.ccOn      = apvts.getRawParameterValue (params::laneCcOnId (lane));
        cp.ccNumber  = apvts.getRawParameterValue (params::laneCcNumId (lane));
        cp.ccChannel = apvts.getRawParameterValue (params::laneCcChanId (lane));
        cp.ccOffset  = apvts.getRawParameterValue (params::laneCcOffsetId (lane));
    }

    pNoteTriggerSrc = apvts.getRawParameterValue (params::noteTriggerSrcId);
    pQuantize      = apvts.getRawParameterValue (params::quantizeId);
    pBendRange     = apvts.getRawParameterValue (params::bendRangeId);
    pRootNote      = apvts.getRawParameterValue (params::rootNoteId);
    pRangeSteps    = apvts.getRawParameterValue (params::rangeStepsId);
    pScale         = apvts.getRawParameterValue (params::scaleId);
    pVelocity      = apvts.getRawParameterValue (params::velocityId);
    pMidiChannel   = apvts.getRawParameterValue (params::midiChannelId);
    pCcNumber      = apvts.getRawParameterValue (params::ccNumberId);
    pCcChannel     = apvts.getRawParameterValue (params::ccChannelId);
    pNoteOffset    = apvts.getRawParameterValue (params::noteOffsetId);
    pCcOffset      = apvts.getRawParameterValue (params::ccOffsetId);
    pSlew          = apvts.getRawParameterValue (params::slewId);
    pFreeRun       = apvts.getRawParameterValue (params::freeRunId);
    pNoteSwing     = apvts.getRawParameterValue (params::noteSwingId);
    pCcSwing       = apvts.getRawParameterValue (params::ccSwingId);
    pVoiceCount    = apvts.getRawParameterValue (params::voiceCountId);
    pPolyMode      = apvts.getRawParameterValue (params::polyModeId);
    pNoteLaneCount = apvts.getRawParameterValue (params::noteLaneCountId);
    pCcLaneCount   = apvts.getRawParameterValue (params::ccLaneCountId);
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

    const int noteLaneCount = juce::jlimit (1, params::numLanes,
                                            (int) std::lround (pNoteLaneCount->load()));
    const int ccLaneCount   = juce::jlimit (1, params::numLanes,
                                            (int) std::lround (pCcLaneCount->load()));

    for (int lane = 0; lane < params::numLanes; ++lane)
    {
        const auto& np = noteLaneParams[lane];
        auto& ns = s.noteLanes[lane];

        for (int step = 0; step < params::numSteps; ++step)
        {
            ns.values[step]  = np.values[step]->load();
            ns.enabled[step] = np.enabled[step]->load() > 0.5f;
            ns.chance[step]  = np.chance[step]->load();
            ns.velocity[step] = np.stepVelocity[step]->load();
            ns.gate[step]     = np.stepGate[step]->load();
        }

        // A lane the instance has not been given yet is inert in exactly the same way a
        // muted one is, so the engine needs to know about only the one flag.
        ns.active    = lane < noteLaneCount && np.active->load() > 0.5f;
        ns.length    = (int) std::lround (np.length->load());
        ns.division  = (int) std::lround (np.division->load());
        ns.direction = (int) std::lround (np.direction->load());
        ns.depth     = np.depth->load();
        ns.mode      = (int) std::lround (np.mode->load());
        ns.nudge     = np.nudge->load();
        ns.humanize  = np.humanize->load();

        const auto& cp = ccLaneParams[lane];
        auto& cs = s.ccLanes[lane];

        for (int step = 0; step < params::numSteps; ++step)
        {
            cs.values[step]  = cp.values[step]->load();
            cs.enabled[step] = cp.enabled[step]->load() > 0.5f;
            cs.chance[step]  = cp.chance[step]->load();
        }

        cs.active    = lane < ccLaneCount && cp.active->load() > 0.5f;
        cs.length    = (int) std::lround (cp.length->load());
        cs.division  = (int) std::lround (cp.division->load());
        cs.direction = (int) std::lround (cp.direction->load());
        cs.depth     = cp.depth->load();
        cs.mode      = (int) std::lround (cp.mode->load());
        cs.nudge     = cp.nudge->load();
        cs.humanize  = cp.humanize->load();
        cs.ccOn      = cp.ccOn->load() > 0.5f;
        cs.ccNumber  = (int) std::lround (cp.ccNumber->load());
        cs.ccChannel = (int) std::lround (cp.ccChannel->load());
        cs.ccOffset  = cp.ccOffset->load();
    }

    s.noteTriggerSource = (int) std::lround (pNoteTriggerSrc->load());
    s.quantize          = pQuantize->load() > 0.5f;
    s.bendRange         = (int) std::lround (pBendRange->load());
    s.root              = (int) std::lround (pRootNote->load());
    s.rangeSteps        = (int) std::lround (pRangeSteps->load());
    s.scale             = (int) std::lround (pScale->load());
    s.velocity          = (int) std::lround (pVelocity->load());
    s.midiChannel       = (int) std::lround (pMidiChannel->load());
    s.ccNumber          = (int) std::lround (pCcNumber->load());
    s.ccChannel         = (int) std::lround (pCcChannel->load());
    s.noteOffset        = pNoteOffset->load();
    s.ccOffset          = pCcOffset->load();
    s.slewMs            = pSlew->load();
    s.noteSwing         = pNoteSwing->load();
    s.ccSwing           = pCcSwing->load();
    s.voiceCount        = (int) std::lround (pVoiceCount->load());
    s.polyMode          = pPolyMode->load() > 0.5f;

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
            apvts.replaceState (tree);

            // Whatever this instance held before the host handed it a session is not a state
            // the user chose, so it is not one Ctrl+Z should be able to walk back into.
            undoHistory.clear();
        }
    }
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new RavelAudioProcessor();
}
