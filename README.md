# HARDFLIP — Clipper  (by BUYORBEALOSER)

A soft / hard clipper with a skater that rides the transfer curve, jumps off a wooden kicker and lands one trick per run.
VST3 + AU (macOS, universal) and VST3 (Windows).

## Install on macOS (FL Studio)

1. Open the **Actions** tab, click the latest green run, download the **HARDFLIP-macOS** artifact and unzip it.
2. Copy `HARDFLIP.vst3` to `/Library/Audio/Plug-Ins/VST3/` (or `~/Library/Audio/Plug-Ins/VST3/`)
   and `HARDFLIP.component` to `/Library/Audio/Plug-Ins/Components/`.
3. The build is ad-hoc signed, not notarized. Remove the download quarantine flag:
   ```
   xattr -dr com.apple.quarantine /Library/Audio/Plug-Ins/VST3/HARDFLIP.vst3
   xattr -dr com.apple.quarantine /Library/Audio/Plug-Ins/Components/HARDFLIP.component
   ```
4. FL Studio: *Options → Manage plugins → Find plugins*. HARDFLIP appears under *Effects*. Use the **VST3** version first.

For public distribution you will need an Apple Developer ID (signing + notarization).

## Controls

| Control | What it does |
|---|---|
| **SOFT / HARD** | SOFT (default): unity below THRESH, then a tanh knee that bends into a 0 dBFS ceiling. HARD: flat brick wall at THRESH. Switching is crossfaded (15 ms), no click. |
| **DRIVE** | 0 … +24 dB of gain before the clipper (pushes the sound into it). |
| **THRESH** | −24.4 … 0 dB, where the clip starts. |
| **POST** | −12 … 0 dB output gain after the clipper. |
| **MIX** | 0 … 100 % dry/wet. The dry signal is the original (before DRIVE), delayed by the oversampling latency so mixing never comb-filters. |
| **BYPASS** | Fades to the latency-aligned dry signal (also exposed as the host bypass). |
| Presets | Gentle, Punchy, Brick Wall, 808 Smash, Parallel Drums, Default Setting (opens on Default Setting). |
| Skater | ON / OFF. In SOFT the trick depends on the slope at the kicker, in HARD on the height of the drop. |

Every control is a host parameter (automation, state saved with the project).

## Audio engine

* Clip runs at **4x oversampling** (linear-phase half-band FIR), latency reported to the host.
* All parameters are smoothed (20 ms), so automation and knob moves do not zipper.
* NaN / infinite input is silenced.
* The curve formulas live in `Source/DSP/ClipDsp.h`; the UI draws the exact same formulas.

Unit tests (`Tests/dsp_test.cpp`): unity below threshold, monotonic, symmetric, continuous, HARD never above THRESH,
SOFT never above 0 dBFS (input up to +12 dBFS), mode crossfade stays between the two curves, NaN/inf safety.

```
g++ -std=c++17 -O2 Tests/dsp_test.cpp -o dsp_test && ./dsp_test
```

## Build locally

Requires CMake ≥ 3.22 and Xcode (macOS) or Visual Studio 2022 (Windows). JUCE 8.0.8 is fetched automatically.

```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --target HARDFLIP_VST3 HARDFLIP_AU
```

The interface is `UI/index.html` (HTML/JS, embedded in the binary). Opened in a normal browser it runs as a preview
with its own audio player (drop a file or use the demo loop); inside the plugin it talks to the processor instead.

## Licensing — read before selling

* **JUCE** is AGPLv3 **or** a commercial license. Selling a closed-source plugin built with JUCE requires a JUCE commercial license (https://juce.com/get-juce).
* **VST3** is a Steinberg trademark/SDK: check the current VST3 license terms (free) before distributing.
* Font *Permanent Marker* is SIL Open Font License (downloaded at configure time from google/fonts).
* Check that the name **HARDFLIP** is free (other plugins / trademarks) before selling.
