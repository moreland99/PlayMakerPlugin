#include "PluginProcessor.h"
#include "PluginEditor.h"

PlaymakersEQAudioProcessor::PlaymakersEQAudioProcessor()
    : AudioProcessor(BusesProperties()
                          .withInput("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput("Output", juce::AudioChannelSet::stereo(), true))
{
}

PlaymakersEQAudioProcessor::~PlaymakersEQAudioProcessor() = default;

void PlaymakersEQAudioProcessor::prepareToPlay(double, int)
{
}

void PlaymakersEQAudioProcessor::releaseResources()
{
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

void PlaymakersEQAudioProcessor::getStateInformation(juce::MemoryBlock&)
{
}

void PlaymakersEQAudioProcessor::setStateInformation(const void*, int)
{
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PlaymakersEQAudioProcessor();
}
