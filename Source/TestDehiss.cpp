/*
  ==============================================================================
    TestDehiss.cpp – offline sanity-check for the spectral gate.

    Test 1 (Signal + Noise):
      • 1 kHz sine at −20 dBFS  +  white noise at −40 dBFS
      • Verify: no gain boost, signal preserved

    Test 2 (Noise only):
      • Pure white noise at −30 dBFS
      • Verify: significant noise reduction
  ==============================================================================
*/

#include "PluginProcessor.h"
#include <cstdio>
#include <cmath>
#include <cstdlib>
#include <vector>

//==============================================================================
struct TestResult
{
    double inRMS, outRMS, inPeak, outPeak, diffDB;
    bool   pass;
};

static TestResult runTest (const char* name, const std::vector<float>& testL,
                           int totalSamples, int skipSamples)
{
    HisstoryAudioProcessor proc;
    constexpr double sampleRate = 44100.0;
    constexpr int    blockSize  = 512;

    proc.setPlayConfigDetails (1, 1, sampleRate, blockSize);
    proc.prepareToPlay (sampleRate, blockSize);

    const int numBlocks = totalSamples / blockSize;
    std::vector<float> outL (totalSamples, 0.0f);
    juce::MidiBuffer midi;

    for (int b = 0; b < numBlocks; ++b)
    {
        juce::AudioBuffer<float> buf (1, blockSize);
        auto* L = buf.getWritePointer (0);

        for (int i = 0; i < blockSize; ++i)
            L[i] = testL[b * blockSize + i];

        proc.processBlock (buf, midi);

        for (int i = 0; i < blockSize; ++i)
            outL[b * blockSize + i] = L[i];
    }

    // ── Analyse ──────────────────────────────────────────────────────────────
    double inRMS = 0.0, outRMS = 0.0;
    double inPeak = 0.0, outPeak = 0.0;

    for (int i = skipSamples; i < totalSamples; ++i)
    {
        double s = testL[i];
        double o = outL[i];

        inRMS  += s * s;
        outRMS += o * o;

        if (std::abs (s) > inPeak)   inPeak  = std::abs (s);
        if (std::abs (o) > outPeak)  outPeak = std::abs (o);
    }

    const int n = totalSamples - skipSamples;
    inRMS  = std::sqrt (inRMS  / n);
    outRMS = std::sqrt (outRMS / n);

    const double inDB  = 20.0 * std::log10 (inRMS  + 1e-20);
    const double outDB = 20.0 * std::log10 (outRMS + 1e-20);
    const double diffDB = outDB - inDB;

    std::printf ("\n=== %s ===\n", name);
    std::printf ("Input  RMS : %.6f  (%.1f dB)   peak %.6f\n", inRMS,  inDB,  inPeak);
    std::printf ("Output RMS : %.6f  (%.1f dB)   peak %.6f\n", outRMS, outDB, outPeak);
    std::printf ("Change     : %+.2f dB\n", diffDB);

    // ── Verdict ──────────────────────────────────────────────────────────────
    bool pass = true;

    if (outPeak > inPeak * 1.05)
    {
        std::printf ("FAIL: peak gain increase (%.1f%% above input)\n",
                     (outPeak / inPeak - 1.0) * 100.0);
        pass = false;
    }

    if (diffDB > 0.5)
    {
        std::printf ("FAIL: RMS gain increase (+%.1f dB)\n", diffDB);
        pass = false;
    }

    if (outRMS < inRMS * 0.001)
    {
        std::printf ("FAIL: output is near-silent – signal was destroyed\n");
        pass = false;
    }

    return { inRMS, outRMS, inPeak, outPeak, diffDB, pass };
}

//==============================================================================
int main()
{
    constexpr double sampleRate   = 44100.0;
    constexpr int    blockSize    = 512;
    constexpr int    numBlocks    = 600;           // ≈ 7 seconds
    constexpr int    totalSamples = numBlocks * blockSize;
    constexpr int    skip         = HisstoryAudioProcessor::fftSize + 4096;

    std::srand (42);

    // ── Test 1: sine + noise ─────────────────────────────────────────────────
    std::vector<float> sig1 (totalSamples);
    for (int i = 0; i < totalSamples; ++i)
    {
        const float t     = static_cast<float> (i) / static_cast<float> (sampleRate);
        const float sine  = 0.1f * std::sin (2.0f * 3.14159265f * 1000.0f * t);
        const float noise = (static_cast<float> (std::rand()) / RAND_MAX * 2.0f - 1.0f)
                            * 0.01f;
        sig1[i] = sine + noise;
    }

    auto r1 = runTest ("Signal + Noise (sine -20 dBFS, noise -40 dBFS)", sig1,
                        totalSamples, skip);

    // ── Test 2: pure noise ───────────────────────────────────────────────────
    std::vector<float> sig2 (totalSamples);
    std::srand (99);
    for (int i = 0; i < totalSamples; ++i)
    {
        sig2[i] = (static_cast<float> (std::rand()) / RAND_MAX * 2.0f - 1.0f) * 0.03f;
    }

    auto r2 = runTest ("Pure Noise (-30 dBFS)", sig2, totalSamples, skip);

    // ── Test 3: pure silence ─────────────────────────────────────────────────
    std::vector<float> sig3 (totalSamples, 0.0f);
    auto r3 = runTest ("Silence", sig3, totalSamples, skip);

    // ── Summary ──────────────────────────────────────────────────────────────
    std::printf ("\n================= SUMMARY =================\n");

    bool allPass = true;

    // Test 1: no gain boost
    if (r1.pass && r1.diffDB <= 0.5)
        std::printf ("Test 1: PASS  (no gain boost: %+.2f dB)\n", r1.diffDB);
    else
    { std::printf ("Test 1: FAIL\n"); allPass = false; }

    // Test 2: noise should be reduced
    if (r2.pass && r2.diffDB < -1.0)
        std::printf ("Test 2: PASS  (noise reduced by %.1f dB)\n", -r2.diffDB);
    else if (r2.diffDB >= -1.0)
    { std::printf ("Test 2: FAIL  (noise only reduced by %.1f dB – expected > 1 dB)\n",
                   -r2.diffDB); allPass = false; }
    else
    { std::printf ("Test 2: FAIL\n"); allPass = false; }

    // Test 3: silence in → silence out
    if (r3.pass && r3.outPeak < 0.0001)
        std::printf ("Test 3: PASS  (silence preserved)\n");
    else
    { std::printf ("Test 3: FAIL  (output peak = %.8f)\n", r3.outPeak); allPass = false; }

    std::printf ("===========================================\n");
    std::printf ("Overall: %s\n", allPass ? "ALL PASS" : "SOME FAILED");

    return allPass ? 0 : 1;
}
