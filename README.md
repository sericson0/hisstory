# Hisstory De-Hisser

A real-time JUCE-based VST3/Standalone de-hiss plugin using **spectral gating with soft-knee spectral subtraction**. Inspired by iZotope RX Voice De-noise.

## Documentation

- Quick start: [`QUICK_START.md`](QUICK_START.md)
- User guide: [`USER_GUIDE.md`](USER_GUIDE.md)

## Features

- **Real-time STFT De-Hiss Engine** - 2048-point FFT, 75% overlap, Hann window, overlap-add synthesis
- **Adaptive Noise Tracking** - Continuously updates the noise floor during quieter passages
- **6-Band Threshold Curve** - Frequency-dependent threshold shaping for targeted cleanup
- **Soft-Knee Spectral Gating** - Smooth attenuation transitions to reduce artifacts
- **Temporal + Frequency Smoothing** - Helps suppress "musical noise" artifacts
- **Analyzer + Spectrogram View** - Real-time visual feedback for input/output behavior
- **Live Quality Metrics** - HF Removed, Mid Preserved, Output Level, Harmonic Loss
- **Glitch-Reduced Bypass Switching** - Short crossfade on bypass transitions to avoid clicks/pops
- **Cross-Platform Builds** - VST3 + Standalone targets for Windows and macOS

## Parameters

| Parameter | Range | Default | Description |
|-----------|-------|---------|-------------|
| Threshold | -40 to -10 dB | -23.0 | Global noise detection sensitivity |
| Reduction | 0 to 32 dB | 12.0 | Amount of attenuation applied to detected noise |
| Smoothing* | 0 to 100% | 50.0 | Temporal smoothing amount in the DSP engine |
| Band 1–6 | -30 to +30 dB | per-band defaults | Per-band threshold offset |
| Adaptive | On/Off | On | Continuous profile adaptation |
| Bypass | On/Off | Off | Bypass all processing |

\* `Smoothing` is part of the processing parameter set and can be automated/host-managed.

## Building

### Prerequisites

- **CMake** 3.22 or later
- **C++17** compatible compiler (MSVC 2019+, GCC 9+, Clang 10+)
- **Git** (JUCE is fetched automatically via FetchContent)

### Build Steps

```bash
# Clone the repository
git clone <repo-url> hisstory-vst
cd hisstory-vst

# Configure (JUCE is downloaded automatically)
cmake -B build -DCMAKE_BUILD_TYPE=Release

# Build
cmake --build build --config Release

# The VST3 plugin will be in:
#   build/HisstoryVST_artefacts/Release/VST3/
# The standalone app will be in:
#   build/HisstoryVST_artefacts/Release/Standalone/
```

### Windows (Visual Studio)

```powershell
cmake -B build -G "Visual Studio 17 2022"
cmake --build build --config Release
```

### macOS (Xcode)

```bash
cmake -B build -G Xcode
cmake --build build --config Release
```

## Processing Overview

1. **STFT Analysis** – Input audio is windowed (Hann) and transformed into frequency bins
2. **Noise Estimation** – Adaptive profile estimates expected noise magnitude per bin
3. **Threshold Evaluation** – Per-bin threshold = `noise_profile[bin] * 10^((global_threshold + band_offset) / 20)`
4. **Spectral Gating** – Each bin's gain is computed via a smoothstep function:
   - Below threshold: gain = `10^(-reduction_dB / 20)` (maximum attenuation)
   - Above threshold: gain ramps to 1.0 over the transition width
   - A spectral floor of -80 dB prevents complete zeroing
5. **Smoothing** – Frequency smoothing + exponential temporal smoothing
6. **STFT Synthesis** – IFFT, synthesis window (Hann), and overlap-add reconstruction
7. **Bypass Transition Handling** – Short dry/wet crossfade reduces audible switching artifacts

## License

MIT
