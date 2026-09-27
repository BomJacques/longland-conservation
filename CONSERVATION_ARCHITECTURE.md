# Conservation architecture

## Model boundary

The ledger is an artificial musical conservation model, not an estimate of
physical acoustic joules. Its contract is precise: all eleven reservoir values
are finite and non-negative, and their sum equals the active budget. DSP modules
read this state; they cannot write it or create a separate energy source.

The reservoirs are Direct, Reflected, Transient, Sustain, Low, Mid, High,
Harmonic, Noise, Spatial, and Temporal. The neutral normalized equilibrium is:

```text
Direct .30   Reflected .05   Transient .10  Sustain .10
Low .08      Mid .12         High .08       Harmonic .04
Noise .02    Spatial .05     Temporal .06
```

The ENERGY control scales the budget in `[0, 1]`; it does not change the
relative conservation rule.

## Legal transfers and exhaustion

`EnergyTransferGraph` is a fixed, authored, bidirectional graph. A request names
a destination, amount, preferred donors, transfer curve, and whether graph-legal
fallback donors may participate. `ConservationSolver` shapes the request, drains
only available legal donors, credits exactly the transferred amount, and reports
applied, partial, exhausted, invalid-request, or invalid-state status.

Resource exhaustion is therefore structural. If Direct reaches zero, a
Direct-only request for more Reflected energy transfers nothing. It cannot silently
turn up an independent delay mix.

## Forces

Forces accelerate legal flows rather than assigning effect values:

- MASS: High to Low, Transient to Sustain, Spatial to Low.
- TIME: Direct to Reflected/Temporal, Transient to Temporal.
- GRAVITY: High to Low, Spatial to Direct, Transient to Sustain.
- PRESSURE: Spatial/Direct to Harmonic, Transient to Mid.
- HEAT: Direct to Harmonic, Transient to Noise, Low to High.
- DISPERSION: Direct to Spatial, Transient to Reflected, Mid to High.

The current phase deliberately leaves pitch/time-stretch DSP deferred; no fake
quality claim is made for algorithms that do not yet exist.

## Momentum, elasticity, friction, and instability

Velocity is stored per legal directed edge. Force adds acceleration; the solver
integrates `velocity * dt` as real transfers. MASS reduces acceleration response
quadratically, creating real inertia while its authored forces still pull energy
toward Low and Sustain.

FRICTION exponentially damps edge velocity. ELASTICITY measures reservoir
deficits and legal surpluses relative to equilibrium and accelerates restorative
flows across the graph. It is a restoring force, not parameter interpolation.

INSTABILITY weakens effective damping and excites a deterministic signed
circulating mode through a legal reservoir cycle. Its phase is stateful, its
motion can overshoot, and all actual movement still passes through the solver.
There is no random parameter modulation and no unconstrained feedback.

Physics steps use elapsed seconds, split into internal increments no larger than
`1/240 s`, so behavior is approximately block-size independent. A maximum of 64
substeps bounds work after hostile host timing.

## Numerical safety

All controls and time deltas are sanitized. Transfers use fixed-capacity arrays
and reject illegal/non-finite requests. After each cycle the state is validated;
negative values are projected to zero, the simplex is normalized to the budget,
and the final floating residual is placed in the largest stable reservoir. A
non-finite/collapsed state returns to equilibrium. Competing destinations share a
scarce donor proportionally from the same pre-transfer state rather than winning
by enum order. No physics update allocates.

## DSP mapping

`DSPMapper` uses conditional ratios so serial processors do not each apply the
entire global budget. Mappings are nonlinear and mostly attenuation-only:

- Direct/Reflected becomes a dry coefficient and conserved delay-tail injection.
- Spectral dominance attenuates competing bands instead of boosting a winner.
- Transient/Sustain attenuates the losing envelope region.
- Spatial expansion attenuates Mid instead of boosting Side.
- Harmonic displacement drives a bounded saturation target while clarity recedes.

The Phase 3 processor renders the full Reflected share as delay. For reflected
power `q` and feedback `g`:

```text
dry = sqrt(1 - q)
wet input = sqrt(q * (1 - g^2))
```

For an isolated impulse and undamped fixed delay, the geometric tail energy is
exactly `q`. More feedback produces more repetitions but a weaker first repeat.
The loop caps feedback at 0.92, filters DC and highs, replaces non-finite input,
and uses a linked sample-peak safety gain. This is an impulse-energy proof, not a
claim that correlated programme material has physical energy equivalence.

## Real-time and UI architecture

The audio thread reads cached atomic parameter pointers, advances fixed-capacity
physics, maps targets, and processes preallocated delay rings. It performs no file
I/O, graph rebuild, GUI access, lock acquisition, or allocation after prepare.

`SnapshotExchange` is an SPSC triple buffer. The audio writer and GUI reader own
different slots and atomically exchange only the middle slot. The Energy Field and
debug panel receive value snapshots. Node size is actual reservoir amount and flow
lines are actual solver transfers; there is no decorative activity source.

Host state recall restores force parameters and atomically requests an audio-thread
reset of reservoirs, momentum, processor memory, and all smoothed targets to the
restored ENERGY budget's equilibrium. Exact mid-motion recall is deferred; the
reset policy is deterministic, explicit, and preserves the snapshot exchange's
single-producer contract.
