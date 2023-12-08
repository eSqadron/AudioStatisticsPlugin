/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
AudioStatisticsPluginAudioProcessor::AudioStatisticsPluginAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
     : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      #endif
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                     #endif
                       ),
    valueTreeState(*this, nullptr, juce::Identifier("Statistics"),
                           {
                                std::make_unique<juce::AudioParameterInt>("zero_passes",            // parameterID
                                                                            "ZeroPasses",            // parameter name
                                                                            0,              // minimum value
                                                                            3.402823466e+38,              // maximum value
                                                                            0),
                                std::make_unique<juce::AudioParameterFloat>("rms",            // parameterID
                                                                            "RMS",            // parameter name
                                                                            -200,              // minimum value
                                                                            200,              // maximum value
                                                                            0),
                                std::make_unique<juce::AudioParameterFloat>("max",            // parameterID
                                                                            "Max",            // parameter name
                                                                            -200,              // minimum value
                                                                            200,              // maximum value
                                                                            -200),
                                std::make_unique<juce::AudioParameterFloat>("min",            // parameterID
                                                                            "Min",            // parameter name
                                                                            -200,              // minimum value
                                                                            200,              // maximum value
                                                                            200),
                                std::make_unique<juce::AudioParameterFloat>("momentary_loudness",            // parameterID
                                                                            "MomentaryLoudness",            // parameter name
                                                                            -20000,              // minimum value
                                                                            20000,              // maximum value
                                                                            -20000),
                                std::make_unique<juce::AudioParameterFloat>("integrated_loudness",            // parameterID
                                                                            "IntegratedLoudness",            // parameter name
                                                                            -20000,              // minimum value
                                                                            20000,              // maximum value
                                                                            -20000),
                                std::make_unique<juce::AudioParameterFloat>("short_term_loudness",            // parameterID
                                                                            "ShortTermLoudness",            // parameter name
                                                                            -20000,              // minimum value
                                                                            20000,              // maximum value
                                                                            -20000)

                           }),
    filter1(),
    filter2(),
    lufs_container(),
    bin_rms_container(),
    segment_square_sums()
#endif
{
    zero_passes = valueTreeState.getRawParameterValue("zero_passes");
    rms = valueTreeState.getRawParameterValue("rms");
    min = valueTreeState.getRawParameterValue("min");
    max = valueTreeState.getRawParameterValue("max");
    last_momentary_loudness = valueTreeState.getRawParameterValue("momentary_loudness");
    integrated_loudness = valueTreeState.getRawParameterValue("integrated_loudness");
    short_term_loudness = valueTreeState.getRawParameterValue("short_term_loudness");
    clearCounters();

    filter1.setCoefficients(juce::IIRCoefficients(1.53512485958697, -2.69169618940638, 1.19839281085285, 0.0, -1.69065929318241, 0.73248077421585));
    filter2.setCoefficients(juce::IIRCoefficients(1.0, -2.0, 1.0, 0.0, -1.99004745483398, 0.99007225036621));
}

AudioStatisticsPluginAudioProcessor::~AudioStatisticsPluginAudioProcessor()
{
}

//==============================================================================
const juce::String AudioStatisticsPluginAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool AudioStatisticsPluginAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool AudioStatisticsPluginAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool AudioStatisticsPluginAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double AudioStatisticsPluginAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int AudioStatisticsPluginAudioProcessor::getNumPrograms()
{
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
                // so this should be at least 1, even if you're not really implementing programs.
}

int AudioStatisticsPluginAudioProcessor::getCurrentProgram()
{
    return 0;
}

void AudioStatisticsPluginAudioProcessor::setCurrentProgram (int index)
{
}

const juce::String AudioStatisticsPluginAudioProcessor::getProgramName (int index)
{
    return {};
}

void AudioStatisticsPluginAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
}

//==============================================================================
void AudioStatisticsPluginAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    this->sampleRate = sampleRate;
    this->bin_length_in_samples = sampleRate / 10;
}

void AudioStatisticsPluginAudioProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool AudioStatisticsPluginAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
  #else
    // This is the place where you check if the layout is supported.
    // In this template code we only support mono or stereo.
    // Some plugin hosts, such as certain GarageBand versions, will only
    // load plugins that support stereo bus layouts.
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    // This checks if the input layout matches the output layout
   #if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
   #endif

    return true;
  #endif
}
#endif

void AudioStatisticsPluginAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();
    auto samplesNum = buffer.getNumSamples();

    // In case we have more outputs than inputs, this code clears any output
    // channels that didn't contain input data, (because these aren't
    // guaranteed to be empty - they may contain garbage).
    // This is here to avoid people getting screaming feedback
    // when they first compile a plugin, but obviously you don't need to keep
    // this code if your algorithm always overwrites all the output channels.
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, samplesNum);

    if (previous_length == nullptr || *previous_length != totalNumInputChannels) {
        previous_length.reset(new unsigned int(totalNumInputChannels));

        samples_count_per_channel.reset(new long long unsigned int[totalNumInputChannels]);
        square_sum_per_channel.reset(new float[totalNumInputChannels]);

        previous_samples.reset(new float[totalNumInputChannels]);
        for (int channel = 0; channel < totalNumInputChannels; ++channel)
        {
            previous_samples[channel] = *buffer.getWritePointer(channel);
            samples_count_per_channel[channel] = 0;
            square_sum_per_channel[channel] = 0;
        }

    }

    float temp_rms = 0;
       
    for (int channel = 0; channel < totalNumInputChannels; ++channel)
    {
        float* channelData = buffer.getWritePointer(channel);
        for (float* i = channelData; i < channelData + samplesNum; i++) {

            if ((previous_samples[channel] * (*i)) < 0) {
                zero_passes->store(zero_passes->load() + 1);
            }

            // RMS
            samples_count_per_channel[channel]++;
            square_sum_per_channel[channel] += (*i * *i);

            // Min Max
            if (*i > max->load()) {
                max->store(*i);
            }
            if (*i < min->load()) {
                min->store(*i);
            }

            previous_samples[channel] = *i;
        }

        temp_rms = temp_rms + std::sqrt(square_sum_per_channel[channel] / samples_count_per_channel[channel]);

        // LUFS
        if (channel == 0) {
            //https://www.mathworks.com/help/audio/ref/integratedloudness.html
            //https://www.itu.int/dms_pubrec/itu-r/rec/bs/R-REC-BS.1770-5-202311-I!!PDF-E.pdf
            //https://github.com/klangfreund/LUFSMeter/blob/master/Ebu128LoudnessMeter.cpp


            // Copy buffer so the original data won't get modified
            float* channelData_copy = new float[samplesNum];
            memcpy(channelData_copy, channelData, sizeof(float) * samplesNum);

            // Filter samples
            filter1.processSamples(channelData_copy, samplesNum);
            filter2.processSamples(channelData_copy, samplesNum);

            // Fill the bins with averages
            for (float* i = channelData; i < channelData + samplesNum; i++) {

                //// OLD VERSION
                //if (lufs_counter == 0) {
                //    lufs_container.push_back(std::vector<float>());
                //}

                //lufs_container.back().push_back(*i);
                //lufs_counter += 1;
            
                //if (lufs_counter >= sampleRate / 10 ) {
                //    lufs_counter = 0;
                //}

                // NEW VERSION

                if (current_position_in_filling_bin == 0) {
                    bin_rms_container.push_back(0);
                    //bin_sums.push_back(0);
                }

                bin_rms_container.back() += (*i * *i);
                //bin_sums.back() += *i;
                current_position_in_filling_bin++;

                if (current_position_in_filling_bin >= bin_length_in_samples) {
                    current_position_in_filling_bin = 0;
                    bin_rms_container.back() = bin_rms_container.back() / bin_length_in_samples;
                }
            }

            //jassert(bin_square_sums.size() == bin_sums.size());

            // Proceed to any LUFS calculation ONLY if new bin was added (and there is AT LEAST default 4 bins stored)
            // Call multiple time if there are multiple new bins to be processed
            // processed_bin_counter starts at 3, so adding FULL 4th bin will cause this while to be called for the first time.
            while ((bin_rms_container.size()-1 > processed_bin_counter) || ((bin_rms_container.size() > processed_bin_counter) && (current_position_in_filling_bin == 0))) {
                // Calculate Momentary LUFS
                float momentary_rms = 0.0;
                unsigned short int position_from_back = bin_rms_container.size() - processed_bin_counter - ((current_position_in_filling_bin == 0) ? 1 : 2);
                if (current_position_in_filling_bin == 0) { // If last bin is full, take last 4 bins
                    momentary_rms = std::accumulate(bin_rms_container.end() - bins_in_400ms - position_from_back, bin_rms_container.end() - position_from_back, 0.0) / bins_in_400ms;
                }
                else { // If last bin is not full
                    momentary_rms = std::accumulate(bin_rms_container.end() - position_from_back - bins_in_400ms-1, bin_rms_container.end() - position_from_back -1, 0.0) / bins_in_400ms;
                }
                // (sum of squares of samples in the last 400ms)/(no. of samples in the last 400ms)
                // Also called momentary power of segment

                processed_bin_counter++; // New bin is being processed - increase the amount of bins processed

                float momentary_loudness = -0.691 + 10 * std::log10(momentary_rms);
                if (momentary_loudness > -70.0) {
                    // First gate passed - display momentary loudness of current segment
                    last_momentary_loudness->store(momentary_loudness);

                    relative_threshold_acumulator += momentary_rms;
                    relative_threshold_segments_count++;

                    // rms over the whole measurement and relative treshold calculation
                    //float rms_from_the_begginig = std::accumulate(segment_square_sums.begin(), segment_square_sums.end(), 0.0) / segment_square_sums.size();
                    //float rms_from_the_begginig = std::accumulate(bin_rms_container.begin(), bin_rms_container.end(), 0.0) / bin_rms_container.size();
                    float rms_from_the_begginig = relative_threshold_acumulator / relative_threshold_segments_count;

                    float relative_treshold = -10.691 + 10.0 * std::log10(rms_from_the_begginig);

                    // TODO - Is this gate correct?
                    if ((momentary_rms >= relative_treshold) || (segment_square_sums.size() == 0)) {
                        // Second gate passed
                        segment_square_sums.push_back(momentary_rms);
                        integrated_loudness->store(-0.691 + 10.0 * std::log10(std::accumulate(segment_square_sums.begin(), segment_square_sums.end(), 0.0) / segment_square_sums.size()));
                    }
                }

                // if buffer is already long enough, we can also calculate short-term LUFS
                if (bin_rms_container.size() >= bins_in_3s) {
                    float short_term_rms = std::accumulate(bin_rms_container.end() - bins_in_3s, bin_rms_container.end(), 0.0) / (bins_in_3s);
                    short_term_loudness->store(-0.691 + 10.0 * std::log10(short_term_rms));
                }
            }

            //while (lufs_container.size() > 4) {
            //    unsigned int sample_counter = 0;
            //    float short_rms = 0.0;
            //    float short_mean_momentary_power = 0.0;
            //    for (auto row = lufs_container.begin(); row < lufs_container.begin() + 4; row++){
            //        for (auto i = row->begin(); i < row->end(); i++) {
            //            short_rms = short_rms + (*i * *i);
            //            short_mean_momentary_power += (*i);
            //            sample_counter++;
            //        }
            //    }
            //    lufs_container.erase(lufs_container.begin());

            //    short_rms = short_rms / sample_counter;
            //    short_mean_momentary_power = short_mean_momentary_power / sample_counter;

            //    float relative_treshold = -0.691 + 10 * std::log10(short_mean_momentary_power) - 10;
            //    float momentary_loudness = -0.691 + 10 * std::log10(short_rms);

            //    if (momentary_loudness >= -70 && short_rms >= relative_treshold) {
            //        //last_momentary_loudness->store(momentary_loudness);
            //        momentary_power_sum += short_rms;
            //        momentary_power_count += 1;
            //        //integrated_loudness->store(-0.691 + 10 * std::log10(momentary_power_sum / momentary_power_count));
            //    }
            //}
        }


    }
    rms->store(temp_rms * 1.0 / totalNumInputChannels);


     
    //juce::AudioBuffer<float> newBuffer;
    //newBuffer.makeCopyOf(buffer);

    //juce::dsp::AudioBlock <float> block(newBuffer);
    //filter1.process(juce::dsp::ProcessContextReplacing<float>(block));
    //filter2.process(juce::dsp::ProcessContextReplacing<float>(block));




}

