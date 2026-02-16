/*
  ==============================================================================
    Hisstory – PluginEditor.cpp

    Dark-themed GUI with orange accent colours.
  ==============================================================================
*/

#include "PluginEditor.h"

using namespace HisstoryColours;
using namespace HisstoryConstants;

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

    g.setColour (sliderTrack);
    g.drawLine (cx, top, cx, bottom, 3.0f);

    g.setColour (accentBright);
    g.drawLine (cx, sliderPos, cx, bottom, 3.0f);

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

    juce::Colour bg = on ? buttonSelected : (highlighted ? buttonBgHover : buttonBg);
    if (down) bg = bg.brighter (0.1f);

    g.setColour (bg);
    g.fillRoundedRectangle (bounds, 5.0f);

    g.setColour (on ? buttonSelected.brighter (0.3f) : gridLine);
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

    for (auto& col : spectrogramBuf)
        col.fill (minDB);

    buildMelFilterbank();
}

void SpectrumDisplay::resized()
{
    plotArea = getLocalBounds().toFloat()
                  .withTrimmedLeft   (8.0f)
                  .withTrimmedBottom (22.0f)
                  .withTrimmedTop    (22.0f)    // room for legend inside
                  .withTrimmedRight  (44.0f);
}

// ── Coordinate mapping ──────────────────────────────────────────────────────

float SpectrumDisplay::freqToX (float hz) const
{
    float logMin = std::log10 (minFreq);
    float logMax = std::log10 (maxFreq);
    float logF   = std::log10 (std::max (hz, minFreq));
    float t = (logF - logMin) / (logMax - logMin);
    // Apply power > 1.0 to give more space to higher frequencies
    t = std::pow (t, 0.85f);
    return plotArea.getX() + t * plotArea.getWidth();
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
    // Inverse of the power mapping
    t = std::pow (std::max (t, 0.0f), 1.0f / 0.85f);
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
        float inFS  = processor.inputSpectrumDB[i]  + fftNormDB;
        float outFS = processor.outputSpectrumDB[i] + fftNormDB;

        dispInput[i]  = decay * dispInput[i]  + (1.0f - decay) * inFS;
        dispOutput[i] = decay * dispOutput[i] + (1.0f - decay) * outFS;
    }

    if (showSpectrogram)
        updateSpectrogramColumn();
}

// ── Paint ───────────────────────────────────────────────────────────────────

void SpectrumDisplay::paint (juce::Graphics& g)
{
    g.setColour (plotBackground);
    g.fillRoundedRectangle (getLocalBounds().toFloat(), 4.0f);

    if (plotArea.isEmpty()) return;

    if (showSpectrogram)
    {
        drawSpectrogram (g);
        drawMelGrid (g);
    }
    else
    {
        drawGrid (g);
        drawSpectrumCurve (g, dispInput,  inputCurve.withAlpha (0.5f), 1.0f);
        drawSpectrumCurve (g, dispOutput, outputCurve, 1.5f);
        drawThresholdCurve (g);
        drawBandPoints (g);
        drawLegend (g);
    }
}

// ── Legend (drawn inside spectrum display, top-left) ────────────────────────

void SpectrumDisplay::drawLegend (juce::Graphics& g)
{
    float legendY = plotArea.getY() - 16.0f;
    float legendX = plotArea.getX() + 6.0f;
    g.setFont (11.0f);

    // Input
    g.setColour (inputCurve);
    g.fillRoundedRectangle (legendX, legendY + 5.0f, 16.0f, 2.5f, 1.0f);
    g.setColour (textNormal);
    g.drawText ("Input", (int)(legendX + 20), (int)legendY, 36, 14,
                juce::Justification::centredLeft);

    legendX += 62.0f;

    // Output
    g.setColour (outputCurve);
    g.fillRoundedRectangle (legendX, legendY + 5.0f, 16.0f, 2.5f, 1.0f);
    g.setColour (textNormal);
    g.drawText ("Output", (int)(legendX + 20), (int)legendY, 44, 14,
                juce::Justification::centredLeft);

    legendX += 70.0f;

    // Threshold
    g.setColour (thresholdCurve);
    g.fillRoundedRectangle (legendX, legendY + 5.0f, 16.0f, 2.5f, 1.0f);
    g.setColour (textNormal);
    g.drawText ("Threshold", (int)(legendX + 20), (int)legendY, 60, 14,
                juce::Justification::centredLeft);
}

