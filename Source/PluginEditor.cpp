/*
  ==============================================================================
    Hisstory – PluginEditor.cpp

    Dark-themed GUI with orange accent colours.
  ==============================================================================
*/

#include "PluginEditor.h"

using namespace HisstoryColours;

//==============================================================================
//  HisstoryLookAndFeel
//==============================================================================
HisstoryLookAndFeel::HisstoryLookAndFeel()
{
    setColour (juce::ResizableWindow::backgroundColourId, background);
    setColour (juce::Label::textColourId, textNormal);
    setColour (juce::Slider::textBoxTextColourId, textBright);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::TextButton::buttonColourId, buttonBg);
    setColour (juce::TextButton::textColourOffId, textNormal);
    setColour (juce::TextButton::textColourOnId, textBright);
}

void HisstoryLookAndFeel::drawLinearSlider (juce::Graphics& g,
                                             int x, int y, int w, int h,
                                             float sliderPos,
                                             float /*minSliderPos*/,
                                             float /*maxSliderPos*/,
                                             juce::Slider::SliderStyle style,
                                             juce::Slider& slider)
{
    if (style != juce::Slider::LinearVertical)
    {
        LookAndFeel_V4::drawLinearSlider (g, x, y, w, h, sliderPos, 0, 0, style, slider);
        return;
    }

    const float cx     = static_cast<float> (x) + static_cast<float> (w) * 0.5f;
    const float top    = static_cast<float> (y) + 6.0f;
    const float bottom = static_cast<float> (y + h) - 6.0f;

    // Track background.
    g.setColour (sliderTrack);
    g.drawLine (cx, top, cx, bottom, 3.0f);

    // Filled portion (below thumb).
    g.setColour (accentBright);
    g.drawLine (cx, sliderPos, cx, bottom, 3.0f);

    // Thumb.
    const float thumbR = 7.0f;
    g.setColour (textBright);
    g.fillEllipse (cx - thumbR, sliderPos - thumbR, thumbR * 2.0f, thumbR * 2.0f);
    g.setColour (accent);
    g.drawEllipse (cx - thumbR, sliderPos - thumbR, thumbR * 2.0f, thumbR * 2.0f, 1.5f);
}

void HisstoryLookAndFeel::drawToggleButton (juce::Graphics& g,
                                              juce::ToggleButton& button,
                                              bool /*highlighted*/,
                                              bool /*down*/)
{
    const auto bounds = button.getLocalBounds().toFloat();
    const bool on = button.getToggleState();

    const float circleSize = 14.0f;
    const float cy = bounds.getCentreY();
    const float cx = bounds.getX() + circleSize * 0.5f + 2.0f;

    g.setColour (on ? accent : inactive);
    g.fillEllipse (cx - circleSize * 0.5f, cy - circleSize * 0.5f,
                   circleSize, circleSize);

    if (on)
    {
        g.setColour (background);
        g.fillEllipse (cx - 3.0f, cy - 3.0f, 6.0f, 6.0f);
    }

    g.setColour (on ? textBright : textNormal);
    g.setFont (14.0f);
    g.drawText (button.getButtonText(),
                juce::Rectangle<float> (cx + circleSize * 0.5f + 4.0f,
                                         bounds.getY(),
                                         bounds.getWidth() - circleSize - 8.0f,
                                         bounds.getHeight()),
                juce::Justification::centredLeft);
}

void HisstoryLookAndFeel::drawButtonBackground (juce::Graphics& g,
                                                  juce::Button& button,
                                                  const juce::Colour&,
                                                  bool highlighted,
                                                  bool down)
{
    auto bounds = button.getLocalBounds().toFloat().reduced (1.0f);
    bool on = button.getToggleState();

    juce::Colour bg = on ? accent : (highlighted ? buttonBgHover : buttonBg);
    if (down) bg = bg.brighter (0.1f);

    g.setColour (bg);
    g.fillRoundedRectangle (bounds, 5.0f);

    g.setColour (on ? accent.brighter (0.3f) : gridLine);
    g.drawRoundedRectangle (bounds, 5.0f, 1.0f);
}

