/*
  ==============================================================================
    Benchmark.cpp

    Offline benchmark tool that:
      1. Reads FLAC audio files from example_track/
      2. Processes each through Hisstory (internal) and RX 11 Voice De-noise (VST3)
      3. Writes WAV outputs to benchmark_output/
      4. Computes and prints objective quality metrics
  ==============================================================================
*/

#include "PluginProcessor.h"
#include <JuceHeader.h>
#include <cstdio>
#include <cmath>
#include <vector>
#include <memory>
#include <algorithm>

//==============================================================================
//  Metrics
//==============================================================================
struct AudioMetrics
{
    double overallRMS       = 0.0;
    double quietRMS         = 0.0;   // RMS of first+last 2 seconds (noise floor)
    double midBandEnergy    = 0.0;   // 200-3000 Hz
    double hfEnergy         = 0.0;   // 4000-16000 Hz
    double crestFactor      = 0.0;   // peak / RMS
    double dynamicRangeDB   = 0.0;   // loudest - quietest 100ms window
    double peakLevel        = 0.0;
};

static AudioMetrics computeMetrics (const juce::AudioBuffer<float>& buffer,
                                    double sampleRate)
{
    AudioMetrics m;
    const int numSamples = buffer.getNumSamples();
    const int numCh      = buffer.getNumChannels();
    if (numSamples == 0 || numCh == 0) return m;

    // Mono-mix for analysis
    std::vector<float> mono (numSamples, 0.0f);
    for (int ch = 0; ch < numCh; ++ch)
    {
        auto* data = buffer.getReadPointer (ch);
        for (int i = 0; i < numSamples; ++i)
            mono[i] += data[i] / static_cast<float> (numCh);
    }

    // Overall RMS and peak
    double sumSq = 0.0;
    for (int i = 0; i < numSamples; ++i)
    {
        double s = mono[i];
        sumSq += s * s;
        if (std::abs (s) > m.peakLevel)
            m.peakLevel = std::abs (s);
    }
    m.overallRMS = std::sqrt (sumSq / numSamples);
    m.crestFactor = (m.overallRMS > 1e-12) ? m.peakLevel / m.overallRMS : 0.0;

    // Quiet RMS (first + last 2 seconds)
    {
        int quietSamples = std::min ((int)(sampleRate * 2.0), numSamples / 4);
        double qSumSq = 0.0;
        int qCount = 0;
        for (int i = 0; i < quietSamples; ++i)
        {
            qSumSq += (double)mono[i] * mono[i];
            qCount++;
        }
        for (int i = numSamples - quietSamples; i < numSamples; ++i)
        {
            qSumSq += (double)mono[i] * mono[i];
            qCount++;
        }
        m.quietRMS = (qCount > 0) ? std::sqrt (qSumSq / qCount) : 0.0;
    }

    // Spectral band energies via FFT
    {
        const int fftOrder = 12;
        const int fftSz = 1 << fftOrder;  // 4096
        juce::dsp::FFT fft (fftOrder);
        juce::dsp::WindowingFunction<float> window (
            static_cast<size_t>(fftSz),
            juce::dsp::WindowingFunction<float>::hann, false);

        double midSum = 0.0, hfSum = 0.0;
        int frameCount = 0;
        const int hop = fftSz / 2;

        for (int start = 0; start + fftSz <= numSamples; start += hop)
        {
            alignas(16) float buf[fftSz * 2] {};
            for (int i = 0; i < fftSz; ++i)
                buf[i] = mono[start + i];

            window.multiplyWithWindowingTable (buf, static_cast<size_t>(fftSz));
            fft.performRealOnlyForwardTransform (buf, true);

            const int numBins = fftSz / 2 + 1;
            for (int bin = 0; bin < numBins; ++bin)
            {
                float re = buf[2 * bin];
                float im = buf[2 * bin + 1];
                double energy = (double)re * re + (double)im * im;
                double freq = (double)bin * sampleRate / fftSz;

                if (freq >= 200.0 && freq <= 3000.0)
                    midSum += energy;
                if (freq >= 4000.0 && freq <= 16000.0)
                    hfSum += energy;
            }
            frameCount++;
        }
        if (frameCount > 0)
        {
            m.midBandEnergy = midSum / frameCount;
            m.hfEnergy      = hfSum / frameCount;
        }
    }

    // Dynamic range: loudest vs quietest 100ms window
    {
        int windowLen = (int)(sampleRate * 0.1);
        if (windowLen < 1) windowLen = 1;
        double loudest  = -200.0;
        double quietest = 200.0;

        for (int start = 0; start + windowLen <= numSamples; start += windowLen / 2)
        {
            double wSumSq = 0.0;
            for (int i = start; i < start + windowLen; ++i)
                wSumSq += (double)mono[i] * mono[i];
            double wRMS = std::sqrt (wSumSq / windowLen);
            double wDB  = 20.0 * std::log10 (wRMS + 1e-20);
            if (wDB > loudest)  loudest  = wDB;
            if (wDB < quietest) quietest = wDB;
        }
        m.dynamicRangeDB = loudest - quietest;
    }

    return m;
}

