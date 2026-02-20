/*
  ==============================================================================
    Hisstory – PluginProcessor.h

    Real-time spectral-gating de-hiss plugin focused on the 4 kHz–12 kHz range.
    Uses an STFT overlap-add framework (Hann window, 75 % overlap) with:
      • Learned or adaptive noise profile (+ default hiss-shaped fallback)
      • Per-bin threshold derived from 6 user-draggable band control-points
      • Soft-knee spectral gate with wide frequency smoothing (music-safe)
      • Temporal + frequency smoothing to suppress musical-noise artefacts
  ==============================================================================
*/

#pragma once
#include <JuceHeader.h>
#include <array>
#include <atomic>
#include <cmath>

//==============================================================================
class HisstoryAudioProcessor : public juce::AudioProcessor
{
public:
    //==========================================================================
    HisstoryAudioProcessor();
    ~HisstoryAudioProcessor() override;

    //── AudioProcessor overrides ──────────────────────────────────────────────
    void   prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void   releaseResources() override;
    bool   isBusesLayoutSupported (const BusesLayout&) const override;
    void   processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool   hasEditor() const override { return true; }
    const  juce::String getName() const override { return JucePlugin_Name; }
    bool   acceptsMidi()  const override { return false; }
    bool   producesMidi() const override { return false; }
    bool   isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int    getNumPrograms()    override { return 1; }
    int    getCurrentProgram() override { return 0; }
    void   setCurrentProgram (int) override {}
    const  juce::String getProgramName (int) override { return {}; }
    void   changeProgramName (int, const juce::String&) override {}

    void   getStateInformation (juce::MemoryBlock& destData) override;
    void   setStateInformation (const void* data, int sizeInBytes) override;

    //==========================================================================
    //  DSP constants
    //==========================================================================
    static constexpr int fftOrder  = 12;
    static constexpr int fftSize   = 1 << fftOrder;         // 4096
    static constexpr int hopSize   = fftSize / 4;            // 1024 (75 % overlap)
    static constexpr int numBins   = fftSize / 2 + 1;        // 2049
    static constexpr int numBands  = 6;

    /** Fixed centre-frequencies for the 6 threshold-curve control-points.
        Focused on the hiss range (4 kHz–12 kHz). */
    static constexpr std::array<float, numBands> bandFrequencies
        { { 500.0f, 1500.0f, 3000.0f, 5000.0f, 8000.0f, 12000.0f } };

    //==========================================================================
    //  Data shared with the Editor (lock-free)
    //==========================================================================
    float inputSpectrumDB  [numBins] {};
    float outputSpectrumDB [numBins] {};

    float noiseProfileDisplay [numBins] {};
    std::atomic<bool> noiseProfileReady { false };

    std::atomic<float> currentSampleRate { 44100.0f };

    /** Noise Purity metric: fraction (0–1) of removed energy that came from
        stationary (noise-like) bins.  1.0 = all removed content was noise,
        0.0 = all removed content was music. */
    std::atomic<float> metricNoisePurity { 0.0f };

    /** Harmonic Loss: fraction (0–1) of tonal energy removed by the de-hisser.
        0.0 = no loss (perfect preservation); higher = more loss. */
    std::atomic<float> metricHarmonicLossRatio { 0.0f };

    /** Residual Spectral Flux: normalised frame-to-frame change of the
        residual (removed) spectrum.  Low = noise-like (good), high = musical (bad). */
    std::atomic<float> metricResidualFlux { 0.0f };

    /** STFT normalisation factor – public so the test harness can inspect it. */
    float windowCorrection = 2.0f / 3.0f;

    /** Interpolate the band-offset curve at an arbitrary frequency (Hz).
        Public so the editor can draw the threshold curve. */
    float interpolateBandOffset (float freqHz) const;

    /** In adaptive mode, band offsets are boosted by this amount so that the
        default -30 dB offsets become neutral (0 dB effective). */
    static constexpr float adaptiveBandBoost = 20.0f;

    //==========================================================================
    //  Parameter tree
    //==========================================================================
    juce::AudioProcessorValueTreeState apvts;
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

private:
    //==========================================================================
    //  FFT engine & window
    //==========================================================================
    juce::dsp::FFT                         forwardFFT { fftOrder };
    juce::dsp::WindowingFunction<float>    hannWindow
        { static_cast<size_t>(fftSize),
          juce::dsp::WindowingFunction<float>::hann,
          false  /* normalise = false: standard Hann (peak = 1.0, COLA = 1.5 for Hann²) */ };

    //==========================================================================
    //  Per-channel STFT state
    //==========================================================================
    struct ChannelState
    {
        std::array<float, fftSize>      inputFifo {};
        std::array<float, fftSize * 2>  outputAccum {};
        std::array<float, fftSize>      inputDelayBuf {};  // delay line for output clamping
        int   fifoWritePos    = 0;
        int   outputReadPos   = 0;
        int   delayWritePos   = 0;
        int   samplesUntilHop = hopSize;
        std::array<float, numBins>      prevGain {};
        float signalLevel     = 0.0f;   // smoothed frame-level (dB) for quiet detection

        void reset();
    };

    std::array<ChannelState, 2> channels;

    //==========================================================================
    //  Noise profile
    //==========================================================================
    std::array<float, numBins>  noiseProfile {};

    //==========================================================================
    //  Stationarity tracking (for Noise Purity metric)
    //  Running exponential averages of magnitude and magnitude² per bin.
    //  The coefficient of variation (stddev / mean) indicates how stationary
    //  a bin is: low CV = noise-like (stationary), high CV = music-like.
    //==========================================================================
    std::array<float, numBins>  runningMean   {};
    std::array<float, numBins>  runningMeanSq {};
    float smoothedNoisePurity = 0.5f;
    float smoothedHLR         = 0.0f;
    float smoothedResFlux     = 0.0f;
    std::array<float, numBins>  prevResidualMag {};

    /** Generate a synthetic hiss-shaped default profile so the plugin
        works immediately (before the user presses Learn). */
    void generateDefaultNoiseProfile();

    /** Reset the noise profile to near-zero for adaptive convergence
        from the bottom (no removal initially). */
    void resetAdaptiveProfile();

    //==========================================================================
    //  Bypass crossfade state
    //==========================================================================
    bool previousBypassState          = false;
    int  bypassFadeSamplesRemaining   = 0;
    static constexpr int bypassFadeLength = 64;

    //==========================================================================
    //  New-track / silence detection
    //==========================================================================
    int  silenceSampleCount = 0;
    bool wasInSilence       = false;
    bool lastAdaptiveState  = true;

    //==========================================================================
    //  Pre-computed per-bin threshold multiplier
    //==========================================================================
    std::array<float, numBins> perBinThreshold {};

    //==========================================================================
    //  Internal helpers
    //==========================================================================
    void  processSTFTFrame   (ChannelState& ch, bool updateSharedData, int numActiveChannels = 1, bool bypassed = false);
    void  processSpectrum    (float* fftData, ChannelState& ch, bool updateSharedData, int numActiveChannels = 1, bool bypassed = false);
    void  updatePerBinThreshold();

    //==========================================================================
    //  Cached raw-parameter pointers
    //==========================================================================
    std::atomic<float>* pThreshold  = nullptr;
    std::atomic<float>* pReduction  = nullptr;
    std::atomic<float>* pSmoothing  = nullptr;
    std::atomic<float>* pAdaptive   = nullptr;
    std::atomic<float>* pBypass     = nullptr;
    std::array<std::atomic<float>*, numBands> pBand {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HisstoryAudioProcessor)
};