void HisstoryLookAndFeel::drawButtonText (juce::Graphics& g,
                                            juce::TextButton& button,
                                            bool /*highlighted*/, bool /*down*/)
{
    g.setColour (button.getToggleState() ? textBright : textNormal);
    g.setFont (14.0f);
    g.drawText (button.getButtonText(), button.getLocalBounds(),
                juce::Justification::centred);
}

//==============================================================================
//  SpectrumDisplay
//==============================================================================
SpectrumDisplay::SpectrumDisplay (HisstoryAudioProcessor& p) : processor (p)
{
    dispInput.fill  (-100.0f);
    dispOutput.fill (-100.0f);
}

void SpectrumDisplay::resized()
{
    // Leave room for axis labels INSIDE the component.
    plotArea = getLocalBounds().toFloat()
                  .withTrimmedLeft   (8.0f)
                  .withTrimmedBottom (22.0f)   // frequency labels
                  .withTrimmedTop    (4.0f)
                  .withTrimmedRight  (44.0f);  // dB labels
}

// ── Coordinate mapping ──────────────────────────────────────────────────────

float SpectrumDisplay::freqToX (float hz) const
{
    float logMin = std::log10 (minFreq);
    float logMax = std::log10 (maxFreq);
    float logF   = std::log10 (std::max (hz, minFreq));
    return plotArea.getX()
           + (logF - logMin) / (logMax - logMin) * plotArea.getWidth();
}

float SpectrumDisplay::dbToY (float db) const
{
    float norm = (db - minDB) / (maxDB - minDB);
    return plotArea.getBottom() - norm * plotArea.getHeight();
}

float SpectrumDisplay::xToFreq (float x) const
{
    float logMin = std::log10 (minFreq);
    float logMax = std::log10 (maxFreq);
    float t = (x - plotArea.getX()) / plotArea.getWidth();
    return std::pow (10.0f, logMin + t * (logMax - logMin));
}

float SpectrumDisplay::yToDb (float y) const
{
    float norm = (plotArea.getBottom() - y) / plotArea.getHeight();
    return minDB + norm * (maxDB - minDB);
}

// ── Spectrum data refresh ───────────────────────────────────────────────────

void SpectrumDisplay::updateSpectrumData()
{
    constexpr float decay = 0.75f;
    for (int i = 0; i < HisstoryAudioProcessor::numBins; ++i)
    {
        // Raw FFT dB → approximate dBFS by adding normalisation offset.
        float inFS  = processor.inputSpectrumDB[i]  + fftNormDB;
        float outFS = processor.outputSpectrumDB[i] + fftNormDB;

        dispInput[i]  = decay * dispInput[i]  + (1.0f - decay) * inFS;
        dispOutput[i] = decay * dispOutput[i] + (1.0f - decay) * outFS;
    }
}

// ── Paint ───────────────────────────────────────────────────────────────────

void SpectrumDisplay::paint (juce::Graphics& g)
{
    g.setColour (plotBackground);
    g.fillRoundedRectangle (getLocalBounds().toFloat(), 4.0f);

    if (plotArea.isEmpty()) return;

    drawGrid (g);
    drawSpectrumCurve (g, dispInput,  inputCurve.withAlpha (0.5f), 1.0f);
    drawSpectrumCurve (g, dispOutput, outputCurve, 1.5f);
    drawThresholdCurve (g);
    drawBandPoints (g);
}

// ── Grid ────────────────────────────────────────────────────────────────────

