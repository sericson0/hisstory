/*
  ==============================================================================
    Hisstory – PluginEditor.h

    Dark-themed GUI with orange accent colours:
      • Real-time spectrum display (input, output, threshold) with legend
      • 6 draggable band control-points on the threshold curve
      • Vertical sliders for Threshold and Reduction
      • Real-time quality metrics (Noise Floor, HF Reduction, Mid Preserved, Output Level)
      • Adaptive Mode toggle and Bypass button in the top bar
  ==============================================================================
*/

#pragma once
#include "PluginProcessor.h"
#include <JuceHeader.h>

//==============================================================================
//  Shared constants
//==============================================================================
namespace HisstoryConstants
{
    // FFT normalisation offset: -20 * log10(fftSize / 2)
    // For fftSize = 4096: -20 * log10(2048) ≈ -66.2
    static constexpr float fftNormDB = -66.2f;
}

//==============================================================================
//  Colour palette – dark background with orange accents
//==============================================================================
namespace HisstoryColours
{
    const juce::Colour background      { 0xff12151f };
    const juce::Colour plotBackground   { 0xff0b0e17 };
    const juce::Colour gridLine         { 0xff1e2230 };
    const juce::Colour gridText         { 0xff5a5e70 };
    const juce::Colour textNormal       { 0xffb0b4c0 };
    const juce::Colour textBright       { 0xfff0f0f0 };
    const juce::Colour inputCurve       { 0xff707580 };
    const juce::Colour outputCurve      { 0xffd8dae0 };
    const juce::Colour thresholdCurve   { 0xffF3A10F };   // golden orange
    const juce::Colour accent           { 0xffD96C30 };   // deep orange
    const juce::Colour accentBright     { 0xffF3A10F };   // golden orange
    const juce::Colour accentDim        { 0xff8B4420 };
    const juce::Colour buttonSelected   { 0xffA34210 };
    const juce::Colour inactive         { 0xff5a5e70 };
    const juce::Colour sliderTrack      { 0xff2a2e3e };
    const juce::Colour buttonBg         { 0xff1e2230 };
    const juce::Colour buttonBgHover    { 0xff282c3e };
    const juce::Colour metricGood       { 0xff4CAF50 };   // green
    const juce::Colour metricWarn       { 0xffFF9800 };   // orange
    const juce::Colour metricBad        { 0xffF44336 };   // red
}

//==============================================================================
//  Custom LookAndFeel
//==============================================================================
class HisstoryLookAndFeel : public juce::LookAndFeel_V4
{
public:
    HisstoryLookAndFeel();
    void setCompactTooltipMode (bool shouldUseCompact) { compactTooltipMode = shouldUseCompact; }

    void drawLinearSlider (juce::Graphics&, int x, int y, int w, int h,
                           float sliderPos, float minSliderPos, float maxSliderPos,
                           juce::Slider::SliderStyle, juce::Slider&) override;

    void drawToggleButton (juce::Graphics&, juce::ToggleButton&,
                           bool shouldDrawButtonAsHighlighted,
                           bool shouldDrawButtonAsDown) override;

    void drawButtonBackground (juce::Graphics&, juce::Button&,
                               const juce::Colour& backgroundColour,
                               bool shouldDrawButtonAsHighlighted,
                               bool shouldDrawButtonAsDown) override;

    void drawButtonText (juce::Graphics&, juce::TextButton&,
                         bool shouldDrawButtonAsHighlighted,
                         bool shouldDrawButtonAsDown) override;

    void drawTooltip (juce::Graphics&, const juce::String& text, int width, int height) override;

private:
    bool compactTooltipMode = false;
};

//==============================================================================
//  Spectrum display component (includes legend)
//==============================================================================
class SpectrumDisplay : public juce::Component
{
public:
    explicit SpectrumDisplay (HisstoryAudioProcessor& p);

    void paint    (juce::Graphics&) override;
    void resized  () override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp   (const juce::MouseEvent&) override;

    void updateSpectrumData();

    /** Toggle between spectrum analyser and spectrogram views. */
    void setSpectrogramMode (bool enabled);
    bool isSpectrogramMode() const { return showSpectrogram; }

    juce::TextButton spectrogramToggle { "Spectrogram" };

    juce::Rectangle<float> plotArea;

private:
    HisstoryAudioProcessor& processor;

    std::array<float, HisstoryAudioProcessor::numBins> dispInput  {};
    std::array<float, HisstoryAudioProcessor::numBins> dispOutput {};

