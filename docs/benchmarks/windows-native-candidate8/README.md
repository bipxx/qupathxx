# Candidate8 native deployment receipt

Local install/use/removal passed on Windows x64 at source68bf2d (805 exact
compiler fingerprint inputs). This is a fresh external per-user prefix on the
existing workstation, using Windows-only runtime PATH and no Python paths.
Publication status is recorded by the release manifest and GitHub release.

- Current SDK configures/builds all120 public/generated headers and the included
  C++ example.471 compiler include records use only the installed SDK and native
  Visual Studio/Windows toolchain; no original source/cache dependency or CRT
  conflict is present. The example executable has no Python import.
- Native QC/normalization/pseudobulk, seeded PCA/clustering and replicate Welch/BH
  run through installed services. Exact saved recipes/results reopen successfully.
  Batch DE exports equal SDK CSV bytes; PCA replay/plot succeeds. Region output
  matches an independently specified authored RGB crop. Docked GUI startup opens
  the retained project without changing its bytes.
- All251 manifest-owned app/SDK files are removed. All18 research output files
  retain their hashes, including a user-added note inside the application prefix.
  Removal tests prove modified-member and external-path refusal before deletion,
  plus successful removal preserving user files:3 passed.

[handoff.json](handoff.json) records source/configuration/scope;
[artifacts.json](artifacts.json) records the four exact archive hashes;
[removal-proof.json](removal-proof.json) records relative member/output hashes;
[consumer.json](consumer.json) records exact native saved IDs; and
[source-inputs.sha256](source-inputs.sha256) retains the805-input native manifest.
Public logs replace the external validation prefix with `VALIDATION_ROOT`; raw
local logs/failures remain under `build/feature-expansion/p02-native-release`.

Corrections: Store Python redirected the first AppData install, before any native
compilation; use the existing unpackaged controller. An incompatible GUI test
argument pair refused; rerun startup alone after dropping `--compact`. Windows
PowerShell tests needed its own module path, with uppercased Windows environment
keys; assertions require the intended refusal rather than any nonzero exit.
No native engine, kernel, GUI, binding or wheel rebuild was needed for P02.

The separate P01 SDK and all old published candidates remain frozen. Scientific
and UI evidence keep their original source/scope. This receipt does not extend
support to another OS/compiler/ABI, a fresh VM, full corpus/biological acceptance
or signing/stable status. See [support/use](../../windows-native-candidate8.md).
