# Native reader plugins and saved sources

The optional Windows x64 pilot loads explicitly configured native image readers
through a versioned [C ABI](../cpp/include/qupathxx/reader_plugin.h). The installed
P6 example decodes 8-bit binary PPM inside its own DLL, using the same extracted
decoder component as the built-in PPM reader. It links no application core library
and provides display RGBA only. The frozen
[phase 3 receipt](benchmarks/reader-plugin-pilot/README.md) covers that standalone
pilot, including its then-current Project refusal. The
[phase 4 receipt](benchmarks/reader-plugin-project/README.md) records source-scoped
native tests and local deployment validation for the saved-source implementation.
It has no new wheel, publication or release qualification claim.

## Build and use the installed kit

`QUPATHXX_BUILD_READER_PLUGINS` defaults to `OFF`. Enable it in an existing native
Windows x64 MSVC Release `/MD` build; Python remains optional and defaults OFF.
Build `qupathxx_region`, then install the development components:

```text
cmake --install BUILD --config Release --prefix INSTALL --component ReaderPluginKit
cmake --install BUILD --config Release --prefix INSTALL --component ReaderPluginHost
```

The kit installs `include/qupathxx/reader_plugin.h`, the dependency-free decoder,
the recorded compiler profile, license and example under
`share/qupathxx/reader-plugin`. Copy the example outside the application checkout.
Its [instructions](../examples/reader_plugin/README.md) build a DLL and a
hash-bound `reader-plugins.json` using only the relocated installed kit. Compiler,
architecture, Release and dynamic-CRT guards apply to that independent build.

Configure the CLI with an explicit absolute manifest path before the existing
listing or display command:

```text
qupathxx_region --reader-plugins ABS_MANIFEST --readers
qupathxx_region --reader-plugins ABS_MANIFEST --reader org.qupathxx.p6 IMAGE.ppm 0 0 0 512 512 OUTPUT.pam
```

Thumbnail, resampling and RGB-nominal display modes accept the same prefixes.
Existing positional commands and automatic built-in selection keep their legacy
policies. The quantitative raw CLI retains its explicit TIFF contract.

Native application and batch processes accept the same bootstrap prefix before
opening a project or running an existing command:

```text
qupathxx_app --reader-plugins ABS_MANIFEST --project SAVED_PROJECT
qupathxx_app --reader-plugins ABS_MANIFEST IMAGE.ppm --reader org.qupathxx.p6
qupathxx_batch --reader-plugins ABS_MANIFEST EXISTING_BATCH_COMMAND ...
```

Configured providers append to the existing desktop reader choices. Automatic
selection and the original built-in positions keep their existing behavior.
The manifest must be configured before the first catalog query or source open.

## Admission and lifetime

The manifest schema is `qpx.reader-plugins/1`. Each provider declares a unique
namespaced ID, display name, version, semantic contract, ABI/profile, display/raw
upper bounds, one module path/hash and an explicit dependency path/hash list.
An optional `options` JSON object supplies defaults for newly opened sources;
the host normalizes it before passing it to the module. Options are bounded to
4096 UTF-8 bytes; an empty object has the canonical empty representation.
The example generates this manifest after compilation. Module/dependency paths
are relative to the manifest directory and must resolve within that package;
discovery does not search CWD or PATH. At most 32 providers, 63 dependencies per
provider, 1 MiB manifest text and 256 MiB package image bytes are admitted.

