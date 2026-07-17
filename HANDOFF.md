# PLAYMAKERS EQ — Handoff

Spec: `PLAYMAKERS_EQ_claude_code_prompt.md` (project root). Build with:
```
cmake -B build -G Xcode
cmake --build build --config Debug --target PlaymakersEQ_AU --target PlaymakersEQ_VST3
```
Installs to `~/Library/Audio/Plug-Ins/{Components,VST3}/`. Verify AU with
`auval -v aufx Peq1 Plmk`. DSP correctness test: build+run the `DspSmokeTest` target.

## 1. What's built (Phases 1–4, all committed)

- **Phase 1** — JUCE 8.0.14 pulled via CMake `FetchContent`. Plugin shell (`Source/PluginProcessor.*`,
  `PluginEditor.*`) builds AU + VST3, loads silently, zero parameters. `auval` clean.
- **Phase 2** — `Source/Params.h/.cpp` defines the full 24-band parameter tree via
  `AudioProcessorValueTreeState`: per band — `enabled`, `type` (10-value filter enum), `freq`, `gain`, `q`.
  `Source/FilterBand.h` wraps one stereo-linked `juce::dsp::IIR::Filter` pair (shared coefficients,
  separate per-channel state) and computes RBJ-cookbook coefficients via JUCE's built-in
  `dsp::IIR::Coefficients::makeXXX`. Only Bell/Low-Shelf/High-Shelf/Low-Cut/High-Cut are real DSP;
  the other 5 enum values return identity coefficients (no-op) until Phase 6.
- **Phase 3** — added `stereoMode` (6-value enum: L/R, Left-only, Right-only, M/S, Mid-only, Side-only)
  and `balance` (-1..1) per band. Routing lives in `PluginProcessor::processStereoBand()`. One
  crossfade implementation serves both L/R and M/S domains — see decisions below.
- **Phase 4** — `Source/SpectrumAnalyzer.h`: `AnalyzerDataProvider` (2048-pt FFT, Hann window, fed a
  post-EQ mono mixdown from `processBlock`) + `SpectrumAnalyzerComponent` (30fps repaint, draws grid,
  live spectrum, and per-band response curves). Curves are computed with the same
  `FilterBand::computeCoefficients()` the audio thread uses, so UI and DSP can't drift apart.

All 4 phases: clean build, `auval` pass, committed individually.

## 2. In progress / partial / untested

- **Nothing mid-edit** — working tree was clean at handoff time, Phase 4 was the last completed commit.
- **Phase 5 (direct spectrum interaction) has not been started at all.** No mouse handling exists yet
  in `SpectrumAnalyzerComponent` — it's currently `Component` + `Timer`, read-only, no `mouseDown`/
  `mouseDrag`/etc.
- **Only mono-summed post-EQ is analyzed** — no pre-EQ or external-sidechain analyzer path exists yet,
  even though the full spec (Metering & analyzer section) calls for pre/post/external toggle. This was
  a deliberate scope cut for Phase 4 (see decisions below), not an oversight, but it's incomplete
  relative to the full spec.
- **No automated test covers stereo/MS routing (Phase 3) or the analyzer (Phase 4).** Only the DSP
  filter math (Phase 2) has a regression test (`Tests/DspSmokeTest.cpp`). If you touch
  `processStereoBand` or `AnalyzerDataProvider`, there's no safety net — verify manually or add tests.
- **No UI beyond the analyzer view.** No band list, no knobs, no theme — `PluginEditor` just hosts the
  analyzer component full-window.

## 3. Decisions made that weren't explicit in the spec

- **JUCE 8.0.14** pinned via `FetchContent` (latest stable tag at the time) rather than vendoring/
  submodule — keeps the repo small; first configure re-clones JUCE (~200MB) if `build/` is wiped.
- **Manufacturer/plugin codes**: `Plmk` / `Peq1` (4-char codes JUCE/AU require). Not specified by the
  user — picked as an obvious abbreviation. If a formal AU registration is ever done, double-check
  these don't collide with another registered developer's codes.
