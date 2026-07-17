#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "Params.h"
#include "FilterBand.h"
#include "DynamicBand.h"
#include "LinearPhaseEQ.h"
#include "SpectrumAnalyzer.h"

class PlaymakersEQAudioProcessor : public juce::AudioProcessor, private juce::Timer
{
public:
    PlaymakersEQAudioProcessor();
    ~PlaymakersEQAudioProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int index, const juce::String& newName) override;

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    // Declared before apvts: the tree state keeps a pointer to it for undoable edits.
    juce::UndoManager undoManager;
    juce::AudioProcessorValueTreeState apvts;

    AnalyzerDataProvider& getPostAnalyzer() { return postAnalyzer; }
    double& getSampleRateRef() { return currentSampleRate; }

    // A/B comparison — message thread only.
    void toggleAB();
    void copyCurrentToOtherSlot();
    bool isOnSlotA() const { return onSlotA; }

private:
    void timerCallback() override;
    juce::uint64 computeParamsHash() const;
    void rebuildLinearPhase(Params::PhaseMode mode);
    int firTapsForMode(Params::PhaseMode mode, int quality) const;
    bool bandUsesFIR(int bandIndex) const;
    void updateBandCoefficients(int bandIndex, float dynGainOffsetDb, int numSamplesForSmoothing);
    void processStereoBand(int bandIndex, float* leftData, float* rightData, int numSamples);

    struct BandSmoothers
    {
        juce::SmoothedValue<float> freq, gain, q;
    };

    std::array<FilterBand, Params::numBands> bands;
    std::array<Params::BandParamPointers, Params::numBands> paramPointers;
    std::array<BandSmoothers, Params::numBands> smoothers;
    std::array<DynamicBandDetector, Params::numBands> dynDetectors;
    Params::GlobalParamPointers globalPointers;

    LinearPhaseEQ linearEQ;
    juce::uint64 lastFirParamsHash = 0;

    juce::AudioBuffer<float> scratchPreL, scratchPreR, scratchA, scratchB;
    AnalyzerDataProvider postAnalyzer;
    double currentSampleRate = 0.0;
    std::vector<float> monoScratch, preMonoScratch, scMonoScratch, detectorScratch;

    juce::ValueTree slotA, slotB;
    bool onSlotA = true;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PlaymakersEQAudioProcessor)
};
