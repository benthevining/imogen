
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
 
 PluginProcessorNetworking.cpp: This file contains functions dealing with networking and communications.
 
 ======================================================================================================================================================*/

#include "PluginProcessor.h"


/* Functions for recieving events from the editor */

void ImogenAudioProcessor::recieveParameterValueChange (ParameterID paramID, float newValue)
{
    getParameterPntr(paramID)->orig()->setValueNotifyingHost (newValue);
}

void ImogenAudioProcessor::recieveParameterChangeGestureBegin (ParameterID paramID)
{
    getParameterPntr(paramID)->orig()->beginChangeGesture();
}

void ImogenAudioProcessor::recieveParameterChangeGestureEnd (ParameterID paramID)
{
    getParameterPntr(paramID)->orig()->endChangeGesture();
}

void ImogenAudioProcessor::recieveMidiLatchEvent (bool isNowLatched)
{
    const auto value = isNowLatched ? 1.0f : 0.0f;
    nonParamEvents.pushMessage (midiLatchID, value);
}

void ImogenAudioProcessor::recieveKillAllMidiEvent()
{
    nonParamEvents.pushMessage (killAllMidiID, 1.0f);
}

void ImogenAudioProcessor::recieveEditorPitchbendEvent (int wheelValue)
{
    nonParamEvents.pushMessage (pitchBendFromEditorID,
                                pitchbendNormalizedRange.convertTo0to1 (float (wheelValue)));
}



/* Functions for sending events to the editor (& any connected ImogenRemotes) */

void ImogenAudioProcessor::sendParameterChange (ParameterID paramID, float newValue)
{
    currentProgram.store (-1);
    
    if (auto* editor = getActiveGui())
        editor->recieveParameterChange (paramID, newValue);
    
    // send param change as OSC message to any recievers...
}

void ImogenAudioProcessor::sendParameterChangeGestureBegin (ParameterID paramID)
{
    if (auto* editor = getActiveGui())
        editor->recieveParameterChangeGestureStart (paramID);
    
    // send OSC message...
}

void ImogenAudioProcessor::sendParameterChangeGestureEnd (ParameterID paramID)
{
    if (auto* editor = getActiveGui())
        editor->recieveParameterChangeGestureEnd (paramID);
    
    // send OSC message...
}



/*
 OUTGOING OSC MESSAGES
 virtual void sendParameterChange (ParameterID paramID, float newValue) = 0;
 virtual void startParameterChangeGesture (ParameterID paramID) = 0;
 virtual void endParameterChangeGesture (ParameterID paramID) = 0;
 
 virtual void sendEditorPitchbend (int wheelValue) = 0;
 
 virtual void sendMidiLatch (bool shouldBeLatched) = 0;
 
 virtual void loadPreset   (const juce::String& presetName) = 0;
 virtual void savePreset   (const juce::String& presetName) = 0;
 virtual void deletePreset (const juce::String& presetName) = 0;
 
 virtual void enableAbletonLink (bool shouldBeEnabled) = 0;
 
 
 INCOMING OSC MESSAGES
 parameterChangeRecieved (ParameterID paramID, float newValue);
 void parameterChangeGestureStarted (ParameterID paramID);
 void parameterChangeGestureEnded (ParameterID paramID);
 
 void presetNameChanged (const juce::String& newPresetName);
 
 void mts_connectionChange (bool isNowConnected);
 void mts_scaleChange (const juce::String& newScaleName);
 
 void abletonLinkChange (bool isNowEnabled)
*/
