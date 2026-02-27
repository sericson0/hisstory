/*
  ==============================================================================
    Hisstory – PluginEditor.cpp

    Dark-themed GUI with orange accent colours.
  ==============================================================================
*/

#include "PluginEditor.h"
#include "BinaryData.h"

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

    const auto trackCol = slider.findColour (juce::Slider::trackColourId);
    const auto fillCol  = slider.findColour (juce::Slider::rotarySliderFillColourId);
    const auto thumbCol = slider.findColour (juce::Slider::thumbColourId);

    g.setColour (trackCol);
    g.drawLine (cx, top, cx, bottom, 3.0f);

    g.setColour (fillCol);
    g.drawLine (cx, sliderPos, cx, bottom, 3.0f);

    const float thumbR = 7.0f;
    g.setColour (thumbCol);
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
    if (button.getComponentID() == "spectrogramModeToggle")
    {
        auto bounds = button.getLocalBounds().toFloat().reduced (1.0f);
        juce::Colour bg = highlighted ? buttonBgHover : buttonBg;
        if (down) bg = bg.brighter (0.08f);

        g.setColour (bg);
        g.fillRoundedRectangle (bounds, 5.0f);
        g.setColour (gridLine);
        g.drawRoundedRectangle (bounds, 5.0f, 1.0f);
        return;
    }

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
    if (button.getComponentID() == "spectrogramModeToggle")
    {
        const bool spectrogramOn = button.getToggleState();
        const juce::Font normalFont (13.0f);
        const juce::Font boldFont = normalFont.boldened();
        auto bounds = button.getLocalBounds().toFloat().reduced (2.0f);
        auto leftHalf = bounds.removeFromLeft (bounds.getWidth() * 0.5f);
        auto rightHalf = bounds;

        g.setColour (spectrogramOn ? textNormal : accentBright);
        g.setFont (spectrogramOn ? normalFont : boldFont);
        g.drawText ("Analyzer", leftHalf, juce::Justification::centred, true);

        g.setColour (spectrogramOn ? accentBright : textNormal);
        g.setFont (spectrogramOn ? boldFont : normalFont);
        g.drawText ("Spectrogram", rightHalf, juce::Justification::centred, true);

        g.setColour (gridLine);
        g.drawLine (button.getWidth() * 0.5f, 5.0f, button.getWidth() * 0.5f,
                    (float) button.getHeight() - 5.0f, 1.0f);
        return;
    }

    if (button.getComponentID() == "collapseGlyphButton")
    {
        const bool showExpandGlyph = button.getToggleState();
        const auto b = button.getLocalBounds().toFloat().reduced (8.0f);
        const auto c = b.getCentre();
        const float s = juce::jmin (b.getWidth(), b.getHeight()) * 0.30f;

        auto drawArrow = [&] (juce::Point<float> start, juce::Point<float> end)
        {
            juce::Path p;
            p.startNewSubPath (start);
            p.lineTo (end);

            const auto dir = end - start;
            const float len = std::sqrt (dir.x * dir.x + dir.y * dir.y);
            if (len > 0.001f)
            {
                const juce::Point<float> n (dir.x / len, dir.y / len);
                const juce::Point<float> t (-n.y, n.x);
                const float head = 3.0f;
                p.startNewSubPath (end);
                p.lineTo (end - n * head + t * head * 0.75f);
                p.startNewSubPath (end);
                p.lineTo (end - n * head - t * head * 0.75f);
            }

            g.strokePath (p, juce::PathStrokeType (1.7f));
        };

        g.setColour (textBright);
        if (showExpandGlyph)
        {
            // Expand icon: two diagonal arrows pointing outwards.
            drawArrow ({ c.x - 1.0f, c.y + 1.0f }, { c.x - s - 4.0f, c.y + s + 4.0f });
            drawArrow ({ c.x + 1.0f, c.y - 1.0f }, { c.x + s + 4.0f, c.y - s - 4.0f });
        }
        else
        {
            // Collapse icon: two diagonal arrows pointing inwards.
            drawArrow ({ c.x - s - 4.0f, c.y + s + 4.0f }, { c.x - 1.0f, c.y + 1.0f });
            drawArrow ({ c.x + s + 4.0f, c.y - s - 4.0f }, { c.x + 1.0f, c.y - 1.0f });
        }
        return;
    }

    g.setColour (button.getToggleState() ? textBright : textNormal);
    g.setFont (14.0f);
    g.drawText (button.getButtonText(), button.getLocalBounds(),
                juce::Justification::centred);
}