static void printMetrics (const char* label, const AudioMetrics& m)
{
    std::printf ("  %-22s  RMS=%.4f  QuietRMS=%.6f  MidE=%.1f  HfE=%.1f  "
                 "Crest=%.2f  DynRange=%.1fdB  Peak=%.4f\n",
                 label, m.overallRMS, m.quietRMS, m.midBandEnergy,
                 m.hfEnergy, m.crestFactor, m.dynamicRangeDB, m.peakLevel);
}

static void printComparison (const char* pluginName,
                             const AudioMetrics& input,
                             const AudioMetrics& output)
{
    double noiseReductionDB = 20.0 * std::log10 ((output.quietRMS + 1e-20)
                                                 / (input.quietRMS + 1e-20));
    double midPreservation = (input.midBandEnergy > 1e-20)
                                ? output.midBandEnergy / input.midBandEnergy : 0.0;
    double hfReduction = (input.hfEnergy > 1e-20)
                            ? output.hfEnergy / input.hfEnergy : 0.0;
    double crestChange = (input.crestFactor > 0.01)
                            ? output.crestFactor / input.crestFactor : 0.0;
    double dynRangeChange = output.dynamicRangeDB - input.dynamicRangeDB;

    std::printf ("  %-14s  NoiseReduc=%+.1fdB  MidPreserv=%.3f  "
                 "HfReduc=%.3f  CrestRatio=%.3f  DynRangeDelta=%+.1fdB\n",
                 pluginName, noiseReductionDB, midPreservation,
                 hfReduction, crestChange, dynRangeChange);
}

//==============================================================================
//  Process a buffer through HisstoryAudioProcessor
//==============================================================================
static juce::AudioBuffer<float> processWithHisttory (
    const juce::AudioBuffer<float>& input, double sampleRate)
{
    HisstoryAudioProcessor proc;
    const int blockSize = 512;
    const int numCh = input.getNumChannels();
    const int numSamples = input.getNumSamples();

    proc.setPlayConfigDetails (numCh, numCh, sampleRate, blockSize);
    proc.prepareToPlay (sampleRate, blockSize);

    juce::AudioBuffer<float> output (numCh, numSamples);
    output.clear();
    juce::MidiBuffer midi;

    for (int pos = 0; pos < numSamples; pos += blockSize)
    {
        int thisBlock = std::min (blockSize, numSamples - pos);
        juce::AudioBuffer<float> block (numCh, thisBlock);

        for (int ch = 0; ch < numCh; ++ch)
            block.copyFrom (ch, 0, input, ch, pos, thisBlock);

        proc.processBlock (block, midi);

        for (int ch = 0; ch < numCh; ++ch)
            output.copyFrom (ch, pos, block, ch, 0, thisBlock);
    }

    return output;
}

//==============================================================================
//  Process a buffer through an external VST3 plugin
//==============================================================================
static juce::AudioBuffer<float> processWithVST3 (
    const juce::AudioBuffer<float>& input, double sampleRate,
    const juce::String& vst3Path)
{
    juce::AudioPluginFormatManager formatManager;
    formatManager.addFormat (new juce::VST3PluginFormat());

    juce::OwnedArray<juce::PluginDescription> descriptions;
    juce::KnownPluginList pluginList;
    juce::VST3PluginFormat vst3Format;

    // Scan the specific VST3 file
    pluginList.scanAndAddFile (vst3Path, true, descriptions, vst3Format);

    if (descriptions.isEmpty())
    {
        std::printf ("  [ERROR] Could not load VST3: %s\n", vst3Path.toRawUTF8());
        return input;  // return unprocessed
    }

    juce::String errorMessage;
    auto plugin = formatManager.createPluginInstance (
        *descriptions[0], sampleRate, 512, errorMessage);

    if (plugin == nullptr)
    {
        std::printf ("  [ERROR] Could not instantiate VST3: %s\n",
                     errorMessage.toRawUTF8());
        return input;
    }

    const int blockSize = 512;
    const int numCh = input.getNumChannels();
    const int numSamples = input.getNumSamples();

    plugin->setPlayConfigDetails (numCh, numCh, sampleRate, blockSize);
    plugin->prepareToPlay (sampleRate, blockSize);

    juce::AudioBuffer<float> output (numCh, numSamples);
    output.clear();
    juce::MidiBuffer midi;

    for (int pos = 0; pos < numSamples; pos += blockSize)
    {
        int thisBlock = std::min (blockSize, numSamples - pos);
        juce::AudioBuffer<float> block (numCh, thisBlock);

        for (int ch = 0; ch < numCh; ++ch)
            block.copyFrom (ch, 0, input, ch, pos, thisBlock);

        plugin->processBlock (block, midi);

        for (int ch = 0; ch < numCh; ++ch)
            output.copyFrom (ch, pos, block, ch, 0, thisBlock);
    }

    plugin->releaseResources();
    return output;
}

