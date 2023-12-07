/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

//==============================================================================
/**
*/
class AudioStatisticsPluginAudioProcessor  : public juce::AudioProcessor
                            #if JucePlugin_Enable_ARA
                             , public juce::AudioProcessorARAExtension
                            #endif
{
public:
    //==============================================================================
    AudioStatisticsPluginAudioProcessor();
    ~AudioStatisticsPluginAudioProcessor() override;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

   #ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
   #endif

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    //==============================================================================
    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    //==============================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    //==============================================================================
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    void clearCounters();

    juce::IIRFilter filter1;
    juce::IIRFilter filter2;

private:
    double sampleRate = 0.0;
    unsigned int lufs_counter = 0;

    float momentary_power_sum = 0.0;
    unsigned int momentary_power_count = 0;

    std::vector<std::vector<float>> lufs_container;

    std::vector<float> momentary_power;

    std::atomic<float>* last_momentary_loudness = nullptr;
    std::atomic<float>* integrated_loudness = nullptr;

    std::atomic<float>* zero_passes = nullptr;
    std::atomic<float>* rms = nullptr;
    std::atomic<float>* min = nullptr;
    std::atomic<float>* max = nullptr;

    std::unique_ptr<float[]> previous_samples = nullptr;
    std::unique_ptr<unsigned int> previous_length = nullptr;

    std::unique_ptr<long long unsigned int[]> samples_count_per_channel = nullptr;
    std::unique_ptr<float[]> square_sum_per_channel = nullptr;

    juce::AudioProcessorValueTreeState valueTreeState;

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioStatisticsPluginAudioProcessor)
};