//==============================================================================
bool AudioStatisticsPluginAudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor* AudioStatisticsPluginAudioProcessor::createEditor()
{
    return new AudioStatisticsPluginAudioProcessorEditor (*this, valueTreeState);
}

//==============================================================================
void AudioStatisticsPluginAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    // You should use this method to store your parameters in the memory block.
    // You could do that either as raw data, or use the XML or ValueTree classes
    // as intermediaries to make it easy to save and load complex data.
}

void AudioStatisticsPluginAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    // You should use this method to restore your parameters from this memory block,
    // whose contents will have been created by the getStateInformation() call.
}

void AudioStatisticsPluginAudioProcessor::clearCounters()
{
    zero_passes->store(0);
    rms->store(-std::numeric_limits<double>::infinity());

    min->store(std::numeric_limits<double>::infinity());
    max->store(-std::numeric_limits<double>::infinity());
    last_momentary_loudness->store(-std::numeric_limits<double>::infinity());
    integrated_loudness->store(-std::numeric_limits<double>::infinity());
    short_term_loudness->store(-std::numeric_limits<double>::infinity());

    processed_bin_counter = bins_in_400ms - 1;
    bin_rms_container.clear();
    segment_square_sums.clear();
    current_position_in_filling_bin = 0;

    //momentary_power_sum = 0.0;
    //momentary_power_count = 0;

    if (previous_length == nullptr) {
        return;
    }

    int channel_len = *previous_length.get();

    samples_count_per_channel.reset(new long long unsigned int[channel_len]);
    square_sum_per_channel.reset(new float[channel_len]);

    for (int channel = 0; channel < channel_len; ++channel)
    {
        samples_count_per_channel[channel] = 0;
        square_sum_per_channel[channel] = 0;
    }
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new AudioStatisticsPluginAudioProcessor();
}