void HisstoryLookAndFeel::drawTooltip (juce::Graphics& g,
                                       const juce::String& text,
                                       int width, int height)
{
    auto bounds = juce::Rectangle<int> (width, height).toFloat();
    g.setColour (background.brighter (0.20f));
    g.fillRoundedRectangle (bounds.reduced (0.5f), 4.0f);

    g.setColour (gridLine.brighter (0.2f));
    g.drawRoundedRectangle (bounds.reduced (0.5f), 4.0f, 1.0f);

    g.setColour (textBright);
    g.setFont (compactTooltipMode ? 11.0f : 14.0f);
    g.drawFittedText (text, juce::Rectangle<int> (width, height).reduced (6, 4),
                      juce::Justification::centredLeft, 3);
}

//==============================================================================
//  SpectrumDisplay
//==============================================================================
SpectrumDisplay::SpectrumDisplay (HisstoryAudioProcessor& p) : processor (p)
{
    dispInput.fill  (-100.0f);
    dispOutput.fill (-100.0f);

    for (auto& col : spectrogramBuf)
        col.fill (spectrogramMinDB);

    buildMelFilterbank();
}

void SpectrumDisplay::resized()
{
    plotArea = getLocalBounds().toFloat()
                  .withTrimmedLeft   (8.0f)
                  .withTrimmedBottom (22.0f)
                  .withTrimmedTop    (22.0f)
                  .withTrimmedRight  (58.0f);
}

// ── Coordinate mapping ──────────────────────────────────────────────────────

float SpectrumDisplay::freqToX (float hz) const
{
    float logMin = std::log10 (analyzerMinFreq);
    float logMax = std::log10 (analyzerMaxFreq);
    float logF   = std::log10 (std::max (hz, analyzerMinFreq));
    float t = (logF - logMin) / (logMax - logMin);
    // Apply power > 1.0 to give more space to higher frequencies
    t = std::pow (t, 0.85f);
    return plotArea.getX() + t * plotArea.getWidth();
}

float SpectrumDisplay::dbToY (float db) const
{
    float norm = (db - analyzerMinDB) / (analyzerMaxDB - analyzerMinDB);
    return plotArea.getBottom() - norm * plotArea.getHeight();
}

float SpectrumDisplay::xToFreq (float x) const
{
    float logMin = std::log10 (analyzerMinFreq);
    float logMax = std::log10 (analyzerMaxFreq);
    float t = (x - plotArea.getX()) / plotArea.getWidth();
    // Inverse of the power mapping
    t = std::pow (std::max (t, 0.0f), 1.0f / 0.85f);
    return std::pow (10.0f, logMin + t * (logMax - logMin));
}

float SpectrumDisplay::yToDb (float y) const
{
    float norm = (plotArea.getBottom() - y) / plotArea.getHeight();
    return analyzerMinDB + norm * (analyzerMaxDB - analyzerMinDB);
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
    g.setFont (13.0f);

    // Frequency lines (analyzer starts at 20 Hz)
    const float freqLines[] = { 20, 50, 100, 200, 500, 1000, 2000, 5000, 10000, 20000 };
    const char* freqLabels[] = { "20", "50", "100", "200", "500", "1k", "2k", "5k", "10k", "20k" };

    for (int i = 0; i < 10; ++i)
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

    // "Hz" label -- placed after the rightmost frequency label with gap
    g.setColour (gridText);
    g.drawText ("Hz",
                juce::Rectangle<float> (plotArea.getRight() + 4.0f,
                                         plotArea.getBottom() + 3.0f,
                                         24.0f, 16.0f),
                juce::Justification::centredLeft);

    // dB lines: from analyzer max down to min (skip top-most to avoid overlap with "dB" label)
    for (float db = analyzerMaxDB; db >= analyzerMinDB; db -= 10.0f)
    {
        float y = dbToY (db);
        g.setColour (gridLine);
        g.drawHorizontalLine (static_cast<int> (y),
                              plotArea.getX(), plotArea.getRight());

        // Skip the top label so it doesn't overlap with the "dB" unit label
        if (db < analyzerMaxDB - 1.0f)
        {
            g.setColour (gridText);
            g.drawText (juce::String (static_cast<int> (db)),
                        juce::Rectangle<float> (plotArea.getRight() + 4.0f,
                                                 y - 7.0f, 50.0f, 14.0f),
                        juce::Justification::centredLeft);
        }
    }

    // "dB" label at top-right (with the top dB value shown inline)
    g.setColour (gridText);
    g.drawText (juce::String (static_cast<int> (analyzerMaxDB)) + " dB",
                juce::Rectangle<float> (plotArea.getRight() + 4.0f,
                                         plotArea.getY() - 7.0f,
                                         50.0f, 14.0f),
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
        if (freq < analyzerMinFreq || freq > analyzerMaxFreq) continue;

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

    for (float logF = std::log10 (analyzerMinFreq); logF <= std::log10 (analyzerMaxFreq); logF += 0.02f)
    {
        float freq = std::pow (10.0f, logF);
        float db   = getThresholdDB (freq);
        float x    = freqToX (freq);
        float y    = dbToY (db);
        y = juce::jlimit (plotArea.getY() - 5.0f, plotArea.getBottom() + 5.0f, y);

        if (! started) { path.startNewSubPath (x, y); started = true; }
        else           { path.lineTo (x, y); }
    }

    const bool bypassed = processor.apvts.getRawParameterValue ("bypass")->load() > 0.5f;
    g.setColour (bypassed ? inactive : thresholdCurve);
    g.strokePath (path, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved));
}

