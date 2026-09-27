# Conservation gauntlet results

## Build under test

```text
Date: 2026-08-31 (Australia/Brisbane)
Version: 0.1.0 prototype
JUCE: 9.0.0
Compiler: MSVC 19.51.36252.0, x64
Configuration: Release
Format: VST3 only
```

The JUCE VST3 manifest helper successfully loaded the built module and generated
`moduleinfo.json` identifying VST SDK 3.8.0. No Standalone target was produced.

## Automated results

Command:

```powershell
ctest --test-dir build-release --output-on-failure
```

Final result: **5/5 passed** in 28.61 seconds (including the million-update
physics torture run).

### conservation-physics — passed

- Exact Direct-to-Reflected transfer and donor exhaustion.
- Illegal graph edge rejection and corrupt-state recovery.
- TIME and PRESSURE directional consequences.
- Elastic return and friction-dependent coasting.
- 1,000,000 aggressive automation updates with a conservation assertion after every
  update, including changing time steps, maximum instability, and changing budget.
- Deterministic instability, control-rate consistency, and independent instances.

The test threshold is `abs(sum - budget) < 1e-9`. Maximum observed error in the
final Release run: `3.33066907387546962e-15`.

Packaged VST3 ZIP SHA-256:
`5D53B942BB62653D03B5B05D8AE798778C193A1B1FE417F7615EDC0165ECAA0B`.

### dsp-mapper — passed

- Analytic impulse-tail conservation and feedback/first-echo trade.
- Monotonic dry recession as Reflected increases.
- Direct/delay/reverb partition sum.
- Attenuation-only transient/sustain, spectral, harmonic, and spatial costs.
- Zero budget and NaN/Inf recovery.
- 100,000 randomized valid ledgers mapped to finite bounded targets.

### adversarial-dsp — passed

- Finite and sample-peak-bounded impulse render at 44.1, 48, 88.2, and 96 kHz.
- Identical impulse trajectory across block sizes 1, 7, 31, 64, 127, 256, 511,
  and 1024 within `1e-7` maximum sample difference.
- NaN/Inf target and input sanitization.
- Digital silence remains digital silence.
- Analytic reflection feedback cap and tail-energy proof.
- End-to-end proof that Harmonic and Spatial ledger displacement changes rendered
  programme audio rather than only changing GUI/mapping state.

### plugin-smoke — passed

- Instantiated the actual processor at 44.1, 48, 88.2, and 96 kHz.
- Exercised block sizes 1, 7, 31, 64, 127, 256, 511, and 1024 with simultaneous
  TIME, PRESSURE, HEAT, DISPERSION, low FRICTION, and maximum INSTABILITY.
- Verified finite/sample-bounded output and conserved coherent snapshots.
- Verified zero-length callbacks, deterministic state recall after different prior
  histories, independent instances, and the 65-second host tail contract.
- Rendered the real JUCE editor and verified non-black control/visualizer content.

### vst3-load — passed

- Scanned the built `.vst3` bundle through JUCE 9's VST3 host implementation.
- Discovered and instantiated the exported plug-in type from the bundle.
- Verified at least ten force parameters, editor creation/layout, tail declaration,
  and a finite bounded impulse render through the loaded VST3.

## Known limits and tests not claimed

- No eight-hour soak was run.
- No third-party DAW host matrix or external VST validator was run; the actual bundle
  was scanned, instantiated, opened, and rendered by the JUCE VST3 host harness.
- No true-peak measurement was run; the current safety stage is a zero-lookahead
  sample-peak limiter and may add discontinuity on hostile correlated peaks.
- No listening panel, psychoacoustic loudness calibration, CPU percentile capture,
  allocation instrumentation, sanitizer run, or 32-instance host stress was run.
- Delay-time changes currently use a smoothed fractional read head, so Doppler is an
  intentional prototype limitation; dual-tap crossfading is not implemented.
- Transient/sustain, attenuation-only three-band redistribution, bounded harmonic
  shaping, and M/S centre/side costs are rendered. Reverb/FDN, autonomous noise,
  oversampled saturation, time stretch, and pitch shift remain deferred.
- Host recall deliberately restores controls and resets the dynamic system to the
  restored budget's equilibrium; exact reservoir/momentum resume is not implemented.
- Presets are not yet implemented. This prototype proves Phases 0–7 at prototype
  depth and the real
  Energy Field/debug pipeline; it is not represented as the finished AAA product.

See `AUDIO_GAUNTLET.md` for the full host, long-run, aliasing, stereo, and CPU matrix
required before a production release.
