# Hisstory Quick Start

Use this if you want results in a few minutes.

## 1) Install the Plugin

### macOS
Copy `Hisstory.vst3` to one of:
- `~/Library/Audio/Plug-Ins/VST3/` (just your user)
- `/Library/Audio/Plug-Ins/VST3/` (all users)

Then restart/rescan plugins in your DAW.

### Windows
Copy `Hisstory.vst3` to your DAW's VST3 folder (typically):
- `C:\Program Files\Common Files\VST3\`

Then restart/rescan plugins in your DAW.

## 2) Load Hisstory on the Noisy Track

- Insert **Hisstory** on the track or bus with hiss/noise.
- Start playback on a representative section.

## 3) Dial In Settings (Fast Method)

Start here:
- **Threshold**: `-23 dB`
- **Reduction**: `12 dB`
- **Adaptive**: ON

Then:
1. Raise **Threshold** slowly until hiss drops clearly.
2. Raise **Reduction** only as needed.
3. Use **Bypass** to A/B and keep the result natural.

## 4) If You Hear Artifacts

- Sound dull/lispy: lower Threshold (more negative) or lower Reduction.
- Warbly/chirpy: back off both controls slightly.
- Not enough cleanup: raise Threshold a bit and/or add a little Reduction.

## 5) Final Check

- A/B with **Bypass** at matched listening level.
- Keep the setting where noise is reduced but tone still feels natural.

## Useful Controls

- **Adaptive**: continuously tracks changing noise floor.
- **Spectrogram**: visual analyzer mode.
- **?** button: in-plugin control/metric help.

## Need More Detail?

See the full guide: [`USER_GUIDE.md`](USER_GUIDE.md)
