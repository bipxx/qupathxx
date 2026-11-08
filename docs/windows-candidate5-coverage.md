# Windows candidate5 packaging coverage

All12 selected checks are complete. [Install guide](benchmarks/windows-candidate5/RUN.md),
[delivery evidence](benchmarks/windows-candidate5/README.md) and the source-scoped
[signed workflow coverage](signed-distances-coverage.md) define this release.

| Requirement | Evidence | State |
| --- | --- | --- |
| Frozen matching native source |3155df80, raw522-input tree2ddb06c5da4d; candidate/source manifest | Complete |
| One cached build, preserve costly binding objects | Only provenance.cpp compiled; product targets relinked | Complete |
| Installable CPython3.12 x64 wheel | Build tag5 installed in a new isolated environment | Complete |
| Wheel RECORD and installed immutable files |67 immutable members match exactly | Complete |
| Actual installed DLL origins | GEOS/geos_c/OpenSlide mapped from the package directory | Complete |
| Installed signed jobs/CSV/save/reopen/replay | -5/10/0um; exact stored hashes/CSV and frozen QSD1 compatibility | Complete |
| Installed ImGui consumer | Managed90-frame run ready with expected values and no error | Complete |
| Independent native ZIP | Native app opens installed signed project; no Python imports in native PE closure | Complete |
| Static package dependency closures | Wheel68 files/7 binaries; native58/6; complete declared closure | Complete |
| Open dependencies and corresponding sources | AGPL/notices,522 exact source inputs and12 dependency archives verified | Complete |
| Removal preserves user outputs | Wheel absent after uninstall; all10 workflow files unchanged | Complete |
| Public immutable handoff |19 assets have matching GitHub sizes/SHA256 before/after publication; tag targets frozen source | Complete |

This is local Windows engineering distribution. It adds no fresh-host, signing,
additional-platform, exhaustive dynamic-loading, speed or biological claim.
