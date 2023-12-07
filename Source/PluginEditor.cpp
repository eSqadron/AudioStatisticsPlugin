/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"


//==============================================================================
AudioStatisticsPluginAudioProcessorEditor::AudioStatisticsPluginAudioProcessorEditor (AudioStatisticsPluginAudioProcessor& p, juce::AudioProcessorValueTreeState& vts)
    : AudioProcessorEditor (&p), audioProcessor (p), valueTreeState(vts)//, RefreshTimer(this->updateValues)
{
    updateValues();
    addAndMakeVisible(&ZeroPassesTextBox);
    addAndMakeVisible(&RmsTextBox);
    addAndMakeVisible(&MinTextBox);
    addAndMakeVisible(&MaxTextBox);
    addAndMakeVisible(&MomentaryLoudnessBox);
    addAndMakeVisible(&IntegratedLoudnessBox);

    resetButton.setButtonText("Reset Statistics");
    resetButton.onClick = [this]() {
        this->audioProcessor.clearCounters();
    };
    addAndMakeVisible(&resetButton);

    updateButton.setButtonText("Update Statistics");
    updateButton.onClick = [this]() {
        this->updateValues();
        };
    addAndMakeVisible(&updateButton);

    setSize (520, 300);

    this->startTimer(100);
}

AudioStatisticsPluginAudioProcessorEditor::~AudioStatisticsPluginAudioProcessorEditor()
{
}

//==============================================================================
void AudioStatisticsPluginAudioProcessorEditor::paint (juce::Graphics& g)
{
    // (Our component is opaque, so we must completely fill the background with a solid colour)
    ZeroPassesTextBox.setBounds(10, 10, 500, 20);
    RmsTextBox.setBounds(10, 40, 500, 20);
    MinTextBox.setBounds(10, 70, 500, 20);
    MaxTextBox.setBounds(10, 100, 500, 20);
    MomentaryLoudnessBox.setBounds(10, 130, 500, 20);
    IntegratedLoudnessBox.setBounds(10, 160, 500, 20);

    resetButton.setBounds(getWidth()/2+10, getHeight() - 60, getWidth() / 2 -20, 50);
    updateButton.setBounds(10, getHeight() - 60, getWidth() / 2 - 20, 50);
}

void AudioStatisticsPluginAudioProcessorEditor::resized()
{
    // This is generally where you'll want to lay out the positions of any
    // subcomponents in your editor..
}

void AudioStatisticsPluginAudioProcessorEditor::updateValues()
{
    ZeroPassesTextBox.setText("Zero Passes: " + parameterToString("zero_passes"), juce::NotificationType::dontSendNotification);
    RmsTextBox.setText("RMS: " + parameterToString("rms"), juce::NotificationType::dontSendNotification);
    MinTextBox.setText("MIN: " + parameterToString("min"), juce::NotificationType::dontSendNotification);
    MaxTextBox.setText("MAX: " + parameterToString("max"), juce::NotificationType::dontSendNotification);
    MomentaryLoudnessBox.setText("Momentary LUFS: " + parameterToString("momentary_loudness"), juce::NotificationType::dontSendNotification);
    IntegratedLoudnessBox.setText("Integrated LUFS: " + parameterToString("integrated_loudness"), juce::NotificationType::dontSendNotification);
}

std::string AudioStatisticsPluginAudioProcessorEditor::parameterToString(std::string param_name)
{
    std::string t = std::to_string(valueTreeState.getRawParameterValue(param_name)->load());
    return t.erase(t.find_last_not_of('0') + 1, std::string::npos).erase(t.find_last_not_of('.') + 1, std::string::npos);
}

void AudioStatisticsPluginAudioProcessorEditor::timerCallback()
{
    updateValues();
}
