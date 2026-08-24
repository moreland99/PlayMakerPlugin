#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <atomic>
#include <cmath>

PlaymakersEQAudioProcessor::PlaymakersEQAudioProcessor()
    : AudioProcessor(BusesProperties()
                          .withInput("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput("Output", juce::AudioChannelSet::stereo(), true)
                          .withInput("Sidechain", juce::AudioChannelSet::stereo(), false)),
      apvts(*this, &undoManager, "PARAMETERS", Params::createParameterLayout()),
      presetManager(apvts, undoManager)
{
    for (int i = 0; i < Params::numBands; ++i)
    {
        auto& p = paramPointers[(size_t) i];
        p.enabled = apvts.getRawParameterValue(Params::bandParamID(i, "enabled"));
        p.solo = apvts.getRawParameterValue(Params::bandParamID(i, "solo"));
        p.type = apvts.getRawParameterValue(Params::bandParamID(i, "type"));
        p.freq = apvts.getRawParameterValue(Params::bandParamID(i, "freq"));
        p.gain = apvts.getRawParameterValue(Params::bandParamID(i, "gain"));
        p.q = apvts.getRawParameterValue(Params::bandParamID(i, "q"));
        p.stereoMode = apvts.getRawParameterValue(Params::bandParamID(i, "stereoMode"));
        p.balance = apvts.getRawParameterValue(Params::bandParamID(i, "balance"));
        p.slope = apvts.getRawParameterValue(Params::bandParamID(i, "slope"));
        p.brickwall = apvts.getRawParameterValue(Params::bandParamID(i, "brickwall"));
        p.dynEnabled = apvts.getRawParameterValue(Params::bandParamID(i, "dynEnabled"));
        p.dynThreshold = apvts.getRawParameterValue(Params::bandParamID(i, "dynThreshold"));
        p.dynAutoThreshold = apvts.getRawParameterValue(Params::bandParamID(i, "dynAutoThreshold"));
        p.dynRange = apvts.getRawParameterValue(Params::bandParamID(i, "dynRange"));
        p.dynRatio = apvts.getRawParameterValue(Params::bandParamID(i, "dynRatio"));
        p.dynAttack = apvts.getRawParameterValue(Params::bandParamID(i, "dynAttack"));
        p.dynRelease = apvts.getRawParameterValue(Params::bandParamID(i, "dynRelease"));
        p.dynRelativeBlend = apvts.getRawParameterValue(Params::bandParamID(i, "dynRelativeBlend"));
        p.dynSidechainBlend = apvts.getRawParameterValue(Params::bandParamID(i, "dynSidechainBlend"));
    }

    globalPointers.phaseMode = apvts.getRawParameterValue("phaseMode");
    globalPointers.linearQuality = apvts.getRawParameterValue("linearQuality");
    globalPointers.outputGain = apvts.getRawParameterValue("outputGain");
    globalPointers.pluginBypass = apvts.getRawParameterValue("pluginBypass");

    presetManager.loadDefaultPresetIfPresent();

    for (auto& d : dynDisplayOffsetDb)
        d.store(0.0f, std::memory_order_relaxed);
    for (auto& m : dynDetectionMeterDb)
        m.store(-100.0f, std::memory_order_relaxed);

    startTimerHz(8);
}

PlaymakersEQAudioProcessor::~PlaymakersEQAudioProcessor()
{
    stopTimer();
}

void PlaymakersEQAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;

    juce::dsp::ProcessSpec monoSpec;
    monoSpec.sampleRate = sampleRate;
    monoSpec.maximumBlockSize = (juce::uint32) samplesPerBlock;
    monoSpec.numChannels = 1;

    for (auto& band : bands)
        band.prepare(monoSpec);

    for (auto& det : dynDetectors)
        det.prepare(sampleRate);

    for (int i = 0; i < Params::numBands; ++i)
    {
        auto& p = paramPointers[(size_t) i];
        auto& sm = smoothers[(size_t) i];
        for (auto* s : { &sm.freq, &sm.gain, &sm.q })
            s->reset(sampleRate, 0.02);
        sm.freq.setCurrentAndTargetValue(p.freq->load());
        sm.gain.setCurrentAndTargetValue(p.gain->load());
        sm.q.setCurrentAndTargetValue(p.q->load());
        lastAppliedCoeffs[(size_t) i].valid = false;
        updateBandCoefficients(i, 0.0f, 1);
    }

    const auto channels = juce::jmax(1, getMainBusNumOutputChannels());
    linearEQ.prepare({ sampleRate, (juce::uint32) samplesPerBlock, (juce::uint32) channels });

    const auto mode = static_cast<Params::PhaseMode>((int) globalPointers.phaseMode->load());
    const auto quality = (int) globalPointers.linearQuality->load();
    linearEQ.setNumTaps(firTapsForMode(mode, quality));
    setLatencySamples(mode == Params::PhaseMode::zeroLatency ? 0 : linearEQ.getLatencySamples());
    lastFirParamsHash = 0; // force a rebuild on the next timer tick

    scratchPreL.setSize(1, samplesPerBlock);
    scratchPreR.setSize(1, samplesPerBlock);
    scratchA.setSize(1, samplesPerBlock);
    scratchB.setSize(1, samplesPerBlock);
    monoScratch.resize((size_t) samplesPerBlock);
    preMonoScratch.resize((size_t) samplesPerBlock);
    scMonoScratch.resize((size_t) samplesPerBlock);
    detectorScratch.resize((size_t) samplesPerBlock);

    outputGainLinear.reset(sampleRate, 0.02);
    outputGainLinear.setCurrentAndTargetValue(
        juce::Decibels::decibelsToGain(globalPointers.outputGain->load()));
}

