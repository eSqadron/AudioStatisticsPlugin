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

#include "LufsCalc.h"

LufsChannel::LufsChannel(unsigned int channel_no, juce::IIRFilter filter1, juce::IIRFilter filter2, float weight) :
    channelNo(channel_no),
    Weight(weight), 
    filter1(filter1), 
    filter2(filter2),
    bin_rms_container(),
    bin_length_in_samples(0),
    segment_square_sums(),
    // TEMP variables:
    momentary_rms(0.0),
    position_from_back(0),
    momentary_loudness(0.0),
    rms_from_the_begginig(0.0),
    relative_treshold(0.0),
    integrated_loudness(0.0),
    averageOfMomentaryPowerSegments(0.0),
    use_filters(false)
{
}

void LufsChannel::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    unsigned int bin_len_temp = sampleRate / 10.0;
    double bin_len_temp2 = sampleRate / 10.0;
    unsigned int bin_len_temp3 = static_cast<unsigned int>(sampleRate / 10.0);
    this->bin_length_in_samples = bin_len_temp; // calcluate 100ms bin length
    bin_len_temp;

}

void LufsChannel::clearCounters()
{
    processed_bin_counter = bins_in_400ms - 1;
    bin_rms_container.clear();
    segment_square_sums.clear();
    current_position_in_filling_bin = 0;

    relative_threshold_acumulator = 0.0;
    relative_threshold_segments_count = 0;
}

void LufsChannel::fillBins(float* write_pointer, int samplesNum)
{
    float* channelData_copy = new float[samplesNum];
    memcpy(channelData_copy, write_pointer, sizeof(float) * samplesNum);

    if (use_filters) {
        // Filter samples
        filter1.processSamples(channelData_copy, samplesNum);
        filter2.processSamples(channelData_copy, samplesNum);
    }

    // Fill the bins with averages
    for (float* i = channelData_copy; i < channelData_copy + samplesNum; i++) {
        if (current_position_in_filling_bin == 0) {
            bin_rms_container.push_back(0);
        }

        bin_rms_container.back() += (*i * *i);
        current_position_in_filling_bin++;

        if (current_position_in_filling_bin >= bin_length_in_samples) {
            current_position_in_filling_bin = 0;
            bin_rms_container.back() = bin_rms_container.back() / bin_length_in_samples;
        }
    }
}

bool LufsChannel::isEnoughForMomentary()
{
    // Proceed to any LUFS calculation ONLY if new bin was added (and there is AT LEAST default 4 bins stored)
    // Call multiple time if there are multiple new bins to be processed
    // processed_bin_counter starts at 3, so adding FULL 4th bin will cause this while to be called for the first time.
    // There are two conditions separated with OR. First requiers 5 bins (last one may be half-filled) and the second one requires 4 fully filled bins.
    // Bin is fully filled when it accumulated 100ms worth of samples into it, and divided it by bin size.
    return  ((bin_rms_container.size() - 1 > processed_bin_counter) && bin_rms_container.size() > 0) || ((bin_rms_container.size() > processed_bin_counter) && (current_position_in_filling_bin == 0));
}

const double& LufsChannel::calculateMomentaryRmsForChannel()
{
    momentary_rms = 0.0;
    // momentary_rms = (sum of squares of samples in the last 400ms)/(no. of samples in the last 400ms)
    // Also called momentary power of segment

    position_from_back = bin_rms_container.size() - processed_bin_counter - ((current_position_in_filling_bin == 0) ? 1 : 2);
    // position_from_back - variable created in order to take first 4 unprocessed bins.

    // TODO - test against HUGE buffer sizes
    if (current_position_in_filling_bin == 0) { // If last bin is full, take last 4 bins
        momentary_rms = std::accumulate(bin_rms_container.end() - bins_in_400ms - position_from_back, bin_rms_container.end() - position_from_back, 0.0) / bins_in_400ms;
    }
    else { // If last bin is not full
        momentary_rms = std::accumulate(bin_rms_container.end() - position_from_back - bins_in_400ms - 1, bin_rms_container.end() - position_from_back - 1, 0.0) / bins_in_400ms;
    }

    // now we have momentary_rms of the latest 400ms segment

    processed_bin_counter++; // New bin is being processed - increase the amount of bins processed

    return momentary_rms;
}

