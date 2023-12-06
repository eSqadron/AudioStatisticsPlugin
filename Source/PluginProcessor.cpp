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
                                                                            -20000)

                           }),
    filter1(),
    filter2(),
    lufs_container()
    //filter2(juce::dsp::IIR::Coefficients<float>(
    //    1.0,               // b0
    //    -2.0,              // b1
    //    1.0,
    //    0.0,
    //    -1.99004745483398, // a1
    //    0.99007225036621))
#endif
{
    zero_passes = valueTreeState.getRawParameterValue("zero_passes");
    rms = valueTreeState.getRawParameterValue("rms");
    min = valueTreeState.getRawParameterValue("min");
    max = valueTreeState.getRawParameterValue("max");
    last_momentary_loudness = valueTreeState.getRawParameterValue("momentary_loudness");
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
            //samples_count_per_channel[channel] = 0;
            //square_sum_per_channel[channel] = 0;
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



            float* channelData_copy = new float[samplesNum];
            memcpy(channelData_copy, channelData, sizeof(float) * samplesNum);

            filter1.processSamples(channelData_copy, samplesNum);
            filter2.processSamples(channelData_copy, samplesNum);

            for (float* i = channelData; i < channelData + samplesNum; i++) {
                if (lufs_counter == 0) {
                    lufs_container.push_back(std::vector<float>());
                }

                lufs_container.back().push_back(*i);
                lufs_counter += 1;
            
                if (lufs_counter >= sampleRate / 10 ) {
                    lufs_counter = 0;
                }
            }

            while (lufs_container.size() > 4) {
                unsigned int sample_counter = 0;
                float short_rms = 0.0;
                for (auto row = lufs_container.begin(); row < lufs_container.begin() + 4; row++){
                    for (auto i = row->begin(); i < row->end(); i++) {
                        short_rms = short_rms + (*i * *i);
                        sample_counter++;
                    }
                }
                lufs_container.erase(lufs_container.begin());

                short_rms = short_rms / (sample_counter);

                float momentary_loudness = -0.691 + 10 * std::log10(short_rms);

                if (momentary_loudness >= -70) {
                    last_momentary_loudness->store(momentary_loudness);
                }
                else {
                    last_momentary_loudness->store(-std::numeric_limits<double>::infinity());
                }
            }
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