void SpectrumDisplay::drawGrid (juce::Graphics& g)
{
    g.setFont (11.0f);

    // Frequency lines.
    const float freqLines[] = { 50, 100, 200, 500, 1000, 2000, 5000, 10000 };
    const char* freqLabels[] = { "50", "100", "200", "500", "1k", "2k", "5k", "10k" };

    for (int i = 0; i < 8; ++i)
    {
        float x = freqToX (freqLines[i]);
        g.setColour (gridLine);
        g.drawVerticalLine (static_cast<int> (x),
                            plotArea.getY(), plotArea.getBottom());
        g.setColour (gridText);
        g.drawText (freqLabels[i],
                    juce::Rectangle<float> (x - 18.0f,
                                             plotArea.getBottom() + 3.0f,
                                             36.0f, 16.0f),
                    juce::Justification::centred);
    }

    // "Hz" label.
    g.setColour (gridText);
    g.drawText ("Hz",
                juce::Rectangle<float> (plotArea.getRight() - 16.0f,
                                         plotArea.getBottom() + 3.0f,
                                         24.0f, 16.0f),
                juce::Justification::centred);

    // dB lines – labels drawn in the right margin (inside the component).
    for (float db = 0.0f; db >= -90.0f; db -= 10.0f)
    {
        float y = dbToY (db);
        g.setColour (gridLine);
        g.drawHorizontalLine (static_cast<int> (y),
                              plotArea.getX(), plotArea.getRight());
        g.setColour (gridText);
        g.drawText (juce::String (static_cast<int> (db)),
                    juce::Rectangle<float> (plotArea.getRight() + 4.0f,
                                             y - 7.0f, 36.0f, 14.0f),
                    juce::Justification::centredLeft);
    }

    // "dB" label at top-right.
    g.drawText ("dB",
                juce::Rectangle<float> (plotArea.getRight() + 4.0f,
                                         plotArea.getY() - 2.0f,
                                         24.0f, 14.0f),
                juce::Justification::centredLeft);
}

// ── Spectrum curve ──────────────────────────────────────────────────────────

void SpectrumDisplay::drawSpectrumCurve (
    juce::Graphics& g,
    const std::array<float, HisstoryAudioProcessor::numBins>& data,
    juce::Colour colour,
    float thickness)
{
    const float sr   = processor.currentSampleRate.load();
    const float binW = sr / static_cast<float> (HisstoryAudioProcessor::fftSize);

    juce::Path path;
    bool started = false;

    for (int bin = 1; bin < HisstoryAudioProcessor::numBins; bin += 2)
    {
        float freq = static_cast<float> (bin) * binW;
        if (freq < minFreq || freq > maxFreq) continue;

        float x = freqToX (freq);
        float y = dbToY (data[bin]);
        y = juce::jlimit (plotArea.getY(), plotArea.getBottom(), y);

        if (! started) { path.startNewSubPath (x, y); started = true; }
        else           { path.lineTo (x, y); }
    }

    g.setColour (colour);
    g.strokePath (path, juce::PathStrokeType (thickness,
                  juce::PathStrokeType::curved));
}

// ── Threshold curve ─────────────────────────────────────────────────────────

void SpectrumDisplay::drawThresholdCurve (juce::Graphics& g)
{
    const float sr = processor.currentSampleRate.load();
    const float globalThr = processor.apvts.getRawParameterValue ("threshold")->load();
    const bool  hasProfile = processor.noiseProfileReady.load();

    juce::Path path;
    bool started = false;

    auto getThresholdDB = [&] (float freq) -> float
    {
        float bandOff = processor.interpolateBandOffset (freq);
        float offsetDB = globalThr + bandOff;

        if (hasProfile)
        {
            int bin = juce::jlimit (0,
                HisstoryAudioProcessor::numBins - 1,
                static_cast<int> (freq / (sr / HisstoryAudioProcessor::fftSize) + 0.5f));
            float noiseMagLin = processor.noiseProfileDisplay[bin];
            float noiseDB = juce::Decibels::gainToDecibels (noiseMagLin, -150.0f)
                          + fftNormDB;
            return noiseDB + offsetDB;
        }

        return -50.0f + offsetDB;   // fallback baseline in dBFS
    };

    for (float logF = std::log10 (minFreq); logF <= std::log10 (maxFreq); logF += 0.02f)
    {
        float freq = std::pow (10.0f, logF);
        float db   = getThresholdDB (freq);
        float x    = freqToX (freq);
        float y    = dbToY (db);
        y = juce::jlimit (plotArea.getY() - 5.0f, plotArea.getBottom() + 5.0f, y);

        if (! started) { path.startNewSubPath (x, y); started = true; }
        else           { path.lineTo (x, y); }
    }

    g.setColour (thresholdCurve);
    g.strokePath (path, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved));
}

