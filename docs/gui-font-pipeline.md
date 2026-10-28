# GUI font delivery

The application preserves supplementary UTF8 input but its224-glyph default font
still displays placeholders. [Pilot282](benchmarks/gui-font-pilot-282/receipt.json)
compares the retained32-bit ImGui1.91.9b library with pinned1.93.0-docking
`b916186e66aa3c02b72c6ca17455516512ed8049`. The annotated tag object is not the
commit. The pilot changes no application dependency or binary.

## Implementation route

1. Upgrade ImGui core and both existing GLFW/OpenGL backends together. Keep the
  32-bit character ABI in every translation unit consuming ImGui. Explicitly
  keep the existing bitmap Latin face; individual A/é advances and raster hashes
  match the old library at normal and large/dense settings. Do not change the
  public replay API to inject test text.
2. Package the pinned, unmodified OFL Noto Sans CJK SC and monochrome Noto Emoji
  files with their licenses/notices. Locate them beside the owning native module
  or executable, not relative to the working directory or source checkout. Bound
  and authenticate asset bytes before parsing; reuse an immutable, fixed-size
  font-data snapshot across successive GUI sessions. Missing/corrupt resources
  must produce a useful error without mutating the project.
3. Merge these sources and request glyphs on demand. Do not prebuild full BMP
  ranges or restrict delivery to a hardcoded sample corpus. Keep GuiScale's
  logical/framebuffer distinction; vary the requested size rather than rereading
  font files or rebuilding the full atlas at each DPI change.
4. Cap live RGBA atlas dimensions against the existing GL hardware limit and a
  2048-square application cap. Preserve the upstream false return on exhausted
  packing and remove only the immediately preceding aborting assertion, using a
  source-hash-checked generated derivative. The original checkout stays clean.
  All other assertions stay enabled. Pin and document this narrow patch.
5. Invalidate the framebuffer overlay cache when the current atlas identity or
  baked-font glyph state changes. Never retain ImFontBaked pointers across frames.
  Reuse the existing overlay key, session lifecycle and owner GL thread.

The application implementation is now prepared as delivery283, with installed
validation still pending. Keep the existing `font_rebuilds` field as the count
of accepted typography configurations; it is not a dynamic texture-allocation
counter. Preserve existing per-texture filtering when adopting the newer sampler
backend and match its rounded framebuffer dimensions.

## Acceptance

Use existing native gui_replay/overlay suites, installed GUI/Python cohorts and
package/runtime checks. Require exact glyph lookup plus nonzero raster pixels
for the sample CJK/supplementary symbols, zero advance for combining/ZWJ entries,
stable Latin metrics, staged dynamic texture creation/update/destruction and
recoverable quota exhaustion with assertions enabled. Sample coverage is not a
general Unicode or shaping claim.

Exercise actual GL texture updates, multiple DPI/size changes, repeated windows,
cache-versus-immediate pixel identity, package-relative resources, missing/corrupt
assets, source-preserving startup and actual Name editing/save/reopen/Undo/Redo.
Keep absent-glyph fallback deterministic and preserve original UTF8 bytes. Check
headless builds remain independent of GUI fonts and qualify changed application
sources through their own complete platform roles after local delivery.

The live texture cap is16MiB, not a total font-memory bound. Old/new CPU textures
and GPU resources coexist during growth. The pilot's stress case peaks at
33,292,288 CPU texture bytes and33,030,144 simulated GPU bytes. The two immutable
font files add18,419,960 bytes before loader metadata. Measure real GL allocations,
transient behavior and RSS; do not infer them from a CPU protocol simulation.
The pinned OpenGL3 backend requires RGBA32, so Alpha8 CPU support alone provides
no GPU saving.

Arabic/Indic shaping, bidi layout, ZWJ ligatures, locale-specific Han forms,
physical monitor/DPI delivery, broader glyph coverage and supported-platform/UI
acceptance remain separate M2 work. This does not close any M0-M7 milestone or
establish a general speed advantage.

## Primary sources

- [Pinned docking font documentation](https://github.com/ocornut/imgui/blob/b916186e66aa3c02b72c6ca17455516512ed8049/docs/FONTS.md)
- [Pinned atlas implementation](https://github.com/ocornut/imgui/blob/b916186e66aa3c02b72c6ca17455516512ed8049/imgui_draw.cpp)
- [Pinned OpenGL backend](https://github.com/ocornut/imgui/blob/b916186e66aa3c02b72c6ca17455516512ed8049/backends/imgui_impl_opengl3.cpp)
- [Noto CJK source/license manifest](benchmarks/gui-font-pilot-282/inputs/cjk-manifest.json)
- [Noto Emoji source/license manifest](benchmarks/gui-font-pilot-282/inputs/emoji-manifest.json)
