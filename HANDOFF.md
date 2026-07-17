# PLAYMAKERS EQ — Handoff

Spec: `PLAYMAKERS_EQ_claude_code_prompt.md` (project root). Repo: https://github.com/moreland99/PlayMakerPlugin
Build with:
```
cmake -B build -G Xcode
cmake --build build --config Debug --target PlaymakersEQ_AU --target PlaymakersEQ_VST3
```
Installs to `~/Library/Audio/Plug-Ins/{Components,VST3}/`. Verify AU with
`auval -v aufx Peq1 Plmk`. DSP correctness test: build+run the `DspSmokeTest` target.

## 1. What's built (Phases 1–10, all verified: build + DspSmokeTest + auval)

- **Phase 1** — JUCE 8.0.14 via CMake `FetchContent`. AU + VST3 targets, `auval` clean.
- **Phase 2** — 24-band parameter tree (`Params.h/.cpp`), `FilterBand.h` stereo-linked biquads.
- **Phase 3** — per-band `stereoMode` (L/R, L, R, M/S, M, S) + `balance`, routed in
  `PluginProcessor::processStereoBand()`.
- **Phase 4** — `SpectrumAnalyzer.h/.cpp`: post-EQ FFT analyzer + band response curves.
- **Phase 5** — direct spectrum interaction: double-click / drag-create (live preview), handle drag
  (freq+gain, Cmd=fine), scroll-Q (Shift=finer), Alt-click delete, shift-click / marquee multi-select.
- **Phase 6** — all 10 filter types real DSP (`FilterBand.h`): notch/bandpass/allpass via JUCE
  coefficients; tilt shelf = ∓gain/2 low+high shelf pair at the pivot; flat tilt = wide-spaced
  gentle shelves (freq/4, freq×4, Q 0.5). Continuous slope 12–96 dB/oct on Low/High Cut via up to 8
  cascaded second-order sections with proper Butterworth pole-Q distribution; user Q scales the last
  (resonant) section. Brickwall = separate design path: full 16th-order Butterworth (8 sections, exact
  Qs, user Q ignored). New per-band params: `slope`, `brickwall`.
- **Phase 7** — dynamic EQ (`DynamicBand.h`): per band `dynEnabled`, `dynThreshold`, `dynRange`
  (sign = direction, magnitude = max change), `dynRatio`, `dynAttack`, `dynRelease`,
  `dynRelativeBlend` (0=direct level detection, 1=level relative to full-mix reference envelope),
  `dynSidechainBlend`. Detector = bandpass at band freq on a mono mix of main input and the new
  optional external **sidechain bus**, envelope follower → gain offset in dB added to the band's
  static gain per block. Only gain-capable types (`Params::typeSupportsDynamics`).
- **Phase 8** — global `phaseMode` (Zero Latency / Low-Latency Phase-Corrected / Linear Phase) +
  `linearQuality` (Low/Med/High = 511/2047/8191 taps; low-latency mode = 127 taps).
  `LinearPhaseEQ.h` builds a symmetric FIR from the composite static band magnitude (zero-phase
  spectrum → IFFT → centered + Hann window) and runs `juce::dsp::Convolution`. Latency reported via
  `setLatencySamples`. Rebuilds are hash-gated on a processor `Timer` (8 Hz, message thread).
- **Phase 9** — data-driven theming (`Theme.h`): plain struct ⇄ JSON, Dark + Light presets, persisted
  in `apvts.state` property `themeJSON` (saves with the session). `ThemeManager` owns the active theme;
  the analyzer and editor read colours from a `const Theme&`. Editor header bar has the theme toggle.
- **Phase 10** — workflow: `UndoManager` wired into APVTS (Undo/Redo buttons + Cmd-Z / Shift-Cmd-Z /
  Cmd-Y, transactions batched by a 2 Hz editor timer); A/B state slots on the processor
  (`toggleAB` / `copyCurrentToOtherSlot`, header buttons); 20 ms `SmoothedValue` smoothing on
  freq/gain/q targets in the audio path (click-free automation/preset/A-B moves at block rate);
  fine-tune modifier already present from Phase 5.

