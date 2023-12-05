/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

//==============================================================================
/**
*/
class AudioStatisticsPluginAudioProcessorEditor  : public juce::AudioProcessorEditor
{
public:
    AudioStatisticsPluginAudioProcessorEditor (AudioStatisticsPluginAudioProcessor&, juce::AudioProcessorValueTreeState&);
    ~AudioStatisticsPluginAudioProcessorEditor() override;

    //==============================================================================
    void paint (juce::Graphics&) override;
    void resized() override;

    void updateValues();

    std::string parameterToString(std::string param_name);

private:
    // This reference is provided as a quick way for your editor to
    // access the processor object that created it.
    AudioStatisticsPluginAudioProcessor& audioProcessor;
    juce::AudioProcessorValueTreeState& valueTreeState;

    juce::Label ZeroPassesTextBox;
    juce::Label RmsTextBox;
    juce::Label MinTextBox;
    juce::Label MaxTextBox;

    juce::TextButton resetButton;
    juce::TextButton updateButton;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioStatisticsPluginAudioProcessorEditor)
};