The host locks and hashes actual module/dependency files before loading, checks
x64 PE imports, refuses undeclared non-system dependencies, and checks actual
resolved import targets. Loads use package/System32 search flags without changing
the process-wide search path. See Microsoft's [LoadLibraryExW contract](https://learn.microsoft.com/en-us/windows/win32/api/libloaderapi/nf-libloaderapi-loadlibraryexw)
and [PE import format](https://learn.microsoft.com/en-us/windows/win32/debug/pe-format).
The pilot rejects delay-load and cyclic package dependencies. Its manifest profile
must match the host's recorded MSVC series; the supplied kit additionally checks
the exact compiler version and `/MD` build configuration.

Returned ABI tables, callback origins and provider descriptors must match the
admitted package. The catalog freezes before its first open/listing and cannot
reload. Rejected modules produce owned `plugin_diagnostics` and cannot replace
built-ins. Loaded modules are pinned until process exit; restart to change them.
These are admitted in-process native modules. Reuse the existing
[material/reader admission policy](reader-license-policy.md) for code, formats,
dependencies and notices; a runtime manifest does not establish licensing rights.

## Data contract and current limits

The host owns output buffers and copies bounded UTF-8 metadata. It validates
shapes, strides, exact fill lengths, sample types, calibration and validity before
returning results. Raw transfer is packed native-endian HWC with one validity byte
per pixel; valid integer bits, NaN/Inf and signed zero remain unchanged. Per-handle
calls and close serialize, and module-owned handles are destroyed exactly once.
Cancellation is a synchronous host callback; modules retain no host pointers.

Providers that declare `source_files` also export the optional
`qpx_reader_files_entry_v1` table. The host discovers up to 63 absolute companion
paths before opening the decoder, under the primary file guard, and repeats
discovery around reads. The host observes versions and identities for the whole
set and rejects duplicate paths, file identities or primary aliases. This
extension leaves the base v1 function table unchanged at 72 bytes on the supported
x64 profile, so single-file v1 modules do not need the extra entry point.

The P6 module admits single-file sources, conservatively bounds encoded files to
the host's 256 MiB work allowance, and refuses raw access. A separate test-only
module proves typed raw transfer and is never installed. Source file guards check
currentness around native work. Provider capability flags do not admit a source
into an existing scientific recipe. The bridge exposes no legacy TIFF/Zarr
view/options objects and rejects view mutation. Cached raw metadata and typed raw
reads do not provide a `raw_view()`; existing raw-analysis, raw-classifier,
dark/flat and unmixing reader/view requirements remain in force. The P6 module
exposes no raw metadata.

## Saved projects and implementation review

Project schema 30 stores a bounded `qpx.reader-plugin-link/1` payload for each
linked plugin image. The private canonical record contains display metadata,
optional raw axes/channel metadata, host-observed single/compound file facts and
an implementation snapshot. That snapshot binds the provider, version, semantic
contract, ABI, compiler/CRT profile, capabilities, module SHA-256, sorted dependency
basenames and SHA-256 values, and normalized explicit options. Installed module
paths are excluded from implementation identity so relocation preserves it.
The link is bounded to 2 MiB, raw metadata to 1 MiB, and source observations to
128 KiB with at most 63 companions. Directory stores remain the existing
OME-Zarr contract.

The implementation digest appears in the plugin's observed reader identity as
`PROVIDER:plugin/1:SHA256`; `ImageServer::reader_id()` and `images.reader` retain
the provider ID. Built-in reader IDs, source JSON and verification/provenance
encodings keep their existing representations.

`Project::open` validates saved links without loading modules or freezing the
catalog. With a missing provider or a loader-OFF build, the image, saved metadata
and self-contained retained reports remain readable and can be re-saved. Source
pixel reads refuse without falling back to another reader. A successful lazy open
requires the saved implementation, metadata and observed file set to match.
Older hosts reject schema 30 rather than treating plugin links as built-ins.

Saved options remain explicit: changing manifest defaults does not replace them
on reopen. A new source opened with different explicit options gets a different
binding. Different module or dependency bytes under the same version string also
change implementation identity. Native source review reports
`changed_implementation`, and `Project::apply_source_review` requires
`accept_changed=true` to adopt that candidate. Existing stale-review and file
guards remain the publication boundary; historical records stay unchanged.

The retained RegionAnalysis digest binds plugin implementation identity as well
as its RGBA input. Tissue and Cell pixel-content hashes retain their existing
formats, while their native verification authority refuses to make an archived
run current under a different plugin implementation, even when masks and values
are identical. Recompute and retain a new result after adoption. No quantitative
recipe gate is expanded by this saved-source support.

## Native SDK and local development scope

Installed native consumers include
[`qupathxx/reader_plugins.hpp`](../cpp/include/qupathxx/reader_plugins.hpp) and call
`qpx::configure_reader_plugins(absolute_manifest)` before a catalog query or source
pixel read. `qpx::reader_catalog_json()` returns an owned report. Configuration
may follow `Project::open`, because parsing a saved project needs no module.
The [external SDK example](../examples/reader_plugin_project/main.cpp) performs
native add, RegionAnalysis, keep, source review, save, reopen and replay through
these same services. Python remains optional and supplies no implementation or
fallback for this workflow.

The installed kit and SDK handoff are local development artifacts. The
[phase 4 receipt](benchmarks/reader-plugin-project/README.md) records source-scoped
native tests, relocated SDK/module use and replay, and removal preserving research
files.
Distributable membership and publication
remain a separate owner-selected checkpoint in the
[plan](format-reader-plugin-plan.md); the frozen phase 3 evidence and candidate8
qualification do not transfer to this source.