// ── Band control points ─────────────────────────────────────────────────────

void SpectrumDisplay::drawBandPoints (juce::Graphics& g)
{
    const float sr = processor.currentSampleRate.load();
    const float globalThr = processor.apvts.getRawParameterValue ("threshold")->load();
    const bool  hasProfile = processor.noiseProfileReady.load();
    const bool  isAdaptive = processor.apvts.getRawParameterValue ("adaptive")->load() > 0.5f;

    const bool bypassed = processor.apvts.getRawParameterValue ("bypass")->load() > 0.5f;
    const auto pointColour = bypassed ? inactive : thresholdCurve;

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

        g.setColour (pointColour);
        g.fillEllipse (x - r, y - r, r * 2.0f, r * 2.0f);

        g.setColour (plotBackground);
        g.fillEllipse (x - r + 2.5f, y - r + 2.5f,
                       (r - 2.5f) * 2.0f, (r - 2.5f) * 2.0f);

        g.setColour (pointColour);
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
            col.fill (spectrogramMinDB);
    }

    repaint();
}

void SpectrumDisplay::buildMelFilterbank()
{
    melFilters.clear();
    melFilters.resize (static_cast<size_t> (numMelBins));

    const float sr   = std::max (processor.currentSampleRate.load(), 1.0f);
    const float binW = sr / static_cast<float> (HisstoryAudioProcessor::fftSize);

    const float melMin = hzToMel (spectrogramMinFreq);
    const float melMax = hzToMel (std::min (spectrogramMaxFreq, sr * 0.5f));

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
            : spectrogramMinDB;

        col[static_cast<size_t> (m)] = juce::jlimit (spectrogramMinDB, spectrogramMaxDB, melDB);
    }

    spectrogramWritePos = (spectrogramWritePos + 1) % numTimeCols;
}

juce::Colour SpectrumDisplay::dbToColour (float db) const
{
    float t = juce::jlimit (0.0f, 1.0f, (db - spectrogramMinDB) / (spectrogramMaxDB - spectrogramMinDB));

    // Orange-themed colourmap: black → dark brown → #A34210 → golden orange → white
    if (t < 0.2f)
    {
        float s = t / 0.2f;
        return juce::Colour::fromFloatRGBA (s * 0.12f, s * 0.06f, s * 0.02f, 1.0f);
    }
    else if (t < 0.45f)
    {
        float s = (t - 0.2f) / 0.25f;
        return juce::Colour::fromFloatRGBA (0.12f + s * 0.52f, 0.06f + s * 0.20f, 0.02f + s * 0.04f, 1.0f);
    }
    else if (t < 0.7f)
    {
        float s = (t - 0.45f) / 0.25f;
        return juce::Colour::fromFloatRGBA (0.64f + s * 0.31f, 0.26f + s * 0.37f, 0.06f + s * 0.0f, 1.0f);
    }
    else if (t < 0.9f)
    {
        float s = (t - 0.7f) / 0.2f;
        return juce::Colour::fromFloatRGBA (0.95f + s * 0.05f, 0.63f + s * 0.27f, 0.06f + s * 0.24f, 1.0f);
    }
    else
    {
        float s = (t - 0.9f) / 0.1f;
        return juce::Colour::fromFloatRGBA (1.0f, 0.9f + s * 0.1f, 0.3f + s * 0.7f, 1.0f);
    }
}