void PlaymakersEQAudioProcessor::releaseResources()
{
}

int PlaymakersEQAudioProcessor::firTapsForMode(Params::PhaseMode mode, int quality) const
{
    if (mode == Params::PhaseMode::lowLatencyCorrected)
        return 127;
    switch (quality)
    {
        case 0: return 511;
        case 2: return 8191;
        case 1:
        default: return 2047;
    }
}

bool PlaymakersEQAudioProcessor::anyBandSoloActive() const
{
    for (const auto& p : paramPointers)
        if (p.enabled->load() >= 0.5f && p.solo->load() >= 0.5f)
            return true;
    return false;
}

bool PlaymakersEQAudioProcessor::bandContributesToAudio(int bandIndex) const
{
    const auto& p = paramPointers[(size_t) bandIndex];
    if (p.enabled->load() < 0.5f)
        return false;
    if (anyBandSoloActive() && p.solo->load() < 0.5f)
        return false;
    return true;
}

bool PlaymakersEQAudioProcessor::bandUsesFIR(int bandIndex) const
{
    if (!bandContributesToAudio(bandIndex))
        return false;

    // The FIR carries only plain stereo-linked static bands. Dynamic bands and bands with
    // stereo/MS routing keep the minimum-phase IIR path even in FIR modes, layered on top.
    const auto& p = paramPointers[(size_t) bandIndex];
    if (p.enabled->load() < 0.5f)
        return false;

    const auto type = static_cast<Params::FilterType>((int) p.type->load());
    if (p.dynEnabled->load() >= 0.5f && Params::typeSupportsDynamics(type))
        return false;

    return static_cast<Params::StereoMode>((int) p.stereoMode->load()) == Params::StereoMode::leftRight
        && std::abs(p.balance->load()) < 0.001f;
}

juce::uint64 PlaymakersEQAudioProcessor::computeParamsHash() const
{
    auto fold = [](juce::uint64 h, float v)
    {
        juce::uint32 bits;
        std::memcpy(&bits, &v, sizeof(bits));
        return h * 1099511628211ULL + bits;
    };

    juce::uint64 h = 14695981039346656037ULL;
    for (const auto& p : paramPointers)
        for (auto* v : { p.enabled, p.solo, p.type, p.freq, p.gain, p.q, p.stereoMode, p.balance,
                          p.slope, p.brickwall, p.dynEnabled })
            h = fold(h, v->load());

    h = fold(h, globalPointers.phaseMode->load());
    h = fold(h, globalPointers.linearQuality->load());
    h = fold(h, (float) currentSampleRate);
    return h;
}

