# Native OME-Zarr exact integral fills

I22/R15 selects a correctness repair in the existing native NGFF0.4/Zarr2 image
reader. The original JSON spelling of an integral array's `fill_value` must
prove an exact, in-range integer. A rounded binary64 value cannot prove that.
For example, `1.00000000000000000001` previously became1, and an overflowing
negative Int64 literal could become `INT64_MIN` before validation.

This is a separately identified native source candidate. The frozen FP04 core,
A08 label provider/addon, M05 Loom products, GUI, bindings and wheels retain
their existing bytes and qualification; the repair does not patch those files.
The broader eligible format and application/integration catalogue remains open.

## Scalar contract

| Source fill and array type | Behavior |
| --- | --- |
| In-range JSON integer for an integral dtype | Preserve exact bits, including values above2^53 and UInt64 maximum |
| Exact integral decimal/exponent token such as `1.0`, `-0.0`, `1000e-3` or `0.001e3` | Preserve the existing exact value; integral negative zero remains zero |
| Fractional token that binary64 rounds to an integer, or a nonzero underflow | Refuse when opening the source |
| Integral floating token whose value loses precision in binary64, such as `9007199254740993.0` | Refuse rather than substitute a nearby integer |
| Out-of-range fill, including integer tokens falling back to JSON floating storage | Refuse when opening the source |
| `null` | Present chunks remain readable; reading a missing chunk refuses undefined data |
| Float32/Float64 array | Existing numeric conversion, signed zero and declared nonfinite-string behavior remain unchanged |

All eight integral dtypes retain their individual ranges. Validation covers every
selected pyramid level before publication, including fully populated arrays.
This preserves the image reader's useful exact floating-token inputs. The
separate A08 label profile intentionally remains stricter: integer/null tokens
only, with no changed module or implied new label qualification.

The native metadata authority uses public JSON SAX callbacks to examine the
original numeric token, checks decimal integrality and magnitude without an
expanded-decimal allocation, and verifies agreement with the previously exact
binary64 conversion. It then passes a typed integer to the existing native
scalar/range authority. Escaped root keys are decoded; nested keys cannot supply
the fill. Metadata size/depth bounds remain in force. No public or private data
layout, source-ID recipe, pixel decoder, coordinate mapping or persistence schema
changes. This is not a process sandbox or a total-process memory claim.

The [Zarr2 storage specification](https://zarr-specs.readthedocs.io/en/latest/v2/v2.0.html)
defines the source fill and missing-chunk semantics. No new third-party code,
codec, upstream image/dataset/model or required asset enters this repair.
First-party changes retain AGPL-3.0-only; the existing JSON/zlib/native dependency
notices and corresponding-source obligations remain component-specific.

## Native evidence

The [before-state receipt](../../../build/gui-aggregation/catalogue-closure/ngff-fill/before.json)
preserves affected sources, public/private interfaces and the prior integration
recipe, with exact hashes of seven frozen products. The
[native integration receipt](../../../build/gui-aggregation/catalogue-closure/ngff-fill/integration-02/results.json)
passes all five commands: the old observation, new verification and existing
source/server/project regressions. The same663 authored fixture files remain
unchanged across old/new runs; the170 original regression fixture files retain
their hashes.

All214 focused cases pass:133 valid cases retain exact old pixels, metadata and
source identities;71 malformed fills refuse at open and10 undefined null fills
refuse at read. The old implementation accepted42 of the malformed cases; the
candidate accepts none. These counts distinguish a completed old observation
from new refusal qualification rather than relabeling old behavior as correct.

Two directly included object files compile the changed shared authorities over
the exact retained native addon/core. No live historical native target, whole
core, GUI, Python/binding or wheel rebuild is part of this check. The focused
test runs the old and candidate implementations against the same authored
Unicode fixture paths, and checks membership, content hashes and native file
versions before comparing valid source IDs, metadata and exact pixel bits.

Affected existing source/server/project tests cover chunk seams/codecs/axes,
padding, work limits, cancellation, source changes, calibrated raw statistics,
explicit display windows, saved views/records, offline inspection and replay.
These are native source-scoped checks, not a release, biological, speed or
general NGFF/codec/platform qualification. The actual compiler inputs and new
test products must retain their own identities; earlier evidence is not
transferred to changed code.

The [compiler-input receipt](../../../build/gui-aggregation/catalogue-closure/ngff-fill/compiled-inputs.json)
records104 observed inputs, six targets and11 products. Public/private layout
headers and all seven retained products remain exact. Configure01 failed before
compilation because the addon import existed only in a child scope; the same
package was imported in the parent scope for configure02. An initial controller
environment-key failure ran no native command; using Windows' uppercase
`SYSTEMROOT` key corrected it. Both failures and their original recipes remain
in the packet. One combined native build and one five-command native run pass;
no scientific assertion, numeric tolerance or stored identity was relaxed.

The [consolidated receipt](../../../build/gui-aggregation/catalogue-closure/ngff-fill/results.json)
binds these checks to their actual source and product hashes. This source-scoped
repair closes the demonstrated native correctness case; packaging a changed
application/SDK remains a separate delivery selection.