float SpectrumDisplay::melToY (float mel) const
{
    const float melMin = hzToMel (spectrogramMinFreq);
    const float melMax = hzToMel (spectrogramMaxFreq);
    float t = (mel - melMin) / (melMax - melMin);
    return plotArea.getBottom() - t * plotArea.getHeight();
}

float SpectrumDisplay::yToMel (float y) const
{
    const float melMin = hzToMel (spectrogramMinFreq);
    const float melMax = hzToMel (spectrogramMaxFreq);
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
            float melIdx = (mel - hzToMel (spectrogramMinFreq))
                         / (hzToMel (spectrogramMaxFreq) - hzToMel (spectrogramMinFreq))
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
    g.setFont (13.0f);

    const float freqLines[] = { 100, 200, 500, 1000, 2000, 5000, 10000, 20000 };
    const char* freqLabels[] = { "100", "200", "500", "1k", "2k", "5k", "10k", "20k" };

    // Find the topmost visible label to skip it (avoid overlapping with "Hz" unit label)
    float topMostY = 1e6f;
    int topMostIdx = -1;
    for (int i = 0; i < 8; ++i)
    {
        float mel = hzToMel (freqLines[i]);
        float y = melToY (mel);
        if (y >= plotArea.getY() && y <= plotArea.getBottom() && y < topMostY)
        {
            topMostY = y;
            topMostIdx = i;
        }
    }

    for (int i = 0; i < 8; ++i)
    {
        float mel = hzToMel (freqLines[i]);
        float y = melToY (mel);
        if (y < plotArea.getY() || y > plotArea.getBottom()) continue;

        g.setColour (HisstoryColours::gridLine.withAlpha (0.5f));
        g.drawHorizontalLine (static_cast<int> (y),
                              plotArea.getX(), plotArea.getRight());

        // Skip the topmost label to avoid overlapping with the "Hz" unit label
        if (i != topMostIdx)
        {
            g.setColour (HisstoryColours::gridText);
            g.drawText (freqLabels[i],
                        juce::Rectangle<float> (plotArea.getRight() + 4.0f,
                                                 y - 7.0f, 36.0f, 14.0f),
                        juce::Justification::centredLeft);
        }
    }

    // Top-right: show the topmost frequency and "Hz" together
    g.setColour (HisstoryColours::gridText);
    if (topMostIdx >= 0)
    {
        juce::String topLabel = juce::String (freqLabels[topMostIdx]) + " Hz";
        g.drawText (topLabel,
                    juce::Rectangle<float> (plotArea.getRight() + 4.0f,
                                             topMostY - 7.0f, 42.0f, 14.0f),
                    juce::Justification::centredLeft);
    }
    else
    {
        g.drawText ("Hz",
                    juce::Rectangle<float> (plotArea.getRight() + 4.0f,
                                             plotArea.getY() - 2.0f,
                                             24.0f, 14.0f),
                    juce::Justification::centredLeft);
    }
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

    // ── Spectrogram toggle (in top bar, left side) ──────────────────────────
    spectrumDisplay.spectrogramToggle.setComponentID ("spectrogramModeToggle");
    spectrumDisplay.spectrogramToggle.setClickingTogglesState (true);
    spectrumDisplay.spectrogramToggle.onClick = [this]
    {
        spectrumDisplay.setSpectrogramMode (
            spectrumDisplay.spectrogramToggle.getToggleState());
    };
    addAndMakeVisible (spectrumDisplay.spectrogramToggle);

    // ── Adaptive mode (TextButton, same style as Bypass) ────────────────────
    adaptiveButton.setClickingTogglesState (true);
    adaptiveButton.setTooltip (
        "Enable adaptive noise profiling that continuously learns the noise floor");
    addAndMakeVisible (adaptiveButton);
    adaptiveAttach = std::make_unique<ButtonAttach> (processor.apvts, "adaptive", adaptiveButton);

    // ── Bypass button ─────────────────────────────────────────────────────────
    bypassButton.setClickingTogglesState (true);
    bypassButton.setTooltip (
        "Bypass all processing and pass audio through unchanged");
    addAndMakeVisible (bypassButton);
    bypassAttach = std::make_unique<ButtonAttach> (processor.apvts, "bypass", bypassButton);

    // ── Collapse toggle ───────────────────────────────────────────────────────
    collapseButton.setComponentID ("collapseGlyphButton");
    collapseButton.setButtonText ("");
    collapseButton.setToggleState (false, juce::dontSendNotification);
    collapseButton.setTooltip (
        "Collapse or expand the spectrum display panel");
    collapseButton.onClick = [this]
    {
        collapsed = ! collapsed;
        collapseButton.setToggleState (collapsed, juce::dontSendNotification);
        spectrumDisplay.setVisible (! collapsed);
        spectrumDisplay.spectrogramToggle.setVisible (! collapsed);
        lnf.setCompactTooltipMode (collapsed);

        if (collapsed)
            setSize (228, 320);
        else
            setSize (880, 500);
    };
    addAndMakeVisible (collapseButton);

    // ── Threshold slider ─────────────────────────────────────────────────────
    thresholdSlider.setSliderStyle (juce::Slider::LinearVertical);
    thresholdSlider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 60, 22);
    thresholdSlider.setTextBoxIsEditable (true);
    thresholdSlider.setColour (juce::Slider::trackColourId, sliderTrack);
    thresholdSlider.setColour (juce::Slider::rotarySliderFillColourId, accentBright);
    thresholdSlider.setColour (juce::Slider::thumbColourId, textBright);
    thresholdSlider.setColour (juce::Slider::textBoxBackgroundColourId, background.brighter (0.03f));
    thresholdSlider.setColour (juce::Slider::textBoxOutlineColourId, gridLine);
    thresholdSlider.textFromValueFunction = [] (double val)
    {
        return juce::String (std::abs (val), 1);
    };
    thresholdSlider.valueFromTextFunction = [] (const juce::String& text)
    {
        return -std::abs (text.getDoubleValue());
    };
    addAndMakeVisible (thresholdSlider);
    thresholdAttach = std::make_unique<SliderAttach> (
        processor.apvts, "threshold", thresholdSlider);

    thresholdLabel.setText ("Threshold [dB]", juce::dontSendNotification);
    thresholdLabel.setJustificationType (juce::Justification::centred);
    thresholdLabel.setFont (juce::Font (13.0f));
    thresholdLabel.setColour (juce::Label::textColourId, textNormal);
    addAndMakeVisible (thresholdLabel);

    // ── Reduction slider ─────────────────────────────────────────────────────
    reductionSlider.setSliderStyle (juce::Slider::LinearVertical);
    reductionSlider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 60, 22);
    reductionSlider.setTextBoxIsEditable (true);
    reductionSlider.setColour (juce::Slider::trackColourId, sliderTrack);
    reductionSlider.setColour (juce::Slider::rotarySliderFillColourId, accentBright);
    reductionSlider.setColour (juce::Slider::thumbColourId, textBright);
    reductionSlider.setColour (juce::Slider::textBoxBackgroundColourId, background.brighter (0.03f));
    reductionSlider.setColour (juce::Slider::textBoxOutlineColourId, gridLine);
    reductionSlider.setScrollWheelEnabled (true);
    reductionSlider.setMouseDragSensitivity (320);
    addAndMakeVisible (reductionSlider);
    reductionAttach = std::make_unique<SliderAttach> (
        processor.apvts, "reduction", reductionSlider);

    reductionLabel.setText ("Reduction [dB]", juce::dontSendNotification);
    reductionLabel.setJustificationType (juce::Justification::centred);
    reductionLabel.setFont (juce::Font (13.0f));
    reductionLabel.setColour (juce::Label::textColourId, textNormal);
    addAndMakeVisible (reductionLabel);

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

    setupMetricValueLabel (metricHfRemovedVal);
    setupMetricValueLabel (metricMidKeptVal);
    setupMetricValueLabel (metricOutputVal);
    setupMetricValueLabel (metricHLRVal);

    addAndMakeVisible (metricHfRemovedName);     addAndMakeVisible (metricHfRemovedVal);
    addAndMakeVisible (metricMidKeptName);       addAndMakeVisible (metricMidKeptVal);
    addAndMakeVisible (metricOutputName);        addAndMakeVisible (metricOutputVal);
    addAndMakeVisible (metricHLRName);           addAndMakeVisible (metricHLRVal);

    // ── Help button ──────────────────────────────────────────────────────────
    helpButton.setColour (juce::TextButton::buttonColourId, accent);
    helpButton.setColour (juce::TextButton::textColourOffId, textBright);
    helpButton.onClick = [this]
    {
        auto* aw = new juce::AlertWindow ("Hisstory Help", {}, juce::AlertWindow::NoIcon);

        struct HelpEntry { const char* title; const char* body; };
        const HelpEntry entries[] = {
            { "THRESHOLD",     "Noise gate sensitivity. Higher values remove more noise, but also more signal." },
            { "REDUCTION",     "dB reduction applied to signals below the threshold. Higher values attenuate noise more aggressively." },
            { "HF REMOVED",    "dB of high-frequency energy removed." },
            { "MID PRESERVED", "Removal of mid-range content (200-3000 Hz). If too high, reduce threshold." },
            { "OUTPUT LEVEL",  "Overall dB gain change between input and output." },
            { "HARMONIC LOSS", "Percent tonal energy lost during processing. Lower is better. High value indicates signal loss." }, 
            { "CONTACT", "If you experience any issues, or have any suggestions, please contact us at tangotoolkit@gmail.com" }
        };

        juce::String formatted;
        for (auto& e : entries)
        {
            if (formatted.isNotEmpty())
                formatted += "\n";
            formatted += juce::String (e.title) + "\n" + juce::String (e.body) + "\n";
        }

        auto* te = new juce::TextEditor();
        te->setMultiLine (true, true);
        te->setReadOnly (true);
        te->setScrollbarsShown (true);
        te->setColour (juce::TextEditor::backgroundColourId, juce::Colours::transparentBlack);
        te->setColour (juce::TextEditor::outlineColourId, juce::Colours::transparentBlack);

        juce::Font normalFont (14.0f);
        juce::Font boldFont   (14.0f);
        boldFont = boldFont.boldened();

        te->clear();
        for (int i = 0; i < 7; ++i)
        {
            if (i > 0) te->setFont (normalFont);
            if (i > 0) te->insertTextAtCaret ("\n");
            te->setFont (boldFont);
            te->insertTextAtCaret (juce::String (entries[i].title) + "\n");
            te->setFont (normalFont);
            te->insertTextAtCaret (juce::String (entries[i].body) + "\n");
        }
        te->setCaretPosition (0);
        te->setSize (320, 300);

        aw->addCustomComponent (te);
        aw->addButton ("OK", 0);
        aw->enterModalState (true, nullptr, true);
    };
    addAndMakeVisible (helpButton);

    // ── Brand logo ──────────────────────────────────────────────────────────
    brandLogoImage = juce::ImageCache::getFromMemory (
        BinaryData::Logo_png, BinaryData::Logo_pngSize);

    lnf.setCompactTooltipMode (collapsed);
    updateBypassVisualState (processor.apvts.getRawParameterValue ("bypass")->load() > 0.5f);
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
    const bool isCompact = collapsed;

    // ── Top control bar ──────────────────────────────────────────────────────
    auto topBar = bounds.removeFromTop (isCompact ? 32 : 36);
    topBar.reduce (12, 6);

    // Left side: Spectrogram toggle (only when expanded)
    if (! collapsed)
    {
        spectrumDisplay.spectrogramToggle.setBounds (
            topBar.removeFromLeft (230).reduced (0, 2));
        topBar.removeFromLeft (8);
    }

    // Right side: Bypass, Adaptive, Collapse (right to left)
    const int toggleW = isCompact ? 72 : 88;
    bypassButton.setBounds (topBar.removeFromRight (toggleW).reduced (0, 2));
    topBar.removeFromRight (8);
    adaptiveButton.setBounds (topBar.removeFromRight (toggleW).reduced (0, 2));
    topBar.removeFromRight (8);
    collapseButton.setBounds (topBar.removeFromRight (40).reduced (0, 2));

    // ── Right panel (sliders + metrics) ──────────────────────────────────────
    const int panelW = collapsed ? bounds.getWidth() : 180;
    auto rightPanel = collapsed ? bounds : bounds.removeFromRight (panelW);
    rightPanel.reduce (isCompact ? 6 : 8, isCompact ? 2 : 4);

    // Slider columns
    const int sliderSectionH = isCompact ? 142 : 220;
    auto sliderSection = rightPanel.removeFromTop (sliderSectionH);
    auto thrCol = sliderSection.removeFromLeft (sliderSection.getWidth() / 2);
    auto redCol = sliderSection;

    thresholdLabel.setBounds  (thrCol.removeFromTop (isCompact ? 16 : 18));
    thresholdSlider.setBounds (thrCol);

    reductionLabel.setBounds  (redCol.removeFromTop (isCompact ? 16 : 18));
    reductionSlider.setBounds (redCol);

    // Metrics section
    rightPanel.removeFromTop (isCompact ? 2 : 6);

    if (! isCompact)
    {
        auto metricsRow = rightPanel.removeFromTop (18);
        helpButton.setBounds (metricsRow.removeFromRight (18));
        metricsHeader.setBounds (metricsRow);
        rightPanel.removeFromTop (4);
    }

    // Metric rows
    auto layoutMetricRow = [&] (juce::Label& name, juce::Label& value)
    {
        auto row = rightPanel.removeFromTop (isCompact ? 18 : 22);
        name.setBounds (row.removeFromLeft (row.getWidth() * 2 / 3).reduced (4, 0));
        value.setBounds (row.reduced (2, 0));
    };

    metricsHeader.setVisible (! isCompact);
    helpButton.setVisible (! isCompact);
    metricMidKeptName.setVisible (! isCompact);
    metricMidKeptVal.setVisible (! isCompact);
    metricOutputName.setVisible (! isCompact);
    metricOutputVal.setVisible (! isCompact);

    layoutMetricRow (metricHfRemovedName, metricHfRemovedVal);
    if (! isCompact)
    {
        layoutMetricRow (metricMidKeptName, metricMidKeptVal);
        layoutMetricRow (metricOutputName, metricOutputVal);
    }
    layoutMetricRow (metricHLRName, metricHLRVal);

    // ── Brand logo below metrics ─────────────────────────────────────────────
    if (! isCompact)
    {
        rightPanel.removeFromTop (8);
        brandLogoBounds = rightPanel.removeFromTop (
            std::min (rightPanel.getHeight(), 100));
        compactFooterBounds = {};
    }
    else
    {
        brandLogoBounds = {};
        rightPanel.removeFromTop (2);
        compactFooterBounds = rightPanel.removeFromTop (24).reduced (0, 2);
    }

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
    const bool isCompact = collapsed;

    // Draw a subtle separator line between sliders and metrics
    const int panelW = collapsed ? getWidth() : 180;
    auto rightPanel = getLocalBounds().removeFromRight (panelW);
    rightPanel.removeFromTop (isCompact ? 32 : 36);
    rightPanel.reduce (isCompact ? 6 : 8, isCompact ? 2 : 4);
    const int sliderSectionH = isCompact ? 142 : 220;
    int sepY = rightPanel.getY() + sliderSectionH + 2;
    g.setColour (gridLine);
    g.drawHorizontalLine (sepY, (float)(rightPanel.getX() + 8),
                          (float)(rightPanel.getRight() - 8));

    // ── Draw brand logo with high-quality resampling ─────────────────────────
    if (brandLogoImage.isValid() && ! brandLogoBounds.isEmpty())
    {
        auto dest = brandLogoBounds.toFloat();

        float imgAspect = static_cast<float> (brandLogoImage.getWidth())
                        / static_cast<float> (brandLogoImage.getHeight());
        float destAspect = dest.getWidth() / dest.getHeight();

        juce::Rectangle<float> drawArea;
        if (imgAspect > destAspect)
        {
            float h = dest.getWidth() / imgAspect;
            drawArea = { dest.getX(), dest.getCentreY() - h * 0.5f,
                         dest.getWidth(), h };
        }
        else
        {
            float w = dest.getHeight() * imgAspect;
            drawArea = { dest.getCentreX() - w * 0.5f, dest.getY(),
                         w, dest.getHeight() };
        }

        g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
        g.drawImage (brandLogoImage, drawArea);
    }

    if (isCompact)
    {
        g.setColour (accentBright);
        g.setFont (juce::Font (16.0f).boldened());
        g.drawText ("HISSTORY", compactFooterBounds, juce::Justification::centred);
    }
}