void PlaymakersEQAudioProcessor::rebuildLinearPhase(Params::PhaseMode mode)
{
    if (currentSampleRate <= 0.0)
        return;

    // Snapshot stage sets for all FIR-qualifying bands, then sample the composite magnitude.
    std::vector<FilterBand::StageSet> stageSets;
    for (int i = 0; i < Params::numBands; ++i)
    {
        if (!bandUsesFIR(i))
            continue;

        const auto& p = paramPointers[(size_t) i];
        stageSets.push_back(FilterBand::computeStages(
            static_cast<Params::FilterType>((int) p.type->load()), currentSampleRate,
            p.freq->load(), p.gain->load(), p.q->load(), p.slope->load(), p.brickwall->load() >= 0.5f));
    }

    linearEQ.setNumTaps(firTapsForMode(mode, (int) globalPointers.linearQuality->load()));
    linearEQ.updateResponse(currentSampleRate, [this, &stageSets](double freq)
    {
        double magnitude = 1.0;
        for (const auto& set : stageSets)
            magnitude *= FilterBand::getMagnitudeForFrequency(set, freq, currentSampleRate);
        return magnitude;
    });
}

void PlaymakersEQAudioProcessor::timerCallback()
{
    const auto mode = static_cast<Params::PhaseMode>((int) globalPointers.phaseMode->load());
    const auto quality = (int) globalPointers.linearQuality->load();

    const int wantedLatency = mode == Params::PhaseMode::zeroLatency
        ? 0
        : firTapsForMode(mode, quality) / 2;
    if (wantedLatency != getLatencySamples())
        setLatencySamples(wantedLatency);

    if (mode == Params::PhaseMode::zeroLatency)
        return;

    const auto hash = computeParamsHash();
    if (hash == lastFirParamsHash)
        return;

    lastFirParamsHash = hash;
    rebuildLinearPhase(mode);
}

void PlaymakersEQAudioProcessor::toggleAB()
{
    auto current = apvts.copyState();
    if (onSlotA)
    {
        slotA = current;
        if (slotB.isValid())
            apvts.replaceState(slotB.createCopy());
        onSlotA = false;
    }
    else
    {
        slotB = current;
        if (slotA.isValid())
            apvts.replaceState(slotA.createCopy());
        onSlotA = true;
    }
}

void PlaymakersEQAudioProcessor::copyCurrentToOtherSlot()
{
    if (onSlotA)
        slotB = apvts.copyState();
    else
        slotA = apvts.copyState();
}

void PlaymakersEQAudioProcessor::updateBandCoefficients(int bandIndex, float dynGainOffsetDb, int numSamplesForSmoothing)
{
    const auto& p = paramPointers[(size_t) bandIndex];
    auto& sm = smoothers[(size_t) bandIndex];

    sm.freq.setTargetValue(p.freq->load());
    sm.gain.setTargetValue(p.gain->load());
    sm.q.setTargetValue(p.q->load());

    const auto freq = sm.freq.skip(numSamplesForSmoothing);
    const auto gain = sm.gain.skip(numSamplesForSmoothing);
    const auto q = sm.q.skip(numSamplesForSmoothing);

    const auto type = static_cast<Params::FilterType>((int) p.type->load());
    const auto slope = p.slope->load();
    const bool brickwall = p.brickwall->load() >= 0.5f;
    const auto appliedGain = gain + dynGainOffsetDb;

    auto& last = lastAppliedCoeffs[(size_t) bandIndex];
    constexpr float eps = 1.0e-5f;
    if (last.valid
        && last.type == type
        && last.brickwall == brickwall
        && std::abs(last.freq - freq) < eps
        && std::abs(last.gain - appliedGain) < eps
        && std::abs(last.q - q) < eps
        && std::abs(last.slope - slope) < eps
        && std::abs(last.dynOffset - dynGainOffsetDb) < eps)
        return;

    bands[(size_t) bandIndex].update(type, freq, appliedGain, q, slope, brickwall);
    last = { type, freq, appliedGain, q, slope, dynGainOffsetDb, brickwall, true };
}

bool PlaymakersEQAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
        && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;

    // Sidechain: disabled, mono, or stereo are all fine.
    if (layouts.inputBuses.size() > 1)
    {
        const auto sc = layouts.getChannelSet(true, 1);
        if (!sc.isDisabled() && sc != juce::AudioChannelSet::mono() && sc != juce::AudioChannelSet::stereo())
            return false;
    }

    return true;
}

float PlaymakersEQAudioProcessor::getDynDetectionMeterDb(int bandIndex) const
{
    if (bandIndex < 0 || bandIndex >= Params::numBands)
        return -100.0f;
    return dynDetectionMeterDb[(size_t) bandIndex].load(std::memory_order_relaxed);
}

