
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
 
 PluginProcessor.h: This file defines the core interface for Imogen's AudioProcessor as a whole. The class ImogenAudioProcessor is the top-level object that represents an instance of Imogen.
 
======================================================================================================================================================*/


#pragma once

#include "ImogenEngine/ImogenEngine.h"

#include "ImogenCommon/ImogenCommon.h"

#include <../../third-party/ableton-link/include/ableton/Link.hpp>


class ImogenAudioProcessorEditor;  // forward declaration...


struct ImogenGUIUpdateReciever
{
    virtual void applyValueTreeStateChange (const void* encodedChangeData, size_t encodedChangeDataSize) = 0;
};



/*
*/

class ImogenAudioProcessor    : public  juce::AudioProcessor,
                                private juce::Timer
{
    using Parameter     = bav::Parameter;
    
    using RAP = juce::RangedAudioParameter;
    
    using ParameterID = Imogen::ParameterID;
    using MeterID = Imogen::MeterID;
    using NonAutomatableParameterID = Imogen::NonAutomatableParameterID;
    
    using ChangeDetails = juce::AudioProcessorListener::ChangeDetails;

    
public:
    ImogenAudioProcessor();
    ~ImogenAudioProcessor() override;
    
    /*=========================================================================================*/
    
    void timerCallback() override final;
    
    /*=========================================================================================*/
    /* juce::AudioProcessor functions */

    void prepareToPlay (double sampleRate, int samplesPerBlock) override final;
    
    void releaseResources() override final;
    
    void reset() override final;
    
    void processBlock (juce::AudioBuffer<float>& buffer,  juce::MidiBuffer& midiMessages) override final;
    void processBlock (juce::AudioBuffer<double>& buffer, juce::MidiBuffer& midiMessages) override final;
    
    void processBlockBypassed (juce::AudioBuffer<float>& buffer,  juce::MidiBuffer& midiMessages) override final;
    void processBlockBypassed (juce::AudioBuffer<double>& buffer, juce::MidiBuffer& midiMessages) override final;
    
    bool canAddBus (bool isInput) const override final { return isInput; }
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override final;
    
    double getTailLengthSeconds() const override final;
    
    void getStateInformation (juce::MemoryBlock& destData) override final;
    void setStateInformation (const void* data, int sizeInBytes) override final;
    
    juce::AudioProcessorParameter* getBypassParameter() const override final { return mainBypassPntr; }
    
    int getNumPrograms() override final { return 1; }
    int getCurrentProgram() override final { return 0; }
    void setCurrentProgram (int) override final { }
    const juce::String getProgramName (int) override final { return {}; }
    void changeProgramName (int, const juce::String&) override final { }
    
    bool acceptsMidi()  const override final { return true;  }
    bool producesMidi() const override final { return true;  }
    bool supportsMPE()  const override final { return false; }
    bool isMidiEffect() const override final { return false; }
    
    const juce::String getName() const override final { return JucePlugin_Name; }
    juce::StringArray getAlternateDisplayNames() const override final { return { "Imgn" }; }
    
    bool hasEditor() const override final;
    
    juce::AudioProcessorEditor* createEditor() override final;
    
    bool supportsDoublePrecisionProcessing() const override final { return true; }
    
    /*=========================================================================================*/
    
    inline bool isMidiLatched() const;
    
    bool isAbletonLinkEnabled() const;
    
    int getNumAbletonLinkSessionPeers() const;
    
    bool isConnectedToMtsEsp() const noexcept;
    
    juce::String getScaleName() const;
    
    /*=========================================================================================*/
    
    juce::Point<int> getLastEditorSize() const { return savedEditorSize; }
    
    void saveEditorSize (int width, int height);
    
    /*=========================================================================================*/
    
    void applyValueTreeStateChange (const void* encodedChangeData, size_t encodedChangeDataSize);
    
    /*=========================================================================================*/
    
private:
    
    inline void actionAllParameterUpdates()
    {
        for (auto* pntr : parameterPointers)
            pntr->doAction();
    }
    
    inline void actionAllPropertyUpdates()
    {
        for (auto* pntr : propertyPointers)
            pntr->doAction();
    }
    
    inline void resetParameterDefaultsToCurrentValues()
    {
        for (auto* pntr : parameterPointers)
            pntr->refreshDefault();
    }
    
    /*=========================================================================================*/
    /* Initialization functions */
    
    template <typename SampleType>
    void initialize (bav::ImogenEngine<SampleType>& activeEngine);
    
    BusesProperties makeBusProperties() const;
    
    template <typename SampleType>
    void initializeParameterFunctionPointers (bav::ImogenEngine<SampleType>& engine);
    
    template <typename SampleType>
    void initializePropertyActions (bav::ImogenEngine<SampleType>& engine);
    
    void initializePropertyValueUpdatingFunctions();
    
    /*=========================================================================================*/
    
    template <typename SampleType1, typename SampleType2>
    void prepareToPlayWrapped (const double sampleRate,
                               bav::ImogenEngine<SampleType1>& activeEngine,
                               bav::ImogenEngine<SampleType2>& idleEngine);
    
    template <typename SampleType>
    inline void processBlockWrapped (juce::AudioBuffer<SampleType>& buffer,
                                     juce::MidiBuffer& midiMessages,
                                     bav::ImogenEngine<SampleType>& engine,
                                     const bool isBypassedThisCallback);
    
    /*=========================================================================================*/
    
    void changeMidiLatchState (bool isNowLatched);
    
    /*=========================================================================================*/
    
    void updateMeters (ImogenMeterData meterData);
    
    void updateProperties()
    {
        for (auto* pntr : propertyPointers)
            pntr->updateValueFromExternalSource();
    }
    
    /*=========================================================================================*/

    ImogenGUIUpdateReciever* getActiveGuiEventReciever() const;
    
    Parameter* getParameterPntr (const ParameterID paramID) const;
    
    inline Parameter* getMeterParamPntr (const MeterID meterID) const;
    
    void saveEditorSizeToValueTree();
    
    void updateEditorSizeFromValueTree();
    
    bav::NonParamValueTreeNode* getPropertyPntr (const NonAutomatableParameterID propID) const;
    
    /*=========================================================================================*/
    
    // one engine of each type. The idle one isn't destroyed, but takes up few resources.
    bav::ImogenEngine<float>  floatEngine;
    bav::ImogenEngine<double> doubleEngine;
    
    /*=========================================================================================*/
    
    juce::ValueTree state;
    
    juce::OwnedArray< bav::ParameterAttachment >             parameterTreeAttachments;  // these are two-way
    juce::OwnedArray< bav::ParameterToValueTreeAttachment >  meterTreeAttachments;  // these are write-only
    juce::OwnedArray< bav::PropertyAttachmentBase >          propertyValueTreeAttachments;  // these are two-way
    
    struct ValueTreeSynchronizer  :   public juce::ValueTreeSynchroniser
    {
        ValueTreeSynchronizer (const juce::ValueTree& vtree, ImogenAudioProcessor& p)
            : juce::ValueTreeSynchroniser(vtree), processor(p) { }
        
        void stateChanged (const void* encodedChange, size_t encodedChangeSize) override final
        {
            if (auto* editor = processor.getActiveGuiEventReciever())
            {
                editor->applyValueTreeStateChange (encodedChange, encodedChangeSize);
            }
            
            // transmit to OSC...
        }
        
        ImogenAudioProcessor& processor;
    };
    
    ValueTreeSynchronizer treeSync;
    
    std::unique_ptr<bav::NonParamValueTreeNodeGroup> properties;
    
    /*=========================================================================================*/
    
    std::vector< Parameter* > parameterPointers;
    RAP* mainBypassPntr;  // this one gets referenced specifically...
    
    std::vector< Parameter* > meterParameterPointers;
    
    std::vector< bav::NonParamValueTreeNode* > propertyPointers;
    
    juce::NormalisableRange<float> pitchbendNormalizedRange { 0.0f, 127.0f, 1.0f }; // range object used to scale pitchbend values
    
    juce::Point<int> savedEditorSize;
    
    ableton::Link abletonLink;  // this object represents the plugin as a participant in an Ableton Link session
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ImogenAudioProcessor)
};