// ── Grid ────────────────────────────────────────────────────────────────────

void SpectrumDisplay::drawGrid (juce::Graphics& g)
{
    g.setFont (11.0f);

    // Frequency lines (starting at 100 Hz, emphasis on higher frequencies)
    const float freqLines[] = { 100, 200, 500, 1000, 2000, 5000, 10000, 20000 };
    const char* freqLabels[] = { "100", "200", "500", "1k", "2k", "5k", "10k", "20k" };

    for (int i = 0; i < 8; ++i)
    {
        float x = freqToX (freqLines[i]);
        if (x < plotArea.getX() || x > plotArea.getRight()) continue;

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

    // "Hz" label
    g.setColour (gridText);
    g.drawText ("Hz",
                juce::Rectangle<float> (plotArea.getRight() - 16.0f,
                                         plotArea.getBottom() + 3.0f,
                                         24.0f, 16.0f),
                juce::Justification::centred);

    // dB lines: from -20 dB down to -100 dB
    for (float db = maxDB; db >= minDB; db -= 10.0f)
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

    // "dB" label at top-right
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
    const bool  isAdaptive = processor.apvts.getRawParameterValue ("adaptive")->load() > 0.5f;

    juce::Path path;
    bool started = false;

    auto getThresholdDB = [&] (float freq) -> float
    {
        float bandOff = processor.interpolateBandOffset (freq);
        if (isAdaptive)
            bandOff += HisstoryAudioProcessor::adaptiveBandBoost;

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

        return -50.0f + offsetDB;
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
    const bool  isAdaptive = processor.apvts.getRawParameterValue ("adaptive")->load() > 0.5f;

    for (int i = 0; i < HisstoryAudioProcessor::numBands; ++i)
    {
        float freq    = HisstoryAudioProcessor::bandFrequencies[i];
        float bandOff = processor.apvts.getRawParameterValue (
                            "band" + juce::String (i + 1))->load();
        if (isAdaptive)
            bandOff += HisstoryAudioProcessor::adaptiveBandBoost;
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

        g.setColour (thresholdCurve);
        g.fillEllipse (x - r, y - r, r * 2.0f, r * 2.0f);

        g.setColour (plotBackground);
        g.fillEllipse (x - r + 2.5f, y - r + 2.5f,
                       (r - 2.5f) * 2.0f, (r - 2.5f) * 2.0f);

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
    if (showSpectrogram) return;

    const float sr = processor.currentSampleRate.load();
    const float globalThr = processor.apvts.getRawParameterValue ("threshold")->load();
    const bool  hasProfile = processor.noiseProfileReady.load();
    const bool  isAdaptive = processor.apvts.getRawParameterValue ("adaptive")->load() > 0.5f;

    for (int i = 0; i < HisstoryAudioProcessor::numBands; ++i)
    {
        float freq    = HisstoryAudioProcessor::bandFrequencies[i];
        float bandOff = processor.apvts.getRawParameterValue (
                            "band" + juce::String (i + 1))->load();
        if (isAdaptive)
            bandOff += HisstoryAudioProcessor::adaptiveBandBoost;
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
    const bool  isAdaptive = processor.apvts.getRawParameterValue ("adaptive")->load() > 0.5f;

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
    if (isAdaptive)
        newOffset -= HisstoryAudioProcessor::adaptiveBandBoost;
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

// ── Spectrogram support ─────────────────────────────────────────────────────

void SpectrumDisplay::setSpectrogramMode (bool enabled)
{
    if (showSpectrogram == enabled) return;
    showSpectrogram = enabled;

    if (enabled)
    {
        spectrogramImage = {};
        spectrogramWritePos = 0;
        for (auto& col : spectrogramBuf)
            col.fill (minDB);
    }

    repaint();
}

void SpectrumDisplay::buildMelFilterbank()
{
    melFilters.clear();
    melFilters.resize (static_cast<size_t> (numMelBins));

    const float sr   = std::max (processor.currentSampleRate.load(), 1.0f);
    const float binW = sr / static_cast<float> (HisstoryAudioProcessor::fftSize);

    const float melMin = hzToMel (minFreq);
    const float melMax = hzToMel (std::min (maxFreq, sr * 0.5f));

    // numMelBins + 2 edge points for triangular filters
    std::vector<float> melEdges (static_cast<size_t> (numMelBins + 2));
    for (int i = 0; i < numMelBins + 2; ++i)
        melEdges[static_cast<size_t> (i)] = melToHz (melMin + static_cast<float> (i) / static_cast<float> (numMelBins + 1) * (melMax - melMin));

    for (int m = 0; m < numMelBins; ++m)
    {
        const float fLow  = melEdges[static_cast<size_t> (m)];
        const float fMid  = melEdges[static_cast<size_t> (m + 1)];
        const float fHigh = melEdges[static_cast<size_t> (m + 2)];

        int binLow  = std::max (1, static_cast<int> (std::floor (fLow  / binW)));
        int binHigh = std::min (HisstoryAudioProcessor::numBins - 1,
                                static_cast<int> (std::ceil (fHigh / binW)));

        auto& filt = melFilters[static_cast<size_t> (m)];
        filt.startBin = binLow;
        filt.endBin   = binHigh;
        filt.weights.resize (static_cast<size_t> (binHigh - binLow + 1), 0.0f);

        for (int bin = binLow; bin <= binHigh; ++bin)
        {
            float freq = static_cast<float> (bin) * binW;
            float w = 0.0f;
            if (freq >= fLow && freq <= fMid && fMid > fLow)
                w = (freq - fLow) / (fMid - fLow);
            else if (freq > fMid && freq <= fHigh && fHigh > fMid)
                w = (fHigh - freq) / (fHigh - fMid);
            filt.weights[static_cast<size_t> (bin - binLow)] = w;
        }
    }
}

void SpectrumDisplay::updateSpectrogramColumn()
{
    if (! showSpectrogram) return;

    auto& col = spectrogramBuf[static_cast<size_t> (spectrogramWritePos)];

    for (int m = 0; m < numMelBins; ++m)
    {
        const auto& filt = melFilters[static_cast<size_t> (m)];
        float sum = 0.0f;
        float wSum = 0.0f;

        for (int bin = filt.startBin; bin <= filt.endBin; ++bin)
        {
            float w = filt.weights[static_cast<size_t> (bin - filt.startBin)];
            float db = dispOutput[static_cast<size_t> (bin)];
            float linPow = std::pow (10.0f, db / 10.0f);
            sum  += w * linPow;
            wSum += w;
        }

        float melDB = (wSum > 1e-20f)
            ? 10.0f * std::log10 (sum / wSum + 1e-20f)
            : minDB;

        col[static_cast<size_t> (m)] = juce::jlimit (minDB, maxDB, melDB);
    }

    spectrogramWritePos = (spectrogramWritePos + 1) % numTimeCols;
}

juce::Colour SpectrumDisplay::dbToColour (float db) const
{
    float t = juce::jlimit (0.0f, 1.0f, (db - minDB) / (maxDB - minDB));

    // Perceptual colourmap: dark blue → cyan → yellow → red → white
    if (t < 0.25f)
    {
        float s = t / 0.25f;
        return juce::Colour::fromFloatRGBA (0.0f, s * 0.3f, 0.15f + s * 0.55f, 1.0f);
    }
    else if (t < 0.5f)
    {
        float s = (t - 0.25f) / 0.25f;
        return juce::Colour::fromFloatRGBA (s * 0.2f, 0.3f + s * 0.7f, 0.7f - s * 0.2f, 1.0f);
    }
    else if (t < 0.75f)
    {
        float s = (t - 0.5f) / 0.25f;
        return juce::Colour::fromFloatRGBA (0.2f + s * 0.8f, 1.0f - s * 0.2f, 0.5f - s * 0.5f, 1.0f);
    }
    else
    {
        float s = (t - 0.75f) / 0.25f;
        return juce::Colour::fromFloatRGBA (1.0f, 0.8f * (1.0f - s) + s, s, 1.0f);
    }
}

float SpectrumDisplay::melToY (float mel) const
{
    const float melMin = hzToMel (minFreq);
    const float melMax = hzToMel (maxFreq);
    float t = (mel - melMin) / (melMax - melMin);
    return plotArea.getBottom() - t * plotArea.getHeight();
}

float SpectrumDisplay::yToMel (float y) const
{
    const float melMin = hzToMel (minFreq);
    const float melMax = hzToMel (maxFreq);
    float t = (plotArea.getBottom() - y) / plotArea.getHeight();
    return melMin + t * (melMax - melMin);
}

void SpectrumDisplay::drawSpectrogram (juce::Graphics& g)
{
    const int imgW = static_cast<int> (plotArea.getWidth());
    const int imgH = static_cast<int> (plotArea.getHeight());

    if (imgW <= 0 || imgH <= 0) return;

    if (spectrogramImage.isNull()
        || spectrogramImage.getWidth() != imgW
        || spectrogramImage.getHeight() != imgH)
    {
        spectrogramImage = juce::Image (juce::Image::RGB, imgW, imgH, true);
    }

    // Render the full spectrogram image from the circular buffer
    juce::Image::BitmapData bmp (spectrogramImage,
                                  juce::Image::BitmapData::writeOnly);

    const int colsToDraw = std::min (imgW, numTimeCols);
    const float colWidth = static_cast<float> (imgW) / static_cast<float> (colsToDraw);

    for (int col = 0; col < colsToDraw; ++col)
    {
        int bufIdx = (spectrogramWritePos - colsToDraw + col + numTimeCols) % numTimeCols;
        const auto& melCol = spectrogramBuf[static_cast<size_t> (bufIdx)];

        int xStart = static_cast<int> (static_cast<float> (col) * colWidth);
        int xEnd   = static_cast<int> (static_cast<float> (col + 1) * colWidth);
        xEnd = std::min (xEnd, imgW);

        for (int py = 0; py < imgH; ++py)
        {
            float mel = yToMel (plotArea.getY() + static_cast<float> (py));
            float melIdx = (mel - hzToMel (minFreq))
                         / (hzToMel (maxFreq) - hzToMel (minFreq))
                         * static_cast<float> (numMelBins - 1);
            melIdx = juce::jlimit (0.0f, static_cast<float> (numMelBins - 1), melIdx);

            int lo = static_cast<int> (melIdx);
            int hi = std::min (lo + 1, numMelBins - 1);
            float frac = melIdx - static_cast<float> (lo);
            float db = melCol[static_cast<size_t> (lo)] * (1.0f - frac)
                      + melCol[static_cast<size_t> (hi)] * frac;

            juce::Colour c = dbToColour (db);

            for (int px = xStart; px < xEnd; ++px)
                bmp.setPixelColour (px, py, c);
        }
    }

    g.drawImageAt (spectrogramImage,
                   static_cast<int> (plotArea.getX()),
                   static_cast<int> (plotArea.getY()));
}

void SpectrumDisplay::drawMelGrid (juce::Graphics& g)
{
    g.setFont (11.0f);

    const float freqLines[] = { 100, 200, 500, 1000, 2000, 5000, 10000, 20000 };
    const char* freqLabels[] = { "100", "200", "500", "1k", "2k", "5k", "10k", "20k" };

    for (int i = 0; i < 8; ++i)
    {
        float mel = hzToMel (freqLines[i]);
        float y = melToY (mel);
        if (y < plotArea.getY() || y > plotArea.getBottom()) continue;

        g.setColour (HisstoryColours::gridLine.withAlpha (0.5f));
        g.drawHorizontalLine (static_cast<int> (y),
                              plotArea.getX(), plotArea.getRight());

        g.setColour (HisstoryColours::gridText);
        g.drawText (freqLabels[i],
                    juce::Rectangle<float> (plotArea.getRight() + 4.0f,
                                             y - 7.0f, 36.0f, 14.0f),
                    juce::Justification::centredLeft);
    }

    g.setColour (HisstoryColours::gridText);
    g.drawText ("Hz",
                juce::Rectangle<float> (plotArea.getRight() + 4.0f,
                                         plotArea.getY() - 2.0f,
                                         24.0f, 14.0f),
                juce::Justification::centredLeft);
}

//==============================================================================
//  HisstoryAudioProcessorEditor
//==============================================================================

static void setupMetricNameLabel (juce::Label& label, const juce::String& text)
{
    label.setText (text, juce::dontSendNotification);
    label.setJustificationType (juce::Justification::centredLeft);
    label.setFont (juce::Font (11.0f));
    label.setColour (juce::Label::textColourId, HisstoryColours::gridText);
}

static void setupMetricValueLabel (juce::Label& label)
{
    label.setText ("---", juce::dontSendNotification);
    label.setJustificationType (juce::Justification::centredRight);
    label.setFont (juce::Font (12.0f).boldened());
    label.setColour (juce::Label::textColourId, HisstoryColours::textBright);
}

HisstoryAudioProcessorEditor::HisstoryAudioProcessorEditor (
    HisstoryAudioProcessor& p)
    : AudioProcessorEditor (&p),
      processor (p),
      spectrumDisplay (p)
{
    setLookAndFeel (&lnf);
    setSize (880, 500);

    addAndMakeVisible (spectrumDisplay);

    // ── Adaptive mode (TextButton, same style as Bypass) ────────────────────
    adaptiveButton.setClickingTogglesState (true);
    addAndMakeVisible (adaptiveButton);
    adaptiveAttach = std::make_unique<ButtonAttach> (processor.apvts, "adaptive", adaptiveButton);

    // ── Bypass button ─────────────────────────────────────────────────────────
    bypassButton.setClickingTogglesState (true);
    addAndMakeVisible (bypassButton);
    bypassAttach = std::make_unique<ButtonAttach> (processor.apvts, "bypass", bypassButton);

    // ── Spectrogram toggle ────────────────────────────────────────────────────
    spectrogramToggle.setClickingTogglesState (true);
    spectrogramToggle.onClick = [this]
    {
        spectrumDisplay.setSpectrogramMode (spectrogramToggle.getToggleState());
    };
    addAndMakeVisible (spectrogramToggle);

    // ── Collapse toggle ───────────────────────────────────────────────────────
    collapseButton.onClick = [this]
    {
        collapsed = ! collapsed;
        collapseButton.setButtonText (collapsed ? ">>" : "<<");
        spectrumDisplay.setVisible (! collapsed);

        if (collapsed)
            setSize (280, 500);
        else
            setSize (880, 500);
    };
    addAndMakeVisible (collapseButton);

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

    // ── Metrics ──────────────────────────────────────────────────────────────
    metricsHeader.setText ("METRICS", juce::dontSendNotification);
    metricsHeader.setJustificationType (juce::Justification::centred);
    metricsHeader.setFont (juce::Font (11.0f).boldened());
    metricsHeader.setColour (juce::Label::textColourId, gridText);
    addAndMakeVisible (metricsHeader);

    setupMetricNameLabel (metricHfRemovedName,    "HF Removed");
    setupMetricNameLabel (metricMidKeptName,      "Mid Preserved");
    setupMetricNameLabel (metricOutputName,       "Output Level");
    setupMetricNameLabel (metricHLRName,          "Harmonic Loss");
    setupMetricNameLabel (metricFluxName,         "Residual Noise");

    setupMetricValueLabel (metricHfRemovedVal);
    setupMetricValueLabel (metricMidKeptVal);
    setupMetricValueLabel (metricOutputVal);
    setupMetricValueLabel (metricHLRVal);
    setupMetricValueLabel (metricFluxVal);

    addAndMakeVisible (metricHfRemovedName);     addAndMakeVisible (metricHfRemovedVal);
    addAndMakeVisible (metricMidKeptName);       addAndMakeVisible (metricMidKeptVal);
    addAndMakeVisible (metricOutputName);        addAndMakeVisible (metricOutputVal);
    addAndMakeVisible (metricHLRName);           addAndMakeVisible (metricHLRVal);
    addAndMakeVisible (metricFluxName);          addAndMakeVisible (metricFluxVal);

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
    auto topBar = bounds.removeFromTop (36);
    topBar.reduce (12, 6);

    // Left side: collapse toggle + spectrogram toggle
    collapseButton.setBounds (topBar.removeFromLeft (36).reduced (0, 2));
    topBar.removeFromLeft (6);

    if (! collapsed)
    {
        spectrogramToggle.setBounds (topBar.removeFromLeft (110).reduced (0, 2));
        spectrogramToggle.setVisible (true);
    }
    else
    {
        spectrogramToggle.setVisible (false);
    }

    // Right side: Adaptive + Bypass
    bypassButton.setBounds (topBar.removeFromRight (80).reduced (0, 2));
    topBar.removeFromRight (8);
    adaptiveButton.setBounds (topBar.removeFromRight (100).reduced (0, 2));

    // ── Right panel (sliders + metrics) ──────────────────────────────────────
    const int panelW = collapsed ? bounds.getWidth() : 180;
    auto rightPanel = collapsed ? bounds : bounds.removeFromRight (panelW);
    rightPanel.reduce (8, 4);

    // Slider columns
    auto sliderSection = rightPanel.removeFromTop (220);
    auto thrCol = sliderSection.removeFromLeft (sliderSection.getWidth() / 2);
    auto redCol = sliderSection;

    thresholdLabel.setBounds  (thrCol.removeFromTop (18));
    thresholdValue.setBounds  (thrCol.removeFromBottom (24).reduced (6, 0));
    thresholdSlider.setBounds (thrCol.reduced (thrCol.getWidth() / 2 - 16, 2));

    reductionLabel.setBounds  (redCol.removeFromTop (18));
    reductionValue.setBounds  (redCol.removeFromBottom (24).reduced (6, 0));
    reductionSlider.setBounds (redCol.reduced (redCol.getWidth() / 2 - 16, 2));

    // Metrics section
    rightPanel.removeFromTop (6);

    // Separator line area
    metricsHeader.setBounds (rightPanel.removeFromTop (18));
    rightPanel.removeFromTop (4);

    // Metric rows
    auto layoutMetricRow = [&] (juce::Label& name, juce::Label& value)
    {
        auto row = rightPanel.removeFromTop (22);
        name.setBounds (row.removeFromLeft (row.getWidth() * 2 / 3).reduced (4, 0));
        value.setBounds (row.reduced (2, 0));
    };

    layoutMetricRow (metricHfRemovedName,    metricHfRemovedVal);
    layoutMetricRow (metricMidKeptName,      metricMidKeptVal);
    layoutMetricRow (metricOutputName,       metricOutputVal);
    layoutMetricRow (metricHLRName,          metricHLRVal);
    layoutMetricRow (metricFluxName,         metricFluxVal);

    // ── Spectrum display (remaining space) ───────────────────────────────────
    if (! collapsed)
    {
        bounds.reduce (8, 2);
        spectrumDisplay.setBounds (bounds);
    }
}

//==============================================================================
//  Paint
//==============================================================================
void HisstoryAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (background);

    // Draw a subtle separator line between sliders and metrics
    const int panelW = collapsed ? getWidth() : 180;
    auto rightPanel = getLocalBounds().removeFromRight (panelW);
    rightPanel.removeFromTop (36);
    rightPanel.reduce (8, 4);
    int sepY = rightPanel.getY() + 220 + 2;
    g.setColour (gridLine);
    g.drawHorizontalLine (sepY, (float)(rightPanel.getX() + 8),
                          (float)(rightPanel.getRight() - 8));
}

//==============================================================================
//  Metrics computation
//==============================================================================
void HisstoryAudioProcessorEditor::updateMetrics()
{
    const float sr = processor.currentSampleRate.load();
    const float binHz = sr / static_cast<float> (HisstoryAudioProcessor::fftSize);

    float inputMidPower  = 0.0f, outputMidPower  = 0.0f;
    float inputHfPower   = 0.0f, outputHfPower   = 0.0f;
    float inputTotalPower = 0.0f, outputTotalPower = 0.0f;

    for (int bin = 1; bin < HisstoryAudioProcessor::numBins; ++bin)
    {
        float freq = static_cast<float> (bin) * binHz;

        float inDB  = processor.inputSpectrumDB[bin]  + fftNormDB;
        float outDB = processor.outputSpectrumDB[bin] + fftNormDB;

        float inPow  = std::pow (10.0f, inDB / 10.0f);
        float outPow = std::pow (10.0f, outDB / 10.0f);

        if (freq >= 200.0f && freq <= 3000.0f)
        {
            inputMidPower  += inPow;
            outputMidPower += outPow;
        }
        if (freq >= 4000.0f && freq <= 16000.0f)
        {
            inputHfPower  += inPow;
            outputHfPower += outPow;
        }

        inputTotalPower  += inPow;
        outputTotalPower += outPow;
    }

    float hfRedDB   = 10.0f * std::log10 ((outputHfPower + 1e-20f) / (inputHfPower + 1e-20f));
    float midPresDB = 10.0f * std::log10 ((outputMidPower + 1e-20f) / (inputMidPower + 1e-20f));
    float overallDB = 10.0f * std::log10 ((outputTotalPower + 1e-20f) / (inputTotalPower + 1e-20f));

    constexpr float k = 0.92f;
    smoothHfRemoved  = k * smoothHfRemoved  + (1.0f - k) * hfRedDB;
    smoothMidKept    = k * smoothMidKept    + (1.0f - k) * midPresDB;
    smoothOutput     = k * smoothOutput     + (1.0f - k) * overallDB;

    metricHfRemovedVal.setText (
        juce::String (smoothHfRemoved, 1) + " dB", juce::dontSendNotification);
    metricMidKeptVal.setText (
        juce::String (smoothMidKept, 1) + " dB", juce::dontSendNotification);
    metricOutputVal.setText (
        juce::String (smoothOutput, 1) + " dB", juce::dontSendNotification);

    // Color-code HF Removed (more negative = better noise removal)
    if (smoothHfRemoved < -3.0f)
        metricHfRemovedVal.setColour (juce::Label::textColourId, metricGood);
    else if (smoothHfRemoved < -1.0f)
        metricHfRemovedVal.setColour (juce::Label::textColourId, metricWarn);
    else
        metricHfRemovedVal.setColour (juce::Label::textColourId, textNormal);

    // Color-code Mid Preserved (closer to 0 = better signal preservation)
    if (smoothMidKept > -1.0f)
        metricMidKeptVal.setColour (juce::Label::textColourId, metricGood);
    else if (smoothMidKept > -3.0f)
        metricMidKeptVal.setColour (juce::Label::textColourId, metricWarn);
    else
        metricMidKeptVal.setColour (juce::Label::textColourId, metricBad);

    // Color-code Output Level (close to 0 = no unwanted gain change)
    if (std::abs (smoothOutput) < 1.0f)
        metricOutputVal.setColour (juce::Label::textColourId, metricGood);
    else if (std::abs (smoothOutput) < 3.0f)
        metricOutputVal.setColour (juce::Label::textColourId, metricWarn);
    else
        metricOutputVal.setColour (juce::Label::textColourId, metricBad);

    // ── Harmonic Loss Ratio ─────────────────────────────────────────────
    //  Reads from the processor's atomic: output_HNR / input_HNR.
    //  > 1.0 = harmonics preserved well relative to noise.
    //  < 1.0 = harmonic content is being degraded.
    {
        const float rawHLR = processor.metricHarmonicLossRatio.load();
        constexpr float hlrSmooth = 0.92f;
        smoothHLR = hlrSmooth * smoothHLR + (1.0f - hlrSmooth) * rawHLR;

        metricHLRVal.setText (
            juce::String (smoothHLR, 2) + "x", juce::dontSendNotification);

        if (smoothHLR >= 1.0f)
            metricHLRVal.setColour (juce::Label::textColourId, metricGood);
        else if (smoothHLR >= 0.7f)
            metricHLRVal.setColour (juce::Label::textColourId, metricWarn);
        else
            metricHLRVal.setColour (juce::Label::textColourId, metricBad);
    }

    // ── Residual Spectral Flux ───────────────────────────────────────────
    //  Low flux = residual is noise-like (good).
    //  High flux = residual contains structured / musical content (bad).
    {
        const float rawFlux = processor.metricResidualFlux.load();
        constexpr float fluxSmooth = 0.92f;
        smoothFlux = fluxSmooth * smoothFlux + (1.0f - fluxSmooth) * rawFlux;

        const float pct = juce::jlimit (0.0f, 100.0f, smoothFlux * 100.0f);
        metricFluxVal.setText (
            juce::String (static_cast<int> (pct + 0.5f)) + "%", juce::dontSendNotification);

        if (pct < 25.0f)
            metricFluxVal.setColour (juce::Label::textColourId, metricGood);
        else if (pct < 50.0f)
            metricFluxVal.setColour (juce::Label::textColourId, metricWarn);
        else
            metricFluxVal.setColour (juce::Label::textColourId, metricBad);
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

    updateMetrics();
}
