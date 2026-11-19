# I24 TensorStore N5 image reader

The workflow opens one explicitly selected local N5 dataset directory as raw
calibrated image planes and a default display projection. Its
`attributes.json` declares the axes and pixel spacing. The selected format
uses UInt16, X-fastest blocks, big-endian data and mode-0 headers. It accepts
raw payloads and the documented gzip or zlib-header option. A missing block
has the producer's zero fill. The edge rule admits a full padded block or a
block clipped to the exact remaining array extent.

The [fixture generator](../../../tools/ci/make_n5_producer_fixture.py) used
[TensorStore 0.1.86](https://pypi.org/project/tensorstore/0.1.86/) and
NumPy 2.5.5 to write and reopen a synthetic XYZCT image through each codec.
All 216 member hashes and three original sets of attributes are in the
[producer receipt](../../../build/gui-aggregation/catalogue-closure/n5/producer-01/producer.json).
Independent generations produced identical bytes and reference arrays. The
[retained reader](../../../build/gui-aggregation/catalogue-closure/n5/retained-reader.json)
refused the N5 selection and the same input through the OME-Zarr provider.

The [N5 file-system specification](https://github.com/saalfeldlab/n5#file-system-specification)
describes the directory and binary block format. The
[TensorStore N5 driver](https://google.github.io/tensorstore/driver/n5/index.html)
defines the writer's explicit axes, resolution and units. TensorStore
is an Apache-2.0 development oracle. The shipped decoder is project C++
and uses the admitted matching SDK zlib. TensorStore and Python stay
outside the native product and its package inputs.

## Windows handoff

The optional `org.qupathxx.n5` reader uses contract
`n5-tensorstore-uint16/1`. The [60-command native check](../../../build/gui-aggregation/catalogue-closure/n5/integration-01/results.json)
compares full raw arrays, seams, terminal blocks, fill, calibration and RGBA.
The shared [directory-store regression](../../../build/gui-aggregation/catalogue-closure/n5/directory-tests-01.log)
also verifies bounded member reads, source mutation, cancellation, canonical
keys, and Windows junctions. File symlink creation required a privilege absent
from this runner.

The [Windows ZIP](../../../build/gui-aggregation/catalogue-closure/n5/qupathxx-n5-1.0.0-windows-x64.zip)
contains the optional module, exact store-aware CLI, build inputs and notices.
All [120 members passed admission](../../../build/gui-aggregation/catalogue-closure/n5/qupathxx-n5-1.0.0-windows-x64.admission.json).
The [relocated install](../../../build/gui-aggregation/catalogue-closure/n5/deployment-01/results.json)
and [offline module rebuild](../../../build/gui-aggregation/catalogue-closure/n5/rebuild-check-01/results.json)
each passed all 60 checks. [Uninstall](../../../build/gui-aggregation/catalogue-closure/n5/uninstall-01.json)
removed only the 120 package-owned files and retained the three user files used
in the check.

The separate BigDataViewer XML/HDF5 formats, arbitrary transforms, additional
scalar types, layouts and compressors remain open under I24.
