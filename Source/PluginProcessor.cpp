/*
  ==============================================================================
    Hisstory – PluginProcessor.cpp

    STFT-based spectral gating de-hiss engine, focused on 4 kHz–12 kHz.
  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
//  Parameter layout
//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout
HisstoryAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    // ── Main controls ────────────────────────────────────────────────────────
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "threshold", 1 }, "Threshold",
        juce::NormalisableRange<float> (-40.0f, 10.0f, 0.1f), -13.0f,
        juce::AudioParameterFloatAttributes().withLabel ("dB")));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "reduction", 1 }, "Reduction",
        juce::NormalisableRange<float> (0.0f, 40.0f, 0.1f), 12.0f,
        juce::AudioParameterFloatAttributes().withLabel ("dB")));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "smoothing", 1 }, "Smoothing",
        juce::NormalisableRange<float> (0.0f, 100.0f, 1.0f), 50.0f,
        juce::AudioParameterFloatAttributes().withLabel ("%")));

    // ── Toggles ──────────────────────────────────────────────────────────────
    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { "adaptive", 1 }, "Adaptive Mode",  true));
    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { "bypass",   1 }, "Bypass",         false));

    // ── 6-band threshold offsets  ────────────────────────────────────────────
    //  Defaults focus on the 4 kHz–12 kHz hiss range.
    constexpr float defaultOffsets[numBands] = { 0.0f, 0.0f, 5.0f, 10.0f, 12.0f, 10.0f };

    for (int i = 0; i < numBands; ++i)
    {
        auto id   = juce::String ("band") + juce::String (i + 1);
        auto name = juce::String ("Band ") + juce::String (i + 1);
        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { id, 1 }, name,
            juce::NormalisableRange<float> (-30.0f, 30.0f, 0.1f),
            defaultOffsets[i],
            juce::AudioParameterFloatAttributes().withLabel ("dB")));
    }

    return layout;
}

//==============================================================================
//  Constructor / Destructor
//==============================================================================
HisstoryAudioProcessor::HisstoryAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMETERS", createParameterLayout())
{
    pThreshold = apvts.getRawParameterValue ("threshold");
    pReduction = apvts.getRawParameterValue ("reduction");
    pSmoothing = apvts.getRawParameterValue ("smoothing");
    pAdaptive  = apvts.getRawParameterValue ("adaptive");
    pBypass    = apvts.getRawParameterValue ("bypass");

    for (int i = 0; i < numBands; ++i)
        pBand[i] = apvts.getRawParameterValue ("band" + juce::String (i + 1));
}

HisstoryAudioProcessor::~HisstoryAudioProcessor() {}

//==============================================================================
void HisstoryAudioProcessor::ChannelState::reset()
{
    inputFifo.fill (0.0f);
    outputAccum.fill (0.0f);
    inputDelayBuf.fill (0.0f);
    fifoWritePos    = 0;
    outputReadPos   = 0;
    delayWritePos   = 0;
    samplesUntilHop = hopSize;
    prevGain.fill (1.0f);
    signalLevel     = 0.0f;
}

//==============================================================================
//  Default noise profile – hiss-shaped so the plugin works before Learn
//==============================================================================
void HisstoryAudioProcessor::generateDefaultNoiseProfile()
{
    const float sr = currentSampleRate.load();

    for (int bin = 0; bin < numBins; ++bin)
    {
        float freq = static_cast<float> (bin) * sr / static_cast<float> (fftSize);

        // Model hiss as a gentle upward slope above 1 kHz (~3 dB/octave).
        // Start HIGH so the adaptive min-follower always converges DOWNWARD
        // (fast attack) to the actual noise floor within the first second.
        // Starting low would force the slow release branch and take minutes.
        float baseMag = 10.0f;

        if (freq > 1000.0f)
        {
            float octaves = std::log2 (freq / 1000.0f);
            baseMag *= std::pow (1.41f, octaves);   // +3 dB per octave
        }
        else
        {
            // Roll off gently below 1 kHz so bass is left untouched.
            float rolloff = freq / 1000.0f;
            baseMag *= std::max (rolloff, 0.1f);
        }

        noiseProfile[bin] = baseMag;
    }

    // Copy for GUI display.
    std::copy (noiseProfile.begin(), noiseProfile.end(), noiseProfileDisplay);
    noiseProfileReady.store (true);
}

//==============================================================================
//  Prepare / Release
//==============================================================================
void HisstoryAudioProcessor::prepareToPlay (double sampleRate, int /*samplesPerBlock*/)
{
    currentSampleRate.store (static_cast<float> (sampleRate));
    setLatencySamples (fftSize);

    for (auto& ch : channels)
        ch.reset();

    std::memset (inputSpectrumDB,  0, sizeof (inputSpectrumDB));
    std::memset (outputSpectrumDB, 0, sizeof (outputSpectrumDB));

    // ── Measure the actual FFT round-trip scaling on this platform ───────────
    //  JUCE's inverse FFT may or may not apply 1/N normalisation depending on
    //  the backend.  We send a unit impulse through forward+inverse and measure.
    {
        alignas(16) float probe[fftSize * 2] {};
        probe[fftSize / 2] = 1.0f;   // impulse at window centre (Hann = 1.0)
        forwardFFT.performRealOnlyForwardTransform (probe, true);
        forwardFFT.performRealOnlyInverseTransform (probe);

        const float fftRoundTrip = std::abs (probe[fftSize / 2]);

        // Quantise: JUCE's inverse FFT either normalises by 1/N (→ 1)
        // or not (→ N).  Pick the closer canonical value.
        const float safeRT =
            (fftRoundTrip > static_cast<float>(fftSize) * 0.25f)
                ? static_cast<float>(fftSize)    // unnormalised backend (IPP etc.)
                : 1.0f;                          // normalised backend (JUCE fallback)

        // For Hann² (analysis + synthesis window) with 75 % overlap the COLA
        // sum is exactly 1.5.  Full correction = 1 / (roundTrip * 1.5).
        windowCorrection = 1.0f / (safeRT * 1.5f);
    }

    // Start with a synthetic hiss-shaped profile.
    generateDefaultNoiseProfile();

    updatePerBinThreshold();
}

