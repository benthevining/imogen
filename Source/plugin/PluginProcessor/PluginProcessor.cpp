
#if ! IMOGEN_HEADLESS
#    include "../PluginEditor/PluginEditor.h"
#endif

#include "PluginProcessor.h"


namespace Imogen
{
Processor::Processor()
: ProcessorBase (state.parameters)
{
    state.addTo (*this);

    if (isUsingDoublePrecision())
        initialize (doubleEngine);
    else
        initialize (floatEngine);

    dataSync.connect ("host");
}

Processor::~Processor()
{
    dataSync.disconnect();
}

template < typename SampleType >
inline void Processor::initialize (Engine< SampleType >& activeEngine)
{
    auto initSamplerate = getSampleRate();
    if (initSamplerate <= 0.0) initSamplerate = 44100.0;

    auto initBlockSize = getBlockSize();
    if (initBlockSize <= 0) initBlockSize = 512;

    activeEngine.initialize (initSamplerate, initBlockSize);

    setLatencySamples (activeEngine.reportLatency());

    updateHostDisplay();

    prepareToPlay (initSamplerate, 512);
}

void Processor::prepareToPlay (const double sampleRate, const int)
{
    if (isUsingDoublePrecision())
        prepareToPlayWrapped (sampleRate, doubleEngine, floatEngine);
    else
        prepareToPlayWrapped (sampleRate, floatEngine, doubleEngine);
}

template < typename SampleType1, typename SampleType2 >
inline void Processor::prepareToPlayWrapped (const double           sampleRate,
                                             Engine< SampleType1 >& activeEngine,
                                             Engine< SampleType2 >& idleEngine)
{
    if (! idleEngine.hasBeenReleased()) idleEngine.releaseResources();

    initializeParameterFunctionPointers (activeEngine);

    jassert (activeEngine.getLatency() > 0);

    activeEngine.prepare (sampleRate);

    parameters.doAllActions();

    setLatencySamples (activeEngine.reportLatency());

    updateHostDisplay();
}

bav::BoolParameter& Processor::getMainBypass() const
{
    return *parameters.mainBypass.get();
}

void Processor::releaseResources()
{
    if (! doubleEngine.hasBeenReleased()) doubleEngine.releaseResources();

    if (! floatEngine.hasBeenReleased()) floatEngine.releaseResources();
}

void Processor::renderChunk (juce::AudioBuffer< float >& audio, juce::MidiBuffer& midi)
{
    renderChunkInternal (floatEngine, audio, midi);
}

void Processor::renderChunk (juce::AudioBuffer< double >& audio, juce::MidiBuffer& midi)
{
    renderChunkInternal (doubleEngine, audio, midi);
}

template < typename SampleType >
void Processor::renderChunkInternal (Engine< SampleType >& engine, juce::AudioBuffer< SampleType >& audio, juce::MidiBuffer& midi)
{
    parameters.doAllActions();
    
    if (audio.getNumSamples() == 0 || audio.getNumChannels() == 0) return;

    auto inBus  = getBusBuffer (audio, true, getBusesLayout().getMainInputChannelSet() == juce::AudioChannelSet::disabled());
    auto outBus = getBusBuffer (audio, false, 0);

    engine.process (inBus, outBus, midi, parameters.mainBypass->get());

    updateMeters (engine.getLatestMeterData());
    updateInternals (engine.getLatestInternalsData());
}

void Processor::updateMeters (ImogenMeterData meterData)
{
    meters.inputLevel->set (meterData.inputLevel);
    meters.outputLevelL->set (meterData.outputLevelL);
    meters.outputLevelR->set (meterData.outputLevelR);
    meters.gateRedux->set (meterData.noiseGateGainReduction);
    meters.compRedux->set (meterData.compressorGainReduction);
    meters.deEssRedux->set (meterData.deEsserGainReduction);
    meters.limRedux->set (meterData.limiterGainReduction);
    meters.reverbLevel->set (meterData.reverbLevel);
    meters.delayLevel->set (meterData.delayLevel);
}

void Processor::updateInternals (ImogenInternalsData internalsData)
{
    internals.abletonLinkEnabled->set (transport.isAbletonLinkEnabled());
    internals.abletonLinkSessionPeers->set (transport.getNumAbletonLinkSessionPeers());
    internals.mtsEspIsConnected->set (internalsData.mtsEspConnected);
    internals.currentCentsSharp->set (internalsData.currentCentsSharp);
    internals.currentInputNote->set (internalsData.currentPitch);
    internals.lastMovedMidiController->set (internalsData.lastMovedMidiController);
    internals.lastMovedCCValue->set (internalsData.lastMovedControllerValue);
}

juce::String Processor::getScaleName() const
{
    return isUsingDoublePrecision() ? doubleEngine.getScaleName() : floatEngine.getScaleName();
}

double Processor::getTailLengthSeconds() const
{
    return parameters.adsrRelease->get();
}

juce::AudioProcessor::BusesProperties Processor::createBusProperties() const
{
    const auto stereo = juce::AudioChannelSet::stereo();
    const auto mono   = juce::AudioChannelSet::mono();

    return BusesProperties()
        .withInput (TRANS ("Input"), stereo, true)
        .withInput (TRANS ("Sidechain"), mono, false)
        .withOutput (TRANS ("Output"), stereo, true);
}

bool Processor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto disabled = juce::AudioChannelSet::disabled();

