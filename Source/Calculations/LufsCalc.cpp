/*
  ==============================================================================

    LufsCalc.cpp
    Created: 6 Jan 2024 3:27:40pm
    Author:  kubam

  ==============================================================================
*/

#include "LufsCalc.h"

LufsChannel::LufsChannel(juce::IIRFilter& filter1, juce::IIRFilter& filter2, float weight = 1.0): 
    Weight(weight), 
    filter1(filter1), 
    filter2(filter2),
    bin_rms_container(),
    bin_length_in_samples(0)
{
}

void LufsChannel::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    this->bin_length_in_samples = sampleRate / 10; // calcluate 100ms bin length
}

void LufsChannel::fillBins(float* write_pointer, int samplesNum)
{
    float* channelData_copy = new float[samplesNum];
    memcpy(channelData_copy, write_pointer, sizeof(float) * samplesNum);

    // Filter samples
    filter1.processSamples(channelData_copy, samplesNum);
    filter2.processSamples(channelData_copy, samplesNum);

    // Fill the bins with averages
    for (float* i = write_pointer; i < write_pointer + samplesNum; i++) {
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


LufsCalculations::LufsCalculations() :
    filter1(),
    filter2(),
    bin_rms_container(),
    segment_square_sums(),
    bin_length_in_samples(0),
    channels()
{
    filter1.setCoefficients(juce::IIRCoefficients(1.53512485958697, -2.69169618940638, 1.19839281085285, 0.0, -1.69065929318241, 0.73248077421585));
    filter2.setCoefficients(juce::IIRCoefficients(1.0, -2.0, 1.0, 0.0, -1.99004745483398, 0.99007225036621));

    channels.push_back(LufsChannel(filter1, filter2));
    channels.push_back(LufsChannel(filter1, filter2));
}

void LufsCalculations::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    this->bin_length_in_samples = sampleRate / 10; // calcluate 100ms bin length


    for each (LufsChannel channel in channels) {
        channel.prepareToPlay(sampleRate, samplesPerBlock);
    }
}

void LufsCalculations::clearCounters()
{
    last_momentary_loudness->store(-std::numeric_limits<double>::infinity());
    integrated_loudness->store(-std::numeric_limits<double>::infinity());
    short_term_loudness->store(-std::numeric_limits<double>::infinity());

    processed_bin_counter = bins_in_400ms - 1;
    bin_rms_container.clear();
    segment_square_sums.clear();
    current_position_in_filling_bin = 0;

    relative_threshold_acumulator = 0.0;
    relative_threshold_segments_count = 0;
}

void LufsCalculations::processBlock(juce::AudioBuffer<float>& buffer, int channelCount)
{
    int samplesNum = buffer.getNumSamples();


    for (int channel = 0; channel < channelCount; ++channel) {
        //https://www.mathworks.com/help/audio/ref/integratedloudness.html
        //https://www.itu.int/dms_pubrec/itu-r/rec/bs/R-REC-BS.1770-5-202311-I!!PDF-E.pdf
        //https://github.com/klangfreund/LUFSMeter/blob/master/Ebu128LoudnessMeter.cpp
        if (channel == 0) {
            float* write_pointer = buffer.getWritePointer(channel);

            // Copy buffer so the original data won't get modified
            //float* channelData_copy = new float[samplesNum];
            //memcpy(channelData_copy, write_pointer, sizeof(float) * samplesNum);

            //// Filter samples
            //filter1.processSamples(channelData_copy, samplesNum);
            //filter2.processSamples(channelData_copy, samplesNum);

            //// Fill the bins with averages
            //for (float* i = write_pointer; i < write_pointer + samplesNum; i++) {
            //    if (current_position_in_filling_bin == 0) {
            //        bin_rms_container.push_back(0);
            //    }

            //    bin_rms_container.back() += (*i * *i);
            //    current_position_in_filling_bin++;

            //    if (current_position_in_filling_bin >= bin_length_in_samples) {
            //        current_position_in_filling_bin = 0;
            //        bin_rms_container.back() = bin_rms_container.back() / bin_length_in_samples;
            //    }
            //}

            // Proceed to any LUFS calculation ONLY if new bin was added (and there is AT LEAST default 4 bins stored)
            // Call multiple time if there are multiple new bins to be processed
            // processed_bin_counter starts at 3, so adding FULL 4th bin will cause this while to be called for the first time.
            // There are two conditions separated with OR. First requiers 5 bins (last one may be half-filled) and the second one requires 4 fully filled bins.
            // Bin is fully filled when it accumulated 100ms worth of samples into it, and divided it by bin size.
            while ((bin_rms_container.size() - 1 > processed_bin_counter) || ((bin_rms_container.size() > processed_bin_counter) && (current_position_in_filling_bin == 0))) {
                float momentary_rms = 0.0;
                // momentary_rms = (sum of squares of samples in the last 400ms)/(no. of samples in the last 400ms)
                // Also called momentary power of segment

                unsigned short int position_from_back = bin_rms_container.size() - processed_bin_counter - ((current_position_in_filling_bin == 0) ? 1 : 2);
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

                float momentary_loudness = -0.691 + 10 * std::log10(momentary_rms); // weighted for channels
                if (momentary_loudness > -70.0) {
                    // First gate passed - display momentary loudness of current segment
                    last_momentary_loudness->store(momentary_loudness);

                    relative_threshold_acumulator += momentary_rms;
                    relative_threshold_segments_count++;

                    // rms over the whole measurement and relative treshold calculation
                    float rms_from_the_begginig = relative_threshold_acumulator / relative_threshold_segments_count;

                    float relative_treshold = -10.691 + 10.0 * std::log10(rms_from_the_begginig); // weighted for channels

                    // TODO - Is this gate correct?
                    if ((momentary_rms >= relative_treshold) || (segment_square_sums.size() == 0)) {
                        // Second gate passed
                        segment_square_sums.push_back(momentary_rms);
                        integrated_loudness->store(-0.691 + 10.0 * std::log10(std::accumulate(segment_square_sums.begin(), segment_square_sums.end(), 0.0) / segment_square_sums.size())); // weighted for channels
                    }
                }

            }


        }
    }


}