void PlaymakersEQAudioProcessor::applyOutputGain(float* leftData, float* rightData, int numSamples)
{
    outputGainLinear.setTargetValue(
        juce::Decibels::decibelsToGain(globalPointers.outputGain->load()));

    for (int n = 0; n < numSamples; ++n)
    {
        const float g = outputGainLinear.getNextValue();
        leftData[n] *= g;
        if (rightData != nullptr)
            rightData[n] *= g;
    }
}

void PlaymakersEQAudioProcessor::updateOutputMeters(const float* leftData, const float* rightData, int numSamples)
{
    float pkL = 0.0f, pkR = 0.0f;
    for (int n = 0; n < numSamples; ++n)
    {
        pkL = juce::jmax(pkL, std::abs(leftData[n]));
        pkR = juce::jmax(pkR, std::abs(rightData != nullptr ? rightData[n] : leftData[n]));
    }

    const float dbL = juce::Decibels::gainToDecibels(pkL, -100.0f);
    const float dbR = juce::Decibels::gainToDecibels(pkR, -100.0f);
    const float dt = (float) numSamples / (float) juce::jmax(1.0, currentSampleRate);
    const float release = 1.0f - std::exp(-dt / 0.30f);
    const int holdSamples = (int) std::lround(1.5 * juce::jmax(1.0, currentSampleRate));

    auto follow = [release] (float& env, float target)
    {
        if (target > env)
            env = target;
        else
            env += (target - env) * release;
    };
    follow(meterEnvL, dbL);
    follow(meterEnvR, dbR);

    auto hold = [holdSamples, numSamples] (float& held, int& remaining, float env, float instant)
    {
        if (instant >= held)
        {
            held = instant;
            remaining = holdSamples;
        }
        else
        {
            remaining -= numSamples;
            if (remaining <= 0)
                held = env;
        }
    };
    hold(meterHoldL, meterHoldSamplesL, meterEnvL, dbL);
    hold(meterHoldR, meterHoldSamplesR, meterEnvR, dbR);

    outputMeters.peakL.store(meterEnvL, std::memory_order_relaxed);
    outputMeters.peakR.store(meterEnvR, std::memory_order_relaxed);
    outputMeters.holdL.store(meterHoldL, std::memory_order_relaxed);
    outputMeters.holdR.store(meterHoldR, std::memory_order_relaxed);
}