void HisstoryAudioProcessor::releaseResources() {}

bool HisstoryAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& mainIn  = layouts.getMainInputChannelSet();
    const auto& mainOut = layouts.getMainOutputChannelSet();

    if (mainOut != juce::AudioChannelSet::mono()
        && mainOut != juce::AudioChannelSet::stereo())
        return false;

    return mainIn == mainOut;
}

//==============================================================================
//  State save / load
//==============================================================================
void HisstoryAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void HisstoryAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml && xml->hasTagName (apvts.state.getType()))
        apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

//==============================================================================
//  Per-bin threshold curve
//==============================================================================
float HisstoryAudioProcessor::interpolateBandOffset (float freqHz) const
{
    const float logFreq = std::log2 (std::max (freqHz, 1.0f));

    std::array<float, numBands> offsets;
    for (int i = 0; i < numBands; ++i)
        offsets[i] = pBand[i]->load();

    const float logFirst = std::log2 (bandFrequencies.front());
    const float logLast  = std::log2 (bandFrequencies.back());

    if (logFreq <= logFirst) return offsets.front();
    if (logFreq >= logLast)  return offsets.back();

    for (int i = 0; i < numBands - 1; ++i)
    {
        float logLow  = std::log2 (bandFrequencies[i]);
        float logHigh = std::log2 (bandFrequencies[i + 1]);

        if (logFreq <= logHigh)
        {
            float t = (logFreq - logLow) / (logHigh - logLow);
            return offsets[i] + t * (offsets[i + 1] - offsets[i]);
        }
    }

    return 0.0f;
}

void HisstoryAudioProcessor::updatePerBinThreshold()
{
    const float sr = currentSampleRate.load();
    const float globalThrDB = pThreshold->load();

    for (int bin = 0; bin < numBins; ++bin)
    {
        float freq    = static_cast<float> (bin) * sr / static_cast<float> (fftSize);
        float bandOff = interpolateBandOffset (freq);
        float totalDB = globalThrDB + bandOff;
        perBinThreshold[bin] = juce::Decibels::decibelsToGain (totalDB);
    }
}

//==============================================================================
//  processBlock
//==============================================================================
void HisstoryAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                            juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    if (pBypass->load() > 0.5f)
        return;

    updatePerBinThreshold();

    // ── Process each channel ─────────────────────────────────────────────────
    const int numCh      = std::min (buffer.getNumChannels(), 2);
    const int numSamples = buffer.getNumSamples();

    for (int ch = 0; ch < numCh; ++ch)
    {
        auto* data  = buffer.getWritePointer (ch);
        auto& state = channels[ch];

        for (int i = 0; i < numSamples; ++i)
        {
            const float inputSample = data[i];

            // ── Input delay line (matches reported latency) for clamping ─────
            const float delayedInput = state.inputDelayBuf[state.delayWritePos];
            state.inputDelayBuf[state.delayWritePos] = inputSample;
            state.delayWritePos = (state.delayWritePos + 1) % fftSize;

            // ── Feed STFT ────────────────────────────────────────────────────
            state.inputFifo[state.fifoWritePos] = inputSample;
            state.fifoWritePos = (state.fifoWritePos + 1) % fftSize;

            float output = state.outputAccum[state.outputReadPos];
            state.outputAccum[state.outputReadPos] = 0.0f;
            state.outputReadPos = (state.outputReadPos + 1) % (fftSize * 2);

            if (--state.samplesUntilHop == 0)
            {
                state.samplesUntilHop = hopSize;
                processSTFTFrame (state, ch == 0, numCh);
            }

            // ── Hard safety: catch catastrophic gain only ─────────────────
            //  With correct normalisation + spectral gains ≤ 1.0 this
            //  should never trigger.  The 4× margin avoids interfering
            //  with normal spectral-gating, while still protecting
            //  against gross FFT-scaling errors on exotic backends.
            const float absOut = std::abs (output);
            const float absIn  = std::abs (delayedInput);

            if (absOut > absIn * 4.0f)
            {
                if (absIn > 1e-8f)
                    output *= absIn / absOut;
                else
                    output = 0.0f;
            }

            data[i] = output;
        }
    }
}

