/*
  ==============================================================================

    LufsChannel.cpp
    Created: 7 Jan 2024 12:51:47pm
    Author:  kubam

  ==============================================================================
*/

#include "LufsChannel.h"


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
    processed_bin_counter_for_momentary = bins_in_400ms - 1;
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
    return  ((bin_rms_container.size() - 1 > processed_bin_counter_for_momentary) && bin_rms_container.size() > 0) || ((bin_rms_container.size() > processed_bin_counter_for_momentary) && (current_position_in_filling_bin == 0));
}

bool LufsChannel::isEnoughForShortTerm()
{
    return  ((bin_rms_container.size() - 1 > processed_bin_counter_for_short_term) && bin_rms_container.size() > 0) || ((bin_rms_container.size() > processed_bin_counter_for_short_term) && (current_position_in_filling_bin == 0));
}

const double& LufsChannel::calculateMomentaryRmsForChannel()
{
    momentary_rms = 0.0;
    // momentary_rms = (sum of squares of samples in the last 400ms)/(no. of samples in the last 400ms)
    // Also called momentary power of segment

    position_from_back = bin_rms_container.size() - processed_bin_counter_for_momentary - ((current_position_in_filling_bin == 0) ? 1 : 2);
    // position_from_back - variable created in order to take first 4 unprocessed bins.

    // TODO - test against HUGE buffer sizes
    if (current_position_in_filling_bin == 0) { // If last bin is full, take last 4 bins
        momentary_rms = std::accumulate(bin_rms_container.end() - bins_in_400ms - position_from_back, bin_rms_container.end() - position_from_back, 0.0) / bins_in_400ms;
    }
    else { // If last bin is not full
        momentary_rms = std::accumulate(bin_rms_container.end() - position_from_back - bins_in_400ms - 1, bin_rms_container.end() - position_from_back - 1, 0.0) / bins_in_400ms;
    }

    // now we have momentary_rms of the latest 400ms segment

    processed_bin_counter_for_momentary++; // New bin is being processed - increase the amount of bins processed

    return momentary_rms;
}

const double& LufsChannel::calculateShortTermRmsForChannel()
{
    short_term_rms = 0.0;

    position_from_back = bin_rms_container.size() - processed_bin_counter_for_short_term - ((current_position_in_filling_bin == 0) ? 1 : 2);

    // TODO - test against HUGE buffer sizes
    if (current_position_in_filling_bin == 0) {
        short_term_rms = std::accumulate(bin_rms_container.end() - bins_in_3s - position_from_back, bin_rms_container.end() - position_from_back, 0.0) / bins_in_3s;
    }
    else {
        short_term_rms = std::accumulate(bin_rms_container.end() - position_from_back - bins_in_3s - 1, bin_rms_container.end() - position_from_back - 1, 0.0) / bins_in_3s;
    }
    processed_bin_counter_for_short_term++;

    return short_term_rms;
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