    if (layouts.getMainInputChannelSet() == disabled && layouts.getChannelSet (true, 1) == disabled) return false;

    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

template < typename SampleType >
void Processor::initializeParameterFunctionPointers (Imogen::Engine< SampleType >& engine)
{
    parameters.adsrAttack->setAction ([&engine] (float value)
                                      { engine.updateAdsrAttack (value); });
    parameters.adsrDecay->setAction ([&engine] (float value)
                                     { engine.updateAdsrDecay (value); });
    parameters.adsrSustain->setAction ([&engine] (int value)
                                       { engine.updateAdsrSustain (value); });
    parameters.adsrRelease->setAction ([&engine] (float value)
                                       { engine.updateAdsrRelease (value); });
    parameters.inputGain->setAction ([&engine] (float value)
                                     { engine.updateInputGain (value); });
    parameters.outputGain->setAction ([&engine] (float value)
                                      { engine.updateOutputGain (value); });
    parameters.noiseGateThresh->setAction ([&engine] (float value)
                                           { engine.updateNoiseGateThresh (value); });
    parameters.compAmount->setAction ([&engine] (int value)
                                      { engine.updateCompressorAmount (value); });
    parameters.deEsserThresh->setAction ([&engine] (float value)
                                         { engine.updateDeEsserThresh (value); });
    parameters.deEsserAmount->setAction ([&engine] (int value)
                                         { engine.updateDeEsserAmount (value); });
    parameters.reverbDecay->setAction ([&engine] (int value)
                                       { engine.updateReverbDecay (value); });
    parameters.reverbDuck->setAction ([&engine] (int value)
                                      { engine.updateReverbDuck (value); });
    parameters.reverbLoCut->setAction ([&engine] (float value)
                                       { engine.updateReverbLoCut (value); });
    parameters.reverbHiCut->setAction ([&engine] (float value)
                                       { engine.updateReverbHiCut (value); });

    parameters.inputMode->setAction ([&engine] (int value)
                                     { engine.setModulatorSource (value); });
    parameters.leadPan->setAction ([&engine] (int value)
                                   { engine.updateDryVoxPan (value); });
    parameters.stereoWidth->setAction ([&engine] (int value)
                                       { engine.updateStereoWidth (value); });
    parameters.lowestPanned->setAction ([&engine] (int value)
                                        { engine.updateLowestPannedNote (value); });
    parameters.velocitySens->setAction ([&engine] (int value)
                                        { engine.updateMidiVelocitySensitivity (value); });
    parameters.pitchbendRange->setAction ([&engine] (int value)
                                          { engine.updatePitchbendRange (value); });
    parameters.pedalThresh->setAction ([&engine] (int value)
                                       { engine.updatePedalThresh (value); });
    parameters.pedalInterval->setAction ([&engine] (int value)
                                         { engine.updatePedalInterval (value); });
    parameters.descantThresh->setAction ([&engine] (int value)
                                         { engine.updateDescantThresh (value); });
    parameters.descantInterval->setAction ([&engine] (int value)
                                           { engine.updateDescantInterval (value); });
    parameters.reverbDryWet->setAction ([&engine] (int value)
                                        { engine.updateReverbDryWet (value); });
    parameters.delayDryWet->setAction ([&engine] (int value)
                                       { engine.updateDelayDryWet (value); });
    parameters.editorPitchbend->setAction ([&engine] (int value)
                                           { engine.recieveExternalPitchbend (value); });

    parameters.midiLatch->setAction ([&engine] (bool value)
                                     { engine.updateMidiLatch (value); });
    parameters.leadBypass->setAction ([&engine] (bool value)
                                      { engine.updateLeadBypass (value); });
    parameters.harmonyBypass->setAction ([&engine] (bool value)
                                         { engine.updateHarmonyBypass (value); });
    parameters.pedalToggle->setAction ([&engine] (bool value)
                                       { engine.updatePedalToggle (value); });
    parameters.descantToggle->setAction ([&engine] (bool value)
                                         { engine.updateDescantToggle (value); });
    parameters.voiceStealing->setAction ([&engine] (bool value)
                                         { engine.updateNoteStealing (value); });
    parameters.limiterToggle->setAction ([&engine] (bool value)
                                         { engine.updateLimiter (value); });
    parameters.noiseGateToggle->setAction ([&engine] (bool value)
                                           { engine.updateNoiseGateToggle (value); });
    parameters.compToggle->setAction ([&engine] (bool value)
                                      { engine.updateCompressorToggle (value); });
    parameters.aftertouchToggle->setAction ([&engine] (bool value)
                                            { engine.updateAftertouchGainOnOff (value); });
    parameters.deEsserToggle->setAction ([&engine] (bool value)
                                         { engine.updateDeEsserToggle (value); });
    parameters.reverbToggle->setAction ([&engine] (bool value)
                                        { engine.updateReverbToggle (value); });
    parameters.delayToggle->setAction ([&engine] (bool value)
                                       { engine.updateDelayToggle (value); });
}

bool Processor::hasEditor() const
{
#if IMOGEN_HEADLESS
    return false;
#endif
    return true;
}

juce::AudioProcessorEditor* Processor::createEditor()
{
#if IMOGEN_HEADLESS
    return nullptr;
#else
    return new Editor (*this);
#endif
}


}  // namespace Imogen

// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new Imogen::Processor();
}