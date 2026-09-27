# Longland Conservation

Longland Conservation is an experimental audio effect built around a finite,
explicit musical-energy ledger. Controls apply forces to that ledger; they do
not independently turn effects up. Any reservoir gain is funded by legal donor
reservoirs, and an exhausted donor limits the requested transformation.

This repository starts with the most important proof: a JUCE-independent C++20
physics engine, property-style tests, a bounded direct/reflected DSP path, and a
JUCE shell whose visualizer reads the same snapshots that drive the audio.

## Current scope

- Eleven non-negative reservoirs with a configurable conserved budget.
- A fixed legal transfer graph and donor-aware resource exhaustion.
- Momentum, equilibrium attraction, friction, inertia, and bounded instability.
- Per-cycle validation, residual correction, and safe recovery from invalid state.
- Block-rate controls with sample-rate-independent physics timing.
- A reflection processor whose wet gain, feedback, damping, and dry recession are
  derived from the ledger.
- Attenuation-only transient/sustain, three-band, harmonic, and mid/side
  consequence stages driven by the same ledger.
- A live Energy Field and numerical debug panel sourced from real physics state.

The current DSP proves the direct/reflected conservation experience and makes the
major non-time forces audible, but it is not a finished multi-effect. Reverb,
pitch shifting, and time stretching are deferred until algorithms and their
audible conservation costs can be validated.

## Build the core tests

From a Visual Studio Developer PowerShell:

```powershell
cmake -S . -B build -DLONGLAND_BUILD_PLUGIN=OFF
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

## Build the plugin

Install or clone JUCE 9, then configure with its directory:

```powershell
cmake -S . -B build -DLONGLAND_JUCE_PATH=C:/SDKs/JUCE
cmake --build build --config Release
```

The only plugin format is VST3. The audio thread performs no file
I/O, locking, graph rebuilds, or allocation after `prepareToPlay`.

The ready-to-use Windows x64 bundle is in `dist/`. Extract the ZIP and copy the
`Longland Conservation.vst3` directory into a VST3 folder scanned by your DAW.

## Honest status

See `CONSERVATION_GAUNTLET_RESULTS.md` for exactly what was executed in this
environment. No eight-hour run, DAW validation, or listening result is claimed
unless recorded there.