- **Full filter-type enum locked in during Phase 2**, not grown incrementally per phase, because
  AudioParameterChoice index order becomes part of saved-session/automation identity. Reordering later
  would break anyone's saved projects. The 5 unimplemented types return identity coefficients rather
  than being omitted from the enum.
- **Stereo `balance` param is one continuous float reused across L/R and M/S modes**, rather than two
  separate params (a "pan" one and an "M/S balance" one). Implementation: `effectiveBalance` is the
  param value for `LeftRight`/`MidSide` modes, or pinned to ±1 for the `*Only` variants. `wetA = 1 -
  max(0, balance)`, `wetB = 1 - max(0, -balance)`, crossfaded between dry and filtered per side. This
  was a simplification not spelled out in the spec — reduces to 1 param instead of 2, but if a design
  review wants独立 pan vs. M/S-balance semantics, this'll need splitting.
- **DSP is dual-mono per band, not stereo-coupled in a single filter** — each band holds two independent
  `IIR::Filter` instances (L, R) sharing one `Coefficients` object. This is what makes L-only/R-only/
  Mid-only/Side-only routing possible without extra filter instances.
- **Analyzer uses a deliberately racy single-flag handshake** (`nextBlockReady` bool, no mutex) between
  audio thread and UI thread — standard pattern for FFT-visualization-only data (worst case: one stale/
  dropped frame, no audible or correctness impact). Don't reuse this pattern for anything that affects
  audio output.
- **Test target (`Tests/DspSmokeTest`) added even though not in the original phase plan** — needed real
  verification that Phase 2's biquad coefficients were actually correct (not just that code compiled).
  Links `juce_dsp` + `juce_audio_processors` directly against `FilterBand.h`/`Params.cpp`, bypassing the
  plugin-client macros — reuse this pattern if you add more DSP-only tests.

## 4. Known issues / TODOs

- Coefficient recompute happens **unconditionally every block** for every enabled band (no
  change-detection, no smoothing) — fine for now (correctness-first per the spec), but will need
  smoothing before Phase 10's "click-free parameter transitions on automation" requirement. Flagging
  now so it isn't forgotten.
- `SpectrumAnalyzerComponent` grid has only 3 vertical (100Hz/1kHz/10kHz) and 4 horizontal (0/-30/-60/
  -90dB) lines, no text labels — intentionally minimal for Phase 4's "basic" scope, will likely want
  labels once the UI is otherwise more built out.
- No `.claude/settings.json` committed (only `.local.json`, gitignored) — if Cursor's agent needs
  permission scaffolding it'll start fresh; nothing to port over there.
- CMake `Debug` config only has been exercised — never built/tested `Release`.
- Only tested on this machine (macOS, arm64, Xcode 26.2 SDK). Never verified in an actual DAW (FL
  Studio / Logic / GarageBand) — only `auval` and the standalone DSP test. Loading it in a real host is
  still an open verification step.

## 5. Next concrete steps

1. **Load the plugin in an actual host** (Logic/GarageBand for AU, or any VST3 host) and eyeball the
   spectrum analyzer + confirm a band curve appears when you flip `enabled` via the host's generic
   parameter UI (there's no custom UI to toggle it yet, so use the host's built-in parameter list).
2. **Start Phase 5** (direct spectrum interaction) in `SpectrumAnalyzerComponent`:
   - double-click → find first disabled band slot, set its freq/gain from click position, flip `enabled`
   - drag-create, curve-grab (freq+gain), modifier/scroll for Q, modifier-click to delete
   - default filter type by frequency zone (low/mid/high → shelf/HP vs bell vs shelf/LP)
   - selection state + multi-select (marquee/shift-click) for later batch-edit
3. Once Phase 5 has real interaction, revisit whether the single `balance` param design (decision #3
   above) still feels right once there's a UI control surface for it — may want to split before too much
   UI is built on top of the current shape.
