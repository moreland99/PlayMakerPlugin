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
        p.stereoMode = apvts.getRawParameterValue(Params::bandParamID(i, "stereoMode"));
        p.balance = apvts.getRawParameterValue(Params::bandParamID(i, "balance"));
    }
}

PlaymakersEQAudioProcessor::~PlaymakersEQAudioProcessor() = default;

void PlaymakersEQAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;

    juce::dsp::ProcessSpec monoSpec;
    monoSpec.sampleRate = sampleRate;
    monoSpec.maximumBlockSize = (juce::uint32) samplesPerBlock;
    monoSpec.numChannels = 1;

    for (auto& band : bands)
        band.prepare(monoSpec);

    for (int i = 0; i < Params::numBands; ++i)
        updateBandCoefficients(i);

    scratchPreL.setSize(1, samplesPerBlock);
    scratchPreR.setSize(1, samplesPerBlock);
    scratchA.setSize(1, samplesPerBlock);
    scratchB.setSize(1, samplesPerBlock);
    monoScratch.resize((size_t) samplesPerBlock);
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

    const auto numSamples = buffer.getNumSamples();
    auto* leftData = buffer.getWritePointer(0);
    auto* rightData = numChannels > 1 ? buffer.getWritePointer(1) : nullptr;

    for (int i = 0; i < Params::numBands; ++i)
    {
        if (paramPointers[(size_t) i].enabled->load() < 0.5f)
            continue;

        updateBandCoefficients(i);

        if (rightData == nullptr)
        {
            juce::dsp::AudioBlock<float> monoBlock(&leftData, 1, (size_t) numSamples);
            bands[(size_t) i].processLeft(juce::dsp::ProcessContextReplacing<float>(monoBlock));
            continue;
        }

        processStereoBand(i, leftData, rightData, numSamples);
    }

    // Feed the analyzer a mono mixdown of the post-EQ signal.
    if (rightData != nullptr)
    {
        for (int n = 0; n < numSamples; ++n)
            monoScratch[(size_t) n] = 0.5f * (leftData[n] + rightData[n]);
    }
    else
    {
        std::copy(leftData, leftData + numSamples, monoScratch.begin());
    }
    postAnalyzer.pushSamples(monoScratch.data(), numSamples);
}

void PlaymakersEQAudioProcessor::processStereoBand(int bandIndex, float* leftData, float* rightData, int numSamples)
{
    const auto& p = paramPointers[(size_t) bandIndex];
    const auto mode = static_cast<Params::StereoMode>((int) p.stereoMode->load());
    const bool msDomain = (mode == Params::StereoMode::midSide
                            || mode == Params::StereoMode::midOnly
                            || mode == Params::StereoMode::sideOnly);

    float effectiveBalance = p.balance->load();
    if (mode == Params::StereoMode::leftOnly || mode == Params::StereoMode::midOnly)
        effectiveBalance = -1.0f;
    else if (mode == Params::StereoMode::rightOnly || mode == Params::StereoMode::sideOnly)
        effectiveBalance = 1.0f;

    const float wetA = 1.0f - juce::jmax(0.0f, effectiveBalance);
    const float wetB = 1.0f - juce::jmax(0.0f, -effectiveBalance);

    scratchPreL.copyFrom(0, 0, leftData, numSamples);
    scratchPreR.copyFrom(0, 0, rightData, numSamples);
    auto* preL = scratchPreL.getReadPointer(0);
    auto* preR = scratchPreR.getReadPointer(0);

    auto* workA = scratchA.getWritePointer(0);
    auto* workB = scratchB.getWritePointer(0);

    for (int n = 0; n < numSamples; ++n)
    {
        if (msDomain)
        {
            workA[n] = 0.5f * (preL[n] + preR[n]);
            workB[n] = 0.5f * (preL[n] - preR[n]);
        }
        else
        {
            workA[n] = preL[n];
            workB[n] = preR[n];
        }
    }

    juce::dsp::AudioBlock<float> blockA(scratchA);
    juce::dsp::AudioBlock<float> blockB(scratchB);
    bands[(size_t) bandIndex].processLeft(juce::dsp::ProcessContextReplacing<float>(blockA));
    bands[(size_t) bandIndex].processRight(juce::dsp::ProcessContextReplacing<float>(blockB));

    for (int n = 0; n < numSamples; ++n)
    {
        const float preA = msDomain ? 0.5f * (preL[n] + preR[n]) : preL[n];
        const float preB = msDomain ? 0.5f * (preL[n] - preR[n]) : preR[n];
        const float mixedA = preA + wetA * (workA[n] - preA);
        const float mixedB = preB + wetB * (workB[n] - preB);

        if (msDomain)
        {
            leftData[n] = mixedA + mixedB;
            rightData[n] = mixedA - mixedB;
        }
        else
        {
            leftData[n] = mixedA;
            rightData[n] = mixedB;
        }
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