    int draggingBand = -1;

    float freqToX (float hz) const;
    float dbToY   (float db) const;
    float xToFreq (float x)  const;
    float yToDb   (float y)  const;

    void drawGrid           (juce::Graphics&);
    void drawLegend         (juce::Graphics&);
    void drawSpectrumCurve  (juce::Graphics&, const std::array<float,
                             HisstoryAudioProcessor::numBins>& data,
                             juce::Colour colour, float thickness);
    void drawThresholdCurve (juce::Graphics&);
    void drawBandPoints     (juce::Graphics&);

    static constexpr float analyzerMinFreq  = 20.0f;
    static constexpr float analyzerMaxFreq  = 22000.0f;
    static constexpr float analyzerMinDB    = -100.0f;
    static constexpr float analyzerMaxDB    = -30.0f;
    static constexpr float spectrogramMinFreq = 100.0f;
    static constexpr float spectrogramMaxFreq = 22000.0f;
    static constexpr float spectrogramMinDB   = -100.0f;
    static constexpr float spectrogramMaxDB   = -20.0f;

    // ── Spectrogram ──────────────────────────────────────────────────────────
    bool showSpectrogram = false;

    static constexpr int numMelBins   = 256;
    static constexpr int numTimeCols  = 1024;

    // Mel filterbank: for each Mel band, store (startBin, endBin) and weights
    struct MelFilter { int startBin; int endBin; std::vector<float> weights; };
    std::vector<MelFilter> melFilters;
    void buildMelFilterbank();

    // Circular buffer of Mel spectra (in dB)
    std::array<std::array<float, numMelBins>, numTimeCols> spectrogramBuf {};
    int spectrogramWritePos = 0;

    // Back-buffer image for efficient rendering
    juce::Image spectrogramImage;

    void updateSpectrogramColumn();
    void drawSpectrogram     (juce::Graphics&);
    void drawMelGrid         (juce::Graphics&);

    static float hzToMel (float hz)  { return 2595.0f * std::log10 (1.0f + hz / 700.0f); }
    static float melToHz (float mel) { return 700.0f * (std::pow (10.0f, mel / 2595.0f) - 1.0f); }

    juce::Colour dbToColour (float db) const;
    float melToY (float mel) const;
    float yToMel (float y)   const;
};

//==============================================================================
//  Main editor
//==============================================================================
class HisstoryAudioProcessorEditor : public juce::AudioProcessorEditor,
                                      private juce::Timer
{
public:
    explicit HisstoryAudioProcessorEditor (HisstoryAudioProcessor&);
    ~HisstoryAudioProcessorEditor() override;

    void paint   (juce::Graphics&) override;
    void resized () override;

private:
    void timerCallback() override;
    void updateMetrics();
    void updateBypassVisualState (bool bypassed);

    HisstoryAudioProcessor& processor;
    HisstoryLookAndFeel     lnf;
    juce::TooltipWindow     tooltipWindow { this, 500 };

    SpectrumDisplay spectrumDisplay;

    juce::TextButton adaptiveButton      { "Adaptive" };
    juce::TextButton bypassButton        { "Bypass" };
    juce::TextButton collapseButton      { "<<" };

    bool collapsed = false;

    juce::Slider thresholdSlider, reductionSlider;
    juce::Label  thresholdLabel, reductionLabel;

    // ── Metrics ──────────────────────────────────────────────────────────────
    juce::Label metricsHeader;
    juce::TextButton helpButton { "?" };

    juce::Label metricHfRemovedName,    metricHfRemovedVal;
    juce::Label metricMidKeptName,      metricMidKeptVal;
    juce::Label metricOutputName,       metricOutputVal;
    juce::Label metricHLRName,          metricHLRVal;

    juce::Image brandLogoImage;
    juce::Rectangle<int> brandLogoBounds;
    juce::Rectangle<int> compactFooterBounds;

    float smoothHfRemoved    = 0.0f;
    float smoothMidKept      = 0.0f;
    float smoothOutput       = 0.0f;
    float smoothHLR          = 0.0f;
    bool bypassVisualState   = false;

    using SliderAttach = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttach = juce::AudioProcessorValueTreeState::ButtonAttachment;

    std::unique_ptr<SliderAttach> thresholdAttach, reductionAttach;
    std::unique_ptr<ButtonAttach> adaptiveAttach, bypassAttach;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HisstoryAudioProcessorEditor)
};
