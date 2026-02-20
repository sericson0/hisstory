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
        juce::NormalisableRange<float> (-40.0f, -10.0f, 0.1f), -23.0f,
        juce::AudioParameterFloatAttributes().withLabel ("dB")));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "reduction", 1 }, "Reduction",
        juce::NormalisableRange<float> (0.0f, 32.0f, 0.1f), 12.0f,
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
    //  Defaults start at minimum (no removal).  In adaptive mode, the
    //  adaptiveBandBoost constant shifts these to effective 0 → 10 dB.
    //  In non-adaptive mode, all points start at the bottom of the display.
    constexpr float defaultOffsets[numBands] = { -20.0f, -20.0f, -15.0f, -10.0f, -8.0f, -10.0f };

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
        // Use a low base magnitude so the threshold curve starts near the
        // bottom of the display in non-adaptive mode (user drags up to gate).
        float baseMag = 0.5f;

        if (freq > 1000.0f)
        {
            float octaves = std::log2 (freq / 1000.0f);
            baseMag *= std::pow (1.41f, octaves);   // +3 dB per octave
        }
        else
        {
            float rolloff = freq / 1000.0f;
            baseMag *= std::max (rolloff, 0.1f);
        }

        noiseProfile[bin] = baseMag;
    }

    std::copy (noiseProfile.begin(), noiseProfile.end(), noiseProfileDisplay);
    noiseProfileReady.store (true);
}

//==============================================================================
//  Reset adaptive profile – start from near-zero (no removal)
//==============================================================================
void HisstoryAudioProcessor::resetAdaptiveProfile()
{
    for (int bin = 0; bin < numBins; ++bin)
        noiseProfile[bin] = 1e-7f;

    std::copy (noiseProfile.begin(), noiseProfile.end(), noiseProfileDisplay);
    noiseProfileReady.store (true);

    runningMean.fill (0.0f);
    runningMeanSq.fill (0.0f);
    smoothedNoisePurity = 0.5f;
    prevResidualMag.fill (0.0f);

    for (auto& ch : channels)
        ch.prevGain.fill (1.0f);
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

    previousBypassState        = false;
    bypassFadeSamplesRemaining = 0;

    std::memset (inputSpectrumDB,  0, sizeof (inputSpectrumDB));
    std::memset (outputSpectrumDB, 0, sizeof (outputSpectrumDB));

    runningMean.fill (0.0f);
    runningMeanSq.fill (0.0f);
    smoothedNoisePurity = 0.5f;
    smoothedHLR         = 1.0f;
    smoothedResFlux     = 0.0f;
    prevResidualMag.fill (0.0f);

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

    // If adaptive mode is active, start from near-zero so the plugin
    // begins without removing any sound, then converges upward.
    lastAdaptiveState = pAdaptive->load() > 0.5f;
    if (lastAdaptiveState)
        resetAdaptiveProfile();

    silenceSampleCount = 0;
    wasInSilence = false;

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
    const float sr          = currentSampleRate.load();
    const float globalThrDB = pThreshold->load();
    const bool  isAdaptive  = pAdaptive->load() > 0.5f;

    for (int bin = 0; bin < numBins; ++bin)
    {
        float freq    = static_cast<float> (bin) * sr / static_cast<float> (fftSize);
        float bandOff = interpolateBandOffset (freq);

        // In adaptive mode, shift band offsets upward so the default
        // low values still provide effective gating once the profile
        // has converged.
        if (isAdaptive)
            bandOff += adaptiveBandBoost;

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

    const bool bypassed = pBypass->load() > 0.5f;

    if (bypassed != previousBypassState)
    {
        bypassFadeSamplesRemaining = bypassFadeLength;
        previousBypassState = bypassed;
    }

    const bool currentAdaptive = pAdaptive->load() > 0.5f;

    // ── Detect adaptive mode transitions ─────────────────────────────────────
    if (currentAdaptive && ! lastAdaptiveState)
    {
        // Switched to adaptive: start profile from zero (no removal)
        resetAdaptiveProfile();
    }
    else if (! currentAdaptive && lastAdaptiveState)
    {
        // Switched to non-adaptive: reset to synthetic hiss profile
        generateDefaultNoiseProfile();
    }
    lastAdaptiveState = currentAdaptive;

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
                processSTFTFrame (state, ch == 0, numCh, bypassed);
            }

            // ── Safety clamp (always, so wet is valid during crossfade) ──
            {
                const float absOut = std::abs (output);
                const float absIn  = std::abs (delayedInput);

                if (absOut > absIn * 4.0f)
                {
                    if (absIn > 1e-8f)
                        output *= absIn / absOut;
                    else
                        output = 0.0f;
                }
            }

            const float dry = inputSample;
            const float wet = output;

            if (bypassFadeSamplesRemaining > 0)
            {
                const float t = static_cast<float> (bypassFadeSamplesRemaining)
                              / static_cast<float> (bypassFadeLength);

                if (bypassed)
                    data[i] = wet * t + dry * (1.0f - t);
                else
                    data[i] = dry * t + wet * (1.0f - t);

                --bypassFadeSamplesRemaining;
            }
            else
            {
                data[i] = bypassed ? dry : wet;
            }
        }
    }

    // ── New-track detection via silence gap ───────────────────────────────────
    //  When a silence gap (> 0.5 s below −60 dBFS) ends and adaptive mode is
    //  active, reset the noise profile so the plugin re-adapts to the new track.
    if (currentAdaptive)
    {
        float blockSumSq = 0.0f;
        for (int ch = 0; ch < numCh; ++ch)
        {
            const auto* rd = buffer.getReadPointer (ch);
            for (int i = 0; i < numSamples; ++i)
                blockSumSq += rd[i] * rd[i];
        }
        const float blockRMS = std::sqrt (blockSumSq / static_cast<float> (numCh * numSamples));
        const float blockDB  = 20.0f * std::log10 (blockRMS + 1e-20f);

        if (blockDB < -60.0f)
        {
            silenceSampleCount += numSamples;
            const float sr = currentSampleRate.load();
            if (static_cast<float> (silenceSampleCount) / sr > 0.5f)
                wasInSilence = true;
        }
        else
        {
            if (wasInSilence)
                resetAdaptiveProfile();

            wasInSilence       = false;
            silenceSampleCount = 0;
        }
    }
}

