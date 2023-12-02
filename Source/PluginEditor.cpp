/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
AudioStatisticsPluginAudioProcessorEditor::AudioStatisticsPluginAudioProcessorEditor (AudioStatisticsPluginAudioProcessor& p, juce::AudioProcessorValueTreeState& vts)
    : AudioProcessorEditor (&p), audioProcessor (p), valueTreeState(vts)
{
    updateValues();
    addAndMakeVisible(&ZeroPassesTextBox);
    addAndMakeVisible(&TODO1TextBox);
    addAndMakeVisible(&TODO2TextBox);
    addAndMakeVisible(&TODO3TextBox);



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
}

AudioStatisticsPluginAudioProcessorEditor::~AudioStatisticsPluginAudioProcessorEditor()
{
}

//==============================================================================
void AudioStatisticsPluginAudioProcessorEditor::paint (juce::Graphics& g)
{
    // (Our component is opaque, so we must completely fill the background with a solid colour)
    ZeroPassesTextBox.setBounds(10, 10, 500, 20);
    TODO1TextBox.setBounds(10, 40, 500, 20);
    TODO2TextBox.setBounds(10, 70, 500, 20);
    TODO3TextBox.setBounds(10, 100, 500, 20);

    resetButton.setBounds(getWidth()/2+10, getHeight() - 60, getWidth() / 2 -20, 50);
    updateButton.setBounds(10, getHeight() - 60, getWidth() / 2 - 20, 50);

    updateValues();
}

void AudioStatisticsPluginAudioProcessorEditor::resized()
{
    // This is generally where you'll want to lay out the positions of any
    // subcomponents in your editor..
}

void AudioStatisticsPluginAudioProcessorEditor::updateValues()
{
    ZeroPassesTextBox.setText("Zero Passes: " + parameterToString("zero_passes"), juce::NotificationType::dontSendNotification);
    TODO1TextBox.setText("TODO 1: ", juce::NotificationType::dontSendNotification);
    TODO2TextBox.setText("TODO 2: ", juce::NotificationType::dontSendNotification);
    TODO3TextBox.setText("TODO 3: ", juce::NotificationType::dontSendNotification);
}

std::string AudioStatisticsPluginAudioProcessorEditor::parameterToString(std::string param_name)
{
    std::string t = std::to_string(valueTreeState.getRawParameterValue(param_name)->load());
    return t.erase(t.find_last_not_of('0') + 1, std::string::npos).erase(t.find_last_not_of('.') + 1, std::string::npos);
}
