# PLAYMAKERS EQ — Handoff

Spec: `PLAYMAKERS_EQ_claude_code_prompt.md` (project root). Build with:
```
cmake -B build -G Xcode
cmake --build build --config Debug --target PlaymakersEQ_AU --target PlaymakersEQ_VST3
```
Installs to `~/Library/Audio/Plug-Ins/{Components,VST3}/`. Verify AU with
`auval -v aufx Peq1 Plmk`. DSP correctness test: build+run the `DspSmokeTest` target.

## 1. What's built (Phases 1–5)

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
- **Phase 4** — `Source/SpectrumAnalyzer.h/.cpp`: `AnalyzerDataProvider` (2048-pt FFT, Hann window, fed a
  post-EQ mono mixdown from `processBlock`) + `SpectrumAnalyzerComponent` (30fps repaint, draws grid,
  live spectrum, and per-band response curves). Curves are computed with the same
  `FilterBand::computeCoefficients()` the audio thread uses, so UI and DSP can't drift apart.
- **Phase 5** — direct spectrum interaction on `SpectrumAnalyzerComponent`:
  - **Double-click** → first disabled band slot enabled at click freq/gain; default type by zone
  - **Click-drag** on empty space → live create preview, commits on mouse-up
  - **Handle drag** → freq+gain (Cmd/Ctrl = fine-tune); multi-selected bands move together
  - **Scroll wheel** → Q (Shift = finer); applies to hovered or selected band(s)
  - **Alt/Option-click** → disable (delete) band
  - **Shift-click** toggle select; **Shift-drag** marquee multi-select
  - Selected curves/handles highlighted; others recede
  - Zone defaults: <250 Hz → Low Shelf, >5 kHz → High Shelf, else Bell (hardcoded; settings later)

Phases 1–4: clean build, `auval` pass, committed individually. Phase 5: clean build, `auval` pass,
`DspSmokeTest` pass — **not yet committed** at handoff time.

## 2. In progress / partial / untested

- **Nothing mid-edit** — Phase 5 interaction is implemented and verified via build/`auval`/`DspSmokeTest`.
- **Only mono-summed post-EQ is analyzed** — no pre-EQ or external-sidechain analyzer path exists yet,
  even though the full spec (Metering & analyzer section) calls for pre/post/external toggle. This was
  a deliberate scope cut for Phase 4, not an oversight, but it's incomplete relative to the full spec.
- **No automated test covers stereo/MS routing (Phase 3), the analyzer (Phase 4), or spectrum
  interaction (Phase 5).** Only the DSP filter math (Phase 2) has a regression test
  (`Tests/DspSmokeTest.cpp`). Manual host testing is the safety net for UI gestures.
- **No UI beyond the interactive analyzer.** No band list, no knobs, no theme — `PluginEditor` just
  hosts the analyzer component full-window.
- **Host load still unverified in a real DAW** (Logic / GarageBand / FL Studio) — only `auval` + DSP
  smoke test. Phase 5 gestures especially need a quick eyeball in an actual host editor window.

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
- **Band curves / handles use a ±24 dB vertical scale** (0 dB at display centre), while the live FFT
  spectrum keeps its wide absolute-level range (−90…+6 dB). Phase 4 had drawn both on the spectrum
  scale, which crushed usable gain-edit space into the top ~30% of the view; Phase 5 split the scales
  so click-to-set-gain is usable. Spectrum and EQ curve no longer share a common dB axis — intentional,
  and matches how most modern EQs present absolute spectrum vs. relative gain.
- **Frequency-zone boundaries hardcoded** (250 Hz / 5 kHz) until a settings UI exists. Spec calls them
  user-configurable; don't invent a settings system early — just keep the constants named and local
  (`lowZoneMaxHz` / `highZoneMinHz` in `SpectrumAnalyzerComponent`).
- **SpectrumAnalyzer implementation moved to `.cpp`** in Phase 5 (was header-only in Phase 4) — mouse
  interaction made the component too large for a header. `AnalyzerDataProvider` stays in the header.

## 4. Known issues / TODOs

- Coefficient recompute happens **unconditionally every block** for every enabled band (no
  change-detection, no smoothing) — fine for now (correctness-first per the spec), but will need
  smoothing before Phase 10's "click-free parameter transitions on automation" requirement. Flagging
  now so it isn't forgotten.
- `SpectrumAnalyzerComponent` grid has no text labels (freq/dB) — intentionally minimal, will likely
  want labels once the UI is otherwise more built out. Horizontal lines now mark ±12 / 0 dB on the
  *curve* scale (not the old spectrum −30/−60/−90 lines).
- No `.claude/settings.json` committed (only `.local.json`, gitignored) — if Cursor's agent needs
  permission scaffolding it'll start fresh; nothing to port over there.
- CMake `Debug` config only has been exercised — never built/tested `Release`.
- Only tested on this machine (macOS, arm64, Xcode 26.2 SDK). Never verified in an actual DAW (FL
  Studio / Logic / GarageBand) — only `auval` and the standalone DSP test. Loading it in a real host is
  still an open verification step (especially Phase 5 gestures).

## 5. Next concrete steps

1. **Commit Phase 5** if desired (`SpectrumAnalyzer.h/.cpp`, `Source/CMakeLists.txt`, this handoff).
2. **Load the plugin in an actual host** and exercise Phase 5 gestures: double-click create, drag-create
   with preview, handle drag, scroll-Q, Alt-delete, shift/marquee multi-select.
3. **Start Phase 6** (remaining filter types + continuous slope + brickwall mode) in `FilterBand` /
   `Params` — Notch, Band Pass, All Pass, Tilt Shelf, Flat Tilt are still identity no-ops; slope /
   brickwall params don't exist yet.
4. Once there's a control surface for stereo balance, revisit whether the single `balance` param
   design (decision above) still feels right — may want to split before too much UI is built on it.
