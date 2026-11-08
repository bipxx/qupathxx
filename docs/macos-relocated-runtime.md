# Relocated native Mac host-workflow validation

Candidate 152 connects the [staged relocation executor](macos-relocation.md) to
the native Intel/arm64 workflow. Both the original snapshot and relocated ZIP
are uploaded with canonical manifests, the plan, Apple command records and raw
logs. No build SDK or original executable is edited. Ad-hoc signature integrity
checks support builder execution; publisher signing remains unverified.

`check_macos_relocated_runtime.py` reinspects the completed relocated archive and
extracts a fresh copy into `fresh relocated package with spaces`. Every extracted
file must match the archived byte hash and mode policy. Loader search overrides
are removed. Native CLI diagnostics use `DYLD_PRINT_LIBRARIES` only to observe
images; expected captured runtime paths must point into the fresh package.

The host probes are:

- A two-pixel PPM thumbnail must contain exact RGBA `[60,100,180,255]`.
- The existing embedded version-1 project fixture selects image 1/object 1 and
  runs native RGB optical-density analysis. One new checkpoint must retain one
  analysis record and the native CSV exporter must reopen it successfully.
- CSV geometry/counts, white reference, calibration and provenance are checked.
  Four identical `[30,60,90]` pixels require channel means `log10(255/sample)`
  within a fixed absolute `2e-7` float tolerance, area four pixels/0.5 square
  micrometres and perimeter three micrometres. Nonfinite values fail.
- A fresh isolated Python child loads the packaged GEOS C API through `ctypes`,
  unions two known squares and requires exact area 150. The public dyld image
  enumeration must identify the expected package GEOS files and hashes.
- Source project bytes and every extracted package file must remain unchanged.

These probes need only Python's standard library; no qupathxx Python wheel is
installed on the Mac runner. The GEOS API follows the
[C API declarations](https://libgeos.org/doxygen/geos__c_8h.html). Image enumeration
is single-threaded. Diagnostic library paths are parsed lexically so offline
auditing works on other host operating systems.

## Source-bound audit

`accept_macos_native.py --macos-relocation` implies the original snapshot gate
and all inherited native/TMA cohorts. Its helper checks exact committed tooling,
workflow and project fixture bytes, reconstructs the plan from the original
uploaded snapshot and reinspects all relocated bytes/protected metadata. Every
Apple command/exit status/raw log and verified binary digest must match.

The runtime auditor checks actual command arguments, logs, original fixture,
checkpoint, PAM, CSV, geometry child and mapped-library hashes. Missing mappings,
external SDK paths, duplicate JSON keys, Boolean count substitutions, failed
commands, altered records and release/search-override claims reject acceptance.
The geometry child result has a distinct filename from its command ledger.

## Current evidence and remaining gates

The local suite passes 389 cases, including 38 runtime contract/simulated-flow
tests, 14 command-ledger audit cases and 24 complete synthetic artifact-audit
cases. One complete synthetic runtime evidence set is accepted; 23 altered or
coordinately rewritten artifact sets reject acceptance. Seven additional PAM
cases enforce the exact dimensions, depth, tuple type, header uniqueness and
pixel length. The uploaded fixed PPM input is checked independently. These
fixtures exercise the offline protocol and do not execute Apple tools or native
Mac binaries; see the [candidate receipt](benchmarks/macos-runtime-artifact-audit-local-windows.json). The complete local probe test uses
labelled simulated commands and never executes Mac binary fixtures. It found
and fixed an initial collision between `geometry.json` child output and its
command ledger; the original failed run is retained. The shared frozen Python
case contract now includes all ten runtime-inventory classes for future
installed engine/portable validation.

A separate pilot checks the scalar CSV contract against the previously accepted
Windows executable and existing project fixture. It uses an older accepted
compiled engine and does not qualify this Mac source. The
[local receipt](benchmarks/macos-relocation-runtime-local-windows.json) records
that distinction. Native compiler inputs are unchanged. Actual Apple edits,
Intel/arm64 package image mappings and host workflows require exact-source
hosted execution and subsequent artifact audit before acceptance. Run
[40125033683](https://github.com/bipxx/qupathxx/actions/runs/40125033683)
uses the earlier `b0d12245` source and was in progress when this candidate was
recorded. Its source-bound audit uses that version of the tools; it cannot
qualify the new PPM upload or stronger PAM checks in this candidate.

This scope does not establish an older-macOS support matrix, arbitrary symbol/
ABI coverage, Python/GUI/OpenSlide Mac packages, installers, distribution signing,
hardware performance or biological/QuPath equivalence. License and remaining
M0-M7 requirements stay open; full goal completion is not claimed.
