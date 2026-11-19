# I24 BigDataViewer HDF5 project import

The [pinned producer](../../../tools/ci/make_bdv_producer_fixture.py) uses
[pybdv 0.6.0](https://pypi.org/project/pybdv/0.6.0/) and h5py 3.16.0 to
write and reopen an authored UInt16 volume. Its [export](../../../build/gui-aggregation/catalogue-closure/bdv/h5-producer-01/producer.json)
contains `dataset.xml` and one relative HDF5 file. It declares one
calibrated setup, two timepoints and two resolution levels without general
affine registration.

The HDF5 file contains setup-level `resolutions` and `subdivisions` matrices
in XYZ order and per-view rank-3 `cells` arrays in ZYX order. The producer
writes the cells as signed Int16 with Deflate compression and omits the
setup `dataType` attribute. The upstream
[HDF5 loader](https://github.com/bigdataviewer/bigdataviewer-core/blob/master/src/main/java/bdv/img/hdf5/Hdf5ImageLoader.java)
uses UInt16 for this legacy representation. The fixture preserves sample
bits across the signed boundary, including 32768. The oracle reinterprets
the written bits and verifies the original level-zero samples. The
[frozen N5 provider](../../../build/gui-aggregation/catalogue-closure/bdv/h5-retained-reader.json)
refused the HDF5 backend. The [shifted HDF5 fixture](../../../build/gui-aggregation/catalogue-closure/bdv/h5-shifted-01/producer.json)
contains a producer-authored nonzero X translation for the second timepoint.
An independent [HDF5 inspection](../../../build/gui-aggregation/catalogue-closure/bdv/h5-layout-01.json)
records datatypes, chunk dimensions, axis factors and unsigned sample hashes.

## Local Windows handoff

The `org.qupathxx.bdv-hdf5` provider uses the exact SDK's local HDF5 driver
and the shared BDV XML parser. It bounds chunk reads, accepts unfiltered or
Deflate local storage and uses zero fill for absent chunks. It preserves
calibrated XY/Z, ordinal timepoints, and the selected equal-XY,
unchanged-Z pyramid. Its [40-command verification](../../../build/gui-aggregation/catalogue-closure/bdv/h5-integration-01/results.json)
included legacy signed storage, malformed chunks, links and filters, sparse
fill, exact raw samples, grayscale and guarded export. The shared parser also
passed [35 BDV N5 checks](../../../build/gui-aggregation/catalogue-closure/bdv/h5-n5-regression-01/results.json).

The [Windows ZIP](../../../build/gui-aggregation/catalogue-closure/bdv/qupathxx-bdv-hdf5-1.0.0-windows-x64.zip)
passed [admission for all 137 files](../../../build/gui-aggregation/catalogue-closure/bdv/qupathxx-bdv-hdf5-1.0.0-windows-x64.admission.json).
The [relocated install](../../../build/gui-aggregation/catalogue-closure/bdv/h5-deployment-01/results.json)
and [offline rebuild](../../../build/gui-aggregation/catalogue-closure/bdv/h5-rebuild-check-01/results.json)
each passed 30 runtime checks. [Uninstall](../../../build/gui-aggregation/catalogue-closure/bdv/h5-uninstall-01.json)
removed only package-owned files and preserved the three user files.

## Aligned channel setups in 1.1.0

The [two-channel pybdv HDF5 fixture](../../../build/gui-aggregation/catalogue-closure/bdv/h5-multiset-02/producer.json)
has two times and levels. The reader uses ascending setup ID for channel
order, with equal geometry, calibration and zero-origin registration for
every view. The [29-command test](../../../build/gui-aggregation/catalogue-closure/bdv/multiset-hdf5-integration-01/results.json)
verified exact UInt16 values, selected channels, RGB, and refusal of out-of-profile
views. The [40-command single-setup regression](../../../build/gui-aggregation/catalogue-closure/bdv/multiset-hdf5-regression-01/results.json)
passed, including its malformed HDF5 cases.

The [1.1.0 Windows ZIP](../../../build/gui-aggregation/catalogue-closure/bdv/qupathxx-bdv-hdf5-1.1.0-windows-x64.zip)
passed [admission for 138 files](../../../build/gui-aggregation/catalogue-closure/bdv/qupathxx-bdv-hdf5-1.1.0-windows-x64.admission.json).
The [relocated install](../../../build/gui-aggregation/catalogue-closure/bdv/multiset-hdf5-deployment-01/results.json)
and [offline rebuild](../../../build/gui-aggregation/catalogue-closure/bdv/multiset-hdf5-rebuild-check-01/results.json)
each passed the complete 29-command multichannel check. [Removal](../../../build/gui-aggregation/catalogue-closure/bdv/multiset-hdf5-uninstall-01.json)
preserved all three user files. The module uses the unchanged reader ABI and
describes the aligned channel profile as `bdv-hdf5-uint16/2`.

## Local HDF5 partitions in 1.2.0

The [synthetic partition fixture](../../../build/gui-aggregation/catalogue-closure/bdv/h5-partitioned-03/producer.json)
repackages pinned pybdv pixels using the master and remapped partition layout
described by the [upstream writer](https://github.com/bigdataviewer/bigdataviewer-core/blob/master/src/main/java/bdv/export/WriteSequenceToHdf5.java).
h5py independently reads the expected arrays through the master links. The
fixture includes two channels, two times, two levels, three partition files,
explicit unsigned UInt16 in both byte orders and both HDF5 object link forms.
At one time, both channels share a partition. At the other, they span two files.

The optional native module validates the entire XML view map and every
corresponding master link. It opens only the declared and observed local files
and reads the selected cell datasets directly. The directory identity captures
the XML, master and partitions together. The
[44-command integration check](../../../build/gui-aggregation/catalogue-closure/bdv/partitioned-hdf5-integration-03/results.json)
covered exact raw and RGBA results, path separators, invalid maps and links,
missing companions and a changed companion identity at the same source path.
Legacy HDF5 [single setup](../../../build/gui-aggregation/catalogue-closure/bdv/partitioned-hdf5-regression-03/results.json)
and [multiple setups](../../../build/gui-aggregation/catalogue-closure/bdv/partitioned-hdf5-unpartitioned-03/results.json)
passed 69 checks. The shared parser passed 64 BDV N5 regression checks.

The [1.2.0 Windows ZIP](../../../build/gui-aggregation/catalogue-closure/bdv/qupathxx-bdv-hdf5-1.2.0-windows-x64.zip)
passed [admission for 138 files](../../../build/gui-aggregation/catalogue-closure/bdv/qupathxx-bdv-hdf5-1.2.0-windows-x64.admission.json).
The [relocated install](../../../build/gui-aggregation/catalogue-closure/bdv/partitioned-hdf5-deployment-01/results.json)
and [offline module rebuild](../../../build/gui-aggregation/catalogue-closure/bdv/partitioned-hdf5-rebuild-check-01/results.json)
each ran the same partition checks. The [removal receipt](../../../build/gui-aggregation/catalogue-closure/bdv/partitioned-hdf5-uninstall-01.json)
records the package files removed and three user files preserved. The native ABI
is unchanged and the profile contract is `bdv-hdf5-uint16/3`.

## Shared acquisition roles in 1.3.0

The [partitioned fixture](../../../build/gui-aggregation/catalogue-closure/bdv/roles-hdf5-producer-01/producer.json)
keeps one pybdv-authored tile, angle and illumination across both channels.
The shared XML parser admits one entry per role and requires all channels to
reference the same declared IDs. Explicit location and rotation must be zero.
The original XML remains part of the directory identity, and partition
routing, calibration and registration checks still apply.

The [53-command integration check](../../../build/gui-aggregation/catalogue-closure/bdv/roles-hdf5-integration-01/results.json)
verified exact raw and RGBA results and refused invalid roles or partition
mapping. The [single-setup](../../../build/gui-aggregation/catalogue-closure/bdv/roles-hdf5-regression-01/results.json),
[unpartitioned multichannel](../../../build/gui-aggregation/catalogue-closure/bdv/roles-hdf5-unpartitioned-01/results.json)
and [partition](../../../build/gui-aggregation/catalogue-closure/bdv/roles-hdf5-partitioned-01/results.json)
regressions passed all 113 checks.

The [1.3.0 Windows ZIP](../../../build/gui-aggregation/catalogue-closure/bdv/qupathxx-bdv-hdf5-1.3.0-windows-x64.zip)
contains 138 admitted files. The [relocated install](../../../build/gui-aggregation/catalogue-closure/bdv/roles-hdf5-deployment-01/results.json)
and [offline rebuild](../../../build/gui-aggregation/catalogue-closure/bdv/roles-hdf5-rebuild-check-01/results.json)
each passed 53 checks. [Removal](../../../build/gui-aggregation/catalogue-closure/bdv/roles-hdf5-uninstall-01.json)
preserved three user files. The profile is `bdv-hdf5-uint16/4`. Distinct tiles
or angles and general affine reslicing remain open.

## Declared missing views in 1.4.0

The [partitioned HDF5 fixture](../../../build/gui-aggregation/catalogue-closure/bdv/missing-hdf5-producer-01/producer.json)
declares that its first channel is absent at the second timepoint. Only
available pairs require a matching master link and local cell dataset.
The reader preserves the original channel and timepoint indices, and
rejects a raw or display request that includes a missing view.

The [58-command check](../../../build/gui-aggregation/catalogue-closure/bdv/missing-hdf5-integration-02/results.json)
verified exact results for available views, malformed declaration refusal,
partially acquired times and a partition assigned only to a missing view.
The [1.4.0 Windows ZIP](../../../build/gui-aggregation/catalogue-closure/bdv/qupathxx-bdv-hdf5-1.4.0-windows-x64.zip)
includes 138 admitted files. [Relocated install](../../../build/gui-aggregation/catalogue-closure/bdv/missing-hdf5-deployment-01/results.json),
[offline rebuild](../../../build/gui-aggregation/catalogue-closure/bdv/missing-hdf5-rebuild-check-01/results.json)
and [removal](../../../build/gui-aggregation/catalogue-closure/bdv/missing-hdf5-uninstall-01.json)
verified the handoff. The reader contract is `bdv-hdf5-uint16/5`.