## 2. Deliberate scope cuts vs. the full spec (not oversights)

- **FIR modes carry only plain bands**: bands that are dynamic, or use non-default stereo routing
  (mode ≠ L/R or balance ≠ 0), fall back to the minimum-phase IIR path layered on top of the FIR —
  so those bands stay minimum-phase even in Linear Phase mode.
- **Dynamics are block-rate**: the gain offset updates once per block via coefficient recompute, not
  per-sample. Attack/release ranges are well above typical block durations, so it tracks fine, but a
  per-sample smoother would be the upgrade path.
- **Dynamic channel-scope decoupling** (detect on Mid, apply on Side) not built. Detector is always
  a mono mixdown; application follows the band's own stereo mode.
- **Relative detection reference** is the full-mix envelope only — no adjustable reference frequency
  range yet.
- **Analyzer**: still post-EQ mono only (no pre/external toggle, range/speed/tilt, freeze, collision
  flags, note overlay, EQ matching). No band list UI, no knobs, no numeric entry — the analyzer + host
  generic view is still the only control surface.
- **Modifier keys are fixed**, not user-customizable; frequency-zone boundaries (250 Hz / 5 kHz)
  hardcoded (`SpectrumAnalyzerComponent`). No settings UI exists.
- **Theme**: no layout-density toggle, no knob skins (no knobs yet), in-app save only (per spec).
- **Undo history includes host automation moves** (they flow through the same APVTS/UndoManager
  path) — standard JUCE tradeoff, revisit if it annoys.

## 3. Key decisions (older ones from Phases 1–5 still apply)

- JUCE 8.0.14 pinned via FetchContent; codes `Plmk`/`Peq1`; filter-type & stereo-mode enum order is
  saved-session ABI — never reorder. Same now applies to `phaseMode`, `linearQuality`, and all the
  new per-band param IDs (`slope`, `brickwall`, `dyn*`).
- Single `balance` float still serves L/R and M/S modes (see Phase 3 notes in git history).
- Band curves/handles use ±24 dB display scale; spectrum uses −90…+6 dB absolute.
- `bandUsesFIR()` (processor) is the single source of truth for FIR vs. IIR path selection —
  the FIR rebuild and processBlock both consult it; keep them consistent through it.
- FIR rebuild throttle: params hashed (FNV-style fold over atomics) at 8 Hz; rebuild only on change.
  Sidechain bus added as a third bus — `isBusesLayoutSupported` accepts disabled/mono/stereo for it.
- Analyzer's racy single-flag FFT handshake unchanged — display-only, don't reuse for audio.

## 4. Known issues / TODOs

- Coefficient recompute still unconditional per enabled band per block (now with smoothed values).
  Cheap enough, but change-detection would save CPU with many bands.
- `DynamicBandDetector` recomputes its bandpass coefficients every block (same tradeoff).
- FIR rebuild does its FFT work on the message thread — a very fast automation of many band params in
  FIR mode re-renders at most 8×/s; fine, but a worker thread would be cleaner.
- Editor keyboard shortcuts need the editor focused (click it first); `EDITOR_WANTS_KEYBOARD_FOCUS`
  is still FALSE in CMake.
- Release config never built; never verified in a real DAW (only `auval` + smoke test). Loading in
  Logic/GarageBand/FL and exercising gestures + phase modes + sidechain is the top verification gap.
- Linear-phase High quality = 8191 taps ≈ 93 ms latency at 44.1 kHz — check hosts report/compensate it.

## 5. Next concrete steps

1. **Load in a real DAW** and verify: Phase 5 gestures, slope/brickwall curves, a dynamic band
   ducking on loud input, phase-mode switching (latency change audible/reported), sidechain routing,
   A/B + undo + theme toggle.
2. **Band list / knob UI** — the biggest remaining spec item (Metering & analyzer section's band list,
   double-click numeric entry, per-band controls beyond the spectrum gestures).
3. **Analyzer upgrades** — pre/post/external toggle, range/speed/tilt, freeze.
4. Revisit block-rate dynamics and message-thread FIR rebuild if profiling or listening tests flag them.