//==============================================================================
//  Read audio file (FLAC, WAV, etc.)
//==============================================================================
static bool readAudioFile (const juce::File& file,
                           juce::AudioBuffer<float>& buffer,
                           double& sampleRate)
{
    juce::AudioFormatManager formatManager;
    formatManager.registerBasicFormats();

    std::unique_ptr<juce::AudioFormatReader> reader (
        formatManager.createReaderFor (file));

    if (reader == nullptr)
    {
        std::printf ("  [ERROR] Cannot read: %s\n", file.getFullPathName().toRawUTF8());
        return false;
    }

    sampleRate = reader->sampleRate;
    buffer.setSize ((int) reader->numChannels, (int) reader->lengthInSamples);
    reader->read (&buffer, 0, (int) reader->lengthInSamples, 0, true, true);
    return true;
}

//==============================================================================
//  Write WAV file
//==============================================================================
static bool writeWavFile (const juce::File& file,
                          const juce::AudioBuffer<float>& buffer,
                          double sampleRate)
{
    file.getParentDirectory().createDirectory();

    std::unique_ptr<juce::FileOutputStream> outStream (file.createOutputStream());
    if (outStream == nullptr) return false;

    juce::WavAudioFormat wavFormat;
    std::unique_ptr<juce::AudioFormatWriter> writer (
        wavFormat.createWriterFor (outStream.get(),
                                   sampleRate,
                                   (unsigned int) buffer.getNumChannels(),
                                   24, {}, 0));
    if (writer == nullptr) return false;
    outStream.release();  // writer now owns the stream

    writer->writeFromAudioSampleBuffer (buffer, 0, buffer.getNumSamples());
    return true;
}

