/*
  ==============================================================================

    LufsCalc.h
    Created: 6 Jan 2024 3:27:40pm
    Author:  kubam

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>


class LufsChannel {
public:
    LufsChannel(unsigned int channel_no, juce::IIRFilter filter1, juce::IIRFilter filter2, float weight = 1.0);

    void prepareToPlay(double sampleRate, int samplesPerBlock);

    void clearCounters();

    void fillBins(float* write_pointer, int samplesNum);

    bool isEnoughForMomentary();

    const double& calculateMomentaryRmsForChannel();
    const double& calculateRmsForRelativeThreshold();

    bool relativeThresholdGate(double calculatedRelativeThreshold);

    const double& calculateAverageOfMomentaryPowerSegments();


    float Weight;
    unsigned int channelNo;
    juce::IIRFilter filter1;
    juce::IIRFilter filter2;
private:

    // bin: 100ms - length container
    // segment: 400ms - length container, that passed two gates-checks
    unsigned int current_position_in_filling_bin = 0; // Position in bin for filling it.
    std::vector<float> bin_rms_container; // Sum of squares of samples in each bin
    unsigned int bin_length_in_samples; // length of single bin

    unsigned short int bins_in_400ms = 4; // Number of bins that form Momentary Loudness
    unsigned short int bins_in_3s = 30; // Number of bins that form Short Term Loudness
    unsigned long long int processed_bin_counter = bins_in_400ms - 1; // counter of already processed bins. Processing starts at bin bins_in_400ms (value 4), so default is bins_in_400ms-1 (value 3)

    std::vector<float> segment_square_sums; // Sum of squares of samples in each segment

    // Accumulators for calculating relative_thresholds
    float relative_threshold_acumulator = 0.0;
    unsigned long int relative_threshold_segments_count = 0;

    bool use_filters;

    // TEMP variables:
    double momentary_rms;
    unsigned short int position_from_back;
    double momentary_loudness;
    double rms_from_the_begginig;
    double relative_treshold;
    double integrated_loudness;
    double averageOfMomentaryPowerSegments;

};

class LufsCalculations {
public:
    LufsCalculations();

    void prepareToPlay(double sampleRate, int samplesPerBlock);

    void clearCounters();

    void processBlock(juce::AudioBuffer<float>& buffer, int channelCount);

    std::atomic<float>* last_momentary_loudness = nullptr;
    std::atomic<float>* integrated_loudness = nullptr;
    std::atomic<float>* short_term_loudness = nullptr;

private:
    bool isEnoughForMomentaryInEachChannel();
    bool relativeThresholdGateForEachChannel(double relativeThresholdWeighted);

    juce::IIRFilter filter1;
    juce::IIRFilter filter2;

    // bin: 100ms - length container
    // segment: 400ms - length container, that passed two gates-checks
    //unsigned int bin_length_in_samples; // length of single bin
    //std::vector<float> bin_rms_container; // Sum of squares of samples in each bin

    //std::vector<float> segment_square_sums; // Sum of squares of samples in each segment

    //unsigned int current_position_in_filling_bin = 0; // Position in bin for filling it.


    //unsigned short int bins_in_400ms = 4; // Number of bins that form Momentary Loudness
    //unsigned short int bins_in_3s = 30; // Number of bins that form Short Term Loudness
    //unsigned long long int processed_bin_counter = bins_in_400ms - 1; // counter of already processed bins. Processing starts at bin bins_in_400ms (value 4), so default is bins_in_400ms-1 (value 3)


    //// Accumulators for calculating relative_thresholds
    //float relative_threshold_acumulator = 0.0;
    //unsigned long int relative_threshold_segments_count = 0;


    std::list<LufsChannel> channels;
};