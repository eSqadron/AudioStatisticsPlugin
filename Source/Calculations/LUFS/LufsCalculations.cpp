/*
  ==============================================================================

    LufsCalc.cpp
    Created: 6 Jan 2024 3:27:40pm
    Author:  kubam

  ==============================================================================
*/

// sources:
//https://www.mathworks.com/help/audio/ref/integratedloudness.html
//https://www.itu.int/dms_pubrec/itu-r/rec/bs/R-REC-BS.1770-5-202311-I!!PDF-E.pdf
//https://github.com/klangfreund/LUFSMeter/blob/master/Ebu128LoudnessMeter.cpp

#include "LufsCalculations.h"

LufsCalculations::LufsCalculations() :
    filter1(),
    filter2(),
    channels(),
    // temp vars:
    samplesNum(0),
    momentaryLoudnessWeighted(0.0),
    relativeThresholdWeighted(0.0),
    integratedLoudnessWeighted(0.0)
{
    int modifier = 1.0;
    int a0 = 1.0;
    filter1.setCoefficients(juce::IIRCoefficients(1.53512485958697, -2.69169618940638, 1.19839281085285, a0, -1.69065929318241 * modifier, 0.73248077421585 * modifier));
    filter2.setCoefficients(juce::IIRCoefficients(1.0, -2.0, 1.0, a0, -1.99004745483398 * modifier, 0.99007225036621 * modifier));

    channels.push_back(LufsChannel(0, filter1, filter2));
    channels.push_back(LufsChannel(1, filter1, filter2));
}

void LufsCalculations::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    //this->bin_length_in_samples = sampleRate / 10.0; // calcluate 100ms bin length


    for (ChannelIt = channels.begin(); ChannelIt != channels.end(); ++ChannelIt) {
        ChannelIt->prepareToPlay(sampleRate, samplesPerBlock);
    }
}

void LufsCalculations::clearCounters()
{
    last_momentary_loudness->store(-std::numeric_limits<double>::infinity());
    integrated_loudness->store(-std::numeric_limits<double>::infinity());
    short_term_loudness->store(-std::numeric_limits<double>::infinity());

    for (ChannelIt = channels.begin(); ChannelIt != channels.end(); ++ChannelIt) {
        ChannelIt->clearCounters();
    }
}

void LufsCalculations::processBlock(juce::AudioBuffer<float>& buffer, int channelCount)
{
    samplesNum = buffer.getNumSamples();
    for (ChannelIt = channels.begin(); ChannelIt != channels.end(); ++ChannelIt) {
        if (ChannelIt->channelNo >= channelCount) {
            break;
        }

        float* write_pointer = buffer.getWritePointer(ChannelIt->channelNo);
        ChannelIt->fillBins(write_pointer, samplesNum);
    }


    // if there is enough NEW bins (at least 400ms of data and at leas 100ms of NEW data) for momentary lufs calculation in EACH channel
    while (isEnoughForMomentaryInEachChannel()) {

        // calculate momentary loudness weighted for channels:
        this->calculateMomentaryLoudnessWeighted();

        // if momentary loudness passes gate 1:
        if (momentaryLoudnessWeighted > -70.0) {

            // store and display it:
            last_momentary_loudness->store(momentaryLoudnessWeighted);

            // calculate relative threshold for gate 2:
            this->calculateRelativeThresholdWeighted();

            // check if all channels pass the gate:
            if (this->relativeThresholdGateForEachChannel()) {

                // calculate integrated loduness and store it, display it:
                this->calculateIntegratedLoudnessWeighted();
                integrated_loudness->store(integratedLoudnessWeighted);
            }
        }

    }

    while (isEnoughForShortTermInEachChannel()) {
        this->calculateShortTermLoudnessWeighted();
        short_term_loudness->store(integratedLoudnessWeighted);
    }
}

bool LufsCalculations::isEnoughForMomentaryInEachChannel()
{
    bool temp_condition = true;
    for (ChannelIt = channels.begin(); ChannelIt != channels.end(); ++ChannelIt) {
        bool tt_cond = ChannelIt->isEnoughForMomentary();
        temp_condition = temp_condition && tt_cond;
    }
    return temp_condition;
}

void LufsCalculations::calculateMomentaryLoudnessWeighted()
{
    momentaryLoudnessWeighted = 0.0;
    for (ChannelIt = channels.begin(); ChannelIt != channels.end(); ++ChannelIt) {

        // calculate momentary rms weighted for channels
        // momentary rms = sum of squares of samples in last 400ms /no. of samples in 400ms
        momentaryLoudnessWeighted += ChannelIt->calculateMomentaryRmsForChannel() * ChannelIt->Weight;
    }

    // calculate momentary loudness based on momentary RMS
    momentaryLoudnessWeighted = -0.691 + 10 * std::log10(momentaryLoudnessWeighted);
}

void LufsCalculations::calculateRelativeThresholdWeighted()
{
    relativeThresholdWeighted = 0.0;
    for (ChannelIt = channels.begin(); ChannelIt != channels.end(); ++ChannelIt) {

        // rmses for second gate: momentary rmses from the beginning of measurement are avergaed and weighted for each channel
        relativeThresholdWeighted += ChannelIt->calculateRmsForRelativeThreshold() * ChannelIt->Weight;
    }

    // relative threshold is calculated based on average of momentary RMSes
    relativeThresholdWeighted = -10.691 + 10.0 * std::log10(relativeThresholdWeighted);
}

bool LufsCalculations::relativeThresholdGateForEachChannel()
{
    // each channel needs to pass second (relative threshold) gate.
    // their last momentary RMSes must be higher than calculated relative threshold
    bool temp_threshold_passed = true;
    for (ChannelIt = channels.begin(); ChannelIt != channels.end(); ++ChannelIt) {
        temp_threshold_passed = temp_threshold_passed && ChannelIt->relativeThresholdGate(relativeThresholdWeighted);
    }
    return temp_threshold_passed;
}

void LufsCalculations::calculateIntegratedLoudnessWeighted()
{
    integratedLoudnessWeighted = 0.0;
    for (ChannelIt = channels.begin(); ChannelIt != channels.end(); ++ChannelIt) {

        //accumulate all momentary rmses that passed both gates and average them. 
        integratedLoudnessWeighted += ChannelIt->calculateAverageOfMomentaryPowerSegments() * ChannelIt->Weight;
    }

    //based on that average we can calulate channel weighted integrated loudness
    integratedLoudnessWeighted = -0.691 + 10.0 * std::log10(integratedLoudnessWeighted);
}

bool LufsCalculations::isEnoughForShortTermInEachChannel()
{
    bool temp_condition = true;
    for (ChannelIt = channels.begin(); ChannelIt != channels.end(); ++ChannelIt) {
        bool tt_cond = ChannelIt->isEnoughForShortTerm();
        temp_condition = temp_condition && tt_cond;
    }
    return temp_condition;
}

void LufsCalculations::calculateShortTermLoudnessWeighted()
{
    shortTermWeighted = 0.0;
    for (ChannelIt = channels.begin(); ChannelIt != channels.end(); ++ChannelIt) {

        shortTermWeighted += ChannelIt->calculateShortTermRmsForChannel() * ChannelIt->Weight;
    }

    shortTermWeighted = -0.691 + 10 * std::log10(shortTermWeighted);
}

