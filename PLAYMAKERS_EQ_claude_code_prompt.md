# PLAYMAKERS EQ — Build Kickoff Prompt (for Claude Code)

Copy everything below into Claude Code as your first message in a fresh project directory.

---

## Project

Build **PLAYMAKERS EQ**, a parametric equalizer audio plugin, from scratch. This is the first
plugin in a suite (EQ now, Compressor/Saturator/Reverb later) that will share a common visual
identity and codebase conventions — architect accordingly, but only build the EQ for now.

**Targets:** VST3 and AU, so it loads in FL Studio (VST3, Windows/Mac) and GarageBand/Logic
(AU, Mac). Use **JUCE (C++)** as the framework — it's the standard choice for this exact
cross-host target list and handles the VST3/AU parameter-and-UI bridging for us. Confirm JUCE
version and license tier (free under the indie revenue threshold, paid above it) before we ship,
but don't let that block dev setup now.

Do not reference, imitate, or reverse-engineer any specific competing plugin's code, mode names,
or marketing terms. Build every feature from first-principles DSP (standard biquad filter design,
minimum-phase vs. linear-phase FIR theory, envelope-follower dynamics, etc.) and name every
mode/feature with our own terminology. If you're ever unsure whether a name or behavior is
too close to a specific competitor's branding, default to a plainer, more descriptive name instead.

## Architecture (decide this before writing DSP or UI code)

- **24-band hard cap**, fixed parameter slots allocated at plugin instantiation
  (freq/gain/Q/type/enabled/stereo-mode/dynamic-params per slot). VST3/AU expect a stable
  parameter count for the plugin's lifetime, so don't grow/shrink the parameter tree at runtime —
  "adding a band" means populating the next disabled slot and flipping it active; "removing a
  band" means disabling the slot, not deleting the parameter.
- Disabled/inactive slots must not render on the spectrum display, must not appear in the band
  list, and should report a neutral/bypassed state to the host.
- Each band is **one stereo-linked unit** (Left/Right-linked or Mid/Side-linked) — no
  splitting a single band into two independent channel-specific bands under the hood. Keep this
  simple; revisit only if we get real user demand later.
- Set up the parameter tree so per-band dynamic-EQ sub-parameters exist for every band (all 24
  are dynamic-capable — see Dynamic EQ section) but are only active/exposed for filter types
  that support gain modulation.

## Filter types (v1 full set)

Bell, Low Shelf, High Shelf, Low Cut/High Pass, High Cut/Low Pass, Notch, Band Pass, All Pass,
Tilt Shelf, Flat Tilt. Implement via standard biquad coefficient design (RBJ Audio EQ Cookbook
formulas are a solid reference starting point) — no vintage/hardware-modeled/character filter
types; keep everything mathematically transparent.

- **Continuous slope control up to 96 dB/oct** for all filter types where slope applies, via
  cascaded biquad stages (each stage ~12 dB/oct for a standard 2nd-order section; interpolate
  the displayed slope value smoothly as stages are added).
- **Brickwall mode** on High Cut/Low Pass and Low Cut/High Pass only: a separate, much steeper
  cutoff mode (not just the top of the continuous slider) — this needs its own filter design
  path since near-vertical cutoffs have different stability/ringing tradeoffs than stacking
  standard biquads.

## Core interaction model (this is the product's main differentiator — build it first after DSP core)

- Double-click on the spectrum display creates a band at that exact frequency/gain point.
- Click-and-drag on the spectrum creates a band, with initial gain set by drag distance/direction.
- Once a band exists, its curve/peak can be grabbed directly on the analyzer and dragged to
  adjust frequency + gain together; a separate gesture (e.g. scroll wheel, or modifier + drag)
  adjusts Q on the same handle.
- New band's default filter type depends on the frequency zone clicked (low → low shelf/high
  pass, mid → bell, high → high shelf/low pass); zone boundaries are user-configurable in
  settings.
- Direct-gesture deletion on the band itself (e.g. modifier-key click) — never require opening
  a separate list to remove a band.
- Live curve preview while dragging to create a band, before the gesture commits it.
- Selected band's curve/handle is highlighted; other bands recede in opacity/line weight.
- Multi-select bands directly on the spectrum (marquee or shift-click) and batch-edit shared
  parameters across the selection.

## Stereo / Mid-Side

Per band, independently selectable: Left/Right (default, with a pan-style stereo-position
control), Left only, Right only, Mid/Side (with a Mid/Side balance control), Mid only, Side only.

## Dynamic EQ

