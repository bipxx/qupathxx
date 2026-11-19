# I24 BigDataViewer N5 project import

This workflow opens a local project folder containing `dataset.xml` and its N5
storage subtree, with one explicitly declared setup, an ordinal time range,
and aligned UInt16 pyramid levels. Project XML supplies the XYZ calibration
and view registration. The reader can represent a matching diagonal physical
scale with zero translation through the native image ABI. Other registrations
require an explicit frame contract.

The [fixture generator](../../../tools/ci/make_bdv_producer_fixture.py)
used [pybdv 0.6.0](https://pypi.org/project/pybdv/0.6.0/) and
[z5py 3.0.2](https://pypi.org/project/z5py/3.0.2/) as MIT development
oracles to write and reopen authored samples. The
[61-member producer receipt](../../../build/gui-aggregation/catalogue-closure/bdv/n5-producer-01/producer.json)
records the exact files, including setup and per-level N5 metadata. The
[second producer input](../../../build/gui-aggregation/catalogue-closure/bdv/n5-shifted-02/producer.json)
assigns a nonzero X translation to the second view.

The [BDV N5 format](https://github.com/bigdataviewer/bigdataviewer-core/blob/master/BDV%20N5%20format.md)
defines setup, timepoint and level coordinates. The ordinary
[TensorStore N5 reader](../n5/README.md) requires axes in the selected dataset
attributes. A BDV level omits them because BDV defines its XYZ native order;
the retained reader's [observed refusal](../../../build/gui-aggregation/catalogue-closure/bdv/legacy-reader.json)
confirms the missing route.

## Windows handoff

The [35-command native test](../../../build/gui-aggregation/catalogue-closure/bdv/integration-01/results.json)
verified both times and levels, physical XY/Z spacing, absent blocks, odd
pyramid origins, default grayscale and strict refusal of the shifted producer
view, escaped paths and damaged inputs. The shared N5 decoder also passed
[60 standalone regression commands](../../../build/gui-aggregation/catalogue-closure/bdv/standalone-regression-01/results.json).

The [Windows ZIP](../../../build/gui-aggregation/catalogue-closure/bdv/qupathxx-bdv-n5-1.0.0-windows-x64.zip)
passed [admission for all 125 paths](../../../build/gui-aggregation/catalogue-closure/bdv/qupathxx-bdv-n5-1.0.0-windows-x64.admission.json).
The [relocated install](../../../build/gui-aggregation/catalogue-closure/bdv/deployment-01/results.json)
and [offline module rebuild](../../../build/gui-aggregation/catalogue-closure/bdv/rebuild-check-01/results.json)
each passed 35 native checks. [Uninstall](../../../build/gui-aggregation/catalogue-closure/bdv/uninstall-01.json)
removed exactly 125 package files and retained the three user files.
The module uses admitted TinyXML2 11.0.0 and shares the existing native
N5 block decoder and ABI bridge. The ZIP contains matching local source
changes, relevant dependency sources and notices, and identifies the separate
retained SDK and CLI source prerequisites.

## Aligned channel setups in 1.1.0

The [two-channel pybdv N5 fixture](../../../build/gui-aggregation/catalogue-closure/bdv/n5-multiset-02/producer.json)
has two times and levels. The reader uses ascending setup ID for channel
order, with equal geometry, calibration and zero-origin registration for
every view. The [29-command test](../../../build/gui-aggregation/catalogue-closure/bdv/multiset-n5-integration-01/results.json)
verified exact UInt16 values, selected channels, RGB, and refusal of out-of-profile
views. The [35-command single-setup regression](../../../build/gui-aggregation/catalogue-closure/bdv/multiset-n5-regression-01/results.json)
and the [60-command standalone N5 regression](../../../build/gui-aggregation/catalogue-closure/bdv/multiset-standalone-regression-01/results.json)
passed.

The [1.1.0 Windows ZIP](../../../build/gui-aggregation/catalogue-closure/bdv/qupathxx-bdv-n5-1.1.0-windows-x64.zip)
passed [admission for 126 files](../../../build/gui-aggregation/catalogue-closure/bdv/qupathxx-bdv-n5-1.1.0-windows-x64.admission.json).
The [relocated install](../../../build/gui-aggregation/catalogue-closure/bdv/multiset-n5-deployment-01/results.json)
and [offline rebuild](../../../build/gui-aggregation/catalogue-closure/bdv/multiset-n5-rebuild-check-01/results.json)
each passed the complete 29-command multichannel check. [Removal](../../../build/gui-aggregation/catalogue-closure/bdv/multiset-n5-uninstall-01.json)
preserved all three user files. The module uses the unchanged reader ABI and
describes the aligned channel profile as `bdv-n5-uint16/2`.

The [separate HDF5 plugin](../bdv-hdf5/README.md) supports the same aligned channel
profile.

## Shared acquisition roles in 1.2.0

Pinned pybdv wrote a [two-channel project](../../../build/gui-aggregation/catalogue-closure/bdv/roles-n5-producer-01/producer.json)
with one shared tile, angle and illumination. The previous reader refused it.
The current profile admits one entry per role and requires every channel to
reference the same declared ID. Location and rotation may be absent or zero.
The original XML remains part of the directory identity and the per-view
registration still has to match the calibrated grid.

The [38-command integration check](../../../build/gui-aggregation/catalogue-closure/bdv/roles-n5-integration-01/results.json)
verified exact raw and RGBA output and refusal of mismatched roles. The
[single-setup](../../../build/gui-aggregation/catalogue-closure/bdv/roles-n5-regression-01/results.json)
and [multichannel](../../../build/gui-aggregation/catalogue-closure/bdv/roles-n5-unpartitioned-01/results.json)
regressions passed 64 checks.

The [1.2.0 Windows ZIP](../../../build/gui-aggregation/catalogue-closure/bdv/qupathxx-bdv-n5-1.2.0-windows-x64.zip)
contains 126 admitted files. The [relocated install](../../../build/gui-aggregation/catalogue-closure/bdv/roles-n5-deployment-01/results.json)
and [offline rebuild](../../../build/gui-aggregation/catalogue-closure/bdv/roles-n5-rebuild-check-01/results.json)
each passed 38 checks. [Removal](../../../build/gui-aggregation/catalogue-closure/bdv/roles-n5-uninstall-01.json)
preserved three user files. The profile is `bdv-n5-uint16/3`. Distinct tiles,
distinct angles and general affine reslicing remain open.

## Declared missing views in 1.3.0

The [two-channel N5 fixture](../../../build/gui-aggregation/catalogue-closure/bdv/missing-n5-producer-01/producer.json)
contains only one acquired channel at its second timepoint. The
[SpimData missing-view declaration](https://github.com/bigdataviewer/spimdata/blob/master/src/main/java/mpicbg/spim/data/sequence/XmlIoMissingViews.java)
marks the absent pair. Indices, setup names and calibrated axes are preserved.
A raw request for the available channel succeeds. Raw and display requests
that include a missing view fail explicitly before export. Sparse blocks in
an available view retain their normal zero-fill behavior.

The [42-command check](../../../build/gui-aggregation/catalogue-closure/bdv/missing-n5-integration-02/results.json)
verified exact samples, absent data, invalid declarations and stored data
marked missing by XML. The [60-command standalone N5 regression](../../../build/gui-aggregation/catalogue-closure/bdv/missing-standalone-regression-01/results.json)
also passed. The [1.3.0 Windows ZIP](../../../build/gui-aggregation/catalogue-closure/bdv/qupathxx-bdv-n5-1.3.0-windows-x64.zip)
includes 126 admitted files. [Relocated install](../../../build/gui-aggregation/catalogue-closure/bdv/missing-n5-deployment-01/results.json),
[offline rebuild](../../../build/gui-aggregation/catalogue-closure/bdv/missing-n5-rebuild-check-01/results.json)
and [removal](../../../build/gui-aggregation/catalogue-closure/bdv/missing-n5-uninstall-01.json)
verified the handoff. The reader contract is `bdv-n5-uint16/4`.