//==============================================================================
//  processSTFTFrame
//==============================================================================
void HisstoryAudioProcessor::processSTFTFrame (ChannelState& ch,
                                                bool updateSharedData,
                                                int numActiveChannels)
{
    alignas(16) float fftData[fftSize * 2] {};
    for (int i = 0; i < fftSize; ++i)
        fftData[i] = ch.inputFifo[(ch.fifoWritePos + i) % fftSize];

    hannWindow.multiplyWithWindowingTable (fftData, static_cast<size_t> (fftSize));
    forwardFFT.performRealOnlyForwardTransform (fftData, true);

    processSpectrum (fftData, ch, updateSharedData, numActiveChannels);

    forwardFFT.performRealOnlyInverseTransform (fftData);
    hannWindow.multiplyWithWindowingTable (fftData, static_cast<size_t> (fftSize));

    for (int i = 0; i < fftSize; ++i)
    {
        int pos = (ch.outputReadPos + i) % (fftSize * 2);
        ch.outputAccum[pos] += fftData[i] * windowCorrection;
    }
}

//==============================================================================
//  processSpectrum – core spectral-gating loop
//==============================================================================
void HisstoryAudioProcessor::processSpectrum (float* fftData,
                                               ChannelState& ch,
                                               bool updateSharedData,
                                               int numActiveChannels)
{
    // ── Parameters ───────────────────────────────────────────────────────────
    const float reductionDB   = pReduction->load();
    const float smoothPct     = pSmoothing->load() / 100.0f;
    const bool  isAdaptive    = pAdaptive->load() > 0.5f;

    const float spectralFloor = juce::Decibels::decibelsToGain (-80.0f);

    // Oversubtraction factor: higher = more aggressive noise removal.
    // Scales with the reduction knob (0-40 dB mapped to alpha 2.0-8.0).
    const float alpha = 2.0f + (reductionDB / 40.0f) * 6.0f;

    // Frequency-dependent noise bias: the min-follower tracks the minimum
    // of Rayleigh-distributed FFT magnitudes.  We use a lower bias in the
    // mid range (where signal lives) to preserve clarity, and a higher
    // bias in the HF range (where hiss lives) for stronger noise removal.
    const float sr   = currentSampleRate.load();
    const float binHz = sr / static_cast<float> (fftSize);

    // ── Compute magnitudes and update noise tracker ────────────────────────
    std::array<float, numBins> mags;
    std::array<float, numBins> magsSq;

    for (int bin = 0; bin < numBins; ++bin)
    {
        const float re = fftData[2 * bin];
        const float im = fftData[2 * bin + 1];
        magsSq[bin] = re * re + im * im;
        mags[bin]   = std::sqrt (magsSq[bin]);

        if (updateSharedData)
            inputSpectrumDB[bin] = juce::Decibels::gainToDecibels (mags[bin], -150.0f);

        // Adaptive noise floor tracker (min-follower)
        // Runs on ALL channels for a more stable noise estimate.
        if (isAdaptive)
        {
            if (mags[bin] < noiseProfile[bin])
            {
                // Scale attack by 1/numChannels so each channel
                // contributes proportionally to the estimate
                const float floorAttack = 0.06f / numActiveChannels;
                noiseProfile[bin] += floorAttack * (mags[bin] - noiseProfile[bin]);
            }
            else
            {
                noiseProfile[bin] *= (1.0f + 0.001f / numActiveChannels);
            }
            if (updateSharedData)
                noiseProfileDisplay[bin] = noiseProfile[bin];
        }
    }

    // ── Tonal peak detection (protect strong harmonics from over-gating) ──
    //  Only protect bins that are prominent tonal peaks (at least 10 dB above
    //  their neighbors).  This prevents artifacts on strong harmonics while
    //  still allowing broadband noise gating everywhere else.
    std::array<bool, numBins> isTonalPeak {};
    for (int bin = 3; bin < numBins - 3; ++bin)
    {
        float neighborAvg = (magsSq[bin - 2] + magsSq[bin - 1]
                           + magsSq[bin + 1] + magsSq[bin + 2]) * 0.25f;
        // 10 dB above neighbors in power = 10x power ratio
        if (magsSq[bin] > neighborAvg * 10.0f)
            isTonalPeak[bin] = true;
    }

    // ── Per-bin gain computation (Wiener-style spectral subtraction) ──────
    std::array<float, numBins> gains;

    for (int bin = 0; bin < numBins; ++bin)
    {
        const float freq = static_cast<float> (bin) * binHz;

        // Frequency-dependent noise bias:
        //   Below 2 kHz: bias = 1.2 (conservative, preserve signal)
        //   2-4 kHz: ramp from 1.2 to 2.5
        //   Above 4 kHz: bias = 2.5 (aggressive, target hiss)
        float noiseBias;
        if (freq < 2000.0f)
            noiseBias = 1.2f;
        else if (freq < 4000.0f)
            noiseBias = 1.2f + 1.3f * ((freq - 2000.0f) / 2000.0f);
        else
            noiseBias = 2.5f;

        const float noiseEst   = noiseProfile[bin];
        const float thrMult    = perBinThreshold[bin];
        const float noiseLevel = noiseEst * thrMult * noiseBias;

        float gain = 1.0f;

        if (magsSq[bin] > 1e-20f)
        {
            const float noiseSq = noiseLevel * noiseLevel;
            // Use lower alpha for tonal peaks to preserve harmonics
            const float binAlpha = isTonalPeak[bin] ? (alpha * 0.3f) : alpha;
            const float subtracted = 1.0f - binAlpha * (noiseSq / magsSq[bin]);
            gain = std::sqrt (std::max (0.0f, subtracted));
        }

        gain = juce::jlimit (spectralFloor, 1.0f, gain);
        gains[bin] = gain;
    }

    // ── Frequency smoothing (3-tap) ──────────────────────────────────────────
    {
        std::array<float, numBins> s;
        s[0] = 0.667f * gains[0] + 0.333f * gains[1];

        for (int b = 1; b < numBins - 1; ++b)
            s[b] = 0.25f * gains[b - 1]
                 + 0.50f * gains[b]
                 + 0.25f * gains[b + 1];

        s[numBins - 1] = 0.333f * gains[numBins - 2] + 0.667f * gains[numBins - 1];
        gains = s;
    }

    // ── Wider frequency smoothing (5-tap, music-safe) ────────────────────────
    {
        std::array<float, numBins> s;
        s[0] = gains[0];
        s[1] = 0.25f * gains[0] + 0.50f * gains[1] + 0.25f * gains[2];

        for (int b = 2; b < numBins - 2; ++b)
            s[b] = 0.1f  * gains[b - 2]
                 + 0.2f  * gains[b - 1]
                 + 0.4f  * gains[b]
                 + 0.2f  * gains[b + 1]
                 + 0.1f  * gains[b + 2];

        s[numBins - 2] = 0.25f * gains[numBins - 3]
                        + 0.50f * gains[numBins - 2]
                        + 0.25f * gains[numBins - 1];
        s[numBins - 1] = gains[numBins - 1];
        gains = s;
    }

    // ── Asymmetric temporal smoothing & apply gains ─────────────────────────
    //  Fast attack (let signal through quickly when gain increases) +
    //  slow release (smooth noise removal to suppress musical noise).
    //  The smoothing parameter controls the release time.
    for (int bin = 0; bin < numBins; ++bin)
    {
        const float prev = ch.prevGain[bin];
        float g;

        if (gains[bin] > prev)
        {
            // Attack: gain is increasing (signal onset) – react quickly
            constexpr float attackCoeff = 0.15f;
            g = prev + attackCoeff * (gains[bin] - prev);
        }
        else
        {
            // Release: gain is decreasing (signal offset) – smooth slowly
            // to avoid musical noise artifacts during decays
            const float releaseCoeff = smoothPct;
            g = prev + (1.0f - releaseCoeff) * (gains[bin] - prev);
        }

        g = juce::jlimit (spectralFloor, 1.0f, g);
        ch.prevGain[bin] = g;

        // Apply gain to non-negative frequency bins only.
        // The inverse FFT handles conjugate symmetry internally.
        fftData[2 * bin]     *= g;
        fftData[2 * bin + 1] *= g;

        if (updateSharedData)
        {
            float outMag = std::sqrt (fftData[2 * bin] * fftData[2 * bin]
                                    + fftData[2 * bin + 1] * fftData[2 * bin + 1]);
            outputSpectrumDB[bin] = juce::Decibels::gainToDecibels (outMag, -150.0f);
        }
    }
}

//==============================================================================
juce::AudioProcessorEditor* HisstoryAudioProcessor::createEditor()
{
    return new HisstoryAudioProcessorEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new HisstoryAudioProcessor();
}
