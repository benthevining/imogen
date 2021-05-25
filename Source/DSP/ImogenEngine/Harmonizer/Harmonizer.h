
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
 
 bv_Harmonizer.h: This file defines the interfaces for the Harmonizer class. The Harmonizer class is essentially a synthesizer that makes sound by pitch shifting an input audio signal. The Harmonizer class owns and manages a collection of harmonizerVoice objects to play sound; a single HarmonizerVoice plays one note at a time.
 
======================================================================================================================================================*/


#pragma once


namespace bav
{
/***********************************************************************************************************************************************
***********************************************************************************************************************************************/

/*
    Harmonizer: base class for the polyphonic instrument owning & managing a collection of HarmonizerVoices
*/

template < typename SampleType >
class Harmonizer : public dsp::SynthBase< SampleType >
{
    using AudioBuffer    = juce::AudioBuffer< SampleType >;
    using MidiBuffer     = juce::MidiBuffer;
    using Voice          = HarmonizerVoice< SampleType >;
    using Base           = dsp::SynthBase< SampleType >;

public:
    Harmonizer();

    void render (const AudioBuffer& input, AudioBuffer& output, juce::MidiBuffer& midiMessages);

    void release() override;

    int getLatencySamples() const noexcept { return analyzer.getLatency(); }

    
private:
    friend class HarmonizerVoice< SampleType >;

    void initialized (const double initSamplerate, const int initBlocksize) override;

    void prepared (int blocksize) override;

    void resetTriggered() override;

    void samplerateChanged (double newSamplerate) override;

    void addNumVoices (const int voicesToAdd) override;

    dsp::PsolaAnalyzer< SampleType > analyzer;

    static constexpr auto adsrQuickReleaseMs               = 5;
    static constexpr auto playingButReleasedGainMultiplier = 0.4f;
    static constexpr auto softPedalGainMultiplier          = 0.65f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Harmonizer)
};


} // namespace bav