const double& LufsChannel::calculateRmsForRelativeThreshold()
{
    // accumulate momentary rmses for relative threshold calculation
    relative_threshold_acumulator += momentary_rms;
    relative_threshold_segments_count++;

    // rms over the whole measurement and relative treshold calculation
    rms_from_the_begginig = relative_threshold_acumulator / relative_threshold_segments_count;

    return rms_from_the_begginig;
}

bool LufsChannel::relativeThresholdGate(double calculatedRelativeThreshold)
{
    return (momentary_rms >= calculatedRelativeThreshold) || (segment_square_sums.size() == 0);
}

const double& LufsChannel::calculateAverageOfMomentaryPowerSegments()
{
    segment_square_sums.push_back(momentary_rms);
    averageOfMomentaryPowerSegments = std::accumulate(segment_square_sums.begin(), segment_square_sums.end(), 0.0) / segment_square_sums.size();
    return averageOfMomentaryPowerSegments;
}

////////////////////////////////
/////////////////////////////////

LufsCalculations::LufsCalculations() :
    filter1(),
    filter2(),
    channels()
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


    for (auto it = channels.begin(); it != channels.end(); ++it) {
        it->prepareToPlay(sampleRate, samplesPerBlock);
    }
}

void LufsCalculations::clearCounters()
{
    last_momentary_loudness->store(-std::numeric_limits<double>::infinity());
    integrated_loudness->store(-std::numeric_limits<double>::infinity());
    short_term_loudness->store(-std::numeric_limits<double>::infinity());

    for (auto it = channels.begin(); it != channels.end(); ++it) {
        it->clearCounters();
    }
}

void LufsCalculations::processBlock(juce::AudioBuffer<float>& buffer, int channelCount)
{
    int samplesNum = buffer.getNumSamples();
    for (auto it = channels.begin(); it != channels.end(); ++it) {
        if (it->channelNo >= channelCount) {
            break;
        }

        float* write_pointer = buffer.getWritePointer(it->channelNo);
        it->fillBins(write_pointer, samplesNum);
    }


    // if there is enough bins (at least 400ms of data) for momentary lufs calculation in EACH channel
    while (isEnoughForMomentaryInEachChannel()) {
        double momentaryLoudnessWeighted = 0.0;
        for (auto it = channels.begin(); it != channels.end(); ++it) {
            momentaryLoudnessWeighted += it->calculateMomentaryRmsForChannel() * it->Weight;
        }

        momentaryLoudnessWeighted = -0.691 + 10 * std::log10(momentaryLoudnessWeighted);

        if (momentaryLoudnessWeighted > -70.0) {
            last_momentary_loudness->store(momentaryLoudnessWeighted);

            double relativeThresholdWeighted = 0.0;
            for (auto it = channels.begin(); it != channels.end(); ++it) {
                relativeThresholdWeighted += it->calculateRmsForRelativeThreshold() * it->Weight;
            }
            relativeThresholdWeighted = -10.691 + 10.0 * std::log10(relativeThresholdWeighted);

            if (relativeThresholdGateForEachChannel(relativeThresholdWeighted)) {
                double integratedLoudnessWeighted = 0.0;
                for (auto it = channels.begin(); it != channels.end(); ++it) {
                    integratedLoudnessWeighted += it->calculateAverageOfMomentaryPowerSegments() * it->Weight;
                }
                integratedLoudnessWeighted = -0.691 + 10.0 * std::log10(integratedLoudnessWeighted);
                integrated_loudness->store(integratedLoudnessWeighted);
            }
        }

    }
}

bool LufsCalculations::isEnoughForMomentaryInEachChannel()
{
    bool temp_condition = true;
    for (auto it = channels.begin(); it != channels.end(); ++it) {
        bool tt_cond = it->isEnoughForMomentary();
        temp_condition = temp_condition && tt_cond;
    }
    return temp_condition;
}

bool LufsCalculations::relativeThresholdGateForEachChannel(double relativeThresholdWeighted)
{
    bool temp_threshold_passed = true;
    for (auto it = channels.begin(); it != channels.end(); ++it) {
        temp_threshold_passed = temp_threshold_passed && it->relativeThresholdGate(relativeThresholdWeighted);
    }
    return temp_threshold_passed;
    return false;
}

