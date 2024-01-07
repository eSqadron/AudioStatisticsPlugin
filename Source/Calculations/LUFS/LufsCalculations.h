/*
  ==============================================================================

    LufsCalc.h
    Created: 6 Jan 2024 3:27:40pm
    Author:  kubam

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "LufsChannel.h"

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
    bool relativeThresholdGateForEachChannel();
    void calculateMomentaryLoudnessWeighted();
    void calculateRelativeThresholdWeighted();
    void calculateIntegratedLoudnessWeighted();

    bool isEnoughForShortTermInEachChannel();
    void calculateShortTermLoudnessWeighted();

    juce::IIRFilter filter1;
    juce::IIRFilter filter2;


    std::list<LufsChannel> channels;

    // temp variables!
    std::list<LufsChannel>::iterator ChannelIt;
    int samplesNum;
    double momentaryLoudnessWeighted;
    double relativeThresholdWeighted;
    double integratedLoudnessWeighted;
    double shortTermWeighted;
};