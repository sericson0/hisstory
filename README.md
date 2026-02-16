# Hisstory De-Hisser

A real-time JUCE-based VST3/Standalone de-hiss plugin using **spectral gating with soft-knee spectral subtraction**. Inspired by iZotope RX Voice De-noise.

## Features

- **STFT Spectral Processing** – 2048-point FFT with 75% overlap (Hann window, WOLA framework)
- **Noise Profile Learning** – Press "Learn" while noise-only audio plays to capture the noise spectrum
- **Adaptive Mode** – Continuously updates the noise profile during quiet passages
- **6-Band Threshold Curve** – Drag control points to shape the noise detection threshold per frequency region
- **Soft-Knee Spectral Gate** – Smoothstep transition prevents hard on/off switching artefacts
- **Temporal + Frequency Smoothing** – Suppresses "musical noise" (bird-chirping) artefacts
- **Dialogue / Music Modes** – Dialogue mode uses tighter frequency resolution; Music mode applies wider smoothing to preserve harmonics
- **Surgical / Gentle Filter Types** – Controls the transition width of the spectral gate
- **Dark-Themed GUI** – Real-time spectrum display with input, output, and threshold curves

## Parameters

| Parameter | Range | Default | Description |
|-----------|-------|---------|-------------|
| Threshold | -40 to +10 dB | -13.0 | Global noise detection sensitivity |
| Reduction | 0 to 40 dB | 12.0 | Amount of noise attenuation |
| Smoothing | 0 to 100% | 50 | Temporal smoothing of gain values |
| Band 1–6 | -30 to +30 dB | 0.0 | Per-band threshold offset |
| Learn | On/Off | Off | Noise profile learning |
| Adaptive | On/Off | Off | Continuous profile adaptation |
| Bypass | On/Off | Off | Bypass all processing |

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

## Algorithm Overview

1. **STFT Analysis** – Input audio is windowed (Hann) and transformed via FFT into frequency bins
2. **Noise Estimation** – Learned or adaptive noise profile provides expected noise magnitude per bin
3. **Threshold Evaluation** – Per-bin threshold = `noise_profile[bin] × 10^((global_threshold + band_offset) / 20)`
4. **Spectral Gating** – Each bin's gain is computed via a smoothstep function:
   - Below threshold: gain = `10^(-reduction_dB / 20)` (maximum attenuation)
   - Above threshold: gain ramps to 1.0 over the transition width
   - A spectral floor of -80 dB prevents complete zeroing
5. **Smoothing** – 3-tap (or 5-tap for Music mode) frequency smoothing + exponential temporal smoothing
6. **STFT Synthesis** – IFFT, synthesis window (Hann), and overlap-add reconstruction

## License

MIT