void HisstoryAudioProcessorEditor::updateBypassVisualState (bool bypassed)
{
    if (bypassed == bypassVisualState)
        return;

    bypassVisualState = bypassed;

    const auto mutedText = gridText;
    const auto activeText = textNormal;
    const auto thrTrack = bypassed ? inactive : sliderTrack;
    const auto thrFill = bypassed ? inactive : accentBright;
    const auto thrThumb = bypassed ? gridText : textBright;
    const auto redTrack = bypassed ? inactive : sliderTrack;
    const auto redFill = bypassed ? inactive : accentBright;
    const auto redThumb = bypassed ? gridText : textBright;

    thresholdSlider.setColour (juce::Slider::trackColourId, thrTrack);
    thresholdSlider.setColour (juce::Slider::rotarySliderFillColourId, thrFill);
    thresholdSlider.setColour (juce::Slider::thumbColourId, thrThumb);
    thresholdSlider.setColour (juce::Slider::textBoxTextColourId, bypassed ? mutedText : textBright);
    thresholdLabel.setColour (juce::Label::textColourId, bypassed ? mutedText : activeText);

    reductionSlider.setColour (juce::Slider::trackColourId, redTrack);
    reductionSlider.setColour (juce::Slider::rotarySliderFillColourId, redFill);
    reductionSlider.setColour (juce::Slider::thumbColourId, redThumb);
    reductionSlider.setColour (juce::Slider::textBoxTextColourId, bypassed ? mutedText : textBright);
    reductionLabel.setColour (juce::Label::textColourId, bypassed ? mutedText : activeText);

    metricHfRemovedVal.setColour (juce::Label::textColourId, bypassed ? mutedText : textBright);
    metricMidKeptVal.setColour (juce::Label::textColourId, bypassed ? mutedText : textBright);
    metricOutputVal.setColour (juce::Label::textColourId, bypassed ? mutedText : textBright);
    metricHLRVal.setColour (juce::Label::textColourId, bypassed ? mutedText : textBright);

    thresholdSlider.repaint();
    reductionSlider.repaint();
    spectrumDisplay.repaint();
}

