#include "PluginProcessor.h"
#include "PluginEditor.h"

PlaymakersEQAudioProcessor::PlaymakersEQAudioProcessor()
    : AudioProcessor(BusesProperties()
                          .withInput("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "PARAMETERS", Params::createParameterLayout())
{
    for (int i = 0; i < Params::numBands; ++i)
    {
        auto& p = paramPointers[(size_t) i];
        p.enabled = apvts.getRawParameterValue(Params::bandParamID(i, "enabled"));
        p.type = apvts.getRawParameterValue(Params::bandParamID(i, "type"));
        p.freq = apvts.getRawParameterValue(Params::bandParamID(i, "freq"));
        p.gain = apvts.getRawParameterValue(Params::bandParamID(i, "gain"));
        p.q = apvts.getRawParameterValue(Params::bandParamID(i, "q"));
    }
}

PlaymakersEQAudioProcessor::~PlaymakersEQAudioProcessor() = default;

void PlaymakersEQAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    juce::dsp::ProcessSpec monoSpec;
    monoSpec.sampleRate = sampleRate;
    monoSpec.maximumBlockSize = (juce::uint32) samplesPerBlock;
    monoSpec.numChannels = 1;

    for (auto& band : bands)
        band.prepare(monoSpec);

    for (int i = 0; i < Params::numBands; ++i)
        updateBandCoefficients(i);
}

void PlaymakersEQAudioProcessor::releaseResources()
{
}

void PlaymakersEQAudioProcessor::updateBandCoefficients(int bandIndex)
{
    const auto& p = paramPointers[(size_t) bandIndex];
    const auto type = static_cast<Params::FilterType>((int) p.type->load());
    bands[(size_t) bandIndex].update(type, p.freq->load(), p.gain->load(), p.q->load());
}

bool PlaymakersEQAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
        && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    return layouts.getMainOutputChannelSet() == layouts.getMainInputChannelSet();
}

void PlaymakersEQAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    for (auto channel = getTotalNumInputChannels(); channel < getTotalNumOutputChannels(); ++channel)
        buffer.clear(channel, 0, buffer.getNumSamples());

    const auto numChannels = buffer.getNumChannels();
    if (numChannels == 0)
        return;

    juce::dsp::AudioBlock<float> block(buffer);
    auto leftBlock = block.getSingleChannelBlock(0);
    auto rightBlock = numChannels > 1 ? block.getSingleChannelBlock(1) : leftBlock;

    for (int i = 0; i < Params::numBands; ++i)
    {
        if (paramPointers[(size_t) i].enabled->load() < 0.5f)
            continue;

        updateBandCoefficients(i);

        bands[(size_t) i].processLeft(juce::dsp::ProcessContextReplacing<float>(leftBlock));
        if (numChannels > 1)
            bands[(size_t) i].processRight(juce::dsp::ProcessContextReplacing<float>(rightBlock));
    }
}

juce::AudioProcessorEditor* PlaymakersEQAudioProcessor::createEditor()
{
    return new PlaymakersEQAudioProcessorEditor(*this);
}

bool PlaymakersEQAudioProcessor::hasEditor() const
{
    return true;
}

const juce::String PlaymakersEQAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool PlaymakersEQAudioProcessor::acceptsMidi() const
{
    return false;
}

bool PlaymakersEQAudioProcessor::producesMidi() const
{
    return false;
}

bool PlaymakersEQAudioProcessor::isMidiEffect() const
{
    return false;
}

double PlaymakersEQAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int PlaymakersEQAudioProcessor::getNumPrograms()
{
    return 1;
}

int PlaymakersEQAudioProcessor::getCurrentProgram()
{
    return 0;
}

void PlaymakersEQAudioProcessor::setCurrentProgram(int)
{
}

const juce::String PlaymakersEQAudioProcessor::getProgramName(int)
{
    return {};
}

void PlaymakersEQAudioProcessor::changeProgramName(int, const juce::String&)
{
}

void PlaymakersEQAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    if (auto state = apvts.copyState(); state.isValid())
    {
        if (auto xml = state.createXml())
            copyXmlToBinary(*xml, destData);
    }
}

void PlaymakersEQAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary(data, sizeInBytes))
        if (xml->hasTagName(apvts.state.getType()))
            apvts.replaceState(juce::ValueTree::fromXml(*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PlaymakersEQAudioProcessor();
}
