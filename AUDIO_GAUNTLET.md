# Longland Conservation Audio Gauntlet

This plan separates claims that can be proven by automated signal tests from checks
that require a host, a built plug-in, or listening. A result must not be marked as
passed unless its command, configuration, and measurement are recorded in
`CONSERVATION_GAUNTLET_RESULTS.md`.

## Safety invariants

For every rendered sample and every control/snapshot update:

- output samples are finite;
- peak output is bounded (initial hard test limit: `<= 2.0`, then tighten after gain
  staging is tuned);
- no reservoir is negative or above the current total budget;
- reservoir sum matches total budget within the solver tolerance;
- feedback and momentum decay or remain in a deliberately bounded orbit;
- identical automation produces approximately identical physics per second at all
  supported block sizes.

The `2.0` sample peak threshold is a crash/runaway sentinel, not an assertion that
inter-sample peaks or output gain are production-ready. Final loudness and true-peak
acceptance criteria require calibrated programme material.

## Deterministic offline matrix

Run each case at 44.1, 48, 88.2, and 96 kHz, mono and stereo where supported, and
block sizes 1, 7, 16, 31, 64, 127, 256, 511, 1024, and a changing sequence
`64, 511, 7, 1024, 31`. The odd sizes catch assumptions hidden by powers of two.

Inputs:

- digital silence, including at least 30 seconds after a high-feedback excitation;
- a single full-scale impulse, plus impulses immediately before and after block
  boundaries;
- full-scale DC and alternating `+1/-1` samples;
- logarithmic sine sweep and stepped sines near crossover/delay interpolation
  boundaries;
- low-level (`-150 dBFS`) noise to expose denormal CPU problems;
- correlated mono, anti-correlated stereo, side-only, and hard-panned signals;
- clipped/full-mastered programme material for listening and true-peak inspection.

Automation:

- every parameter at minimum, maximum, and neutral;
- simultaneous min/max toggles every block;
- deterministic ramps whose control values are sampled at fixed wall-clock times;
- TIME/reflection held at maximum until all preferred donors are exhausted;
- high instability with low friction, then a discontinuous switch to high friction;
- sample-rate and block-size re-prepare between active renders.

## Click and discontinuity tests

For constant or smoothly varying input, compute first differences of output and compare
against a no-automation reference. Flag discontinuities that exceed a calibrated
threshold; do not use one absolute threshold for impulses or transient-rich programme
material. A useful first automated screen is:

1. render a `-18 dBFS`, 997 Hz sine;
2. automate one force at a time using worst-case steps;
3. exclude expected zero-crossing-independent nonlinear residue from the comparison;
4. report maximum derivative, short-window crest factor, and the exact sample index;
5. listen around every flagged index.

Explicitly test delay-time interpolation, bypass transitions, state restore, host
transport start/stop, and prepare/release transitions. A click-free parameter smoother
does not prove click-free delay pointer movement.

## Feedback and runaway tests

Excite the system with one impulse and then render silence for at least ten times the
maximum nominal decay. Track peak and RMS in one-second windows. Fail if any of the
following occurs:

- a non-intentional window grows after the excitation has ended;
- a sample is non-finite or exceeds the hard safety limit;
- energy remains stuck because a smoothed target never reaches its destination;
- output noise appears on digital silence without an explicitly enabled noise source;
- DC accumulates materially during saturation/feedback processing.

Repeat with low friction/high instability and with resource exhaustion. Physics energy
conservation alone does not bound DSP feedback: every feedback gain and nonlinear loop
needs an independent audio-domain bound.

## Aliasing and spectral checks

At each sample rate, feed high-frequency stepped sines through maximum Heat/Pressure and
pitch-up configurations. Compare the residual spectrum against an oversampled or
high-sample-rate reference. Record alias level rather than claiming it is inaudible.
Pitch/time algorithms also need tests for image rejection, transient smearing, and
latency consistency. These are not satisfied by conservation tests.

## Stereo checks

Measure left/right equality for dual-mono input, mono sum level, side gain, channel
crosstalk, and correlation. Extreme Dispersion must remain mono-compatible by design:
width may weaken centre energy, but must not cause arbitrary polarity inversion or a
large mono-sum disappearance unless the preset explicitly documents that behaviour.

## CPU, allocation, and long-run checks

- instrument or override allocation during `processBlock`; any allocation fails;
- collect median, p99, and worst block time, not just average CPU;
- run release builds with denormal input, maximum delay/reverb state, maximum supported
  channels, and at least 32 independent instances;
- run sanitizers in a standalone/offline harness where available;
- run an eight-hour deterministic soak and record resident memory, maximum conservation
  error, non-finite count, maximum sample peak, and CPU percent at fixed intervals;
- verify snapshot publication is wait-free for the audio writer and the UI never reads
  a torn state.

The eight-hour test remains **not run** until an actual timed result is recorded.

## Buffer-size invariance

Compare snapshots at matching wall-clock timestamps, not matching block counts. With
identical input and automation, render each block-size configuration and report the
maximum per-reservoir divergence and audio null/residual. Exact audio equality may not
be appropriate for time/pitch algorithms, but solver trajectories must remain within a
declared tolerance. Any update such as `x += ratePerBlock` is presumed defective unless
it explicitly incorporates elapsed seconds.

## Required automated tests

1. Conservation over at least one million deterministic randomized control updates.
2. Direct-to-reflected transfer and donor exhaustion.
3. Transient/sustain opposition and spectral donor loss.
4. Elastic return rate and friction damping measured in seconds.
5. Instability remains finite and bounded under adversarial automation.
6. Snapshot coherence under concurrent writer/reader stress.
7. DSP impulse response remains finite under maximum reflection settings.
8. Silence produces silence (within an explicitly documented noise floor).
9. Parameter-step click screen and delay-time interpolation test.
10. Sample-rate and block-size invariance matrix.
11. Multiple engine/processor instances do not share mutable state.
12. Re-prepare/reset clears or deliberately preserves state exactly as documented.

## Manual host/format checklist

Validate every produced format with the current official validator and at least two
hosts. Check automation recording/playback, preset/state round-trip, bypass, latency
reporting, mono/stereo layouts, rapid GUI open/close, resizing, sample-rate changes,
block-size changes, offline bounce, transport jumps, and 32-instance stress. Record
host/version/OS and do not infer format compliance from the standalone application.

## Evidence template

For each run record:

```text
date / commit:
build type / compiler:
format / host:
sample rate / block sizes / channels:
input and automation seed:
duration:
max conservation error:
min/max reservoir:
max sample peak / true peak / DC:
non-finite count:
CPU median / p99 / worst:
memory start/end:
result and artifact paths:
```