- Available on Bell, Low Shelf, High Shelf, Tilt Shelf, Flat Tilt only (types with a gain
  parameter to modulate).
- All 24 bands are dynamic-capable.
- Per dynamic band: Threshold; Range (max gain change, independent above/below threshold);
  Ratio (independent above/below); Attack; Release.
- **Two detection modes:**
  1. *Direct* (default/simpler): the dynamic band reacts to its own filtered signal level via a
     standard envelope follower.
  2. *Relative* (advanced, collapsed by default): the band's reaction is judged against a second
     reference envelope (default: full mix; adjustable to a different frequency range; blendable
     between purely relative and purely direct behavior). Build this as a second detector path
     feeding the same gain-computation stage, not a separate dynamics system.
- Side-chain input support: a dynamic band's detector can listen to an external side-chain input
  (with its own filtering applied before detection), blendable with the plugin's own input.
- Channel-scope decoupling: a dynamic band can detect on one channel scope (e.g. Mid) while
  applying its gain change to a different scope (e.g. Side).
- Treat the broader "react to overall spectral shape rather than one band's level" detection mode
  as a v2 stretch feature — build and stabilize everything above first.

## Phase / latency modes

Three modes, described by behavior, not borrowed marketing names:

| Mode | Behavior | Latency |
|---|---|---|
| Zero Latency (default) | Minimum-phase biquads; phase is altered but no pre-ringing | None |
| Low-Latency Phase-Corrected | Small fixed look-ahead reduces phase smearing vs. Zero Latency without full linear-phase cost | Small, fixed |
| Linear Phase | FIR-based, phase untouched; ship 2–3 quality tiers (Low/Med/High) trading FIR length (latency/CPU) for precision | Noticeable, shown to user |

No filter mode should intentionally deviate from ideal/transparent filter behavior for character
or "vintage" effect — that's out of scope entirely for this plugin.

## Metering & analyzer

Real-time spectrum analyzer (pre-EQ, post-EQ, external-input, independently toggleable);
adjustable range (~9 dB tight to ~90 dB wide), speed, resolution, and tilt (compensating display
slope for natural high-frequency roll-off); freeze; overlap/collision flagging between band
curves; optional musical-note frequency axis overlay; reference-track EQ matching (analyze a
reference signal's spectral shape, generate a starting curve the user can edit); a compact
scrollable/grouped band list (Freq/Gain/Q/Type, multi-select) since 24 bands won't fit flat in
one view.

## Visual identity & customization (PLAYMAKERS brand — build this data-driven, not hardcoded)

Theme = a JSON/config object (colors, asset references) the UI reads at runtime — not
per-theme classes — since this system will be reused across the rest of the plugin suite.
Includes: accent color, grid line style/visibility, solid-color background (image/texture
backgrounds: flag as a decision, don't build yet), a small set of preset knob-style skins
(not fully custom per-knob building), light/dark mode, and a layout-density toggle (classic vs.
modern arrangement, not just recolor). In-app theme save/load only for now — no file export.

## Workflow details

Customizable modifier keys (reset-to-default, fine-tune, lock-frequency-while-dragging);
mouse-wheel target customization per filter type; undo/redo; A/B state comparison (two full
recallable plugin states); smooth/click-free parameter transitions on automation, preset
recall, and A/B switching; sample-accurate automation; double-click-to-type numeric entry on
every parameter.

## Explicitly out of scope for now

Vintage/hardware-modeled filter character, any "deviate from ideal filter" phase mode,
ultra-high internal precision beyond standard high-quality processing, always-on oversampling
by default, fully custom per-parameter knob building, theme file export, split-band stereo.

## How to approach the build

Work in phases and check in after each:
1. JUCE project scaffold (VST3 + AU targets), plugin shell loads silently in a host with no
   parameters yet.
2. Parameter tree (24 fixed band slots + globals) wired to a minimal DSP core: Bell/Shelf/Cut
   biquads only, mono processing correctness first.
3. Stereo/MS routing per band.
4. Spectrum analyzer + basic UI rendering of band curves (read-only first, no interaction yet).
5. Direct spectrum interaction (click-to-add, drag, curve-grab, deletion, multi-select).
6. Remaining filter types + continuous slope + brickwall mode.
7. Dynamic EQ (direct detection first, then relative/side-chain/channel-decoupling).
8. Phase modes (Zero Latency already implicit in step 2; add Low-Latency Phase-Corrected and
   Linear Phase).
9. Theming system + visual customization UI.
10. Workflow polish (undo/redo, A/B, modifier keys, automation smoothing).

Start with Phase 1 and confirm the project builds and loads in at least one host before moving on.
