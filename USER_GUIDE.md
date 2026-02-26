# Hisstory User Guide

This guide explains both:
- how Hisstory removes hiss/noise under the hood
- how to use the controls in real sessions

## What Hisstory Does

Hisstory is a real-time de-hisser. It is designed to reduce constant broadband noise (especially high-frequency hiss) while keeping useful program material (voice, instruments, transients) as natural as possible.

It works best on:
- tape hiss
- preamp/interface self-noise
- steady background broadband noise

It is less effective on:
- intermittent noises (clicks, pops, handling noise)
- tonal hum/buzz with strong harmonics
- fast-changing environmental noise

## How the Noise Removal Process Works

Hisstory uses an STFT spectral-gating pipeline.

1. **Time-to-frequency analysis**
   - Audio is split into overlapping windows.
   - Each window is transformed into frequency bins.

2. **Noise floor estimation**
   - The plugin maintains a noise estimate per frequency bin.
   - In **Adaptive** mode, the estimate updates during quieter moments.

3. **Threshold generation**
   - A global **Threshold** plus the 6-band curve creates a per-bin threshold.
   - This threshold determines which spectral bins are likely noise.

4. **Gain computation (soft gate)**
   - Bins below threshold are attenuated by up to **Reduction** dB.
   - A soft transition is used instead of hard on/off switching to reduce artifacts.

5. **Smoothing**
   - Frequency and temporal smoothing reduce "musical noise" (chirpy artifacts).

6. **Reconstruction**
   - Processed bins are transformed back to time-domain and overlap-added.

7. **Bypass transition smoothing**
   - Bypass transitions are crossfaded to avoid clicks/pops when toggling.

## UI Overview

### Top Controls

- **Spectrogram**: toggles spectrogram display mode in the analyzer.
- **Adaptive**: enables continuous noise-floor adaptation.
- **Bypass**: passes dry signal while keeping analyzer behavior coherent.
- **<< / >>**: collapses/expands the analyzer section.
- **?**: opens help with slider/metric descriptions.

### Main Sliders

- **Threshold [dB]** (`-40` to `-10`, default `-23`)
  - Higher (less negative) values = more aggressive detection/removal.
  - Lower (more negative) values = more conservative processing.
  - You can type values directly in the value box.
  - Entering a positive number is interpreted as a negative threshold value.

- **Reduction [dB]** (`0` to `32`, default `12`)
  - Maximum attenuation applied to bins detected as noise.
  - Higher values remove more hiss but can introduce artifacts or dullness.
  - You can type values directly in the value box.

### Threshold Curve (6-band)

The threshold curve lets you bias sensitivity by frequency region.

- Raise a band to remove more in that region.
- Lower a band to preserve more in that region.
- Use this to target hiss-heavy highs while preserving vocal/instrument mids.

## Recommended Workflow

1. **Start conservative**
   - Threshold around `-24 to -20 dB`
   - Reduction around `8 to 14 dB`
   - Adaptive ON (default)

2. **Loop a representative section**
   - Include both quieter and louder passages if possible.

3. **Set Threshold first**
   - Increase until hiss is meaningfully reduced.
   - Stop before you hear dullness, lisping, or pumping.

4. **Add Reduction second**
   - Increase only as much as needed after threshold is set.

5. **Refine with 6-band curve**
   - If highs are still noisy, lift upper bands.
   - If vocal/instrument body is thinning, lower mid bands.

6. **A/B with Bypass**
   - Level-match by ear while comparing.
   - Keep the "cleaned" version sounding natural, not just quieter.

7. **Watch metrics + listen**
   - Use metrics as guidance, but trust audible quality first.

## Understanding the Metrics

- **HF REMOVED**
  - High-frequency energy removed (dB).
  - Higher value means stronger high-end cleanup.

- **MID PRESERVED**
  - Midrange impact indicator (200-3000 Hz context).
  - If this suggests too much mid removal, reduce threshold/reduction.

- **OUTPUT LEVEL**
  - Overall level change between input and output.
  - Large drops can make processing seem "better" due to loudness bias.

- **HARMONIC LOSS**
  - Percent tonal energy lost during processing.
  - Lower is generally better.
  - Rising values can indicate over-processing.

## Quick Presets (Starting Points)

### Light cleanup (podcast/voice)
- Threshold: `-26 dB`
- Reduction: `8-10 dB`
- Adaptive: ON

### Moderate hiss reduction
- Threshold: `-23 dB`
- Reduction: `12-16 dB`
- Adaptive: ON

### Aggressive rescue pass
- Threshold: `-18 to -15 dB`
- Reduction: `18-24 dB`
- Adaptive: ON
- Then back off until artifacts disappear.

## Troubleshooting

- **Sound is dull or lispy**
  - Lower Threshold (more negative), or lower Reduction.
  - Reduce upper-band aggressiveness in the threshold curve.

- **Warbly/chirpy artifacts**
  - Back off Threshold and Reduction slightly.
  - Favor more selective curve shaping instead of global aggression.

- **Not enough hiss removal**
  - Raise Threshold slightly (less negative).
  - Increase Reduction in small steps.
  - Lift upper frequency bands in the curve.

- **Bypass comparison feels jumpy**
  - Toggle once and wait a moment while listening to a steady section.
  - Use matched monitoring level for fair A/B.

## Installation (macOS VST3)

1. Copy `Hisstory.vst3` into:
   - `~/Library/Audio/Plug-Ins/VST3/` (current user), or
   - `/Library/Audio/Plug-Ins/VST3/` (all users)
2. Restart or rescan plugins in your DAW.
3. If blocked by Gatekeeper, open once via right-click **Open** or remove quarantine attribute.

## Practical Tips

- De-hiss in context, not solo only.
- Do a final bypass check at matched loudness.
- Slightly under-processing often sounds more natural than heavy removal.
- For difficult material, automate settings by section instead of one static setting.
