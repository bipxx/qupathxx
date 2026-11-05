# Explicit standard stain profiles — PRESET1 contract

Status: selected R7 workflow; implementation pending. Use existing
ImageAnalysisSettings, StainTransform and Document history/persistence. This adds
two explicit standard profiles, not a new mutable registry or estimator.

## User outcome and reference

Choose H&E or H-DAB by name in SDK/ImGui/CLI, retain the full normalized profile
and apply it deliberately. Saved profile/type/channel names, white and residual
survive undo/redo, project save/reopen and explicit analysis/model export.

Pinned QuPath0.7 factories define H&E default and H-DAB default from the standard
hematoxylin/eosin/DAB directions and white255. The
[verified source/reference](benchmarks/stain-presets-r7/README.md) reuses the
already captured native saved-default profiles, including their exact normalized
three-vector bases and signed residuals. Preserve those doubles directly with
ImageAnalysisSettings.from_normalized; do not re-normalize them or substitute
legacy GUI/CLI vectors. This establishes configuration compatibility, not
biological suitability or whole-estimator agreement.

## Native/SDK contract

Add separate qupathxx/stain_presets.hpp with:

- standard_stain_profile_types(): owning ordered type IDs BRIGHTFIELD_H_E,
  BRIGHTFIELD_H_DAB.
- standard_stain_profile(image_type): owning ImageAnalysisSettings with exact
  pinned name/channel names/full basis/white/residual and numeric model hash.
  Reject unsupported type IDs; no fallback or label inference.

The existing ImageAnalysisSettings(type) constructor still creates a type-only
value; it never silently invents a profile. Existing explicit analysis requests,
legacy GUI/CLI vector defaults and model-file semantics remain unchanged. No
class-layout, snapshot-quota, project-schema or reader changes. Profiles are
fixed reference data returned as independent values; no process-global mutable
library, source file, worker/Python references or new JSON dependency.

## Consumers and easiest implementation path

Bind the same factories in image_analysis_settings_bindings.cpp and export them
through the Python package. SDK callers use the existing document setter,
transform(), model save and managed operations where already available.

The inspector makes Saved analysis defaults available even when no value exists.
A local preset selection and Save default profile action use the existing single
history command; Clear defaults uses the current setter. Preserve no-op/admission
atomicity and keep labels/current saved state readable. Existing Use saved stains
for analysis remains a separate action. Saving defaults must not rewrite or
silently replace current explicit recipes/model controls. Keep the empty section
compact/collapsed by default. No new common GuiOptions/GuiReport/replay enum is
needed: tests inspect authoritative Document state and actual controls.

CLI adds --stain-preset TYPE as an explicit alternative to --stain-model and
numeric --white/--stains/--normalized. Reject mixed modes in either order and
unknown/duplicate/empty preset inputs. Resolve the native profile once before
batch capture and pass its exact transform; batch identity/resume uses existing
numeric model hashing. CLI selection does not mutate per-image saved defaults.

## Delivery and checks

P1 native: implement a small fixed table/free factory in its own source/header,
with exact independent hashes/names/white/residual from the old fixture, owning
copies/unsupported IDs and unchanged type-only constructor. Reuse current
settings/transform authority; no new runtime capture or numeric-normalization
kernel. A separate header avoids changing unrelated common GUI/core layouts.

P2 consumers: bind the factory, GUI authoring/history/adoption and CLI explicit
mode once; verify owning SDK values, exact imported-profile identity, saved
reopen/replay/export, real GUI Save/Clear/adopt/history and CLI SDK equivalence/
resume/conflicts. Reuse existing persistence faults. Settle interfaces before
one cached binding/stub pass. Inspect actual changed controls and recheck affected
model/default GUI flows; no full qualification.

P3 installed: one settled wheel/install, exact source/member checks and installed
select/save/adopt/analyze/retain/export/reopen route, reusing compatible native/
staged/typing results. Only the demonstrated two-profile workflow closes.
Custom named catalogues, other profile formats, estimator/specimen accuracy,
resources/hosts/release and all broader R2-R16/M0-M7 scope remain visible.

Review actual waste and update the current card after each task/failed attempt
before the next action. Preserve source-specific receipts and failures; use
atomic UTF-8 edits and actual API/layout discovery. Openly licensed dependencies.
