/*
    Part of module: bv_Harmonizer
    Parent file: bv_Harmonizer.h
    Classes: HarmonizerVoice
*/


#include "bv_Harmonizer/bv_Harmonizer.h"


namespace bav

{
    

template<typename SampleType>
HarmonizerVoice<SampleType>::HarmonizerVoice(Harmonizer<SampleType>* h):
parent(h), currentlyPlayingNote(-1), currentOutputFreq(-1.0f), noteOnTime(0), currentVelocityMultiplier(0.0f), prevVelocityMultiplier(0.0f), lastRecievedVelocity(0.0f), isQuickFading(false), noteTurnedOff(true), keyIsDown(false), currentAftertouch(0), prevSoftPedalMultiplier(1.0f),
    isPedalPitchVoice(false), isDescantVoice(false),
    playingButReleased(false)
{
    const double initSamplerate = 44100.0;
    
    adsr        .setSampleRate (initSamplerate);
    quickRelease.setSampleRate (initSamplerate);
    quickAttack .setSampleRate (initSamplerate);
    
    adsr        .setParameters (parent->getCurrentAdsrParams());
    quickRelease.setParameters (parent->getCurrentQuickReleaseParams());
    quickAttack .setParameters (parent->getCurrentQuickAttackParams());
}

template<typename SampleType>
HarmonizerVoice<SampleType>::~HarmonizerVoice()
{ }

    
template<typename SampleType>
void HarmonizerVoice<SampleType>::prepare (const int blocksize)
{
    // blocksize = maximum period
    
    jassert (blocksize > 0);
    
    constexpr SampleType zero = SampleType(0.0);
    
    synthesisBuffer.setSize (1, blocksize * 4);
    FloatVectorOperations::fill (synthesisBuffer.getWritePointer(0), zero, synthesisBuffer.getNumSamples());
    nextSBindex = 0;
    
    windowingBuffer.setSize (1, blocksize * 2);
    FloatVectorOperations::fill (windowingBuffer.getWritePointer(0), zero, windowingBuffer.getNumSamples());
    
    copyingBuffer.setSize (1, blocksize * 2);
    FloatVectorOperations::fill (copyingBuffer.getWritePointer(0), zero, copyingBuffer.getNumSamples());
    
    prevVelocityMultiplier = currentVelocityMultiplier;
    
    prevSoftPedalMultiplier = parent->getSoftPedalMultiplier();
}


template<typename SampleType>
void HarmonizerVoice<SampleType>::renderNextBlock (const AudioBuffer<SampleType>& inputAudio, AudioBuffer<SampleType>& outputBuffer,
                                                   const int origPeriod,
                                                   const Array<int>& indicesOfGrainOnsets,
                                                   const AudioBuffer<SampleType>& windowToUse)
{
    // determine if the voice is currently active
    
    const bool voiceIsOnRightNow = isQuickFading ? quickRelease.isActive()
                                                 : (parent->isADSRon() ? adsr.isActive() : ! noteTurnedOff);
    
    if (! voiceIsOnRightNow)
    {
        clearCurrentNote();
        return;
    }
    
    const int numSamples = inputAudio.getNumSamples();
    
    // puts shifted samples into the synthesisBuffer, from sample indices starting to starting + numSamples - 1
    sola (inputAudio.getReadPointer(0), numSamples,
          origPeriod,
          roundToInt (parent->getSamplerate() / currentOutputFreq),  // new desired period, in samples
          indicesOfGrainOnsets,
          windowToUse.getReadPointer(0));  // precomputed window length must be input period * 2
    
    //  *****************************************************************************************************
    //  midi velocity gain (aftertouch is applied here as well)

    float velocityMultNow;

    if (parent->isAftertouchGainOn())
    {
        constexpr float inv127 = 1.0f / 127.0f;
        velocityMultNow = currentVelocityMultiplier + ((currentAftertouch * inv127) * (1.0f - currentVelocityMultiplier));
    }
    else
    {
        velocityMultNow = currentVelocityMultiplier;
    }

    synthesisBuffer.applyGainRamp (0, numSamples, prevVelocityMultiplier, velocityMultNow);
    prevVelocityMultiplier = velocityMultNow;

    //  *****************************************************************************************************
    //  soft pedal gain

    const float softPedalMult = parent->isSoftPedalDown() ? parent->getSoftPedalMultiplier() : 1.0f; // soft pedal gain
    synthesisBuffer.applyGainRamp (0, numSamples, prevSoftPedalMultiplier, softPedalMult);
    prevSoftPedalMultiplier = softPedalMult;

    //  *****************************************************************************************************
    //  playing-but-released gain

    const float newPBRmult = playingButReleased ? parent->getPlayingButReleasedMultiplier() : 1.0f;
    synthesisBuffer.applyGainRamp (0, numSamples, lastPBRmult, newPBRmult);
    lastPBRmult = newPBRmult;

    //  *****************************************************************************************************
    //  ADSR

    if (parent->isADSRon())
        adsr.applyEnvelopeToBuffer (synthesisBuffer, 0, numSamples); // midi-triggered adsr envelope
    else
        quickAttack.applyEnvelopeToBuffer (synthesisBuffer, 0, numSamples); // to prevent pops at start of notes if adsr is off

    if (isQuickFading)  // quick fade out for stopNote w/ no tail off, to prevent clicks from jumping to 0
        quickRelease.applyEnvelopeToBuffer (synthesisBuffer, 0, numSamples);
    
    //  *****************************************************************************************************
    
    //  write to output
    
    for (int chan = 0; chan < 2; ++chan)
        outputBuffer.addFromWithRamp (chan, 0, synthesisBuffer.getReadPointer(0), numSamples,
                                      panner.getPrevGain(chan), panner.getGainMult(chan));  // panning is applied while writing to output
    
    moveUpSamples (numSamples);
}


template<typename SampleType>
void HarmonizerVoice<SampleType>::sola (const SampleType* input, const int totalNumInputSamples,
                                        const int origPeriod, // size of analysis grains = origPeriod * 2
                                        const int newPeriod,  // OLA hop size
                                        const Array<int>& indicesOfGrainOnsets, // sample indices marking the beginning of each analysis grain
                                        const SampleType* window) // Hanning window, length origPeriod * 2
{
    const int sampsLeftInBuffer = synthesisBuffer.getNumSamples() - nextSBindex;
    
    if (sampsLeftInBuffer < 1)
        return;
    
    // fill from the last written sample to the end of the buffer with zeroes
    constexpr SampleType zero = SampleType(0.0);
    FloatVectorOperations::fill (synthesisBuffer.getWritePointer(0) + nextSBindex,
                                 zero,
                                 sampsLeftInBuffer);
    
    if (nextSBindex > totalNumInputSamples)  // don't need this frame, we've written enough samples from previous frames...
        return;
    
    const int analysisGrainLength = 2 * origPeriod; // length of the analysis grains & the pre-computed Hanning window
    
    int thisFrameWritingStart = nextSBindex;
    
    for (int grainStart : indicesOfGrainOnsets)
    {
        const int grainEnd = grainStart + analysisGrainLength;
        
        if (grainEnd >= totalNumInputSamples)
        {
            thisFrameWritingStart = totalNumInputSamples;
            break;
        }

        olaFrame (input, grainStart, grainEnd, window, newPeriod, thisFrameWritingStart);
        
        thisFrameWritingStart = grainEnd + 1;
    }
    
    nextSBindex = thisFrameWritingStart;
    jassert (nextSBindex <= totalNumInputSamples);
}


template<typename SampleType>
void HarmonizerVoice<SampleType>::olaFrame (const SampleType* inputAudio,
                                            const int frameStartSample, const int frameEndSample,
                                            const SampleType* window,
                                            const int newPeriod,
                                            const int synthesisBufferStartIndex)
{
    if (synthesisBufferStartIndex > frameEndSample)
        return;
    
    // this function processes one analysis frame of input samples, from readingStartSample to readingEndSample
    // the frame size should be 2 * the original input period, and the length of the window passed to this function MUST equal this value!
    
    const int frameSize = frameEndSample - frameStartSample;  // frame size should equal the original period * 2
    
    // apply the window before OLAing. Writes windowed input samples into the windowingBuffer
    FloatVectorOperations::multiply (windowingBuffer.getWritePointer(0),
                                     window,
                                     inputAudio + frameStartSample,
                                     frameSize);
    
    // resynthesis / OLA
    
    const SampleType* windowBufferReading = windowingBuffer.getReadPointer(0);
    SampleType* synthesisBufferWriting = synthesisBuffer.getWritePointer(0);
    
    int synthesisIndex = synthesisBufferStartIndex;
    
    do {
        jassert (synthesisIndex + frameSize < synthesisBuffer.getNumSamples());
        
        FloatVectorOperations::add (synthesisBufferWriting + synthesisIndex,
                                    windowBufferReading,
                                    frameSize);
        
        synthesisIndex += newPeriod;
    }
    while (synthesisIndex < frameEndSample);
}


template<typename SampleType>
void HarmonizerVoice<SampleType>::moveUpSamples (const int numSamplesUsed)
{
    const int numSamplesLeft = nextSBindex - numSamplesUsed;
    
    if (numSamplesLeft < 1)
    {
        synthesisBuffer.clear();
        nextSBindex = 0;
        return;
    }
    
    copyingBuffer.copyFrom (0, 0, synthesisBuffer, 0, numSamplesUsed, numSamplesLeft);
    synthesisBuffer.clear();
    synthesisBuffer.copyFrom (0, 0, copyingBuffer, 0, 0, numSamplesLeft);
    
    nextSBindex = numSamplesLeft;
}


template<typename SampleType>
void HarmonizerVoice<SampleType>::releaseResources()
{
    synthesisBuffer.setSize(0, 0, false, false, false);
}


template<typename SampleType>
void HarmonizerVoice<SampleType>::clearBuffers()
{
    synthesisBuffer.clear();
}


// this function is called to reset the HarmonizerVoice's internal state to neutral / initial
template<typename SampleType>
void HarmonizerVoice<SampleType>::clearCurrentNote()
{
    currentVelocityMultiplier = 0.0f;
    lastRecievedVelocity      = 0.0f;
    currentAftertouch = 0;
    currentlyPlayingNote = -1;
    noteOnTime    = 0;
    isQuickFading = false;
    noteTurnedOff = true;
    keyIsDown     = false;
    playingButReleased = false;
    nextSBindex = 0;
    isPedalPitchVoice = false;
    isDescantVoice = false;
    sustainingFromSostenutoPedal = false;
    
    setPan (64);
    
    if (quickRelease.isActive())
        quickRelease.reset();
    
    quickRelease.noteOn();
    
    if (adsr.isActive())
        adsr.reset();
    
    if (quickAttack.isActive())
        quickAttack.reset();
    
    clearBuffers();
    synthesisBuffer.clear();
}


// MIDI -----------------------------------------------------------------------------------------------------------
template<typename SampleType>
void HarmonizerVoice<SampleType>::startNote (const int midiPitch, const float velocity,
                                             const uint32 noteOnTimestamp,
                                             const bool keyboardKeyIsDown,
                                             const bool isPedal, const bool isDescant)
{
    noteOnTime = noteOnTimestamp;
    currentlyPlayingNote = midiPitch;
    lastRecievedVelocity = velocity;
    currentVelocityMultiplier = parent->getWeightedVelocity (velocity);
    currentOutputFreq = parent->getOutputFrequency (midiPitch);
    isQuickFading = false;
    noteTurnedOff = false;
    
    adsr.noteOn();
    quickAttack.noteOn();
    
    if (! quickRelease.isActive())
        quickRelease.noteOn();
    
    isPedalPitchVoice = isPedal;
    isDescantVoice = isDescant;
    
    setKeyDown (keyboardKeyIsDown);
}
    
    
template<typename SampleType>
void HarmonizerVoice<SampleType>::setKeyDown (bool isNowDown) noexcept
{
    keyIsDown = isNowDown;
    
    if (isNowDown)
        playingButReleased = false;
    else
        if (isPedalPitchVoice || isDescantVoice)
            playingButReleased = false;
        else if (parent->isLatched() || parent->intervalLatchIsOn)
            playingButReleased = false;
        else
            playingButReleased = isVoiceActive();
}


template<typename SampleType>
void HarmonizerVoice<SampleType>::stopNote (const float velocity, const bool allowTailOff)
{
    ignoreUnused (velocity);
    
    if (allowTailOff)
    {
        adsr.noteOff();
        isQuickFading = false;
    }
    else
    {
        if (! quickRelease.isActive())
            quickRelease.noteOn();
        
        isQuickFading = true;
        
        quickRelease.noteOff();
    }
    
    noteTurnedOff = true;
    keyIsDown = false;
    playingButReleased = false;
}


template<typename SampleType>
void HarmonizerVoice<SampleType>::aftertouchChanged (const int newAftertouchValue)
{
    currentAftertouch = newAftertouchValue;
}


template<typename SampleType>
void HarmonizerVoice<SampleType>::setPan (int newPan)
{
    newPan = jlimit(0, 127, newPan);
    
    if (panner.getLastMidiPan() != newPan)
        panner.setMidiPan (newPan);
}


template<typename SampleType>
void HarmonizerVoice<SampleType>::updateSampleRate (const double newSamplerate)
{
    adsr        .setSampleRate (newSamplerate);
    quickRelease.setSampleRate (newSamplerate);
    quickAttack .setSampleRate (newSamplerate);
    
    adsr        .setParameters (parent->getCurrentAdsrParams());
    quickRelease.setParameters (parent->getCurrentQuickReleaseParams());
    quickAttack .setParameters (parent->getCurrentQuickAttackParams());
}


/*+++++++++++++++++++++++++++++++++++++
 DANGER!!!
 FOR NON REAL TIME ONLY!!!!!!!!
 ++++++++++++++++++++++++++++++++++++++*/
template<typename SampleType>
void HarmonizerVoice<SampleType>::increaseBufferSizes(const int newMaxBlocksize)
{
    synthesisBuffer.setSize(1, newMaxBlocksize, true, true, true);
}


template class HarmonizerVoice<float>;
template class HarmonizerVoice<double>;


} // namespace