void PlaymakersEQAudioProcessor::pushPostAnalyzerFromBus(float* leftData, float* rightData, int numSamples)
{
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

void PlaymakersEQAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;

    if (presetManager.isMidiProgramChangeEnabled())
    {
        for (const auto metadata : midi)
        {
            const auto msg = metadata.getMessage();
            if (msg.isProgramChange())
            {
                const int prog = msg.getProgramChangeNumber();
                juce::MessageManager::callAsync([this, prog] { presetManager.loadFactoryProgram(prog); });
            }
        }
    }
    midi.clear();

    auto mainBus = getBusBuffer(buffer, true, 0);
    const auto numChannels = mainBus.getNumChannels();
    const auto numSamples = buffer.getNumSamples();
    if (numChannels == 0 || numSamples == 0)
        return;

    auto* leftData = mainBus.getWritePointer(0);
    auto* rightData = numChannels > 1 ? mainBus.getWritePointer(1) : nullptr;

    // Pre-EQ mono + sidechain mono captured up front for the dynamic detectors.
    for (int n = 0; n < numSamples; ++n)
        preMonoScratch[(size_t) n] = rightData != nullptr ? 0.5f * (leftData[n] + rightData[n]) : leftData[n];

    preAnalyzer.pushSamples(preMonoScratch.data(), numSamples);

    const bool pluginBypassed = globalPointers.pluginBypass->load() >= 0.5f;
    if (pluginBypassed)
    {
        for (int i = 0; i < Params::numBands; ++i)
            dynDisplayOffsetDb[(size_t) i].store(0.0f, std::memory_order_relaxed);

        applyOutputGain(leftData, rightData, numSamples);
        updateOutputMeters(leftData, rightData, numSamples);
        pushPostAnalyzerFromBus(leftData, rightData, numSamples);
        return;
    }

    std::fill(scMonoScratch.begin(), scMonoScratch.begin() + numSamples, 0.0f);
    if (getBusCount(true) > 1)
    {
        auto scBus = getBusBuffer(buffer, true, 1);
        const auto scChannels = scBus.getNumChannels();
        if (scChannels > 0)
        {
            for (int n = 0; n < numSamples; ++n)
            {
                float sum = 0.0f;
                for (int ch = 0; ch < scChannels; ++ch)
                    sum += scBus.getReadPointer(ch)[n];
                scMonoScratch[(size_t) n] = sum / (float) scChannels;
            }
        }
    }

    const auto mode = static_cast<Params::PhaseMode>((int) globalPointers.phaseMode->load());

    // FIR modes: the composite static curve runs through the convolver first.
    if (mode != Params::PhaseMode::zeroLatency)
    {
        juce::dsp::AudioBlock<float> block(mainBus);
        linearEQ.process(juce::dsp::ProcessContextReplacing<float>(block));
    }

    for (int i = 0; i < Params::numBands; ++i)
    {
        if (!bandContributesToAudio(i))
        {
            dynDisplayOffsetDb[(size_t) i].store(0.0f, std::memory_order_relaxed);
            continue;
        }
        if (mode != Params::PhaseMode::zeroLatency && bandUsesFIR(i))
        {
            dynDisplayOffsetDb[(size_t) i].store(0.0f, std::memory_order_relaxed);
            continue;
        }

        const auto& p = paramPointers[(size_t) i];
        const auto type = static_cast<Params::FilterType>((int) p.type->load());

        float dynOffsetDb = 0.0f;
        if (p.dynEnabled->load() >= 0.5f && Params::typeSupportsDynamics(type))
        {
            const float scBlend = p.dynSidechainBlend->load();
            for (int n = 0; n < numSamples; ++n)
                detectorScratch[(size_t) n] = preMonoScratch[(size_t) n] * (1.0f - scBlend)
                                             + scMonoScratch[(size_t) n] * scBlend;

            DynamicBandDetector::Settings s;
            s.freqHz = p.freq->load();
            s.q = p.q->load();
            s.thresholdDb = p.dynThreshold->load();
            if (p.dynAutoThreshold->load() >= 0.5f)
                s.thresholdDb = dynDetectors[(size_t) i].getLastDetectionDb();
            s.rangeDb = p.dynRange->load();
            s.ratio = p.dynRatio->load();
            s.attackMs = p.dynAttack->load();
            s.releaseMs = p.dynRelease->load();
            s.relativeBlend = p.dynRelativeBlend->load();
            dynOffsetDb = dynDetectors[(size_t) i].processBlock(detectorScratch.data(), numSamples, s);
            dynDetectionMeterDb[(size_t) i].store(dynDetectors[(size_t) i].getLastDetectionDb(),
                                                    std::memory_order_relaxed);
        }

        dynDisplayOffsetDb[(size_t) i].store(
            (p.dynEnabled->load() >= 0.5f && Params::typeSupportsDynamics(type)) ? dynOffsetDb : 0.0f,
            std::memory_order_relaxed);

        updateBandCoefficients(i, dynOffsetDb, numSamples);

        if (rightData == nullptr)
        {
            juce::dsp::AudioBlock<float> monoBlock(&leftData, 1, (size_t) numSamples);
            bands[(size_t) i].processLeft(juce::dsp::ProcessContextReplacing<float>(monoBlock));
            continue;
        }

        processStereoBand(i, leftData, rightData, numSamples);
    }

    applyOutputGain(leftData, rightData, numSamples);
    updateOutputMeters(leftData, rightData, numSamples);
    pushPostAnalyzerFromBus(leftData, rightData, numSamples);
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
    return presetManager.isMidiProgramChangeEnabled();
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
    return presetManager.getProgramCount();
}

int PlaymakersEQAudioProcessor::getCurrentProgram()
{
    if (!presetManager.isMidiProgramChangeEnabled())
        return 0;

    int seen = 0;
    const auto id = presetManager.getCurrentPresetId();
    for (const auto& e : presetManager.getCatalog())
    {
        if (e.kind != PresetManager::Kind::factory)
            continue;
        if (e.id == id)
            return seen;
        ++seen;
    }
    return 0;
}

void PlaymakersEQAudioProcessor::setCurrentProgram(int index)
{
    if (presetManager.isMidiProgramChangeEnabled())
        presetManager.loadFactoryProgram(index);
}

const juce::String PlaymakersEQAudioProcessor::getProgramName(int index)
{
    return presetManager.getFactoryProgramName(index);
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