//==============================================================================
//  Metrics computation
//==============================================================================
void HisstoryAudioProcessorEditor::updateMetrics()
{
    const bool bypassed = processor.apvts.getRawParameterValue ("bypass")->load() > 0.5f;
    if (bypassed)
    {
        metricHfRemovedVal.setText ("-.-", juce::dontSendNotification);
        metricMidKeptVal.setText ("-.-", juce::dontSendNotification);
        metricOutputVal.setText ("-.-", juce::dontSendNotification);
        metricHLRVal.setText ("-.-", juce::dontSendNotification);

        metricHfRemovedVal.setColour (juce::Label::textColourId, gridText);
        metricMidKeptVal.setColour (juce::Label::textColourId, gridText);
        metricOutputVal.setColour (juce::Label::textColourId, gridText);
        metricHLRVal.setColour (juce::Label::textColourId, gridText);
        return;
    }

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

    // ── Harmonic Loss (percentage) ──────────────────────────────────────
    //  Reads from the processor's atomic: fraction (0–1) of tonal energy
    //  removed.  Displayed as percentage.  0% = perfect preservation.
    {
        const float rawLoss = processor.metricHarmonicLossRatio.load();
        constexpr float hlrSmooth = 0.92f;
        smoothHLR = hlrSmooth * smoothHLR + (1.0f - hlrSmooth) * rawLoss;

        float lossPct = smoothHLR * 100.0f;

        metricHLRVal.setText (
            juce::String (lossPct, 1) + "%", juce::dontSendNotification);

        if (lossPct < 3.0f)
            metricHLRVal.setColour (juce::Label::textColourId, metricGood);
        else if (lossPct < 10.0f)
            metricHLRVal.setColour (juce::Label::textColourId, metricWarn);
        else
            metricHLRVal.setColour (juce::Label::textColourId, metricBad);
    }
}

//==============================================================================
//  Timer
//==============================================================================
void HisstoryAudioProcessorEditor::timerCallback()
{
    spectrumDisplay.updateSpectrumData();
    spectrumDisplay.repaint();

    // Slider text boxes update automatically via JUCE text-from-value

    const bool bypassed = processor.apvts.getRawParameterValue ("bypass")->load() > 0.5f;
    updateBypassVisualState (bypassed);
    updateMetrics();
}
