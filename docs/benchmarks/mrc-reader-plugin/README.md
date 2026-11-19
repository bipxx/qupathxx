# Native scalar MRC2014 plugin

## Packed mode 101 in 1.5.0

`org.qupathxx.mrc` 1.5.0 uses contract `mrc2014-scalar-window/3`. Mode 101
opens even-column scalar rasters and stacks. The plugin reads low-nibble-first
codes directly from the packed raster and exposes the exact 0–15 values as
UInt8. It preserves the packed source identity and applies a separate 0–15
display window. [CCP-EM format update](https://www.ccpem.ac.uk/mrc-format/mrc-format-proposals/) · [EMAN2 decoder reference](https://blake.bcm.edu/doxygen/mrcio_8cpp_source.html)

The native corpus passed 94 valid profiles, 54 malformed-file refusals and
31 commands. New cases cover axis permutations and big-endian headers, exact
extent refusal, compressed UInt8 export, and a fresh-process reopen of a
selected packed image with its saved display window. The module SHA-256 is
`e41b99eb798eaa302f038dad86316d482ccce28f89bc3d1168ffa0ecb6ade263`.
[Baseline](../../../build/mrc-reader/packed4-candidate/baseline.json) ·
[Validation](../../../build/mrc-reader/packed4-candidate/validation/validation.json)

## Image and P1 volume stacks in 1.4.0

`org.qupathxx.mrc` 1.4.0 uses `mrc2014-scalar-window/2`. Image stacks with
`ISPG=0` and `MZ=1` expose one XY series per section. P1 volume stacks with
`ISPG=401` expose `NZ/MZ` XYZ series with `MZ` planes each. Both need
`MAPS=3`. The reader selects one series and keeps its native dimensions,
span and calibration. [MRC2014 stack fields](https://www.ccpem.ac.uk/mrc-format/mrc2014/)

The corpus passed 90 valid profiles, 50 malformed-file refusals and 27
native checks. It covers selected raw and display bytes, exact stack bounds,
source identity, and a fresh-process project reopen with a different catalog
default. Plain, gzip and bzip2 stack payloads retain the existing work and
checksum limits. The validated module SHA-256 is
`d5272529af189af197ffe05911893d189c802e74a6d15f3aba281b63d851d269`.
[Baseline](../../../build/mrc-reader/stacks-candidate/baseline.json) ·
[Validation](../../../build/mrc-reader/stacks-candidate/validation/validation.json)

## Bzip2-wrapped volumes in 1.3.0

An MRC with a `.bz2` suffix and matching signature can carry one complete
bzip2 stream. The plugin decodes the primary header first, budgets storage
from its declared raster extent, and verifies the stream checksum and exact
input/output lengths before publishing pixels. The mrcfile 1.5.4 development
oracle opens the authored baseline and produces an independent bzip2 fixture.
[mrcfile compression](https://mrcfile.readthedocs.io/en/stable/usage_guide.html#handling-compressed-files)

The ABI and native consumers cover 85 profiles, 47 refusals and 22 commands,
including calibrated saved-source replay and refusal when the compressed file
is re-encoded with identical decoded content. The source package includes
unmodified libbzip2 1.0.8 and its original license alongside zlib.
[Local handoff](../../../build/mrc-reader/bzip2-candidate/results.json)

## Gzip-wrapped volumes in 1.2.0

The module reads one complete gzip member under the same scalar MRC2014
contract. The retained 1.1.0 provider refuses an authored gzip copy of an
accepted mode 12 volume. The independent mrcfile 1.5.4 oracle reads both
with identical stored values and calibration.
[mrcfile gzip usage](https://mrcfile.readthedocs.io/en/stable/usage_guide.html#handling-compressed-files)

The direct ABI and native consumer suites cover 79 positive profiles, 38
refusals and 18 commands. They check plain and gzip raw/display values,
calibration, both byte orders, all binary16 encodings, corrupted and
oversized streams, and cancellation during inflation. A calibrated gzip
project reopens and replays exactly. Changing only the gzip timestamp
causes source-currentness refusal even though the decoded content matches.
The 25-member package includes the exact zlib source and original notice.
[Local handoff](../../../build/mrc-reader/gzip-candidate/results.json)

## Half-precision volumes in 1.1.0

The optional Windows module reads the CCP-EM mode 12 extension and widens
binary16 values to Float32 at the existing raw boundary. It retains the
two-byte storage stride and uses the same physical axis and display contracts.
The direct ABI suite checks all 65,536 binary16 encodings, including signed
zeros and NaN payloads, with additional fixtures for both byte orders and all
six axis permutations. The saved-project consumer covers a big-endian
mode 12 volume and the CLI checks an exact later Z plane.
[CCP-EM mode 12 proposal](https://www-test2.ccpem.ac.uk/mrc-format/mrc-format-proposals/)

## Initial scalar profiles in 1.0.0

`org.qupathxx.mrc` 1.0.0 uses contract `mrc2014-scalar-window/1`.
The optional native reader opens orthogonal single images and volumes.
It maps file columns, rows and sections to the explicitly named X, Y and Z
indices without resampling. Mode 0 is signed Int8. Modes 1, 2 and 6 are
Int16, Float32 and UInt16. The reader keeps stored sample bits and converts
physical spacing from angstroms to micrometers. Raw and display use the same
index mapping. The [profile guide](../../../plugins/mrc/README.md) describes
the exact scope and display windows.

## Independent fixture and rights

The authored Int16 volume was written by mrcfile 1.5.4 and reopened with
exact samples and voxel sizes of 4, 6 and 20 angstroms. The retained host
tried its TIFF reader and refused raw export before the optional module was
registered. The baseline is `build/mrc-reader/baseline.json`.

The independently implemented adapter and its authored fixtures use
AGPL-3.0-only. The [public format specification](https://www.ccpem.ac.uk/mrc-format/mrc2014/)
and [CC BY paper](https://pmc.ncbi.nlm.nih.gov/articles/PMC4642651/) are references.
mrcfile 1.5.4 is a BSD-3-Clause development oracle. Its wheel hash is
`195370a13db5ce19600499f9bdf90fd9888979bf39de19a17c19024436c6f4c2`.
The package contains the native adapter and reader kit with their matching
sources and notices.