//==============================================================================
//  Main
//==============================================================================
int main (int argc, char* argv[])
{
    // Initialise JUCE message manager (needed for plugin hosting)
    juce::ScopedJuceInitialiser_GUI init;

    // Paths
    juce::File exeDir = juce::File::getSpecialLocation (
        juce::File::currentExecutableFile).getParentDirectory();
    // Navigate up from build/Release to project root
    juce::File projectRoot = exeDir.getParentDirectory()
                                   .getParentDirectory();

    // Allow override via command-line
    if (argc > 1)
        projectRoot = juce::File (juce::String (argv[1]));

    juce::File trackDir  = projectRoot.getChildFile ("example_track");
    juce::File outputDir = projectRoot.getChildFile ("benchmark_output");
    juce::File rx11Path  = trackDir.getChildFile ("RX 11 Voice De-noise.vst3");

    outputDir.createDirectory();

    bool hasRX11 = rx11Path.existsAsFile();

    std::printf ("======================================================\n");
    std::printf ("  Hisstory De-Hisser Benchmark\n");
    std::printf ("======================================================\n");
    std::printf ("Project root : %s\n", projectRoot.getFullPathName().toRawUTF8());
    std::printf ("Track folder : %s\n", trackDir.getFullPathName().toRawUTF8());
    std::printf ("RX 11 VST3   : %s\n",
                 hasRX11 ? "FOUND" : "NOT FOUND (skipping comparison)");
    std::printf ("Output folder: %s\n\n", outputDir.getFullPathName().toRawUTF8());

    // Find all FLAC files
    juce::Array<juce::File> tracks;
    trackDir.findChildFiles (tracks, juce::File::findFiles, false, "*.flac");
    tracks.sort();

    if (tracks.isEmpty())
    {
        std::printf ("No FLAC files found in %s\n",
                     trackDir.getFullPathName().toRawUTF8());
        return 1;
    }

    std::printf ("Found %d track(s).\n\n", tracks.size());

    // Summary accumulators
    double totalNoiseRedHisttory = 0.0, totalNoiseRedRX = 0.0;
    double totalMidPresHisttory = 0.0, totalMidPresRX = 0.0;
    double totalHfRedHisttory = 0.0, totalHfRedRX = 0.0;
    int trackCount = 0;

    for (auto& trackFile : tracks)
    {
        std::printf ("──────────────────────────────────────────────────\n");
        std::printf ("Track: %s\n", trackFile.getFileNameWithoutExtension().toRawUTF8());

        juce::AudioBuffer<float> inputBuffer;
        double sampleRate;

        if (! readAudioFile (trackFile, inputBuffer, sampleRate))
            continue;

        std::printf ("  %d ch, %.0f Hz, %.1f sec (%d samples)\n",
                     inputBuffer.getNumChannels(), sampleRate,
                     inputBuffer.getNumSamples() / sampleRate,
                     inputBuffer.getNumSamples());

        auto inputMetrics = computeMetrics (inputBuffer, sampleRate);
        printMetrics ("Input", inputMetrics);

        // ── Process with Hisstory ────────────────────────────────────────
        std::printf ("  Processing with Hisstory...\n");
        auto hisstoryOut = processWithHisttory (inputBuffer, sampleRate);
        auto hisstoryMetrics = computeMetrics (hisstoryOut, sampleRate);
        printMetrics ("Hisstory Output", hisstoryMetrics);

        // Write output WAV
        juce::String baseName = trackFile.getFileNameWithoutExtension();
        writeWavFile (outputDir.getChildFile (baseName + "_hisstory.wav"),
                      hisstoryOut, sampleRate);

        // ── Process with RX 11 (if available) ────────────────────────────
        AudioMetrics rx11Metrics;
        bool hasRX11Result = false;

        if (hasRX11)
        {
            std::printf ("  Processing with RX 11 Voice De-noise...\n");
            auto rx11Out = processWithVST3 (inputBuffer, sampleRate,
                                            rx11Path.getFullPathName());
            rx11Metrics = computeMetrics (rx11Out, sampleRate);
            printMetrics ("RX 11 Output", rx11Metrics);

            writeWavFile (outputDir.getChildFile (baseName + "_rx11.wav"),
                          rx11Out, sampleRate);
            hasRX11Result = true;
        }

        // ── Comparison ───────────────────────────────────────────────────
        std::printf ("\n  COMPARISON:\n");
        printComparison ("Hisstory", inputMetrics, hisstoryMetrics);

        double noiseRedH = 20.0 * std::log10 (
            (hisstoryMetrics.quietRMS + 1e-20) / (inputMetrics.quietRMS + 1e-20));
        double midPresH = (inputMetrics.midBandEnergy > 1e-20)
            ? hisstoryMetrics.midBandEnergy / inputMetrics.midBandEnergy : 0.0;
        double hfRedH = (inputMetrics.hfEnergy > 1e-20)
            ? hisstoryMetrics.hfEnergy / inputMetrics.hfEnergy : 0.0;
        totalNoiseRedHisttory += noiseRedH;
        totalMidPresHisttory  += midPresH;
        totalHfRedHisttory    += hfRedH;

        if (hasRX11Result)
        {
            printComparison ("RX 11", inputMetrics, rx11Metrics);

            double noiseRedR = 20.0 * std::log10 (
                (rx11Metrics.quietRMS + 1e-20) / (inputMetrics.quietRMS + 1e-20));
            double midPresR = (inputMetrics.midBandEnergy > 1e-20)
                ? rx11Metrics.midBandEnergy / inputMetrics.midBandEnergy : 0.0;
            double hfRedR = (inputMetrics.hfEnergy > 1e-20)
                ? rx11Metrics.hfEnergy / inputMetrics.hfEnergy : 0.0;
            totalNoiseRedRX += noiseRedR;
            totalMidPresRX  += midPresR;
            totalHfRedRX    += hfRedR;
        }

        trackCount++;
        std::printf ("\n");
    }

    // ── Summary ──────────────────────────────────────────────────────────
    if (trackCount > 0)
    {
        std::printf ("======================================================\n");
        std::printf ("  AVERAGE ACROSS %d TRACKS\n", trackCount);
        std::printf ("======================================================\n");
        std::printf ("  Hisstory:  NoiseReduc=%+.1fdB  MidPreserv=%.3f  HfReduc=%.3f\n",
                     totalNoiseRedHisttory / trackCount,
                     totalMidPresHisttory / trackCount,
                     totalHfRedHisttory / trackCount);
        if (hasRX11)
        {
            std::printf ("  RX 11   :  NoiseReduc=%+.1fdB  MidPreserv=%.3f  HfReduc=%.3f\n",
                         totalNoiseRedRX / trackCount,
                         totalMidPresRX / trackCount,
                         totalHfRedRX / trackCount);
        }
        std::printf ("======================================================\n");
    }

    // Write input WAVs too for easy comparison
    for (auto& trackFile : tracks)
    {
        juce::AudioBuffer<float> buf;
        double sr;
        if (readAudioFile (trackFile, buf, sr))
        {
            writeWavFile (outputDir.getChildFile (
                trackFile.getFileNameWithoutExtension() + "_input.wav"), buf, sr);
        }
    }

    std::printf ("\nOutput files written to: %s\n",
                 outputDir.getFullPathName().toRawUTF8());
    return 0;
}