// ── Band control points ─────────────────────────────────────────────────────

void SpectrumDisplay::drawBandPoints (juce::Graphics& g)
{
    const float sr = processor.currentSampleRate.load();
    const float globalThr = processor.apvts.getRawParameterValue ("threshold")->load();
    const bool  hasProfile = processor.noiseProfileReady.load();

    for (int i = 0; i < HisstoryAudioProcessor::numBands; ++i)
    {
        float freq    = HisstoryAudioProcessor::bandFrequencies[i];
        float bandOff = processor.apvts.getRawParameterValue (
                            "band" + juce::String (i + 1))->load();
        float offsetDB = globalThr + bandOff;

        float effectiveDB;
        if (hasProfile)
        {
            int bin = juce::jlimit (0,
                HisstoryAudioProcessor::numBins - 1,
                static_cast<int> (freq / (sr / HisstoryAudioProcessor::fftSize) + 0.5f));
            float noiseMagLin = processor.noiseProfileDisplay[bin];
            float noiseDB = juce::Decibels::gainToDecibels (noiseMagLin, -150.0f)
                          + fftNormDB;
            effectiveDB = noiseDB + offsetDB;
        }
        else
        {
            effectiveDB = -50.0f + offsetDB;
        }

        float x = freqToX (freq);
        float y = dbToY (effectiveDB);
        y = juce::jlimit (plotArea.getY(), plotArea.getBottom(), y);

        const float r = 12.0f;

        // Outer ring.
        g.setColour (thresholdCurve);
        g.fillEllipse (x - r, y - r, r * 2.0f, r * 2.0f);

        // Inner fill (dark).
        g.setColour (plotBackground);
        g.fillEllipse (x - r + 2.5f, y - r + 2.5f,
                       (r - 2.5f) * 2.0f, (r - 2.5f) * 2.0f);

        // Number.
        g.setColour (thresholdCurve);
        g.setFont (juce::Font (12.0f).boldened());
        g.drawText (juce::String (i + 1),
                    juce::Rectangle<float> (x - r, y - r, r * 2.0f, r * 2.0f),
                    juce::Justification::centred);
    }
}

// ── Mouse interaction ───────────────────────────────────────────────────────

void SpectrumDisplay::mouseDown (const juce::MouseEvent& e)
{
    const float sr = processor.currentSampleRate.load();
    const float globalThr = processor.apvts.getRawParameterValue ("threshold")->load();
    const bool  hasProfile = processor.noiseProfileReady.load();

    for (int i = 0; i < HisstoryAudioProcessor::numBands; ++i)
    {
        float freq    = HisstoryAudioProcessor::bandFrequencies[i];
        float bandOff = processor.apvts.getRawParameterValue (
                            "band" + juce::String (i + 1))->load();
        float offsetDB = globalThr + bandOff;

        float effectiveDB;
        if (hasProfile)
        {
            int bin = juce::jlimit (0,
                HisstoryAudioProcessor::numBins - 1,
                static_cast<int> (freq / (sr / HisstoryAudioProcessor::fftSize) + 0.5f));
            float noiseMagLin = processor.noiseProfileDisplay[bin];
            float noiseDB = juce::Decibels::gainToDecibels (noiseMagLin, -150.0f)
                          + fftNormDB;
            effectiveDB = noiseDB + offsetDB;
        }
        else
        {
            effectiveDB = -50.0f + offsetDB;
        }

        float px = freqToX (freq);
        float py = dbToY (effectiveDB);
        py = juce::jlimit (plotArea.getY(), plotArea.getBottom(), py);

        if (e.position.getDistanceFrom ({ px, py }) < 16.0f)
        {
            draggingBand = i;
            auto* param = processor.apvts.getParameter ("band" + juce::String (i + 1));
            param->beginChangeGesture();
            return;
        }
    }

    draggingBand = -1;
}

