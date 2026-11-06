# Windows candidate2 delivery

[Published prerelease](https://github.com/bipxx/qupathxx/releases/tag/v0.1.0-windows-candidate.2)
provides16 authenticated assets: CPython3.12 x64 wheel (build tag2), standalone
native ZIP, matching application sources, unchanged dependency sources, runtime
manifests, checksums, example, guide and installed-route evidence. Project code
is AGPL-3.0-only; bundled dependencies retain their licenses.

Native source is `bee4736f2ad0aeffc9907542c873f06efd335cc7`, raw input tree
`b52fa9b85799b7ce0b68780a84f2c95ff993c69b848ce7b24a9b74d54215e8f7`.
The [candidate inventory](candidate.json) records63 exact installed wheel package
members,497 raw-byte compiler inputs verified inside the corresponding source
archive, and12 unchanged verified dependency source archives. Wheel SPDX/license
metadata and license bytes were checked; runtime inspection accepts68 wheel
members/7 binaries and58 standalone members/6 binaries. This is a source-scoped
engineering candidate, not a transfer of candidate1's fresh-host receipt.

The isolated installed candidate passes the [Python workflow](installed-sdk.json)
and [ImGui workflow](installed-desktop.json):5913875 tissue samples,1083798
foreground samples,688 components, correct input identity, CSV accounting,
QTM6 save/reopen/replay and standalone reopen of the combined Python project.
The Python example independently checks bounded high-resolution RGB statistics.
[All3 installed focused tests](installed-tests.log) and
[14 public-stub/signature checks](stub-checks.log) pass. Kernel/oracle/partition/
cancellation checks and the diagnosed fixture-path failure remain in the
[implementation evidence](README.md), avoiding another full test campaign.

[Uploaded asset digests](uploaded-assets.json) were compared against every local
file before publication. That receipt intentionally records draft state at the
prepublication check; [publication state](published-release.json) confirms the
release is public and a prerelease at the frozen source commit.

The native packaging draft added a duplicate identical project license and used
a basename rejected by the existing runtime inspector. The corrected archive
retains a single license and the required basename; binaries did not change.
The failed draft/log is retained outside the selected delivery. An attempted
draft lookup by tag returned404; listing the authenticated draft supplied its
asset digests, so no upload was repeated. Cached dependency sources were reused
via hard links rather than redownloaded or duplicated.

Local files are under main workspace `build/large-slide-workflow/delivery`;
the isolated final runtime is `build/large-slide-workflow/installed`. The old
candidate1 installation/assets, active native cache, pinned fixtures and parked
PRESET1 source/archive/stash remain preserved. Automatic approval review rejected
the bounded recursive deletion of superseded stage/test directories as "blocked
by policy". No deletion occurred; these directories remain outside the selected
delivery, with no cleanup workaround attempted.

No further feature is selected. Additional platforms, biological validation,
streaming level-zero whole-slide cells, optional feature parity and speed
optimization retain their existing scope; none is claimed by this release.