//==============================================================================
//  processSTFTFrame
//==============================================================================
void HisstoryAudioProcessor::processSTFTFrame (ChannelState& ch,
                                                bool updateSharedData,
                                                int numActiveChannels,
                                                bool bypassed)
{
    alignas(16) float fftData[fftSize * 2] {};
    for (int i = 0; i < fftSize; ++i)
        fftData[i] = ch.inputFifo[(ch.fifoWritePos + i) % fftSize];

    hannWindow.multiplyWithWindowingTable (fftData, static_cast<size_t> (fftSize));
    forwardFFT.performRealOnlyForwardTransform (fftData, true);

    processSpectrum (fftData, ch, updateSharedData, numActiveChannels, bypassed);

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
                                               int numActiveChannels,
                                               bool bypassed)
{
    // ── Parameters ───────────────────────────────────────────────────────────
    const float reductionDB   = pReduction->load();
    const float smoothPct     = pSmoothing->load() / 100.0f;
    const bool  isAdaptive    = pAdaptive->load() > 0.5f;

    // Spectral floor: max attenuation applied per bin.
    // -60 dB preserves a tiny residual, avoiding complete "holes" in the
    // audio that sound unnatural and cause music loss.
    const float spectralFloor = juce::Decibels::decibelsToGain (-60.0f);

    // Oversubtraction factor: 1.5–4.0 (reduced from 2.0–8.0 for less
    // aggressive removal and better music preservation).
    const float alpha = 1.5f + (reductionDB / 40.0f) * 2.5f;

    const float sr    = currentSampleRate.load();
    const float binHz = sr / static_cast<float> (fftSize);

    // ── Compute magnitudes, update noise tracker, and track stationarity ──
    std::array<float, numBins> mags;
    std::array<float, numBins> magsSq;

    // Stationarity tracking coefficient (~0.77 s time constant at 44.1 kHz).
    constexpr float statAlpha = 0.97f;

    for (int bin = 0; bin < numBins; ++bin)
    {
        const float re = fftData[2 * bin];
        const float im = fftData[2 * bin + 1];
        magsSq[bin] = re * re + im * im;
        mags[bin]   = std::sqrt (magsSq[bin]);

        if (updateSharedData)
            inputSpectrumDB[bin] = juce::Decibels::gainToDecibels (mags[bin], -150.0f);

        runningMean[bin]   = statAlpha * runningMean[bin]   + (1.0f - statAlpha) * mags[bin];
        runningMeanSq[bin] = statAlpha * runningMeanSq[bin] + (1.0f - statAlpha) * magsSq[bin];

        // ── Adaptive noise floor tracker ──────────────────────────────────
        //  Converges UPWARD from near-zero: the release branch grows the
        //  profile toward the observed signal; the attack branch pulls it
        //  down.  Equilibrium ≈ 14th percentile of the magnitude distribution
        //  (close to the noise floor for Rayleigh-distributed noise).
        //  The release is gated by stationarity so that the profile only
        //  rises in noise-like (stationary) bins, protecting the estimate
        //  from being inflated by musical content.
        if (isAdaptive)
        {
            if (mags[bin] < noiseProfile[bin])
            {
                // Fast attack: converge down toward minimum
                const float floorAttack = 0.06f / static_cast<float> (numActiveChannels);
                noiseProfile[bin] += floorAttack * (mags[bin] - noiseProfile[bin]);
            }
            else
            {
                // Stationarity-gated release: only grow in noise-like bins
                const float mean   = runningMean[bin];
                const float meanSq = runningMeanSq[bin];
                const float var    = std::max (0.0f, meanSq - mean * mean);
                const float stddev = std::sqrt (var);
                const float cv     = (mean > 1e-10f) ? (stddev / mean) : 0.0f;

                // Stationarity: 1.0 = noise-like (low CV), 0.0 = music (high CV)
                const float stationarity = 1.0f - juce::jlimit (0.0f, 1.0f,
                                                                  (cv - 0.5f) / 1.0f);

                // Faster initial convergence when profile is far from signal
                const float baseRelease =
                    (noiseProfile[bin] < mags[bin] * 0.1f) ? 0.03f : 0.01f;

                const float releaseRate = baseRelease * stationarity
                                        / static_cast<float> (numActiveChannels);

                noiseProfile[bin] += releaseRate * (mags[bin] - noiseProfile[bin]);
            }

            if (updateSharedData)
                noiseProfileDisplay[bin] = noiseProfile[bin];
        }
    }

    // ── Bypass: update display with input spectrum only, skip processing ──
    if (bypassed)
    {
        if (updateSharedData)
        {
            for (int bin = 0; bin < numBins; ++bin)
                outputSpectrumDB[bin] = inputSpectrumDB[bin];
        }
        return;
    }

    // ── Tonal peak detection (protect harmonics from over-gating) ─────────
    //  Protect bins that are at least 7 dB above neighbours (5× power),
    //  and extend protection to immediate neighbours.
    std::array<bool, numBins> isTonalPeak {};
    for (int bin = 3; bin < numBins - 3; ++bin)
    {
        float neighborAvg = (magsSq[bin - 2] + magsSq[bin - 1]
                           + magsSq[bin + 1] + magsSq[bin + 2]) * 0.25f;

        if (magsSq[bin] > neighborAvg * 5.0f)
        {
            isTonalPeak[bin] = true;
            if (bin > 0)            isTonalPeak[bin - 1] = true;
            if (bin < numBins - 1)  isTonalPeak[bin + 1] = true;
        }
    }

    // ── Pre-compute per-bin stationarity (for music-aware gating) ─────────
    std::array<float, numBins> binStationarity {};
    for (int bin = 0; bin < numBins; ++bin)
    {
        const float mean   = runningMean[bin];
        const float meanSq = runningMeanSq[bin];
        const float var    = std::max (0.0f, meanSq - mean * mean);
        const float stddev = std::sqrt (var);
        const float cv     = (mean > 1e-10f) ? (stddev / mean) : 0.0f;

        // 1.0 = noise-like, 0.0 = music-like
        binStationarity[bin] = 1.0f - juce::jlimit (0.0f, 1.0f,
                                                      (cv - 0.5f) / 1.0f);
    }

    // ── Per-bin gain computation (Wiener-style spectral subtraction) ──────
    std::array<float, numBins> gains;

    for (int bin = 0; bin < numBins; ++bin)
    {
        const float freq = static_cast<float> (bin) * binHz;

        // Frequency-dependent noise bias (reduced from 2.5 to 1.8 for HF):
        //   Below 2 kHz: 1.1 (conservative, preserve signal)
        //   2–4 kHz: ramp 1.1 → 1.8
        //   Above 4 kHz: 1.8 (target hiss, but gentler than before)
        float noiseBias;
        if (freq < 2000.0f)
            noiseBias = 1.1f;
        else if (freq < 4000.0f)
            noiseBias = 1.1f + 0.7f * ((freq - 2000.0f) / 2000.0f);
        else
            noiseBias = 1.8f;

        const float noiseEst   = noiseProfile[bin];
        const float thrMult    = perBinThreshold[bin];
        const float noiseLevel = noiseEst * thrMult * noiseBias;

        float gain = 1.0f;

        if (magsSq[bin] > 1e-20f)
        {
            const float noiseSq = noiseLevel * noiseLevel;

            // Base alpha, reduced for tonal peaks
            float binAlpha = isTonalPeak[bin] ? (alpha * 0.15f) : alpha;

            // Stationarity-aware alpha: reduce gating for music-like bins.
            // Noise bins (stationarity≈1) get full alpha.
            // Music bins (stationarity≈0) get 30% of alpha.
            const float stFactor = 0.3f + 0.7f * binStationarity[bin];
            binAlpha *= stFactor;

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
    float noiseRemovedPower = 0.0f;
    float musicRemovedPower = 0.0f;

    float inputTonalPower   = 0.0f, inputNonTonalPower   = 0.0f;
    float outputTonalPower  = 0.0f, outputNonTonalPower  = 0.0f;
    float residualFluxSum   = 0.0f, residualTotalMag     = 0.0f;

    for (int bin = 0; bin < numBins; ++bin)
    {
        const float prev = ch.prevGain[bin];
        float g;

        if (gains[bin] > prev)
        {
            constexpr float attackCoeff = 0.15f;
            g = prev + attackCoeff * (gains[bin] - prev);
        }
        else
        {
            const float releaseCoeff = smoothPct;
            g = prev + (1.0f - releaseCoeff) * (gains[bin] - prev);
        }

        g = juce::jlimit (spectralFloor, 1.0f, g);
        ch.prevGain[bin] = g;

        fftData[2 * bin]     *= g;
        fftData[2 * bin + 1] *= g;

        if (updateSharedData)
        {
            float outMag = std::sqrt (fftData[2 * bin] * fftData[2 * bin]
                                    + fftData[2 * bin + 1] * fftData[2 * bin + 1]);
            outputSpectrumDB[bin] = juce::Decibels::gainToDecibels (outMag, -150.0f);

            // ── Noise Purity: classify removed energy as noise vs music ──
            if (g < 0.999f)
            {
                const float removedPower = magsSq[bin] * (1.0f - g * g);
                const float st = binStationarity[bin];

                noiseRemovedPower += removedPower * st;
                musicRemovedPower += removedPower * (1.0f - st);
            }

            // ── Harmonic Loss Ratio accumulators ─────────────────────────
            if (isTonalPeak[bin])
            {
                inputTonalPower  += magsSq[bin];
                outputTonalPower += magsSq[bin] * g * g;
            }
            else
            {
                inputNonTonalPower  += magsSq[bin];
                outputNonTonalPower += magsSq[bin] * g * g;
            }

            // ── Residual Spectral Flux ───────────────────────────────────
            const float resMag = mags[bin] * (1.0f - g);
            residualFluxSum += std::abs (resMag - prevResidualMag[bin]);
            residualTotalMag += resMag;
            prevResidualMag[bin] = resMag;
        }
    }

    // ── Update metrics (smoothed) ────────────────────────────────────────────
    if (updateSharedData)
    {
        // Noise Purity
        const float totalRemoved = noiseRemovedPower + musicRemovedPower;
        if (totalRemoved > 1e-20f)
        {
            const float purity = noiseRemovedPower / totalRemoved;
            constexpr float puritySmooth = 0.95f;
            smoothedNoisePurity = puritySmooth * smoothedNoisePurity
                                + (1.0f - puritySmooth) * purity;
        }
        metricNoisePurity.store (smoothedNoisePurity);

        // Harmonic Loss: fraction of tonal energy removed by the de-hisser.
        // 0.0 = no tonal energy lost; 0.05 = 5% lost; higher = more loss.
        const float rawHarmLoss = (inputTonalPower > 1e-20f)
                                ? (1.0f - outputTonalPower / inputTonalPower)
                                : 0.0f;
        constexpr float hlrSmooth = 0.95f;
        smoothedHLR = hlrSmooth * smoothedHLR + (1.0f - hlrSmooth) * rawHarmLoss;
        metricHarmonicLossRatio.store (smoothedHLR);

        // Residual Spectral Flux (normalised 0–1)
        const float rawFlux = (residualTotalMag > 1e-20f)
                            ? (residualFluxSum / residualTotalMag) : 0.0f;
        constexpr float fluxSmooth = 0.95f;
        smoothedResFlux = fluxSmooth * smoothedResFlux + (1.0f - fluxSmooth) * rawFlux;
        metricResidualFlux.store (smoothedResFlux);
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