void SpectrumDisplay::mouseDrag (const juce::MouseEvent& e)
{
    if (draggingBand < 0) return;

    const float sr = processor.currentSampleRate.load();
    const float globalThr = processor.apvts.getRawParameterValue ("threshold")->load();
    const bool  hasProfile = processor.noiseProfileReady.load();

    float targetDB = yToDb (e.position.y);

    float baseDB;
    if (hasProfile)
    {
        float freq = HisstoryAudioProcessor::bandFrequencies[draggingBand];
        int bin = juce::jlimit (0,
            HisstoryAudioProcessor::numBins - 1,
            static_cast<int> (freq / (sr / HisstoryAudioProcessor::fftSize) + 0.5f));
        float noiseMagLin = processor.noiseProfileDisplay[bin];
        baseDB = juce::Decibels::gainToDecibels (noiseMagLin, -150.0f)
               + fftNormDB;
    }
    else
    {
        baseDB = -50.0f;
    }

    float newOffset = targetDB - baseDB - globalThr;
    newOffset = juce::jlimit (-30.0f, 30.0f, newOffset);

    auto* param = processor.apvts.getParameter ("band" + juce::String (draggingBand + 1));
    param->setValueNotifyingHost (param->convertTo0to1 (newOffset));
}

void SpectrumDisplay::mouseUp (const juce::MouseEvent&)
{
    if (draggingBand >= 0)
    {
        auto* param = processor.apvts.getParameter ("band" + juce::String (draggingBand + 1));
        param->endChangeGesture();
        draggingBand = -1;
    }
}

//==============================================================================
//  HisstoryAudioProcessorEditor
//==============================================================================
HisstoryAudioProcessorEditor::HisstoryAudioProcessorEditor (
    HisstoryAudioProcessor& p)
    : AudioProcessorEditor (&p),
      processor (p),
      spectrumDisplay (p)
{
    setLookAndFeel (&lnf);
    setSize (880, 500);

    addAndMakeVisible (spectrumDisplay);

    // ── Adaptive mode ────────────────────────────────────────────────────────
    adaptiveButton.setButtonText ("Adaptive mode");
    addAndMakeVisible (adaptiveButton);
    adaptiveAttach = std::make_unique<ButtonAttach> (processor.apvts, "adaptive", adaptiveButton);

    // ── Threshold slider ─────────────────────────────────────────────────────
    thresholdSlider.setSliderStyle (juce::Slider::LinearVertical);
    thresholdSlider.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    addAndMakeVisible (thresholdSlider);
    thresholdAttach = std::make_unique<SliderAttach> (
        processor.apvts, "threshold", thresholdSlider);

    thresholdLabel.setText ("Threshold [dB]", juce::dontSendNotification);
    thresholdLabel.setJustificationType (juce::Justification::centred);
    thresholdLabel.setFont (juce::Font (13.0f));
    thresholdLabel.setColour (juce::Label::textColourId, textNormal);
    addAndMakeVisible (thresholdLabel);

    thresholdValue.setJustificationType (juce::Justification::centred);
    thresholdValue.setFont (juce::Font (15.0f).boldened());
    thresholdValue.setColour (juce::Label::textColourId, textBright);
    thresholdValue.setColour (juce::Label::backgroundColourId, accent.withAlpha (0.15f));
    addAndMakeVisible (thresholdValue);

    // ── Reduction slider ─────────────────────────────────────────────────────
    reductionSlider.setSliderStyle (juce::Slider::LinearVertical);
    reductionSlider.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    addAndMakeVisible (reductionSlider);
    reductionAttach = std::make_unique<SliderAttach> (
        processor.apvts, "reduction", reductionSlider);

    reductionLabel.setText ("Reduction [dB]", juce::dontSendNotification);
    reductionLabel.setJustificationType (juce::Justification::centred);
    reductionLabel.setFont (juce::Font (13.0f));
    reductionLabel.setColour (juce::Label::textColourId, textNormal);
    addAndMakeVisible (reductionLabel);

    reductionValue.setJustificationType (juce::Justification::centred);
    reductionValue.setFont (juce::Font (15.0f).boldened());
    reductionValue.setColour (juce::Label::textColourId, textBright);
    reductionValue.setColour (juce::Label::backgroundColourId, accent.withAlpha (0.15f));
    addAndMakeVisible (reductionValue);

    // ── Bypass button ────────────────────────────────────────────────────────
    bypassButton.setClickingTogglesState (true);
    addAndMakeVisible (bypassButton);
    bypassAttach = std::make_unique<ButtonAttach> (processor.apvts, "bypass", bypassButton);

    startTimerHz (30);
}

