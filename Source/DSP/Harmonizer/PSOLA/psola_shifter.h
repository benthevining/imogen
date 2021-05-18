/*======================================================================================================================================================
           _             _   _                _                _                 _               _
          /\ \          /\_\/\_\ _           /\ \             /\ \              /\ \            /\ \     _
          \ \ \        / / / / //\_\        /  \ \           /  \ \            /  \ \          /  \ \   /\_\
          /\ \_\      /\ \/ \ \/ / /       / /\ \ \         / /\ \_\          / /\ \ \        / /\ \ \_/ / /
         / /\/_/     /  \____\__/ /       / / /\ \ \       / / /\/_/         / / /\ \_\      / / /\ \___/ /
        / / /       / /\/________/       / / /  \ \_\     / / / ______      / /_/_ \/_/     / / /  \/____/
       / / /       / / /\/_// / /       / / /   / / /    / / / /\_____\    / /____/\       / / /    / / /
      / / /       / / /    / / /       / / /   / / /    / / /  \/____ /   / /\____\/      / / /    / / /
  ___/ / /__     / / /    / / /       / / /___/ / /    / / /_____/ / /   / / /______     / / /    / / /
 /\__\/_/___\    \/_/    / / /       / / /____\/ /    / / /______\/ /   / / /_______\   / / /    / / /
 \/_________/            \/_/        \/_________/     \/___________/    \/__________/   \/_/     \/_/
 
 
 This file is part of the Imogen codebase.
 
 @2021 by Ben Vining. All rights reserved.
 
 psola_shifter.h :     This file defines a class that performs the resynthesis stage of the PSOLA process.
 
======================================================================================================================================================*/


namespace bav
{
    
template<typename SampleType>
class PsolaShifter
{
    using Analyzer = PsolaAnalyzer<SampleType>;
    using Synthesis_Grain = SynthesisGrain<SampleType>;
    
public:
    PsolaShifter (Analyzer* parentAnalyzer): analyzer(parentAnalyzer)
    {
        jassert (analyzer != nullptr);
    }
    
    
    void newBlockComing (int prevBlocksize) noexcept
    {
    //    nextSynthesisPitchMark = std::max(0, nextSynthesisPitchMark - prevBlocksize);
        
        juce::ignoreUnused (prevBlocksize);
        
        nextAnalysisPitchMark  = 0;
        nextSynthesisPitchMark = 0;
        
        //nextSynthesisPitchMark -= prevBlocksize;
    }
    
    
    void bypassedBlockRecieved (int numSamples)
    {
        for (auto* grain : synthesisGrains)
            grain->stop();
        
        juce::ignoreUnused (numSamples);
        
 //       nextSynthesisPitchMark = std::max(0, nextSynthesisPitchMark - numSamples);
    }
    
    
    void getSamples (SampleType* outputSamples, const int numSamples, const int newPeriod, const int origPeriod)
    {
        for (int i = 0; i < numSamples; ++i)
            outputSamples[i] = getNextSample (newPeriod, origPeriod);
    }
    
    
    SampleType getNextSample (const int newPeriod, const int origPeriod)
    {
        jassert (synthesisGrains.size() == numSynthesisGrains);
        jassert (newPeriod > 0 && origPeriod > 0);
        
        if (! anyGrainsAreActive())
            startNewGrain (newPeriod, origPeriod, nullptr);
        
        auto sample = SampleType(0);
        
        for (auto* grain : synthesisGrains)
        {
            if (! grain->isActive())
                continue;
            
            sample += grain->getNextSample();
            
            if (! grain->isActive() || grain->isHalfwayThrough())
                startNewGrain (newPeriod, origPeriod, grain->orig());
        }
        
//        if (nextSynthesisPitchMark > 0)
//            --nextSynthesisPitchMark;
        
        return sample;
    }
    
    
    void prepare()
    {
        while (synthesisGrains.size() < numSynthesisGrains)
            synthesisGrains.add (new Synthesis_Grain());
    }
    
    
    void reset()
    {
 //       nextSynthesisPitchMark = 0;
        
        for (auto* grain : synthesisGrains)
            grain->stop();
    }
    
    void releaseResources()
    {
        nextSynthesisPitchMark = 0;
        synthesisGrains.clear();
    }
    
    
private:
    
    inline void startNewGrain (const int newPeriod, const int origPeriod, AnalysisGrain<SampleType>* lastGrain)
    {
        juce::ignoreUnused (lastGrain);
        
        if (auto* newGrain = getAvailableGrain())
        {
 //           auto* analysisGrain = lastGrain == nullptr ? analyzer->findClosestGrain (bufferPos) : analyzer->findBestNewGrain (lastGrain);
            
 //           const auto bufferPos = anyGrainsAreActive() ? lastAnalysisPitchMark + origPeriod : 0;
            
            auto* analysisGrain = analyzer->findClosestGrain (nextAnalysisPitchMark);
            
            const auto samplesInFuture = juce::roundToInt (analysisGrain->percentOfExpectedSize() * (nextSynthesisPitchMark - nextAnalysisPitchMark));
            
            newGrain->startNewGrain (analysisGrain, samplesInFuture);
            
            nextSynthesisPitchMark += newPeriod;
            nextAnalysisPitchMark  += origPeriod;
        }
    }
    
    inline bool anyGrainsAreActive() const
    {
        for (auto* grain : synthesisGrains)
            if (grain->isActive())
                return true;
        
        return false;
    }
    
    inline Synthesis_Grain* getAvailableGrain()
    {
        for (auto* grain : synthesisGrains)
            if (! grain->isActive())
                return grain;
        
        return nullptr;
        
//        Synthesis_Grain* newGrain = nullptr;
//        int mostZeroesLeft = 0;
//
//        for (auto* grain : synthesisGrains)
//        {
//            const auto zeroesLeft = grain->silenceLeft();
//
//            if (newGrain == nullptr || zeroesLeft > mostZeroesLeft)
//            {
//                newGrain = grain;
//                mostZeroesLeft = zeroesLeft;
//            }
//        }
//
//        if (mostZeroesLeft == 0)
//            return nullptr;
//
//        return newGrain;
    }
    
    
    
    Analyzer* analyzer;
    
    juce::OwnedArray<Synthesis_Grain> synthesisGrains;
    
    int nextSynthesisPitchMark = 0;
    
    int nextAnalysisPitchMark = 0;
    
    static constexpr auto numSynthesisGrains = 48;
};


template class PsolaShifter<float>;
template class PsolaShifter<double>;
    
    
}  // namespace
