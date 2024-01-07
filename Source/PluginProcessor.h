/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "Calculations/LUFS/LufsCalculations.h"

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

private:
    // bin: 100ms - length container
    // segment: 400ms - length container, that passed two gates-checks
    //unsigned int bin_length_in_samples; // length of single bin
    //std::vector<float> bin_rms_container; // Sum of squares of samples in each bin

    //std::vector<float> segment_square_sums; // Sum of squares of samples in each segment

    ////std::vector<float> bin_sums; // Sum of samples in each bin
    //unsigned int current_position_in_filling_bin = 0; // Position in bin for filling it.
    //

    //unsigned short int bins_in_400ms = 4; // Number of bins that form Momentary Loudness
    //unsigned short int bins_in_3s = 30; // Number of bins that form Short Term Loudness
    //unsigned long long int processed_bin_counter = bins_in_400ms-1; // counter of already processed bins. Processing starts at bin bins_in_400ms (value 4), so default is bins_in_400ms-1 (value 3)
    
    LufsCalculations lufsCalc;

    // Accumulators for calculating relative_thresholds
    //float relative_threshold_acumulator = 0.0;
    //unsigned long int relative_threshold_segments_count = 0;

    //std::atomic<float>* last_momentary_loudness = nullptr;
    //std::atomic<float>* integrated_loudness = nullptr;
    //std::atomic<float>* short_term_loudness = nullptr;

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