HisstoryAudioProcessorEditor::~HisstoryAudioProcessorEditor()
{
    setLookAndFeel (nullptr);
}

//==============================================================================
//  Layout
//==============================================================================
void HisstoryAudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds();

    // ── Top control bar ──────────────────────────────────────────────────────
    auto topBar = bounds.removeFromTop (50);
    topBar.reduce (12, 8);

    adaptiveButton.setBounds (topBar.removeFromLeft (160));

    // ── Bottom bar ───────────────────────────────────────────────────────────
    auto bottomBar = bounds.removeFromBottom (42);
    bottomBar.reduce (12, 6);
    bypassButton.setBounds (bottomBar.removeFromLeft (90).reduced (0, 2));

    // ── Right slider panel ───────────────────────────────────────────────────
    auto rightPanel = bounds.removeFromRight (180);
    rightPanel.reduce (8, 4);

    auto thrCol = rightPanel.removeFromLeft (rightPanel.getWidth() / 2);
    auto redCol = rightPanel;

    thresholdLabel.setBounds  (thrCol.removeFromTop (20));
    thresholdValue.setBounds  (thrCol.removeFromBottom (26).reduced (8, 0));
    thresholdSlider.setBounds (thrCol.reduced (thrCol.getWidth() / 2 - 20, 4));

    reductionLabel.setBounds  (redCol.removeFromTop (20));
    reductionValue.setBounds  (redCol.removeFromBottom (26).reduced (8, 0));
    reductionSlider.setBounds (redCol.reduced (redCol.getWidth() / 2 - 20, 4));

    // ── Spectrum display ─────────────────────────────────────────────────────
    bounds.reduce (8, 4);
    spectrumDisplay.setBounds (bounds);
}

//==============================================================================
//  Paint
//==============================================================================
void HisstoryAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (background);

    // ── Legend (above spectrum display) ───────────────────────────────────────
    {
        auto legendY = spectrumDisplay.getY() - 20;
        auto legendX = spectrumDisplay.getX() + 8;
        g.setFont (12.0f);

        // Input
        g.setColour (inputCurve);
        g.fillRoundedRectangle (static_cast<float> (legendX),
                                static_cast<float> (legendY + 4),
                                20.0f, 3.0f, 1.0f);
        g.setColour (textNormal);
        g.drawText ("Input", legendX + 24, legendY, 40, 16,
                    juce::Justification::centredLeft);

        legendX += 75;

        // Output
        g.setColour (outputCurve);
        g.fillRoundedRectangle (static_cast<float> (legendX),
                                static_cast<float> (legendY + 4),
                                20.0f, 3.0f, 1.0f);
        g.setColour (textNormal);
        g.drawText ("Output", legendX + 24, legendY, 50, 16,
                    juce::Justification::centredLeft);

        legendX += 85;

        // Threshold
        g.setColour (thresholdCurve);
        g.fillRoundedRectangle (static_cast<float> (legendX),
                                static_cast<float> (legendY + 4),
                                20.0f, 3.0f, 1.0f);
        g.setColour (textNormal);
        g.drawText ("Threshold", legendX + 24, legendY, 70, 16,
                    juce::Justification::centredLeft);
    }
}

//==============================================================================
//  Timer
//==============================================================================
void HisstoryAudioProcessorEditor::timerCallback()
{
    spectrumDisplay.updateSpectrumData();
    spectrumDisplay.repaint();

    thresholdValue.setText (
        juce::String (thresholdSlider.getValue(), 1),
        juce::dontSendNotification);
    reductionValue.setText (
        juce::String (reductionSlider.getValue(), 1),
        juce::dontSendNotification);
}
